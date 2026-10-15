#include "bt_pairing.h"

#include "ble_hogp.h"
#include "bt_mode.h"
#include "hid_server/hid_server.h"

namespace {

int set_transport_pairing(bt_transport_mode_t mode, int enabled)
{
    switch (mode) {
        case BT_TRANSPORT_DUAL:
            hid_set_pairing(enabled);
            ble_hogp_set_pairing(enabled);
            return hid_get_pairing() || ble_hogp_get_pairing();
        case BT_TRANSPORT_CLASSIC_ONLY:
            return hid_set_pairing(enabled);
        case BT_TRANSPORT_BLE_ONLY:
        default:
            return ble_hogp_set_pairing(enabled);
    }
}

int get_transport_pairing(bt_transport_mode_t mode)
{
    switch (mode) {
        case BT_TRANSPORT_DUAL:
            return hid_get_pairing() || ble_hogp_get_pairing();
        case BT_TRANSPORT_CLASSIC_ONLY:
            return hid_get_pairing();
        case BT_TRANSPORT_BLE_ONLY:
        default:
            return ble_hogp_get_pairing();
    }
}

int get_transport_searching(bt_transport_mode_t mode)
{
    switch (mode) {
        case BT_TRANSPORT_DUAL:
            return hid_is_searching() || ble_hogp_is_searching();
        case BT_TRANSPORT_CLASSIC_ONLY:
            return hid_is_searching();
        case BT_TRANSPORT_BLE_ONLY:
        default:
            return ble_hogp_is_searching();
    }
}

int get_transport_connected_count(bt_transport_mode_t mode)
{
    switch (mode) {
        case BT_TRANSPORT_DUAL:
            return hid_connected_count() + ble_hogp_connected_count();
        case BT_TRANSPORT_CLASSIC_ONLY:
            return hid_connected_count();
        case BT_TRANSPORT_BLE_ONLY:
        default:
            return ble_hogp_connected_count();
    }
}

} // namespace

int bt_pairing_set_enabled(int enabled)
{
    return set_transport_pairing(bt_transport_get_active_mode(), enabled ? 1 : 0);
}

int bt_pairing_get_enabled()
{
    return get_transport_pairing(bt_transport_get_active_mode());
}

int bt_pairing_is_searching()
{
    return get_transport_searching(bt_transport_get_active_mode());
}

int bt_pairing_connected_count()
{
    return get_transport_connected_count(bt_transport_get_active_mode());
}

int bt_pairing_should_flash_led()
{
    return bt_pairing_is_searching();
}
