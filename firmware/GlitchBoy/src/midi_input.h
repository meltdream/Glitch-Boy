#ifndef MIDI_INPUT_H
#define MIDI_INPUT_H

#include <stdint.h>

typedef struct {
    uint8_t active;
    uint8_t note;
    uint8_t velocity;
    int16_t pitch_bend;
    uint32_t last_event_ms;
    uint32_t last_note_on_ms;
} midi_channel_visual_state_t;

void midi_input_init();
void midi_input_uart_poll();
int get_hid_midi(uint8_t* dst);

// Channel: -1 = any, 0..15 = specific.
// Note map: 8 notes mapped to UP, DOWN, LEFT, RIGHT, A, B, START, SELECT.
void midi_set_enabled(bool enabled);
void midi_set_channel(int8_t channel);
void midi_set_fx_channel(int8_t channel);  // video effects channel, -1 = any
void midi_set_cc_numbers(uint8_t cc1, uint8_t cc2);
void midi_set_visualizer_sprite_cc(int8_t cc);
void midi_set_note_map(const uint8_t notes[8]);

bool midi_is_enabled();
int8_t midi_get_channel();
int8_t midi_get_fx_channel();
void midi_get_cc_numbers(uint8_t* cc1, uint8_t* cc2);
int8_t midi_get_visualizer_sprite_cc();
void midi_get_note_map(uint8_t notes_out[8]);
bool midi_cc_is_sound_control(uint8_t cc);

// Persist to NVS. Callers throttle this.
void midi_save_prefs();

// Auto mode: 0=OFF, 1=CLOCK, 2=NOTE_ON
void midi_set_auto_mode(uint8_t mode);
uint8_t midi_get_auto_mode();

// Latest monophonic note state on a channel, for the visualizer.
void midi_get_channel_visual_state(uint8_t channel, midi_channel_visual_state_t* out);

uint32_t midi_get_clock_pulse_count();
bool midi_clock_is_active();

#endif // MIDI_INPUT_H
