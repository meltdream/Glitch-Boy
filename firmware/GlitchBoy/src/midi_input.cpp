#include "midi_input.h"
#include "emu.h"
#include "glitch/glitch_controls.h"
#include "hid_server/hci_server.h"  // for sys_get_pref/sys_set_pref
#include "midi_visualizer.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include "nofrendo/nes_apu_midi.h"
#include "gb_apu_midi.h"

#ifdef ESP_PLATFORM
#include <Arduino.h>

#define MIDI_RX_PIN 27

// Defaults, overridden by saved prefs
static uint8_t MIDI_GLITCH_CC = 1;   // CC#1 controls Glitch 1 (PPU lane)
static uint8_t MIDI_GLITCH2_CC = 2;  // CC#2 controls Glitch 2 (CPU/APU lane)
static int8_t MIDI_VISUALIZER_SPRITE_CC = 76; // CC#76 selects visualizer sprite/shape
static int8_t MIDI_CHANNEL = 15;     // -1 = any channel, 0..15 = specific
static int8_t MIDI_FX_CHANNEL = -1;  // video effects (auto mode notes, glitch/sprite CCs); -1 = any
static uint8_t MIDI_AUTO_MODE = 2;   // 0=OFF, 1=CLOCK, 2=NOTE_ON
static const unsigned long MIDI_AUTO_IDLE_TIMEOUT_MS = 1500;
static uint8_t _clock_edge_count = 0;
static uint32_t _clock_pulse_count = 0;
static unsigned long _last_clock_ms = 0;
static unsigned long _last_auto_source_ms = 0;
static bool _clock_stopped = false;
static bool MIDI_INPUT_ENABLED = true;

struct midi_button_state {
    uint16_t current_state;
    uint16_t last_state;
};

static midi_button_state _midi_buttons = {0};
static uint8_t _running_status = 0;   // last channel status 0x80..0xEF
static uint8_t _data[2];
static uint8_t _data_index = 0;
static uint8_t _expected_len = 0;     // expected data bytes for current channel message
static bool _in_sysex = false;        // ignore bytes until 0xF7
static uint8_t _sys_skip = 0;         // number of system-common data bytes to skip
static bool _active_sensing = false;
static unsigned long _last_rx_ms = 0;
static bool _transport_was_active = false;

struct midi_rx_byte_t {
    uint32_t timestamp_us;
    uint8_t value;
};
static const uint16_t MIDI_RX_QUEUE_SIZE = 512;
static_assert((MIDI_RX_QUEUE_SIZE & (MIDI_RX_QUEUE_SIZE - 1u)) == 0,
              "MIDI_RX_QUEUE_SIZE must be a power of two");
static midi_rx_byte_t _rx_queue[MIDI_RX_QUEUE_SIZE] = {};
static volatile uint16_t _rx_queue_read = 0;
static volatile uint16_t _rx_queue_write = 0;
static volatile uint8_t _rx_queue_overflow = 0;

static uint8_t note_map[8] = {60,62,64,65,67,69,71,72};
static midi_channel_visual_state_t _midi_visual_channels[16] = {{0}};
static uint8_t _gameplay_held[16][16] = {{0}};
struct visual_note_entry_t {
    uint8_t note;
    uint8_t velocity;
};
static visual_note_entry_t _visual_note_stack[16][16] = {{{0}}};
static uint8_t _visual_note_count[16] = {0};
static const uint16_t button_map[8] = {
    GENERIC_UP,
    GENERIC_DOWN,
    GENERIC_LEFT,
    GENERIC_RIGHT,
    GENERIC_FIRE_A,
    GENERIC_FIRE_B,
    GENERIC_START,
    GENERIC_SELECT
};

static bool note_held_on_channel(uint8_t chan, uint8_t note)
{
    return (_gameplay_held[chan][note >> 3] & (1u << (note & 7))) != 0;
}

static void recompute_gameplay_buttons()
{
    uint16_t buttons = 0;
    for (int i = 0; i < 8; ++i) {
        bool held = false;
        if (MIDI_CHANNEL >= 0) {
            held = note_held_on_channel((uint8_t)MIDI_CHANNEL, note_map[i]);
        } else {
            for (uint8_t chan = 0; chan < 16 && !held; ++chan)
                held = note_held_on_channel(chan, note_map[i]);
        }
        if (held)
            buttons |= button_map[i];
    }
    _midi_buttons.current_state = buttons;
}

static void handle_note(uint8_t chan, uint8_t note, bool on)
{
    const uint8_t mask = (uint8_t)(1u << (note & 7));
    if (on)
        _gameplay_held[chan][note >> 3] |= mask;
    else
        _gameplay_held[chan][note >> 3] &= (uint8_t)~mask;
    recompute_gameplay_buttons();
}

static void clear_gameplay_channel(uint8_t chan)
{
    memset(_gameplay_held[chan], 0, sizeof(_gameplay_held[chan]));
    recompute_gameplay_buttons();
}

static void visualizer_note_event(uint8_t chan, uint8_t note, uint8_t vel, bool on)
{
    if (chan >= 16)
        return;
    midi_channel_visual_state_t* st = &_midi_visual_channels[chan];
    uint32_t now = millis();
    if (on) {
        uint8_t& count = _visual_note_count[chan];
        if (count >= 16) {
            memmove(&_visual_note_stack[chan][0], &_visual_note_stack[chan][1],
                    15 * sizeof(_visual_note_stack[chan][0]));
            count = 15;
        }
        _visual_note_stack[chan][count++] = {note, vel};
        st->active = 1;
        st->note = note;
        st->velocity = vel;
        st->last_note_on_ms = now;
    } else {
        uint8_t& count = _visual_note_count[chan];
        for (int i = (int)count - 1; i >= 0; --i) {
            if (_visual_note_stack[chan][i].note != note)
                continue;
            if (i + 1 < count) {
                memmove(&_visual_note_stack[chan][i],
                        &_visual_note_stack[chan][i + 1],
                        (count - (uint8_t)i - 1u) *
                            sizeof(_visual_note_stack[chan][0]));
            }
            count--;
            break;
        }
        if (count) {
            const visual_note_entry_t& top =
                _visual_note_stack[chan][count - 1u];
            st->active = 1;
            st->note = top.note;
            st->velocity = top.velocity;
        } else {
            st->active = 0;
        }
    }
    st->last_event_ms = now;
}

static void visualizer_pitch_bend(uint8_t chan, uint16_t bend14)
{
    if (chan >= 16)
        return;
    midi_channel_visual_state_t* st = &_midi_visual_channels[chan];
    st->pitch_bend = (int16_t)bend14 - 8192;
    st->last_event_ms = millis();
}

static void visualizer_all_notes_off(uint8_t chan)
{
    if (chan >= 16)
        return;
    midi_channel_visual_state_t* st = &_midi_visual_channels[chan];
    _visual_note_count[chan] = 0;
    st->active = 0;
    st->velocity = 0;
    st->last_event_ms = millis();
}

static inline bool fx_channel_match(uint8_t chan)
{
    return MIDI_FX_CHANNEL < 0 || chan == (uint8_t)MIDI_FX_CHANNEL;
}

static void handle_cc(uint8_t cc, uint8_t val) {
    if (cc == MIDI_GLITCH_CC) {
        glitch_controls_set_ppu_midi_value(val);
    } else if (cc == MIDI_GLITCH2_CC) {
        glitch_controls_set_corruption_midi_value(val);
    } else if (MIDI_VISUALIZER_SPRITE_CC >= 0 &&
               cc == (uint8_t)MIDI_VISUALIZER_SPRITE_CC) {
        midi_visualizer_set_sprite_control(val);
    }
}

bool midi_cc_is_sound_control(uint8_t cc)
{
    if (gb_apu_midi_cc_is_sound_control(cc))
        return true;
    if (nes_apu_midi_cc_is_sound_control(cc))
        return true;
    return false;
}

static uint8_t find_available_global_cc(uint8_t preferred,
                                        int avoid_a, int avoid_b)
{
    for (int offset = 0; offset < 128; ++offset) {
        const uint8_t candidate = (uint8_t)((preferred + offset) & 0x7F);
        if (candidate == avoid_a || candidate == avoid_b)
            continue;
        if (!midi_cc_is_sound_control(candidate))
            return candidate;
    }
    return preferred;
}

static void normalize_global_ccs()
{
    MIDI_GLITCH_CC = find_available_global_cc(MIDI_GLITCH_CC, -1, -1);
    MIDI_GLITCH2_CC =
        find_available_global_cc(MIDI_GLITCH2_CC, MIDI_GLITCH_CC, -1);
    if (MIDI_VISUALIZER_SPRITE_CC >= 0) {
        const uint8_t sprite = (uint8_t)MIDI_VISUALIZER_SPRITE_CC;
        if (sprite == MIDI_GLITCH_CC || sprite == MIDI_GLITCH2_CC ||
            midi_cc_is_sound_control(sprite)) {
            MIDI_VISUALIZER_SPRITE_CC = -1;
        }
    }
}

static void auto_mode_apply_step(unsigned long now)
{
    _last_auto_source_ms = now;
    glitch_controls_randomize_auto_mask();
    video_palette_glitch_cycle();
    midi_visualizer_auto_advance_scene();
}

static void auto_mode_update_idle_timeout(unsigned long now)
{
    if (MIDI_AUTO_MODE == 0 || _last_auto_source_ms == 0)
        return;
    if ((unsigned long)(now - _last_auto_source_ms) < MIDI_AUTO_IDLE_TIMEOUT_MS)
        return;
    if (glitch_controls_get_auto_ppu_mask() != 0)
        glitch_controls_clear_auto_mask();
}

static void reset_parser_state()
{
    _running_status = 0;
    _data_index = 0;
    _expected_len = 0;
    _in_sysex = false;
    _sys_skip = 0;
}

static void all_notes_off_everywhere(uint32_t timestamp_us)
{
    memset(_gameplay_held, 0, sizeof(_gameplay_held));
    recompute_gameplay_buttons();
    for (uint8_t chan = 0; chan < 16; ++chan) {
        visualizer_all_notes_off(chan);
        if (nes_apu_midi_is_enabled())
            nes_apu_midi_cc(chan, 120, 0);
        if (gb_apu_midi_is_enabled())
            gb_apu_midi_cc_at(chan, 120, 0, timestamp_us);
    }
}

static void active_sensing_timeout_check(unsigned long now)
{
    if (!_active_sensing || (unsigned long)(now - _last_rx_ms) <= 300)
        return;
    _active_sensing = false;
    reset_parser_state();
    all_notes_off_everywhere(micros());
}

static bool any_midi_consumer_enabled()
{
    if (MIDI_INPUT_ENABLED || gb_apu_midi_is_enabled())
        return true;
    if (nes_apu_midi_is_enabled())
        return true;
    return false;
}

static inline uint8_t midi_data_len_for_status(uint8_t status)
{
    uint8_t hi = status & 0xF0;
    if (status < 0xF0) {
        if (hi == 0xC0 || hi == 0xD0) return 1; // Program Change, Channel Pressure
        return 2; // Note Off/On, Poly AT, CC, Pitch Bend
    }
    // System Common payloads are skipped; real-time and SysEx are handled elsewhere.
    if (status == 0xF1 || status == 0xF3) return 1;
    if (status == 0xF2) return 2;
    return 0;
}

static void midi_parse(uint8_t b, uint32_t byte_time_us) {
    const unsigned long byte_time_ms = millis();
    if (_active_sensing)
        _last_rx_ms = byte_time_ms;

    // Handle real-time messages immediately; do not disturb running status
    if (b >= 0xF8) {
        unsigned long now = byte_time_ms;
        if (b == 0xF8) { // MIDI Clock (24 PPQN)
            _clock_pulse_count++;
            _last_clock_ms = now;
            if (MIDI_INPUT_ENABLED && MIDI_AUTO_MODE == 1) {
                _last_auto_source_ms = now;
                _clock_edge_count++;
                if (_clock_edge_count >= 24) { // once per beat
                    auto_mode_apply_step(now);
                    _clock_edge_count = 0;
                }
            }
        } else if (b == 0xFA) { // Start
            _clock_pulse_count = 0;
            _clock_edge_count = 0;
            _clock_stopped = false;
            _last_clock_ms = now;
        } else if (b == 0xFB) { // Continue
            _clock_stopped = false;
            _last_clock_ms = now;
        } else if (b == 0xFC) { // Stop
            _clock_stopped = true;
            _last_clock_ms = now;
        } else if (b == 0xFE) { // Active Sensing
            _active_sensing = true;
            _last_rx_ms = now;
        } else if (b == 0xFF) { // System Reset
            all_notes_off_everywhere(byte_time_us);
            for (uint8_t chan = 0; chan < 16; ++chan) {
                if (nes_apu_midi_is_enabled())
                    nes_apu_midi_cc(chan, 121, 0);
                if (gb_apu_midi_is_enabled())
                    gb_apu_midi_cc_at(chan, 121, 0, byte_time_us);
                visualizer_pitch_bend(chan, 8192);
            }
            _clock_pulse_count = 0;
            _clock_edge_count = 0;
            _clock_stopped = false;
            _last_clock_ms = 0;
            _active_sensing = false;
            reset_parser_state();
        }
        return;
    }

    // Consume system-common payload bytes. Realtime bytes do not count.
    if (_sys_skip && !(b & 0x80)) {
        _sys_skip--;
        return;
    }

    // SysEx streaming: ignore until EOX (0xF7)
    if (_in_sysex) {
        if (b == 0xF7) {
            _in_sysex = false;
            return;
        }
        if (!(b & 0x80))
            return;
        // Recover from malformed SysEx at the new status byte.
        _in_sysex = false;
    }

    if (b & 0x80) {
        // Status byte
        _sys_skip = 0;
        if (b == 0xF0) { // SysEx start
            _in_sysex = true;
            _running_status = 0;
            _data_index = 0;
            _expected_len = 0;
            return;
        }
        if (b == 0xF7) { // stray EOX is still System Common
            _running_status = 0;
            _data_index = 0;
            _expected_len = 0;
            return;
        }
        if (b >= 0xF0) {
            // System Common: cancel running status per MIDI spec
            _running_status = 0;
            _data_index = 0;
            _expected_len = 0;
            _sys_skip = midi_data_len_for_status(b);
            return;
        }
        // Channel status 0x80..0xEF
        _running_status = b;
        _data_index = 0;
        _expected_len = midi_data_len_for_status(b);
        return;
    }

    // Data byte for current running status
    if (!_running_status) {
        return; // data without status
    }

    _data[_data_index++] = b;
    if (_expected_len == 0) {
        // Reset malformed channel-message state.
        _data_index = 0;
        return;
    }
    if (_data_index < _expected_len)
        return; // wait for full message

    const uint8_t status = _running_status & 0xF0;
    const uint8_t chan = _running_status & 0x0F;
    const bool is_note = status == 0x90 || status == 0x80;
    const uint8_t note = _data[0];
    const uint8_t vel = _data[1];
    const bool on = status == 0x90 && vel > 0;
    const uint16_t bend14 = (uint16_t)_data[0] | ((uint16_t)_data[1] << 7);

    if (is_note) {
        if (MIDI_INPUT_ENABLED && on && MIDI_AUTO_MODE == 2 && fx_channel_match(chan)) {
            auto_mode_apply_step(millis());
        }
        if (any_midi_consumer_enabled())
            visualizer_note_event(chan, note, vel, on);
    } else if (status == 0xE0) {
        if (any_midi_consumer_enabled())
            visualizer_pitch_bend(chan, bend14);
    } else if (any_midi_consumer_enabled() && status == 0xB0) {
        const uint8_t cc = _data[0];
        if (cc == 120 || cc == 123 || (cc >= 124 && cc <= 127)) {
            visualizer_all_notes_off(chan);
            clear_gameplay_channel(chan);
        } else if (cc == 121) {
            visualizer_pitch_bend(chan, 8192);
        }
    }

    // APU MIDI is independent of the gameplay channel filter.
    if (nes_apu_midi_is_enabled()) {
        if (is_note) {
            if (on) nes_apu_midi_note_on(chan, note, vel);
            else    nes_apu_midi_note_off(chan, note, vel);
        } else if (status == 0xB0) {
            nes_apu_midi_cc(chan, _data[0], _data[1]);
        } else if (status == 0xE0) {
            nes_apu_midi_pitch_bend(chan, bend14);
        }
    }
    if (gb_apu_midi_is_enabled()) {
        if (is_note) {
            if (on)
                gb_apu_midi_note_on_at(chan, note, vel, byte_time_us);
            else
                gb_apu_midi_note_off_at(chan, note, vel, byte_time_us);
        } else if (status == 0xB0) {
            gb_apu_midi_cc_at(chan, _data[0], _data[1], byte_time_us);
        } else if (status == 0xE0) {
            gb_apu_midi_pitch_bend_at(chan, bend14, byte_time_us);
        }
    }

    // Active synth controls consume their CC before video mappings.
    if (MIDI_INPUT_ENABLED && status == 0xB0) {
        bool sound_claimed = false;
        if (nes_apu_midi_is_enabled() &&
            nes_apu_midi_cc_is_sound_control(_data[0])) {
            sound_claimed = true;
        }
        if (gb_apu_midi_is_enabled() &&
            gb_apu_midi_cc_is_sound_control(_data[0])) {
            sound_claimed = true;
        }
        if (!sound_claimed && fx_channel_match(chan))
            handle_cc(_data[0], _data[1]);
    }

    // Gameplay notes honour the channel filter.
    if (MIDI_INPUT_ENABLED && is_note &&
        !(MIDI_CHANNEL >= 0 && chan != (uint8_t)MIDI_CHANNEL)) {
        handle_note(chan, note, on);
    }

    _data_index = 0; // next message may reuse running status
}

void midi_input_uart_poll()
{
    int budget = 128;
    while (budget > 0) {
        int count = Serial1.available();
        if (count <= 0)
            break;
        if (count > budget)
            count = budget;
        const uint32_t newest_us = micros();
        const uint32_t oldest_us =
            newest_us - (uint32_t)(count - 1) * 320u;
        for (int i = 0; i < count; ++i) {
            const int raw = Serial1.read();
            if (raw < 0)
                return;
            const uint16_t write =
                __atomic_load_n(&_rx_queue_write, __ATOMIC_RELAXED);
            const uint16_t next =
                (uint16_t)((write + 1u) & (MIDI_RX_QUEUE_SIZE - 1u));
            const uint16_t read =
                __atomic_load_n(&_rx_queue_read, __ATOMIC_ACQUIRE);
            if (next == read) {
                __atomic_store_n(&_rx_queue_overflow, 1u,
                                 __ATOMIC_RELEASE);
                continue;
            }
            _rx_queue[write].value = (uint8_t)raw;
            _rx_queue[write].timestamp_us =
                oldest_us + (uint32_t)i * 320u;
            __atomic_store_n(&_rx_queue_write, next, __ATOMIC_RELEASE);
            --budget;
        }
    }
}

static void midi_poll() {
    active_sensing_timeout_check(millis());
    if (__atomic_exchange_n(&_rx_queue_overflow, 0u,
                            __ATOMIC_ACQ_REL)) {
        const uint16_t write =
            __atomic_load_n(&_rx_queue_write, __ATOMIC_ACQUIRE);
        __atomic_store_n(&_rx_queue_read, write, __ATOMIC_RELEASE);
        reset_parser_state();
        all_notes_off_everywhere(micros());
    }
    int budget = 256;
    while (budget-- > 0) {
        const uint16_t read =
            __atomic_load_n(&_rx_queue_read, __ATOMIC_RELAXED);
        const uint16_t write =
            __atomic_load_n(&_rx_queue_write, __ATOMIC_ACQUIRE);
        if (read == write)
            break;
        const midi_rx_byte_t item = _rx_queue[read];
        __atomic_store_n(
            &_rx_queue_read,
            (uint16_t)((read + 1u) & (MIDI_RX_QUEUE_SIZE - 1u)),
            __ATOMIC_RELEASE);
        midi_parse(item.value, item.timestamp_us);
    }
    active_sensing_timeout_check(millis());
}

static void discard_midi_input()
{
    const uint16_t write =
        __atomic_load_n(&_rx_queue_write, __ATOMIC_ACQUIRE);
    __atomic_store_n(&_rx_queue_read, write, __ATOMIC_RELEASE);
    __atomic_store_n(&_rx_queue_overflow, 0u, __ATOMIC_RELEASE);
    reset_parser_state();
    _active_sensing = false;
}

static bool parse_pref_int(const char* text, int* out)
{
    if (!text || !out)
        return false;
    errno = 0;
    char* end = 0;
    const long value = strtol(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' ||
        value < INT_MIN || value > INT_MAX)
        return false;
    *out = (int)value;
    return true;
}

static bool load_pref_int(const char* key, int min, int max, int* out)
{
    char buf[32];
    int v = 0;
    if (!sys_get_pref(key, buf, sizeof(buf) - 1) || !parse_pref_int(buf, &v) ||
        v < min || v > max)
        return false;
    *out = v;
    return true;
}

static void load_prefs() {
    int v;
    if (load_pref_int("midien", INT_MIN, INT_MAX, &v))
        MIDI_INPUT_ENABLED = (v != 0);
    if (load_pref_int("midich", -1, 15, &v))
        MIDI_CHANNEL = (int8_t)v;
    if (load_pref_int("midifxch", -1, 15, &v))
        MIDI_FX_CHANNEL = (int8_t)v;
    if (load_pref_int("midicc1", 0, 127, &v))
        MIDI_GLITCH_CC = (uint8_t)v;
    if (load_pref_int("midicc2", 0, 127, &v))
        MIDI_GLITCH2_CC = (uint8_t)v;
    if (load_pref_int("midiviscc", -1, 127, &v))
        MIDI_VISUALIZER_SPRITE_CC = (int8_t)v;
    if (load_pref_int("midiauto", 0, 2, &v))
        MIDI_AUTO_MODE = (uint8_t)v;
    for (int i = 0; i < 8; ++i) {
        char k[16];
        sprintf(k, "midin%d", i);
        if (load_pref_int(k, 0, 127, &v))
            note_map[i] = (uint8_t)v;
    }
}

void midi_input_init() {
    // Idle MIDI RX is high.
    pinMode(MIDI_RX_PIN, INPUT_PULLUP);
    load_prefs();
    nes_apu_midi_load_prefs();
    gb_apu_midi_load_prefs();
    normalize_global_ccs();
    // The UART stays open even when no MIDI consumer is active.
    Serial1.begin(31250, SERIAL_8N1, MIDI_RX_PIN, -1);
    _rx_queue_read = 0;
    _rx_queue_write = 0;
    _rx_queue_overflow = 0;
    reset_parser_state();
    _transport_was_active = MIDI_INPUT_ENABLED;
}

int get_hid_midi(uint8_t* dst) {
    auto_mode_update_idle_timeout(millis());
    if (!any_midi_consumer_enabled()) {
        discard_midi_input();
        _transport_was_active = false;
        return 0;
    }
    if (!_transport_was_active) {
        discard_midi_input();
        _transport_was_active = true;
    }
    midi_poll();
    if (_midi_buttons.current_state != _midi_buttons.last_state) {
        _midi_buttons.last_state = _midi_buttons.current_state;
        dst[0] = 0xA1;
        dst[1] = 0x42;
        dst[2] = _midi_buttons.current_state & 0xFF;
        dst[3] = _midi_buttons.current_state >> 8;
        dst[4] = 0;
        dst[5] = 0;
        return 6;
    }
    return 0;
}

void midi_set_channel(int8_t channel) {
    if (channel < -1) channel = -1;
    if (channel > 15) channel = 15;
    MIDI_CHANNEL = channel;
    recompute_gameplay_buttons();
}

void midi_set_enabled(bool enabled)
{
    if (MIDI_INPUT_ENABLED == enabled)
        return;
    MIDI_INPUT_ENABLED = enabled;
    if (!enabled) {
        all_notes_off_everywhere(micros());
        discard_midi_input();
        _transport_was_active = false;
    }
}

bool midi_is_enabled() { return MIDI_INPUT_ENABLED; }

void midi_set_fx_channel(int8_t channel) {
    if (channel < -1) channel = -1;
    if (channel > 15) channel = 15;
    MIDI_FX_CHANNEL = channel;
}

int8_t midi_get_fx_channel() { return MIDI_FX_CHANNEL; }

void midi_set_cc_numbers(uint8_t cc1, uint8_t cc2) {
    MIDI_GLITCH_CC = cc1 > 127 ? 127 : cc1;
    MIDI_GLITCH2_CC = cc2 > 127 ? 127 : cc2;
    normalize_global_ccs();
}

void midi_set_visualizer_sprite_cc(int8_t cc) {
    MIDI_VISUALIZER_SPRITE_CC = cc < -1 ? -1 : cc;
    normalize_global_ccs();
}

void midi_set_note_map(const uint8_t notes[8]) {
    for (int i = 0; i < 8; ++i)
        note_map[i] = notes[i] > 127 ? 127 : notes[i];
    recompute_gameplay_buttons();
}

int8_t midi_get_channel() { return MIDI_CHANNEL; }
int8_t midi_get_visualizer_sprite_cc() { return MIDI_VISUALIZER_SPRITE_CC; }

void midi_get_cc_numbers(uint8_t* cc1, uint8_t* cc2) {
    if (cc1) *cc1 = MIDI_GLITCH_CC;
    if (cc2) *cc2 = MIDI_GLITCH2_CC;
}

void midi_get_note_map(uint8_t notes_out[8]) {
    if (notes_out)
        memcpy(notes_out, note_map, sizeof(note_map));
}

void midi_save_prefs() {
    char b[8];
    sprintf(b, "%d", MIDI_INPUT_ENABLED ? 1 : 0);
    sys_set_pref("midien", b);
    sprintf(b, "%d", (int)MIDI_CHANNEL);
    sys_set_pref("midich", b);
    sprintf(b, "%d", (int)MIDI_FX_CHANNEL);
    sys_set_pref("midifxch", b);
    sprintf(b, "%d", (int)MIDI_GLITCH_CC);
    sys_set_pref("midicc1", b);
    sprintf(b, "%d", (int)MIDI_GLITCH2_CC);
    sys_set_pref("midicc2", b);
    sprintf(b, "%d", (int)MIDI_VISUALIZER_SPRITE_CC);
    sys_set_pref("midiviscc", b);
    sprintf(b, "%d", (int)MIDI_AUTO_MODE);
    sys_set_pref("midiauto", b);
    for (int i = 0; i < 8; ++i) {
        char k[16];
        sprintf(b, "%d", (int)note_map[i]);
        sprintf(k, "midin%d", i);
        sys_set_pref(k, b);
    }
}

void midi_set_auto_mode(uint8_t mode) {
    if (mode > 2) mode = 0;
    MIDI_AUTO_MODE = mode;
    _clock_edge_count = 0;
    _last_auto_source_ms = 0;
    if (mode == 0) {
        glitch_controls_clear_auto_mask();
    }
}

uint8_t midi_get_auto_mode() { return MIDI_AUTO_MODE; }

uint32_t midi_get_clock_pulse_count() { return _clock_pulse_count; }

bool midi_clock_is_active()
{
    return !_clock_stopped && _last_clock_ms != 0 &&
           (millis() - _last_clock_ms) < 750;
}

void midi_get_channel_visual_state(uint8_t channel, midi_channel_visual_state_t* out)
{
    if (!out)
        return;
    memset(out, 0, sizeof(*out));
    if (channel < 16)
        *out = _midi_visual_channels[channel];
}

#else

void midi_input_init() {}
void midi_input_uart_poll() {}
int get_hid_midi(uint8_t* dst) { (void)dst; return 0; }

void midi_set_enabled(bool enabled) { (void)enabled; }
bool midi_is_enabled() { return false; }
void midi_set_channel(int8_t channel) { (void)channel; }
void midi_set_fx_channel(int8_t channel) { (void)channel; }
int8_t midi_get_fx_channel() { return -1; }
void midi_set_cc_numbers(uint8_t cc1, uint8_t cc2) { (void)cc1; (void)cc2; }
void midi_set_visualizer_sprite_cc(int8_t cc) { (void)cc; }
void midi_set_note_map(const uint8_t notes[8]) { (void)notes; }
int8_t midi_get_channel() { return -1; }
void midi_get_cc_numbers(uint8_t* cc1, uint8_t* cc2) { if (cc1) *cc1 = 1; if (cc2) *cc2 = 2; }
int8_t midi_get_visualizer_sprite_cc() { return 76; }
void midi_get_note_map(uint8_t notes_out[8]) { if (!notes_out) return; uint8_t d[8]={60,62,64,65,67,69,71,72}; for(int i=0;i<8;++i) notes_out[i]=d[i]; }
bool midi_cc_is_sound_control(uint8_t cc) { (void)cc; return false; }
void midi_save_prefs() {}
void midi_get_channel_visual_state(uint8_t channel, midi_channel_visual_state_t* out) { (void)channel; if (out) memset(out, 0, sizeof(*out)); }
uint32_t midi_get_clock_pulse_count() { return 0; }
bool midi_clock_is_active() { return false; }

#endif // ESP_PLATFORM
