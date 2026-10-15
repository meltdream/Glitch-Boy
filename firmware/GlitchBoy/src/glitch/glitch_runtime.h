#ifndef GLITCH_RUNTIME_H
#define GLITCH_RUNTIME_H

#include <stdint.h>

#ifndef __cplusplus
#include "../nofrendo/noftypes.h"
#endif

/*
 * State published by the controls to the emulator adapters: a PPU address-line
 * mask and a 0..GLITCH_MAX_LEVEL corruption level for sprite/OAM and APU writes.
 */
#define GLITCH_MAX_LEVEL 9
#define GLITCH_ADDRESS_LINE_COUNT 9

#ifdef __cplusplus
extern "C" {
#endif

bool glitch_runtime_is_enabled(void);
bool glitch_runtime_is_latch_active(void);
uint8_t glitch_runtime_get_ppu_level(void);
uint8_t glitch_runtime_get_corruption_level(void);
uint16_t glitch_runtime_get_ppu_mask(void);
uint16_t glitch_runtime_get_audio_ppu_mask(void);
uint16_t glitch_runtime_find_ppu_mask_index(uint16_t mask);
uint16_t glitch_runtime_get_ppu_mask_at(uint16_t index);
uint16_t glitch_runtime_apply_ppu_address_mask(uint16_t address);

#ifdef __cplusplus
}
#endif

#endif /* GLITCH_RUNTIME_H */
