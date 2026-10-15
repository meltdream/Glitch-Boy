#include "nes_glitch.h"
#include "glitch_settings.h"

#if NES_GLITCH_ENABLED

#define NES_GLITCH_OAM_BASE 0x0200
#define NES_GLITCH_OAM_END  0x02FF

#define NES_GLITCH_SPRITE_MAX_CHANCE 640u  /* out of 1024 (~62%) */
#define NES_GLITCH_APU_MAX_CHANCE    860u  /* out of 1024 (~84%) */
#define NES_GLITCH_DROP_MAX_CHANCE   180u  /* out of 1024 (~18%), capped for stability */
#define NES_GLITCH_JITTER_MIN        4     /* low slot local XY jitter */
#define NES_GLITCH_JITTER_MAX        22    /* high slot stronger but still bounded */

typedef struct {
    bool enabled;
    bool apu_enabled;
    uint8 sprite_level;
    uint8 apu_level;
    uint16 sprite_threshold; /* 0..1023 chance space */
    uint16 apu_threshold;    /* 0..1023 chance space */
    uint32 rng;
} nes_glitch_state;

static nes_glitch_state g_nes_glitch = {
    true, true, 0, 0, 0, 0, 0x9E3779B9u
};

static inline uint32 nes_glitch_rand(void)
{
    uint32 x = g_nes_glitch.rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    if (0 == x) {
        x = 0xA3C59AC3u;
    }
    g_nes_glitch.rng = x;
    return x;
}

/* Non-linear (quadratic) response: low slots stay manageable, high slots get aggressive. */
static inline uint16 nes_glitch_curve(uint8 level, uint16 max_chance)
{
    uint32 num = (uint32)level * (uint32)level;
    uint32 den = GLITCH_MAX_LEVEL * GLITCH_MAX_LEVEL;
    uint32 chance = (num * (uint32)max_chance + (den / 2u)) / den;
    if (chance > 1023u)
        chance = 1023u;
    return (uint16)chance;
}

static inline uint8 nes_glitch_jitter_limit(uint8 level)
{
    uint32 denom = GLITCH_MAX_LEVEL;
    uint32 span = NES_GLITCH_JITTER_MAX - NES_GLITCH_JITTER_MIN;
    uint32 limit = NES_GLITCH_JITTER_MIN + ((span * (uint32)level + (denom / 2u)) / denom);
    if (limit > NES_GLITCH_JITTER_MAX)
        limit = NES_GLITCH_JITTER_MAX;
    return (uint8)limit;
}

static inline uint8 nes_glitch_add_clamped_u8(uint8 value, int8 delta)
{
    int out = (int)value + (int)delta;
    if (out < 0)
        out = 0;
    if (out > 255)
        out = 255;
    return (uint8)out;
}

#endif /* NES_GLITCH_ENABLED */

void nes_glitch_init(void)
{
#if NES_GLITCH_ENABLED
    g_nes_glitch.enabled = true;
    g_nes_glitch.rng ^= 0x6D2B79F5u;
#endif
}

void nes_glitch_set_enabled(bool enabled)
{
#if NES_GLITCH_ENABLED
    g_nes_glitch.enabled = enabled;
    if (!enabled) {
        g_nes_glitch.sprite_level = 0;
        g_nes_glitch.apu_level = 0;
        g_nes_glitch.sprite_threshold = 0;
        g_nes_glitch.apu_threshold = 0;
    }
#else
    (void)enabled;
#endif
}

bool nes_glitch_is_active(void)
{
#if NES_GLITCH_ENABLED
    return g_nes_glitch.enabled &&
        (g_nes_glitch.sprite_threshold != 0 || g_nes_glitch.apu_threshold != 0);
#else
    return false;
#endif
}

void glitch_settings_set_apu_enabled(bool enabled)
{
#if NES_GLITCH_ENABLED
    g_nes_glitch.apu_enabled = enabled;
    if (!enabled) {
        g_nes_glitch.apu_level = 0;
        g_nes_glitch.apu_threshold = 0;
    }
#else
    (void)enabled;
#endif
}

bool glitch_settings_is_apu_enabled(void)
{
#if NES_GLITCH_ENABLED
    return g_nes_glitch.apu_enabled;
#else
    return false;
#endif
}

void nes_glitch_update(void)
{
#if NES_GLITCH_ENABLED
    if (!g_nes_glitch.enabled || !glitch_runtime_is_enabled() || !glitch_runtime_is_latch_active()) {
        g_nes_glitch.sprite_level = 0;
        g_nes_glitch.apu_level = 0;
        g_nes_glitch.sprite_threshold = 0;
        g_nes_glitch.apu_threshold = 0;
        return;
    }

    uint8 slot2 = glitch_runtime_get_corruption_level();

    g_nes_glitch.sprite_level = slot2;
    g_nes_glitch.apu_level = g_nes_glitch.apu_enabled ? slot2 : 0;

    g_nes_glitch.sprite_threshold = nes_glitch_curve(g_nes_glitch.sprite_level, NES_GLITCH_SPRITE_MAX_CHANCE);
    g_nes_glitch.apu_threshold = g_nes_glitch.apu_enabled
        ? nes_glitch_curve(g_nes_glitch.apu_level, NES_GLITCH_APU_MAX_CHANCE)
        : 0;
#endif
}

bool nes_glitch_apply_cpu_write(uint32 address, uint8 *value)
{
#if NES_GLITCH_ENABLED
    if (!g_nes_glitch.enabled ||
        (g_nes_glitch.sprite_threshold == 0 && g_nes_glitch.apu_threshold == 0)) {
        return false;
    }

    /* Sprites are corrupted as bytes enter OAM (nes_glitch_apply_oam_write),
     * so leave OAM RAM and the DMA page alone. */
    if ((address >= NES_GLITCH_OAM_BASE && address <= NES_GLITCH_OAM_END) ||
        address == 0x4014) {
        return false;
    }

    /* APU writes */
    if ((address >= 0x4000 && address <= 0x4013) || address == 0x4015) {
        /* DMC setup registers stay untouched; $4011 DAC data is safe to corrupt. */
        if (address == 0x4010 || address == 0x4012 || address == 0x4013) {
            return false;
        }

        if (g_nes_glitch.apu_threshold != 0) {
            uint32 r = nes_glitch_rand();
            if ((r & 0x3FF) < g_nes_glitch.apu_threshold) {
                uint16 drop_chance = g_nes_glitch.apu_threshold >> 2;
                if (drop_chance > NES_GLITCH_DROP_MAX_CHANCE)
                    drop_chance = NES_GLITCH_DROP_MAX_CHANCE;
                if (address != 0x4015 && ((r >> 10) & 0x3FF) < drop_chance) {
                    return true; /* drop write */
                }
                /* Flip one or two low APU control bits. */
                uint8 bit_count = (address == 0x4015) ? 4 : 5;
                uint8 b1 = (uint8)(1u << ((r >> 2) % bit_count));
                uint8 b2 = 0;
                if (r & (1u << 12)) {
                    b2 = (uint8)(1u << ((r >> 7) % bit_count));
                }
                *value ^= (uint8)(b1 | b2);
                if (address != 0x4015 && ((r >> 16) & 0x3) == 0) {
                    *value = (uint8)(*value + ((r & (1u << 18)) ? 1 : (uint8)-1));
                }
            }
        }
    }
#else
    (void)address;
    (void)value;
#endif
    return false;
}

void nes_glitch_apply_oam_write(uint8 oam_addr, uint8 *value)
{
#if NES_GLITCH_ENABLED
    uint32 r;
    uint8 sub;

    if (!g_nes_glitch.enabled || g_nes_glitch.sprite_threshold == 0 || value == 0) {
        return;
    }

    r = nes_glitch_rand();
    if ((r & 0x3FF) >= g_nes_glitch.sprite_threshold) {
        return;
    }

    sub = (uint8)(oam_addr & 0x03);
    if (sub == 0 || sub == 3) { /* Y or X */
        uint8 jitter_limit = nes_glitch_jitter_limit(g_nes_glitch.sprite_level);
        int8 delta = (int8)((r >> 26) & 0x3F) - 32; /* -32..+31 */
        if (sub == 0 && *value >= 0xEF) {
            return;
        }
        if (delta > (int8)jitter_limit) delta = (int8)jitter_limit;
        if (delta < -(int8)jitter_limit) delta = (int8)-jitter_limit;
        if (delta != 0) {
            *value = nes_glitch_add_clamped_u8(*value, delta);
        }
    } else if (sub == 1) { /* tile index */
        if (((r >> 10) & 0x3) == 0) {
            *value = (uint8)(*value + ((r & 1) ? 1 : (uint8)-1));
        }
    } else { /* attributes */
        if (((r >> 12) & 0x3) == 0) {
            static const uint8 bits[] = {7, 6, 5, 1, 0};
            *value ^= (uint8)(1u << bits[(r >> 14) % 5u]);
        }
    }
#else
    (void)oam_addr;
    (void)value;
#endif
}
