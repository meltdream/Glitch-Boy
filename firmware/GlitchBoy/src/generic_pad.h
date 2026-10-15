#ifndef GENERIC_PAD_H
#define GENERIC_PAD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int generic_pad_assign(const void* owner);
void generic_pad_release(const void* owner);
void generic_pad_update(const void* owner, uint16_t state);
void generic_pad_update_ex(const void* owner, uint16_t state, uint64_t raw_buttons);
void generic_pad_set_player1_overlay(uint16_t state);
int generic_pad_build_report(uint8_t* dst, int dst_len);

#ifdef __cplusplus
}
#endif

#endif // GENERIC_PAD_H
