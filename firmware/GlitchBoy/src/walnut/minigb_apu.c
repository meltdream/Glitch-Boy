#if defined(__GNUC__)
#pragma GCC optimize("O3","unroll-loops")
#endif

/**
 * Game Boy APU emulator.
 * Copyright (c) 2019 Mahyar Koshkouei <mk@deltabeard.com>
 * Copyright (c) 2017 Alex Baines <alex@abaines.me.uk>
 * minigb_apu is released under the terms of the MIT license.
 *
 * minigb_apu emulates the audio processing unit (APU) of the Game Boy. This
 * project is based on MiniGBS by Alex Baines: https://github.com/baines/MiniGBS
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define MINIGB_APU_AUDIO_FORMAT_S16SYS

#include "minigb_apu.h"

#define DMG_CLOCK_FREQ_U	((unsigned)DMG_CLOCK_FREQ)
#define AUDIO_NSAMPLES		(AUDIO_SAMPLES_TOTAL)

#define MAX(a, b)		( a > b ? a : b )
#define MIN(a, b)		( a <= b ? a : b )

/* Factor in which values are multiplied to compensate for fixed-point
 * arithmetic. Some hard-coded values in this project must be recreated. */
#ifndef FREQ_INC_MULT
# define FREQ_INC_MULT		105
#endif
/* Handles time keeping for sound generation.
 * FREQ_INC_REF must be equal to, or larger than AUDIO_SAMPLE_RATE in order
 * to avoid a division by zero error.
 * Using a square of 2 simplifies calculations. */
#define FREQ_INC_REF		(AUDIO_SAMPLE_RATE * FREQ_INC_MULT)

#define MAX_CHAN_VOLUME		15

static uint32_t pitch_scale_q16(const struct minigb_apu_ctx *ctx)
{
	return (ctx && ctx->pitch_q16) ? ctx->pitch_q16 : (1u << 16);
}

static uint32_t apply_pitch_scale(const struct minigb_apu_ctx *ctx,
		const uint32_t freq_inc)
{
	const uint64_t scaled =
		((uint64_t)freq_inc * (uint64_t)pitch_scale_q16(ctx) +
		 (1u << 15)) >> 16;

	if (scaled == 0)
		return 1;
	if (scaled > UINT32_MAX)
		return UINT32_MAX;
	return (uint32_t)scaled;
}

static void set_note_freq(struct minigb_apu_ctx *ctx, struct chan *c)
{
	/* Lowest expected value of freq is 64. */
	uint32_t freq = (DMG_CLOCK_FREQ_U / 4) / (2048 - c->freq);
	c->freq_inc = apply_pitch_scale(ctx,
			freq * (uint32_t)(FREQ_INC_REF / AUDIO_SAMPLE_RATE));
}

static void chan_enable(struct minigb_apu_ctx *ctx,
		const uint_fast8_t i, const bool enable)
{
	uint8_t val;

	ctx->chans[i].enabled = enable;
	val = (ctx->audio_mem[0xFF26 - AUDIO_ADDR_COMPENSATION] & 0x80) |
		(ctx->chans[3].enabled << 3) | (ctx->chans[2].enabled << 2) |
		(ctx->chans[1].enabled << 1) | (ctx->chans[0].enabled << 0);

	ctx->audio_mem[0xFF26 - AUDIO_ADDR_COMPENSATION] = val;
}

static void update_env(struct chan *c)
{
	c->env.counter += c->env.inc;

	while (c->env.counter > FREQ_INC_REF) {
		if (c->env.step) {
			c->volume += c->env.up ? 1 : -1;
			if (c->volume == 0 || c->volume == MAX_CHAN_VOLUME) {
				c->env.inc = 0;
			}
			c->volume = MAX(0, MIN(MAX_CHAN_VOLUME, c->volume));
		}
		c->env.counter -= FREQ_INC_REF;
	}
}

static void update_len(struct minigb_apu_ctx *ctx, struct chan *c)
{
	if (!c->len.enabled)
		return;

	c->len.counter += c->len.inc;
	if (c->len.counter > FREQ_INC_REF) {
		chan_enable(ctx, c - ctx->chans, 0);
		c->len.counter = 0;
	}
}

static bool update_freq(struct chan *c, uint32_t *pos)
{
	uint32_t inc = c->freq_inc - *pos;
	c->freq_counter += inc;

	if (c->freq_counter > FREQ_INC_REF) {
		*pos		= c->freq_inc - (c->freq_counter - FREQ_INC_REF);
		c->freq_counter = 0;
		return true;
	} else {
		*pos = c->freq_inc;
		return false;
	}
}

static void update_sweep(struct minigb_apu_ctx *ctx, struct chan *c)
{
	c->sweep.counter += c->sweep.inc;

	while (c->sweep.counter > FREQ_INC_REF) {
		if (c->sweep.shift) {
			uint16_t inc = (c->sweep.freq >> c->sweep.shift);
			if (c->sweep.down)
				inc *= -1;

			c->freq = c->sweep.freq + inc;
			if (c->freq > 2047) {
				c->enabled = 0;
			} else {
				set_note_freq(ctx, c);
				c->sweep.freq = c->freq;
			}
		} else if (c->sweep.rate) {
			c->enabled = 0;
		}
		c->sweep.counter -= FREQ_INC_REF;
	}
}

static float midi_poly_blep(float phase, float phase_step)
{
	if (phase < phase_step) {
		float t = phase / phase_step;
		return t + t - t * t - 1.0f;
	}
	if (phase > 1.0f - phase_step) {
		float t = (phase - 1.0f) / phase_step;
		return t * t + t + t + 1.0f;
	}
	return 0.0f;
}

static uint8_t duty_bit_count(uint8_t duty)
{
	uint8_t count = 0;
	while (duty) {
		count += duty & 1u;
		duty >>= 1;
	}
	return count;
}

static int32_t update_square_midi_bandlimited(struct minigb_apu_ctx *ctx,
		struct chan *c, uint_fast8_t channel)
{
	float phase = ctx->midi_square_phase[channel];
	float phase_step =
		(float)c->freq_inc / ((float)FREQ_INC_REF * 8.0f);
	const float duty = (float)duty_bit_count(c->square.duty) / 8.0f;
	float edge_phase;
	float wave;

	if (phase_step < 0.0f)
		phase_step = 0.0f;
	if (phase_step > 0.499f)
		phase_step = 0.499f;

	wave = (phase < duty) ? 1.0f : -1.0f;
	wave += midi_poly_blep(phase, phase_step);
	edge_phase = phase - duty;
	if (edge_phase < 0.0f)
		edge_phase += 1.0f;
	wave -= midi_poly_blep(edge_phase, phase_step);

	phase += phase_step;
	if (phase >= 1.0f)
		phase -= 1.0f;
	ctx->midi_square_phase[channel] = phase;
	c->square.duty_counter = (uint8_t)(phase * 8.0f) & 7u;

	return (int32_t)((float)(VOL_INIT_MAX / MAX_CHAN_VOLUME) * wave);
}

static int32_t filter_midi_wave_sample(struct minigb_apu_ctx *ctx,
		int32_t sample, uint32_t freq_inc)
{
	const uint32_t transition_q16 =
		(uint32_t)(((uint64_t)freq_inc << 16) / FREQ_INC_REF);
	int32_t filtered = sample;

	if (!ctx->midi_wave_history_valid) {
		ctx->midi_wave_history[0] = sample;
		ctx->midi_wave_history[1] = sample;
		ctx->midi_wave_history_valid = 1;
		return sample;
	}

	if (transition_q16 >= (1u << 16))
		filtered = (sample + ctx->midi_wave_history[0] * 2 +
				ctx->midi_wave_history[1]) / 4;
	else if (transition_q16 >= (1u << 15))
		filtered = (sample + ctx->midi_wave_history[0]) / 2;

	ctx->midi_wave_history[1] = ctx->midi_wave_history[0];
	ctx->midi_wave_history[0] = sample;
	return filtered;
}

static void update_square(struct minigb_apu_ctx *ctx, audio_sample_t *samples,
		const bool ch2)
{
	struct chan *c = &ctx->chans[ch2];

	if (!c->powered || !c->enabled)
		return;

	set_note_freq(ctx, c);

	for (uint_fast16_t i = 0; i < AUDIO_NSAMPLES; i += 2) {
		update_len(ctx, c);
		if (!c->enabled)
			return;

		update_env(c);
		if (!c->volume)
			continue;

		if (!ch2)
			update_sweep(ctx, c);

		int32_t sample;
		if (ctx->midi_bandlimit) {
			sample = update_square_midi_bandlimited(ctx, c, ch2 ? 1 : 0);
		} else {
			uint32_t pos = 0;
			uint32_t prev_pos = 0;
			sample = 0;

			while (update_freq(c, &pos)) {
				c->square.duty_counter = (c->square.duty_counter + 1) & 7;
				sample += ((pos - prev_pos) / c->freq_inc) * c->val;
				c->val = (c->square.duty & (1 << c->square.duty_counter)) ?
					VOL_INIT_MAX / MAX_CHAN_VOLUME :
					VOL_INIT_MIN / MAX_CHAN_VOLUME;
				prev_pos = pos;
			}
			sample += c->val;
		}
		sample *= c->volume;
		sample /= 4;

		samples[i + 0] += sample * c->on_left * ctx->vol_l;
		samples[i + 1] += sample * c->on_right * ctx->vol_r;
	}
}

static void update_square_mono(struct minigb_apu_ctx *ctx, audio_sample_t *samples,
		const bool ch2, uint_fast16_t sample_count)
{
	struct chan *c = &ctx->chans[ch2];

	if (!c->powered || !c->enabled)
		return;

	set_note_freq(ctx, c);
	const int32_t mix_gain = ((int32_t)c->on_left * ctx->vol_l) +
			((int32_t)c->on_right * ctx->vol_r);
	if (!mix_gain)
		return;

	for (uint_fast16_t i = 0; i < sample_count; ++i) {
		update_len(ctx, c);
		if (!c->enabled)
			return;

		update_env(c);
		if (!c->volume)
			continue;

		if (!ch2)
			update_sweep(ctx, c);

		int32_t sample;
		if (ctx->midi_bandlimit) {
			sample = update_square_midi_bandlimited(ctx, c, ch2 ? 1 : 0);
		} else {
			uint32_t pos = 0;
			uint32_t prev_pos = 0;
			sample = 0;

			while (update_freq(c, &pos)) {
				c->square.duty_counter = (c->square.duty_counter + 1) & 7;
				sample += ((pos - prev_pos) / c->freq_inc) * c->val;
				c->val = (c->square.duty & (1 << c->square.duty_counter)) ?
					VOL_INIT_MAX / MAX_CHAN_VOLUME :
					VOL_INIT_MIN / MAX_CHAN_VOLUME;
				prev_pos = pos;
			}
			sample += c->val;
		}
		sample *= c->volume;
		sample /= 4;

		samples[i] += (audio_sample_t)((sample * mix_gain) / 2);
	}
}

static uint8_t wave_sample(struct minigb_apu_ctx *ctx,
		const unsigned int pos, const unsigned int volume)
{
	uint8_t sample;

	sample =  ctx->audio_mem[(0xFF30 + pos / 2) - AUDIO_ADDR_COMPENSATION];
	if (pos & 1) {
		sample &= 0xF;
	} else {
		sample >>= 4;
	}
	return volume ? (sample >> (volume - 1)) : 0;
}

static void update_wave(struct minigb_apu_ctx *ctx, audio_sample_t *samples)
{
	struct chan *c = &ctx->chans[2];

	if (!c->powered || !c->enabled || !c->volume)
		return;

	set_note_freq(ctx, c);
	c->freq_inc *= 2;

	for (uint_fast16_t i = 0; i < AUDIO_NSAMPLES; i += 2) {
		update_len(ctx, c);
		if (!c->enabled)
			return;

		uint32_t pos = 0;
		uint32_t prev_pos = 0;
		audio_sample_t sample = 0;

		c->wave.sample = wave_sample(ctx, c->val, c->volume);

		while (update_freq(c, &pos)) {
			c->val = (c->val + 1) & 31;
			sample += ((pos - prev_pos) / c->freq_inc) *
				((audio_sample_t)c->wave.sample - 8) *
					(AUDIO_SAMPLE_MAX/64);
			c->wave.sample = wave_sample(ctx, c->val, c->volume);
			prev_pos  = pos;
		}

		sample += ((audio_sample_t)c->wave.sample - 8) *
				(audio_sample_t)(AUDIO_SAMPLE_MAX/64);
		{
			/* First element is unused. */
			audio_sample_t div[] = { AUDIO_SAMPLE_MAX, 1, 2, 4 };
			sample = sample / (div[c->volume]);
		}

		sample /= 4;
		samples[i + 0] += sample * c->on_left * ctx->vol_l;
		samples[i + 1] += sample * c->on_right * ctx->vol_r;
	}
}

static void update_wave_mono(struct minigb_apu_ctx *ctx, audio_sample_t *samples,
		uint_fast16_t sample_count)
{
	struct chan *c = &ctx->chans[2];

	if (!c->powered || !c->enabled || !c->volume)
		return;

	set_note_freq(ctx, c);
	c->freq_inc *= 2;
	const int32_t mix_gain = ((int32_t)c->on_left * ctx->vol_l) +
			((int32_t)c->on_right * ctx->vol_r);
	if (!mix_gain)
		return;

	for (uint_fast16_t i = 0; i < sample_count; ++i) {
		update_len(ctx, c);
		if (!c->enabled)
			return;

		uint32_t pos = 0;
		uint32_t prev_pos = 0;
		audio_sample_t sample = 0;

		c->wave.sample = wave_sample(ctx, c->val, c->volume);

		while (update_freq(c, &pos)) {
			c->val = (c->val + 1) & 31;
			sample += ((pos - prev_pos) / c->freq_inc) *
				((audio_sample_t)c->wave.sample - 8) *
					(AUDIO_SAMPLE_MAX/64);
			c->wave.sample = wave_sample(ctx, c->val, c->volume);
			prev_pos  = pos;
		}

		sample += ((audio_sample_t)c->wave.sample - 8) *
				(audio_sample_t)(AUDIO_SAMPLE_MAX/64);
		{
			audio_sample_t div[] = { AUDIO_SAMPLE_MAX, 1, 2, 4 };
			sample = sample / (div[c->volume]);
		}

		sample /= 4;
		if (ctx->midi_bandlimit)
			sample = filter_midi_wave_sample(ctx, sample, c->freq_inc);
		samples[i] += (audio_sample_t)((sample * mix_gain) / 2);
	}
}

static void update_noise(struct minigb_apu_ctx *ctx, audio_sample_t *samples)
{
	struct chan *c = &ctx->chans[3];

	if (c->freq >= 14)
		c->enabled = 0;

	if (!c->powered || !c->enabled)
		return;

	{
		const uint32_t lfsr_div_lut[] = {
			8, 16, 32, 48, 64, 80, 96, 112
		};
		uint32_t freq;

		freq = DMG_CLOCK_FREQ_U / (lfsr_div_lut[c->noise.lfsr_div] << c->freq);
		c->freq_inc = apply_pitch_scale(ctx,
				freq * (uint32_t)(FREQ_INC_REF / AUDIO_SAMPLE_RATE));
	}

	for (uint_fast16_t i = 0; i < AUDIO_NSAMPLES; i += 2) {
		update_len(ctx, c);
		if (!c->enabled)
			return;

		update_env(c);
		if (!c->volume)
			continue;

		uint32_t pos      = 0;
		uint32_t prev_pos = 0;
		int32_t sample    = 0;

		while (update_freq(c, &pos)) {
			c->noise.lfsr_reg = (c->noise.lfsr_reg << 1) |
				(c->val >= VOL_INIT_MAX/MAX_CHAN_VOLUME);

			if (c->noise.lfsr_wide) {
				c->val = !(((c->noise.lfsr_reg >> 14) & 1) ^
						((c->noise.lfsr_reg >> 13) & 1)) ?
					VOL_INIT_MAX / MAX_CHAN_VOLUME :
					VOL_INIT_MIN / MAX_CHAN_VOLUME;
			} else {
				c->val = !(((c->noise.lfsr_reg >> 6) & 1) ^
						((c->noise.lfsr_reg >> 5) & 1)) ?
					VOL_INIT_MAX / MAX_CHAN_VOLUME :
					VOL_INIT_MIN / MAX_CHAN_VOLUME;
			}

			sample += ((pos - prev_pos) / c->freq_inc) * c->val;
			prev_pos = pos;
		}

		sample += c->val;
		sample *= c->volume;
		sample /= 4;

		samples[i + 0] += sample * c->on_left * ctx->vol_l;
		samples[i + 1] += sample * c->on_right * ctx->vol_r;
	}
}

static void update_noise_mono(struct minigb_apu_ctx *ctx, audio_sample_t *samples,
		uint_fast16_t sample_count)
{
	struct chan *c = &ctx->chans[3];

	if (c->freq >= 14)
		c->enabled = 0;

	if (!c->powered || !c->enabled)
		return;

	{
		const uint32_t lfsr_div_lut[] = {
			8, 16, 32, 48, 64, 80, 96, 112
		};
		uint32_t freq;

		freq = DMG_CLOCK_FREQ_U / (lfsr_div_lut[c->noise.lfsr_div] << c->freq);
		c->freq_inc = apply_pitch_scale(ctx,
				freq * (uint32_t)(FREQ_INC_REF / AUDIO_SAMPLE_RATE));
	}

	const int32_t mix_gain = ((int32_t)c->on_left * ctx->vol_l) +
			((int32_t)c->on_right * ctx->vol_r);
	if (!mix_gain)
		return;

	for (uint_fast16_t i = 0; i < sample_count; ++i) {
		update_len(ctx, c);
		if (!c->enabled)
			return;

		update_env(c);
		if (!c->volume)
			continue;

		uint32_t pos      = 0;
		uint32_t prev_pos = 0;
		int32_t sample    = 0;

		while (update_freq(c, &pos)) {
			c->noise.lfsr_reg = (c->noise.lfsr_reg << 1) |
				(c->val >= VOL_INIT_MAX/MAX_CHAN_VOLUME);

			if (c->noise.lfsr_wide) {
				c->val = !(((c->noise.lfsr_reg >> 14) & 1) ^
						((c->noise.lfsr_reg >> 13) & 1)) ?
					VOL_INIT_MAX / MAX_CHAN_VOLUME :
					VOL_INIT_MIN / MAX_CHAN_VOLUME;
			} else {
				c->val = !(((c->noise.lfsr_reg >> 6) & 1) ^
						((c->noise.lfsr_reg >> 5) & 1)) ?
					VOL_INIT_MAX / MAX_CHAN_VOLUME :
					VOL_INIT_MIN / MAX_CHAN_VOLUME;
			}

			sample += ((pos - prev_pos) / c->freq_inc) * c->val;
			prev_pos = pos;
		}

		sample += c->val;
		sample *= c->volume;
		sample /= 4;

		samples[i] += (audio_sample_t)((sample * mix_gain) / 2);
	}
}

/**
 * SDL2 style audio callback function.
 */
void minigb_apu_audio_callback(struct minigb_apu_ctx *ctx,
		audio_sample_t *stream)
{
	memset(stream, 0, AUDIO_SAMPLES_TOTAL * sizeof(audio_sample_t));
	update_square(ctx, stream, 0);
	update_square(ctx, stream, 1);
	update_wave(ctx, stream);
	update_noise(ctx, stream);
}

void minigb_apu_audio_callback_mono(struct minigb_apu_ctx *ctx,
		audio_sample_t *stream)
{
	minigb_apu_audio_callback_mono_samples(ctx, stream, AUDIO_SAMPLES);
}

void minigb_apu_audio_callback_mono_samples(struct minigb_apu_ctx *ctx,
		audio_sample_t *stream, uint_fast16_t sample_count)
{
	if (!stream || sample_count == 0)
		return;
	memset(stream, 0, sample_count * sizeof(audio_sample_t));
	update_square_mono(ctx, stream, 0, sample_count);
	update_square_mono(ctx, stream, 1, sample_count);
	update_wave_mono(ctx, stream, sample_count);
	update_noise_mono(ctx, stream, sample_count);
}

void minigb_apu_midi_write_envelope(struct minigb_apu_ctx *ctx,
		uint_fast8_t channel, uint8_t value,
		uint8_t update_current_volume)
{
	static const uint16_t env_addresses[4] = {
		0xFF12, 0xFF17, 0, 0xFF21
	};
	static const uint32_t inc_lut[8] = {
		FREQ_INC_MULT * 8,
		FREQ_INC_MULT * 64,
		FREQ_INC_MULT * 32,
		(FREQ_INC_MULT * 64) / 3,
		FREQ_INC_MULT * 16,
		(FREQ_INC_MULT * 64) / 5,
		(FREQ_INC_MULT * 64) / 6,
		(FREQ_INC_MULT * 64) / 7
	};
	struct chan *c;
	uint16_t address;

	if (!ctx || channel >= 4 || channel == 2)
		return;
	address = env_addresses[channel];
	c = &ctx->chans[channel];
	ctx->audio_mem[address - AUDIO_ADDR_COMPENSATION] = value;
	c->volume_init = value >> 4;
	c->powered = (value >> 3) != 0;
	c->env.step = value & 0x07;
	c->env.up = value & 0x08;
	c->env.inc = inc_lut[c->env.step];
	c->env.counter = 0;
	if (update_current_volume || !c->powered)
		c->volume = c->powered ? c->volume_init : 0;
}

void minigb_apu_midi_reload_length(struct minigb_apu_ctx *ctx,
		uint_fast8_t channel)
{
	struct chan *c;
	uint16_t length_max;
	uint16_t remaining;

	if (!ctx || channel >= 4)
		return;
	c = &ctx->chans[channel];
	length_max = (channel == 2) ? 256 : 64;
	remaining = (uint16_t)(length_max - c->len.load);
	if (remaining == 0)
		remaining = 1;
	c->len.inc =
		(256u * FREQ_INC_REF) / (AUDIO_SAMPLE_RATE * remaining);
	c->len.counter = 0;
}

static void chan_trigger(struct minigb_apu_ctx *ctx, uint_fast8_t i)
{
	struct chan *c = &ctx->chans[i];

	chan_enable(ctx, i, 1);
	c->volume = c->volume_init;

	// volume envelope
	{
		/* LUT created in Julia with:
		 * `(FREQ_INC_MULT * 64)./vcat(8, 1:7)`
		 * Must be recreated when FREQ_INC_MULT modified.
		 */
		const uint32_t inc_lut[8] = {
#if FREQ_INC_MULT == 16
			128, 1024, 512, 341,
			256,  205, 171, 146
#elif FREQ_INC_MULT == 64
            512,  4096, 2048, 1365,
            1024,  819,  683,  585
#elif FREQ_INC_MULT == 105
            /* Multiples of 105 provide integer values. */
            840,  6720, 3360, 2240,
            1680, 1344, 1120,  960
#else
#error "LUT not calculated for this value of FREQ_INC_MULT"
#endif
		};
		uint8_t val;

		val = ctx->audio_mem[(0xFF12 + (i * 5)) - AUDIO_ADDR_COMPENSATION];

		c->env.step = val & 0x7;
		c->env.up   = val & 0x8;
		c->env.inc  = inc_lut[c->env.step];
		c->env.counter = 0;
	}

	// freq sweep
	if (i == 0) {
		uint8_t val = ctx->audio_mem[0xFF10 - AUDIO_ADDR_COMPENSATION];

		c->sweep.freq  = c->freq;
		c->sweep.rate  = (val >> 4) & 0x07;
		c->sweep.down  = (val & 0x08);
		c->sweep.shift = (val & 0x07);
		c->sweep.inc   = c->sweep.rate ?
			((128u * FREQ_INC_REF) / (c->sweep.rate * AUDIO_SAMPLE_RATE)) : 0;
		c->sweep.counter = FREQ_INC_REF;
	}

	int len_max = 64;

	if (i == 2) { // wave
		len_max = 256;
		c->val = 0;
		if (ctx->midi_bandlimit)
			ctx->midi_wave_history_valid = 0;
	} else if (i == 3) { // noise
		c->noise.lfsr_reg = 0xFFFF;
		c->val = VOL_INIT_MIN / MAX_CHAN_VOLUME;
	}

	c->len.inc = (256u * FREQ_INC_REF) / (AUDIO_SAMPLE_RATE * (len_max - c->len.load));
	c->len.counter = 0;
	if (ctx->midi_bandlimit && i < 2)
		ctx->midi_square_phase[i] = 0.0f;
}

/**
 * Read audio register.
 * \param addr	Address of audio register. Must be 0xFF10 <= addr <= 0xFF3F.
 *				This is not checked in this function.
 * \return	Byte at address.
 */
uint8_t minigb_apu_audio_read(struct minigb_apu_ctx *ctx, const uint16_t addr)
{
	static const uint8_t ortab[] = {
		0x80, 0x3f, 0x00, 0xff, 0xbf,
		0xff, 0x3f, 0x00, 0xff, 0xbf,
		0x7f, 0xff, 0x9f, 0xff, 0xbf,
		0xff, 0xff, 0x00, 0x00, 0xbf,
		0x00, 0x00, 0x70,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};

	return ctx->audio_mem[addr - AUDIO_ADDR_COMPENSATION] |
		ortab[addr - AUDIO_ADDR_COMPENSATION];
}

/**
 * Write audio register.
 * \param addr	Address of audio register. Must be 0xFF10 <= addr <= 0xFF3F.
 *				This is not checked in this function.
 * \param val	Byte to write at address.
 */
void minigb_apu_audio_write(struct minigb_apu_ctx *ctx,
		const uint16_t addr, const uint8_t val)
{
	/* Find sound channel corresponding to register address. */
	uint_fast8_t i;

	if(addr == 0xFF26)
	{
		ctx->audio_mem[addr - AUDIO_ADDR_COMPENSATION] = val & 0x80;
		/* On APU power off, clear all registers apart from wave
		 * RAM. */
		if((val & 0x80) == 0)
		{
			memset(ctx->audio_mem,
					0x00, 0xFF26 - AUDIO_ADDR_COMPENSATION);
			ctx->chans[0].enabled = false;
			ctx->chans[1].enabled = false;
			ctx->chans[2].enabled = false;
			ctx->chans[3].enabled = false;
		}

		return;
	}

	/* Ignore register writes if APU powered off. */
	if(ctx->audio_mem[0xFF26 - AUDIO_ADDR_COMPENSATION] == 0x00)
		return;

	ctx->audio_mem[addr - AUDIO_ADDR_COMPENSATION] = val;
	i = (addr - AUDIO_ADDR_COMPENSATION) / 5;

	switch (addr) {
	case 0xFF12:
	case 0xFF17:
	case 0xFF21: {
		ctx->chans[i].volume_init = val >> 4;
		ctx->chans[i].powered     = (val >> 3) != 0;

		// "zombie mode" stuff, needed for Prehistorik Man and probably
		// others
		if (ctx->chans[i].powered && ctx->chans[i].enabled) {
			if ((ctx->chans[i].env.step == 0 && ctx->chans[i].env.inc != 0)) {
				if (val & 0x08) {
					ctx->chans[i].volume++;
				} else {
					ctx->chans[i].volume += 2;
				}
			} else {
				ctx->chans[i].volume = 16 - ctx->chans[i].volume;
			}

			ctx->chans[i].volume &= 0x0F;
			ctx->chans[i].env.step = val & 0x07;
		}
	} break;

	case 0xFF1C:
		ctx->chans[i].volume = ctx->chans[i].volume_init = (val >> 5) & 0x03;
		break;

	case 0xFF11:
	case 0xFF16:
	case 0xFF20: {
		const uint8_t duty_lookup[] = { 0x10, 0x30, 0x3C, 0xCF };
		ctx->chans[i].len.load = val & 0x3f;
		ctx->chans[i].square.duty = duty_lookup[val >> 6];
		break;
	}

	case 0xFF1B:
		ctx->chans[i].len.load = val;
		break;

	case 0xFF13:
	case 0xFF18:
	case 0xFF1D:
		ctx->chans[i].freq &= 0xFF00;
		ctx->chans[i].freq |= val;
		break;

	case 0xFF1A:
		ctx->chans[i].powered = (val & 0x80) != 0;
		chan_enable(ctx, i, val & 0x80);
		break;

	case 0xFF14:
	case 0xFF19:
	case 0xFF1E:
		ctx->chans[i].freq &= 0x00FF;
		ctx->chans[i].freq |= ((val & 0x07) << 8);
		/* Intentional fall-through. */
	case 0xFF23:
		ctx->chans[i].len.enabled = val & 0x40;
		if (val & 0x80)
			chan_trigger(ctx, i);

		break;

	case 0xFF22:
		ctx->chans[3].freq = val >> 4;
		ctx->chans[3].noise.lfsr_wide = !(val & 0x08);
		ctx->chans[3].noise.lfsr_div = val & 0x07;
		break;

	case 0xFF24:
	{
		ctx->vol_l = ((val >> 4) & 0x07);
		ctx->vol_r = (val & 0x07);
		break;
	}

	case 0xFF25:
		for (uint_fast8_t j = 0; j < 4; j++) {
			ctx->chans[j].on_left  = (val >> (4 + j)) & 1;
			ctx->chans[j].on_right = (val >> j) & 1;
		}
		break;
	}
}

void minigb_apu_set_pitch_q16(struct minigb_apu_ctx *ctx, uint32_t pitch_q16)
{
	if (!ctx)
		return;
	ctx->pitch_q16 = pitch_q16 ? pitch_q16 : (1u << 16);
}

void minigb_apu_set_midi_bandlimit(struct minigb_apu_ctx *ctx, uint8_t enable)
{
	if (!ctx)
		return;
	ctx->midi_bandlimit = enable ? 1u : 0u;
	ctx->midi_square_phase[0] = 0.0f;
	ctx->midi_square_phase[1] = 0.0f;
	ctx->midi_wave_history[0] = 0;
	ctx->midi_wave_history[1] = 0;
	ctx->midi_wave_history_valid = 0;
}

void minigb_apu_audio_init(struct minigb_apu_ctx *ctx)
{
	/* Initialise channels and samples. */
	memset(ctx->chans, 0, sizeof(ctx->chans));
	ctx->chans[0].val = ctx->chans[1].val = -1;
	minigb_apu_set_pitch_q16(ctx, 1u << 16);
	ctx->midi_square_phase[0] = 0.0f;
	ctx->midi_square_phase[1] = 0.0f;
	ctx->midi_wave_history[0] = 0;
	ctx->midi_wave_history[1] = 0;
	ctx->midi_wave_history_valid = 0;

	/* Initialise IO registers. */
	{
		const uint8_t regs_init[] = { 0x80, 0xBF, 0xF3, 0xFF, 0x3F,
					      0xFF, 0x3F, 0x00, 0xFF, 0x3F,
					      0x7F, 0xFF, 0x9F, 0xFF, 0x3F,
					      0xFF, 0xFF, 0x00, 0x00, 0x3F,
					      0x77, 0xF3, 0xF1 };

		for(uint_fast8_t i = 0; i < sizeof(regs_init); ++i)
			minigb_apu_audio_write(ctx, 0xFF10 + i, regs_init[i]);
	}

	/* Initialise Wave Pattern RAM. */
	{
		const uint8_t wave_init[] = { 0xac, 0xdd, 0xda, 0x48,
					      0x36, 0x02, 0xcf, 0x16,
					      0x2c, 0x04, 0xe5, 0x2c,
					      0xac, 0xdd, 0xda, 0x48 };

		for(uint_fast8_t i = 0; i < sizeof(wave_init); ++i)
			minigb_apu_audio_write(ctx, 0xFF30 + i, wave_init[i]);
	}
}
