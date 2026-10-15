#ifndef NES_APU_MIDI_H
#define NES_APU_MIDI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* NES APU MIDI override mode. */
void nes_apu_midi_enable(int enable);
int  nes_apu_midi_is_enabled(void);

/* Map MIDI channels to APU voices. -1 disables a voice. */
void nes_apu_midi_set_channel_map(int8_t pulse1, int8_t pulse2, int8_t triangle, int8_t noise);
void nes_apu_midi_get_channel_map(int8_t* pulse1, int8_t* pulse2, int8_t* triangle, int8_t* noise);

/* Persistent mappings. Live enablement is owned by Gameplay/Music mode. */
void nes_apu_midi_load_prefs(void);
void nes_apu_midi_save_prefs(void);

/* Channelized MIDI events. */
void nes_apu_midi_note_on(uint8_t chan, uint8_t note, uint8_t vel);
void nes_apu_midi_note_off(uint8_t chan, uint8_t note, uint8_t vel);
void nes_apu_midi_cc(uint8_t chan, uint8_t cc, uint8_t val);
void nes_apu_midi_pitch_bend(uint8_t chan, uint16_t bend14); /* 0..16383, center 8192 */
int nes_apu_midi_cc_is_sound_control(uint8_t cc);

/* Pulse-duty CC mappings. -1 disables a mapping. */
void nes_apu_midi_set_duty_cc(int8_t cc_p1, int8_t cc_p2);
void nes_apu_midi_get_duty_cc(int8_t* cc_p1, int8_t* cc_p2);

/* Noise-mode CC mapping. -1 disables the mapping. */
void nes_apu_midi_set_noise_mode_cc(int8_t cc_noise);
void nes_apu_midi_get_noise_mode_cc(int8_t* cc_noise);

/* Envelope/length CC mappings. -1 disables an individual mapping. */
void nes_apu_midi_set_env_cc(int8_t cc_mode, int8_t cc_decay,
                             int8_t cc_loop, int8_t cc_length);
void nes_apu_midi_get_env_cc(int8_t* cc_mode, int8_t* cc_decay,
                             int8_t* cc_loop, int8_t* cc_length);

/* Default channel pitch-bend range in semitones. */
void    nes_apu_midi_set_pb_default_range(uint8_t semis);
uint8_t nes_apu_midi_get_pb_default_range(void);


#ifdef __cplusplus
}
#endif

#endif /* NES_APU_MIDI_H */
