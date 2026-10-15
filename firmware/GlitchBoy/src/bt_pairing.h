#ifndef BT_PAIRING_H
#define BT_PAIRING_H

#ifdef __cplusplus
extern "C" {
#endif

int bt_pairing_set_enabled(int enabled);
int bt_pairing_get_enabled();
int bt_pairing_is_searching();
int bt_pairing_connected_count();
int bt_pairing_should_flash_led();

#ifdef __cplusplus
}
#endif

#endif // BT_PAIRING_H
