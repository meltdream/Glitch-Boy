/* Copyright (c) 2020, Peter Barrett
**
** Permission to use, copy, modify, and/or distribute this software for
** any purpose with or without fee is hereby granted, provided that the
** above copyright notice and this permission notice appear in all copies.
**
** THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
** BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
** OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
** WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
** ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
** SOFTWARE.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include "esp_timer.h"
using namespace std;

#include "hci_server.h"
#include "hid_server.h"
#include "hid_gamepad.h"
#include "../emu.h"
#include "../generic_pad.h"

// Reports drained per device per video frame. Was 1: pads that report
// faster than 60 Hz built a backlog, so input lagged further and further
// behind (up to the full queue). Matches the per-socket queue depth.
#ifndef BT_TRACE
#define BT_TRACE 0
#endif
#if BT_TRACE
#define HID_TRACE(...) printf(__VA_ARGS__)
#else
#define HID_TRACE(...) (void)0
#endif

#ifndef HID_GET_DRAIN_BUDGET
#define HID_GET_DRAIN_BUDGET 16
#endif

#ifndef HID_WII_DRAIN_BUDGET
#define HID_WII_DRAIN_BUDGET 8
#endif

enum {
    HID_SDP_PSM = 0x0001,
    HID_CONTROL_PSM = 0x0011,
    HID_INTERRUPT_PSM = 0x0013,
};

class SDPParse;
class InputDevice {
public:

    enum State {
        CLOSED,
        CONNECT,
        READING_NAME,
        READING_SDP,
        AUTHENTICATING,
        ENCRYPTING,
        OPENING,
        OPEN
    };

    InputDevice();
    ~InputDevice();

    State _state;
    bdaddr_t _bdaddr;
    int _dev_class;
    string _name;

    SDPParse* _sdp_handler;
    string _hid_descriptor;

    bool _reconnect;
    bool _fresh_pairing;
    bool _needs_sdp_after_auth;
    uint32_t _connect_started_ms;
    uint32_t _last_attempt_ms;      // last outbound connect, for fair retries
    int _sdp;
    int _control;
    int _interrupt;

    int _wii_index;
    HIDGamepad _gamepad;
    bool _is_gamepad;
    int _pad_index;
    uint16_t _last_buttons;
    bool _seen_in_inquiry;
    uint8_t _pro_3f_layout;
    uint32_t _pro_3f_samples;
    uint32_t _pro_3f_score_wii_u;
    uint32_t _pro_3f_score_classic;

    const char* name();
    void fail();
    void connect();
    void authentication_complete(int status);
    void encryption_changed(int status, bool enabled);
    void connection_request();
    void connection_complete(int status);
    void remote_name_response(const char* name);
    void disconnection_complete();
    void socket_changed(int socket, int state);

    void start_sdp();
    void update_sdp();
    void open_hid_channels();
};

//==================================================================
//==================================================================
// clunky sdp

#define SDP_ATTR_HID_DESCRIPTOR_LIST 0x0206

class SDPParse {
    int _sdp_sock;
    int _current_id;
    const uint8_t *_data;
    const uint8_t *_limit;
    vector<uint8_t> _buf;   // accumulate full search results before parsing
    string _HIDDescriptor;
    bool _failed;

    enum {
        kSdpRecvMax = 680,          // >= L2CAP MTU we advertise (672)
        kSdpMaxAttrBytes = 0x0200,  // response PDU = 7 + 512 + continuation (<= 17)
    };
public:
    bool failed() const { return _failed; }

    SDPParse(int sock) : _sdp_sock(sock), _failed(false)
    {
       _data = 0;
       _limit = 0;
       if (sock > 0 && service_search() < 0)
           _failed = true;
    }

    int service_search(const uint8_t* cont = 0, int contlen = 0)
    {
        // 4.7.1 SDP_ServiceSearchAttributeRequest PDU
        // MaximumAttributeByteCount is capped so every response PDU fits in
        // one receive buffer (kSdpRecvMax). With 0xFFFF a pad could answer
        // with a PDU we truncated, and SDP never completed.
        const uint8_t _sdp_hid_inquiry[] = {
            0x06,                               // SDP_ServiceSearchAttributeRequest
            0x00,0x01,
            0x00,0x0F,
            0x35,0x03,0x19,0x11,0x24,           // search for 1124 hid
            (uint8_t)(kSdpMaxAttrBytes >> 8), (uint8_t)kSdpMaxAttrBytes,
            0x35,0x05,0x0A,0x00,0x00,0xFF,0xFF, // AttributeIDList, range from 0000-FFFF
        };
        uint8_t buf[sizeof(_sdp_hid_inquiry) + 1 + 16];
        memcpy(buf,_sdp_hid_inquiry,sizeof(_sdp_hid_inquiry));
        buf[sizeof(_sdp_hid_inquiry)] = contlen;
        buf[4] += contlen;
        if (contlen)
            memcpy(buf+sizeof(_sdp_hid_inquiry)+1,cont,contlen);
        return l2_send(_sdp_sock,buf,sizeof(_sdp_hid_inquiry)+1+contlen);
    }

    // 0 = finished (record in _buf, possibly empty), 1 = waiting for more.
    int update()
    {
        uint8_t buf[kSdpRecvMax];
        int len = l2_recv(_sdp_sock,buf,sizeof(buf));
        if (len < 0) {
            _failed = true;     // channel gone
            return 0;
        }
        if (len > 0)
            if (add(buf,len) == 0)
                return 0;   // complete record
        return _failed ? 0 : 1;
    }

    int u8()
    {
        if (!_data || !_limit || _data >= _limit)
            return 0;
        return *_data++;
    }
    int u16()
    {
        if (!_data || !_limit || (_data + 1) >= _limit)
            return 0;
        return (u8() << 8) | u8();
    }
    int u32()
    {
        if (!_data || !_limit || (_data + 3) >= _limit)
            return 0;
        return (u16() << 16) | u16();
    }

    #define SDP_DE_NULL   0
    #define SDP_DE_UINT   1
    #define SDP_DE_INT    2
    #define SDP_DE_UUID   3
    #define SDP_DE_STRING 4
    #define SDP_DE_BOOL   5
    #define SDP_DE_SEQ    6
    #define SDP_DE_ALT    7
    #define SDP_DE_URL    8

    int header(uint32_t& n)
    {
        if (!_data || !_limit || _data >= _limit) {
            n = 0;
            return -1;
        }
        uint8_t hdr = u8();
        uint8_t type = hdr >> 3;
        uint8_t i = hdr & 0x07;
        n = 0;
        switch (i) {
            case 5:
                if (_data >= _limit) return -1;
                n = u8();
                break;
            case 6:
                if ((_data + 1) >= _limit) return -1;
                n = u16();
                break;
            case 7:
                if ((_data + 3) >= _limit) return -1;
                n = u32();
                break;
            default:
                n = 1 << i;
        }
        if (type == 0)
            n = 0;  // NULL
        return type;
    }

    int get_id16()
    {
        uint32_t n;
        int t = header(n);
        if (t != SDP_DE_UINT || n != 2) {
            return -1;
        }
        return u16();
    }

    void on_string(const string& s)
    {
        if (_current_id == SDP_ATTR_HID_DESCRIPTOR_LIST)
            _HIDDescriptor = s;
    }

    int item(int len, int level = 0, bool want_id = true)
    {
        if (!_data || !_limit)
            return -1;
        if (len < 0)
            return -1;
        const size_t avail = (size_t)(_limit - _data);
        if ((size_t)len > avail)
            return -1;
        const uint8_t* end = _data + (size_t)len;
        uint32_t n;
        while (_data < end) {
            if (want_id) {
                _current_id = get_id16();
                if (_current_id == -1)
                    return -1;
            }
            int t = header(n);
            if (t < 0)
                return -1;
            if ((_data + n) > end) {
                return -1;
            }
            switch (t) {
                case SDP_DE_NULL:
                    break;
                case SDP_DE_SEQ:            // it is a sequence
                    item(n,level+1,false);  // just a list
                    break;
                case SDP_DE_INT:
                case SDP_DE_UINT:
                    switch (n) {
                        case 1: u8(); break;
                        case 2: u16(); break;
                        case 4: u32(); break;
                        default: _data += n;
                    }
                    break;
                case SDP_DE_STRING:
                {
                    string s((const char*)_data,n);
                    _data += n;
                    on_string(s);
                    break;
                }
                case SDP_DE_UUID:
                    switch (n) {
                        case 2: u16(); break;
                        case 4: u32(); break;
                        default: _data += n;
                    }
                    break;
                case SDP_DE_BOOL:
                    // Must be consumed: HID records have booleans (0x0204,
                    // 0x0205) right before the descriptor list (0x0206), and
                    // leaving the byte made the parse stop before it.
                    _data += n;
                    break;
                default:
                    _data += n;
            }
        }
        return 0;
    }

    int list(int len, int level = 0)
    {
        if (!_data || !_limit)
            return -1;
        if (len < 0)
            return -1;
        const size_t avail = (size_t)(_limit - _data);
        if ((size_t)len > avail)
            return -1;
        const uint8_t* end = _data + (size_t)len;
        while (_data < end) {
            uint32_t n;
            int t = header(n);  // list
            if (t < 0)
                return -1;
            if (t != SDP_DE_SEQ)
                break;
            if (item(n,level) == -1)
                return -1;
        }
        return 0;
    }

    void parse(const uint8_t* data, int len)
    {
        if (!data || len <= 0)
            return;
        _data = data;
        _limit = data + len;
        const uint8_t* end = data + len;
        while (_data < end) {
            uint32_t n;
            int t = header(n);  // list
            if (t < 0)
                break;
            if (t != SDP_DE_SEQ)
                break;
            if (list(n) == -1)
                break;
        }
    }

    // return hid descriptor if found
    string parse()
    {
        if (_buf.empty())
            return string();
        parse(&_buf[0],(int)_buf.size());
        return _HIDDescriptor;
    }

    // packet from sdp socket, accumulate full report before parsing
    int add(const uint8_t* data, int len)
    {
        if (!data || len < 5) {
            return 1;
        }
        _data = data;
        _limit = data + len;
        uint8_t pid = u8();
        uint16_t tid = u16();
        uint16_t total = u16();
        (void)tid;
        (void)total;
        switch (pid) {
            case 1:     // SDP_ErrorResponse: give up on SDP, use what we have
                _failed = true;
                return 0;

            case 7:     // search attribute resonse
            {
                if ((_data + 2) > _limit) {
                    return 1;
                }
                int count = u16();
                if (count < 0) {
                    return 1;
                }
                const size_t payload = (size_t)(_limit - _data);
                if ((size_t)count >= payload) {
                    return 1;
                }
                const uint8_t* cont = _data + count;
                const uint8_t cont_len = cont[0];
                const size_t cont_payload = (size_t)(_limit - cont);
                if (((size_t)cont_len + 1u) > cont_payload) {
                    return 1;
                }
                size_t pos = _buf.size();
                const size_t next = pos + (size_t)count;
                const size_t kMaxSDPRecord = 8192;
                if (next > kMaxSDPRecord) {
                    return 0;
                }
                if (count > 0) {
                    // bad_alloc would abort the firmware (built with
                    // -fexceptions, nothing catches it): fail SDP instead.
                    try {
                        _buf.resize(next);
                    } catch (...) {
                        _failed = true;
                        return 0;
                    }
                    memcpy(&_buf[pos],_data,count);
                }
                if (!cont_len)
                    return 0;   // go all we are going to get: we now have data in _buf
                if (cont_len > 16 || service_search(cont+1,cont_len) < 0) {
                    _failed = true;
                    return 0;
                }
                break;
            }
        }
        return 1;   // more to come?
    }
};

//==================================================================
//==================================================================
//  wiimote is a lovely thing

enum {
    WII_DEVICE_CLASS = 0x042500,       // Nintendo RVL-CNT-01
    WII_TR_DEVICE_CLASS = 0x080500,    // Nintendo RVL-CNT-01-TR, has motion plus
    WII_U_PRO_DEVICE_CLASS = 0x082500, // Nintendo Wii U Pro Controller family

    // Some ESP32 HCI inquiry paths report Class of Device in canonical order.
    WII_DEVICE_CLASS_CANONICAL = 0x002504,
    WII_TR_DEVICE_CLASS_CANONICAL = 0x000508,
    WII_U_PRO_DEVICE_CLASS_CANONICAL = 0x002508,
};
wii_state wii_states[4] = {0};

static bool is_wii_remote_device_class(int dev_class)
{
    return dev_class == WII_DEVICE_CLASS ||
           dev_class == WII_DEVICE_CLASS_CANONICAL ||
           dev_class == WII_TR_DEVICE_CLASS ||
           dev_class == WII_TR_DEVICE_CLASS_CANONICAL;
}

static bool is_wii_u_pro_device_class(int dev_class)
{
    return dev_class == WII_U_PRO_DEVICE_CLASS ||
           dev_class == WII_U_PRO_DEVICE_CLASS_CANONICAL;
}

static bool is_bluetooth_peripheral_input_class(int dev_class)
{
    if (dev_class == 0)
        return true;    // TODO. some wiimotes report 0 on reconnect

    int major = (dev_class >> 8) & 0x1f;
    if (major != 5)
        return false;

    int minor = (dev_class >> 2) & 0x3f;
    switch (minor & 0x0f) {
        case 1: return true;  // joystick
        case 2: return true;  // gamepad
        case 3: return true;  // remote
    }
    switch (minor & 0x30) {
        case 0x10: return true; // Keyboard
        case 0x20: return true; // Pointing device
        case 0x30: return true; // Pointing device/keyboard
    }
    return false;
}

static int device_class_value(const uint8_t* dev_class)
{
    return (dev_class[0] << 16) | (dev_class[1] << 8) | dev_class[2];
}

static int device_class_value_reversed(const uint8_t* dev_class)
{
    return (dev_class[2] << 16) | (dev_class[1] << 8) | dev_class[0];
}

static bool is_native_switch_name(const std::string& name)
{
    return (name == "Pro Controller") ||
           (strncmp(name.c_str(), "Joy-Con", 7) == 0);
}

static bool is_left_joycon_name(const std::string& name)
{
    return name == "Joy-Con (L)";
}

static bool is_right_joycon_name(const std::string& name)
{
    return name == "Joy-Con (R)";
}

static bool is_nintendo_name(const std::string& name)
{
    return (strncmp(name.c_str(), "Nintendo", 8) == 0) ||
           is_native_switch_name(name);
}

static bool contains_token_ci_ascii(const std::string& text, const char* token)
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

// 8BitDo pads in Android mode reuse the Wii U Pro class, so match on name too.
static bool is_nintendo_family_device_class_name(int dev_class, const std::string& name)
{
    return is_wii_remote_device_class(dev_class) || is_nintendo_name(name);
}

// Some 8BitDo Android modes stall if authentication runs before SDP.
static bool should_bypass_explicit_auth_for_generic_hid(int dev_class, const std::string& name)
{
    return is_wii_u_pro_device_class(dev_class) && contains_token_ci_ascii(name, "8bitdo");
}

static bool is_playstation_wireless_controller(const InputDevice* d)
{
    return d->_name == "Wireless Controller";
}

static uint16_t remap_playstation_buttons(uint16_t buttons)
{
    // Sony reports face buttons in Square, Cross, Circle, Triangle order.
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

// 8BitDo pads in D-input (Android) mode number their buttons in the
// Android/Linux order, unlike the older generic order.
static uint32_t hid_decode_quirks(const InputDevice* d)
{
    if (d && d->_name.find("8BitDo") != string::npos)
        return HIDGamepad::DECODE_QUIRK_ANDROID_LAYOUT;
    return HIDGamepad::DECODE_QUIRK_NONE;
}

// https://wiibrew.org/wiki/Wiimote
// https://github.com/dvdhrm/xwiimote/blob/master/doc/PROTOCOL
// https://web.archive.org/web/20080630032911/http://wiki.wiimoteproject.com/Reports

// 0x13 LED Enable // P4 P3 P2 P1 ? ? 1 RUMBLE  // turn on specific leds
// 0x13 IR Enable 1 - ? ? ? ? ? IRPCK 1 RUMBLE  // enable pixel clock for ir camera
// 0x1A IR Enable 2  - ? ? ? ? ? IREN 1 RUMBLE  // enable ir camera

class WII {
public:
    enum {
        WII_REPORT_MODE_REQUESTED = 0x400,
        WII_REPORT_MODE_SELECTED = 0x800,
        WII_REPORT_MODE_RETRY_MS = 250,
        WII_REPORT_MODE_IDLE_REFRESH_MS = 1000,
        WII_EXTENSION_POLL_ADDR = 0xA40008,
        WII_EXTENSION_POLL_MS = 33,
        WII_EXTENSION_POLL_RETRY_MS = 100,
    };

    enum HidResult {
        HID_IGNORED = 0,
        HID_CONSUMED = 1,
        HID_REPORT_CHANGED = 2,
        HID_GENERIC_REPORT_CHANGED = 3,
    };

    uint8_t _switch_subcmd_seq = 0;

    void send(int s, const uint8_t* data, int len)
    {
        l2_send(s,data,len);
    }

    void led(int s, uint8_t id = 0x10)     // manipulate wii leds
    {
        uint8_t buf[3] = {0xA2, 0x11, id};  // set led
        send(s,buf,3);
        buf[1] = 0x15;  // status
        buf[2] = 0;     // request-status payload only carries rumble
        send(s,buf,3);
    }

    void read_mem(int s, int addr, int size)
    {
        uint8_t cmd[8] = {
            0xA2, 0x17,
            0x04,   // registers, not eeprom
            (uint8_t)(addr >> 16),(uint8_t)(addr >> 8),(uint8_t)addr,
            (uint8_t)(size >> 8),(uint8_t)size
        };  // read mem
        send(s,cmd,sizeof(cmd));
    }

    void write_mem(int s, int addr, const uint8_t* data, int size)
    {
        uint8_t cmd[7+16] = {
            0xA2, 0x16,
            0x04,   // registers, not eeprom
            (uint8_t)(addr >> 16),(uint8_t)(addr >> 8),(uint8_t)addr,(uint8_t)size,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
        };  // write mem
        memcpy(cmd+7,data,size);
        send(s,cmd,sizeof(cmd));
    }

    void init_extension_unencrypted(int s)
    {
        // WiiBrew "new way" init: 0x55 -> 0xA400F0, then 0x00 -> 0xA400FB
        // disables extension-byte encryption.
        uint8_t v = 0x55;
        write_mem(s,0xA400F0,&v,1);
        v = 0x00;
        write_mem(s,0xA400FB,&v,1);
    }

    void read_extention_type(int s)
    {
        read_mem(s,0xA400FA,6);
    }

    // 0x32 BTN + 6 EXT             - button + extension
    // 0x37 BTN + XLR + IR + 6 EXT  - turn on accelerometer, ir and external (nunchuck) device
    void select_input_report(int s, uint8_t report = 0x30)
    {
        u8 hid[4] = { 0xA2, 0x12, 0x00, report };
        send(s,hid,4);
    }

    void request_input_report(InputDevice* d, wii_state* state, uint8_t report, bool force = false)
    {
        if (!d || !state || !d->_interrupt)
            return;
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
        if (!force &&
            (state->flags & WII_REPORT_MODE_REQUESTED) &&
            state->requested_report_mode == report &&
            (uint32_t)(now - state->report_mode_request_ms) < WII_REPORT_MODE_RETRY_MS) {
            return;
        }
        select_input_report(d->_interrupt, report);
        state->requested_report_mode = report;
        state->report_mode_request_ms = now;
        state->flags |= WII_REPORT_MODE_REQUESTED | WII_REPORT_MODE_SELECTED;
    }

    void refresh_input_report_on_idle_stream(InputDevice* d, wii_state* state, uint8_t report)
    {
        if (!d || !state)
            return;
        if (state->requested_report_mode != report)
            return;
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
        if ((uint32_t)(now - state->report_mode_request_ms) >= WII_REPORT_MODE_IDLE_REFRESH_MS)
            request_input_report(d, state, report, true);
    }

    uint8_t input_report_for_state(const wii_state* state)
    {
        if (state && (state->flags & extension_polled))
            return 0x30;
        if (state && (state->flags & (nunchuk | classic_controller | drums_guitar)))
            return 0x32;
        return 0x30;
    }

    void begin_extension_polling(InputDevice* d, wii_state* state)
    {
        if (!d || !state || !d->_interrupt)
            return;
        state->flags = (state->flags | extension_polled) & ~extension_poll_pending;
        state->extension_poll_ms = 0;
        poll_extension(d, true);
    }

    void poll_extension(InputDevice* d, bool force = false)
    {
        if (!d || !d->_interrupt || d->_wii_index < 0 || d->_wii_index >= 4)
            return;
        wii_state* state = wii_states + d->_wii_index;
        if (!(state->flags & extension_polled))
            return;
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
        uint32_t elapsed = (uint32_t)(now - state->extension_poll_ms);
        if (!force) {
            if (state->flags & extension_poll_pending) {
                if (elapsed < WII_EXTENSION_POLL_RETRY_MS)
                    return;
            } else if (elapsed < WII_EXTENSION_POLL_MS) {
                return;
            }
        }
        read_mem(d->_interrupt, WII_EXTENSION_POLL_ADDR, 6);
        state->extension_poll_ms = now;
        state->flags |= extension_poll_pending;
    }

    static uint16_t common_buttons_from_report(const uint8_t* data, int len)
    {
        const uint16_t button_mask =
            wii_plus | wii_up | wii_down | wii_right | wii_left |
            wii_home | wii_minus | wii_a | wii_b | wii_one | wii_two;
        if (!data || len < 4)
            return 0;
        return (uint16_t)(((uint16_t)data[2] << 8) | data[3]) & button_mask;
    }

    static uint16_t classic_buttons_from_report(const wii_state* state, const uint8_t* data, int len)
    {
        if (!state || !(state->flags & classic_controller) || !data || len < 2)
            return 0;
        if (data[1] == 0x37) {
            if (len < 23)
                return 0;
            return (uint16_t)((((uint16_t)data[21] << 8) | data[22]) ^ 0xFFFF);
        }
        if (len < 10)
            return 0;
        return (uint16_t)((((uint16_t)data[8] << 8) | data[9]) ^ 0xFFFF);
    }

    static bool mapped_buttons_changed(const wii_state* state, const uint8_t* data, int len)
    {
        return common_buttons_from_report(state->report, (int)sizeof(state->report)) != common_buttons_from_report(data, len) ||
               classic_buttons_from_report(state, state->report, (int)sizeof(state->report)) != classic_buttons_from_report(state, data, len);
    }

// "Nintendo RVL-CNT-01"
// "Nintendo RVL-CNT-01-TR"
    bool is_wii(InputDevice* d)
    {
        if (!d)
            return false;
        return is_nintendo_family_device_class_name(d->_dev_class, d->_name);
    }

    bool is_wii_u_pro(InputDevice* d)
    {
        if (!d)
            return false;
        return is_wii_u_pro_device_class(d->_dev_class) && is_nintendo_name(d->_name);
    }

    static bool is_8bitdo_switch_mode(InputDevice* d)
    {
        if (!d)
            return false;
        // bdaddr_t stores bytes reversed versus display text:
        // text "E4:17:D8:xx:xx:xx" -> b[5]=E4, b[4]=17, b[3]=D8.
        const uint8_t* b = d->_bdaddr.b;
        return (b[5] == 0xE4) && (b[4] == 0x17) && (b[3] == 0xD8);
    }

    static bool is_switch_mode(InputDevice* d)
    {
        if (!d)
            return false;
        return is_native_switch_name(d->_name) ||
               is_8bitdo_switch_mode(d);
    }

    void switch_set_input_report_mode(int s, uint8_t mode)
    {
        // Switch subcommand output report:
        // A2 01 <seq> <rumble[8]> 03 <mode>
        uint8_t seq = (uint8_t)(_switch_subcmd_seq++ & 0x0F);
        uint8_t cmd[] = {
            0xA2, 0x01, seq,
            0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40,
            0x03, mode
        };
        send(s, cmd, sizeof(cmd));
    }

    void switch_set_player_lights(int s, uint8_t lights)
    {
        // Subcommand 0x30 sets the steady player LEDs.
        uint8_t seq = (uint8_t)(_switch_subcmd_seq++ & 0x0F);
        uint8_t cmd[] = {
            0xA2, 0x01, seq,
            0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40,
            0x30, (uint8_t)(lights & 0x0F)
        };
        send(s, cmd, sizeof(cmd));
    }

    static uint16_t map_wii_u_pro_buttons(uint8_t b0, uint8_t b1, uint8_t b2)
    {
        uint16_t out = 0;

        // Face buttons by label. GENERIC_FIRE_A is the console's A; on a
        // Nintendo pad the labels already sit where the NES/GB has them
        // (A right of B), so label = position. The old south/east mapping
        // swapped A<->B and X<->Y on these pads.
        if (b0 & 0x08) out |= GENERIC_FIRE_A; // A (east)
        if (b0 & 0x04) out |= GENERIC_FIRE_B; // B (south)
        if (b0 & 0x02) out |= GENERIC_FIRE_X; // X (north)
        if (b0 & 0x01) out |= GENERIC_FIRE_Y; // Y (west)

        // D-pad.
        if (b2 & 0x08) out |= GENERIC_LEFT;
        if (b2 & 0x04) out |= GENERIC_RIGHT;
        if (b2 & 0x02) out |= GENERIC_UP;
        if (b2 & 0x01) out |= GENERIC_DOWN;

        // Meta/system.
        if (b1 & 0x02) out |= GENERIC_START;   // +
        if (b1 & 0x01) out |= GENERIC_SELECT;  // -
        if (b1 & 0x10) out |= GENERIC_MENU;    // Home

        // Shoulder/trigger extras.
        if (b2 & 0x40) out |= GENERIC_FIRE_C;  // L
        if (b0 & 0x40) out |= GENERIC_FIRE_Z;  // R
        if (b2 & 0x80) out |= GENERIC_FIRE;    // ZL
        if (b0 & 0x80) out |= GENERIC_OTHER;   // ZR

        return out;
    }

    static unsigned button_popcount(uint16_t v)
    {
        return (unsigned)__builtin_popcount((unsigned)v);
    }

    static uint16_t decode_wii_u_pro_buttons_report30(const uint8_t* data, int len)
    {
        // Nintendo Switch Pro report 0x30:
        // [0]=0xA1 [1]=0x30 [2]=timer [3]=battery [4]=buttons0 [5]=buttons1 [6]=buttons2 ...
        // Fold the left stick into the D-pad for digital cores.
        if (!data || len < 10 || data[1] != 0x30)
            return 0;
        const uint16_t lx = (uint16_t)(data[7] | ((data[8] & 0x0F) << 8));
        const uint16_t ly = (uint16_t)((data[8] >> 4) | (data[9] << 4));
        const uint16_t low = 0x500;
        const uint16_t high = 0xB00;
        uint16_t out = map_wii_u_pro_buttons(data[4], data[5], data[6]);

        if (lx <= low) out |= GENERIC_LEFT;
        else if (lx >= high) out |= GENERIC_RIGHT;
        if (ly <= low) out |= GENERIC_DOWN;
        else if (ly >= high) out |= GENERIC_UP;

        return out;
    }

    static uint16_t decode_single_joycon_report30(const uint8_t* data, int len, bool left)
    {
        // Rotate a sideways Joy-Con into a standard digital layout.
        if (!data || len < 13 || data[1] != 0x30)
            return 0;

        const uint8_t cluster = left ? data[6] : data[4];
        const uint16_t sx = left
            ? (uint16_t)(data[7] | ((data[8] & 0x0F) << 8))
            : (uint16_t)(data[10] | ((data[11] & 0x0F) << 8));
        const uint16_t sy = left
            ? (uint16_t)((data[8] >> 4) | (data[9] << 4))
            : (uint16_t)((data[11] >> 4) | (data[12] << 4));
        const uint16_t low = 0x500;
        const uint16_t high = 0xB00;
        uint16_t out = 0;

        if (left) {
            // Left Joy-Con: rotate the D-pad into the face cluster.
            if (cluster & 0x04) out |= GENERIC_FIRE_A; // original right -> south
            if (cluster & 0x02) out |= GENERIC_FIRE_B; // original up -> east
            if (cluster & 0x01) out |= GENERIC_FIRE_X; // original down -> west
            if (cluster & 0x08) out |= GENERIC_FIRE_Y; // original left -> north

            if (sx <= low) out |= GENERIC_UP;
            else if (sx >= high) out |= GENERIC_DOWN;
            if (sy <= low) out |= GENERIC_LEFT;
            else if (sy >= high) out |= GENERIC_RIGHT;
        } else {
            // Right Joy-Con: keep the face cluster and rotate directions.
            if (cluster & 0x01) out |= GENERIC_FIRE_A; // Y -> south
            if (cluster & 0x04) out |= GENERIC_FIRE_B; // B -> east
            if (cluster & 0x02) out |= GENERIC_FIRE_X; // X -> west
            if (cluster & 0x08) out |= GENERIC_FIRE_Y; // A -> north

            if (sx <= low) out |= GENERIC_DOWN;
            else if (sx >= high) out |= GENERIC_UP;
            if (sy <= low) out |= GENERIC_RIGHT;
            else if (sy >= high) out |= GENERIC_LEFT;
        }

        // Sideways rail buttons: SR is Select and SL is Start.
        if (cluster & 0x10) out |= GENERIC_SELECT;
        if (cluster & 0x20) out |= GENERIC_START;
        if (data[5] & 0x10) out |= GENERIC_MENU;

        return out;
    }

    static uint16_t decode_wii_u_pro_buttons_report3f(const uint8_t* data, int len)
    {
        // Wii U Pro/extension payload in 0x3F includes 11 extension bytes.
        // Button bytes are extension byte 8/9 => packet bytes 10/11.
        // Bits are active-low (0 when pressed).
        if (!data || len < 12 || data[1] != 0x3F)
            return 0;
        uint8_t b8 = data[10];
        uint8_t b9 = data[11];
        uint16_t out = 0;

        // Face buttons by label (see map_wii_u_pro_buttons).
        if (!(b9 & 0x10)) out |= GENERIC_FIRE_A; // A
        if (!(b9 & 0x40)) out |= GENERIC_FIRE_B; // B
        if (!(b9 & 0x08)) out |= GENERIC_FIRE_X; // X
        if (!(b9 & 0x20)) out |= GENERIC_FIRE_Y; // Y

        // D-pad.
        if (!(b9 & 0x01)) out |= GENERIC_UP;     // DU
        if (!(b8 & 0x40)) out |= GENERIC_DOWN;   // DD
        if (!(b8 & 0x80)) out |= GENERIC_RIGHT;  // DR
        if (!(b9 & 0x02)) out |= GENERIC_LEFT;   // DL

        // Meta/system.
        if (!(b8 & 0x04)) out |= GENERIC_START;   // +
        if (!(b8 & 0x10)) out |= GENERIC_SELECT;  // -
        if (!(b8 & 0x08)) out |= GENERIC_MENU;    // Home

        // Shoulder/trigger extras.
        if (!(b8 & 0x20)) out |= GENERIC_FIRE_C;  // L
        if (!(b8 & 0x02)) out |= GENERIC_FIRE_Z;  // R
        if (!(b9 & 0x80)) out |= GENERIC_FIRE;    // ZL
        if (!(b9 & 0x04)) out |= GENERIC_OTHER;   // ZR

        return out;
    }

    static uint16_t map_wii_classic_buttons(uint16_t c)
    {
        uint16_t out = 0;

        // Face buttons by label (see map_wii_u_pro_buttons).
        if (c & wii_classic_a) out |= GENERIC_FIRE_A; // A (east)
        if (c & wii_classic_b) out |= GENERIC_FIRE_B; // B (south)
        if (c & wii_classic_x) out |= GENERIC_FIRE_X; // X (north)
        if (c & wii_classic_y) out |= GENERIC_FIRE_Y; // Y (west)

        // D-pad and meta.
        if (c & wii_classic_left) out |= GENERIC_LEFT;
        if (c & wii_classic_right) out |= GENERIC_RIGHT;
        if (c & wii_classic_up) out |= GENERIC_UP;
        if (c & wii_classic_down) out |= GENERIC_DOWN;
        if (c & wii_classic_plus) out |= GENERIC_START;
        if (c & wii_classic_minus) out |= GENERIC_SELECT;
        if (c & wii_classic_home) out |= GENERIC_MENU;

        // Shoulder/trigger extras.
        if (c & wii_classic_left_top) out |= GENERIC_FIRE_C;   // L
        if (c & wii_classic_right_top) out |= GENERIC_FIRE_Z;  // R
        if (c & 0x0080) out |= GENERIC_FIRE;                   // ZL
        if (c & 0x0004) out |= GENERIC_OTHER;                  // ZR

        return out;
    }

    static uint16_t decode_wii_classic_buttons_report3f(const uint8_t* data, int len)
    {
        // Some third-party pads use classic-style bytes at offsets 8 and 9.
        if (!data || len < 10 || data[1] != 0x3F)
            return 0;
        uint16_t classic = ((((uint16_t)data[8]) << 8) | data[9]) ^ 0xFFFF;
        return map_wii_classic_buttons(classic);
    }

    static uint16_t decode_switch_buttons_report3f(const uint8_t* data, int len)
    {
        // 8BitDo 0x3F reports place face buttons, shoulders, and hat in bytes 2..4.
        if (!data || len < 5 || data[1] != 0x3F)
            return 0;

        uint8_t b0 = data[2];
        uint8_t b1 = data[3];
        uint8_t hat = data[4] & 0x0F;
        uint16_t out = 0;

        // Face-button layout used by 8BitDo Switch mode.
        if (b0 & 0x02) out |= GENERIC_FIRE_A;
        if (b0 & 0x01) out |= GENERIC_FIRE_B;
        if (b0 & 0x08) out |= GENERIC_FIRE_X;
        if (b0 & 0x04) out |= GENERIC_FIRE_Y;

        // Hat direction (usual path).
        bool dpad_from_hat = false;
        switch (hat) {
            case 0: out |= GENERIC_UP; dpad_from_hat = true; break;
            case 1: out |= GENERIC_UP | GENERIC_RIGHT; dpad_from_hat = true; break;
            case 2: out |= GENERIC_RIGHT; dpad_from_hat = true; break;
            case 3: out |= GENERIC_DOWN | GENERIC_RIGHT; dpad_from_hat = true; break;
            case 4: out |= GENERIC_DOWN; dpad_from_hat = true; break;
            case 5: out |= GENERIC_DOWN | GENERIC_LEFT; dpad_from_hat = true; break;
            case 6: out |= GENERIC_LEFT; dpad_from_hat = true; break;
            case 7: out |= GENERIC_UP | GENERIC_LEFT; dpad_from_hat = true; break;
            default: break; // 8/15 are typically neutral/invalid
        }

        if (!dpad_from_hat && len >= 9) {
            // Some 8BitDo pads encode directions on the left-stick axes.
            const uint16_t lx = (uint16_t)(data[5] | (data[6] << 8));
            const uint16_t ly = (uint16_t)(data[7] | (data[8] << 8));
            const uint16_t low = 0x3000;
            const uint16_t high = 0xD000;
            if (lx <= low) out |= GENERIC_LEFT;
            else if (lx >= high) out |= GENERIC_RIGHT;
            if (ly <= low) out |= GENERIC_UP;
            else if (ly >= high) out |= GENERIC_DOWN;
        }

        // Meta buttons and shoulders.
        if (b1 & 0x02) out |= GENERIC_START;
        // Treat both Minus and Capture as Select.
        if ((b1 & 0x01) || (b1 & 0x20)) out |= GENERIC_SELECT;
        if (b1 & 0x10) out |= GENERIC_MENU;
        if (b1 & 0x40) out |= GENERIC_FIRE_C;  // L
        if (b1 & 0x80) out |= GENERIC_FIRE_Z;  // R

        return out;
    }

    void on_open(InputDevice* d)
    {
        if (!d || !d->_interrupt || !is_wii(d))
            return;
        int slot = find_slot(d);
        if (slot < 0)
            return;
        // Wii U Pro pads use their default report stream.
        if (is_wii_u_pro_device_class(d->_dev_class) || d->_name == "Pro Controller") {
            d->_pro_3f_layout = 0;
            d->_pro_3f_samples = 0;
            d->_pro_3f_score_wii_u = 0;
            d->_pro_3f_score_classic = 0;

            if (is_switch_mode(d)) {
                switch_set_player_lights(d->_interrupt, (uint8_t)(1 << slot));
                switch_set_input_report_mode(d->_interrupt, 0x30);
                return;
            }

            // Extension bytes are encrypted by default; 0x3F payloads need them clear.
            init_extension_unencrypted(d->_interrupt);
            return;
        }
        led(d->_interrupt, 0x10 << slot);
    }

    // assign this device to a wii slot
    int find_slot(InputDevice* d)
    {
        if (d->_wii_index == -1)
        {
            for (int i = 0; i < 4; i++) {
                if (wii_states[i].flags == 0) {
                    memset(wii_states + i, 0, sizeof(wii_states[i]));
                    wii_states[i].flags = wiimote;
                    d->_wii_index = i;
                    break;
                }
            }
        }
        return d->_wii_index;
    }

    HidResult hid(InputDevice* d, const uint8_t* data, int len)
    {
        if (!is_wii(d) || !data || len < 2)
            return HID_IGNORED;
        int slot = find_slot(d);
        if (slot == -1)
            return HID_CONSUMED;     // does not have a slot yet? odd
        wii_state* state = wii_states + slot;

        if (is_wii_u_pro(d)) {
            memcpy(state->report,data,min((int)sizeof(state->report),len));
            uint16_t wii_u_buttons = 0;
            uint16_t classic_buttons = 0;
            uint16_t switch_buttons = 0;
            uint16_t buttons = 0;
            if (data[1] == 0x30) {
                if (is_left_joycon_name(d->_name))
                    wii_u_buttons = decode_single_joycon_report30(data, len, true);
                else if (is_right_joycon_name(d->_name))
                    wii_u_buttons = decode_single_joycon_report30(data, len, false);
                else
                    wii_u_buttons = decode_wii_u_pro_buttons_report30(data, len);
                buttons = wii_u_buttons;
                d->_pro_3f_layout = 1;
            } else if (data[1] == 0x3F) {
                switch_buttons = decode_switch_buttons_report3f(data, len);
                wii_u_buttons = decode_wii_u_pro_buttons_report3f(data, len);
                classic_buttons = decode_wii_classic_buttons_report3f(data, len);

                if (is_switch_mode(d)) {
                    // Switch devices use the simple 0x3F layout.
                    buttons = switch_buttons;
                    d->_pro_3f_layout = 3;
                } else {
                    if (!d->_pro_3f_layout) {
                        d->_pro_3f_samples++;
                        d->_pro_3f_score_wii_u += button_popcount(wii_u_buttons);
                        d->_pro_3f_score_classic += button_popcount(classic_buttons);
                        if (d->_pro_3f_samples >= 40) {
                            d->_pro_3f_layout = (d->_pro_3f_score_classic + 2 < d->_pro_3f_score_wii_u) ? 2 : 1;
                        }
                    }

                    uint8_t layout = d->_pro_3f_layout;
                    if (!layout) {
                        unsigned pc_wiiu = button_popcount(wii_u_buttons);
                        unsigned pc_classic = button_popcount(classic_buttons);
                        layout = (pc_classic + 1 < pc_wiiu) ? 2 : 1;
                    }
                    buttons = (layout == 2) ? classic_buttons : wii_u_buttons;
                }
            } else {
                return HID_CONSUMED;
            }
            if (d->_pad_index < 0)
                d->_pad_index = generic_pad_assign(d);
            if (d->_pad_index < 0)
                return HID_CONSUMED;
            bool changed = (buttons != d->_last_buttons);
            d->_last_buttons = buttons;
            generic_pad_update_ex(d, buttons, (uint64_t)buttons);
            return changed ? HID_GENERIC_REPORT_CHANGED : HID_CONSUMED;
        }

        auto* h = data+2;
        switch (data[1]) {
            // This report is sent in response to report 0x15 (status, from led setting)
            case 0x20:  // Status
            {
                if (len < 8)
                    return HID_CONSUMED;
                int stat = h[2];
                state->flags &= ~(WII_REPORT_MODE_SELECTED | WII_REPORT_MODE_REQUESTED);
                if (stat & 2) {
                    state->flags &= ~(nunchuk | classic_controller | drums_guitar |
                                      extension_polled | extension_poll_pending);
                    state->flags |= extension_read | encryption_disabled;
                    init_extension_unencrypted(d->_interrupt);
                    read_extention_type(d->_interrupt);
                } else {
                    state->flags &= ~(nunchuk | classic_controller | drums_guitar |
                                      extension_read | encryption_disabled |
                                      extension_polled | extension_poll_pending);
                    state->extension_poll_ms = 0;
                    request_input_report(d, state, input_report_for_state(state), true);
                }
            }
                return HID_CONSUMED;

            // wii response to memory read
            case 0x21: // Read Data
            {
                if (len < 13)
                    return HID_CONSUMED;
                int size = (h[2] >> 4) + 1;
                int error = h[2] & 0x0F;
                int addr = (h[3] << 8) | h[4];  // low bytes of address
                h += 5;

                if (addr == 0x0008 && (state->flags & extension_polled)) {
                    state->flags &= ~extension_poll_pending;
                    if (error != 0 || size < 6)
                        return HID_CONSUMED;
                    uint8_t report[10] = {
                        0xA1, 0x32,
                        data[2], data[3],
                        h[0], h[1], h[2], h[3], h[4], h[5]
                    };
                    bool changed = mapped_buttons_changed(state, report, (int)sizeof(report));
                    memset(state->report, 0, sizeof(state->report));
                    memcpy(state->report, report, sizeof(report));
                    return changed ? HID_REPORT_CHANGED : HID_CONSUMED;
                }

                if (addr != 0x00FA)
                    return HID_CONSUMED;

                if (error != 0 || size < 6) {
                    state->flags &= ~(nunchuk | classic_controller | drums_guitar |
                                      extension_read | encryption_disabled |
                                      extension_polled | extension_poll_pending);
                    state->extension_poll_ms = 0;
                    request_input_report(d, state, input_report_for_state(state), true);
                    return HID_CONSUMED;
                }

                int id = (h[4] << 8) | h[5];
                int flags = 0;
                switch (id) {
                    case 0x0000: flags = nunchuk;            break;
                    case 0x0101: flags = classic_controller; break;
                    case 0x0103: flags = drums_guitar;       break;
                }
                state->flags = (state->flags & ~(nunchuk | classic_controller | drums_guitar |
                                                 extension_polled | extension_poll_pending)) | flags;
                state->extension_poll_ms = 0;
                if (flags)
                    state->flags |= extension_read | encryption_disabled;
                else
                    state->flags &= ~(extension_read | encryption_disabled);
                if (flags == classic_controller)
                    begin_extension_polling(d, state);
                else
                    request_input_report(d, state, input_report_for_state(state), true);
            }
                return HID_CONSUMED;

            case 0x22: // Command Status
                if (len < 6)
                    return HID_CONSUMED;
                if (h[2] == 0x12) {
                    if (h[3] == 0 && state->requested_report_mode == input_report_for_state(state)) {
                        state->flags = (state->flags | WII_REPORT_MODE_SELECTED) & ~WII_REPORT_MODE_REQUESTED;
                    } else {
                        state->flags &= ~(WII_REPORT_MODE_SELECTED | WII_REPORT_MODE_REQUESTED);
                    }
                    return HID_CONSUMED;
                }
                if (!(state->flags & extension_read)) {
                    read_extention_type(d->_interrupt);
                    state->flags |= extension_read;
                } else if (!(state->flags & encryption_disabled)) {
                    uint8_t e = 0;
                    write_mem(d->_interrupt,0xA400FB,&e,1);   // disable encryption
                    state->flags |= encryption_disabled;
                }
                return HID_CONSUMED;

            // default wii report
            case 0x30:
            {
                bool changed;
                if (state->flags & extension_polled) {
                    changed = common_buttons_from_report(state->report, (int)sizeof(state->report)) !=
                              common_buttons_from_report(data, len);
                    if (len >= 4) {
                        state->report[0] = 0xA1;
                        if (state->report[1] == 0)
                            state->report[1] = 0x32;
                        state->report[2] = data[2];
                        state->report[3] = data[3];
                    }
                    return changed ? HID_REPORT_CHANGED : HID_CONSUMED;
                } else {
                    changed = mapped_buttons_changed(state, data, len);
                    memcpy(state->report,data,min((int)sizeof(state->report),len));
                }
                if (!is_wii_u_pro(d) && !(state->flags & WII_REPORT_MODE_SELECTED))
                    request_input_report(d, state, input_report_for_state(state));
                else if (!changed)
                    refresh_input_report_on_idle_stream(d, state, input_report_for_state(state));
                return changed ? HID_REPORT_CHANGED : HID_CONSUMED;
            }

            case 0x32:  // core + ext
            case 0x37:  // big report
            {
                bool changed = mapped_buttons_changed(state, data, len);
                memcpy(state->report,data,min((int)sizeof(state->report),len));
                if (!is_wii_u_pro(d) && !(state->flags & WII_REPORT_MODE_SELECTED))
                    request_input_report(d, state, input_report_for_state(state));
                else if (!changed)
                    refresh_input_report_on_idle_stream(d, state, input_report_for_state(state));
                return changed ? HID_REPORT_CHANGED : HID_CONSUMED;
            }

            default:
                return HID_CONSUMED;
        }
    }
};

InputDevice::InputDevice() : _sdp_handler(0),_reconnect(0),_fresh_pairing(false),_needs_sdp_after_auth(false),_connect_started_ms(0),_last_attempt_ms(0),_wii_index(-1),_is_gamepad(false),_pad_index(-1),_last_buttons(0),_seen_in_inquiry(false),_pro_3f_layout(0),_pro_3f_samples(0),_pro_3f_score_wii_u(0),_pro_3f_score_classic(0) {
}

InputDevice::~InputDevice() {
    generic_pad_release(this);
    disconnection_complete();
    delete _sdp_handler;
}

void InputDevice::start_sdp()
{
    delete _sdp_handler;
    _sdp_handler = new (std::nothrow) SDPParse(_sdp);
    if (!_sdp_handler)
        fail();
}

void InputDevice::update_sdp()
{
    if (_sdp_handler && (_sdp_handler->update() == 0)) {
        // On SDP failure keep going with whatever was parsed: the pad may
        // still work through the generic report path.
        _is_gamepad = false;
        try {
            _hid_descriptor = _sdp_handler->parse();
        } catch (...) {
            _hid_descriptor.clear();    // out of memory: treat as no descriptor
        }
        delete _sdp_handler;
        _sdp_handler = 0;
        const int sdp = _sdp;
        _sdp = 0;       // clear first: closing notifies socket_changed()
        l2_close(sdp);

        if (!_hid_descriptor.empty()) {
            try {
                _is_gamepad = _gamepad.parse_descriptor(
                    (const uint8_t*)_hid_descriptor.c_str(),
                    (int)_hid_descriptor.length());
            } catch (...) {
                _is_gamepad = false;
            }
        }
#if BT_TRACE
        HID_TRACE("HID: %s descriptor len=%u gamepad=%d\n", name(),
                  (unsigned)_hid_descriptor.length(), _is_gamepad ? 1 : 0);
        for (size_t i = 0; i < _hid_descriptor.length(); ++i)
            HID_TRACE("%02X%s", (uint8_t)_hid_descriptor[i], ((i & 31) == 31) ? "\n" : " ");
        HID_TRACE("\n");
#endif
        if (!_is_gamepad) {
            _pad_index = -1;
            generic_pad_release(this);
        }

        open_hid_channels();
    }
}

// New links resolve identity before opening HID channels.

static uint32_t hid_now_ms()
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

// Abort the handshake/connection AND drop the radio link. Plain
// disconnection_complete() only reset host state, leaving the pad connected
// at baseband level, so it stopped searching and could not be re-paged.
void InputDevice::fail()
{
    HID_TRACE("HID: fail %s state=%d\n", name(), (int)_state);
    hci_disconnect_device(&_bdaddr);
    disconnection_complete();
}

void InputDevice::connect()
{
    _reconnect = 0;
    _fresh_pairing = true;
    _state = CONNECT;
    _connect_started_ms = hid_now_ms();
    _last_attempt_ms = _connect_started_ms ? _connect_started_ms : 1;
    int rc = hci_connect(&_bdaddr);
    if (rc != 0)
        disconnection_complete();
}

void InputDevice::connection_request()
{
    _reconnect = 1;
    _fresh_pairing = false;
    _state = CONNECT;
    _connect_started_ms = hid_now_ms();     // handshake timeout runs from here
    if (hci_accept_connection_request(&_bdaddr) != 0)
        disconnection_complete();
}

void InputDevice::connection_complete(int status)
{
    (void)status;
    _connect_started_ms = hid_now_ms();     // restart timer for the HID handshake
    if (_reconnect) {   // start listening to reconnections
        _control = l2_open(&_bdaddr, HID_CONTROL_PSM, true);
        _interrupt = l2_open(&_bdaddr, HID_INTERRUPT_PSM, true);
        if (_control <= 0 || _interrupt <= 0) {
            fail();
            return;
        }
    }
    _state = READING_NAME;
    if (hci_remote_name_request(&_bdaddr) != 0)
        fail();
}

void InputDevice::remote_name_response(const char* n)
{
    _name = n;

    const bool is_nintendo_family = is_nintendo_family_device_class_name(_dev_class, _name);

    if (_reconnect) {
        // The pad opens the HID channels itself and may already have done
        // so before the name arrived; don't downgrade OPEN.
        if (_state == READING_NAME)
            _state = OPENING;
        return;
    }

    if (is_native_switch_name(_name)) {
        _needs_sdp_after_auth = false;
        _state = AUTHENTICATING;
        if (hci_authentication_requested(&_bdaddr) != 0)
            fail();
        return;
    }

    if (is_nintendo_family) {
        open_hid_channels();
        return;
    }

    if (should_bypass_explicit_auth_for_generic_hid(_dev_class, _name)) {
        _needs_sdp_after_auth = false;
        if (!_is_gamepad && !_sdp) {
            _sdp = l2_open(&_bdaddr,HID_SDP_PSM);
            if (_sdp <= 0) {
                _sdp = 0;
                open_hid_channels();    // skip SDP rather than hang on it
                return;
            }
            _state = READING_SDP;
            return;
        }
        open_hid_channels();
        return;
    }

    if (_fresh_pairing)
        forget_link_key(&_bdaddr);
    _needs_sdp_after_auth = !_is_gamepad;
    _state = AUTHENTICATING;
    if (hci_authentication_requested(&_bdaddr) != 0)
        fail();
}

void InputDevice::authentication_complete(int status)
{
    const bool is_native_switch = is_native_switch_name(_name);
    const bool is_nintendo_family = is_nintendo_family_device_class_name(_dev_class, _name);

    if (status != 0) {
        // 0x05 Authentication Failure / 0x06 Key Missing: our stored key
        // no longer matches (pad re-paired elsewhere, or reset). Keeping it
        // made every later reconnect fail the same way.
        const bool stale_key = status == 0x05 || status == 0x06;
        if (is_native_switch) {
            forget_link_key(&_bdaddr);
            fail();
            return;
        }
        if (status == 5 && is_nintendo_family) {
            // Wii remotes legitimately fail auth; carry on unauthenticated.
        } else {
            if (stale_key)
                forget_link_key(&_bdaddr);
            fail();
            return;
        }
    }

    if (_reconnect)
        return;

    if (is_native_switch) {
        _state = ENCRYPTING;
        int rc = hci_set_connection_encryption(&_bdaddr,true);
        if (rc != 0)
            fail();
        return;
    }

    if (_needs_sdp_after_auth && !_sdp) {
        _needs_sdp_after_auth = false;
        _sdp = l2_open(&_bdaddr,HID_SDP_PSM);
        if (_sdp > 0) {
            _state = READING_SDP;
            return;
        }
        _sdp = 0;   // SDP unavailable: open HID channels without it
    }

    open_hid_channels();
}

void InputDevice::encryption_changed(int status, bool enabled)
{
    if (_state != ENCRYPTING)
        return;     // only the Switch path asks for encryption explicitly
    if (status != 0 || !enabled) {
        if (is_native_switch_name(_name))
            forget_link_key(&_bdaddr);
        fail();
        return;
    }
    open_hid_channels();
}

void InputDevice::disconnection_complete()
{
    // Clear the ids before closing: closing a socket notifies
    // socket_changed(), which must not treat it as a new failure.
    const int sdp = _sdp;
    const int control = _control;
    const int interrupt = _interrupt;
    _sdp = _control = _interrupt = 0;
    l2_close(sdp);
    l2_close(control);
    l2_close(interrupt);
    delete _sdp_handler;
    _sdp_handler = 0;

    _reconnect = 0;
    _fresh_pairing = false;
    _needs_sdp_after_auth = false;
    _connect_started_ms = 0;
    _state = CLOSED;
    if (_wii_index != -1)
        memset(wii_states + _wii_index, 0, sizeof(wii_states[_wii_index]));
    _wii_index = -1;
    _pad_index = -1;
    _last_buttons = 0;
    _pro_3f_layout = 0;
    _pro_3f_samples = 0;
    _pro_3f_score_wii_u = 0;
    _pro_3f_score_classic = 0;
    generic_pad_release(this);
}

void InputDevice::open_hid_channels()
{
    if (_reconnect)
        return;
    if (!_control) {
        _state = OPENING;
        _control = l2_open(&_bdaddr, HID_CONTROL_PSM);
        if (_control <= 0) {
            _control = 0;
            fail();
        }
        return;
    }
    if (!_interrupt) {
        _state = OPENING;
        _interrupt = l2_open(&_bdaddr, HID_INTERRUPT_PSM);
        if (_interrupt <= 0) {
            _interrupt = 0;
            fail();
        }
        return;
    }
}

void InputDevice::socket_changed(int socket, int state)
{
    if (socket <= 0 || _state == CLOSED)
        return;

    if (state == L2CAP_CLOSED || state == L2CAP_DELETED) {
        if (socket == _sdp) {
            // SDP refused or dropped: continue without a descriptor.
            delete _sdp_handler;
            _sdp_handler = 0;
            _sdp = 0;
            open_hid_channels();
        } else if (socket == _control || socket == _interrupt) {
            // A HID channel was refused or closed by the pad. Previously
            // ignored, leaving the device stuck in OPENING.
            if (socket == _control)
                _control = 0;
            else
                _interrupt = 0;
            fail();
        }
        return;
    }

    if ((socket == _sdp) && (state == L2CAP_OPEN))  // wait for sdp channel to be open before sending query
        start_sdp();
    else if ((socket == _control) && (state == L2CAP_OPEN)) {
        // Open interrupt after control; Switch pads reject overlapping requests.
        open_hid_channels();
    } else if ((socket == _interrupt) && (state == L2CAP_OPEN)) {
        _state = OPEN;
        _connect_started_ms = 0;
        HID_TRACE("HID: %s OPEN gamepad=%d reconnect=%d\n", name(), _is_gamepad ? 1 : 0, _reconnect ? 1 : 0);
    }
}

const char* InputDevice::name()
{
    return _name.empty() ? batostr(_bdaddr) : _name.c_str();
}

class HIDSource {
    static const int MAX_CLASSIC_CONTROLLERS = 2;
    // Paging (~5 s) + name + pairing/SSP + SDP + two L2CAP channels.
    static const uint32_t HANDSHAKE_TIMEOUT_MS = 20000;
    string _local_name;
    vector<InputDevice*> _devices;
    WII _wii;
    mutable std::recursive_mutex _mutex;
    bool _inquiry_running;
    bool _pairing_enabled;
    int _pairing_baseline_connected;
    std::atomic<int> _cached_pairing_enabled;
    std::atomic<int> _cached_searching;
    std::atomic<int> _cached_connected_count;
    uint32_t _next_inquiry_start_ms;
    uint32_t _inquiry_started_ms;
    uint32_t _idle_shutdown_started_ms;
    bool _pairing_requested_once;
    bool _shutting_down;

    InputDevice* get_device(const bdaddr_t* bdaddr)
    {
        for (auto d : _devices)
            if (memcmp(&d->_bdaddr,bdaddr,6) == 0)
                return d;
        return NULL;
    }

    void clear_inquiry_marks()
    {
        for (auto* d : _devices)
            d->_seen_in_inquiry = false;
    }

    int connected_device_count() const
    {
        int count = 0;
        for (auto* d : _devices) {
            if (d && d->_state == InputDevice::OPEN)
                ++count;
        }
        return count;
    }

    int occupied_device_count() const
    {
        int count = 0;
        for (auto* d : _devices) {
            if (d && d->_state != InputDevice::CLOSED)
                ++count;
        }
        return count;
    }

    bool has_pending_device_activity() const
    {
        for (auto* d : _devices) {
            if (!d)
                continue;
            if (d->_state != InputDevice::CLOSED && d->_state != InputDevice::OPEN)
                return true;
        }
        return false;
    }

    void refresh_status_cache_unlocked()
    {
        _cached_pairing_enabled.store(_pairing_enabled ? 1 : 0, std::memory_order_relaxed);
        _cached_searching.store((_pairing_enabled || _inquiry_running) ? 1 : 0, std::memory_order_relaxed);
        _cached_connected_count.store(connected_device_count(), std::memory_order_relaxed);
    }

    uint32_t now_ms() const
    {
        return (uint32_t)(esp_timer_get_time() / 1000ULL);
    }

#if BT_TRACE
    // Logs a raw report whenever its content changes (first 200 changes).
    void trace_report(InputDevice* d, const uint8_t* rx, int len)
    {
        static uint8_t last[64];
        static int last_len = -1;
        static int logged = 0;
        const int n = min(len, (int)sizeof(last));
        if (logged >= 200 || (n == last_len && memcmp(last, rx, n) == 0))
            return;
        memcpy(last, rx, n);
        last_len = n;
        logged++;
        HIDGamepad::DecodedState st;
        const bool ok = d->_is_gamepad && d->_gamepad.decode_report_state(rx, len, &st, hid_decode_quirks(d));
        printf("HIDRX len=%d gp=%d ok=%d btn=%04X raw=%08X hat=%d |", len,
               d->_is_gamepad ? 1 : 0, ok ? 1 : 0, ok ? st.buttons : 0,
               ok ? (unsigned)st.raw_buttons : 0u, ok ? st.hat : -9);
        for (int i = 0; i < n; ++i)
            printf(" %02X", rx[i]);
        printf("\n");
    }
#endif

    void start_inquiry_cycle()
    {
        if (_inquiry_running)
            return;
        uint32_t now = now_ms();
        if ((int32_t)(now - _next_inquiry_start_ms) < 0)
            return;

        clear_inquiry_marks();
        if (hci_start_inquiry(5) >= 0) {
            _inquiry_running = true;
            _inquiry_started_ms = now;
        } else {
            _next_inquiry_start_ms = now + 1000;
        }
    }

    void stop_pairing_mode()
    {
        if (_inquiry_running) {
            clear_inquiry_marks();
            hci_cancel_inquiry();
            _inquiry_running = false;
        }
        _pairing_enabled = false;
        _pairing_baseline_connected = connected_device_count();
    }

    void update_pairing()
    {
        if (!_pairing_enabled)
            return;

        _idle_shutdown_started_ms = 0;
        uint32_t now = now_ms();
        if (connected_device_count() > _pairing_baseline_connected) {
            stop_pairing_mode();
            return;
        }

        if (_inquiry_running && (int32_t)(now - (_inquiry_started_ms + 8000)) >= 0) {
            clear_inquiry_marks();
            hci_cancel_inquiry();
            _inquiry_running = false;
            _next_inquiry_start_ms = now + 1000;
        }

        if (connected_device_count() >= MAX_CLASSIC_CONTROLLERS) {
            stop_pairing_mode();
            return;
        }

        if (_inquiry_running || has_pending_device_activity())
            return;

        start_inquiry_cycle();
    }

    // Any handshake stage can stall (lost event, pad that never answers).
    // Previously only CONNECT timed out, and only while pairing was enabled;
    // a stall elsewhere blocked inquiry and the slot forever.
    void expire_stalled_handshakes()
    {
        const uint32_t now = now_ms();
        for (auto* d : _devices) {
            if (!d || d->_state == InputDevice::CLOSED || d->_state == InputDevice::OPEN ||
                !d->_connect_started_ms)
                continue;
            if ((int32_t)(now - (d->_connect_started_ms + HANDSHAKE_TIMEOUT_MS)) < 0)
                continue;
            HID_TRACE("HID: handshake timeout %s state=%d\n", d->name(), (int)d->_state);
            d->fail();
            _next_inquiry_start_ms = now + 1000;
        }
    }

    // Pairing used to be enabled exactly once, at boot. Once a pad connected
    // it was switched off for good, so a pad that dropped could only come
    // back if it reconnected by itself. Re-arm it when nothing is connected.
    void rearm_pairing_when_idle()
    {
        // Respect the sketch's boot sequencing (discovery starts after video
        // init): only re-arm once pairing has been requested at least once.
        if (!_pairing_requested_once || _pairing_enabled || occupied_device_count() > 0)
            return;
        _pairing_enabled = true;
        _pairing_baseline_connected = 0;
    }

    static int hci_cb(HCI_CALLBACK_EVENT evt, const void* data, int len, void *ref)
    {
        return ((HIDSource*)ref)->cb(evt,data,len);
    }

    bool is_input_device(const uint8_t* dev_class)
    {
        return is_bluetooth_peripheral_input_class(device_class_value(dev_class)) ||
               is_bluetooth_peripheral_input_class(device_class_value_reversed(dev_class));
    }

    // add a device if we have not seen it before
    InputDevice* add_device(const bdaddr_t* addr, const uint8_t* dev_class)
    {
        if (dev_class && !is_input_device(dev_class))
            return 0;

        InputDevice* d = get_device(addr);
        if (!d) {
            d = new (std::nothrow) InputDevice();
            if (!d)
                return 0;
            d->_bdaddr = *addr;
            d->_dev_class = dev_class ? device_class_value(dev_class) : 0;
            d->_control = d->_interrupt = d->_sdp = 0;
            d->_state = InputDevice::CLOSED;
            try {
                _devices.push_back(d);
            } catch (...) {
                delete d;
                return 0;
            }
        }
        d->_seen_in_inquiry = true;
        return d;
    }

    int cb(HCI_CALLBACK_EVENT evt, const void* data, int len)
    {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        if (_shutting_down)
            return 0;
        InputDevice* d;
        const cbdata* cb = (const cbdata*)data;
        switch (evt) {
            case CALLBACK_READY:
                if (_pairing_enabled)
                    start_inquiry_cycle();
                break;

            case CALLBACK_INQUIRY_RESULT:
                if (_inquiry_running && cb && len >= (int)sizeof(inquiry_info))
                    add_device(&cb->ii.bdaddr,cb->ii.dev_class);
                break;

            case CALLBACK_INQUIRY_DONE:
            {
                _inquiry_running = false;
                if (occupied_device_count() >= MAX_CLASSIC_CONTROLLERS)
                    break;
                // Try the candidate attempted least recently. Always taking
                // the first one meant a nearby device that keeps failing
                // (someone else's pad, a phone) starved every other pad.
                InputDevice* best = 0;
                for (auto* id : _devices) {
                    if (!id->_seen_in_inquiry || id->_state != InputDevice::CLOSED)
                        continue;
                    if (!best || (int32_t)(id->_last_attempt_ms - best->_last_attempt_ms) < 0)
                        best = id;
                }
                if (best)
                    best->connect();
                break;
            }

            case CALLBACK_REMOTE_NAME:
                if (!cb || len < (int)sizeof(remote_name_info))
                    break;
                d = get_device(&cb->ri.bdaddr);
                if (d)
                    d->remote_name_response(cb->ri.name);
                break;

            case CALLBACK_CONNECTION_REQUEST:
                if (!cb || len < (int)sizeof(connection_request_info))
                    break;
                d = add_device(&cb->cri.bdaddr,cb->cri.dev_class);
                if (d && (d->_state != InputDevice::CLOSED ||
                          occupied_device_count() < MAX_CLASSIC_CONTROLLERS)) {
                    d->connection_request();
                } else {
                    hci_reject_connection_request(&cb->cri.bdaddr,0x0D);
                }
                break;

            case CALLBACK_DISCONNECTION_COMP:   // reconnect on disconnect?
                if (!cb || len < (int)sizeof(bdaddr_t))
                    break;
                d = get_device(&cb->bdaddr);
                if (d)
                    d->disconnection_complete();
                break;

            case CALLBACK_CONNECTION_FAILED:
                if (!cb || len < (int)sizeof(connection_info))
                    break;
                d = get_device(&cb->ci.bdaddr);
                if (d)
                    d->disconnection_complete();
                break;

            case CALLBACK_CONNECTION_COMPLETE:
                if (!cb || len < (int)sizeof(connection_info))
                    break;
                d = get_device(&cb->ci.bdaddr);
                if (d)
                    d->connection_complete(cb->ci.status);    // device connection is complete, open l2cap sockets
                break;

            case CALLBACK_AUTHENTICATION_COMPLETE:
                if (!cb || len < (int)sizeof(authentication_complete_info))
                    break;
                d = get_device(&cb->aci.bdaddr);
                if (d)
                    d->authentication_complete(cb->aci.status);
                break;

            case CALLBACK_ENCRYPTION_CHANGE:
                if (!cb || len < (int)sizeof(encryption_change_info))
                    break;
                d = get_device(&cb->eci.bdaddr);
                if (d)
                    d->encryption_changed(cb->eci.status,cb->eci.enabled != 0);
                break;

            case CALLBACK_L2CAP_SOCKET_STATE_CHANGED:
                if (!cb || len < (int)sizeof(socket_state))
                    break;
                d = get_device(&cb->ss.bdaddr);
                if (d) {
                    d->socket_changed(cb->ss.socket,cb->ss.state);
                    if (cb->ss.state == L2CAP_OPEN && cb->ss.socket == d->_interrupt)
                        _wii.on_open(d);
                }
                break;

            default:
                break;
        }
        refresh_status_cache_unlocked();
        return 0;
    }

    public:
    HIDSource(const char* localname)
        : _local_name(localname),
          _inquiry_running(false),
          _pairing_enabled(false),
          _pairing_baseline_connected(0),
          _cached_pairing_enabled(0),
          _cached_searching(0),
          _cached_connected_count(0),
          _next_inquiry_start_ms(0),
          _inquiry_started_ms(0),
          _idle_shutdown_started_ms(0),
          _pairing_requested_once(false),
          _shutting_down(false)
    {
        hci_init(hci_cb,this,_local_name.c_str());
    }

    ~HIDSource()
    {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        _shutting_down = true;      // ignore callbacks fired during teardown
        vector<InputDevice*> devices;
        devices.swap(_devices);
        for (auto* d : devices)
            delete d;               // closes its sockets while HCI is alive
        hci_shutdown();             // then devices, sockets and the controller
    }

    int update()
    {
        // Hold the lock while the HCI layer runs: it creates/deletes L2CAP
        // sockets and devices that get() (emu thread, other core) reads
        // through l2_recv(). The mutex is recursive, so callbacks from
        // hci_update() into cb() are fine.
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        int rc = hci_update();
        for (auto* d : _devices) {
            if (d->_sdp)
                d->update_sdp();    // moved off the emu thread (small stacks)
        }
        expire_stalled_handshakes();
        rearm_pairing_when_idle();
        update_pairing();
        for (auto* d : _devices)
            _wii.poll_extension(d);
        refresh_status_cache_unlocked();
        return rc;
    }

    int set_pairing(int enabled)
    {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        _idle_shutdown_started_ms = 0;
        if (!enabled) {
            _pairing_enabled = false;
            _pairing_baseline_connected = connected_device_count();
            if (_inquiry_running) {
                clear_inquiry_marks();
                hci_cancel_inquiry();
                _inquiry_running = false;
            }
            refresh_status_cache_unlocked();
            return 0;
        }

        _pairing_enabled = true;
        _pairing_requested_once = true;
        _pairing_baseline_connected = connected_device_count();
        update_pairing();
        refresh_status_cache_unlocked();
        return _pairing_enabled ? 1 : 0;
    }

    int get_pairing() const
    {
        std::unique_lock<std::recursive_mutex> lock(_mutex, std::try_to_lock);
        if (lock.owns_lock())
            const_cast<HIDSource*>(this)->refresh_status_cache_unlocked();
        return _cached_pairing_enabled.load(std::memory_order_relaxed);
    }

    int is_searching() const
    {
        std::unique_lock<std::recursive_mutex> lock(_mutex, std::try_to_lock);
        if (lock.owns_lock())
            const_cast<HIDSource*>(this)->refresh_status_cache_unlocked();
        return _cached_searching.load(std::memory_order_relaxed);
    }

    int connected_count() const
    {
        std::unique_lock<std::recursive_mutex> lock(_mutex, std::try_to_lock);
        if (lock.owns_lock())
            const_cast<HIDSource*>(this)->refresh_status_cache_unlocked();
        return _cached_connected_count.load(std::memory_order_relaxed);
    }

    // Drain a bounded burst and return the latest decoded state.
    // called from emu thread
    int get(uint8_t* dst, int dst_len)
    {
        if (!dst || dst_len <= 0)
            return 0;

        // Blocking: update() now holds the lock across hci_update() (a few
        // hundred us on the other core). try_lock would drop input on a
        // noticeable share of frames. Lock order (this mutex, then PacketQ
        // mutexes) is the same on both threads, so this cannot deadlock.
        std::lock_guard<std::recursive_mutex> lock(_mutex);

        uint8_t rx[128];
        int latest_out_len = 0;
        bool have_decoded = false;  // a decoded/generic report beats raw passthrough

        for (auto d : _devices) {
            if (d->_interrupt) {
                const bool wii_device = _wii.is_wii(d);
                const int drain_limit = wii_device ? HID_WII_DRAIN_BUDGET : HID_GET_DRAIN_BUDGET;
                int drain_budget = drain_limit;
                while (drain_budget-- > 0) {
                    int len = l2_recv(d->_interrupt,rx,sizeof(rx));
                    if (len > 0) {
#if BT_TRACE
                        trace_report(d, rx, len);
#endif
                        WII::HidResult wii_result = _wii.hid(d,rx,len);
                        if (wii_result != WII::HID_IGNORED) {
                            if (wii_result == WII::HID_GENERIC_REPORT_CHANGED) {
                                int out_len = generic_pad_build_report(dst, dst_len);
                                if (out_len > 0) {
                                    latest_out_len = out_len;
                                    have_decoded = true;
                                    break;
                                }
                            } else if (wii_result == WII::HID_REPORT_CHANGED) {
                                const uint8_t* out = rx;
                                int out_len = len;
                                if (wii_device && len > 1 && rx[1] == 0x21 &&
                                    d->_wii_index >= 0 && d->_wii_index < 4 &&
                                    (wii_states[d->_wii_index].flags & extension_polled)) {
                                    out = wii_states[d->_wii_index].report;
                                    out_len = 10;
                                }
                                latest_out_len = min(dst_len, out_len);
                                if (latest_out_len > 0) {
                                    memcpy(dst, out, latest_out_len);
                                    have_decoded = true;
                                    break;
                                }
                            }
                            continue;
                        }
                        if (d->_is_gamepad) {
                            if (d->_pad_index < 0)
                                d->_pad_index = generic_pad_assign(d);
                            HIDGamepad::DecodedState decoded;
                            if (d->_pad_index >= 0 &&
                                d->_gamepad.decode_report_state(rx,len,&decoded,hid_decode_quirks(d))) {
                                uint16_t buttons = decoded.buttons;
                                if (is_playstation_wireless_controller(d))
                                    buttons = remap_playstation_buttons(buttons);
                                d->_last_buttons = buttons;
                                generic_pad_update_ex(d, buttons, decoded.raw_buttons);
                                int out_len = generic_pad_build_report(dst, dst_len);
                                if (out_len > 0) {
                                    latest_out_len = out_len;
                                    have_decoded = true;
                                    continue;
                                }
                            }
                        }
                        if (!have_decoded) {
                            // Newest raw report wins now that several are
                            // drained per frame.
                            latest_out_len = min(dst_len, len);
                            if (latest_out_len > 0)
                                memcpy(dst, rx, latest_out_len);
                        }
                        continue;
                    }
                    if (len < 0) {
                        d->fail();
                    }
                    break;
                }
            }
        }
        return latest_out_len;
    }
};

HIDSource* _hid_source = 0;
static string _hid_local_name = "emu32";

int hid_init(const char* local_name)
{
    _hid_local_name = local_name ? local_name : "emu32";
    hid_close();
    _hid_source = new HIDSource(_hid_local_name.c_str());
    return 0;
}

int hid_close()
{
    delete _hid_source;
    _hid_source = 0;
    return 0;
}

int hid_update()
{
    if (!_hid_source)
        return -1;
    // No idle shutdown: it called btStop() 2 s after the last pad left,
    // which also stopped page scan, so paired pads could not reconnect
    // until the device was rebooted.
    return _hid_source->update();
}

int hid_get(uint8_t* dst, int dst_len)
{
    if (!_hid_source)
        return -1;
    return _hid_source->get(dst,dst_len);
}

int hid_set_pairing(int enabled)
{
    if (!_hid_source && enabled)
        hid_init(_hid_local_name.c_str());
    if (!_hid_source)
        return 0;
    return _hid_source->set_pairing(enabled);
}

int hid_get_pairing()
{
    if (!_hid_source)
        return 0;
    return _hid_source->get_pairing();
}

int hid_is_searching()
{
    if (!_hid_source)
        return 0;
    return _hid_source->is_searching();
}

int hid_connected_count()
{
    if (!_hid_source)
        return 0;
    return _hid_source->connected_count();
}

// map wii controller keys
uint32_t wii_map(int index, const uint32_t* common, const uint32_t* classic)
{
    uint32_t f = wii_states[index].flags;
    if (!(f & wiimote))
        return 0;

    uint32_t r = 0;
    int pad = wii_states[index].common();
    for (int i = 0; i < 16; i++) {
        if (pad & (0x8000 >> i))
            r |= common[i];
    }
    if (classic && (f & classic_controller)) {
        pad = wii_states[index].classic();
        for (int i = 0; i < 16; i++) {
            if (pad & (0x8000 >> i))
                r |= classic[i];
        }
    }
    return r;
}
