#ifndef NES_GLITCH_H
#define NES_GLITCH_H

#include "../nofrendo/noftypes.h"
#include "glitch_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NES_GLITCH_ENABLED
#define NES_GLITCH_ENABLED 1
#endif

#ifndef NES_PPU_GLITCH_ENABLED
#define NES_PPU_GLITCH_ENABLED 1
#endif

void nes_glitch_init(void);
void nes_glitch_update(void);
void nes_glitch_set_enabled(bool enabled);
bool nes_glitch_is_active(void);

/* Apply NES APU-side glitching to a write. Returns true to drop the write. */
bool nes_glitch_apply_cpu_write(uint32 address, uint8 *value);

/* Corrupt NES OAM at the actual write/DMA point. */
void nes_glitch_apply_oam_write(uint8 oam_addr, uint8 *value);

/* Apply the shared PPU address-line mask to an NES PPU address. */
static inline uint16 nes_glitch_apply_ppu_address(uint16 address)
{
#if NES_PPU_GLITCH_ENABLED
    return glitch_runtime_apply_ppu_address_mask(address);
#else
    return address;
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* NES_GLITCH_H */
