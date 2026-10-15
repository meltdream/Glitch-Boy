#pragma once

#include <stdint.h>

#ifndef MINIGB_APU_AUDIO_FORMAT_S16SYS
#define MINIGB_APU_AUDIO_FORMAT_S16SYS
#endif

#ifdef __cplusplus
extern "C" {
#endif
#include "walnut/minigb_apu.h"
#ifdef __cplusplus
}
#endif

void gb_apu_midi_enable(int enable);
int gb_apu_midi_is_enabled(void);

void gb_apu_midi_set_channel_map(int8_t square1, int8_t square2,
                                 int8_t wave, int8_t noise);
void gb_apu_midi_get_channel_map(int8_t* square1, int8_t* square2,
                                 int8_t* wave, int8_t* noise);

void gb_apu_midi_load_prefs(void);
void gb_apu_midi_save_prefs(void);

void gb_apu_midi_note_on(uint8_t chan, uint8_t note, uint8_t vel);
void gb_apu_midi_note_off(uint8_t chan, uint8_t note, uint8_t vel);
void gb_apu_midi_cc(uint8_t chan, uint8_t cc, uint8_t val);
void gb_apu_midi_pitch_bend(uint8_t chan, uint16_t bend14);
void gb_apu_midi_note_on_at(uint8_t chan, uint8_t note, uint8_t vel,
                            uint32_t timestamp_us);
void gb_apu_midi_note_off_at(uint8_t chan, uint8_t note, uint8_t vel,
                             uint32_t timestamp_us);
void gb_apu_midi_cc_at(uint8_t chan, uint8_t cc, uint8_t val,
                       uint32_t timestamp_us);
void gb_apu_midi_pitch_bend_at(uint8_t chan, uint16_t bend14,
                               uint32_t timestamp_us);
int gb_apu_midi_cc_is_sound_control(uint8_t cc);

void gb_apu_midi_set_duty_cc(int8_t cc_square1, int8_t cc_square2);
void gb_apu_midi_get_duty_cc(int8_t* cc_square1, int8_t* cc_square2);

void gb_apu_midi_set_noise_mode_cc(int8_t cc_noise);
void gb_apu_midi_get_noise_mode_cc(int8_t* cc_noise);

void gb_apu_midi_set_waveform_cc(int8_t cc_waveform);
void gb_apu_midi_get_waveform_cc(int8_t* cc_waveform);

void gb_apu_midi_set_env_cc(int8_t cc_mode, int8_t cc_decay,
                            int8_t cc_loop, int8_t cc_length);
void gb_apu_midi_get_env_cc(int8_t* cc_mode, int8_t* cc_decay,
                            int8_t* cc_loop, int8_t* cc_length);

void gb_apu_midi_set_pb_default_range(uint8_t semis);
uint8_t gb_apu_midi_get_pb_default_range(void);

void gb_apu_midi_render_samples(audio_sample_t* mono_out, uint16_t count);
void gb_apu_midi_render_samples_timed(audio_sample_t* mono_out,
                                      uint16_t count,
                                      uint32_t first_sample_playout_us);
void gb_apu_midi_frame_tick(audio_sample_t* mono_out);
