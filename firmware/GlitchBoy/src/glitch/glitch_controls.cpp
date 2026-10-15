#include "glitch_controls.h"

struct glitch_controls_state {
    uint8_t ppu_level;
    uint8_t corruption_level;
    uint32_t last_update_ms;
    bool enabled;
};

#ifdef ESP_PLATFORM
#include "driver/adc.h"
#include "driver/gpio.h"
#include <esp_system.h>
#include "esp_adc_cal.h"
#include "Arduino.h"
#include "../emu.h"
#include "nes_glitch.h"
#include <esp_attr.h>

extern "C" {

#define AUDIO_ADC_CH ADC1_CHANNEL_3
#define AUDIO_SAMPLE_INTERVAL_MS 1
#define AUDIO_DC_SHIFT 7
#define AUDIO_ENV_SHIFT 3
#define AUDIO_MEAN_SHIFT 7
#define AUDIO_DEV_SHIFT 6
#define AUDIO_ENV_FLOOR_MULT 1
#define AUDIO_PEAK_DECAY_SHIFT 7
#define AUDIO_PEAK_MIN 256
#define AUDIO_MIN_ENV 64
#define AUDIO_NO_SIGNAL_P2P 120
#define AUDIO_NO_SIGNAL_WINDOW_MS 500
#define AUDIO_NO_SIGNAL_HOLD_MS 2000

static glitch_controls_state _controls_state = {0};
static uint16_t _ppu_primary_mask = 0x0000;   // progressive mask from knob A
static uint16_t _ppu_auto_mask = 0x0000;      // MIDI-driven
static uint8_t _auto_mask_last_level = 0;     // survives mask clears
static uint16_t _ppu_audio_mask = 0x0000;     // audio-reactive
static uint16_t _ppu_hold_mask = 0x0000;      // Y hold/repeat, always applied
static uint16_t _ppu_combined_mask = 0x0000;  // cached for the PPU hot path
static bool _glitch_latch_on = true;
static bool _x_button_momentary = true;
static bool _y_button_palette_mode = false;
static bool _latch_button_raw = false;
static bool _latch_button_state = false;
static uint32_t _latch_button_change_ms = 0;
static bool _y_button_raw = false;
static bool _y_button_state = false;
static uint32_t _y_button_change_ms = 0;
static uint32_t _y_hold_step_ms = 0;
static uint32_t _y_decay_step_ms = 0;
static uint8_t _y_last_level = 0;
static bool _virtual_x_latch_button = false;
static bool _virtual_y_hold_button = false;
static uint32_t _virtual_buttons_last_ms = 0;
static uint16_t _pot_adc_raw1 = 0;
static uint16_t _pot_adc_raw2 = 0;
static uint16_t _cv_adc_raw1 = 0;
static uint16_t _cv_adc_raw2 = 0;
static uint16_t _pot_index1 = 0;
static uint8_t _pot_corruption_level = 0;
static uint16_t _cv_index[GLITCH_CV_COUNT] = {0, 0};
static uint8_t _cv_corruption_level[GLITCH_CV_COUNT] = {0, 0};
static uint8_t _cv_assignment[GLITCH_CV_COUNT] = {
    GLITCH_CV_ASSIGN_DEFAULT_A,
    GLITCH_CV_ASSIGN_DEFAULT_B
};
static bool _cv_gate_pressed[GLITCH_CV_COUNT] = {false, false};
static uint16_t _cv_gate_buttons = 0;
static bool _cv_virtual_x_latch_button = false;
static bool _cv_virtual_y_hold_button = false;
static uint16_t _midi_index1 = 0;
static uint8_t _midi_corruption_level = 0;
static uint8_t _midi_value1 = 0;
static uint8_t _midi_value2 = 0;
static uint32_t _audio_last_sample_ms = 0;
static int32_t _audio_dc = 0;
static uint32_t _audio_env = 0;
static uint32_t _audio_mean = 0;
static uint32_t _audio_dev = 0;
static uint32_t _audio_peak = 0;
static uint8_t _audio_level = 0;
static uint32_t _audio_gate_window_start_ms = 0;
static uint32_t _audio_gate_min = 0xFFFFFFFFu;
static uint32_t _audio_gate_max = 0;
static uint32_t _audio_quiet_ms = 0;
static bool _audio_no_signal = false;
static uint8_t _audio_assignment = GLITCH_AUDIO_ASSIGN_DEFAULT;

#define PPU_COMBO_COUNT (1u << GLITCH_ADDRESS_LINE_COUNT)
#define PPU_COMBO_MAX_INDEX (PPU_COMBO_COUNT - 1u)
/* First active pot position is the single-bit A3 mask (0x0008). */
#define PPU_POT_START_MASK 0x0008u

static uint16_t _ppu_combo_masks[PPU_COMBO_COUNT];
static bool _ppu_combo_masks_ready = false;

static inline uint32_t uabs32(int32_t x) {
    return (uint32_t)(x < 0 ? -x : x);
}

static inline uint32_t smooth_u32(uint32_t current, uint32_t target, uint8_t shift) {
    int32_t diff = (int32_t)target - (int32_t)current;
    int32_t next = (int32_t)current + (diff >> shift);
    if (next < 0) {
        return 0;
    }
    return (uint32_t)next;
}

static void set_single_address_line_mask(uint8_t slot, uint16_t& out_mask);
static void set_ppu_combo_mask_from_index(uint16_t index, uint16_t& out_mask);
static void update_panel_buttons(uint32_t now_ms);
static inline void refresh_effect_levels();
static inline void refresh_ppu_combined_mask();
static inline bool glitch_latch_effective();
static inline uint16_t get_audio_ppu_mask();
static inline uint16_t combine_ppu_masks(uint16_t auto_mask);
static inline uint16_t adc_to_ppu_index(uint16_t raw);
static inline uint16_t adc_to_ppu_pot_index(uint16_t raw);
static inline uint8_t adc_to_corruption_level(uint16_t raw, uint16_t deadzone);
static inline uint16_t midi_to_ppu_index(uint8_t val);
static inline uint8_t ppu_mask_highest_bit(uint16_t mask);
static uint16_t ppu_find_combo_index(uint16_t mask);
static void init_ppu_combo_masks();
static inline uint8_t cv_assignment_clamp(uint8_t assignment);
static uint16_t effective_control_adc_raw(uint8_t assignment, uint16_t pot_raw,
                                         uint8_t midi_value);
static inline bool cv_assignment_is_button(uint8_t assignment);
static uint16_t cv_assignment_button_mask(uint8_t assignment);
static void refresh_cv_gate_buttons();
static void process_cv_channel(uint8_t cv, uint16_t raw, bool glitch_enabled);
static bool process_cv_controls(bool glitch_enabled);

static inline uint8_t max3_u8(uint8_t a, uint8_t b, uint8_t c) {
    uint8_t m = (a > b) ? a : b;
    return (m > c) ? m : c;
}

static inline uint16_t max3_u16(uint16_t a, uint16_t b, uint16_t c) {
    uint16_t m = (a > b) ? a : b;
    return (m > c) ? m : c;
}

static inline void refresh_effect_levels() {
    uint16_t index1 = max3_u16(_pot_index1, _cv_index[0], _midi_index1);
    if (_cv_index[1] > index1) {
        index1 = _cv_index[1];
    }
    uint8_t corruption_level = max3_u8(_pot_corruption_level, _cv_corruption_level[0], _midi_corruption_level);
    if (_cv_corruption_level[1] > corruption_level) {
        corruption_level = _cv_corruption_level[1];
    }
    if (_audio_assignment == GLITCH_AUDIO_ASSIGN_CORRUPTION && _audio_level > corruption_level) {
        corruption_level = _audio_level;
    }
    if (index1 > PPU_COMBO_MAX_INDEX) {
        index1 = PPU_COMBO_MAX_INDEX;
    }
    _controls_state.ppu_level = (uint8_t)((index1 * GLITCH_MAX_LEVEL + (PPU_COMBO_MAX_INDEX / 2u)) / PPU_COMBO_MAX_INDEX);
    _controls_state.corruption_level = corruption_level;
    set_ppu_combo_mask_from_index(index1, _ppu_primary_mask);
}

static void audio_update(uint32_t now_ms) {
    if (now_ms - _audio_last_sample_ms < AUDIO_SAMPLE_INTERVAL_MS) {
        return;
    }
    _audio_last_sample_ms = now_ms;

    int raw = adc1_get_raw(AUDIO_ADC_CH);
    if (raw < 0) {
        return;
    }
    if (_audio_gate_window_start_ms == 0) {
        _audio_gate_window_start_ms = now_ms;
    }
    if ((uint32_t)raw < _audio_gate_min) _audio_gate_min = (uint32_t)raw;
    if ((uint32_t)raw > _audio_gate_max) _audio_gate_max = (uint32_t)raw;
    if (now_ms - _audio_gate_window_start_ms >= AUDIO_NO_SIGNAL_WINDOW_MS) {
        uint32_t p2p = (_audio_gate_max >= _audio_gate_min) ? (_audio_gate_max - _audio_gate_min) : 0;
        if (p2p < AUDIO_NO_SIGNAL_P2P) {
            if (_audio_quiet_ms < AUDIO_NO_SIGNAL_HOLD_MS) {
                _audio_quiet_ms += AUDIO_NO_SIGNAL_WINDOW_MS;
            }
            if (_audio_quiet_ms >= AUDIO_NO_SIGNAL_HOLD_MS) {
                _audio_no_signal = true;
            }
        } else {
            _audio_quiet_ms = 0;
            _audio_no_signal = false;
        }
        _audio_gate_window_start_ms = now_ms;
        _audio_gate_min = 0xFFFFFFFFu;
        _audio_gate_max = 0;
    }

    // DC removal for biased audio input
    _audio_dc += (raw - _audio_dc) >> AUDIO_DC_SHIFT;
    int32_t x = raw - _audio_dc;

    // Envelope follower
    uint32_t absx = uabs32(x);
    _audio_env = smooth_u32(_audio_env, absx, AUDIO_ENV_SHIFT);

    // Adaptive threshold based on running mean + deviation
    _audio_mean = smooth_u32(_audio_mean, _audio_env, AUDIO_MEAN_SHIFT);
    uint32_t delta = (_audio_env > _audio_mean) ? (_audio_env - _audio_mean) : (_audio_mean - _audio_env);
    _audio_dev = smooth_u32(_audio_dev, delta, AUDIO_DEV_SHIFT);

    uint32_t floor = _audio_mean + (_audio_dev * AUDIO_ENV_FLOOR_MULT);
    if (floor < AUDIO_MIN_ENV) {
        floor = AUDIO_MIN_ENV;
    }

    if (_audio_env > _audio_peak) {
        _audio_peak = _audio_env;
    } else {
        _audio_peak -= (_audio_peak >> AUDIO_PEAK_DECAY_SHIFT);
    }

    uint32_t peak = _audio_peak;
    if (peak < floor + 1) {
        peak = floor + 1;
    }
    if (peak < AUDIO_PEAK_MIN) {
        peak = AUDIO_PEAK_MIN;
    }

    uint8_t slot = 0;
    if (!_audio_no_signal && _audio_env > floor) {
        uint32_t active_range = peak - floor;
        uint32_t adjusted = _audio_env - floor;
        if (adjusted > active_range) {
            adjusted = active_range;
        }
        slot = (uint8_t)((adjusted * (GLITCH_ADDRESS_LINE_COUNT - 1)) / active_range + 1);
        if (slot > GLITCH_ADDRESS_LINE_COUNT) {
            slot = GLITCH_ADDRESS_LINE_COUNT;
        }
    }

    if (slot != _audio_level) {
        _audio_level = slot;
        set_single_address_line_mask(slot, _ppu_audio_mask);
        if (_audio_assignment == GLITCH_AUDIO_ASSIGN_CORRUPTION) {
            refresh_effect_levels();
        }
    }
}

void glitch_controls_init() {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);  // GPIO 36
    adc1_config_channel_atten(ADC1_CHANNEL_3, ADC_ATTEN_DB_11);  // GPIO 39
    adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_11);  // GPIO 32
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);  // GPIO 34
    adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11);  // GPIO 35

    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << (gpio_num_t)GLITCH_X_PIN) |
                           (1ULL << (gpio_num_t)GLITCH_Y_PIN);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);

#if GLITCH_LATCH_LED_ENABLED
    gpio_config_t led_conf = {};
    led_conf.pin_bit_mask = (1ULL << (gpio_num_t)GLITCH_LATCH_LED_PIN);
    led_conf.mode = GPIO_MODE_OUTPUT;
    led_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    led_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    led_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&led_conf);
#endif

    uint32_t now_ms = millis();
    _glitch_latch_on = true;
    _latch_button_raw = !gpio_get_level((gpio_num_t)GLITCH_X_PIN);
    _latch_button_state = _latch_button_raw;
    _latch_button_change_ms = now_ms;
    _y_button_raw = !gpio_get_level((gpio_num_t)GLITCH_Y_PIN);
    _y_button_state = _y_button_raw;
    _y_button_change_ms = now_ms;
    _y_last_level = _y_button_state ? 1 : 0;
    set_single_address_line_mask(_y_last_level, _ppu_hold_mask);
    _y_hold_step_ms = _y_button_state ? now_ms : 0;
    _y_decay_step_ms = 0;
#if GLITCH_LATCH_LED_ENABLED
    gpio_set_level((gpio_num_t)GLITCH_LATCH_LED_PIN,
                   (glitch_latch_effective() || _y_button_state) ? 1 : 0);
#endif

    _controls_state.enabled = true;
    nes_glitch_set_enabled(true);
    _controls_state.ppu_level = 0;
    _controls_state.corruption_level = 0;
    _controls_state.last_update_ms = 0;
    _pot_index1 = 0;
    _pot_corruption_level = 0;
    for (uint8_t i = 0; i < GLITCH_CV_COUNT; ++i) {
        _cv_index[i] = 0;
        _cv_corruption_level[i] = 0;
        _cv_gate_pressed[i] = false;
    }
    _cv_gate_buttons = 0;
    _cv_virtual_x_latch_button = false;
    _cv_virtual_y_hold_button = false;
    _midi_index1 = 0;
    _midi_corruption_level = 0;
    _midi_value1 = 0;
    _midi_value2 = 0;
    init_ppu_combo_masks();
    _audio_gate_window_start_ms = 0;
    _audio_gate_min = 0xFFFFFFFFu;
    _audio_gate_max = 0;
    _audio_quiet_ms = 0;
    _audio_no_signal = false;
    refresh_effect_levels();
    refresh_ppu_combined_mask();

    printf("Analog glitch system initialized on GPIO 32/36 (pots), 34/35 (CV), 39 (audio)\n");

    int test_adc = adc1_get_raw(ADC1_CHANNEL_0);
    int test_adc_audio = adc1_get_raw(ADC1_CHANNEL_3);
    int test_adc2 = adc1_get_raw(ADC1_CHANNEL_4);
    int test_adc3 = adc1_get_raw(ADC1_CHANNEL_6);
    int test_adc4 = adc1_get_raw(ADC1_CHANNEL_7);
    printf("Initial ADC test read: CH0=%d CH3=%d CH4=%d CH6=%d CH7=%d\n",
           test_adc, test_adc_audio, test_adc2, test_adc3, test_adc4);
}

static void set_single_address_line_mask(uint8_t slot, uint16_t& out_mask) {
    // Slot 0 is off; slot N selects address line N-1.
    if (slot == 0) {
        out_mask = 0x0000;
    } else if (slot <= GLITCH_ADDRESS_LINE_COUNT) {
        out_mask = 1 << (slot - 1);
    } else {
        out_mask = 1 << (GLITCH_ADDRESS_LINE_COUNT - 1);
    }
    refresh_ppu_combined_mask();
}

static void init_ppu_combo_masks() {
    if (_ppu_combo_masks_ready) {
        return;
    }
    uint16_t idx = 0;
    _ppu_combo_masks[idx++] = 0;
    for (uint8_t highest_bit = 0; highest_bit < GLITCH_ADDRESS_LINE_COUNT; ++highest_bit) {
        for (uint8_t bits = 1; bits <= (highest_bit + 1u); ++bits) {
            for (uint16_t mask = 1; mask < PPU_COMBO_COUNT; ++mask) {
                if ((uint8_t)__builtin_popcount((unsigned int)mask) != bits) {
                    continue;
                }
                if (ppu_mask_highest_bit(mask) != highest_bit) {
                    continue;
                }
                _ppu_combo_masks[idx++] = mask;
            }
        }
    }
    for (; idx < PPU_COMBO_COUNT; ++idx) {
        _ppu_combo_masks[idx] = 0;
    }
    _ppu_combo_masks_ready = true;
}

static void set_ppu_combo_mask_from_index(uint16_t index, uint16_t& out_mask) {
    init_ppu_combo_masks();
    if (index > PPU_COMBO_MAX_INDEX) {
        index = PPU_COMBO_MAX_INDEX;
    }
    out_mask = _ppu_combo_masks[index];
    refresh_ppu_combined_mask();
}

static inline uint16_t adc_to_ppu_index(uint16_t raw) {
    if (raw < GLITCH_DEADZONE) {
        return 0;
    }
    uint32_t active_range = 4095u - GLITCH_DEADZONE;
    uint32_t adjusted = (uint32_t)raw - GLITCH_DEADZONE;
    uint32_t index = 1u + ((adjusted * PPU_COMBO_MAX_INDEX) / active_range);
    if (index > PPU_COMBO_MAX_INDEX) {
        index = PPU_COMBO_MAX_INDEX;
    }
    return (uint16_t)index;
}

static inline uint8_t adc_to_corruption_level(uint16_t raw, uint16_t deadzone) {
    if (raw < deadzone) {
        return 0;
    }
    const uint32_t active_range = 4095u - (uint32_t)deadzone;
    const uint32_t adjusted = (uint32_t)raw - (uint32_t)deadzone;
    uint8_t slot = (uint8_t)((adjusted * (GLITCH_MAX_LEVEL - 1u)) / active_range + 1u);
    if (slot > GLITCH_MAX_LEVEL) {
        slot = GLITCH_MAX_LEVEL;
    }
    return slot;
}

static inline uint8_t ppu_mask_highest_bit(uint16_t mask) {
    for (int8_t bit = GLITCH_ADDRESS_LINE_COUNT - 1; bit >= 0; --bit) {
        if (mask & (uint16_t)(1u << bit)) {
            return (uint8_t)bit;
        }
    }
    return GLITCH_ADDRESS_LINE_COUNT;
}

static inline uint16_t adc_to_ppu_pot_index(uint16_t raw) {
    if (raw < GLITCH_KNOB_DEADZONE) {
        return 0;
    }
    init_ppu_combo_masks();

    const uint32_t active_range = 4095u - GLITCH_KNOB_DEADZONE;
    const uint32_t adjusted = (uint32_t)raw - GLITCH_KNOB_DEADZONE;
    const uint32_t start_index = (uint32_t)ppu_find_combo_index(PPU_POT_START_MASK);
    uint32_t index = start_index;

    if (start_index < PPU_COMBO_MAX_INDEX) {
        const uint32_t remaining = (uint32_t)PPU_COMBO_MAX_INDEX - start_index;
        /* Cubic ease-in keeps the first half of the knob in mild masks. */
        const uint64_t eased_num = (uint64_t)adjusted * (uint64_t)adjusted * (uint64_t)adjusted;
        const uint64_t eased_den = (uint64_t)active_range * (uint64_t)active_range * (uint64_t)active_range;
        index = start_index + (uint32_t)((eased_num * remaining + (eased_den / 2u)) / eased_den);
    }

    if (index > PPU_COMBO_MAX_INDEX) {
        index = PPU_COMBO_MAX_INDEX;
    }
    return (uint16_t)index;
}

static inline uint16_t midi_to_ppu_index(uint8_t val) {
    if (val == 0) {
        return 0;
    }
    return (uint16_t)(((uint32_t)val * PPU_COMBO_MAX_INDEX) / 127u);
}

static inline uint8_t cv_assignment_clamp(uint8_t assignment) {
    if (assignment >= GLITCH_CV_ASSIGN_COUNT) {
        return GLITCH_CV_ASSIGN_PPU;
    }
    return assignment;
}

static uint16_t effective_control_adc_raw(uint8_t assignment, uint16_t pot_raw,
                                         uint8_t midi_value) {
    if (pot_raw > 4095u) {
        pot_raw = 4095u;
    }
    uint16_t intensity = (uint16_t)(4095u - pot_raw);

    for (uint8_t i = 0; i < GLITCH_CV_COUNT; ++i) {
        if (cv_assignment_clamp(_cv_assignment[i]) != assignment) {
            continue;
        }
        uint16_t cv_raw = (i == GLITCH_CV_A) ? _cv_adc_raw1 : _cv_adc_raw2;
        if (cv_raw > 4095u) {
            cv_raw = 4095u;
        }
        if (cv_raw > intensity) {
            intensity = cv_raw;
        }
    }

    const uint16_t midi_intensity =
        (uint16_t)(((uint32_t)midi_value * 4095u + 63u) / 127u);
    if (midi_intensity > intensity) {
        intensity = midi_intensity;
    }

    const uint8_t audio_assignment =
        (_audio_assignment == GLITCH_AUDIO_ASSIGN_CORRUPTION)
            ? GLITCH_CV_ASSIGN_CORRUPTION
            : GLITCH_CV_ASSIGN_PPU;
    if (assignment == audio_assignment && _audio_level > 0) {
        const uint16_t audio_intensity =
            (uint16_t)(((uint32_t)_audio_level * 4095u +
                        (GLITCH_ADDRESS_LINE_COUNT / 2u)) /
                       GLITCH_ADDRESS_LINE_COUNT);
        if (audio_intensity > intensity) {
            intensity = audio_intensity;
        }
    }

    return (uint16_t)(4095u - intensity);
}

static inline bool cv_assignment_is_button(uint8_t assignment) {
    return assignment >= GLITCH_CV_ASSIGN_BUTTON_LEFT &&
           assignment <= GLITCH_CV_ASSIGN_BUTTON_SELECT;
}

static uint16_t cv_assignment_button_mask(uint8_t assignment) {
    switch (assignment) {
        case GLITCH_CV_ASSIGN_BUTTON_LEFT:   return GENERIC_LEFT;
        case GLITCH_CV_ASSIGN_BUTTON_RIGHT:  return GENERIC_RIGHT;
        case GLITCH_CV_ASSIGN_BUTTON_UP:     return GENERIC_UP;
        case GLITCH_CV_ASSIGN_BUTTON_DOWN:   return GENERIC_DOWN;
        case GLITCH_CV_ASSIGN_BUTTON_A:      return GENERIC_FIRE_A;
        case GLITCH_CV_ASSIGN_BUTTON_B:      return GENERIC_FIRE_B;
        case GLITCH_CV_ASSIGN_BUTTON_X:      return GENERIC_FIRE_X;
        case GLITCH_CV_ASSIGN_BUTTON_Y:      return GENERIC_FIRE_Y;
        case GLITCH_CV_ASSIGN_BUTTON_START:  return GENERIC_START;
        case GLITCH_CV_ASSIGN_BUTTON_SELECT: return GENERIC_SELECT;
        default:                             return 0;
    }
}

static void refresh_cv_gate_buttons() {
    uint16_t buttons = 0;
    for (uint8_t i = 0; i < GLITCH_CV_COUNT; ++i) {
        const uint8_t assignment = cv_assignment_clamp(_cv_assignment[i]);
        if (_cv_gate_pressed[i] && cv_assignment_is_button(assignment)) {
            buttons |= cv_assignment_button_mask(assignment);
        }
    }
    _cv_gate_buttons = buttons;
    _cv_virtual_x_latch_button = (buttons & GENERIC_FIRE_X) != 0;
    _cv_virtual_y_hold_button = (buttons & GENERIC_FIRE_Y) != 0;
}

static void process_cv_channel(uint8_t cv, uint16_t raw, bool glitch_enabled) {
    if (cv >= GLITCH_CV_COUNT) {
        return;
    }

    const uint8_t assignment = cv_assignment_clamp(_cv_assignment[cv]);
    uint16_t new_index = 0;
    uint8_t new_corruption_level = 0;
    bool gate_changed = false;

    if (assignment == GLITCH_CV_ASSIGN_PPU) {
        if (glitch_enabled) {
            new_index = adc_to_ppu_index(raw);
        }
        if (_cv_gate_pressed[cv]) {
            _cv_gate_pressed[cv] = false;
            gate_changed = true;
        }
    } else if (assignment == GLITCH_CV_ASSIGN_CORRUPTION) {
        if (glitch_enabled) {
            new_corruption_level = adc_to_corruption_level(raw, GLITCH_DEADZONE);
        }
        if (_cv_gate_pressed[cv]) {
            _cv_gate_pressed[cv] = false;
            gate_changed = true;
        }
    } else if (assignment == GLITCH_CV_ASSIGN_PALETTE || cv_assignment_is_button(assignment)) {
        bool pressed = _cv_gate_pressed[cv];
        if (pressed) {
            if (raw < GLITCH_CV_GATE_OFF_THRESHOLD) {
                pressed = false;
            }
        } else if (raw >= GLITCH_CV_GATE_ON_THRESHOLD) {
            pressed = true;
        }
        if (pressed != _cv_gate_pressed[cv]) {
            _cv_gate_pressed[cv] = pressed;
            gate_changed = true;
            if (pressed && assignment == GLITCH_CV_ASSIGN_PALETTE) {
                video_palette_glitch_cycle();
            }
        }
    }

    const bool slot_changed = (new_index != _cv_index[cv]) || (new_corruption_level != _cv_corruption_level[cv]);
    if (slot_changed) {
        _cv_index[cv] = new_index;
        _cv_corruption_level[cv] = new_corruption_level;
        refresh_effect_levels();
    }
    if (gate_changed) {
        refresh_cv_gate_buttons();
    }
}

static bool process_cv_controls(bool glitch_enabled) {
    bool ok = false;
    int adc_raw3 = adc1_get_raw(ADC1_CHANNEL_6);
    if (adc_raw3 >= 0) {
        _cv_adc_raw1 = (uint16_t)adc_raw3;
        process_cv_channel(GLITCH_CV_A, (uint16_t)adc_raw3, glitch_enabled);
        ok = true;
    }

    int adc_raw4 = adc1_get_raw(ADC1_CHANNEL_7);
    if (adc_raw4 >= 0) {
        _cv_adc_raw2 = (uint16_t)adc_raw4;
        process_cv_channel(GLITCH_CV_B, (uint16_t)adc_raw4, glitch_enabled);
        ok = true;
    }
    return ok;
}

static uint16_t ppu_find_combo_index(uint16_t mask) {
    init_ppu_combo_masks();
    for (uint16_t i = 0; i < PPU_COMBO_COUNT; ++i) {
        if (_ppu_combo_masks[i] == mask) {
            return i;
        }
    }
    return 1u;
}

static void update_panel_buttons(uint32_t now_ms) {
    const bool virtual_buttons_fresh = (now_ms - _virtual_buttons_last_ms) <= 250;
    const bool virtual_latch = (virtual_buttons_fresh && _virtual_x_latch_button) ||
                               _cv_virtual_x_latch_button;
    const bool virtual_y = (virtual_buttons_fresh && _virtual_y_hold_button) ||
                           _cv_virtual_y_hold_button;

    bool latch_raw = !gpio_get_level((gpio_num_t)GLITCH_X_PIN);
    if (virtual_latch)
        latch_raw = true;
    if (latch_raw != _latch_button_raw) {
        _latch_button_raw = latch_raw;
        _latch_button_change_ms = now_ms;
    }
    if ((now_ms - _latch_button_change_ms) >= GLITCH_BUTTON_DEBOUNCE_MS &&
        _latch_button_state != _latch_button_raw) {
        _latch_button_state = _latch_button_raw;
        if (_x_button_momentary) {
            refresh_ppu_combined_mask();
        } else if (_latch_button_state) {
            _glitch_latch_on = !_glitch_latch_on;
            refresh_ppu_combined_mask();
        }
    }

    bool y_raw = !gpio_get_level((gpio_num_t)GLITCH_Y_PIN);
    if (virtual_y)
        y_raw = true;
    if (y_raw != _y_button_raw) {
        _y_button_raw = y_raw;
        _y_button_change_ms = now_ms;
    }
    if ((now_ms - _y_button_change_ms) >= GLITCH_BUTTON_DEBOUNCE_MS &&
        _y_button_state != _y_button_raw) {
        _y_button_state = _y_button_raw;
        if (_y_button_state) {
            if (_y_button_palette_mode) {
                video_palette_glitch_cycle();
            } else {
                if (_y_last_level == 0) {
                    _y_last_level = 1;
                } else if (_y_last_level < GLITCH_ADDRESS_LINE_COUNT) {
                    _y_last_level++;
                }
                set_single_address_line_mask(_y_last_level, _ppu_hold_mask);
                _y_hold_step_ms = now_ms;
                _y_decay_step_ms = 0;
            }
        } else {
            _y_hold_step_ms = 0;
            if (_y_last_level > 0) {
                _y_decay_step_ms = now_ms;
            }
        }
    }

    if (!_y_button_palette_mode && _y_button_state) {
        if (_y_last_level < GLITCH_ADDRESS_LINE_COUNT &&
            (now_ms - _y_hold_step_ms) >= GLITCH_Y_HOLD_STEP_MS) {
            _y_last_level++;
            set_single_address_line_mask(_y_last_level, _ppu_hold_mask);
            _y_hold_step_ms = now_ms;
        }
    } else if (_y_last_level > 0 && _y_decay_step_ms > 0 &&
               (now_ms - _y_decay_step_ms) >= GLITCH_Y_DECAY_STEP_MS) {
        _y_last_level--;
        set_single_address_line_mask(_y_last_level, _ppu_hold_mask);
        if (_y_last_level == 0) {
            _y_decay_step_ms = 0;
        } else {
            _y_decay_step_ms = now_ms;
        }
    }

#if GLITCH_LATCH_LED_ENABLED
    gpio_set_level((gpio_num_t)GLITCH_LATCH_LED_PIN,
                   (glitch_latch_effective() || _y_button_state || (_y_last_level > 0)) ? 1 : 0);
#endif
}

void glitch_controls_set_virtual_buttons(bool x_latch_pressed, bool y_hold_pressed) {
    _virtual_x_latch_button = x_latch_pressed;
    _virtual_y_hold_button = y_hold_pressed;
    _virtual_buttons_last_ms = millis();
}

void glitch_controls_set_x_button_momentary(bool momentary) {
    if (_x_button_momentary == momentary) {
        return;
    }
    _x_button_momentary = momentary;
    refresh_ppu_combined_mask();
}

bool glitch_controls_get_x_button_momentary() {
    return _x_button_momentary;
}

void glitch_controls_set_y_palette_mode(bool palette_mode) {
    if (_y_button_palette_mode == palette_mode) {
        return;
    }
    _y_button_palette_mode = palette_mode;
    if (palette_mode) {
        _y_last_level = 0;
        _y_hold_step_ms = 0;
        _y_decay_step_ms = 0;
        _ppu_hold_mask = 0x0000;
        refresh_ppu_combined_mask();
    }
}

bool glitch_controls_get_y_palette_mode() {
    return _y_button_palette_mode;
}

void glitch_controls_set_cv_assignment(uint8_t cv, uint8_t assignment) {
    if (cv >= GLITCH_CV_COUNT) {
        return;
    }
    assignment = cv_assignment_clamp(assignment);
    if (_cv_assignment[cv] == assignment) {
        return;
    }
    _cv_assignment[cv] = assignment;
    _cv_index[cv] = 0;
    _cv_corruption_level[cv] = 0;
    _cv_gate_pressed[cv] = false;
    refresh_cv_gate_buttons();
    refresh_effect_levels();
}

uint8_t glitch_controls_get_cv_assignment(uint8_t cv) {
    if (cv >= GLITCH_CV_COUNT) {
        return GLITCH_CV_ASSIGN_PPU;
    }
    return cv_assignment_clamp(_cv_assignment[cv]);
}

void glitch_controls_set_audio_assignment(uint8_t assignment) {
    if (assignment != GLITCH_AUDIO_ASSIGN_CORRUPTION) {
        assignment = GLITCH_AUDIO_ASSIGN_PPU;
    }
    if (_audio_assignment == assignment) {
        return;
    }
    _audio_assignment = assignment;
    refresh_effect_levels();
    refresh_ppu_combined_mask();
}

uint8_t glitch_controls_get_audio_assignment() {
    return _audio_assignment;
}

uint8_t glitch_controls_get_audio_level() {
    return _audio_level;
}

uint16_t glitch_controls_get_cv_gate_buttons() {
    return _cv_gate_buttons;
}

void glitch_controls_set_ppu_midi_value(uint8_t slot) {
    if (slot > 127) {
        slot = 127;
    }
    _midi_value1 = slot;
    _midi_index1 = midi_to_ppu_index(slot);
    refresh_effect_levels();
}

void glitch_controls_set_corruption_level(uint8_t slot) {
    if (slot > GLITCH_MAX_LEVEL) {
        slot = GLITCH_MAX_LEVEL;
    }
    _midi_corruption_level = slot;
    refresh_effect_levels();
}

void glitch_controls_set_corruption_midi_value(uint8_t value) {
    if (value > 127) {
        value = 127;
    }
    _midi_value2 = value;
    _midi_corruption_level = (uint8_t)(((uint16_t)value * GLITCH_MAX_LEVEL) / 127u);
    refresh_effect_levels();
}

void glitch_controls_update() {
    uint32_t current_ms = millis();
    const bool cv_gate_fast =
        cv_assignment_is_button(_cv_assignment[0]) ||
        cv_assignment_is_button(_cv_assignment[1]) ||
        _cv_assignment[0] == GLITCH_CV_ASSIGN_PALETTE ||
        _cv_assignment[1] == GLITCH_CV_ASSIGN_PALETTE;
    if (cv_gate_fast) {
        process_cv_controls(_controls_state.enabled);
    }
    update_panel_buttons(current_ms);
    if (!_controls_state.enabled) {
        return;
    }
    audio_update(current_ms);

    if (current_ms - _controls_state.last_update_ms < GLITCH_UPDATE_INTERVAL_MS) {
        return;
    }

    // Knob A (PPU lane). The pots are wired reversed, so invert.
    int adc_raw = adc1_get_raw(ADC1_CHANNEL_4);
    if (adc_raw < 0) {
        return;
    }
    _pot_adc_raw1 = (uint16_t)adc_raw;
    uint16_t new_index1 = adc_to_ppu_pot_index((uint16_t)(4095 - adc_raw));
    if (new_index1 != _pot_index1) {
        _pot_index1 = new_index1;
        refresh_effect_levels();
    }

    // Knob B (CPU/APU corruption lane).
    int adc_raw2 = adc1_get_raw(ADC1_CHANNEL_0);
    if (adc_raw2 >= 0) {
        _pot_adc_raw2 = (uint16_t)adc_raw2;
        uint8_t new_corruption_level = adc_to_corruption_level((uint16_t)(4095 - adc_raw2), GLITCH_KNOB_DEADZONE);
        if (new_corruption_level != _pot_corruption_level) {
            _pot_corruption_level = new_corruption_level;
            refresh_effect_levels();
        }
    }

    if (!cv_gate_fast) {
        process_cv_controls(true);
    }

    _controls_state.last_update_ms = current_ms;
}

uint8_t glitch_runtime_get_ppu_level() {
    return _controls_state.ppu_level;
}

uint16_t glitch_controls_get_pot1_adc_raw() {
    return _pot_adc_raw1;
}

uint16_t glitch_controls_get_pot2_adc_raw() {
    return _pot_adc_raw2;
}

uint16_t glitch_controls_get_cv1_adc_raw() {
    return _cv_adc_raw1;
}

uint16_t glitch_controls_get_cv2_adc_raw() {
    return _cv_adc_raw2;
}

uint16_t glitch_controls_get_effective_ppu_adc_raw() {
    return effective_control_adc_raw(GLITCH_CV_ASSIGN_PPU, _pot_adc_raw1,
                                    _midi_value1);
}

uint16_t glitch_controls_get_effective_corruption_adc_raw() {
    return effective_control_adc_raw(GLITCH_CV_ASSIGN_CORRUPTION, _pot_adc_raw2,
                                    _midi_value2);
}

uint16_t glitch_controls_get_primary_ppu_mask() {
    return _ppu_primary_mask;
}

uint16_t glitch_controls_get_auto_ppu_mask() {
    return _ppu_auto_mask;
}

uint16_t glitch_runtime_get_audio_ppu_mask() {
    return get_audio_ppu_mask();
}

uint16_t glitch_controls_get_hold_ppu_mask() {
    return _ppu_hold_mask;
}

uint16_t glitch_runtime_get_ppu_mask() {
    return _ppu_combined_mask;
}

uint16_t glitch_runtime_find_ppu_mask_index(uint16_t mask) {
    return ppu_find_combo_index(mask);
}

uint16_t glitch_runtime_get_ppu_mask_at(uint16_t index) {
    init_ppu_combo_masks();
    if (index > PPU_COMBO_MAX_INDEX) {
        index = PPU_COMBO_MAX_INDEX;
    }
    return _ppu_combo_masks[index];
}

bool glitch_controls_get_panel_x_pressed() {
    return _latch_button_state;
}

bool glitch_controls_get_panel_y_pressed() {
    return _y_button_state;
}

uint8_t glitch_runtime_get_corruption_level() {
    return _controls_state.corruption_level;
}

bool glitch_runtime_is_enabled() {
    return _controls_state.enabled;
}

void glitch_controls_set_enabled(bool enabled) {
    _controls_state.enabled = enabled;
    nes_glitch_set_enabled(enabled);
    if (!enabled) {
        _ppu_primary_mask = 0x0000;
        _ppu_auto_mask = 0x0000;
        _ppu_audio_mask = 0x0000;
        _ppu_hold_mask = 0x0000;
        _audio_level = 0;
        _pot_index1 = 0;
        _pot_corruption_level = 0;
        for (uint8_t i = 0; i < GLITCH_CV_COUNT; ++i) {
            _cv_index[i] = 0;
            _cv_corruption_level[i] = 0;
        }
        _midi_index1 = 0;
        _midi_corruption_level = 0;
        _midi_value1 = 0;
        _midi_value2 = 0;
        refresh_effect_levels();
        _y_last_level = 0;
        _y_hold_step_ms = 0;
        _y_decay_step_ms = 0;
    }
    refresh_ppu_combined_mask();
}

bool glitch_runtime_is_latch_active() {
    return glitch_latch_effective();
}

// Force the selected PPU address lines low.
uint16_t IRAM_ATTR glitch_runtime_apply_ppu_address_mask(uint16_t addr) {
    return addr & ~_ppu_combined_mask;
}

void glitch_controls_clear_auto_mask() {
    _ppu_auto_mask = 0x0000;
    refresh_ppu_combined_mask();
}

static inline bool glitch_latch_effective() {
    // Momentary X is a kill switch: held disables glitching, released enables it.
    return _x_button_momentary ? !_latch_button_state : _glitch_latch_on;
}

static inline uint16_t get_audio_ppu_mask() {
    return (_audio_assignment == GLITCH_AUDIO_ASSIGN_PPU) ? _ppu_audio_mask : 0x0000;
}

static inline uint16_t combine_ppu_masks(uint16_t auto_mask) {
    uint16_t combined = _ppu_hold_mask;
    if (glitch_latch_effective()) {
        combined |= _ppu_primary_mask | auto_mask | get_audio_ppu_mask();
    }
    if (!_controls_state.enabled) {
        combined = 0x0000;
    }
    return combined;
}

void glitch_controls_randomize_auto_mask() {
    // Never repeat the previous random slot, even after an inactivity clear.
    if (!glitch_runtime_is_enabled()) {
        _ppu_auto_mask = 0;
        refresh_ppu_combined_mask();
        return;
    }

    const uint16_t current_combined = combine_ppu_masks(_ppu_auto_mask);
    uint16_t fallback = 0;
    uint8_t fallback_slot = 0;
    const uint8_t start = (uint8_t)(esp_random() % GLITCH_ADDRESS_LINE_COUNT);

    for (uint8_t offset = 0; offset < GLITCH_ADDRESS_LINE_COUNT; ++offset) {
        const uint8_t pin = 1 + (uint8_t)((start + offset) % GLITCH_ADDRESS_LINE_COUNT);
        const uint16_t candidate = (uint16_t)(1u << (pin - 1));
        if (pin == _auto_mask_last_level) {
            continue;
        }
        if (combine_ppu_masks(candidate) != current_combined) {
            _ppu_auto_mask = candidate;
            _auto_mask_last_level = pin;
            refresh_ppu_combined_mask();
            return;
        }
        if (fallback == 0) {
            fallback = candidate;
            fallback_slot = pin;
        }
    }

    if (fallback != 0) {
        _ppu_auto_mask = fallback;
        _auto_mask_last_level = fallback_slot;
    }
    refresh_ppu_combined_mask();
}

static inline void refresh_ppu_combined_mask() {
    _ppu_combined_mask = combine_ppu_masks(_ppu_auto_mask);
}

} // extern "C"

#else

extern "C" {

static glitch_controls_state _controls_state = {0};

void glitch_controls_init() {
    _controls_state.enabled = false;
}

void glitch_controls_update() {}
void glitch_controls_set_ppu_midi_value(uint8_t slot) { (void)slot; }
void glitch_controls_set_corruption_level(uint8_t slot) { (void)slot; }
void glitch_controls_set_corruption_midi_value(uint8_t value) { (void)value; }
void glitch_controls_set_virtual_buttons(bool x_latch_pressed, bool y_hold_pressed) { (void)x_latch_pressed; (void)y_hold_pressed; }
void glitch_controls_set_x_button_momentary(bool momentary) { (void)momentary; }
bool glitch_controls_get_x_button_momentary() { return false; }
void glitch_controls_set_y_palette_mode(bool palette_mode) { (void)palette_mode; }
bool glitch_controls_get_y_palette_mode() { return false; }
void glitch_controls_set_cv_assignment(uint8_t cv, uint8_t assignment) { (void)cv; (void)assignment; }
uint8_t glitch_controls_get_cv_assignment(uint8_t cv) {
    if (cv == GLITCH_CV_B) {
        return GLITCH_CV_ASSIGN_DEFAULT_B;
    }
    return GLITCH_CV_ASSIGN_DEFAULT_A;
}
void glitch_controls_set_audio_assignment(uint8_t assignment) { (void)assignment; }
uint8_t glitch_controls_get_audio_assignment() {
    return GLITCH_AUDIO_ASSIGN_DEFAULT;
}
uint8_t glitch_controls_get_audio_level() {
    return 0;
}
uint16_t glitch_controls_get_cv_gate_buttons() {
    return 0;
}

uint16_t glitch_controls_get_pot1_adc_raw() {
    return 0;
}

uint16_t glitch_controls_get_pot2_adc_raw() {
    return 0;
}

uint16_t glitch_controls_get_cv1_adc_raw() {
    return 0;
}

uint16_t glitch_controls_get_cv2_adc_raw() {
    return 0;
}

uint16_t glitch_controls_get_effective_ppu_adc_raw() {
    return glitch_controls_get_pot1_adc_raw();
}

uint16_t glitch_controls_get_effective_corruption_adc_raw() {
    return glitch_controls_get_pot2_adc_raw();
}

uint16_t glitch_controls_get_primary_ppu_mask() {
    return 0;
}

uint16_t glitch_controls_get_auto_ppu_mask() {
    return 0;
}

uint16_t glitch_runtime_get_audio_ppu_mask() {
    return 0;
}

uint16_t glitch_controls_get_hold_ppu_mask() {
    return 0;
}

uint16_t glitch_runtime_get_ppu_mask() {
    return 0;
}

uint16_t glitch_runtime_find_ppu_mask_index(uint16_t mask) {
    (void)mask;
    return 0;
}

uint16_t glitch_runtime_get_ppu_mask_at(uint16_t index) {
    (void)index;
    return 0;
}

bool glitch_controls_get_panel_x_pressed() {
    return false;
}

bool glitch_controls_get_panel_y_pressed() {
    return false;
}

uint8_t glitch_runtime_get_ppu_level() {
    return 0;
}

uint8_t glitch_runtime_get_corruption_level() {
    return 0;
}

bool glitch_runtime_is_enabled() {
    return false;
}

void glitch_controls_set_enabled(bool enabled) {
    (void)enabled;
}

bool glitch_runtime_is_latch_active() {
    return false;
}

void glitch_controls_clear_auto_mask() {}
void glitch_controls_randomize_auto_mask() {}

uint16_t IRAM_ATTR glitch_runtime_apply_ppu_address_mask(uint16_t addr) {
    return addr;
}

} // extern "C"

#endif // ESP_PLATFORM
