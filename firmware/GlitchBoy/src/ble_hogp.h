#ifndef BLE_HOGP_H
#define BLE_HOGP_H

#include <stdint.h>

void ble_hogp_init(const char* local_name);
int ble_hogp_is_available();
void ble_hogp_update();
int get_hid_ble(uint8_t* dst);
int ble_hogp_set_pairing(int enabled);
int ble_hogp_get_pairing();
int ble_hogp_is_searching();
int ble_hogp_connected_count();

#endif // BLE_HOGP_H
