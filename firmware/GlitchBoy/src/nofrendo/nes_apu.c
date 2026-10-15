/*
** Nofrendo (c) 1998-2000 Matthew Conte (matt@conte.com)
**
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of version 2 of the GNU Library General
** Public License as published by the Free Software Foundation.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** Library General Public License for more details.  To obtain a
** copy of the GNU Library General Public License, write to the Free
** Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** Any permitted reproduction of these routines, in whole or in part,
** must bear this legend.
**
**
** nes_apu.c
**
** NES APU emulation
** $Id: nes_apu.c,v 1.2 2001/04/27 14:37:11 neil Exp $
*/

#include "stdint.h"
#include "string.h"
#include "noftypes.h"
#include "log.h"
#include "nes_apu.h"
#include "nes_midi_pulse_tables.h"
#include "nes6502.h"
#include "../glitch/nes_glitch.h"
/* From mapper_shims.c; releases the DMC IRQ line */
extern void nes_irq_ack(void);


#define  APU_OVERSAMPLE
#define  APU_VOLUME_DECAY(x)  ((x) -= ((x) >> 7))

/* the following seem to be the correct (empirically determined)
** relative volumes between the sound channels
*/
#define  APU_RECTANGLE_OUTPUT(channel) (apu.rectangle[channel].output_vol)
#define  APU_TRIANGLE_OUTPUT           (apu.triangle.output_vol + (apu.triangle.output_vol >> 2))
#define  APU_NOISE_OUTPUT              ((apu.noise.output_vol + apu.noise.output_vol + apu.noise.output_vol) >> 2)
#define  APU_DMC_OUTPUT                ((apu.dmc.output_vol + apu.dmc.output_vol + apu.dmc.output_vol) >> 2)

/* active APU */
static apu_t apu;
static int s_apu_midi_override = 0;
static int s_apu_midi_internal_write = 0;
static int s_apu_midi_continuous_write = 0;
static int s_apu_midi_length_write = 0;
static uint16 s_apu_midi_gain_target_q8[4] = {0, 0, 0, 0};
static uint16 s_apu_midi_gain_current_q8[4] = {0, 0, 0, 0};
static int32 s_apu_prev_sample = 0;

/* look up table madness */
static int32 decay_lut[16];
static int vbl_lut[32];
static int trilength_lut[128];

/* noise lookups for both modes */
#ifndef REALTIME_NOISE
static int8 noise_long_lut[APU_NOISE_32K];
static int8 noise_short_lut[APU_NOISE_93];
#endif /* !REALTIME_NOISE */


/* vblank length table used for rectangles, triangle, noise */
static const uint8 vbl_length[32] =
{
    5, 127,
   10,   1,
   19,   2,
   40,   3,
   80,   4,
   30,   5,
    7,   6,
   13,   7,
    6,   8,
   12,   9,
   24,  10,
   48,  11,
   96,  12,
   36,  13,
    8,  14,
   16,  15
};

/* frequency limit of rectangle channels */
static const int freq_limit[8] =
{
   0x3FF, 0x555, 0x666, 0x71C, 0x787, 0x7C1, 0x7E0, 0x7F0
};

/* noise frequency lookup table */
static const int noise_freq[16] =
{
     4,    8,   16,   32,   64,   96,  128,  160,
   202,  254,  380,  508,  762, 1016, 2034, 4068
};

/* DMC transfer freqs (NTSC, CPU cycles per bit) */
const int dmc_clocks[16] =
{
   428, 380, 340, 320, 286, 254, 226, 214,
   190, 160, 142, 128, 106,  85,  72,  54
};

/* DMC transfer freqs (PAL, CPU cycles per bit) */
const int dmc_clocks_pal[16] =
{
   398, 354, 316, 298, 276, 236, 210, 198,
   176, 148, 132, 118,  98,  78,  66,  50
};

/* ratios of pos/neg pulse for rectangle waves */
static const int duty_flip[4] = { 2, 4, 8, 12 };

static uint32 s_apu_midi_rect_phase[2] = {0, 0};
static uint16 s_apu_midi_rect_cached_freq[2] = {0xFFFF, 0xFFFF};
static uint32 s_apu_midi_rect_phase_inc[2] = {0, 0};
static uint8 s_apu_midi_rect_table_low[2] = {0, 0};
static uint8 s_apu_midi_rect_table_high[2] = {0, 0};
static uint8 s_apu_midi_rect_table_mix[2] = {0, 0};
static int32 s_apu_midi_rect_amp_q8[2] = {0, 0};
static int32 s_apu_midi_tri_history[2] = {0, 0};
static uint8 s_apu_midi_tri_history_valid = 0;

static void apu_midi_reset_filter_history(void)
{
   s_apu_midi_rect_phase[0] = 0;
   s_apu_midi_rect_phase[1] = 0;
   s_apu_midi_rect_cached_freq[0] = 0xFFFF;
   s_apu_midi_rect_cached_freq[1] = 0xFFFF;
   s_apu_midi_rect_phase_inc[0] = 0;
   s_apu_midi_rect_phase_inc[1] = 0;
   s_apu_midi_rect_amp_q8[0] = 0;
   s_apu_midi_rect_amp_q8[1] = 0;
   s_apu_prev_sample = 0;
   memset(s_apu_midi_tri_history, 0, sizeof(s_apu_midi_tri_history));
   s_apu_midi_tri_history_valid = 0;
}

static int32 apu_midi_antialias(int32 sample, float timer_period,
                                int32 history[2], uint8 *valid)
{
   int32 filtered = sample;

   if (!*valid)
   {
      history[0] = sample;
      history[1] = sample;
      *valid = 1;
      return sample;
   }

   /*
   ** Above one timer edge per output sample, use a short binomial FIR.
   ** In the transition region use one additional box stage. Lower notes are
   ** left untouched so their normal NES edge and brightness are preserved.
   */
   if (timer_period <= apu.cycle_rate)
      filtered = (sample + (history[0] * 2) + history[1]) >> 2;
   else if (timer_period <= (apu.cycle_rate * 2.0f))
      filtered = (sample + history[0]) >> 1;

   history[1] = history[0];
   history[0] = sample;
   return filtered;
}

static inline int32 apu_midi_triangle_level(uint8 adder)
{
   const int phase = adder & 0x1F;
   const int level = (phase < 16) ? phase : (31 - phase);
   return (int32)((level * 2 - 15) * 256);
}

static inline int32 apu_midi_pulse_table_sample(uint8 duty_index,
                                                uint8 level,
                                                uint32 phase)
{
   const uint32 index = phase >> (32 - NES_MIDI_PULSE_TABLE_BITS);
   const uint32 next = (index + 1) & (NES_MIDI_PULSE_TABLE_SIZE - 1);
   const uint32 fraction =
      (phase >> (32 - NES_MIDI_PULSE_TABLE_BITS - 8)) & 0xFF;
   const int32 a = nes_midi_pulse_tables[duty_index][level][index];
   const int32 b = nes_midi_pulse_tables[duty_index][level][next];

   return a + (((b - a) * (int32)fraction) >> 8);
}

static void apu_midi_update_pulse_table(int chan, const rectangle_t *rect)
{
   const float period = (float)(rect->freq + 1);
   const float phase_step = apu.cycle_rate / (period * 16.0f);
   uint8 high = 0;
   uint8 low;
   uint8 mix;
   int level;

   if (s_apu_midi_rect_cached_freq[chan] == rect->freq &&
       s_apu_midi_rect_phase_inc[chan] != 0)
      return;

   s_apu_midi_rect_cached_freq[chan] = rect->freq;
   s_apu_midi_rect_phase_inc[chan] =
      (uint32)((double)phase_step * 4294967296.0 + 0.5);
   if (s_apu_midi_rect_phase_inc[chan] == 0)
      s_apu_midi_rect_phase_inc[chan] = 1;

   /*
   ** Choose the richest table whose highest partial remains below 0.49 of
   ** the sample rate. Fade that table out between 0.42 and 0.49 so pitch
   ** bends cross table boundaries without a timbral step.
   */
   for (level = 0; level < NES_MIDI_PULSE_TABLE_LEVELS; ++level)
   {
      if ((float)nes_midi_pulse_harmonics[level] * phase_step >= 0.49f)
         break;
      high = (uint8)level;
   }
   low = high ? (uint8)(high - 1) : high;
   if (high == 0)
   {
      mix = 255;
   }
   else
   {
      const float highest_partial =
         (float)nes_midi_pulse_harmonics[high] * phase_step;
      float blend = (0.49f - highest_partial) / 0.07f;
      if (blend < 0.0f)
         blend = 0.0f;
      if (blend > 1.0f)
         blend = 1.0f;
      mix = (uint8)(blend * 255.0f + 0.5f);
   }

   s_apu_midi_rect_table_low[chan] = low;
   s_apu_midi_rect_table_high[chan] = high;
   s_apu_midi_rect_table_mix[chan] = mix;
}

/*
** The MIDI pulse renderer uses precomputed, harmonic-limited wavetables.
** Unlike a first-order BLEP, the tables contain no partials above Nyquist,
** so a held note cannot produce a repeating folded alias pattern.
**
** The hardware envelope is stepped and its loop jumps directly from 0 to 15.
** Slew only the MIDI pulse gain over about two milliseconds so those
** control-rate discontinuities do not become clicks. Gameplay emulation is
** unchanged.
*/
static int32 apu_midi_rectangle_integrated(int chan)
{
   rectangle_t *rect = &apu.rectangle[chan];
   const float period = (float) (rect->freq + 1);
   uint32 phase = s_apu_midi_rect_phase[chan];
   const uint8 duty_index = (uint8)(rect->regs[0] >> 6) & 3;
   int32 low_sample;
   int32 high_sample;
   int32 sample;
   int32 target_amplitude;
   int32 target_amp_q8;
   int32 amp_delta;
   int32 amplitude;

   if (period <= 0.0f || apu.cycle_rate <= 0.0f)
      return APU_RECTANGLE_OUTPUT(chan);

   if (rect->fixed_envelope)
      target_amplitude = rect->volume << 8;
   else
      target_amplitude = (rect->env_vol ^ 0x0F) << 8;

   target_amp_q8 = target_amplitude << 8;
   amp_delta = target_amp_q8 - s_apu_midi_rect_amp_q8[chan];
   if (amp_delta > -64 && amp_delta < 64)
      s_apu_midi_rect_amp_q8[chan] = target_amp_q8;
   else
      s_apu_midi_rect_amp_q8[chan] += amp_delta / 64;
   amplitude = s_apu_midi_rect_amp_q8[chan] >> 8;

   apu_midi_update_pulse_table(chan, rect);
   low_sample = apu_midi_pulse_table_sample(
      duty_index, s_apu_midi_rect_table_low[chan], phase);
   high_sample = apu_midi_pulse_table_sample(
      duty_index, s_apu_midi_rect_table_high[chan], phase);
   sample = low_sample +
      (((high_sample - low_sample) *
        (int32)s_apu_midi_rect_table_mix[chan]) >> 8);

   phase += s_apu_midi_rect_phase_inc[chan];
   s_apu_midi_rect_phase[chan] = phase;
   rect->adder = (uint8)(phase >> 28) & 0x0F;
   rect->accum = period;
   rect->output_vol =
      (int32)(((int64_t)amplitude * sample) >>
              NES_MIDI_PULSE_TABLE_SCALE_BITS);
   return APU_RECTANGLE_OUTPUT(chan);
}

static int32 apu_midi_triangle_integrated(void)
{
   triangle_t *tri = &apu.triangle;
   const float period = (float) tri->freq;
   const float sample_cycles = apu.cycle_rate;
   float until_tick = tri->accum;
   float remaining = sample_cycles;
   float total = 0.0f;
   int32 averaged;

   if (period <= 0.0f || sample_cycles <= 0.0f)
      return APU_TRIANGLE_OUTPUT;

   if (until_tick <= 0.0f || until_tick > period)
      until_tick = period;

   while (remaining > 0.0f)
   {
      float span = (until_tick < remaining) ? until_tick : remaining;
      const int32 level = apu_midi_triangle_level(tri->adder);

      total += (float) level * span;
      remaining -= span;
      until_tick -= span;

      if (until_tick <= 0.0001f)
      {
         until_tick += period;
         tri->adder = (tri->adder + 1) & 0x1F;
      }
   }

   tri->accum = until_tick;
   averaged = (int32) (total / sample_cycles);
   averaged = apu_midi_antialias(averaged, period,
                                 s_apu_midi_tri_history,
                                 &s_apu_midi_tri_history_valid);
   return averaged + (averaged >> 2);
}


void apu_setcontext(apu_t *src_apu)
{
   apu = *src_apu;
}

void apu_getcontext(apu_t *dest_apu)
{
   *dest_apu = apu;
}

void apu_getdiag(uint32 *enable_reg, uint32 *mix_enable,
                 uint32 *sample_rate, uint32 *refresh_rate,
                 uint32 *base_freq_hz)
{
   if (enable_reg)
      *enable_reg = apu.enable_reg;
   if (mix_enable)
      *mix_enable = apu.mix_enable;
   if (sample_rate)
      *sample_rate = (uint32)apu.sample_rate;
   if (refresh_rate)
      *refresh_rate = (uint32)apu.refresh_rate;
   if (base_freq_hz)
      *base_freq_hz = (uint32)(apu.base_freq + 0.5);
}

void apu_set_midi_override(int enable)
{
   s_apu_midi_override = enable ? 1 : 0;
   if (s_apu_midi_override)
   {
      memset(s_apu_midi_gain_target_q8, 0,
             sizeof(s_apu_midi_gain_target_q8));
      memset(s_apu_midi_gain_current_q8, 0,
             sizeof(s_apu_midi_gain_current_q8));
   }
   apu_midi_reset_filter_history();
}

void apu_set_midi_voice_gain(int voice, uint8 gain)
{
   if (voice < 0 || voice >= 4)
      return;
   if (gain > 127)
      gain = 127;
   s_apu_midi_gain_target_q8[voice] = (uint16)gain << 8;
}

static inline int32 apu_midi_apply_voice_gain(int voice, int32 sample)
{
   int32 current = s_apu_midi_gain_current_q8[voice];
   const int32 target = s_apu_midi_gain_target_q8[voice];
   const int32 delta = target - current;
   if (delta > -64 && delta < 64)
      current = target;
   else
      current += delta / 64;
   s_apu_midi_gain_current_q8[voice] = (uint16)current;
   return (int32)(((int64_t)sample * current) / (127 << 8));
}

double apu_get_base_freq(void)
{
   return apu.base_freq;
}

void apu_setchan(int chan, bool enabled)
{
   if (enabled)
      apu.mix_enable |= (1 << chan);
   else
      apu.mix_enable &= ~(1 << chan);
}

/* emulation of the 15-bit shift register the
** NES uses to generate pseudo-random series
** for the white noise channel
*/
#ifdef REALTIME_NOISE
INLINE int8 shift_register15(uint8 xor_tap)
{
   static int sreg = 0x4000;
   int bit0, tap, bit14;

   bit0 = sreg & 1;
   tap = (sreg & xor_tap) ? 1 : 0;
   bit14 = (bit0 ^ tap);
   sreg >>= 1;
   sreg |= (bit14 << 14);
   return (bit0 ^ 1);
}
#else /* !REALTIME_NOISE */
static void shift_register15(int8 *buf, int count)
{
   static int sreg = 0x4000;
   int bit0, bit1, bit6, bit14;

   if (count == APU_NOISE_93)
   {
      while (count--)
      {
         bit0 = sreg & 1;
         bit6 = (sreg & 0x40) >> 6;
         bit14 = (bit0 ^ bit6);
         sreg >>= 1;
         sreg |= (bit14 << 14);
         *buf++ = bit0 ^ 1;
      }
   }
   else /* 32K noise */
   {
      while (count--)
      {
         bit0 = sreg & 1;
         bit1 = (sreg & 2) >> 1;
         bit14 = (bit0 ^ bit1);
         sreg >>= 1;
         sreg |= (bit14 << 14);
         *buf++ = bit0 ^ 1;
      }
   }
}
#endif /* !REALTIME_NOISE */

/* RECTANGLE WAVE
** ==============
** reg0: 0-3=volume, 4=envelope, 5=hold, 6-7=duty cycle
** reg1: 0-2=sweep shifts, 3=sweep inc/dec, 4-6=sweep length, 7=sweep on
** reg2: 8 bits of freq
** reg3: 0-2=high freq, 7-4=vbl length counter
*/
#ifdef APU_OVERSAMPLE

#define  APU_MAKE_RECTANGLE(ch) \
static int32 apu_rectangle_##ch(void) \
{ \
   int32 output, total; \
   int num_times; \
\
   APU_VOLUME_DECAY(apu.rectangle[ch].output_vol); \
\
   if (false == apu.rectangle[ch].enabled \
       || (0 == apu.rectangle[ch].vbl_length && \
           (!s_apu_midi_override || \
            false == apu.rectangle[ch].holdnote))) \
      return APU_RECTANGLE_OUTPUT(ch); \
\
   /* vbl length counter */ \
   if (false == apu.rectangle[ch].holdnote && \
       apu.rectangle[ch].vbl_length) \
      apu.rectangle[ch].vbl_length--; \
\
   /* envelope decay at a rate of (env_delay + 1) / 240 secs */ \
   apu.rectangle[ch].env_phase -= 4; /* 240/60 */ \
   while (apu.rectangle[ch].env_phase < 0) \
   { \
      apu.rectangle[ch].env_phase += apu.rectangle[ch].env_delay; \
\
      if (apu.rectangle[ch].holdnote) \
         apu.rectangle[ch].env_vol = (apu.rectangle[ch].env_vol + 1) & 0x0F; \
      else if (apu.rectangle[ch].env_vol < 0x0F) \
         apu.rectangle[ch].env_vol++; \
   } \
\
   /* TODO: find true relation of freq_limit to register values */ \
   if (apu.rectangle[ch].freq < 8 \
       || (false == apu.rectangle[ch].sweep_inc \
           && apu.rectangle[ch].freq > apu.rectangle[ch].freq_limit)) \
      return APU_RECTANGLE_OUTPUT(ch); \
\
   /* frequency sweeping at a rate of (sweep_delay + 1) / 120 secs */ \
   if (apu.rectangle[ch].sweep_on && apu.rectangle[ch].sweep_shifts) \
   { \
      apu.rectangle[ch].sweep_phase -= 2; /* 120/60 */ \
      while (apu.rectangle[ch].sweep_phase < 0) \
      { \
         apu.rectangle[ch].sweep_phase += apu.rectangle[ch].sweep_delay; \
\
         if (apu.rectangle[ch].sweep_inc) /* ramp up */ \
         { \
            if (0 == ch) \
               apu.rectangle[ch].freq += ~(apu.rectangle[ch].freq >> apu.rectangle[ch].sweep_shifts); \
            else \
               apu.rectangle[ch].freq -= (apu.rectangle[ch].freq >> apu.rectangle[ch].sweep_shifts); \
         } \
         else /* ramp down */ \
         { \
            apu.rectangle[ch].freq += (apu.rectangle[ch].freq >> apu.rectangle[ch].sweep_shifts); \
         } \
      } \
   } \
\
   if (s_apu_midi_override) \
      return apu_midi_rectangle_integrated(ch); \
\
   apu.rectangle[ch].accum -= apu.cycle_rate; \
   if (apu.rectangle[ch].accum >= 0) \
      return APU_RECTANGLE_OUTPUT(ch); \
\
   if (apu.rectangle[ch].fixed_envelope) \
      output = apu.rectangle[ch].volume << 8; /* fixed volume */ \
   else \
      output = (apu.rectangle[ch].env_vol ^ 0x0F) << 8; \
\
   num_times = total = 0; \
\
   while (apu.rectangle[ch].accum < 0) \
   { \
      apu.rectangle[ch].accum += apu.rectangle[ch].freq + 1; \
      apu.rectangle[ch].adder = (apu.rectangle[ch].adder + 1) & 0x0F; \
\
      if (apu.rectangle[ch].adder < apu.rectangle[ch].duty_flip) \
         total += output; \
      else \
         total -= output; \
\
      num_times++; \
   } \
\
   apu.rectangle[ch].output_vol = total / num_times; \
   return APU_RECTANGLE_OUTPUT(ch); \
}

#else /* !APU_OVERSAMPLE */
#define  APU_MAKE_RECTANGLE(ch) \
static int32 apu_rectangle_##ch(void) \
{ \
   int32 output; \
\
   APU_VOLUME_DECAY(apu.rectangle[ch].output_vol); \
\
   if (false == apu.rectangle[ch].enabled \
       || (0 == apu.rectangle[ch].vbl_length && \
           (!s_apu_midi_override || \
            false == apu.rectangle[ch].holdnote))) \
      return APU_RECTANGLE_OUTPUT(ch); \
\
   /* vbl length counter */ \
   if (false == apu.rectangle[ch].holdnote && \
       apu.rectangle[ch].vbl_length) \
      apu.rectangle[ch].vbl_length--; \
\
   /* envelope decay at a rate of (env_delay + 1) / 240 secs */ \
   apu.rectangle[ch].env_phase -= 4; /* 240/60 */ \
   while (apu.rectangle[ch].env_phase < 0) \
   { \
      apu.rectangle[ch].env_phase += apu.rectangle[ch].env_delay; \
\
      if (apu.rectangle[ch].holdnote) \
         apu.rectangle[ch].env_vol = (apu.rectangle[ch].env_vol + 1) & 0x0F; \
      else if (apu.rectangle[ch].env_vol < 0x0F) \
         apu.rectangle[ch].env_vol++; \
   } \
\
   /* TODO: find true relation of freq_limit to register values */ \
   if (apu.rectangle[ch].freq < 8 || (false == apu.rectangle[ch].sweep_inc && apu.rectangle[ch].freq > apu.rectangle[ch].freq_limit)) \
      return APU_RECTANGLE_OUTPUT(ch); \
\
   /* frequency sweeping at a rate of (sweep_delay + 1) / 120 secs */ \
   if (apu.rectangle[ch].sweep_on && apu.rectangle[ch].sweep_shifts) \
   { \
      apu.rectangle[ch].sweep_phase -= 2; /* 120/60 */ \
      while (apu.rectangle[ch].sweep_phase < 0) \
      { \
         apu.rectangle[ch].sweep_phase += apu.rectangle[ch].sweep_delay; \
\
         if (apu.rectangle[ch].sweep_inc) /* ramp up */ \
         { \
            if (0 == ch) \
               apu.rectangle[ch].freq += ~(apu.rectangle[ch].freq >> apu.rectangle[ch].sweep_shifts); \
            else \
               apu.rectangle[ch].freq -= (apu.rectangle[ch].freq >> apu.rectangle[ch].sweep_shifts); \
         } \
         else /* ramp down */ \
         { \
            apu.rectangle[ch].freq += (apu.rectangle[ch].freq >> apu.rectangle[ch].sweep_shifts); \
         } \
      } \
   } \
\
   apu.rectangle[ch].accum -= apu.cycle_rate; \
   if (apu.rectangle[ch].accum >= 0) \
      return APU_RECTANGLE_OUTPUT(ch); \
\
   while (apu.rectangle[ch].accum < 0) \
   { \
      apu.rectangle[ch].accum += (apu.rectangle[ch].freq + 1); \
      apu.rectangle[ch].adder = (apu.rectangle[ch].adder + 1) & 0x0F; \
   } \
\
   if (apu.rectangle[ch].fixed_envelope) \
      output = apu.rectangle[ch].volume << 8; /* fixed volume */ \
   else \
      output = (apu.rectangle[ch].env_vol ^ 0x0F) << 8; \
\
   if (0 == apu.rectangle[ch].adder) \
      apu.rectangle[ch].output_vol = output; \
   else if (apu.rectangle[ch].adder == apu.rectangle[ch].duty_flip) \
      apu.rectangle[ch].output_vol = -output; \
\
   return APU_RECTANGLE_OUTPUT(ch); \
}

#endif /* !APU_OVERSAMPLE */

/* generate the functions */
APU_MAKE_RECTANGLE(0)
APU_MAKE_RECTANGLE(1)


/* TRIANGLE WAVE
** =============
** reg0: 7=holdnote, 6-0=linear length counter
** reg2: low 8 bits of frequency
** reg3: 7-3=length counter, 2-0=high 3 bits of frequency
*/
static int32 apu_triangle(void)
{
   APU_VOLUME_DECAY(apu.triangle.output_vol);

   if (false == apu.triangle.enabled ||
       (0 == apu.triangle.vbl_length &&
        (!s_apu_midi_override || false == apu.triangle.holdnote)))
      return APU_TRIANGLE_OUTPUT;

   if (!s_apu_midi_override && apu.triangle.counter_started)
   {
      if (apu.triangle.linear_length > 0)
         apu.triangle.linear_length--;
      if (apu.triangle.vbl_length && false == apu.triangle.holdnote)
         apu.triangle.vbl_length--;
   }
   else if (!s_apu_midi_override &&
            false == apu.triangle.holdnote &&
            apu.triangle.write_latency)
   {
      if (--apu.triangle.write_latency == 0)
         apu.triangle.counter_started = true;
   }

   if (s_apu_midi_override && false == apu.triangle.holdnote &&
       apu.triangle.counter_started)
   {
      if (apu.triangle.linear_length)
         apu.triangle.linear_length--;
      if (apu.triangle.vbl_length)
         apu.triangle.vbl_length--;
   }

   if (((!s_apu_midi_override ||
         false == apu.triangle.holdnote) &&
        0 == apu.triangle.linear_length) ||
       apu.triangle.freq < 4) /* inaudible */
      return APU_TRIANGLE_OUTPUT;

   if (s_apu_midi_override)
      return apu_midi_triangle_integrated();

   apu.triangle.accum -= apu.cycle_rate; \
   while (apu.triangle.accum < 0)
   {
      apu.triangle.accum += apu.triangle.freq;
      apu.triangle.adder = (apu.triangle.adder + 1) & 0x1F;

      if (apu.triangle.adder & 0x10)
         apu.triangle.output_vol -= (2 << 8);
      else
         apu.triangle.output_vol += (2 << 8);
   }

   return APU_TRIANGLE_OUTPUT;
}


/* WHITE NOISE CHANNEL
** ===================
** reg0: 0-3=volume, 4=envelope, 5=hold
** reg2: 7=small(93 byte) sample,3-0=freq lookup
** reg3: 7-4=vbl length counter
*/
/* TODO: AAAAAAAAAAAAAAAAAAAAAAAA!  #ifdef MADNESS! */
static int32 apu_noise(void)
{
   int32 outvol;

#if defined(APU_OVERSAMPLE) && defined(REALTIME_NOISE)
#else /* !(APU_OVERSAMPLE && REALTIME_NOISE) */
   int32 noise_bit;
#endif /* !(APU_OVERSAMPLE && REALTIME_NOISE) */
#ifdef APU_OVERSAMPLE
   int num_times;
   int32 total;
#endif /* APU_OVERSAMPLE */

   APU_VOLUME_DECAY(apu.noise.output_vol);

   if (false == apu.noise.enabled ||
       (0 == apu.noise.vbl_length &&
        (!s_apu_midi_override || false == apu.noise.holdnote)))
      return APU_NOISE_OUTPUT;

   /* vbl length counter */
   if (false == apu.noise.holdnote && apu.noise.vbl_length)
      apu.noise.vbl_length--;

   /* envelope decay at a rate of (env_delay + 1) / 240 secs */
   apu.noise.env_phase -= 4; /* 240/60 */
   while (apu.noise.env_phase < 0)
   {
      apu.noise.env_phase += apu.noise.env_delay;

      if (apu.noise.holdnote)
         apu.noise.env_vol = (apu.noise.env_vol + 1) & 0x0F;
      else if (apu.noise.env_vol < 0x0F)
         apu.noise.env_vol++;
   }

   apu.noise.accum -= apu.cycle_rate;
   if (apu.noise.accum >= 0)
      return APU_NOISE_OUTPUT;

#ifdef APU_OVERSAMPLE
   if (apu.noise.fixed_envelope)
      outvol = apu.noise.volume << 8; /* fixed volume */
   else
      outvol = (apu.noise.env_vol ^ 0x0F) << 8;

   num_times = total = 0;
#endif /* APU_OVERSAMPLE */

   while (apu.noise.accum < 0)
   {
      apu.noise.accum += apu.noise.freq;

#ifdef REALTIME_NOISE

#ifdef APU_OVERSAMPLE
      if (shift_register15(apu.noise.xor_tap))
         total += outvol;
      else
         total -= outvol;

      num_times++;
#else /* !APU_OVERSAMPLE */
      noise_bit = shift_register15(apu.noise.xor_tap);
#endif /* !APU_OVERSAMPLE */

#else /* !REALTIME_NOISE */
      apu.noise.cur_pos++;

      if (apu.noise.short_sample)
      {
         if (APU_NOISE_93 == apu.noise.cur_pos)
            apu.noise.cur_pos = 0;
      }
      else
      {
         if (APU_NOISE_32K == apu.noise.cur_pos)
            apu.noise.cur_pos = 0;
      }

#ifdef APU_OVERSAMPLE
      if (apu.noise.short_sample)
         noise_bit = noise_short_lut[apu.noise.cur_pos];
      else
         noise_bit = noise_long_lut[apu.noise.cur_pos];

      if (noise_bit)
         total += outvol;
      else
         total -= outvol;

      num_times++;
#endif /* APU_OVERSAMPLE */
#endif /* !REALTIME_NOISE */
   }

#ifdef APU_OVERSAMPLE
   apu.noise.output_vol = total / num_times;
#else /* !APU_OVERSAMPLE */
   if (apu.noise.fixed_envelope)
      outvol = apu.noise.volume << 8; /* fixed volume */
   else
      outvol = (apu.noise.env_vol ^ 0x0F) << 8;

#ifndef REALTIME_NOISE
   if (apu.noise.short_sample)
      noise_bit = noise_short_lut[apu.noise.cur_pos];
   else
      noise_bit = noise_long_lut[apu.noise.cur_pos];
#endif /* !REALTIME_NOISE */

   if (noise_bit)
      apu.noise.output_vol = outvol;
   else
      apu.noise.output_vol = -outvol;
#endif /* !APU_OVERSAMPLE */

   return APU_NOISE_OUTPUT;
}


INLINE void apu_dmcreload(void)
{
   apu.dmc.address = apu.dmc.cached_addr;
   apu.dmc.dma_length = apu.dmc.cached_dmalength;
   apu.dmc.irq_occurred = false;
}

/* DELTA MODULATION CHANNEL
** =========================
** reg0: 7=irq gen, 6=looping, 3-0=pointer to clock table
** reg1: output dc level, 6 bits unsigned
** reg2: 8 bits of 64-byte aligned address offset : $C000 + (value * 64)
** reg3: length, (value * 16) + 1
*/
static int32 apu_dmc(void)
{
   /* NES DMC outputs bits LSB-first from each fetched byte. */
   APU_VOLUME_DECAY(apu.dmc.output_vol);

   /* only process when channel is alive */
   if (apu.dmc.dma_length)
   {
      apu.dmc.accum -= apu.cycle_rate;

      while (apu.dmc.accum < 0)
      {
         apu.dmc.accum += apu.dmc.freq;

         /* Compute bit index 0..7 within the current byte (LSB first). */
         int bit_index = 7 - ((apu.dmc.dma_length - 1) & 7);

         /* At the start of each 8-bit group, fetch the next byte. */
         if (0 == bit_index)
         {
            apu.dmc.cur_byte = nes6502_getbyte(apu.dmc.address);

            /* DMC sample fetch stalls CPU for 4 cycles (accurate) */
            nes6502_burn(4);

            /* prevent wraparound */
            if (0xFFFF == apu.dmc.address)
               apu.dmc.address = 0x8000;
            else
               apu.dmc.address++;
         }

         if (--apu.dmc.dma_length == 0)
         {
            /* if loop bit set, we're cool to retrigger sample */
            if (apu.dmc.looping)
            {
               apu_dmcreload();
            }
            else
            {
               /* check to see if we should generate an irq */
               if (apu.dmc.irq_gen)
               {
                  apu.dmc.irq_occurred = true;
                  if (apu.irq_callback)
                     apu.irq_callback();
               }

               /* bodge for timestamp queue */
               apu.dmc.enabled = false;
               break;
            }
         }

         /* positive delta */
         if (apu.dmc.cur_byte & (1 << bit_index))
         {
            if (apu.dmc.regs[1] < 0x7D)
            {
               apu.dmc.regs[1] += 2;
               apu.dmc.output_vol += (2 << 8);
            }
         }
         /* negative delta */
         else
         {
            if (apu.dmc.regs[1] > 1)
            {
               apu.dmc.regs[1] -= 2;
               apu.dmc.output_vol -= (2 << 8);
            }
         }
      }
   }

   return APU_DMC_OUTPUT;
}


void apu_write(uint32 address, uint8 value)
{
   int chan;

   /* When MIDI override is enabled, ignore CPU-driven writes. */
   if (s_apu_midi_override && !s_apu_midi_internal_write)
      return;

   switch (address)
   {
   /* rectangles */
   case APU_WRA0:
   case APU_WRB0:
      chan = (address & 4) >> 2;
      apu.rectangle[chan].regs[0] = value;
      apu.rectangle[chan].volume = value & 0x0F;
      apu.rectangle[chan].env_delay = decay_lut[value & 0x0F];
      apu.rectangle[chan].holdnote = (value & 0x20) ? true : false;
      apu.rectangle[chan].fixed_envelope = (value & 0x10) ? true : false;
      apu.rectangle[chan].duty_flip = duty_flip[value >> 6];
      break;

   case APU_WRA1:
   case APU_WRB1:
      chan = (address & 4) >> 2;
      apu.rectangle[chan].regs[1] = value;
      apu.rectangle[chan].sweep_on = (value & 0x80) ? true : false;
      apu.rectangle[chan].sweep_shifts = value & 7;
      apu.rectangle[chan].sweep_delay = decay_lut[(value >> 4) & 7];
      apu.rectangle[chan].sweep_inc = (value & 0x08) ? true : false;
      apu.rectangle[chan].freq_limit = freq_limit[value & 7];
      if (s_apu_midi_override)
         apu.rectangle[chan].sweep_phase = apu.rectangle[chan].sweep_delay;
      break;

   case APU_WRA2:
   case APU_WRB2:
      chan = (address & 4) >> 2;
      apu.rectangle[chan].regs[2] = value;
      apu.rectangle[chan].freq = (apu.rectangle[chan].freq & ~0xFF) | value;
      break;

   case APU_WRA3:
   case APU_WRB3:
      chan = (address & 4) >> 2;
      apu.rectangle[chan].regs[3] = value;
      apu.rectangle[chan].freq = ((value & 7) << 8) | (apu.rectangle[chan].freq & 0xFF);
      if (!s_apu_midi_continuous_write)
      {
         apu.rectangle[chan].vbl_length = vbl_lut[value >> 3];
         if (!s_apu_midi_length_write)
         {
            apu.rectangle[chan].env_vol = 0;
            apu.rectangle[chan].env_phase = apu.rectangle[chan].env_delay;
            apu.rectangle[chan].adder = 0;
            if (s_apu_midi_override)
            {
               s_apu_midi_rect_phase[chan] = 0;
               s_apu_midi_rect_amp_q8[chan] = 0;
               apu.rectangle[chan].output_vol = 0;
            }
         }
      }
      break;

   /* triangle */
   case APU_WRC0:
      apu.triangle.regs[0] = value;
      apu.triangle.holdnote = (value & 0x80) ? true : false;

      if (s_apu_midi_override)
         apu.triangle.linear_length = trilength_lut[value & 0x7F];
      else if (false == apu.triangle.counter_started &&
               apu.triangle.vbl_length)
         apu.triangle.linear_length = trilength_lut[value & 0x7F];

      break;

   case APU_WRC2:
      apu.triangle.regs[1] = value;
      apu.triangle.freq = (((apu.triangle.regs[2] & 7) << 8) + value) + 1;
      break;

   case APU_WRC3:

      apu.triangle.regs[2] = value;
      apu.triangle.freq = (((value & 7) << 8) + apu.triangle.regs[1]) + 1;
      if (!s_apu_midi_continuous_write)
      {
         /* this is somewhat of a hack.  there appears to be some latency on
         ** the Real Thing between when trireg0 is written to and when the
         ** linear length counter actually begins its countdown.  we want to
         ** prevent the case where the program writes to the freq regs first,
         ** then to reg 0, and the counter accidentally starts running because
         ** of the sound queue's timestamp processing.
         **
         ** set latency to a couple hundred cycles -- should be plenty of time
         ** for the 6502 code to do a couple of table dereferences and load up
         ** the other triregs
         */
         apu.triangle.write_latency = (int) (228 / apu.cycle_rate);
         apu.triangle.vbl_length = vbl_lut[value >> 3];
         if (!s_apu_midi_length_write)
         {
            apu.triangle.counter_started = false;
            apu.triangle.linear_length = trilength_lut[apu.triangle.regs[0] & 0x7F];
            if (s_apu_midi_override)
            {
               apu.triangle.counter_started = true;
               s_apu_midi_tri_history_valid = 0;
            }
         }
      }
      break;

   /* noise */
   case APU_WRD0:
      apu.noise.regs[0] = value;
      apu.noise.env_delay = decay_lut[value & 0x0F];
      apu.noise.holdnote = (value & 0x20) ? true : false;
      apu.noise.fixed_envelope = (value & 0x10) ? true : false;
      apu.noise.volume = value & 0x0F;
      break;

   case APU_WRD2:
      apu.noise.regs[1] = value;
      apu.noise.freq = noise_freq[value & 0x0F];

#ifdef REALTIME_NOISE
      apu.noise.xor_tap = (value & 0x80) ? 0x40: 0x02;
#else /* !REALTIME_NOISE */
      /* detect transition from long->short sample */
      if ((value & 0x80) && false == apu.noise.short_sample)
      {
         /* recalculate short noise buffer */
         shift_register15(noise_short_lut, APU_NOISE_93);
         apu.noise.cur_pos = 0;
      }
      apu.noise.short_sample = (value & 0x80) ? true : false;
#endif /* !REALTIME_NOISE */
      break;

   case APU_WRD3:
      apu.noise.regs[2] = value;
      apu.noise.vbl_length = vbl_lut[value >> 3];
      if (!s_apu_midi_length_write)
      {
         apu.noise.env_vol = 0; /* reset envelope */
         if (s_apu_midi_override)
            apu.noise.env_phase = apu.noise.env_delay;
      }
      break;

   /* DMC */
   case APU_WRE0:
      apu.dmc.regs[0] = value;
      /* Select region-appropriate DMC period table */
      apu.dmc.freq = (apu.refresh_rate == 50)
                        ? dmc_clocks_pal[value & 0x0F]
                        : dmc_clocks[value & 0x0F];
      apu.dmc.looping = (value & 0x40) ? true : false;

      if (value & 0x80)
      {
         apu.dmc.irq_gen = true;
      }
      else
      {
         apu.dmc.irq_gen = false;
         apu.dmc.irq_occurred = false;
      }
      break;

   case APU_WRE1: /* 7-bit DAC */
      /* add the _delta_ between written value and
      ** current output level of the volume reg
      */
      value &= 0x7F; /* bit 7 ignored */
      apu.dmc.output_vol += ((value - apu.dmc.regs[1]) << 8);
      apu.dmc.regs[1] = value;
      break;

   case APU_WRE2:
      apu.dmc.regs[2] = value;
      apu.dmc.cached_addr = 0xC000 + (uint16) (value << 6);
      break;

   case APU_WRE3:
      apu.dmc.regs[3] = value;
      apu.dmc.cached_dmalength = ((value << 4) + 1) << 3;
      break;

   case APU_SMASK:
      /* bodge for timestamp queue */
      apu.dmc.enabled = (value & 0x10) ? true : false;
      apu.enable_reg = value;

      for (chan = 0; chan < 2; chan++)
      {
         if (value & (1 << chan))
         {
            apu.rectangle[chan].enabled = true;
         }
         else
         {
            apu.rectangle[chan].enabled = false;
            apu.rectangle[chan].vbl_length = 0;
            if (s_apu_midi_override)
            {
               apu.rectangle[chan].output_vol = 0;
               s_apu_midi_rect_amp_q8[chan] = 0;
               s_apu_midi_gain_target_q8[chan] = 0;
               s_apu_midi_gain_current_q8[chan] = 0;
            }
         }
      }

      if (value & 0x04)
      {
         apu.triangle.enabled = true;
      }
      else
      {
         apu.triangle.enabled = false;
         apu.triangle.vbl_length = 0;
         apu.triangle.linear_length = 0;
         apu.triangle.counter_started = false;
         apu.triangle.write_latency = 0;
         if (s_apu_midi_override)
         {
            apu.triangle.output_vol = 0;
            s_apu_midi_gain_target_q8[2] = 0;
            s_apu_midi_gain_current_q8[2] = 0;
         }
      }

      if (value & 0x08)
      {
         apu.noise.enabled = true;
      }
      else
      {
         apu.noise.enabled = false;
         apu.noise.vbl_length = 0;
         if (s_apu_midi_override)
         {
            apu.noise.output_vol = 0;
            s_apu_midi_gain_target_q8[3] = 0;
            s_apu_midi_gain_current_q8[3] = 0;
         }
      }

      if (value & 0x10)
      {
         if (0 == apu.dmc.dma_length)
            apu_dmcreload();
      }
      else
      {
         apu.dmc.dma_length = 0;
      }

      apu.dmc.irq_occurred = false;
      break;

      /* unused, but they get hit in some mem-clear loops */
   case 0x4009:
   case 0x400D:
      break;

   default:
      break;
   }
}

void apu_midi_write(uint32 address, uint8 value)
{
   /* MIDI writes bypass the 6502 hook, so apply the glitch here. $4015 is
      left alone so note-off masks stay reliable. */
   if (address != APU_SMASK && nes_glitch_apply_cpu_write(address, &value))
      return;

   /* allow internal writes from MIDI bridge even when override is on */
   s_apu_midi_internal_write = 1;
   apu_write(address, value);
   s_apu_midi_internal_write = 0;
}

void apu_midi_write_continuous(uint32 address, uint8 value)
{
   /* Continuous retunes use the same glitch hook without restarting voice state. */
   if (address != APU_SMASK && nes_glitch_apply_cpu_write(address, &value))
      return;

   s_apu_midi_internal_write = 1;
   s_apu_midi_continuous_write = 1;
   apu_write(address, value);
   s_apu_midi_continuous_write = 0;
   s_apu_midi_internal_write = 0;
}

void apu_midi_write_length(uint32 address, uint8 value)
{
   if (address != APU_SMASK && nes_glitch_apply_cpu_write(address, &value))
      return;

   s_apu_midi_internal_write = 1;
   s_apu_midi_length_write = 1;
   apu_write(address, value);
   s_apu_midi_length_write = 0;
   s_apu_midi_internal_write = 0;
}

/* Read from $4000-$4017 */
uint8 apu_read(uint32 address)
{
   uint8 value;

   switch (address)
   {
   case APU_SMASK:
      value = 0;
      /* Return 1 in 0-5 bit pos if a channel is playing */
      if (apu.rectangle[0].enabled && apu.rectangle[0].vbl_length)
         value |= 0x01;
      if (apu.rectangle[1].enabled && apu.rectangle[1].vbl_length)
         value |= 0x02;
      if (apu.triangle.enabled && apu.triangle.vbl_length)
         value |= 0x04;
      if (apu.noise.enabled && apu.noise.vbl_length)
         value |= 0x08;

      /* bodge for timestamp queue */
      if (apu.dmc.enabled)
         value |= 0x10;

      /* DMC IRQ flag: reading $4015 returns it and clears it */
      if (apu.dmc.irq_occurred)
      {
         value |= 0x80;
         apu.dmc.irq_occurred = false;
         /* Also deassert CPU IRQ line raised by DMC */
         nes_irq_ack();
      }

      if (apu.irqclear_callback)
         value |= apu.irqclear_callback();

      break;

   default:
      value = (address >> 8); /* heavy capacitance on data bus */
      break;
   }

   return value;
}

#define CLIP_OUTPUT16(out) \
{ \
   /*out <<= 1;*/ \
   if (out > 0x7FFF) \
      out = 0x7FFF; \
   else if (out < -0x8000) \
      out = -0x8000; \
}

void apu_process(void *buffer, int num_samples)
{
   int16 *buf16;
   uint8 *buf8;

   if (NULL != buffer)
   {
      /* Cache typed output-buffer views. */
      apu.buffer = buffer;

      buf16 = (int16 *) buffer;
      buf8 = (uint8 *) buffer;

      while (num_samples--)
      {
         int32 next_sample, accum = 0;

         if (apu.mix_enable & 0x01)
         {
            int32 voice = apu_rectangle_0();
            accum += s_apu_midi_override
               ? apu_midi_apply_voice_gain(0, voice) : voice;
         }
         if (apu.mix_enable & 0x02)
         {
            int32 voice = apu_rectangle_1();
            accum += s_apu_midi_override
               ? apu_midi_apply_voice_gain(1, voice) : voice;
         }
         if (apu.mix_enable & 0x04)
         {
            int32 voice = apu_triangle();
            accum += s_apu_midi_override
               ? apu_midi_apply_voice_gain(2, voice) : voice;
         }
         if (apu.mix_enable & 0x08)
         {
            int32 voice = apu_noise();
            accum += s_apu_midi_override
               ? apu_midi_apply_voice_gain(3, voice) : voice;
         }
         if (apu.mix_enable & 0x10)
            accum += apu_dmc();
         if (!s_apu_midi_override && apu.ext && (apu.mix_enable & 0x20))
            accum += apu.ext->process();

         /* do any filtering */
         if (APU_FILTER_NONE != apu.filter_type)
         {
            next_sample = accum;

            if (APU_FILTER_LOWPASS == apu.filter_type)
            {
               accum += s_apu_prev_sample;
               accum >>= 1;
            }
            else
               accum = (accum + accum + accum + s_apu_prev_sample) >> 2;

            s_apu_prev_sample = next_sample;
         }

         /* do clipping */
         CLIP_OUTPUT16(accum);

         /* signed 16-bit output, unsigned 8-bit */
         if (16 == apu.sample_bits)
            *buf16++ = (int16) accum;
         else
            *buf8++ = (accum >> 8) ^ 0x80;
      }
   }
}

/* set the filter type */
void apu_setfilter(int filter_type)
{
   apu.filter_type = filter_type;
}

void apu_reset(void)
{
   uint32 address;

   /* Allow reset writes even if MIDI override is enabled */
   int prev_internal = s_apu_midi_internal_write;
   s_apu_midi_internal_write = 1;

   /* initialize all channel members */
   for (address = 0x4000; address <= 0x4013; address++)
      apu_write(address, 0);

   apu_write(0x4015, 0);

   if (apu.ext && NULL != apu.ext->reset)
      apu.ext->reset();

   s_apu_midi_internal_write = prev_internal;
   s_apu_prev_sample = 0;
}

void apu_build_luts(int num_samples)
{
   int i;

   /* lut used for enveloping and frequency sweeps */
   for (i = 0; i < 16; i++)
      decay_lut[i] = num_samples * (i + 1);

   /* used for note length, based on vblanks and size of audio buffer */
   for (i = 0; i < 32; i++)
      vbl_lut[i] = vbl_length[i] * num_samples;

   /* triangle wave channel's linear length table */
   for (i = 0; i < 128; i++)
      trilength_lut[i] = (int) (0.25 * i * num_samples);

#ifndef REALTIME_NOISE
   /* generate noise samples */
   shift_register15(noise_long_lut, APU_NOISE_32K);
   shift_register15(noise_short_lut, APU_NOISE_93);
#endif /* !REALTIME_NOISE */
}

void apu_setparams(double base_freq, int sample_rate, int refresh_rate, int sample_bits)
{
   apu.sample_rate = sample_rate;
   apu.refresh_rate = refresh_rate;
   apu.sample_bits = sample_bits;
   apu.num_samples = sample_rate / refresh_rate;
   if (0 == base_freq) {
      /* Choose default CPU base frequency based on TV system */
      apu.base_freq = (refresh_rate == 50) ? 1662607.125 : APU_BASEFREQ; /* PAL vs NTSC */
   } else
      apu.base_freq = base_freq;
   apu.cycle_rate = (float) (apu.base_freq / sample_rate);

   /* build various lookup tables for apu */
   apu_build_luts(apu.num_samples);

   apu_reset();
}

/* Initializes emulated sound hardware, creates waveforms/voices */
apu_t *apu_create(double base_freq, int sample_rate, int refresh_rate, int sample_bits)
{
   apu_t *temp_apu;
   int channel;

   temp_apu = &apu;

   memset(temp_apu, 0, sizeof(apu_t));

   /* set the update routine */
   temp_apu->process = apu_process;
   temp_apu->ext = NULL;

   /* clear the callbacks */
   temp_apu->irq_callback = NULL;
   temp_apu->irqclear_callback = NULL;

   apu_setcontext(temp_apu);

   apu_setparams(base_freq, sample_rate, refresh_rate, sample_bits);

   for (channel = 0; channel < 6; channel++)
      apu_setchan(channel, true);

   apu_setfilter(APU_FILTER_WEIGHTED);

   apu_getcontext(temp_apu);

   return temp_apu;
}

void apu_destroy(apu_t **src_apu)
{
   if (*src_apu)
   {
      if ((*src_apu)->ext && NULL != (*src_apu)->ext->shutdown)
         (*src_apu)->ext->shutdown();
      if (*src_apu != &apu)
         free(*src_apu);
      else
         memset(&apu, 0, sizeof(apu));
      *src_apu = NULL;
   }
}

void apu_setext(apu_t *src_apu, apuext_t *ext)
{
   ASSERT(src_apu);

   src_apu->ext = ext;

   /* initialize it */
   if (src_apu->ext && NULL != src_apu->ext->init)
      src_apu->ext->init();
}

/*
** $Log: nes_apu.c,v $
** Revision 1.2  2001/04/27 14:37:11  neil
** wheeee
**
** Revision 1.1  2001/04/27 12:54:40  neil
** blah
**
** Revision 1.1.1.1  2001/04/27 07:03:54  neil
** initial
**
** Revision 1.6  2000/12/08 02:36:14  matt
** bye bye apu queue (for now)
**
** Revision 1.5  2000/11/27 19:33:53  matt
** no special treatment for nsf
**
** Revision 1.4  2000/11/25 20:29:17  matt
** weighted filter is now default
**
** Revision 1.3  2000/11/21 13:28:19  matt
** take care to zero allocated mem
**
** Revision 1.2  2000/10/28 15:20:59  matt
** irq callbacks in nes_apu
**
** Revision 1.1  2000/10/24 12:19:59  matt
** changed directory structure
**
** Revision 1.44  2000/10/23 17:53:06  matt
** set ptr to NULL after freeing
**
** Revision 1.43  2000/10/17 11:56:42  matt
** selectable apu base frequency
**
** Revision 1.42  2000/10/13 12:16:01  matt
** macro-ized the stuff that should be removed
**
** Revision 1.41  2000/10/10 13:58:18  matt
** stroustrup squeezing his way in the door
**
** Revision 1.40  2000/10/03 11:56:20  matt
** better support for optional sound ext routines
**
** Revision 1.39  2000/09/27 12:26:03  matt
** changed sound accumulators back to floats
**
** Revision 1.38  2000/09/18 02:12:55  matt
** more optimizations
**
** Revision 1.37  2000/09/15 13:38:40  matt
** changes for optimized apu core
**
** Revision 1.36  2000/09/15 04:58:07  matt
** simplifying and optimizing APU core
**
** Revision 1.35  2000/09/07 21:57:14  matt
** api change
**
** Revision 1.34  2000/08/16 05:01:01  matt
** small buglet fixed
**
** Revision 1.33  2000/08/15 12:38:04  matt
** removed debug output
**
** Revision 1.32  2000/08/15 12:36:51  matt
** calling apu_process with buffer=NULL causes silent emulation of APU
**
** Revision 1.31  2000/08/11 02:27:21  matt
** general cleanups, plus apu_setparams routine
**
** Revision 1.30  2000/07/31 04:32:52  matt
** fragsize problem fixed, perhaps
**
** Revision 1.29  2000/07/30 04:32:59  matt
** no more apu_getcyclerate hack
**
** Revision 1.28  2000/07/28 03:15:46  matt
** accuracy changes for rectangle frequency sweeps
**
** Revision 1.27  2000/07/27 02:49:50  matt
** eccentricity in sweeping hardware now emulated correctly
**
** Revision 1.26  2000/07/25 02:25:14  matt
** safer apu_destroy
**
** Revision 1.25  2000/07/23 15:10:54  matt
** hacks for win32
**
** Revision 1.24  2000/07/17 01:52:31  matt
** made sure last line of all source files is a newline
**
** Revision 1.23  2000/07/10 19:24:55  matt
** irqs are not supported in NSF playing mode
**
** Revision 1.22  2000/07/10 13:54:32  matt
** using generic nes_irq() now
**
** Revision 1.21  2000/07/10 05:29:34  matt
** moved joypad/oam dma from apu to ppu
**
** Revision 1.20  2000/07/09 03:49:31  matt
** apu irqs now draw an irq line (bleh)
**
** Revision 1.19  2000/07/04 04:53:26  matt
** minor changes, sound amplification
**
** Revision 1.18  2000/07/03 02:18:53  matt
** much better external module exporting
**
** Revision 1.17  2000/06/26 11:01:55  matt
** made triangle a tad quieter
**
** Revision 1.16  2000/06/26 05:10:33  matt
** fixed cycle rate generation accuracy
**
** Revision 1.15  2000/06/26 05:00:37  matt
** cleanups
**
** Revision 1.14  2000/06/23 11:06:24  matt
** more faithful mixing of channels
**
** Revision 1.13  2000/06/23 03:29:27  matt
** cleaned up external sound inteface
**
** Revision 1.12  2000/06/20 00:08:39  matt
** bugfix to rectangle wave
**
** Revision 1.11  2000/06/13 13:48:58  matt
** fixed triangle write latency for fixed point apu cycle rate
**
** Revision 1.10  2000/06/12 01:14:36  matt
** minor change to clipping extents
**
** Revision 1.9  2000/06/09 20:00:56  matt
** fixed noise hiccup in NSF player mode
**
** Revision 1.8  2000/06/09 16:49:02  matt
** removed all floating point from sound generation
**
** Revision 1.7  2000/06/09 15:12:28  matt
** initial revision
**
*/
