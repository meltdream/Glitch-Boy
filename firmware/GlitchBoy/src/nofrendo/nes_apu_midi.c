/* NES APU MIDI bridge: drives the APU voices directly from MIDI input. */

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "noftypes.h"
#include "nes_apu.h"
#include "nes_apu_midi.h"

typedef int32_t q16_t;  /* 16.16 MIDI note number */

static int s_enabled = 0;
static int8_t s_map_p1 = 0;   /* defaults: P1=0, P2=1, TRI=2, NOI=9 */
static int8_t s_map_p2 = 1;
static int8_t s_map_tri = 2;
static int8_t s_map_noi = 9;   /* drums */
/* Optional CC numbers for duty control, -1=disabled
 * Defaults: Pulse1=CC74 (Brightness), Pulse2=CC71 (Resonance) */
static int8_t s_cc_p1_duty = 74;
static int8_t s_cc_p2_duty = 71;
/* Optional CC numbers for envelope/length controls (MIDI standard-ish) */
static int8_t s_cc_env_mode = 78;     /* >=64 enables envelope mode, <64 fixed volume */
static int8_t s_cc_env_decay = 75;    /* 0..127 -> 0..15 */
static int8_t s_cc_env_loop = 64;     /* >=64 hold/loop, <64 release */
static int8_t s_cc_length = 72;       /* 0..127 -> length index 0..31 */
static int8_t s_cc_tri_linear = 73;   /* 0..127 linear counter */
/* Current duty index 0..3 for each pulse (default 50%) */
static uint8_t s_p1_duty_idx = 2;
static uint8_t s_p2_duty_idx = 2;

/* Per-MIDI-channel expression/volume/pitch + RPN state */
typedef struct {
    int8_t current_note;    /* -1 if none */
    uint8_t current_vel;    /* 0..127 */
    uint8_t held_count;
    struct {
        uint8_t note;
        uint8_t vel;
    } held[16];
    uint8_t cc_volume;      /* CC7 0..127 */
    uint8_t cc_expression;  /* CC11 0..127 */
    int16_t pitch_bend;     /* -8192..+8191 */
    uint8_t env_mode;       /* 0=fixed volume, 1=envelope */
    uint8_t env_decay;      /* 0..15 */
    uint8_t env_loop;       /* 0/1 (hold/loop) */
    uint8_t length_idx;     /* 0..31 */
    uint8_t tri_linear;     /* 0..127 */
    /* RPN selection and per-channel bend range */
    uint8_t rpn_msb;        /* 0..127, 127=unset */
    uint8_t rpn_lsb;        /* 0..127, 127=unset */
    uint8_t pb_range_semi;  /* 0..24 semitones */
    uint8_t pb_range_cents; /* 0..99 cents */
} midi_chan_state_t;

static midi_chan_state_t s_chan[16];
static uint16_t s_rect_note_period[128];
static uint16_t s_tri_note_period[128];

/* Channel enable bitmask we maintain and write to $4015 */
static uint8_t s_4015_mask = 0; /* bit0=P1, bit1=P2, bit2=TRI, bit3=NOI */

/* Global/defaults */
static uint8_t s_pb_default_range = 2; /* semitones */
static int8_t  s_cc_noise_mode = -1;   /* -1 disabled; else 0..127 toggles short/long when >=64 */
static uint8_t s_noise_short = 0;      /* 0=long, 1=short */
/* Track current noise frequency index (0..15) for mode toggling) */
static uint8_t s_noise_idx = 0;

static void normalize_configurable_ccs(void)
{
    uint8_t used[128] = {0};
    used[6] = used[7] = used[11] = used[38] = 1;
    for (int cc = 96; cc <= 101; ++cc) used[cc] = 1;
    for (int cc = 120; cc <= 127; ++cc) used[cc] = 1;
    if (s_cc_tri_linear >= 0)
        used[(uint8_t)s_cc_tri_linear] = 1;
    int8_t* mappings[] = {
        &s_cc_env_mode,
        &s_cc_env_decay,
        &s_cc_env_loop,
        &s_cc_length,
        &s_cc_p1_duty,
        &s_cc_p2_duty,
        &s_cc_noise_mode,
    };
    for (unsigned int i = 0; i < sizeof(mappings) / sizeof(mappings[0]); ++i) {
        int8_t* mapping = mappings[i];
        if (*mapping < 0)
            continue;
        if (used[(uint8_t)*mapping]) {
            *mapping = -1;
            continue;
        }
        used[(uint8_t)*mapping] = 1;
    }
}

/* Track last written periods to avoid unnecessary high-byte writes (reduce clicks) */
static uint16_t s_last_per_p1 = 0xFFFF;
static uint16_t s_last_per_p2 = 0xFFFF;
static uint16_t s_last_per_tri = 0xFFFF;

static void init_midi_chan_state(void)
{
    for (int i = 0; i < 16; ++i) {
        s_chan[i].current_note = -1;
        s_chan[i].current_vel = 0;
        s_chan[i].held_count = 0;
        s_chan[i].cc_volume = 127;
        s_chan[i].cc_expression = 127;
        s_chan[i].pitch_bend = 0;
        s_chan[i].env_mode = 0;
        s_chan[i].env_decay = 8;
        s_chan[i].env_loop = 1;
        s_chan[i].length_idx = 1;
        s_chan[i].tri_linear = 0x7F;
        s_chan[i].rpn_msb = 127;
        s_chan[i].rpn_lsb = 127;
        s_chan[i].pb_range_semi = s_pb_default_range;
        s_chan[i].pb_range_cents = 0;
    }
}

/* Helpers */
static inline q16_t semitone_offset_q16(uint8_t note, int16_t bend, uint8_t range_semi, uint8_t range_cents)
{
    const int32_t range_c =
        (int32_t)range_semi * 100 + (int32_t)range_cents;
    const int64_t bend_q16 =
        ((int64_t)range_c * (int64_t)bend * 65536) / (8192 * 100);
    return (q16_t)(((int32_t)note << 16) + (int32_t)bend_q16);
}

static inline uint8_t scale127_to_15(uint8_t v)
{
    /* round to nearest */
    return (uint8_t)((v * 15 + 63) / 127);
}

static inline uint8_t length_index_from_cc(uint8_t v)
{
    static const uint8_t sorted_length_indices[32] = {
        3, 5, 7, 9, 0, 11, 13, 16,
        12, 15, 17, 28, 19, 2, 21, 23,
        18, 25, 14, 27, 29, 31, 30, 4,
        20, 10, 26, 6, 22, 8, 24, 1
    };
    const uint8_t position = (uint8_t)((v * 31u + 63u) / 127u);
    return sorted_length_indices[position];
}

static inline uint8_t midi_gain127(uint8_t volume, uint8_t expression,
                                   uint8_t velocity)
{
    const uint32_t denom = 127u * 127u;
    uint32_t scaled127 = ((uint32_t)volume * (uint32_t)expression * (uint32_t)velocity + (denom / 2u)) / denom;
    if (scaled127 > 127u)
        scaled127 = 127u;
    return (uint8_t)scaled127;
}

static inline uint8_t midi_chan_gain127(const midi_chan_state_t* cs)
{
    return midi_gain127(cs->cc_volume, cs->cc_expression, cs->current_vel);
}

static inline uint16_t clamp_u16(int v, int minv, int maxv)
{
    if (v < minv) v = minv;
    if (v > maxv) v = maxv;
    return (uint16_t)v;
}

static void write_4015(void)
{
    apu_midi_write(APU_SMASK, s_4015_mask);
}

static void build_note_period_tables(void)
{
    const double base = apu_get_base_freq();
    double hz = 8.175798915643707;
    const double ratio = 1.0594630943592953;
    for (int note = 0; note < 128; ++note) {
        const int rect = (int)(base / (16.0 * hz) + 0.5) - 1;
        const int tri = (int)(base / (32.0 * hz) + 0.5) - 1;
        s_rect_note_period[note] = clamp_u16(rect, 8, 0x7FF);
        s_tri_note_period[note] = clamp_u16(tri, 4, 0x7FF);
        hz *= ratio;
    }
}

static uint16_t interpolate_period(const uint16_t table[128],
                                   q16_t note_q16, int minimum)
{
    if (note_q16 <= 0)
        return table[0];
    if (note_q16 >= (127 << 16))
        return table[127];
    const int index = note_q16 >> 16;
    const uint32_t fraction = (uint32_t)note_q16 & 0xFFFFu;
    const int32_t a = table[index];
    const int32_t b = table[index + 1];
    const int32_t value =
        a + (int32_t)(((int64_t)(b - a) * fraction + 32768) >> 16);
    return clamp_u16(value, minimum, 0x7FF);
}

static uint16_t rect_note_to_period_q(q16_t note_q16)
{
    return interpolate_period(s_rect_note_period, note_q16, 8);
}

static uint16_t tri_note_to_period_q(q16_t note_q16)
{
    return interpolate_period(s_tri_note_period, note_q16, 4);
}

static inline uint8_t pulse_reg0_from_state(const midi_chan_state_t* cs, uint8_t duty_idx)
{
    uint8_t env_val = cs->env_mode ? cs->env_decay : 15;
    uint8_t fixed = cs->env_mode ? 0x00 : 0x10;
    uint8_t hold = cs->env_loop ? 0x20 : 0x00;
    return (uint8_t)(((duty_idx & 3) << 6) | hold | fixed | (env_val & 0x0F));
}

static inline uint8_t noise_reg0_from_state(const midi_chan_state_t* cs)
{
    uint8_t env_val = cs->env_mode ? cs->env_decay : 15;
    uint8_t fixed = cs->env_mode ? 0x00 : 0x10;
    uint8_t hold = cs->env_loop ? 0x20 : 0x00;
    return (uint8_t)(hold | fixed | (env_val & 0x0F));
}

static inline uint8_t triangle_reg0_from_state(const midi_chan_state_t* cs)
{
    uint8_t hold = cs->env_loop ? 0x80 : 0x00;
    return (uint8_t)(hold | (cs->tri_linear & 0x7F));
}

static void set_pulse(int idx, const midi_chan_state_t* cs, uint8_t duty_idx, uint16_t period)
{
    uint16_t p = period & 0x7FF;
    uint8_t v0 = pulse_reg0_from_state(cs, duty_idx);
    uint8_t v2 = (uint8_t)(p & 0xFF);
    uint8_t v3 = (uint8_t)((p >> 8) & 0x07); /* length set separately by upper bits; we only need non-zero */
    v3 |= (uint8_t)((cs->length_idx & 0x1F) << 3);
    if (idx == 0) {
        /* $08 fully disables sweep including its always-active overflow mute. */
        apu_midi_write(APU_WRA1, 0x08);
        apu_midi_write(APU_WRA0, v0);
        apu_midi_write(APU_WRA2, v2);
        apu_midi_write(APU_WRA3, v3);
        s_last_per_p1 = p;
    } else {
        apu_midi_write(APU_WRB1, 0x08);
        apu_midi_write(APU_WRB0, v0);
        apu_midi_write(APU_WRB2, v2);
        apu_midi_write(APU_WRB3, v3);
        s_last_per_p2 = p;
    }
}

static void retune_pulse_continuous(int idx, const midi_chan_state_t* cs,
                                    uint16_t period)
{
    uint16_t p = period & 0x7FF;
    uint16_t* last = (idx == 0) ? &s_last_per_p1 : &s_last_per_p2;
    uint32_t lo_addr = (idx == 0) ? APU_WRA2 : APU_WRB2;
    uint32_t hi_addr = (idx == 0) ? APU_WRA3 : APU_WRB3;
    uint8_t hi = (uint8_t)((p >> 8) & 0x07);

    if (*last == p)
        return;

    apu_midi_write(lo_addr, (uint8_t)(p & 0xFF));
    if (*last == 0xFFFF || (((*last >> 8) & 0x07) != hi)) {
        apu_midi_write_continuous(
            hi_addr, (uint8_t)(hi | ((cs->length_idx & 0x1F) << 3)));
    }
    *last = p;
}

static void set_triangle(const midi_chan_state_t* cs, uint16_t period)
{
    uint16_t p = period & 0x7FF;
    uint8_t v0 = triangle_reg0_from_state(cs);
    uint8_t v2 = (uint8_t)(p & 0xFF);
    uint8_t v3 = (uint8_t)((p >> 8) & 0x07);
    v3 |= (uint8_t)((cs->length_idx & 0x1F) << 3);
    apu_midi_write(APU_WRC0, v0);
    apu_midi_write(APU_WRC2, v2);
    apu_midi_write(APU_WRC3, v3);
    s_last_per_tri = p;
}

static void retune_triangle_continuous(const midi_chan_state_t* cs,
                                       uint16_t period)
{
    uint16_t p = period & 0x7FF;
    uint8_t hi = (uint8_t)((p >> 8) & 0x07);

    if (s_last_per_tri == p)
        return;

    apu_midi_write(APU_WRC2, (uint8_t)(p & 0xFF));
    if (s_last_per_tri == 0xFFFF ||
        (((s_last_per_tri >> 8) & 0x07) != hi)) {
        apu_midi_write_continuous(
            APU_WRC3, (uint8_t)(hi | ((cs->length_idx & 0x1F) << 3)));
    }
    s_last_per_tri = p;
}

static void set_noise(const midi_chan_state_t* cs, uint8_t noise_idx)
{
    /* noise_idx: 0..15 into noise_freq[] via WRD2 low nibble */
    uint8_t v0 = noise_reg0_from_state(cs);
    s_noise_idx = (uint8_t)(noise_idx & 0x0F);
    uint8_t v2 = (uint8_t)((s_noise_short ? 0x80 : 0) | s_noise_idx);
    uint8_t v3 = (uint8_t)((cs->length_idx & 0x1F) << 3);
    apu_midi_write(APU_WRD0, v0);
    apu_midi_write(APU_WRD2, v2);
    apu_midi_write(APU_WRD3, v3);
}

/* -1 disables a voice, 0..15 selects a MIDI channel. */
static inline int8_t clamp_midi_channel(int8_t chan)
{
    if (chan < -1) return -1;
    if (chan > 15) return 15;
    return chan;
}

static inline int voice_matches(uint8_t midi_chan, int8_t mapped)
{
    return (mapped >= 0 && mapped < 16 && midi_chan == (uint8_t)mapped);
}

static void sync_current_note_from_stack(midi_chan_state_t* cs)
{
    if (cs->held_count == 0) {
        cs->current_note = -1;
        cs->current_vel = 0;
        return;
    }
    const uint8_t top = (uint8_t)(cs->held_count - 1u);
    cs->current_note = (int8_t)cs->held[top].note;
    cs->current_vel = cs->held[top].vel;
}

static void push_held_note(midi_chan_state_t* cs, uint8_t note, uint8_t vel)
{
    if (cs->held_count >= 16) {
        memmove(&cs->held[0], &cs->held[1], 15 * sizeof(cs->held[0]));
        cs->held_count = 15;
    }
    cs->held[cs->held_count].note = note;
    cs->held[cs->held_count].vel = vel;
    cs->held_count++;
    sync_current_note_from_stack(cs);
}

static int remove_held_note(midi_chan_state_t* cs, uint8_t note)
{
    for (int i = (int)cs->held_count - 1; i >= 0; --i) {
        if (cs->held[i].note != note)
            continue;
        if (i + 1 < cs->held_count) {
            memmove(&cs->held[i], &cs->held[i + 1],
                    (cs->held_count - (uint8_t)i - 1u) *
                        sizeof(cs->held[0]));
        }
        cs->held_count--;
        sync_current_note_from_stack(cs);
        return 1;
    }
    return 0;
}

static void apply_channel_gain(uint8_t chan)
{
    const uint8_t gain = midi_chan_gain127(&s_chan[chan]);
    if (voice_matches(chan, s_map_p1)) apu_set_midi_voice_gain(0, gain);
    if (voice_matches(chan, s_map_p2)) apu_set_midi_voice_gain(1, gain);
    if (voice_matches(chan, s_map_tri)) apu_set_midi_voice_gain(2, gain);
    if (voice_matches(chan, s_map_noi)) apu_set_midi_voice_gain(3, gain);
}

static void clear_channel_notes(uint8_t chan)
{
    s_chan[chan].held_count = 0;
    sync_current_note_from_stack(&s_chan[chan]);
    apply_channel_gain(chan);
}

static void update_enable_mask_from_notes(void)
{
    uint8_t m = 0;
    if (s_map_p1 >= 0 && s_map_p1 < 16 && s_chan[(uint8_t)s_map_p1].current_note >= 0) m |= 0x01;
    if (s_map_p2 >= 0 && s_map_p2 < 16 && s_chan[(uint8_t)s_map_p2].current_note >= 0) m |= 0x02;
    if (s_map_tri >= 0 && s_map_tri < 16 && s_chan[(uint8_t)s_map_tri].current_note >= 0) m |= 0x04;
    if (s_map_noi >= 0 && s_map_noi < 16 && s_chan[(uint8_t)s_map_noi].current_note >= 0) m |= 0x08;
    s_4015_mask = m;
    write_4015();
}

static void program_mapped_voice(int voice, int8_t mapped)
{
    if (mapped < 0 || mapped >= 16)
        return;
    midi_chan_state_t* cs = &s_chan[(uint8_t)mapped];
    if (cs->current_note < 0)
        return;

    const uint8_t note = (uint8_t)cs->current_note;
    const q16_t pitch = semitone_offset_q16(
        note, cs->pitch_bend, cs->pb_range_semi, cs->pb_range_cents);
    if (voice == 0) {
        set_pulse(0, cs, s_p1_duty_idx, rect_note_to_period_q(pitch));
    } else if (voice == 1) {
        set_pulse(1, cs, s_p2_duty_idx, rect_note_to_period_q(pitch));
    } else if (voice == 2) {
        set_triangle(cs, tri_note_to_period_q(pitch));
    } else if (voice == 3) {
        /* NES noise period grows with index, so invert: higher notes, higher pitch. */
        const uint8_t scaled = (uint8_t)((note <= 24) ? 0 :
            (note >= 108 ? 15 : ((note - 24) * 16) / 84));
        set_noise(cs, (uint8_t)(15 - scaled));
    }
    apu_set_midi_voice_gain(voice, midi_chan_gain127(cs));
}

static void refresh_all_mapped_voices(void)
{
    program_mapped_voice(0, s_map_p1);
    program_mapped_voice(1, s_map_p2);
    program_mapped_voice(2, s_map_tri);
    program_mapped_voice(3, s_map_noi);
    update_enable_mask_from_notes();
}

void nes_apu_midi_enable(int enable)
{
    const int next = enable ? 1 : 0;
    if (s_enabled == next)
        return;
    if (!next && s_enabled) {
        s_4015_mask = 0;
        write_4015();
        for (int voice = 0; voice < 4; ++voice)
            apu_set_midi_voice_gain(voice, 0);
    }
    s_enabled = next;
    apu_set_midi_override(s_enabled);
    if (s_enabled) {
        build_note_period_tables();
        /*
         * A loaded ROM can leave sweep, envelope, DMC DAC, and channel output
         * state active. Start MIDI override from a deterministic silent APU
         * instead of inheriting note-dependent modulation from gameplay.
         */
        apu_reset();
        apu_midi_write(APU_WRA1, 0x08);
        apu_midi_write(APU_WRB1, 0x08);
        /* On enable: immediately silence CPU-driven APU audio and reset MIDI state. */
        s_4015_mask = 0;
        write_4015();
        init_midi_chan_state();
        s_last_per_p1 = s_last_per_p2 = s_last_per_tri = 0xFFFF;
    }
}

int nes_apu_midi_is_enabled(void)
{
    return s_enabled;
}

void nes_apu_midi_set_channel_map(int8_t pulse1, int8_t pulse2, int8_t triangle, int8_t noise)
{
    s_map_p1 = clamp_midi_channel(pulse1);
    s_map_p2 = clamp_midi_channel(pulse2);
    s_map_tri = clamp_midi_channel(triangle);
    s_map_noi = clamp_midi_channel(noise);
    if (s_enabled)
        refresh_all_mapped_voices();
}

void nes_apu_midi_get_channel_map(int8_t* pulse1, int8_t* pulse2, int8_t* triangle, int8_t* noise)
{
    if (pulse1) *pulse1 = s_map_p1;
    if (pulse2) *pulse2 = s_map_p2;
    if (triangle) *triangle = s_map_tri;
    if (noise) *noise = s_map_noi;
}

void nes_apu_midi_note_on(uint8_t chan, uint8_t note, uint8_t vel)
{
    if (!s_enabled) return;
    if (chan >= 16) return;
    if (vel == 0) {
        nes_apu_midi_note_off(chan, note, 0);
        return;
    }
    push_held_note(&s_chan[chan], note, vel);
    if (voice_matches(chan, s_map_p1)) program_mapped_voice(0, s_map_p1);
    if (voice_matches(chan, s_map_p2)) program_mapped_voice(1, s_map_p2);
    if (voice_matches(chan, s_map_tri)) program_mapped_voice(2, s_map_tri);
    if (voice_matches(chan, s_map_noi)) program_mapped_voice(3, s_map_noi);
    update_enable_mask_from_notes();
}

void nes_apu_midi_note_off(uint8_t chan, uint8_t note, uint8_t vel)
{
    (void)vel;
    if (!s_enabled) return;
    if (chan >= 16) return;
    midi_chan_state_t* cs = &s_chan[chan];
    const int8_t previous = cs->current_note;
    const uint8_t previous_vel = cs->current_vel;
    if (!remove_held_note(cs, note))
        return;
    if (cs->current_note != previous) {
        if (cs->current_note >= 0) {
            if (voice_matches(chan, s_map_p1)) program_mapped_voice(0, s_map_p1);
            if (voice_matches(chan, s_map_p2)) program_mapped_voice(1, s_map_p2);
            if (voice_matches(chan, s_map_tri)) program_mapped_voice(2, s_map_tri);
            if (voice_matches(chan, s_map_noi)) program_mapped_voice(3, s_map_noi);
        }
        apply_channel_gain(chan);
        update_enable_mask_from_notes();
    } else if (cs->current_vel != previous_vel) {
        apply_channel_gain(chan);
    }
}

static void retune_channel(uint8_t chan)
{
    midi_chan_state_t* cs = &s_chan[chan];
    if (cs->current_note < 0)
        return;
    const uint8_t note = (uint8_t)cs->current_note;
    const q16_t pitch = semitone_offset_q16(
        note, cs->pitch_bend, cs->pb_range_semi, cs->pb_range_cents);
    if (voice_matches(chan, s_map_p1))
        retune_pulse_continuous(0, cs, rect_note_to_period_q(pitch));
    if (voice_matches(chan, s_map_p2))
        retune_pulse_continuous(1, cs, rect_note_to_period_q(pitch));
    if (voice_matches(chan, s_map_tri))
        retune_triangle_continuous(cs, tri_note_to_period_q(pitch));
}

void nes_apu_midi_cc(uint8_t chan, uint8_t cc, uint8_t val)
{
    if (!s_enabled) return;
    if (chan >= 16) return;
    midi_chan_state_t* cs = &s_chan[chan];
    if (cc == 7) {
        if (cs->cc_volume == val) return;
        cs->cc_volume = val;
        apply_channel_gain(chan);
        return;
    } else if (cc == 11) {
        if (cs->cc_expression == val) return;
        cs->cc_expression = val;
        apply_channel_gain(chan);
        return;
    } else if (cc == 101) { /* RPN MSB */
        cs->rpn_msb = val;
        return;
    } else if (cc == 100) { /* RPN LSB */
        cs->rpn_lsb = val;
        return;
    } else if (cc == 98 || cc == 99) {
        cs->rpn_msb = 127;
        cs->rpn_lsb = 127;
        return;
    } else if (cc == 6) { /* Data Entry MSB */
        if (cs->rpn_msb == 0 && cs->rpn_lsb == 0) {
            const uint8_t semis = (val > 24) ? 24 : val;
            if (cs->pb_range_semi != semis) {
                cs->pb_range_semi = semis;
                retune_channel(chan);
            }
        }
        return;
    } else if (cc == 38) { /* Data Entry LSB */
        if (cs->rpn_msb == 0 && cs->rpn_lsb == 0) {
            const uint8_t cents = (val > 99) ? 99 : val;
            if (cs->pb_range_cents != cents) {
                cs->pb_range_cents = cents;
                retune_channel(chan);
            }
        }
        return;
    } else if (cc == 121) {
        cs->cc_volume = 127;
        cs->cc_expression = 127;
        cs->pitch_bend = 0;
        cs->rpn_msb = 127;
        cs->rpn_lsb = 127;
        cs->pb_range_semi = s_pb_default_range;
        cs->pb_range_cents = 0;
        apply_channel_gain(chan);
        retune_channel(chan);
        return;
    } else if (cc == 120 || cc == 123 || (cc >= 124 && cc <= 127)) {
        clear_channel_notes(chan);
        update_enable_mask_from_notes();
        return;
    }

    uint8_t envelope_update = 0;
    uint8_t tri_update = 0;
    uint8_t length_update = 0;
    if (s_cc_env_mode >= 0 && cc == (uint8_t)s_cc_env_mode) {
        const uint8_t next = (val >= 64) ? 1 : 0;
        if (cs->env_mode == next) return;
        cs->env_mode = next;
        envelope_update = 1;
    } else if (s_cc_env_decay >= 0 && cc == (uint8_t)s_cc_env_decay) {
        const uint8_t next = scale127_to_15(val);
        if (cs->env_decay == next) return;
        cs->env_decay = next;
        envelope_update = 1;
    } else if (s_cc_env_loop >= 0 && cc == (uint8_t)s_cc_env_loop) {
        const uint8_t next = (val >= 64) ? 1 : 0;
        if (cs->env_loop == next) return;
        cs->env_loop = next;
        envelope_update = 1;
        tri_update = 1;
    } else if (s_cc_tri_linear >= 0 && cc == (uint8_t)s_cc_tri_linear) {
        if (cs->tri_linear == val) return;
        cs->tri_linear = val;
        tri_update = 1;
    } else if (s_cc_length >= 0 && cc == (uint8_t)s_cc_length) {
        const uint8_t next = length_index_from_cc(val);
        if (cs->length_idx == next) return;
        cs->length_idx = next;
        length_update = 1;
    } else if (s_cc_noise_mode >= 0 &&
               cc == (uint8_t)s_cc_noise_mode &&
               voice_matches(chan, s_map_noi)) {
        const uint8_t next = (val >= 64) ? 1 : 0;
        if (next == s_noise_short) return;
        s_noise_short = next;
        if (cs->current_note >= 0) {
            const uint8_t v2 =
                (uint8_t)((s_noise_short ? 0x80 : 0) |
                          (s_noise_idx & 0x0F));
            apu_midi_write(APU_WRD2, v2);
        }
        return;
    } else if (s_cc_p1_duty >= 0 &&
               cc == (uint8_t)s_cc_p1_duty &&
               voice_matches(chan, s_map_p1)) {
        const uint8_t next = (uint8_t)((val * 4) / 128);
        if (s_p1_duty_idx == next) return;
        s_p1_duty_idx = next;
        if (cs->current_note >= 0)
            apu_midi_write(APU_WRA0, pulse_reg0_from_state(cs, s_p1_duty_idx));
        return;
    } else if (s_cc_p2_duty >= 0 &&
               cc == (uint8_t)s_cc_p2_duty &&
               voice_matches(chan, s_map_p2)) {
        const uint8_t next = (uint8_t)((val * 4) / 128);
        if (s_p2_duty_idx == next) return;
        s_p2_duty_idx = next;
        if (cs->current_note >= 0)
            apu_midi_write(APU_WRB0, pulse_reg0_from_state(cs, s_p2_duty_idx));
        return;
    } else {
        return;
    }

    if (envelope_update && cs->current_note >= 0) {
        if (voice_matches(chan, s_map_p1))
            apu_midi_write(APU_WRA0, pulse_reg0_from_state(cs, s_p1_duty_idx));
        if (voice_matches(chan, s_map_p2))
            apu_midi_write(APU_WRB0, pulse_reg0_from_state(cs, s_p2_duty_idx));
        if (voice_matches(chan, s_map_noi))
            apu_midi_write(APU_WRD0, noise_reg0_from_state(cs));
    }
    if (tri_update && cs->current_note >= 0 && voice_matches(chan, s_map_tri)) {
        apu_midi_write(APU_WRC0, triangle_reg0_from_state(cs));
    }
    if (length_update && cs->current_note >= 0) {
        uint8_t len = (uint8_t)((cs->length_idx & 0x1F) << 3);
        if (voice_matches(chan, s_map_p1) && s_last_per_p1 != 0xFFFF) {
            uint8_t hi = (uint8_t)((s_last_per_p1 >> 8) & 0x07);
            apu_midi_write_length(APU_WRA3, (uint8_t)(hi | len));
        }
        if (voice_matches(chan, s_map_p2) && s_last_per_p2 != 0xFFFF) {
            uint8_t hi = (uint8_t)((s_last_per_p2 >> 8) & 0x07);
            apu_midi_write_length(APU_WRB3, (uint8_t)(hi | len));
        }
        if (voice_matches(chan, s_map_tri) && s_last_per_tri != 0xFFFF) {
            uint8_t hi = (uint8_t)((s_last_per_tri >> 8) & 0x07);
            apu_midi_write_length(APU_WRC3, (uint8_t)(hi | len));
        }
        if (voice_matches(chan, s_map_noi)) {
            apu_midi_write_length(APU_WRD3, len);
        }
    }
}

int nes_apu_midi_cc_is_sound_control(uint8_t cc)
{
    if (cc == 6 || cc == 7 || cc == 11 || cc == 38 ||
        (cc >= 96 && cc <= 101) || (cc >= 120 && cc <= 127))
        return 1;
    return (s_cc_env_mode >= 0 && cc == (uint8_t)s_cc_env_mode) ||
           (s_cc_env_decay >= 0 && cc == (uint8_t)s_cc_env_decay) ||
           (s_cc_env_loop >= 0 && cc == (uint8_t)s_cc_env_loop) ||
           (s_cc_tri_linear >= 0 && cc == (uint8_t)s_cc_tri_linear) ||
           (s_cc_length >= 0 && cc == (uint8_t)s_cc_length) ||
           (s_cc_noise_mode >= 0 && cc == (uint8_t)s_cc_noise_mode) ||
           (s_cc_p1_duty >= 0 && cc == (uint8_t)s_cc_p1_duty) ||
           (s_cc_p2_duty >= 0 && cc == (uint8_t)s_cc_p2_duty);
}

void nes_apu_midi_pitch_bend(uint8_t chan, uint16_t bend14)
{
    if (!s_enabled) return;
    if (chan >= 16) return;
    if (bend14 > 16383) bend14 = 16383;
    midi_chan_state_t* cs = &s_chan[chan];
    int16_t bend = (int16_t)bend14 - 8192; /* -8192..+8191 */
    if (cs->pitch_bend == bend)
        return;
    cs->pitch_bend = bend;

    retune_channel(chan);
}

/* Preference load/save lives in nes_apu_midi_opt.c. */

void nes_apu_midi_set_noise_mode_cc(int8_t cc_noise)
{
    s_cc_noise_mode = cc_noise < -1 ? -1 : cc_noise;
    normalize_configurable_ccs();
}

void nes_apu_midi_get_noise_mode_cc(int8_t* cc_noise)
{
    if (cc_noise) *cc_noise = s_cc_noise_mode;
}

void nes_apu_midi_set_env_cc(int8_t cc_mode, int8_t cc_decay,
                             int8_t cc_loop, int8_t cc_length)
{
    if (cc_mode < -1) cc_mode = -1;
    if (cc_decay < -1) cc_decay = -1;
    if (cc_loop < -1) cc_loop = -1;
    if (cc_length < -1) cc_length = -1;
    s_cc_env_mode = cc_mode;
    s_cc_env_decay = cc_decay;
    s_cc_env_loop = cc_loop;
    s_cc_length = cc_length;
    normalize_configurable_ccs();
}

void nes_apu_midi_get_env_cc(int8_t* cc_mode, int8_t* cc_decay,
                             int8_t* cc_loop, int8_t* cc_length)
{
    if (cc_mode) *cc_mode = s_cc_env_mode;
    if (cc_decay) *cc_decay = s_cc_env_decay;
    if (cc_loop) *cc_loop = s_cc_env_loop;
    if (cc_length) *cc_length = s_cc_length;
}

void nes_apu_midi_set_pb_default_range(uint8_t semis)
{
    if (semis > 24) semis = 24;
    if (s_pb_default_range == semis)
        return;
    const uint8_t old_default = s_pb_default_range;
    s_pb_default_range = semis;
    if (!s_enabled)
        return;

    /* Channels still on the old default follow the new one. */
    for (uint8_t chan = 0; chan < 16; ++chan) {
        midi_chan_state_t* cs = &s_chan[chan];
        if (cs->pb_range_semi != old_default || cs->pb_range_cents != 0)
            continue;
        cs->pb_range_semi = semis;
        retune_channel(chan);
    }
}

uint8_t nes_apu_midi_get_pb_default_range(void)
{
    return s_pb_default_range;
}

void nes_apu_midi_set_duty_cc(int8_t cc_p1, int8_t cc_p2)
{
    s_cc_p1_duty = cc_p1 < -1 ? -1 : cc_p1;
    s_cc_p2_duty = cc_p2 < -1 ? -1 : cc_p2;
    normalize_configurable_ccs();
}

void nes_apu_midi_get_duty_cc(int8_t* cc_p1, int8_t* cc_p2)
{
    if (cc_p1) *cc_p1 = s_cc_p1_duty;
    if (cc_p2) *cc_p2 = s_cc_p2_duty;
}

