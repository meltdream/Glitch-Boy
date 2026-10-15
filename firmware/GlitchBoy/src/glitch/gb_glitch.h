#ifndef GB_GLITCH_H
#define GB_GLITCH_H

#include <stdint.h>

#ifndef __cplusplus
#include "../nofrendo/noftypes.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern volatile uint16_t g_gb_glitch_ppu_mask;
extern volatile uint16_t g_gb_glitch_bg_mask;

void gb_glitch_sync_state(void);
void gb_glitch_apply_oam_write(uint16_t address, uint8_t *value);
void gb_glitch_apply_oam_dma(uint8_t *oam);
bool gb_glitch_apply_apu_write(uint16_t address, uint8_t *value);

#ifdef __cplusplus
}
#endif

#endif /* GB_GLITCH_H */
