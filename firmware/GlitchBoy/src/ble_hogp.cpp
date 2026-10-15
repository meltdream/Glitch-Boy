#include "ble_hogp.h"
#include <stdio.h>
#include "diag_config.h"

#ifndef BLE_HOGP_DIAG_LOGS
#define BLE_HOGP_DIAG_LOGS EVENT_LOGS
#endif

#if BLE_HOGP_DIAG_LOGS
#define BLE_LOG(fmt, ...) printf("[ble] " fmt "\n", ##__VA_ARGS__)
#else
#define BLE_LOG(fmt, ...) do { } while (0)
#endif

#if BLE_HOGP_ENABLED

#include <Arduino.h>
#include "sdkconfig.h"

#include <string.h>

#include <map>
#include <string>
#include <vector>

#include <NimBLEDevice.h>
#include <NimBLEAddress.h>
#include <NimBLEAdvertisedDevice.h>
#include <NimBLEClient.h>
#include <NimBLERemoteCharacteristic.h>
#include <NimBLERemoteDescriptor.h>
#include <NimBLERemoteService.h>
#include <NimBLEScan.h>
#include <NimBLEUUID.h>
#include <NimBLESecurity.h>

#include "esp_bt_defs.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_heap_caps.h"

#include "emu.h"
#include "generic_pad.h"
#include "hid_server/hid_gamepad.h"
#include "hid_server/hid_server.h"

namespace {

enum {
    BLE_HID_SERVICE_UUID = 0x1812,
    BLE_HID_INFO_UUID = 0x2A4A,
    BLE_REPORT_MAP_UUID = 0x2A4B,
    BLE_HID_CONTROL_POINT_UUID = 0x2A4C,
    BLE_REPORT_UUID = 0x2A4D,
    BLE_PROTOCOL_MODE_UUID = 0x2A4E,
    BLE_REPORT_REFERENCE_UUID = 0x2908,

    BLE_HID_REPORT_TYPE_INPUT = 0x01,
    BLE_HID_CONTROL_EXIT_SUSPEND = 0x00,
    BLE_PROTOCOL_MODE_REPORT = 0x01,

    BLE_APPEARANCE_JOYSTICK = 0x03C3,
    BLE_APPEARANCE_GAMEPAD = 0x03C4,
};

enum CandidateFlags {
    CAND_HAS_HID_SERVICE = 1 << 0,
    CAND_APPEARANCE_JOYSTICK = 1 << 1,
    CAND_APPEARANCE_GAMEPAD = 1 << 2,
    CAND_NAME_HINT = 1 << 3,
    CAND_NAME_XBOX = 1 << 4,
    CAND_NAME_PLAYSTATION = 1 << 5,
    CAND_NAME_8BITDO = 1 << 6,
    CAND_MFG_XBOX = 1 << 7,
    CAND_PAIRING_PROBE = 1 << 8,
};

enum FailureStage {
    FAIL_NONE = 0,
    FAIL_SCAN,
    FAIL_CONNECT,
    FAIL_AUTH,
    FAIL_DISCOVERY,
};

static const char* failure_stage_name(FailureStage stage)
{
    switch (stage) {
        case FAIL_NONE:
            return "none";
        case FAIL_SCAN:
            return "scan";
        case FAIL_CONNECT:
            return "connect";
        case FAIL_AUTH:
            return "auth";
        case FAIL_DISCOVERY:
            return "discovery";
        default:
            return "unknown";
    }
}

static const uint32_t kScanIntervalPairingMs = 300;
static const uint32_t kScanIntervalBackgroundMs = 14000;
static const uint32_t kScanDurationPairingSeconds = 2;
static const uint32_t kScanDurationBackgroundSeconds = 4;
static const uint32_t kPostScanConnectDelayMs = 500;
static const int kPairingProbeMinRssi = -70;
static const int kXboxConnectAttempts = 4;
static const uint32_t kXboxConnectRetryMs = 100;
static const uint8_t kConnectTimeoutSeconds = 6;
static const uint32_t kPostConnectDiscoveryDelayMs = 600;
static const uint32_t kAuthTimeoutMs = 8000;
static const uint32_t kCooldownConnectFailMs = 8000;
static const uint32_t kCooldownPairingConnectFailMs = 2000;
static const uint32_t kCooldownAuthFailMs = 30000;
static const uint32_t kCooldownDiscoveryFailMs = 15000;
static const uint32_t kDiscoveryMinFreeI8 = 6000;
static const uint32_t kDiscoveryMinLargestI8 = 1536;
static const uint32_t kSubscribeMinFreeI8 = 5000;
static const uint32_t kSubscribeMinLargestI8 = 1024;

struct ReportBinding {
    BLERemoteCharacteristic* characteristic;
    uint16_t handle;
    uint8_t report_id;
    uint8_t report_type;
};

struct CandidateInfo {
    bool valid;
    esp_bd_addr_t addr;
    esp_ble_addr_type_t addr_type;
    std::string name;
    uint16_t appearance;
    bool have_appearance;
    uint16_t flags;
    uint8_t score;
};

struct ConnectedInfo {
    bool valid;
    esp_bd_addr_t addr;
    esp_ble_addr_type_t addr_type;
    std::string name;
    uint16_t appearance;
    bool have_appearance;
    uint16_t flags;
};

struct HogpQuirkProfile {
    uint32_t parse_hints;
    uint32_t decode_quirks;
    bool remap_playstation_face;
    const char* first_rule;
};

struct CooldownEntry {
    bool valid;
    esp_bd_addr_t addr;
    uint32_t until_ms;
};

struct HogpState {
    bool initialized;
    bool init_failed;
    bool scan_running;
    bool scan_finished;
    bool connecting;
    bool connected;
    bool ready;
    bool packet_pending;
    bool pairing_enabled;
    uint32_t next_scan_ms;
    uint32_t next_connect_ms;
    uint32_t discovery_ready_ms;

    CandidateInfo candidate;
    ConnectedInfo connected_device;

    BLEClient* client;
    HIDGamepad parser;
    std::vector<ReportBinding> reports;
    HogpQuirkProfile quirks;

    uint32_t report_map_hash;
    uint16_t buttons;
    uint64_t raw_buttons;
    int pad_index;

    uint32_t rx_packets;
    uint32_t rx_decode_ok;
    uint32_t rx_decode_fail;
    uint32_t rx_drop_unbound;
    uint32_t rx_last_len;
    uint32_t rx_last_report_id;
    uint16_t rx_last_buttons;

    uint32_t rx_diag_next_ms;
    uint32_t state_diag_next_ms;
    FailureStage last_failure_stage;

    CooldownEntry cooldown[8];
    uint8_t cooldown_cursor;
};

static const int kMaxBleControllers = 2;
static HogpState _hogp_slots[kMaxBleControllers] = {};
// Per-task: HogpScope is used from the NimBLE host task (notifications,
// connect/auth callbacks), the main loop and the emulator thread. As a plain
// global, every input notification swapped the slot under the main loop,
// which then ran scan/connect logic against the wrong controller's state.
static __thread HogpState* _active_hogp = &_hogp_slots[0];
static HogpState* _scan_hogp = 0;

#define _hogp (*_active_hogp)

static int active_slot_number()
{
    return (int)(_active_hogp - _hogp_slots) + 1;
}

static void format_ble_address(const esp_bd_addr_t addr, char* out, size_t out_len)
{
    if (!out || out_len == 0)
        return;
    snprintf(out, out_len, "%02X:%02X:%02X:%02X:%02X:%02X",
        addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

static uint32_t _scan_window_id = 0;
static uint32_t _scan_seen = 0;
static uint32_t _scan_hid_seen = 0;
static uint32_t _scan_no_hid = 0;
static uint32_t _scan_cooldown = 0;
static uint32_t _scan_adv_logged = 0;
static uint32_t _classic_scan_block_log_next_ms = 0;
static uint32_t _pairing_unavailable_log_next_ms = 0;
static bool bdaddr_equal(const esp_bd_addr_t a, const esp_bd_addr_t b);

class HogpScope {
public:
    explicit HogpScope(HogpState* slot) : _previous(_active_hogp)
    {
        if (slot)
            _active_hogp = slot;
    }

    ~HogpScope()
    {
        _active_hogp = _previous;
    }

private:
    HogpState* _previous;
};

static void initialize_hogp_slot(HogpState& slot)
{
    slot = HogpState();
    slot.pad_index = -1;
    slot.candidate.addr_type = BLE_ADDR_TYPE_PUBLIC;
    slot.connected_device.addr_type = BLE_ADDR_TYPE_PUBLIC;
    slot.quirks.parse_hints = HIDGamepad::PARSE_HINT_NONE;
    slot.quirks.decode_quirks = HIDGamepad::DECODE_QUIRK_NONE;
    slot.quirks.remap_playstation_face = false;
    slot.quirks.first_rule = 0;
}

static int ready_hogp_count()
{
    int count = 0;
    for (int i = 0; i < kMaxBleControllers; ++i) {
        if (_hogp_slots[i].ready)
            ++count;
    }
    return count;
}

static HogpState* first_available_hogp_slot()
{
    for (int i = 0; i < kMaxBleControllers; ++i) {
        HogpState& slot = _hogp_slots[i];
        if (!slot.connecting && !slot.connected && !slot.candidate.valid)
            return &slot;
    }
    return 0;
}

static bool hogp_connection_setup_pending()
{
    for (int i = 0; i < kMaxBleControllers; ++i) {
        const HogpState& slot = _hogp_slots[i];
        if (slot.candidate.valid || slot.connecting || (slot.connected && !slot.ready))
            return true;
    }
    return false;
}

static HogpState* find_hogp_by_client(BLEClient* client)
{
    if (!client)
        return 0;
    for (int i = 0; i < kMaxBleControllers; ++i) {
        if (_hogp_slots[i].client == client)
            return &_hogp_slots[i];
    }
    return 0;
}

static HogpState* find_hogp_by_characteristic(BLERemoteCharacteristic* characteristic)
{
    if (!characteristic)
        return 0;
    for (int i = 0; i < kMaxBleControllers; ++i) {
        HogpState& slot = _hogp_slots[i];
        for (size_t j = 0; j < slot.reports.size(); ++j) {
            if (slot.reports[j].characteristic == characteristic)
                return &slot;
        }
    }
    return 0;
}

static HogpState* find_hogp_by_conn_handle(uint16_t conn_handle)
{
    for (int i = 0; i < kMaxBleControllers; ++i) {
        HogpState& slot = _hogp_slots[i];
        if (slot.client && slot.client->isConnected() &&
            slot.client->getConnId() == conn_handle)
            return &slot;
    }
    return 0;
}

static bool hogp_address_is_active(const esp_bd_addr_t addr)
{
    for (int i = 0; i < kMaxBleControllers; ++i) {
        const HogpState& slot = _hogp_slots[i];
        if (slot.connected_device.valid && bdaddr_equal(slot.connected_device.addr, addr))
            return true;
        if (slot.candidate.valid && bdaddr_equal(slot.candidate.addr, addr))
            return true;
    }
    return false;
}

static void copy_ble_address(esp_bd_addr_t dst, const BLEAddress& address)
{
    const uint8_t* native = address.getNative();
    for (int i = 0; i < 6; ++i)
        dst[i] = native[5 - i];
}

static BLEAddress make_ble_address(const esp_bd_addr_t addr, esp_ble_addr_type_t type)
{
    return BLEAddress((uint8_t*)addr, (uint8_t)type);
}

static bool ble_controller_ready_after_init()
{
    return BLEDevice::getInitialized();
}

static void log_pairing_unavailable(const char* reason)
{
    uint32_t now = millis();
    if ((int32_t)(now - _pairing_unavailable_log_next_ms) < 0)
        return;
    _pairing_unavailable_log_next_ms = now + 2000;
    BLE_LOG("pairing unavailable reason=%s free_i8=%lu largest_i8=%lu",
        reason ? reason : "unknown",
        (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
}

static bool ble_heap_guard_ok(const char* phase, uint32_t min_free, uint32_t min_largest)
{
    const uint32_t free_i8 =
        (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const uint32_t largest_i8 =
        (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (free_i8 >= min_free && largest_i8 >= min_largest)
        return true;

    BLE_LOG("slot=%d heap guard failed phase=%s free=%lu/%lu largest=%lu/%lu",
        active_slot_number(), phase ? phase : "unknown",
        (unsigned long)free_i8, (unsigned long)min_free,
        (unsigned long)largest_i8, (unsigned long)min_largest);
    return false;
}

struct QuirkRule {
    const char* name;
    uint16_t required_all_flags;
    uint16_t required_any_flags;
    uint32_t parse_hints;
    uint32_t decode_quirks;
    bool remap_playstation_face;
};

static const QuirkRule kQuirkRules[] = {
    {"xbox-ble-signature", (uint16_t)(CAND_HAS_HID_SERVICE | CAND_MFG_XBOX), 0,
     HIDGamepad::PARSE_HINT_STRONG_XBOX_BLE, HIDGamepad::DECODE_QUIRK_XBOX_BLE, false},
    {"xbox-ble-retry-signature", (uint16_t)(CAND_APPEARANCE_GAMEPAD | CAND_MFG_XBOX), 0,
     HIDGamepad::PARSE_HINT_STRONG_XBOX_BLE, HIDGamepad::DECODE_QUIRK_XBOX_BLE, false},
    {"playstation-face-remap", 0, CAND_NAME_PLAYSTATION,
     HIDGamepad::PARSE_HINT_NONE, HIDGamepad::DECODE_QUIRK_NONE, true},
};

static bool contains_token_ci(const std::string& text, const char* token)
{
    if (!token || !*token || text.empty())
        return false;
    size_t token_len = strlen(token);
    if (token_len > text.size())
        return false;
    for (size_t i = 0; i + token_len <= text.size(); ++i) {
        size_t j = 0;
        while (j < token_len) {
            char a = text[i + j];
            char b = token[j];
            if (a >= 'A' && a <= 'Z')
                a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z')
                b = (char)(b - 'A' + 'a');
            if (a != b)
                break;
            ++j;
        }
        if (j == token_len)
            return true;
    }
    return false;
}

static bool matches_bytes(const std::string& data, const uint8_t* bytes, size_t len)
{
    return bytes && data.size() == len && memcmp(data.data(), bytes, len) == 0;
}

static bool has_xbox_manufacturer_signature(const std::string& data)
{
    static const uint8_t kXboxMfgShort[] = {0x06, 0x00, 0x00};
    static const uint8_t kXboxMfgLong[] = {0x06, 0x00, 0x03, 0x00, 0x80};
    return matches_bytes(data, kXboxMfgShort, sizeof(kXboxMfgShort)) ||
           matches_bytes(data, kXboxMfgLong, sizeof(kXboxMfgLong));
}

static bool bdaddr_equal(const esp_bd_addr_t a, const esp_bd_addr_t b)
{
    return memcmp(a, b, sizeof(esp_bd_addr_t)) == 0;
}

static uint32_t fnv1a32(const uint8_t* data, size_t len)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; ++i) {
        h ^= (uint32_t)data[i];
        h *= 16777619u;
    }
    return h;
}

static bool is_playstation_name(const std::string& name)
{
    return contains_token_ci(name, "wireless controller") ||
           contains_token_ci(name, "dualshock") ||
           contains_token_ci(name, "dualsense");
}

static uint16_t remap_playstation_buttons(uint16_t buttons)
{
    uint16_t remapped = buttons & ~(GENERIC_FIRE_A | GENERIC_FIRE_B | GENERIC_FIRE_X | GENERIC_FIRE_Y);

    if (buttons & GENERIC_FIRE_A)
        remapped |= GENERIC_FIRE_X;
    if (buttons & GENERIC_FIRE_B)
        remapped |= GENERIC_FIRE_A;
    if (buttons & GENERIC_FIRE_X)
        remapped |= GENERIC_FIRE_B;
    if (buttons & GENERIC_FIRE_Y)
        remapped |= GENERIC_FIRE_Y;

    return remapped;
}

static uint16_t remap_xbox_series_face_buttons(uint16_t buttons, const uint8_t* data, size_t length)
{
    // Xbox BLE face buttons use bits 0, 1, 3, and 4.
    if (!data || length != 16)
        return buttons;

    uint16_t remapped = buttons & ~(GENERIC_FIRE_A | GENERIC_FIRE_B | GENERIC_FIRE_X | GENERIC_FIRE_Y);
    const uint8_t face = data[13];
    if (face & 0x01)
        remapped |= GENERIC_FIRE_A;
    if (face & 0x02)
        remapped |= GENERIC_FIRE_B;
    if (face & 0x08)
        remapped |= GENERIC_FIRE_X;
    if (face & 0x10)
        remapped |= GENERIC_FIRE_Y;
    return remapped;
}

static uint16_t classify_candidate_flags(BLEAdvertisedDevice& device, std::string* out_name, uint16_t* out_appearance, bool* out_have_appearance)
{
    uint16_t flags = 0;

    std::string name;
    if (device.haveName()) {
        name = device.getName();
        if (contains_token_ci(name, "xbox"))
            flags |= CAND_NAME_XBOX;
        if (contains_token_ci(name, "8bitdo"))
            flags |= CAND_NAME_8BITDO;
        if (!(flags & CAND_NAME_XBOX) && is_playstation_name(name))
            flags |= CAND_NAME_PLAYSTATION;
        if (contains_token_ci(name, "gamepad") ||
            contains_token_ci(name, "controller") ||
            contains_token_ci(name, "joystick"))
            flags |= CAND_NAME_HINT;
        if (flags & (CAND_NAME_XBOX | CAND_NAME_8BITDO | CAND_NAME_PLAYSTATION))
            flags |= CAND_NAME_HINT;
    }

    if (device.haveServiceUUID() && device.isAdvertisingService(BLEUUID((uint16_t)BLE_HID_SERVICE_UUID)))
        flags |= CAND_HAS_HID_SERVICE;

    bool have_appearance = false;
    uint16_t appearance = 0;
    if (device.haveAppearance()) {
        appearance = (uint16_t)device.getAppearance();
        have_appearance = true;
        if (appearance == BLE_APPEARANCE_JOYSTICK)
            flags |= CAND_APPEARANCE_JOYSTICK;
        if (appearance == BLE_APPEARANCE_GAMEPAD)
            flags |= CAND_APPEARANCE_GAMEPAD;
    }

    if (device.haveManufacturerData() && has_xbox_manufacturer_signature(device.getManufacturerData()))
        flags |= CAND_MFG_XBOX;

    if (out_name)
        *out_name = name;
    if (out_appearance)
        *out_appearance = appearance;
    if (out_have_appearance)
        *out_have_appearance = have_appearance;

    return flags;
}

static bool has_xbox_pairing_signature(uint16_t flags)
{
    const uint16_t required =
        CAND_APPEARANCE_GAMEPAD | CAND_MFG_XBOX;
    return (flags & required) == required;
}

static bool advertised_device_is_connectable(BLEAdvertisedDevice& device)
{
    const unsigned adv_type = (unsigned)device.getAdvType();
#if defined(CONFIG_BT_NIMBLE_EXT_ADV) && CONFIG_BT_NIMBLE_EXT_ADV
    return device.isConnectable();
#else
    // NimBLE-Arduino 1.4.x treats the legacy event type as extended-
    // advertisement property bits in isConnectable(). For legacy reports,
    // ADV_IND (0) and ADV_DIRECT_IND (1) are the connectable types.
    return adv_type == 0 || adv_type == 1;
#endif
}

static bool is_candidate_admissible(uint16_t flags)
{
    // Xbox advertisements can omit the HID UUID briefly after a failed connection.
    return (flags & CAND_HAS_HID_SERVICE) || has_xbox_pairing_signature(flags);
}

static uint8_t score_candidate_flags(uint16_t flags)
{
    int score = 0;

    if (flags & CAND_HAS_HID_SERVICE)
        score += 24;
    if (flags & CAND_APPEARANCE_GAMEPAD)
        score += 8;
    if (flags & CAND_APPEARANCE_JOYSTICK)
        score += 6;

    if (flags & CAND_MFG_XBOX)
        score += 4;
    if (flags & CAND_NAME_XBOX)
        score += 2;
    if (flags & CAND_NAME_8BITDO)
        score += 1;
    if (flags & CAND_NAME_PLAYSTATION)
        score += 1;
    if (flags & CAND_NAME_HINT)
        score += 1;

    if (score <= 0)
        score = 1;
    if (score > 255)
        score = 255;
    return (uint8_t)score;
}

static uint8_t score_pairing_probe(int rssi, uint16_t flags)
{
    // Keep unclassified probes below a positively advertised HOGP device,
    // while preferring a nearby controller over other connectable devices.
    int score = (rssi + 100) / 4 + 1;
    if (flags & CAND_NAME_HINT)
        score += 2;
    if (score < 1)
        score = 1;
    if (score > 20)
        score = 20;
    return (uint8_t)score;
}

static HogpQuirkProfile resolve_quirks(uint16_t flags)
{
    HogpQuirkProfile out = {
        HIDGamepad::PARSE_HINT_NONE,
        HIDGamepad::DECODE_QUIRK_NONE,
        false,
        0,
    };

    for (size_t i = 0; i < sizeof(kQuirkRules) / sizeof(kQuirkRules[0]); ++i) {
        const QuirkRule& r = kQuirkRules[i];
        if (r.required_all_flags && ((flags & r.required_all_flags) != r.required_all_flags))
            continue;
        if (r.required_any_flags && !(flags & r.required_any_flags))
            continue;

        out.parse_hints |= r.parse_hints;
        out.decode_quirks |= r.decode_quirks;
        out.remap_playstation_face = out.remap_playstation_face || r.remap_playstation_face;
        if (!out.first_rule)
            out.first_rule = r.name;
    }

    return out;
}

static void schedule_scan(uint32_t delay_ms)
{
    _hogp.next_scan_ms = millis() + delay_ms;
}

static uint32_t next_scan_delay_ms()
{
    return _hogp.pairing_enabled ? kScanIntervalPairingMs : kScanIntervalBackgroundMs;
}

static uint32_t scan_window_seconds()
{
    return _hogp.pairing_enabled ? kScanDurationPairingSeconds : kScanDurationBackgroundSeconds;
}

static void clear_candidate()
{
    _hogp.candidate.valid = false;
    _hogp.next_connect_ms = 0;
    memset(_hogp.candidate.addr, 0, sizeof(_hogp.candidate.addr));
    _hogp.candidate.addr_type = BLE_ADDR_TYPE_PUBLIC;
    _hogp.candidate.name.clear();
    _hogp.candidate.appearance = 0;
    _hogp.candidate.have_appearance = false;
    _hogp.candidate.flags = 0;
    _hogp.candidate.score = 0;
}

static void clear_connected_device()
{
    _hogp.connected_device.valid = false;
    memset(_hogp.connected_device.addr, 0, sizeof(_hogp.connected_device.addr));
    _hogp.connected_device.addr_type = BLE_ADDR_TYPE_PUBLIC;
    _hogp.connected_device.name.clear();
    _hogp.connected_device.appearance = 0;
    _hogp.connected_device.have_appearance = false;
    _hogp.connected_device.flags = 0;
}

static void clear_cooldown_for(const esp_bd_addr_t addr)
{
    for (size_t i = 0; i < sizeof(_hogp.cooldown) / sizeof(_hogp.cooldown[0]); ++i) {
        if (!_hogp.cooldown[i].valid)
            continue;
        if (bdaddr_equal(_hogp.cooldown[i].addr, addr))
            _hogp.cooldown[i].valid = false;
    }
}

static bool is_on_cooldown(const esp_bd_addr_t addr, uint32_t* remaining_ms)
{
    uint32_t now = millis();
    for (size_t i = 0; i < sizeof(_hogp.cooldown) / sizeof(_hogp.cooldown[0]); ++i) {
        CooldownEntry& e = _hogp.cooldown[i];
        if (!e.valid)
            continue;
        if (!bdaddr_equal(e.addr, addr))
            continue;
        int32_t delta = (int32_t)(e.until_ms - now);
        if (delta <= 0) {
            e.valid = false;
            break;
        }
        if (remaining_ms)
            *remaining_ms = (uint32_t)delta;
        return true;
    }
    if (remaining_ms)
        *remaining_ms = 0;
    return false;
}

static void mark_cooldown(const esp_bd_addr_t addr, uint32_t cooldown_ms)
{
    if (!cooldown_ms)
        return;

    size_t slot = (size_t)-1;
    uint32_t now = millis();

    for (size_t i = 0; i < sizeof(_hogp.cooldown) / sizeof(_hogp.cooldown[0]); ++i) {
        CooldownEntry& e = _hogp.cooldown[i];
        if (!e.valid) {
            slot = i;
            break;
        }
        if (bdaddr_equal(e.addr, addr)) {
            slot = i;
            break;
        }
        if ((int32_t)(e.until_ms - now) <= 0) {
            slot = i;
            break;
        }
    }

    if (slot == (size_t)-1) {
        slot = _hogp.cooldown_cursor++ % (uint8_t)(sizeof(_hogp.cooldown) / sizeof(_hogp.cooldown[0]));
    }

    _hogp.cooldown[slot].valid = true;
    memcpy(_hogp.cooldown[slot].addr, addr, sizeof(esp_bd_addr_t));
    _hogp.cooldown[slot].until_ms = millis() + cooldown_ms;
}

static void release_pad_slot(bool emit_report)
{
    if (_hogp.pad_index < 0)
        return;
    generic_pad_release(&_hogp);
    _hogp.pad_index = -1;
    _hogp.buttons = 0;
    _hogp.raw_buttons = 0;
    if (emit_report)
        _hogp.packet_pending = true;
}

static void reset_runtime_state(bool emit_release)
{
    release_pad_slot(emit_release);
    _hogp.connecting = false;
    _hogp.connected = false;
    _hogp.ready = false;
    _hogp.discovery_ready_ms = 0;
    _hogp.reports.clear();
    _hogp.parser = HIDGamepad();
    _hogp.quirks = resolve_quirks(0);
    _hogp.report_map_hash = 0;
    clear_connected_device();
}

static void maybe_log_state_diag()
{
    uint32_t now = millis();
    if ((int32_t)(now - _hogp.state_diag_next_ms) < 0)
        return;

    _hogp.state_diag_next_ms = now + 2000;
    if (!_hogp.pairing_enabled && !_hogp.scan_running && !_hogp.connecting &&
        !_hogp.connected && !_hogp.candidate.valid && _hogp.last_failure_stage == FAIL_NONE)
        return;

    BLE_LOG("slot=%d state pair=%d scan=%d candidate=%d connecting=%d connected=%d ready=%d pad=%d fail=%s",
        active_slot_number(), _hogp.pairing_enabled ? 1 : 0,
        _hogp.scan_running ? 1 : 0, _hogp.candidate.valid ? 1 : 0,
        _hogp.connecting ? 1 : 0, _hogp.connected ? 1 : 0,
        _hogp.ready ? 1 : 0,
        _hogp.pad_index, failure_stage_name(_hogp.last_failure_stage));
}

static void maybe_log_input_diag()
{
    uint32_t now = millis();
    if ((int32_t)(now - _hogp.rx_diag_next_ms) < 0)
        return;

    _hogp.rx_diag_next_ms = now + 1000;
    BLE_LOG("slot=%d input packets=%lu ok=%lu decode_fail=%lu unbound=%lu last_len=%lu report_id=%lu buttons=0x%04X raw=0x%llX",
        active_slot_number(), (unsigned long)_hogp.rx_packets,
        (unsigned long)_hogp.rx_decode_ok, (unsigned long)_hogp.rx_decode_fail,
        (unsigned long)_hogp.rx_drop_unbound, (unsigned long)_hogp.rx_last_len,
        (unsigned long)_hogp.rx_last_report_id, (unsigned)_hogp.rx_last_buttons,
        (unsigned long long)_hogp.raw_buttons);
}

static const ReportBinding* find_report_binding(uint16_t handle)
{
    for (size_t i = 0; i < _hogp.reports.size(); ++i) {
        if (_hogp.reports[i].handle == handle)
            return &_hogp.reports[i];
    }
    return 0;
}

static void push_buttons(uint16_t buttons, uint64_t raw_buttons)
{
    if (_hogp.pad_index < 0) {
        _hogp.pad_index = generic_pad_assign(&_hogp);
        if (_hogp.pad_index < 0) {
            BLE_LOG("slot=%d no free generic player slot", active_slot_number());
            return;
        }
        BLE_LOG("slot=%d assigned NES player=%d", active_slot_number(), _hogp.pad_index + 1);
    }
    if (buttons == _hogp.buttons && raw_buttons == _hogp.raw_buttons)
        return;
    _hogp.buttons = buttons;
    _hogp.raw_buttons = raw_buttons;
    generic_pad_update_ex(&_hogp, buttons, raw_buttons);
    _hogp.packet_pending = true;
}

static void enter_failure(FailureStage stage, const char* message, uint32_t cooldown_ms)
{
    _hogp.last_failure_stage = stage;
    BLE_LOG("slot=%d failure stage=%s reason=%s cooldown_ms=%lu",
        active_slot_number(), failure_stage_name(stage),
        message ? message : "unspecified", (unsigned long)cooldown_ms);

    if (_hogp.connected_device.valid)
        mark_cooldown(_hogp.connected_device.addr, cooldown_ms);
    else if (_hogp.candidate.valid)
        mark_cooldown(_hogp.candidate.addr, cooldown_ms);

    _hogp.ready = false;

    if (_hogp.client && _hogp.client->isConnected()) {
        _hogp.client->disconnect();
        return;
    }

    reset_runtime_state(true);
    clear_candidate();
    schedule_scan(next_scan_delay_ms());
}

static void on_input_report(BLERemoteCharacteristic* characteristic, uint8_t* data, size_t length, bool is_notify)
{
    HogpState* slot = find_hogp_by_characteristic(characteristic);
    if (!slot)
        return;
    HogpScope scope(slot);
    if (!_hogp.ready || !characteristic || !data || !length || !is_notify)
        return;

    _hogp.rx_packets++;
    _hogp.rx_last_len = (uint32_t)length;

    const ReportBinding* binding = find_report_binding(characteristic->getHandle());
    if (!binding || binding->report_type != BLE_HID_REPORT_TYPE_INPUT) {
        _hogp.rx_drop_unbound++;
        maybe_log_input_diag();
        return;
    }

    _hogp.rx_last_report_id = (uint32_t)binding->report_id;

    uint8_t packet[96];
    size_t offset = 0;
    packet[offset++] = 0xA1;

    const bool payload_includes_report_id =
        _hogp.parser.has_report_id() && binding->report_id != 0 && data[0] == binding->report_id;

    if (_hogp.parser.has_report_id() && !payload_includes_report_id)
        packet[offset++] = binding->report_id;

    if (offset + length > sizeof(packet))
        return;
    memcpy(packet + offset, data, length);

    HIDGamepad::DecodedState decoded;
    if (!_hogp.parser.decode_report_state(packet, (int)(offset + length), &decoded, _hogp.quirks.decode_quirks)) {
        _hogp.rx_decode_fail++;
        maybe_log_input_diag();
        return;
    }

    _hogp.rx_decode_ok++;

    uint16_t buttons = decoded.buttons;
    if (_hogp.quirks.decode_quirks & HIDGamepad::DECODE_QUIRK_XBOX_BLE)
        buttons = remap_xbox_series_face_buttons(buttons, data, length);
    if (_hogp.quirks.remap_playstation_face)
        buttons = remap_playstation_buttons(buttons);

    _hogp.rx_last_buttons = buttons;
    push_buttons(buttons, decoded.raw_buttons);
    maybe_log_input_diag();
}

class HogpSecurityCallbacks : public BLESecurityCallbacks {
public:
    uint32_t onPassKeyRequest() override
    {
        return 0;
    }

    void onPassKeyNotify(uint32_t pass_key) override
    {
    }

    bool onSecurityRequest() override
    {
        return true;
    }

    void onAuthenticationComplete(ble_gap_conn_desc* desc) override
    {
        HogpState* slot = desc ? find_hogp_by_conn_handle(desc->conn_handle) : 0;
        if (!slot)
            return;
        HogpScope scope(slot);
        if (!_hogp.connected)
            return;

        if (desc->sec_state.encrypted) {
            BLE_LOG("slot=%d authentication complete encrypted=1", active_slot_number());
            _hogp.discovery_ready_ms = millis() + kPostConnectDiscoveryDelayMs;
            return;
        }

        BLE_LOG("slot=%d authentication complete encrypted=0", active_slot_number());
        enter_failure(FAIL_AUTH, "BLE: authentication failed", kCooldownAuthFailMs);
    }

    bool onConfirmPIN(uint32_t pin) override
    {
        return true;
    }
};

class HogpClientCallbacks : public BLEClientCallbacks {
public:
    void onConnect(BLEClient* client) override
    {
        HogpState* slot = find_hogp_by_client(client);
        if (!slot)
            return;
        HogpScope scope(slot);
        _hogp.connecting = false;
        _hogp.connected = true;
        _hogp.ready = false;
        _hogp.discovery_ready_ms = 0;
        BLE_LOG("slot=%d client connected", active_slot_number());
    }

    void onDisconnect(BLEClient* client) override
    {
        HogpState* slot = find_hogp_by_client(client);
        if (!slot)
            return;
        HogpScope scope(slot);
        BLE_LOG("slot=%d client disconnected", active_slot_number());
        reset_runtime_state(true);
        clear_candidate();
        schedule_scan(next_scan_delay_ms());
    }
};

class HogpAdvertisedCallbacks : public BLEAdvertisedDeviceCallbacks {
public:
    void handle(BLEAdvertisedDevice& device)
    {
        if (!_scan_hogp)
            return;
        HogpScope scope(_scan_hogp);
        if (_hogp.connecting || _hogp.connected)
            return;

        _scan_seen++;

        CandidateInfo candidate = {};
        BLEAddress address = device.getAddress();
        copy_ble_address(candidate.addr, address);
        candidate.addr_type = (esp_ble_addr_type_t)device.getAddressType();
        if (hogp_address_is_active(candidate.addr))
            return;
        candidate.flags = classify_candidate_flags(device, &candidate.name, &candidate.appearance, &candidate.have_appearance);
        const bool connectable = advertised_device_is_connectable(device);
        const unsigned adv_type = (unsigned)device.getAdvType();
        // A pad we already bonded with may reconnect with minimal or directed
        // advertising (no HID UUID, no name). Outside pairing mode those
        // were ignored, so bonded pads could not come back on their own.
        const bool bonded = connectable && NimBLEDevice::isBonded(address);
        if (bonded)
            candidate.flags |= CAND_HAS_HID_SERVICE;
        const bool advertised_hogp = is_candidate_admissible(candidate.flags);
        const bool pairing_probe =
            _hogp.pairing_enabled && connectable && !advertised_hogp &&
            device.getRSSI() >= kPairingProbeMinRssi;
        if (pairing_probe)
            candidate.flags |= CAND_PAIRING_PROBE;
        const bool admissible = advertised_hogp || pairing_probe;
        const uint16_t interesting_flags =
            CAND_HAS_HID_SERVICE | CAND_NAME_HINT | CAND_NAME_XBOX | CAND_MFG_XBOX;
        if (_hogp.pairing_enabled &&
            (_scan_adv_logged < 12 || ((candidate.flags & interesting_flags) && _scan_adv_logged < 20))) {
            _scan_adv_logged++;
            char addr_text[18];
            format_ble_address(candidate.addr, addr_text, sizeof(addr_text));
            BLE_LOG("scan=%lu adv addr=%s name='%s' flags=0x%03X appearance=%s0x%04X rssi=%d type=%u connectable=%d probe=%d admissible=%d",
                (unsigned long)_scan_window_id, addr_text,
                candidate.name.empty() ? "" : candidate.name.c_str(),
                (unsigned)candidate.flags, candidate.have_appearance ? "" : "none/",
                (unsigned)candidate.appearance, device.getRSSI(), adv_type,
                connectable ? 1 : 0, pairing_probe ? 1 : 0,
                admissible ? 1 : 0);
        }

        if (!admissible) {
            _scan_no_hid++;
            return;
        }

        _scan_hid_seen++;

        uint32_t cooldown_remaining = 0;
        if (is_on_cooldown(candidate.addr, &cooldown_remaining)) {
            // Xbox controllers fail transiently on the ESP32 link layer, so
            // while pairing, retry them rather than probing other devices.
            if (!(_hogp.pairing_enabled &&
                  has_xbox_pairing_signature(candidate.flags))) {
                _scan_cooldown++;
                return;
            }
            BLE_LOG("slot=%d Xbox candidate bypassing cooldown remaining_ms=%lu",
                active_slot_number(), (unsigned long)cooldown_remaining);
        }

        candidate.score = score_candidate_flags(candidate.flags);
        if (pairing_probe)
            candidate.score = score_pairing_probe(device.getRSSI(), candidate.flags);
        candidate.valid = true;

        if (_hogp.candidate.valid && candidate.score <= _hogp.candidate.score)
            return;

        _hogp.candidate = candidate;
        char addr_text[18];
        format_ble_address(_hogp.candidate.addr, addr_text, sizeof(addr_text));
        BLE_LOG("slot=%d selected candidate addr=%s name='%s' score=%u flags=0x%02X",
            active_slot_number(), addr_text,
            _hogp.candidate.name.empty() ? "" : _hogp.candidate.name.c_str(),
            (unsigned)_hogp.candidate.score, (unsigned)_hogp.candidate.flags);
    }

    void onResult(BLEAdvertisedDevice* device) override
    {
        if (!device)
            return;
        handle(*device);
    }
};

static HogpSecurityCallbacks _security_callbacks;
static HogpClientCallbacks _client_callbacks;
static HogpAdvertisedCallbacks _advertised_callbacks;

static void configure_security()
{
    BLEDevice::setSecurityCallbacks(&_security_callbacks);
    BLEDevice::setSecurityAuth(true, false, true);
    BLEDevice::setSecurityIOCap(ESP_IO_CAP_NONE);
    BLEDevice::setSecurityInitKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    BLEDevice::setSecurityRespKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    BLEDevice::setMTU(185);
    BLEDevice::setPower(ESP_PWR_LVL_P9);
}

static void configure_scan(BLEScan* scan)
{
    scan->setActiveScan(true);
    scan->setInterval(80);
    scan->setWindow(40);
    scan->setMaxResults(0);
    scan->setAdvertisedDeviceCallbacks(&_advertised_callbacks);
}

static bool restart_nimble_after_xbox_link_failure()
{
    if (ready_hogp_count() != 0)
        return false;

    BLE_LOG("restarting NimBLE after persistent Xbox link failure free_i8=%lu largest_i8=%lu",
        (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

    bool pairing[kMaxBleControllers];
    _scan_hogp = 0;
    for (int i = 0; i < kMaxBleControllers; ++i) {
        pairing[i] = _hogp_slots[i].pairing_enabled;
        if (_hogp_slots[i].client) {
            BLEDevice::deleteClient(_hogp_slots[i].client);
            _hogp_slots[i].client = 0;
        }
    }

    BLEDevice::deinit(false);
    delay(100);
    BLEDevice::init("emu32");
    configure_security();

    BLEScan* scan = BLEDevice::getScan();
    if (!scan)
        return false;
    scan->clearResults();
    configure_scan(scan);

    for (int i = 0; i < kMaxBleControllers; ++i) {
        initialize_hogp_slot(_hogp_slots[i]);
        _hogp_slots[i].initialized = true;
        _hogp_slots[i].pairing_enabled = pairing[i];
        _hogp_slots[i].next_scan_ms =
            millis() + (i == 0 ? kScanIntervalPairingMs : kScanIntervalBackgroundMs);
        _hogp_slots[i].state_diag_next_ms = millis() + 1000;
        _hogp_slots[i].rx_diag_next_ms = millis() + 1000;
    }
    BLE_LOG("NimBLE restart complete free_i8=%lu largest_i8=%lu",
        (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    return true;
}

static void on_scan_complete(BLEScanResults scan_results)
{
    (void)scan_results;
    HogpState* slot = _scan_hogp;
    _scan_hogp = 0;
    if (!slot)
        return;
    HogpScope scope(slot);
    _hogp.scan_finished = true;

    if (_hogp.candidate.valid)
        _hogp.next_connect_ms = millis() + kPostScanConnectDelayMs;
    // Clear this last: ble_hogp_update() runs on another core and may connect
    // as soon as it sees the scan finish.
    _hogp.scan_running = false;
    BLE_LOG("slot=%d scan=%lu complete seen=%lu hid=%lu rejected=%lu cooldown=%lu candidate=%d",
        active_slot_number(), (unsigned long)_scan_window_id,
        (unsigned long)_scan_seen, (unsigned long)_scan_hid_seen,
        (unsigned long)_scan_no_hid, (unsigned long)_scan_cooldown,
        _hogp.candidate.valid ? 1 : 0);
    if (!_hogp.candidate.valid)
        schedule_scan(next_scan_delay_ms());
}

static void maybe_start_scan()
{
    if (_scan_hogp || hogp_connection_setup_pending() ||
        ready_hogp_count() >= kMaxBleControllers ||
        _hogp.scan_running || _hogp.candidate.valid || _hogp.connecting || _hogp.connected)
        return;

    int classic_connected = hid_connected_count();
    if (classic_connected > 0) {
        uint32_t now = millis();
        if ((int32_t)(now - _classic_scan_block_log_next_ms) >= 0) {
            _classic_scan_block_log_next_ms = now + 5000;
            BLE_LOG("scan paused because %d Classic controller(s) are connected", classic_connected);
        }
        schedule_scan(kScanIntervalBackgroundMs);
        return;
    }

    if ((int32_t)(millis() - _hogp.next_scan_ms) < 0)
        return;

    BLEScan* scan = BLEDevice::getScan();
    if (!scan) {
        enter_failure(FAIL_SCAN, "BLE: scanner unavailable", kScanIntervalBackgroundMs);
        return;
    }

    scan->clearResults();
    _hogp.scan_finished = false;
    _hogp.next_connect_ms = 0;
    _scan_window_id++;
    _scan_seen = 0;
    _scan_hid_seen = 0;
    _scan_no_hid = 0;
    _scan_cooldown = 0;
    _scan_adv_logged = 0;

    const uint32_t window_s = scan_window_seconds();
    scan->setDuplicateFilter(true);
    scan->setWindow(40);
    _scan_hogp = &_hogp;
    BLE_LOG("slot=%d scan=%lu start duration_s=%lu pairing=%d",
        active_slot_number(), (unsigned long)_scan_window_id,
        (unsigned long)window_s, _hogp.pairing_enabled ? 1 : 0);
    if (!scan->start(window_s, on_scan_complete, false)) {
        _scan_hogp = 0;
        enter_failure(FAIL_SCAN, "BLE: failed to start scan", kScanIntervalBackgroundMs);
        return;
    }

    _hogp.scan_running = true;
}

static void promote_candidate_to_connected()
{
    _hogp.connected_device.valid = true;
    memcpy(_hogp.connected_device.addr, _hogp.candidate.addr, sizeof(esp_bd_addr_t));
    _hogp.connected_device.addr_type = _hogp.candidate.addr_type;
    _hogp.connected_device.name = _hogp.candidate.name;
    _hogp.connected_device.appearance = _hogp.candidate.appearance;
    _hogp.connected_device.have_appearance = _hogp.candidate.have_appearance;
    _hogp.connected_device.flags = _hogp.candidate.flags;
    _hogp.quirks = resolve_quirks(_hogp.connected_device.flags);
}

static bool connect_candidate()
{
    if (!_hogp.candidate.valid)
        return false;

    if (!_hogp.client) {
        _hogp.client = BLEDevice::createClient();
        if (!_hogp.client) {
            clear_candidate();
            schedule_scan(next_scan_delay_ms());
            _hogp.last_failure_stage = FAIL_CONNECT;
            return false;
        }
        _hogp.client->setClientCallbacks(&_client_callbacks, false);
        // connect() blocks the main loop. NimBLE's default is 30 s per
        // attempt (x4 for Xbox) when the pad has gone away.
        _hogp.client->setConnectTimeout(kConnectTimeoutSeconds);
    }

    BLEAddress address = make_ble_address(_hogp.candidate.addr, _hogp.candidate.addr_type);
    const char* name = _hogp.candidate.name.empty() ? "HOGP device" : _hogp.candidate.name.c_str();
    char addr_text[18];
    format_ble_address(_hogp.candidate.addr, addr_text, sizeof(addr_text));
    BLE_LOG("slot=%d connecting addr=%s type=%d name='%s'",
        active_slot_number(), addr_text, (int)_hogp.candidate.addr_type, name);

    _hogp.connecting = true;
    _hogp.connected = false;
    _hogp.ready = false;

    const bool xbox_candidate = has_xbox_pairing_signature(_hogp.candidate.flags);
    const int connect_attempts = xbox_candidate ? kXboxConnectAttempts : 1;
    bool ok = false;
    for (int attempt = 1; attempt <= connect_attempts; ++attempt) {
        if (attempt > 1)
            delay(kXboxConnectRetryMs);
        BLE_LOG("slot=%d connect attempt=%d/%d addr=%s",
            active_slot_number(), attempt, connect_attempts, addr_text);
        ok = _hogp.client->connect(address);
        if (ok && _hogp.client->isConnected())
            break;
        BLE_LOG("slot=%d connect attempt=%d failed status=%d",
            active_slot_number(), attempt, _hogp.client->getLastError());
    }
    _hogp.connecting = false;
    BLEDevice::getScan()->clearResults();

    if (!ok || !_hogp.client->isConnected()) {
        const bool restart_xbox_stack =
            xbox_candidate && _hogp.client->getLastError() == 574;
        mark_cooldown(_hogp.candidate.addr,
            _hogp.pairing_enabled ? kCooldownPairingConnectFailMs : kCooldownConnectFailMs);
        BLEDevice::deleteClient(_hogp.client);
        _hogp.client = 0;
        BLE_LOG("slot=%d connection failed addr=%s", active_slot_number(), addr_text);
        clear_candidate();
        schedule_scan(next_scan_delay_ms());
        _hogp.last_failure_stage = FAIL_CONNECT;
        if (restart_xbox_stack)
            restart_nimble_after_xbox_link_failure();
        return false;
    }

    if (_hogp.candidate.flags & CAND_PAIRING_PROBE) {
        BLE_LOG("slot=%d probing unclassified connection for HID service", active_slot_number());
        BLERemoteService* hid_service =
            _hogp.client->getService(BLEUUID((uint16_t)BLE_HID_SERVICE_UUID));
        if (!hid_service) {
            BLE_LOG("slot=%d probe rejected: HID service 0x1812 not found addr=%s",
                active_slot_number(), addr_text);
            mark_cooldown(_hogp.candidate.addr, kCooldownDiscoveryFailMs);
            _hogp.client->disconnect();
            clear_candidate();
            schedule_scan(next_scan_delay_ms());
            _hogp.last_failure_stage = FAIL_DISCOVERY;
            return false;
        }
        _hogp.candidate.flags |= CAND_HAS_HID_SERVICE;
        BLE_LOG("slot=%d probe accepted: HID service 0x1812 found addr=%s",
            active_slot_number(), addr_text);
    }

    if (!_hogp.client->secureConnection()) {
        BLE_LOG("slot=%d secureConnection failed addr=%s", active_slot_number(), addr_text);
        mark_cooldown(_hogp.candidate.addr, kCooldownAuthFailMs);
        _hogp.client->disconnect();
        clear_candidate();
        schedule_scan(next_scan_delay_ms());
        _hogp.last_failure_stage = FAIL_AUTH;
        return false;
    }

    promote_candidate_to_connected();
    clear_candidate();
    clear_cooldown_for(_hogp.connected_device.addr);
    _hogp.discovery_ready_ms = millis() + kPostConnectDiscoveryDelayMs;
    BLE_LOG("slot=%d link ready; HID discovery scheduled in %lu ms",
        active_slot_number(), (unsigned long)kPostConnectDiscoveryDelayMs);
    return true;
}

static void configure_protocol_mode(BLERemoteService* hid_service)
{
    BLERemoteCharacteristic* protocol_mode = hid_service->getCharacteristic(BLEUUID((uint16_t)BLE_PROTOCOL_MODE_UUID));
    if (protocol_mode && (protocol_mode->canWrite() || protocol_mode->canWriteNoResponse())) {
        uint8_t mode = BLE_PROTOCOL_MODE_REPORT;
        protocol_mode->writeValue(&mode, 1, protocol_mode->canWrite());
    }

    BLERemoteCharacteristic* control_point = hid_service->getCharacteristic(BLEUUID((uint16_t)BLE_HID_CONTROL_POINT_UUID));
    if (control_point && (control_point->canWrite() || control_point->canWriteNoResponse())) {
        uint8_t cmd = BLE_HID_CONTROL_EXIT_SUSPEND;
        control_point->writeValue(&cmd, 1, control_point->canWrite());
    }
}

static bool parse_report_map_with_quirks(const uint8_t* descriptor_data, int descriptor_len)
{
    _hogp.quirks = resolve_quirks(_hogp.connected_device.flags);
    return _hogp.parser.parse_descriptor(descriptor_data, descriptor_len, _hogp.quirks.parse_hints);
}

static bool discover_reports()
{
    BLE_LOG("slot=%d HID discovery start", active_slot_number());
    if (!_hogp.client || !_hogp.connected) {
        BLE_LOG("slot=%d HID discovery aborted: client/link unavailable", active_slot_number());
        return false;
    }
    if (!ble_heap_guard_ok("discovery:start",
            kDiscoveryMinFreeI8, kDiscoveryMinLargestI8))
        return false;

    BLERemoteService* hid_service = _hogp.client->getService(BLEUUID((uint16_t)BLE_HID_SERVICE_UUID));
    if (!hid_service) {
        BLE_LOG("slot=%d HID service 0x1812 not found", active_slot_number());
        return false;
    }
    BLE_LOG("slot=%d HID service found", active_slot_number());

    configure_protocol_mode(hid_service);

    BLERemoteCharacteristic* hid_info = hid_service->getCharacteristic(BLEUUID((uint16_t)BLE_HID_INFO_UUID));
    if (hid_info && hid_info->canRead())
        hid_info->readValue();

    BLERemoteCharacteristic* report_map = hid_service->getCharacteristic(BLEUUID((uint16_t)BLE_REPORT_MAP_UUID));
    if (!report_map || !report_map->canRead()) {
        BLE_LOG("slot=%d readable HID report map 0x2A4B not found", active_slot_number());
        return false;
    }

    if (!ble_heap_guard_ok("discovery:report_map",
            kDiscoveryMinFreeI8, kDiscoveryMinLargestI8))
        return false;

    NimBLEAttValue descriptor = report_map->readValue();
    if (!descriptor.size()) {
        BLE_LOG("slot=%d HID report map is empty", active_slot_number());
        return false;
    }

    const uint8_t* descriptor_data = descriptor.data();
    int descriptor_len = (int)descriptor.size();
    _hogp.report_map_hash = fnv1a32(descriptor_data, (size_t)descriptor_len);
    BLE_LOG("slot=%d report map len=%d hash=0x%08lX",
        active_slot_number(), descriptor_len, (unsigned long)_hogp.report_map_hash);

    if (!parse_report_map_with_quirks(descriptor_data, descriptor_len)) {
        BLE_LOG("slot=%d report map parser rejected descriptor", active_slot_number());
        return false;
    }

    BLE_LOG("slot=%d report map parsed report_ids=%d quirk=%s parse_hints=0x%lX decode_quirks=0x%lX",
        active_slot_number(), _hogp.parser.has_report_id() ? 1 : 0,
        _hogp.quirks.first_rule ? _hogp.quirks.first_rule : "none",
        (unsigned long)_hogp.quirks.parse_hints,
        (unsigned long)_hogp.quirks.decode_quirks);

    _hogp.reports.clear();
    std::vector<BLERemoteCharacteristic*>* characteristics = hid_service->getCharacteristics(true);
    if (!characteristics) {
        BLE_LOG("slot=%d HID characteristic enumeration failed", active_slot_number());
        return false;
    }

    for (std::vector<BLERemoteCharacteristic*>::const_iterator it = characteristics->begin();
         it != characteristics->end();
         ++it) {
        BLERemoteCharacteristic* characteristic = *it;
        if (!characteristic || !characteristic->getUUID().equals(BLEUUID((uint16_t)BLE_REPORT_UUID)))
            continue;

        if (!characteristic->canNotify())
            continue;

        uint8_t report_id = 0;
        uint8_t report_type = BLE_HID_REPORT_TYPE_INPUT;
        bool has_report_reference = false;

        BLERemoteDescriptor* ref = characteristic->getDescriptor(BLEUUID((uint16_t)BLE_REPORT_REFERENCE_UUID));
        if (ref) {
            NimBLEAttValue ref_value = ref->readValue();
            if (ref_value.size()) {
                const uint8_t* ref_data = ref_value.data();
                has_report_reference = true;
                report_id = ref_data[0];
                if (ref_value.size() > 1)
                    report_type = ref_data[1];
            }
        }

        if (has_report_reference && report_type != BLE_HID_REPORT_TYPE_INPUT)
            continue;

        if (!has_report_reference && _hogp.parser.has_report_id())
            continue;

        ReportBinding binding;
        binding.characteristic = characteristic;
        binding.handle = characteristic->getHandle();
        binding.report_id = report_id;
        binding.report_type = BLE_HID_REPORT_TYPE_INPUT;

        _hogp.reports.push_back(binding);
        BLE_LOG("slot=%d subscribing handle=0x%04X report_id=%u",
            active_slot_number(), (unsigned)binding.handle, (unsigned)binding.report_id);
        if (!ble_heap_guard_ok("discovery:subscribe",
                kSubscribeMinFreeI8, kSubscribeMinLargestI8)) {
            _hogp.reports.pop_back();
            return false;
        }
        bool notify_ok = characteristic->registerForNotify(on_input_report);
        if (!notify_ok)
            notify_ok = characteristic->registerForNotify(on_input_report, true, false);
        if (!notify_ok) {
            BLE_LOG("slot=%d notification subscription failed handle=0x%04X",
                active_slot_number(), (unsigned)binding.handle);
            _hogp.reports.pop_back();
            return false;
        }
    }

    if (_hogp.reports.empty()) {
        BLE_LOG("slot=%d no subscribable HID input reports found", active_slot_number());
        return false;
    }

    push_buttons(0, 0);
    _hogp.ready = true;
    _hogp.last_failure_stage = FAIL_NONE;

    if (_hogp.pairing_enabled) {
        for (int i = 0; i < kMaxBleControllers; ++i)
            _hogp_slots[i].pairing_enabled = false;
    }
    BLE_LOG("slot=%d controller ready player=%d reports=%u",
        active_slot_number(), _hogp.pad_index + 1, (unsigned)_hogp.reports.size());
    return true;
}

} // namespace

void ble_hogp_init(const char* local_name)
{
    HogpScope scope(&_hogp_slots[0]);
    if (_hogp.initialized || _hogp.init_failed)
        return;

    for (int i = 0; i < kMaxBleControllers; ++i)
        initialize_hogp_slot(_hogp_slots[i]);

    const char* name = local_name ? local_name : "emu32";
    BLE_LOG("init start name='%s' backend=NimBLE free_i8=%lu largest_i8=%lu",
        name,
        (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

    BLEDevice::init(name);
    BLE_LOG("init returned controller_ready=%d free_i8=%lu largest_i8=%lu",
        ble_controller_ready_after_init() ? 1 : 0,
        (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (!ble_controller_ready_after_init()) {
        BLE_LOG("init failed: Bluetooth controller/host stack is not ready");
        _hogp.init_failed = true;
        return;
    }

    configure_security();

    BLEScan* scan = BLEDevice::getScan();
    if (!scan) {
        BLE_LOG("init failed: BLE scanner unavailable free_i8=%lu largest_i8=%lu",
            (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
            (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        _hogp.init_failed = true;
        return;
    }

    configure_scan(scan);

    for (int i = 0; i < kMaxBleControllers; ++i) {
        _hogp_slots[i].initialized = true;
        _hogp_slots[i].next_scan_ms = millis() + kScanIntervalBackgroundMs;
        _hogp_slots[i].state_diag_next_ms = millis() + 1000;
        _hogp_slots[i].rx_diag_next_ms = millis() + 1000;
    }
    BLE_LOG("init complete max_controllers=%d free_i8=%lu largest_i8=%lu",
        kMaxBleControllers,
        (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
}

int ble_hogp_is_available()
{
    return 1;
}

void ble_hogp_update()
{
    HogpState& coordinator = _hogp_slots[0];
    if (!coordinator.initialized || coordinator.init_failed)
        return;

    int classic_connected = hid_connected_count();
    if (classic_connected > 0) {
        uint32_t now = millis();
        if ((int32_t)(now - _classic_scan_block_log_next_ms) >= 0) {
            _classic_scan_block_log_next_ms = now + 5000;
            BLE_LOG("update paused because %d Classic controller(s) are connected", classic_connected);
        }
        if (_scan_hogp)
            BLEDevice::getScan()->stop();
        for (int i = 0; i < kMaxBleControllers; ++i) {
            HogpScope scope(&_hogp_slots[i]);
            _hogp.scan_running = false;
            _hogp.scan_finished = false;
            if (!_hogp.connecting && !_hogp.connected)
                clear_candidate();
            schedule_scan(kScanIntervalBackgroundMs);
            maybe_log_state_diag();
        }
        return;
    }

    for (int i = 0; i < kMaxBleControllers; ++i) {
        HogpScope scope(&_hogp_slots[i]);
        if (_hogp.scan_finished) {
            _hogp.scan_finished = false;
            if (!_hogp.candidate.valid)
                schedule_scan(next_scan_delay_ms());
        }

        if (_hogp.candidate.valid && !_hogp.scan_running && !_hogp.connecting && !_hogp.connected) {
            if ((int32_t)(millis() - _hogp.next_connect_ms) >= 0)
                connect_candidate();
        }

        if (_hogp.connected && !_hogp.ready) {
            if (_hogp.discovery_ready_ms && (int32_t)(millis() - _hogp.discovery_ready_ms) < 0) {
                maybe_log_state_diag();
                continue;
            }
            _hogp.discovery_ready_ms = 0;

            if (!discover_reports()) {
                enter_failure(FAIL_DISCOVERY, "BLE: HOGP discovery failed", kCooldownDiscoveryFailMs);
                maybe_log_state_diag();
                continue;
            }
        }

        maybe_log_state_diag();
    }

    if (ready_hogp_count() >= kMaxBleControllers) {
        for (int i = 0; i < kMaxBleControllers; ++i)
            _hogp_slots[i].pairing_enabled = false;
        return;
    }

    HogpState* available = first_available_hogp_slot();
    if (available) {
        HogpScope scope(available);
        maybe_start_scan();
    }
}

int get_hid_ble(uint8_t* dst)
{
    for (int i = 0; i < kMaxBleControllers; ++i) {
        if (_hogp_slots[i].packet_pending) {
            _hogp_slots[i].packet_pending = false;
            return generic_pad_build_report(dst, 6);
        }
    }
    return 0;
}

int ble_hogp_set_pairing(int enabled)
{
    HogpScope scope(&_hogp_slots[0]);
    BLE_LOG("pairing request enabled=%d initialized=%d init_failed=%d ready=%d",
        enabled ? 1 : 0, _hogp.initialized ? 1 : 0,
        _hogp.init_failed ? 1 : 0, ready_hogp_count());
    if (!_hogp.initialized || _hogp.init_failed) {
        _hogp.init_failed = false;
        ble_hogp_init("emu32");
        if (!_hogp.initialized || _hogp.init_failed) {
            log_pairing_unavailable("init");
            return 0;
        }
    }

    if (!enabled) {
        for (int i = 0; i < kMaxBleControllers; ++i)
            _hogp_slots[i].pairing_enabled = false;
        if (_scan_hogp)
            BLEDevice::getScan()->stop();
        for (int i = 0; i < kMaxBleControllers; ++i) {
            HogpScope slot_scope(&_hogp_slots[i]);
            _hogp.scan_running = false;
            _hogp.scan_finished = false;
            if (!_hogp.connecting && !_hogp.connected)
                clear_candidate();
            schedule_scan(kScanIntervalBackgroundMs);
        }
        return 0;
    }

    for (int i = 0; i < kMaxBleControllers; ++i)
        _hogp_slots[i].pairing_enabled = true;
    if (ready_hogp_count() >= kMaxBleControllers) {
        for (int i = 0; i < kMaxBleControllers; ++i)
            _hogp_slots[i].pairing_enabled = false;
        return 0;
    }

    HogpState* available = first_available_hogp_slot();
    if (available) {
        HogpScope slot_scope(available);
        schedule_scan(0);
        BLE_LOG("pairing scan scheduled slot=%d", active_slot_number());
    }
    return 1;
}

int ble_hogp_get_pairing()
{
    for (int i = 0; i < kMaxBleControllers; ++i) {
        if (_hogp_slots[i].pairing_enabled)
            return 1;
    }
    return 0;
}

int ble_hogp_is_searching()
{
    return (ble_hogp_get_pairing() || _scan_hogp) ? 1 : 0;
}

int ble_hogp_connected_count()
{
    return ready_hogp_count();
}

#else

void ble_hogp_init(const char* local_name)
{
    (void)local_name;
}

int ble_hogp_is_available()
{
    return 0;
}

void ble_hogp_update()
{
}

int get_hid_ble(uint8_t* dst)
{
    (void)dst;
    return 0;
}

int ble_hogp_set_pairing(int enabled)
{
    static bool warned = false;
    if (!warned) {
        warned = true;
        fprintf(stdout, "BLE: HOGP unavailable in this build\n");
    }
    (void)enabled;
    return 0;
}

int ble_hogp_get_pairing()
{
    return 0;
}

int ble_hogp_is_searching()
{
    return 0;
}

int ble_hogp_connected_count()
{
    return 0;
}

#endif
