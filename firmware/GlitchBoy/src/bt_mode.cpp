#include "bt_mode.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "hid_server/hci_server.h"

static bt_transport_mode_t _active_bt_transport_mode = BT_TRANSPORT_BLE_ONLY;

static bool pref_equal(const char* value, const char* expected)
{
    if (!value || !expected)
        return false;
    while (*value && *expected) {
        char a = *value++;
        char b = *expected++;
        if (a >= 'A' && a <= 'Z')
            a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z')
            b = (char)(b - 'A' + 'a');
        if (a != b)
            return false;
    }
    return *value == 0 && *expected == 0;
}

static bt_transport_mode_t parse_transport_pref(const char* value, bt_transport_mode_t fallback)
{
    if (!value || !*value)
        return fallback;

    if (pref_equal(value, "2") || pref_equal(value, "ble") || pref_equal(value, "xbox"))
        return BT_TRANSPORT_BLE_ONLY;
    if (pref_equal(value, "1") || pref_equal(value, "classic") ||
        pref_equal(value, "standard") || pref_equal(value, "std") ||
        pref_equal(value, "0") || pref_equal(value, "dual"))
        return BT_TRANSPORT_CLASSIC_ONLY;

    bt_transport_mode_t mode = bt_transport_normalize(atoi(value), fallback);
    return mode == BT_TRANSPORT_BLE_ONLY ? BT_TRANSPORT_BLE_ONLY : BT_TRANSPORT_CLASSIC_ONLY;
}

bt_transport_mode_t bt_transport_normalize(int mode, bt_transport_mode_t fallback)
{
    switch (mode) {
        case BT_TRANSPORT_DUAL:
        case BT_TRANSPORT_CLASSIC_ONLY:
        case BT_TRANSPORT_BLE_ONLY:
            return (bt_transport_mode_t)mode;
        default:
            return fallback;
    }
}

// Dual mode is not user-selectable; treat it as Classic.
static bt_transport_mode_t normalize_user_mode(bt_transport_mode_t mode)
{
    mode = bt_transport_normalize((int)mode, BT_TRANSPORT_CLASSIC_ONLY);
    return mode == BT_TRANSPORT_DUAL ? BT_TRANSPORT_CLASSIC_ONLY : mode;
}

bt_transport_mode_t bt_transport_load_pref(bt_transport_mode_t fallback)
{
    fallback = normalize_user_mode(fallback);
    char buf[16] = {0};
    if (!sys_get_pref("btmode", buf, sizeof(buf) - 1))
        return fallback;
    return parse_transport_pref(buf, fallback);
}

void bt_transport_save_pref(bt_transport_mode_t mode)
{
    char buf[8];
    sprintf(buf, "%d", (int)normalize_user_mode(mode));
    sys_set_pref("btmode", buf);
}

const char* bt_transport_name(bt_transport_mode_t mode)
{
    switch (bt_transport_normalize((int)mode, BT_TRANSPORT_BLE_ONLY)) {
        case BT_TRANSPORT_DUAL:
            return "Dual";
        case BT_TRANSPORT_CLASSIC_ONLY:
            return "Classic";
        case BT_TRANSPORT_BLE_ONLY:
        default:
            return "BLE";
    }
}

const char* bt_transport_user_label(bt_transport_mode_t mode)
{
    mode = bt_transport_normalize((int)mode, BT_TRANSPORT_CLASSIC_ONLY);
    return mode == BT_TRANSPORT_BLE_ONLY ? "Xbox" : "Classic";
}

// Only two user modes, so cycling in either direction toggles.
bt_transport_mode_t bt_transport_cycle_user_mode(bt_transport_mode_t current, int delta)
{
    (void)delta;
    return normalize_user_mode(current) == BT_TRANSPORT_BLE_ONLY ? BT_TRANSPORT_CLASSIC_ONLY
                                                                 : BT_TRANSPORT_BLE_ONLY;
}

void bt_transport_set_active_mode(bt_transport_mode_t mode)
{
    _active_bt_transport_mode = bt_transport_normalize((int)mode, BT_TRANSPORT_CLASSIC_ONLY);
}

bt_transport_mode_t bt_transport_get_active_mode()
{
    return _active_bt_transport_mode;
}
