#ifndef BT_MODE_H
#define BT_MODE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BT_TRANSPORT_DUAL = 0,
    BT_TRANSPORT_CLASSIC_ONLY = 1,
    BT_TRANSPORT_BLE_ONLY = 2,
} bt_transport_mode_t;

bt_transport_mode_t bt_transport_normalize(int mode, bt_transport_mode_t fallback);
bt_transport_mode_t bt_transport_load_pref(bt_transport_mode_t fallback);
void bt_transport_save_pref(bt_transport_mode_t mode);
const char* bt_transport_name(bt_transport_mode_t mode);
const char* bt_transport_user_label(bt_transport_mode_t mode);
bt_transport_mode_t bt_transport_cycle_user_mode(bt_transport_mode_t current, int delta);
void bt_transport_set_active_mode(bt_transport_mode_t mode);
bt_transport_mode_t bt_transport_get_active_mode();

#ifdef __cplusplus
}
#endif

#endif // BT_MODE_H
