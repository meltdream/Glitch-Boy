#include "gb_apu_midi.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
int sys_get_pref(const char* key, char* value, int max_len);
void sys_set_pref(const char* key, const char* value);
}

namespace {

constexpr uint16_t kGbNr10 = 0xFF10;
constexpr uint16_t kGbNr11 = 0xFF11;
constexpr uint16_t kGbNr12 = 0xFF12;
constexpr uint16_t kGbNr13 = 0xFF13;
constexpr uint16_t kGbNr14 = 0xFF14;
constexpr uint16_t kGbNr21 = 0xFF16;
constexpr uint16_t kGbNr22 = 0xFF17;
constexpr uint16_t kGbNr23 = 0xFF18;
constexpr uint16_t kGbNr24 = 0xFF19;
constexpr uint16_t kGbNr30 = 0xFF1A;
constexpr uint16_t kGbNr31 = 0xFF1B;
constexpr uint16_t kGbNr32 = 0xFF1C;
constexpr uint16_t kGbNr33 = 0xFF1D;
constexpr uint16_t kGbNr34 = 0xFF1E;
constexpr uint16_t kGbNr41 = 0xFF20;
constexpr uint16_t kGbNr42 = 0xFF21;
constexpr uint16_t kGbNr43 = 0xFF22;
constexpr uint16_t kGbNr44 = 0xFF23;
constexpr uint16_t kGbNr50 = 0xFF24;
constexpr uint16_t kGbNr51 = 0xFF25;
constexpr uint16_t kGbNr52 = 0xFF26;
constexpr uint16_t kGbWaveRam = 0xFF30;

constexpr uint8_t kGbVoiceSquare1 = 0;
constexpr uint8_t kGbVoiceSquare2 = 1;
constexpr uint8_t kGbVoiceWave = 2;
constexpr uint8_t kGbVoiceNoise = 3;
constexpr uint8_t kMidiChannelCount = 16;
constexpr uint8_t kGbVoiceCount = 4;
constexpr uint8_t kHeldNoteDepth = 16;
constexpr uint8_t kCcDirtyVolume = 0x01;
constexpr uint8_t kCcDirtyEnvelope = 0x02;
constexpr uint8_t kCcDirtyLength = 0x04;
constexpr uint8_t kCcDirtySquare1Duty = 0x08;
constexpr uint8_t kCcDirtySquare2Duty = 0x10;
constexpr uint8_t kCcDirtyNoiseMode = 0x20;
constexpr uint8_t kCcDirtyWaveform = 0x40;
constexpr uint16_t kMidiEventQueueSize = 256;
// Timestamp events against first-sample playout, including queued DAC audio.
constexpr uint32_t kMidiRenderLatencyUs = 28000u;
constexpr uint32_t kMidiRenderStallResetUs = 50000u;
static_assert((kMidiEventQueueSize & (kMidiEventQueueSize - 1u)) == 0,
              "MIDI event queue size must be a power of two");

enum midi_event_type_t : uint8_t {
    kMidiEventNoteOn,
    kMidiEventNoteOff,
    kMidiEventCc,
    kMidiEventPitchBend,
};

struct midi_event_t {
    uint32_t timestamp_us;
    uint16_t value;
    uint8_t type;
    uint8_t chan;
    uint8_t data;
};

struct midi_chan_state_t {
    int8_t current_note;
    uint8_t current_vel;
    uint8_t held_count;
    struct {
        uint8_t note;
        uint8_t vel;
    } held[kHeldNoteDepth];
    uint8_t cc_volume;
    uint8_t cc_expression;
    int16_t pitch_bend;
    uint8_t rpn_msb;
    uint8_t rpn_lsb;
    uint8_t pb_range_semi;
    uint8_t pb_range_cents;
    uint8_t env_mode;
    uint8_t env_decay;
    uint8_t env_loop;
    uint8_t length_value;
};

static minigb_apu_ctx s_apu = {};
static midi_chan_state_t s_chan[kMidiChannelCount] = {};
static int s_enabled = 0;
static int8_t s_map_square1 = 0;
static int8_t s_map_square2 = 1;
static int8_t s_map_wave = 2;
static int8_t s_map_noise = 9;
static int8_t s_cc_square1_duty = 74;
static int8_t s_cc_square2_duty = 71;
static int8_t s_cc_noise_mode = -1;
static int8_t s_cc_waveform = 73;
static int8_t s_cc_env_mode = 78;
static int8_t s_cc_env_decay = 75;
static int8_t s_cc_env_loop = 64;
static int8_t s_cc_length = 72;
static uint8_t s_square1_duty = 2;
static uint8_t s_square2_duty = 2;
static uint8_t s_pb_default_range = 2;
static uint8_t s_noise_short = 0;
static uint8_t s_waveform_index = 0;
static uint16_t s_square_note_regs[128] = {};
static uint16_t s_wave_note_regs[128] = {};
static uint8_t s_noise_note_regs[128] = {};
static bool s_note_tables_ready = false;
static midi_event_t s_event_queue[kMidiEventQueueSize] = {};
static uint16_t s_event_read = 0;
static uint16_t s_event_write = 0;
static uint32_t s_render_time_us = 0;
static uint32_t s_render_time_remainder = 0;
static bool s_render_time_ready = false;

static const uint8_t kWaveformPresets[][16] = {
    { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10 }, // saw
    { 0x13, 0x57, 0x9B, 0xDF, 0xFD, 0xB9, 0x75, 0x31,
      0x13, 0x57, 0x9B, 0xDF, 0xFD, 0xB9, 0x75, 0x31 }, // triangle
    { 0xFF, 0xFF, 0xFF, 0xFF, 0x88, 0x88, 0x88, 0x88,
      0x00, 0x00, 0x00, 0x00, 0x88, 0x88, 0x88, 0x88 }, // pulse
    { 0xF0, 0xF0, 0xE1, 0xD2, 0xC3, 0xB4, 0xA5, 0x96,
      0x87, 0x78, 0x69, 0x5A, 0x4B, 0x3C, 0x2D, 0x1E }, // bright saw
    { 0x8A, 0xCE, 0xFF, 0xEC, 0xA8, 0x64, 0x21, 0x00,
      0x12, 0x46, 0x8A, 0xCE, 0xFF, 0xDB, 0x86, 0x31 }, // sine-ish
    { 0xEE, 0xCC, 0xAA, 0x88, 0x66, 0x44, 0x22, 0x00,
      0x22, 0x44, 0x66, 0x88, 0xAA, 0xCC, 0xEE, 0xFF }, // organ
};

static uint8_t blended_volume_15(const midi_chan_state_t& state);
static uint8_t wave_volume_code(uint8_t vol15);
static void refresh_voice(uint8_t voice, int8_t mapped_chan);

static void reset_event_timeline()
{
    s_event_read = 0;
    s_event_write = 0;
    s_render_time_us = 0;
    s_render_time_remainder = 0;
    s_render_time_ready = false;
}

static void advance_render_time(uint16_t samples)
{
    const uint64_t scaled =
        (uint64_t)s_render_time_remainder +
        (uint64_t)samples * 1000000u;
    s_render_time_us += (uint32_t)(scaled / AUDIO_SAMPLE_RATE);
    s_render_time_remainder = (uint32_t)(scaled % AUDIO_SAMPLE_RATE);
}

static int clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value)
        return min_value;
    if (value > max_value)
        return max_value;
    return value;
}

static bool standard_sound_cc(uint8_t cc)
{
    return cc == 6 || cc == 7 || cc == 11 || cc == 38 ||
           (cc >= 96 && cc <= 101) || (cc >= 120 && cc <= 127);
}

static void normalize_custom_ccs()
{
    bool used[128] = {};
    used[6] = true;
    used[7] = true;
    used[11] = true;
    used[38] = true;
    for (uint8_t cc = 96; cc <= 101; ++cc)
        used[cc] = true;
    for (uint8_t cc = 120; cc <= 127; ++cc)
        used[cc] = true;

    int8_t* mappings[] = {
        &s_cc_env_mode,
        &s_cc_env_decay,
        &s_cc_env_loop,
        &s_cc_length,
        &s_cc_square1_duty,
        &s_cc_square2_duty,
        &s_cc_noise_mode,
        &s_cc_waveform,
    };
    for (int8_t* mapping : mappings) {
        if (*mapping < 0)
            continue;
        const uint8_t cc = (uint8_t)*mapping;
        if (used[cc]) {
            *mapping = -1;
            continue;
        }
        used[cc] = true;
    }
}

static uint8_t scale127_to_15(uint8_t value)
{
    return (uint8_t)((value * 15u + 63u) / 127u);
}

static void build_note_tables()
{
    if (s_note_tables_ready)
        return;

    double hz = 8.175798915643707;
    constexpr double kSemitoneRatio = 1.0594630943592953;
    static const uint8_t kNoiseDivLut[8] = {8, 16, 32, 48, 64, 80, 96, 112};

    for (int note = 0; note < 128; ++note) {
        int square_reg = 2048 - (int)((131072.0 / hz) + 0.5);
        int wave_reg = 2048 - (int)((65536.0 / hz) + 0.5);
        s_square_note_regs[note] =
            (uint16_t)clamp_int(square_reg, 0, 2047);
        s_wave_note_regs[note] =
            (uint16_t)clamp_int(wave_reg, 0, 2047);

        double best_error = 1.0e30;
        uint8_t best_reg = 0;
        for (uint8_t shift = 0; shift < 14; ++shift) {
            for (uint8_t div = 0; div < 8; ++div) {
                const double candidate =
                    4194304.0 /
                    ((double)kNoiseDivLut[div] * (double)(1u << shift));
                const double error =
                    (candidate >= hz) ? (candidate - hz) : (hz - candidate);
                if (error < best_error) {
                    best_error = error;
                    best_reg = (uint8_t)((shift << 4) | div);
                }
            }
        }
        s_noise_note_regs[note] = best_reg;
        hz *= kSemitoneRatio;
    }
    s_note_tables_ready = true;
}

static int32_t bent_note_q16(uint8_t note, int16_t pitch_bend,
                             uint8_t range_semi, uint8_t range_cents)
{
    const int32_t range_cents_total =
        (int32_t)range_semi * 100 + (int32_t)range_cents;
    const int64_t bend_q16 =
        ((int64_t)range_cents_total * (int64_t)pitch_bend * 65536) /
        (8192 * 100);
    return ((int32_t)note << 16) + (int32_t)bend_q16;
}

static uint16_t interpolate_note_reg(const uint16_t table[128],
                                     int32_t note_q16)
{
    if (note_q16 <= 0)
        return table[0];
    if (note_q16 >= (127 << 16))
        return table[127];

    const int index = note_q16 >> 16;
    const uint32_t fraction = (uint32_t)note_q16 & 0xFFFFu;
    const int32_t a = table[index];
    const int32_t b = table[index + 1];
    const int32_t interpolated =
        a + (int32_t)(((int64_t)(b - a) * fraction + 32768) >> 16);
    return (uint16_t)clamp_int(interpolated, 0, 2047);
}

static uint8_t voice_status_mask(uint8_t voice)
{
    return (uint8_t)(1u << voice);
}

static void set_voice_enabled(uint8_t voice, bool enabled)
{
    if (voice >= kGbVoiceCount)
        return;
    s_apu.chans[voice].enabled = enabled ? 1 : 0;
    if (!enabled)
        s_apu.chans[voice].volume = 0;

    uint8_t nr52 = s_apu.audio_mem[kGbNr52 - AUDIO_ADDR_COMPENSATION] & 0x80;
    for (uint8_t i = 0; i < kGbVoiceCount; ++i) {
        if (s_apu.chans[i].enabled)
            nr52 |= voice_status_mask(i);
    }
    s_apu.audio_mem[kGbNr52 - AUDIO_ADDR_COMPENSATION] = nr52;
}

static bool voice_matches(uint8_t midi_chan, int8_t mapped_chan)
{
    return mapped_chan >= 0 && mapped_chan < 16 && midi_chan == (uint8_t)mapped_chan;
}

static void init_midi_chan_state()
{
    for (uint8_t i = 0; i < kMidiChannelCount; ++i) {
        s_chan[i].current_note = -1;
        s_chan[i].current_vel = 0;
        s_chan[i].held_count = 0;
        s_chan[i].cc_volume = 127;
        s_chan[i].cc_expression = 127;
        s_chan[i].pitch_bend = 0;
        s_chan[i].rpn_msb = 127;
        s_chan[i].rpn_lsb = 127;
        s_chan[i].pb_range_semi = s_pb_default_range;
        s_chan[i].pb_range_cents = 0;
        s_chan[i].env_mode = 0;
        s_chan[i].env_decay = 4;
        s_chan[i].env_loop = 1;
        s_chan[i].length_value = 127;
    }
}

static void sync_current_note_from_stack(midi_chan_state_t& state)
{
    if (state.held_count == 0) {
        state.current_note = -1;
        state.current_vel = 0;
        return;
    }
    const uint8_t top = (uint8_t)(state.held_count - 1u);
    state.current_note = (int8_t)state.held[top].note;
    state.current_vel = state.held[top].vel;
}

static void push_held_note(midi_chan_state_t& state, uint8_t note, uint8_t vel)
{
    if (state.held_count >= kHeldNoteDepth) {
        memmove(&state.held[0], &state.held[1],
                (kHeldNoteDepth - 1u) * sizeof(state.held[0]));
        state.held_count = (uint8_t)(kHeldNoteDepth - 1u);
    }
    state.held[state.held_count].note = note;
    state.held[state.held_count].vel = vel;
    state.held_count++;
    sync_current_note_from_stack(state);
}

static bool remove_held_note(midi_chan_state_t& state, uint8_t note)
{
    for (int i = (int)state.held_count - 1; i >= 0; --i) {
        if (state.held[i].note != note)
            continue;
        if (i + 1 < state.held_count) {
            memmove(&state.held[i], &state.held[i + 1],
                    (state.held_count - (uint8_t)i - 1u) *
                        sizeof(state.held[0]));
        }
        state.held_count--;
        sync_current_note_from_stack(state);
        return true;
    }
    return false;
}

static void load_waveform_preset(uint8_t preset)
{
    const uint8_t preset_index =
        (uint8_t)(preset % (sizeof(kWaveformPresets) / sizeof(kWaveformPresets[0])));
    s_waveform_index = preset_index;

    for (uint8_t i = 0; i < sizeof(kWaveformPresets[0]); ++i)
        minigb_apu_audio_write(&s_apu, (uint16_t)(kGbWaveRam + i), kWaveformPresets[preset_index][i]);
}

static uint8_t length_load_from_cc(uint8_t voice, uint8_t length_value)
{
    const int len_max = (voice == kGbVoiceWave) ? 256 : 64;
    const int span = len_max - 1;
    const int scaled = (length_value * span + 63) / 127;
    return (uint8_t)(span - scaled);
}

static uint8_t square_env_reg(const midi_chan_state_t& state, uint8_t volume)
{
    if (!state.env_mode)
        return (uint8_t)(volume << 4);

    const uint8_t step = (uint8_t)clamp_int((int)state.env_decay, 0, 7);
    return (uint8_t)((volume << 4) | step);
}

static uint8_t noise_env_reg(const midi_chan_state_t& state, uint8_t volume)
{
    return square_env_reg(state, volume);
}

static uint8_t wave_volume_code_from_state(const midi_chan_state_t& state)
{
    const uint8_t vol15 = blended_volume_15(state);
    return wave_volume_code(vol15);
}

static void retrigger_looping_voice(uint8_t voice, int8_t mapped_chan)
{
    if (mapped_chan < 0 || mapped_chan >= 16)
        return;

    const midi_chan_state_t& state = s_chan[(uint8_t)mapped_chan];
    if (!state.env_mode || !state.env_loop || state.current_note < 0)
        return;
    if (voice == kGbVoiceWave)
        return;
    if (s_apu.chans[voice].volume != 0)
        return;

    refresh_voice(voice, mapped_chan);
}

static uint16_t square_reg_from_note(uint8_t note, int16_t pitch_bend,
                                     uint8_t range_semi, uint8_t range_cents)
{
    return interpolate_note_reg(
        s_square_note_regs,
        bent_note_q16(note, pitch_bend, range_semi, range_cents));
}

static uint16_t wave_reg_from_note(uint8_t note, int16_t pitch_bend,
                                   uint8_t range_semi, uint8_t range_cents)
{
    return interpolate_note_reg(
        s_wave_note_regs,
        bent_note_q16(note, pitch_bend, range_semi, range_cents));
}

static uint8_t noise_reg_from_note(uint8_t note)
{
    return (uint8_t)(s_noise_note_regs[note] |
                     (s_noise_short ? 0x08 : 0x00));
}

static uint8_t blended_volume_15(const midi_chan_state_t& state)
{
    const uint32_t denominator = 127u * 127u;
    uint32_t scaled127 =
        ((uint32_t)state.cc_volume *
         (uint32_t)state.cc_expression *
         (uint32_t)state.current_vel +
         (denominator / 2u)) /
        denominator;
    if (scaled127 > 127u)
        scaled127 = 127u;
    return scale127_to_15((uint8_t)scaled127);
}

static uint8_t wave_volume_code(uint8_t vol15)
{
    if (vol15 >= 11)
        return 1;
    if (vol15 >= 6)
        return 2;
    if (vol15 >= 1)
        return 3;
    return 0;
}

static void program_square_voice(uint8_t voice, const midi_chan_state_t& state, uint8_t duty)
{
    const uint16_t base = (voice == kGbVoiceSquare1) ? kGbNr11 : kGbNr21;
    const uint16_t env = (voice == kGbVoiceSquare1) ? kGbNr12 : kGbNr22;
    const uint16_t freq_lo = (voice == kGbVoiceSquare1) ? kGbNr13 : kGbNr23;
    const uint16_t freq_hi = (voice == kGbVoiceSquare1) ? kGbNr14 : kGbNr24;
    const uint8_t volume = blended_volume_15(state);
    const uint16_t freq = square_reg_from_note(
        (uint8_t)state.current_note, state.pitch_bend,
        state.pb_range_semi, state.pb_range_cents);
    const uint8_t len_load = length_load_from_cc(voice, state.length_value);
    const uint8_t len_flag = state.env_loop ? 0x00 : 0x40;

    if (voice == kGbVoiceSquare1)
        minigb_apu_audio_write(&s_apu, kGbNr10, 0x00);
    minigb_apu_audio_write(&s_apu, base, (uint8_t)(((duty & 0x03) << 6) | (len_load & 0x3F)));
    minigb_apu_audio_write(&s_apu, env, square_env_reg(state, volume));
    minigb_apu_audio_write(&s_apu, freq_lo, (uint8_t)(freq & 0xFF));
    minigb_apu_audio_write(&s_apu, freq_hi, (uint8_t)(0x80 | len_flag | ((freq >> 8) & 0x07)));
}

static void program_wave_voice(const midi_chan_state_t& state)
{
    const uint16_t freq = wave_reg_from_note(
        (uint8_t)state.current_note, state.pitch_bend,
        state.pb_range_semi, state.pb_range_cents);
    const uint8_t volume = wave_volume_code_from_state(state);
    const uint8_t len_load = length_load_from_cc(kGbVoiceWave, state.length_value);
    const uint8_t len_flag = state.env_loop ? 0x00 : 0x40;

    minigb_apu_audio_write(&s_apu, kGbNr30, 0x80);
    minigb_apu_audio_write(&s_apu, kGbNr31, len_load);
    minigb_apu_audio_write(&s_apu, kGbNr32, (uint8_t)(volume << 5));
    minigb_apu_audio_write(&s_apu, kGbNr33, (uint8_t)(freq & 0xFF));
    minigb_apu_audio_write(&s_apu, kGbNr34, (uint8_t)(0x80 | len_flag | ((freq >> 8) & 0x07)));
}

static void program_noise_voice(const midi_chan_state_t& state)
{
    const uint8_t volume = blended_volume_15(state);
    const uint8_t len_load = length_load_from_cc(kGbVoiceNoise, state.length_value);
    const uint8_t len_flag = state.env_loop ? 0x00 : 0x40;

    minigb_apu_audio_write(&s_apu, kGbNr41, len_load & 0x3F);
    minigb_apu_audio_write(&s_apu, kGbNr42, noise_env_reg(state, volume));
    minigb_apu_audio_write(&s_apu, kGbNr43, noise_reg_from_note((uint8_t)state.current_note));
    minigb_apu_audio_write(&s_apu, kGbNr44, (uint8_t)(0x80 | len_flag));
}

static uint8_t length_flag_from_state(const midi_chan_state_t& state)
{
    return state.env_loop ? 0x00 : 0x40;
}

static uint8_t current_freq_high(uint8_t voice)
{
    return (uint8_t)((s_apu.chans[voice].freq >> 8) & 0x07);
}

static void update_square_length_no_trigger(uint8_t voice, const midi_chan_state_t& state, uint8_t duty)
{
    const uint16_t base = (voice == kGbVoiceSquare1) ? kGbNr11 : kGbNr21;
    const uint16_t freq_hi = (voice == kGbVoiceSquare1) ? kGbNr14 : kGbNr24;
    const uint8_t len_load = length_load_from_cc(voice, state.length_value);

    minigb_apu_audio_write(&s_apu, base, (uint8_t)(((duty & 0x03) << 6) | (len_load & 0x3F)));
    minigb_apu_audio_write(&s_apu, freq_hi,
                            (uint8_t)(length_flag_from_state(state) | current_freq_high(voice)));
    minigb_apu_midi_reload_length(&s_apu, voice);
}

static void update_square_envelope_no_trigger(uint8_t voice, const midi_chan_state_t& state,
                                              bool update_current_volume)
{
    const uint8_t volume = blended_volume_15(state);
    minigb_apu_midi_write_envelope(
        &s_apu, voice, square_env_reg(state, volume),
        (uint8_t)(update_current_volume || !state.env_mode));
}

static void update_wave_length_no_trigger(const midi_chan_state_t& state)
{
    const uint8_t len_load = length_load_from_cc(kGbVoiceWave, state.length_value);

    minigb_apu_audio_write(&s_apu, kGbNr31, len_load);
    minigb_apu_audio_write(&s_apu, kGbNr34,
                            (uint8_t)(length_flag_from_state(state) | current_freq_high(kGbVoiceWave)));
    minigb_apu_midi_reload_length(&s_apu, kGbVoiceWave);
}

static void update_wave_volume_no_trigger(const midi_chan_state_t& state)
{
    minigb_apu_audio_write(&s_apu, kGbNr32, (uint8_t)(wave_volume_code_from_state(state) << 5));
}

static void update_noise_length_no_trigger(const midi_chan_state_t& state)
{
    const uint8_t len_load = length_load_from_cc(kGbVoiceNoise, state.length_value);

    minigb_apu_audio_write(&s_apu, kGbNr41, len_load & 0x3F);
    minigb_apu_audio_write(&s_apu, kGbNr44, length_flag_from_state(state));
    minigb_apu_midi_reload_length(&s_apu, kGbVoiceNoise);
}

static void update_noise_envelope_no_trigger(const midi_chan_state_t& state, bool update_current_volume)
{
    const uint8_t volume = blended_volume_15(state);
    minigb_apu_midi_write_envelope(
        &s_apu, kGbVoiceNoise, noise_env_reg(state, volume),
        (uint8_t)(update_current_volume || !state.env_mode));
}

static void update_noise_mode_no_trigger(const midi_chan_state_t& state)
{
    minigb_apu_audio_write(&s_apu, kGbNr43, noise_reg_from_note((uint8_t)state.current_note));
}

static void retune_square_no_trigger(uint8_t voice, const midi_chan_state_t& state)
{
    const uint16_t freq_lo = (voice == kGbVoiceSquare1) ? kGbNr13 : kGbNr23;
    const uint16_t freq_hi = (voice == kGbVoiceSquare1) ? kGbNr14 : kGbNr24;
    const uint16_t freq = square_reg_from_note(
        (uint8_t)state.current_note, state.pitch_bend,
        state.pb_range_semi, state.pb_range_cents);

    minigb_apu_audio_write(&s_apu, freq_lo, (uint8_t)(freq & 0xFF));
    minigb_apu_audio_write(&s_apu, freq_hi,
                            (uint8_t)(length_flag_from_state(state) | ((freq >> 8) & 0x07)));
}

static void retune_wave_no_trigger(const midi_chan_state_t& state)
{
    const uint16_t freq = wave_reg_from_note(
        (uint8_t)state.current_note, state.pitch_bend,
        state.pb_range_semi, state.pb_range_cents);

    minigb_apu_audio_write(&s_apu, kGbNr33, (uint8_t)(freq & 0xFF));
    minigb_apu_audio_write(&s_apu, kGbNr34,
                            (uint8_t)(length_flag_from_state(state) | ((freq >> 8) & 0x07)));
}

static const midi_chan_state_t* active_state_for_mapped_voice(int8_t mapped_chan)
{
    if (mapped_chan < 0 || mapped_chan >= 16)
        return 0;

    const midi_chan_state_t& state = s_chan[(uint8_t)mapped_chan];
    return (state.current_note >= 0) ? &state : 0;
}

static void apply_cc_update_to_voice(uint8_t voice, int8_t mapped_chan, uint8_t dirty)
{
    const midi_chan_state_t* state = active_state_for_mapped_voice(mapped_chan);
    if (!state)
        return;

    switch (voice) {
        case kGbVoiceSquare1:
            if (dirty & (kCcDirtyLength | kCcDirtySquare1Duty))
                update_square_length_no_trigger(voice, *state, s_square1_duty);
            if (dirty & (kCcDirtyVolume | kCcDirtyEnvelope))
                update_square_envelope_no_trigger(voice, *state, dirty & kCcDirtyVolume);
            break;
        case kGbVoiceSquare2:
            if (dirty & (kCcDirtyLength | kCcDirtySquare2Duty))
                update_square_length_no_trigger(voice, *state, s_square2_duty);
            if (dirty & (kCcDirtyVolume | kCcDirtyEnvelope))
                update_square_envelope_no_trigger(voice, *state, dirty & kCcDirtyVolume);
            break;
        case kGbVoiceWave:
            if (dirty & kCcDirtyLength)
                update_wave_length_no_trigger(*state);
            if (dirty & kCcDirtyVolume)
                update_wave_volume_no_trigger(*state);
            break;
        case kGbVoiceNoise:
            if (dirty & kCcDirtyLength)
                update_noise_length_no_trigger(*state);
            if (dirty & (kCcDirtyVolume | kCcDirtyEnvelope))
                update_noise_envelope_no_trigger(*state, dirty & kCcDirtyVolume);
            if (dirty & kCcDirtyNoiseMode)
                update_noise_mode_no_trigger(*state);
            break;
        default:
            break;
    }
}

static void retune_voice_no_trigger(uint8_t voice, int8_t mapped_chan)
{
    const midi_chan_state_t* state = active_state_for_mapped_voice(mapped_chan);
    if (!state)
        return;

    if (voice == kGbVoiceSquare1 || voice == kGbVoiceSquare2)
        retune_square_no_trigger(voice, *state);
    else if (voice == kGbVoiceWave)
        retune_wave_no_trigger(*state);
}

static void retune_channel_no_trigger(uint8_t chan)
{
    if (chan >= kMidiChannelCount || s_chan[chan].current_note < 0)
        return;
    if (voice_matches(chan, s_map_square1))
        retune_voice_no_trigger(kGbVoiceSquare1, s_map_square1);
    if (voice_matches(chan, s_map_square2))
        retune_voice_no_trigger(kGbVoiceSquare2, s_map_square2);
    if (voice_matches(chan, s_map_wave))
        retune_voice_no_trigger(kGbVoiceWave, s_map_wave);
}

static void refresh_voice(uint8_t voice, int8_t mapped_chan)
{
    if (mapped_chan < 0 || mapped_chan >= 16) {
        set_voice_enabled(voice, false);
        return;
    }

    const midi_chan_state_t& state = s_chan[(uint8_t)mapped_chan];
    if (state.current_note < 0) {
        set_voice_enabled(voice, false);
        return;
    }

    switch (voice) {
        case kGbVoiceSquare1:
            program_square_voice(voice, state, s_square1_duty);
            break;
        case kGbVoiceSquare2:
            program_square_voice(voice, state, s_square2_duty);
            break;
        case kGbVoiceWave:
            program_wave_voice(state);
            break;
        case kGbVoiceNoise:
            program_noise_voice(state);
            break;
        default:
            break;
    }
}

static void refresh_all_voices()
{
    refresh_voice(kGbVoiceSquare1, s_map_square1);
    refresh_voice(kGbVoiceSquare2, s_map_square2);
    refresh_voice(kGbVoiceWave, s_map_wave);
    refresh_voice(kGbVoiceNoise, s_map_noise);
}

static void apply_enabled_state()
{
    build_note_tables();
    minigb_apu_audio_init(&s_apu);
    minigb_apu_set_midi_bandlimit(&s_apu, 1);
    load_waveform_preset(s_waveform_index);
    minigb_apu_audio_write(&s_apu, kGbNr50, 0x77);
    minigb_apu_audio_write(&s_apu, kGbNr51, 0xFF);
    init_midi_chan_state();
    s_square1_duty = 2;
    s_square2_duty = 2;
    s_noise_short = 0;
}

static void save_pref_int(const char* key, int value)
{
    char buf[12];
    snprintf(buf, sizeof(buf), "%d", value);
    sys_set_pref(key, buf);
}

static bool load_pref_int(const char* key, int* out)
{
    if (!out)
        return false;
    char buf[16] = {};
    if (!sys_get_pref(key, buf, sizeof(buf) - 1))
        return false;
    errno = 0;
    char* end = 0;
    const long parsed = strtol(buf, &end, 10);
    if (errno == ERANGE || end == buf || *end != '\0' ||
        parsed < INT_MIN || parsed > INT_MAX)
        return false;
    *out = (int)parsed;
    return true;
}

} // namespace

static void apply_note_on(uint8_t chan, uint8_t note, uint8_t vel);
static void apply_note_off(uint8_t chan, uint8_t note, uint8_t vel);
static void apply_cc(uint8_t chan, uint8_t cc, uint8_t val);
static void apply_pitch_bend(uint8_t chan, uint16_t bend14);

static void apply_event(const midi_event_t& event)
{
    switch ((midi_event_type_t)event.type) {
        case kMidiEventNoteOn:
            apply_note_on(event.chan, event.data, (uint8_t)event.value);
            break;
        case kMidiEventNoteOff:
            apply_note_off(event.chan, event.data, (uint8_t)event.value);
            break;
        case kMidiEventCc:
            apply_cc(event.chan, event.data, (uint8_t)event.value);
            break;
        case kMidiEventPitchBend:
            apply_pitch_bend(event.chan, event.value);
            break;
    }
}

static void apply_all_queued_events()
{
    while (s_event_read != s_event_write) {
        apply_event(s_event_queue[s_event_read]);
        s_event_read =
            (uint16_t)((s_event_read + 1u) & (kMidiEventQueueSize - 1u));
    }
}

static void queue_event(midi_event_type_t type, uint8_t chan, uint8_t data,
                        uint16_t value, uint32_t timestamp_us)
{
    uint16_t next =
        (uint16_t)((s_event_write + 1u) & (kMidiEventQueueSize - 1u));
    if (next == s_event_read) {
        // Fold a full queue into current state so Note Off is not lost.
        apply_all_queued_events();
        s_render_time_ready = false;
        next = (uint16_t)((s_event_write + 1u) &
                          (kMidiEventQueueSize - 1u));
    }

    midi_event_t& event = s_event_queue[s_event_write];
    event.timestamp_us = timestamp_us;
    event.value = value;
    event.type = (uint8_t)type;
    event.chan = chan;
    event.data = data;
    s_event_write = next;
}

void gb_apu_midi_enable(int enable)
{
    const int next = enable ? 1 : 0;
    if (s_enabled == next)
        return;
    reset_event_timeline();
    s_enabled = next;
    if (s_enabled) {
        apply_enabled_state();
    } else {
        init_midi_chan_state();
    }
}

int gb_apu_midi_is_enabled(void)
{
    return s_enabled;
}

void gb_apu_midi_set_channel_map(int8_t square1, int8_t square2,
                                 int8_t wave, int8_t noise)
{
    s_map_square1 = (int8_t)clamp_int(square1, -1, 15);
    s_map_square2 = (int8_t)clamp_int(square2, -1, 15);
    s_map_wave = (int8_t)clamp_int(wave, -1, 15);
    s_map_noise = (int8_t)clamp_int(noise, -1, 15);
    if (s_enabled)
        refresh_all_voices();
}

void gb_apu_midi_get_channel_map(int8_t* square1, int8_t* square2,
                                 int8_t* wave, int8_t* noise)
{
    if (square1)
        *square1 = s_map_square1;
    if (square2)
        *square2 = s_map_square2;
    if (wave)
        *wave = s_map_wave;
    if (noise)
        *noise = s_map_noise;
}

void gb_apu_midi_load_prefs(void)
{
    int value = 0;
    if (load_pref_int("gbmid_sq1", &value))
        s_map_square1 = (int8_t)clamp_int(value, -1, 15);
    if (load_pref_int("gbmid_sq2", &value))
        s_map_square2 = (int8_t)clamp_int(value, -1, 15);
    if (load_pref_int("gbmid_wav", &value))
        s_map_wave = (int8_t)clamp_int(value, -1, 15);
    if (load_pref_int("gbmid_noi", &value))
        s_map_noise = (int8_t)clamp_int(value, -1, 15);
    if (load_pref_int("gbmid_d1", &value))
        s_cc_square1_duty = (int8_t)clamp_int(value, -1, 127);
    if (load_pref_int("gbmid_d2", &value))
        s_cc_square2_duty = (int8_t)clamp_int(value, -1, 127);
    if (load_pref_int("gbmid_pbr", &value))
        s_pb_default_range = (uint8_t)clamp_int(value, 0, 24);
    if (load_pref_int("gbmid_ncc", &value))
        s_cc_noise_mode = (int8_t)clamp_int(value, -1, 127);
    if (load_pref_int("gbmid_wcc", &value))
        s_cc_waveform = (int8_t)clamp_int(value, -1, 127);
    if (load_pref_int("gbmid_em", &value))
        s_cc_env_mode = (int8_t)clamp_int(value, -1, 127);
    if (load_pref_int("gbmid_ed", &value))
        s_cc_env_decay = (int8_t)clamp_int(value, -1, 127);
    if (load_pref_int("gbmid_el", &value))
        s_cc_env_loop = (int8_t)clamp_int(value, -1, 127);
    if (load_pref_int("gbmid_len", &value))
        s_cc_length = (int8_t)clamp_int(value, -1, 127);
    normalize_custom_ccs();
}

void gb_apu_midi_save_prefs(void)
{
    save_pref_int("gbmid_sq1", s_map_square1);
    save_pref_int("gbmid_sq2", s_map_square2);
    save_pref_int("gbmid_wav", s_map_wave);
    save_pref_int("gbmid_noi", s_map_noise);
    save_pref_int("gbmid_d1", s_cc_square1_duty);
    save_pref_int("gbmid_d2", s_cc_square2_duty);
    save_pref_int("gbmid_pbr", s_pb_default_range);
    save_pref_int("gbmid_ncc", s_cc_noise_mode);
    save_pref_int("gbmid_wcc", s_cc_waveform);
    save_pref_int("gbmid_em", s_cc_env_mode);
    save_pref_int("gbmid_ed", s_cc_env_decay);
    save_pref_int("gbmid_el", s_cc_env_loop);
    save_pref_int("gbmid_len", s_cc_length);
}

void gb_apu_midi_note_on(uint8_t chan, uint8_t note, uint8_t vel)
{
    gb_apu_midi_note_on_at(chan, note, vel, 0);
}

void gb_apu_midi_note_on_at(uint8_t chan, uint8_t note, uint8_t vel,
                            uint32_t timestamp_us)
{
    if (!s_enabled || chan >= kMidiChannelCount)
        return;
    if (timestamp_us != 0) {
        queue_event(vel == 0 ? kMidiEventNoteOff : kMidiEventNoteOn,
                    chan, note, vel, timestamp_us);
        return;
    }
    apply_note_on(chan, note, vel);
}

static void apply_note_on(uint8_t chan, uint8_t note, uint8_t vel)
{
    if (vel == 0) {
        apply_note_off(chan, note, 0);
        return;
    }

    push_held_note(s_chan[chan], note, vel);

    if (voice_matches(chan, s_map_square1))
        refresh_voice(kGbVoiceSquare1, s_map_square1);
    if (voice_matches(chan, s_map_square2))
        refresh_voice(kGbVoiceSquare2, s_map_square2);
    if (voice_matches(chan, s_map_wave))
        refresh_voice(kGbVoiceWave, s_map_wave);
    if (voice_matches(chan, s_map_noise))
        refresh_voice(kGbVoiceNoise, s_map_noise);
}

void gb_apu_midi_note_off(uint8_t chan, uint8_t note, uint8_t vel)
{
    gb_apu_midi_note_off_at(chan, note, vel, 0);
}

void gb_apu_midi_note_off_at(uint8_t chan, uint8_t note, uint8_t vel,
                             uint32_t timestamp_us)
{
    if (!s_enabled || chan >= kMidiChannelCount)
        return;
    if (timestamp_us != 0) {
        queue_event(kMidiEventNoteOff, chan, note, vel, timestamp_us);
        return;
    }
    apply_note_off(chan, note, vel);
}

static void apply_note_off(uint8_t chan, uint8_t note, uint8_t vel)
{
    (void)vel;
    midi_chan_state_t& state = s_chan[chan];
    const int8_t previous_note = state.current_note;
    const uint8_t previous_vel = state.current_vel;
    if (!remove_held_note(state, note))
        return;

    if (state.current_note == previous_note) {
        if (state.current_vel != previous_vel && state.current_note >= 0) {
            if (voice_matches(chan, s_map_square1))
                apply_cc_update_to_voice(kGbVoiceSquare1, s_map_square1,
                                         kCcDirtyVolume);
            if (voice_matches(chan, s_map_square2))
                apply_cc_update_to_voice(kGbVoiceSquare2, s_map_square2,
                                         kCcDirtyVolume);
            if (voice_matches(chan, s_map_wave))
                apply_cc_update_to_voice(kGbVoiceWave, s_map_wave,
                                         kCcDirtyVolume);
            if (voice_matches(chan, s_map_noise))
                apply_cc_update_to_voice(kGbVoiceNoise, s_map_noise,
                                         kCcDirtyVolume);
        }
        return;
    }

    if (voice_matches(chan, s_map_square1))
        refresh_voice(kGbVoiceSquare1, s_map_square1);
    if (voice_matches(chan, s_map_square2))
        refresh_voice(kGbVoiceSquare2, s_map_square2);
    if (voice_matches(chan, s_map_wave))
        refresh_voice(kGbVoiceWave, s_map_wave);
    if (voice_matches(chan, s_map_noise))
        refresh_voice(kGbVoiceNoise, s_map_noise);
}

static void clear_channel_notes(uint8_t chan)
{
    midi_chan_state_t& state = s_chan[chan];
    state.held_count = 0;
    sync_current_note_from_stack(state);
    if (voice_matches(chan, s_map_square1))
        set_voice_enabled(kGbVoiceSquare1, false);
    if (voice_matches(chan, s_map_square2))
        set_voice_enabled(kGbVoiceSquare2, false);
    if (voice_matches(chan, s_map_wave))
        set_voice_enabled(kGbVoiceWave, false);
    if (voice_matches(chan, s_map_noise))
        set_voice_enabled(kGbVoiceNoise, false);
}

void gb_apu_midi_cc(uint8_t chan, uint8_t cc, uint8_t val)
{
    gb_apu_midi_cc_at(chan, cc, val, 0);
}

void gb_apu_midi_cc_at(uint8_t chan, uint8_t cc, uint8_t val,
                       uint32_t timestamp_us)
{
    if (!s_enabled || chan >= kMidiChannelCount)
        return;
    if (timestamp_us != 0) {
        queue_event(kMidiEventCc, chan, cc, val, timestamp_us);
        return;
    }
    apply_cc(chan, cc, val);
}

static void apply_cc(uint8_t chan, uint8_t cc, uint8_t val)
{
    midi_chan_state_t& state = s_chan[chan];
    uint8_t dirty = 0;
    if (cc == 7) {
        if (state.cc_volume != val) {
            state.cc_volume = val;
            dirty |= kCcDirtyVolume;
        }
    } else if (cc == 11) {
        if (state.cc_expression != val) {
            state.cc_expression = val;
            dirty |= kCcDirtyVolume;
        }
    } else if (cc == 101) {
        state.rpn_msb = val;
        return;
    } else if (cc == 100) {
        state.rpn_lsb = val;
        return;
    } else if (cc == 98 || cc == 99) {
        state.rpn_msb = 127;
        state.rpn_lsb = 127;
        return;
    } else if (cc == 6) {
        if (state.rpn_msb == 0 && state.rpn_lsb == 0) {
            const uint8_t next = (uint8_t)clamp_int((int)val, 0, 24);
            if (state.pb_range_semi != next) {
                state.pb_range_semi = next;
                retune_channel_no_trigger(chan);
            }
        }
        return;
    } else if (cc == 38) {
        if (state.rpn_msb == 0 && state.rpn_lsb == 0) {
            const uint8_t next = (uint8_t)clamp_int((int)val, 0, 99);
            if (state.pb_range_cents != next) {
                state.pb_range_cents = next;
                retune_channel_no_trigger(chan);
            }
        }
        return;
    } else if (cc == 121) {
        state.cc_volume = 127;
        state.cc_expression = 127;
        state.pitch_bend = 0;
        state.rpn_msb = 127;
        state.rpn_lsb = 127;
        state.pb_range_semi = s_pb_default_range;
        state.pb_range_cents = 0;
        if (state.current_note >= 0) {
            if (voice_matches(chan, s_map_square1))
                apply_cc_update_to_voice(kGbVoiceSquare1, s_map_square1,
                                         kCcDirtyVolume);
            if (voice_matches(chan, s_map_square2))
                apply_cc_update_to_voice(kGbVoiceSquare2, s_map_square2,
                                         kCcDirtyVolume);
            if (voice_matches(chan, s_map_wave))
                apply_cc_update_to_voice(kGbVoiceWave, s_map_wave,
                                         kCcDirtyVolume);
            if (voice_matches(chan, s_map_noise))
                apply_cc_update_to_voice(kGbVoiceNoise, s_map_noise,
                                         kCcDirtyVolume);
            retune_channel_no_trigger(chan);
        }
        return;
    } else if (cc == 120 || cc == 123 || (cc >= 124 && cc <= 127)) {
        clear_channel_notes(chan);
        return;
    }

    if (cc == 7 || cc == 11) {
        if (dirty && state.current_note >= 0) {
            if (voice_matches(chan, s_map_square1))
                apply_cc_update_to_voice(kGbVoiceSquare1, s_map_square1, dirty);
            if (voice_matches(chan, s_map_square2))
                apply_cc_update_to_voice(kGbVoiceSquare2, s_map_square2, dirty);
            if (voice_matches(chan, s_map_wave))
                apply_cc_update_to_voice(kGbVoiceWave, s_map_wave, dirty);
            if (voice_matches(chan, s_map_noise))
                apply_cc_update_to_voice(kGbVoiceNoise, s_map_noise, dirty);
        }
        return;
    }

    if (s_cc_env_mode >= 0 && cc == (uint8_t)s_cc_env_mode) {
        const uint8_t next = (val >= 64) ? 1 : 0;
        if (state.env_mode != next) {
            state.env_mode = next;
            dirty |= kCcDirtyEnvelope;
        }
    } else if (s_cc_env_decay >= 0 && cc == (uint8_t)s_cc_env_decay) {
        const uint8_t next = (uint8_t)clamp_int((val * 8) / 128, 0, 7);
        if (state.env_decay != next) {
            state.env_decay = next;
            dirty |= kCcDirtyEnvelope;
        }
    } else if (s_cc_env_loop >= 0 && cc == (uint8_t)s_cc_env_loop) {
        const uint8_t next = (val >= 64) ? 1 : 0;
        if (state.env_loop != next) {
            state.env_loop = next;
            dirty |= kCcDirtyLength;
        }
    } else if (s_cc_length >= 0 && cc == (uint8_t)s_cc_length) {
        if (state.length_value != val) {
            state.length_value = val;
            dirty |= kCcDirtyLength;
        }
    } else if (s_cc_square1_duty >= 0 &&
               cc == (uint8_t)s_cc_square1_duty &&
               voice_matches(chan, s_map_square1)) {
        const uint8_t next = (uint8_t)clamp_int((val * 4) / 128, 0, 3);
        if (s_square1_duty != next) {
            s_square1_duty = next;
            dirty |= kCcDirtySquare1Duty;
        }
    } else if (s_cc_square2_duty >= 0 &&
               cc == (uint8_t)s_cc_square2_duty &&
               voice_matches(chan, s_map_square2)) {
        const uint8_t next = (uint8_t)clamp_int((val * 4) / 128, 0, 3);
        if (s_square2_duty != next) {
            s_square2_duty = next;
            dirty |= kCcDirtySquare2Duty;
        }
    } else if (s_cc_noise_mode >= 0 &&
               cc == (uint8_t)s_cc_noise_mode &&
               voice_matches(chan, s_map_noise)) {
        const uint8_t next = (val >= 64) ? 1 : 0;
        if (s_noise_short != next) {
            s_noise_short = next;
            dirty |= kCcDirtyNoiseMode;
        }
    } else if (s_cc_waveform >= 0 &&
               cc == (uint8_t)s_cc_waveform &&
               voice_matches(chan, s_map_wave)) {
        const uint8_t preset_count = (uint8_t)(sizeof(kWaveformPresets) / sizeof(kWaveformPresets[0]));
        const uint8_t preset = (uint8_t)clamp_int((val * preset_count) / 128, 0, preset_count - 1);
        if (s_waveform_index != preset) {
            load_waveform_preset(preset);
            dirty |= kCcDirtyWaveform;
        }
    }

    if (dirty && state.current_note >= 0) {
        if (voice_matches(chan, s_map_square1))
            apply_cc_update_to_voice(kGbVoiceSquare1, s_map_square1, dirty);
        if (voice_matches(chan, s_map_square2))
            apply_cc_update_to_voice(kGbVoiceSquare2, s_map_square2, dirty);
        if (voice_matches(chan, s_map_wave))
            apply_cc_update_to_voice(kGbVoiceWave, s_map_wave, dirty);
        if (voice_matches(chan, s_map_noise))
            apply_cc_update_to_voice(kGbVoiceNoise, s_map_noise, dirty);
    }
}

int gb_apu_midi_cc_is_sound_control(uint8_t cc)
{
    if (standard_sound_cc(cc))
        return 1;
    return (s_cc_env_mode >= 0 && cc == (uint8_t)s_cc_env_mode) ||
           (s_cc_env_decay >= 0 && cc == (uint8_t)s_cc_env_decay) ||
           (s_cc_env_loop >= 0 && cc == (uint8_t)s_cc_env_loop) ||
           (s_cc_length >= 0 && cc == (uint8_t)s_cc_length) ||
           (s_cc_square1_duty >= 0 && cc == (uint8_t)s_cc_square1_duty) ||
           (s_cc_square2_duty >= 0 && cc == (uint8_t)s_cc_square2_duty) ||
           (s_cc_noise_mode >= 0 && cc == (uint8_t)s_cc_noise_mode) ||
           (s_cc_waveform >= 0 && cc == (uint8_t)s_cc_waveform);
}

void gb_apu_midi_pitch_bend(uint8_t chan, uint16_t bend14)
{
    gb_apu_midi_pitch_bend_at(chan, bend14, 0);
}

void gb_apu_midi_pitch_bend_at(uint8_t chan, uint16_t bend14,
                               uint32_t timestamp_us)
{
    if (!s_enabled || chan >= kMidiChannelCount)
        return;
    if (bend14 > 16383)
        bend14 = 16383;
    if (timestamp_us != 0) {
        queue_event(kMidiEventPitchBend, chan, 0, bend14, timestamp_us);
        return;
    }
    apply_pitch_bend(chan, bend14);
}

static void apply_pitch_bend(uint8_t chan, uint16_t bend14)
{
    const int16_t next = (int16_t)bend14 - 8192;
    if (s_chan[chan].pitch_bend == next)
        return;
    s_chan[chan].pitch_bend = next;
    if (s_chan[chan].current_note < 0)
        return;

    if (voice_matches(chan, s_map_square1))
        retune_voice_no_trigger(kGbVoiceSquare1, s_map_square1);
    if (voice_matches(chan, s_map_square2))
        retune_voice_no_trigger(kGbVoiceSquare2, s_map_square2);
    if (voice_matches(chan, s_map_wave))
        retune_voice_no_trigger(kGbVoiceWave, s_map_wave);
}

void gb_apu_midi_set_duty_cc(int8_t cc_square1, int8_t cc_square2)
{
    s_cc_square1_duty = (int8_t)clamp_int(cc_square1, -1, 127);
    s_cc_square2_duty = (int8_t)clamp_int(cc_square2, -1, 127);
    normalize_custom_ccs();
}

void gb_apu_midi_get_duty_cc(int8_t* cc_square1, int8_t* cc_square2)
{
    if (cc_square1)
        *cc_square1 = s_cc_square1_duty;
    if (cc_square2)
        *cc_square2 = s_cc_square2_duty;
}

void gb_apu_midi_set_noise_mode_cc(int8_t cc_noise)
{
    s_cc_noise_mode = (int8_t)clamp_int(cc_noise, -1, 127);
    normalize_custom_ccs();
}

void gb_apu_midi_get_noise_mode_cc(int8_t* cc_noise)
{
    if (cc_noise)
        *cc_noise = s_cc_noise_mode;
}

void gb_apu_midi_set_waveform_cc(int8_t cc_waveform)
{
    s_cc_waveform = (int8_t)clamp_int(cc_waveform, -1, 127);
    normalize_custom_ccs();
}

void gb_apu_midi_get_waveform_cc(int8_t* cc_waveform)
{
    if (cc_waveform)
        *cc_waveform = s_cc_waveform;
}

void gb_apu_midi_set_env_cc(int8_t cc_mode, int8_t cc_decay,
                            int8_t cc_loop, int8_t cc_length)
{
    s_cc_env_mode = (int8_t)clamp_int(cc_mode, -1, 127);
    s_cc_env_decay = (int8_t)clamp_int(cc_decay, -1, 127);
    s_cc_env_loop = (int8_t)clamp_int(cc_loop, -1, 127);
    s_cc_length = (int8_t)clamp_int(cc_length, -1, 127);
    normalize_custom_ccs();
}

void gb_apu_midi_get_env_cc(int8_t* cc_mode, int8_t* cc_decay,
                            int8_t* cc_loop, int8_t* cc_length)
{
    if (cc_mode)
        *cc_mode = s_cc_env_mode;
    if (cc_decay)
        *cc_decay = s_cc_env_decay;
    if (cc_loop)
        *cc_loop = s_cc_env_loop;
    if (cc_length)
        *cc_length = s_cc_length;
}

void gb_apu_midi_set_pb_default_range(uint8_t semis)
{
    const uint8_t next = (uint8_t)clamp_int((int)semis, 0, 24);
    if (s_pb_default_range == next)
        return;
    const uint8_t old_default = s_pb_default_range;
    s_pb_default_range = next;
    for (uint8_t chan = 0; chan < kMidiChannelCount; ++chan) {
        midi_chan_state_t& state = s_chan[chan];
        if (state.pb_range_semi != old_default || state.pb_range_cents != 0)
            continue;
        state.pb_range_semi = next;
        if (s_enabled)
            retune_channel_no_trigger(chan);
    }
}

uint8_t gb_apu_midi_get_pb_default_range(void)
{
    return s_pb_default_range;
}

static void render_midi_samples(audio_sample_t* out, uint16_t count)
{
    if (!out || count == 0)
        return;
    minigb_apu_audio_callback_mono_samples(&s_apu, out, count);
    retrigger_looping_voice(kGbVoiceSquare1, s_map_square1);
    retrigger_looping_voice(kGbVoiceSquare2, s_map_square2);
    retrigger_looping_voice(kGbVoiceNoise, s_map_noise);
}

static void render_midi_samples_timed(audio_sample_t* out, uint16_t count,
                                      uint32_t first_sample_playout_us)
{
    const uint32_t target_render_time_us =
        first_sample_playout_us - kMidiRenderLatencyUs;
    if (!s_render_time_ready) {
        s_render_time_us = target_render_time_us;
        s_render_time_remainder = 0;
        s_render_time_ready = true;
    } else {
        const int32_t behind_us =
            (int32_t)(target_render_time_us - s_render_time_us);
        if (behind_us > (int32_t)kMidiRenderStallResetUs) {
            s_render_time_us = target_render_time_us;
            s_render_time_remainder = 0;
        }
    }

    uint16_t rendered = 0;
    while (rendered < count) {
        while (s_event_read != s_event_write) {
            const midi_event_t& event = s_event_queue[s_event_read];
            const int32_t delta_us =
                (int32_t)(event.timestamp_us - s_render_time_us);
            if (delta_us > 0)
                break;
            apply_event(event);
            s_event_read =
                (uint16_t)((s_event_read + 1u) &
                           (kMidiEventQueueSize - 1u));
        }

        const uint16_t remaining = (uint16_t)(count - rendered);
        if (s_event_read == s_event_write) {
            render_midi_samples(out + rendered, remaining);
            advance_render_time(remaining);
            break;
        }

        const midi_event_t& event = s_event_queue[s_event_read];
        const uint32_t delta_us =
            (uint32_t)(event.timestamp_us - s_render_time_us);
        uint32_t until_event =
            ((uint64_t)delta_us * AUDIO_SAMPLE_RATE + 999999u) / 1000000u;
        if (until_event == 0)
            until_event = 1;
        const uint16_t segment =
            (uint16_t)(until_event < remaining ? until_event : remaining);
        render_midi_samples(out + rendered, segment);
        advance_render_time(segment);
        rendered = (uint16_t)(rendered + segment);
    }
}

void gb_apu_midi_render_samples(audio_sample_t* mono_out, uint16_t count)
{
    if (!mono_out || count == 0)
        return;
    if (!s_enabled) {
        memset(mono_out, 0, count * sizeof(audio_sample_t));
        return;
    }

    apply_all_queued_events();
    render_midi_samples(mono_out, count);
}

void gb_apu_midi_render_samples_timed(audio_sample_t* mono_out,
                                      uint16_t count,
                                      uint32_t now_us)
{
    if (!mono_out || count == 0)
        return;
    if (!s_enabled) {
        memset(mono_out, 0, count * sizeof(audio_sample_t));
        return;
    }

    render_midi_samples_timed(mono_out, count, now_us);
}

void gb_apu_midi_frame_tick(audio_sample_t* mono_out)
{
    if (!mono_out)
        return;
    gb_apu_midi_render_samples(mono_out, AUDIO_SAMPLES);
}
