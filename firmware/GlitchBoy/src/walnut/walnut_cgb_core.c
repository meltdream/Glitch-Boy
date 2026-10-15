#if defined(__GNUC__)
#pragma GCC optimize("O3","unroll-loops")
#endif

#define ENABLE_SOUND 1
#define ENABLE_LCD 1
#define WALNUT_GB_12_COLOUR 1
#define WALNUT_FULL_GBC_SUPPORT 1
#define MINIGB_APU_AUDIO_FORMAT_S16SYS

#include <stdint.h>

#include "minigb_apu.h"

static struct minigb_apu_ctx *s_walnut_apu = 0;

void walnutgb_audio_bind(struct minigb_apu_ctx *ctx)
{
    s_walnut_apu = ctx;
}

uint8_t audio_read(uint16_t addr)
{
    if (!s_walnut_apu)
        return 0xFF;
    return minigb_apu_audio_read(s_walnut_apu, addr);
}

void audio_write(uint16_t addr, uint8_t val)
{
    if (!s_walnut_apu)
        return;
    minigb_apu_audio_write(s_walnut_apu, addr, val);
}
