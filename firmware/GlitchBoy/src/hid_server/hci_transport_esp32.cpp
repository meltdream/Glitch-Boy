#include <esp_bt.h>
#include <esp_err.h>
#include <esp32-hal-log.h>
#include <esp32-hal-bt.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <stdio.h>
#include <string.h>

#ifndef HID_BT_CONTROLLER_LOG_LEVEL
#define HID_BT_CONTROLLER_LOG_LEVEL 0
#endif

// Keep the original namespace so existing settings and pairings survive updates.
#define GLITCH_BOY_NVS_NAMESPACE "esp_8_bit"

extern "C" void esp_bt_controller_set_log_level(int level);

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to enable it
#endif

#include <esp_heap_caps.h>

#include "hci_transport.h"
#include "../bt_mode.h"

// HCI transport for ESP32

// wrapper for vhci
struct {
    hci_on_packet_handler handler;
    void* handler_ref;
    hci_on_ready_to_send_handler ready_handler;
    void* ready_handler_ref;
    esp_vhci_host_callback_t _cb;
} _hci_transport;

static bool configure_classic_bt_controller_runtime()
{
#if HID_BT_CONTROLLER_LOG_LEVEL >= 0
    esp_bt_controller_set_log_level(HID_BT_CONTROLLER_LOG_LEVEL);
#endif
    esp_err_t sleep_rc = esp_bt_sleep_disable();
    if (sleep_rc != ESP_OK)
        log_w("esp_bt_sleep_disable failed rc=%d", (int)sleep_rc);
    return true;
}

// The transport mode is fixed per boot (changing it in the menu needs a
// reboot), so a Classic-only boot never uses the controller's BLE-only RAM.
// Releasing it returns ~5 KB of contiguous internal DRAM to the heap.
static void release_unused_ble_controller_memory()
{
    static bool released = false;
    if (released || esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_IDLE)
        return;
    const size_t before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    esp_err_t rc = esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
    released = true;    // never retry: a second release would re-add the region
    printf("[bt] released BLE controller memory rc=%d free_i8 %u -> %u largest_i8=%u\n",
           (int)rc,
           (unsigned)before,
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
}

static bool start_classic_bt_controller()
{
    esp_bt_controller_status_t status = esp_bt_controller_get_status();
    if (status == ESP_BT_CONTROLLER_STATUS_ENABLED)
        return configure_classic_bt_controller_runtime();

    if (status == ESP_BT_CONTROLLER_STATUS_IDLE) {
        if (bt_transport_get_active_mode() == BT_TRANSPORT_CLASSIC_ONLY)
            release_unused_ble_controller_memory();
        esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        cfg.mode = ESP_BT_MODE_CLASSIC_BT;
        cfg.ble_max_conn = 0;
        // Arduino-ESP32 1.0.6 needs one spare ACL entry for two VHCI gamepads.
        cfg.bt_max_acl_conn = 3;
        esp_err_t rc = esp_bt_controller_init(&cfg);
        if (rc != ESP_OK)
            return false;
        status = esp_bt_controller_get_status();
    }

    if (status == ESP_BT_CONTROLLER_STATUS_INITED) {
        esp_err_t rc = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
        if (rc != ESP_OK)
            return false;
        status = esp_bt_controller_get_status();
    }

    return status == ESP_BT_CONTROLLER_STATUS_ENABLED && configure_classic_bt_controller_runtime();
}

hci_handle hci_open()
{
    if (!start_classic_bt_controller()) {
        log_e("Classic BT controller start failed");
        return NULL;
    }

    _hci_transport._cb.notify_host_recv = [](uint8_t *data, uint16_t len) -> int {
        if (_hci_transport.handler)
            _hci_transport.handler(&_hci_transport,data,len,_hci_transport.handler_ref);
        return 0;
    };
    _hci_transport._cb.notify_host_send_available = []() {
        if (_hci_transport.ready_handler)
            _hci_transport.ready_handler(&_hci_transport,_hci_transport.ready_handler_ref);
    };

    esp_vhci_host_register_callback(&_hci_transport._cb); // open for buisness
    return &_hci_transport;
}

int hci_close(hci_handle h)
{
    _hci_transport.handler = 0;
    _hci_transport.handler_ref = 0;
    _hci_transport.ready_handler = 0;
    _hci_transport.ready_handler_ref = 0;
    btStop();
    return 0;
}

void hci_set_packet_handler(hci_handle h, hci_on_packet_handler p, void* ref)
{
    _hci_transport.handler = p;
    _hci_transport.handler_ref = ref;
}

void hci_set_ready_to_send_handler(hci_handle h, hci_on_ready_to_send_handler p, void* ref)
{
    _hci_transport.ready_handler = p;
    _hci_transport.ready_handler_ref = ref;
}

int  hci_send(hci_handle h, const uint8_t* data, int len)
{
    esp_vhci_host_send_packet((uint8_t*)data,len);
    return 0;
}

int  hci_send_available(hci_handle h)
{
    return esp_vhci_host_check_send_available();
}

// store and load link keys
uint32_t _nvs_handle = 0;
static bool _nvs_init_attempted = false;

static uint32_t open_nvs()
{
    if (_nvs_handle)
        return _nvs_handle;

    if (!_nvs_init_attempted) {
        _nvs_init_attempted = true;
        esp_err_t rc = nvs_flash_init();
        if (rc == ESP_ERR_NVS_NO_FREE_PAGES || rc == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            fprintf(stdout, "[pref] nvs init rc=%d, erasing nvs partition\n", (int)rc);
            nvs_flash_erase();
            rc = nvs_flash_init();
        }
#ifdef ESP_ERR_INVALID_STATE
        if (rc != ESP_OK && rc != ESP_ERR_INVALID_STATE) {
#else
        if (rc != ESP_OK) {
#endif
            fprintf(stdout, "[pref] nvs_flash_init failed rc=%d\n", (int)rc);
            return 0;
        }
    }

    esp_err_t rc = nvs_open(GLITCH_BOY_NVS_NAMESPACE, NVS_READWRITE, &_nvs_handle);
    if (rc != ESP_OK) {
        fprintf(stdout, "[pref] nvs_open failed rc=%d\n", (int)rc);
        _nvs_handle = 0;
        return 0;
    }
    return _nvs_handle;
}

extern "C" int sys_get_pref(const char* key, char* value, int max_len)
{
    if (!key || !value || max_len < 0)
        return 0;
    value[0] = 0;
    uint32_t h = open_nvs();
    if (!h)
        return 0;
    size_t s = (size_t)max_len + 1u;
    esp_err_t rc = nvs_get_str(h, key, value, &s);
    if (rc == ESP_OK) {
       value[max_len] = 0;
       return (int)s;
    }
    if (rc != ESP_ERR_NVS_NOT_FOUND && rc != ESP_ERR_NVS_INVALID_LENGTH)
        fprintf(stdout, "[pref] get %s failed rc=%d cap=%d\n", key, (int)rc, max_len + 1);
    return 0;
}

extern "C" void sys_set_pref(const char* key, const char* value)
{
    if (!key || !value)
        return;
    uint32_t h = open_nvs();
    if (!h)
        return;
    esp_err_t rc = nvs_set_str(h, key, value);
    esp_err_t commit_rc = ESP_OK;
    if (rc == ESP_OK)
        commit_rc = nvs_commit(h);
    if (rc != ESP_OK || commit_rc != ESP_OK) {
        fprintf(stdout, "[pref] set %s='%s' rc=%d commit=%d\n",
            key, value, (int)rc, (int)commit_rc);
    }
}
