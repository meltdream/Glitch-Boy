#if defined(__GNUC__)
#pragma GCC optimize("O3")
#endif

#include "gb_glitch.h"
#include "glitch_runtime.h"
#include "glitch_settings.h"

#ifdef ESP_PLATFORM
#include <esp_attr.h>
#define GB_GLITCH_HOT_ATTR IRAM_ATTR
#else
#define GB_GLITCH_HOT_ATTR
#endif

#define GB_GLITCH_SPRITE_MAX_CHANCE  640u
#define GB_GLITCH_APU_MAX_CHANCE     860u
#define GB_GLITCH_DROP_MAX_CHANCE    180u
#define GB_GLITCH_JITTER_MAX         18
#define GB_GLITCH_OAM_SIZE           160u

static uint32_t g_gb_glitch_rng = 0x7F4A7C15u;
volatile uint16_t g_gb_glitch_ppu_mask = 0;
volatile uint16_t g_gb_glitch_bg_mask = 0;
static uint16_t g_gb_glitch_sprite_threshold = 0;
static uint16_t g_gb_glitch_apu_threshold = 0;
static uint16_t g_gb_glitch_apu_drop_chance = 0;

static inline uint32_t gb_glitch_rand(void)
{
    uint32_t x = g_gb_glitch_rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    if (x == 0) {
        x = 0x51ED270Bu;
    }
    g_gb_glitch_rng = x;
    return x;
}

static inline uint16_t gb_glitch_curve(uint8_t level, uint16_t max_chance)
{
    uint32_t num = (uint32_t)level * (uint32_t)level;
    uint32_t den = GLITCH_MAX_LEVEL * GLITCH_MAX_LEVEL;
    uint32_t chance = (num * (uint32_t)max_chance + (den / 2u)) / den;
    if (chance > 1023u) {
        chance = 1023u;
    }
    return (uint16_t)chance;
}

static inline uint8_t gb_glitch_add_clamped_u8(uint8_t value, int8_t delta)
{
    int out = (int)value + (int)delta;
    if (out < 0) {
        out = 0;
    }
    if (out > 255) {
        out = 255;
    }
    return (uint8_t)out;
}

static inline uint16_t gb_glitch_tune_bg_mask(uint16_t mask)
{
    /* GB uses a gentler quadratic progression for background masks. */
    const uint32_t max_index = (1u << GLITCH_ADDRESS_LINE_COUNT) - 1u;
    const uint32_t index = glitch_runtime_find_ppu_mask_index(mask);
    const uint32_t tuned_index = (index * index + (max_index / 2u)) / max_index;
    return glitch_runtime_get_ppu_mask_at((uint16_t)tuned_index);
}

void gb_glitch_sync_state(void)
{
    const bool enabled = glitch_runtime_is_enabled();
    const bool latch_on = glitch_runtime_is_latch_active();
    uint16_t sprite_threshold = 0;
    uint16_t apu_threshold = 0;
    uint16_t ppu_mask;
    uint16_t audio_mask;
    if (!enabled) {
        g_gb_glitch_ppu_mask = 0;
        g_gb_glitch_bg_mask = 0;
        g_gb_glitch_sprite_threshold = 0;
        g_gb_glitch_apu_threshold = 0;
        g_gb_glitch_apu_drop_chance = 0;
        return;
    }

    ppu_mask = glitch_runtime_get_ppu_mask();
    audio_mask = glitch_runtime_get_audio_ppu_mask();

    g_gb_glitch_ppu_mask = ppu_mask;
    g_gb_glitch_bg_mask = (ppu_mask & audio_mask)
        ? ppu_mask
        : gb_glitch_tune_bg_mask(ppu_mask);
    if (latch_on) {
        const uint8_t slot2 = glitch_runtime_get_corruption_level();
        sprite_threshold = gb_glitch_curve(slot2, GB_GLITCH_SPRITE_MAX_CHANCE);
        if (glitch_settings_is_apu_enabled()) {
            apu_threshold = gb_glitch_curve(slot2, GB_GLITCH_APU_MAX_CHANCE);
        }
    }
    g_gb_glitch_sprite_threshold = sprite_threshold;
    g_gb_glitch_apu_threshold = apu_threshold;
    g_gb_glitch_apu_drop_chance =
        (apu_threshold >> 2) > GB_GLITCH_DROP_MAX_CHANCE
            ? GB_GLITCH_DROP_MAX_CHANCE
            : (uint16_t)(apu_threshold >> 2);
}

static inline void gb_glitch_apply_oam_component(uint8_t sub, uint8_t *value,
                                                  uint16_t threshold)
{
    uint32_t r = gb_glitch_rand();
    if ((r & 0x3FFu) >= threshold) {
        return;
    }

    switch (sub) {
    case 0:
    case 1:
    {
        int8_t delta = (int8_t)(((r >> 26) & 0x3Fu) - 32);
        /* Hidden OAM entries stay hidden instead of drifting to screen edges. */
        if ((sub == 0 && (*value == 0 || *value >= 160u)) ||
            (sub == 1 && (*value == 0 || *value >= 168u))) {
            return;
        }
        if (delta > GB_GLITCH_JITTER_MAX) delta = GB_GLITCH_JITTER_MAX;
        if (delta < -GB_GLITCH_JITTER_MAX) delta = (int8_t)-GB_GLITCH_JITTER_MAX;
        if (delta != 0) {
            *value = gb_glitch_add_clamped_u8(*value, delta);
        }
        break;
    }

    case 2:
        if (((r >> 10) & 0x3u) == 0) {
            *value = (uint8_t)(*value + ((r & 1u) ? 1u : (uint8_t)-1));
        }
        break;

    default:
        if (((r >> 12) & 0x3u) == 0) {
            static const uint8_t attr_bits[] = {7, 6, 5, 4, 3};
            *value ^= (uint8_t)(1u << attr_bits[(r >> 14) % 5u]);
            if (((r >> 18) & 0x3u) == 0) {
                *value ^= (uint8_t)(1u << attr_bits[(r >> 21) % 5u]);
            }
        }
        break;
    }
}

void gb_glitch_apply_oam_write(uint16_t address, uint8_t *value)
{
    const uint16_t threshold = g_gb_glitch_sprite_threshold;
    if (threshold == 0 || value == 0) {
        return;
    }
    gb_glitch_apply_oam_component((uint8_t)(address & 0x03u), value, threshold);
}

void gb_glitch_apply_oam_dma(uint8_t *oam)
{
    const uint16_t threshold = g_gb_glitch_sprite_threshold;
    uint16_t i;

    if (threshold == 0 || oam == 0) {
        return;
    }

    for (i = 0; i < GB_GLITCH_OAM_SIZE; i += 4) {
        gb_glitch_apply_oam_component(0, oam + i, threshold);
        gb_glitch_apply_oam_component(1, oam + i + 1, threshold);
        gb_glitch_apply_oam_component(2, oam + i + 2, threshold);
        gb_glitch_apply_oam_component(3, oam + i + 3, threshold);
    }
}

bool GB_GLITCH_HOT_ATTR gb_glitch_apply_apu_write(uint16_t address, uint8_t *value)
{
    const uint16_t threshold = g_gb_glitch_apu_threshold;
    uint32_t r;

    if (value == 0 || threshold == 0) {
        return false;
    }

    r = gb_glitch_rand();
    if ((r & 0x3FFu) >= threshold) {
        return false;
    }

    if (address == 0xFF26u) {
        if (((r >> 10) & 0x7u) == 0) {
            *value ^= 0x80u;
        }
        return false;
    }

    if (((r >> 10) & 0x3FFu) < g_gb_glitch_apu_drop_chance) {
        return true;
    }

    {
        uint8_t mask = (uint8_t)(1u << ((r >> 2) % 6u));
        if (r & (1u << 12)) {
            mask |= (uint8_t)(1u << ((r >> 7) % 6u));
        }
        *value ^= mask;
    }

    if (address < 0xFF30u && ((r >> 16) & 0x3u) == 0) {
        *value = (uint8_t)(*value + ((r & (1u << 18)) ? 1u : (uint8_t)-1));
    }

    return false;
}
