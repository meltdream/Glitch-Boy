/*
 * Walnut-CGB additions
 * Copyright (c) 2025 Mr. Paul (https://github.com/Mr-PauI)
 *
 * Licensed under the MIT License.
 *
 * --------------------------------------------------------------------------
 *
 * Peanut-GB
 * Copyright (c) 2018-2023 Mahyar Koshkouei
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Please note that at least two parts of source code within this project were
 * taken from the SameBoy project at https://github.com/LIJI32/SameBoy/
 * which, at the time of this writing, is released under the MIT License.
 * Occurrences of this code are marked as being taken from SameBoy with a comment.
 *
 * SameBoy
 * Copyright (c) 2015-2019 Lior Halphon
 */


#ifndef WALNUT_GB_H
#define WALNUT_GB_H
#if defined(__has_include)
# if __has_include("version.all")
#  include "version.all"	/* Version information */
# endif
#else
/* Stub __has_include for later. */
# define __has_include(x) 0
#endif

#include <stdlib.h>	/* Required for abort */
#include <stddef.h>	/* Required for offsetof */
#include <stdbool.h>	/* Required for bool types */
#include <stdint.h>	/* Required for int types */
#include <string.h>	/* Required for memset */
#include <time.h>	/* Required for tm struct */
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif
#ifndef WALNUT_GB_HOT_ATTR
#define WALNUT_GB_HOT_ATTR IRAM_ATTR
#endif
#ifndef WALNUT_GB_NOINLINE
# if defined(__GNUC__)
#  define WALNUT_GB_NOINLINE __attribute__((noinline))
# else
#  define WALNUT_GB_NOINLINE
# endif
#endif
#ifndef WALNUT_GB_ENABLE_GLITCH
#define WALNUT_GB_ENABLE_GLITCH 1
#endif
#if WALNUT_GB_ENABLE_GLITCH
#include "../glitch/gb_glitch.h"
#endif

/**
* If WALNUT_GB_IS_LITTLE_ENDIAN is positive, then Walnut-GB will be configured
* for a little endian platform. If 0, then big endian.
*/
// Safe dual-fetch modes reload opcodes after invalidating writes.
#define WALNUT_GB_SAFE_DUALFETCH_OPCODES 0
#define WALNUT_GB_SAFE_DUALFETCH_DMA  0
#define WALNUT_GB_SAFE_DUALFETCH_MBC  0
// Disabled until 16-bit dual-fetch reads are compatible with all games.
#define WALNUT_GB_16_BIT_OPS_DUALFETCH 0
#define WALNUT_GB_16_BIT_OPS_DUALFETCH_DISABLED2 0
#define WALNUT_GB_16_BIT_OPS_DUALFETCH_DISABLED 0
// Disabled until chained 16-bit reads are compatible with all games.
#ifndef WALNUT_GB_16_BIT_OPS
#define WALNUT_GB_16_BIT_OPS 0
#endif
// DMA width modes are mutually exclusive. Direct stores require safe alignment.
#define WALNUT_GB_16BIT_DMA 0
#define WALNUT_GB_32BIT_DMA 1
// Use aligned wide accesses with a byte fallback.
#define WALNUT_GB_16BIT_ALIGNED 1
#define WALNUT_GB_32BIT_ALIGNED 1
#define WALNUT_GB_RGB565_BIGENDIAN 0
#if WALNUT_GB_ENABLE_GLITCH
#define WGB_GB_GLITCH_PPU_MASK() (g_gb_glitch_ppu_mask)
#define WGB_GB_GLITCH_BG_MASK() (g_gb_glitch_bg_mask)
#define WGB_GB_GLITCH_APPLY_OAM_WRITE(address, value) gb_glitch_apply_oam_write((address), (value))
#define WGB_GB_GLITCH_APPLY_OAM_DMA(oam) gb_glitch_apply_oam_dma((oam))
#define WGB_GB_GLITCH_APPLY_APU_WRITE(address, value) gb_glitch_apply_apu_write((address), (value))
#else
#define WGB_GB_GLITCH_PPU_MASK() ((uint16_t)0)
#define WGB_GB_GLITCH_BG_MASK() ((uint16_t)0)
#define WGB_GB_GLITCH_APPLY_OAM_WRITE(address, value) ((void)(address), (void)(value))
#define WGB_GB_GLITCH_APPLY_OAM_DMA(oam) ((void)(oam))
#define WGB_GB_GLITCH_APPLY_APU_WRITE(address, value) ((void)(address), (void)(value), false)
#endif

uint8_t WALNUT_GB_HOT_ATTR __gb_read(struct gb_s *gb, uint16_t addr);
void WALNUT_GB_HOT_ATTR __gb_write(struct gb_s *gb, uint_fast16_t addr, uint8_t val);
void __gb_write16(struct gb_s *gb, uint_fast16_t addr, uint16_t val);
void __gb_write32(struct gb_s *gb, uint16_t addr, uint32_t val);
void __gb_step_cpu_x(struct gb_s *gb);
static inline void __gb_lcd_reschedule(struct gb_s *gb);
static inline void __gb_lazy_post_flush_gb(struct gb_s *gb);
#if WALNUT_GB_32BIT_DMA && WALNUT_GB_16BIT_DMA
#error "Only one DMA mode can be enabled at a time!"
#endif
#define WALNUT_GB
// Walnut-CGB only supports little endian targets, this is a inherited Peanut-GB limitation. See: https://github.com/deltabeard/Peanut-GB/issues/150
#define WALNUT_GB_IS_LITTLE_ENDIAN 1

#if !defined(WALNUT_GB_IS_LITTLE_ENDIAN)
/* If endian is not defined, then attempt to detect it. */
# if defined(__BYTE_ORDER__)
#  if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
/* Building for a big endian platform. */
#   define WALNUT_GB_IS_LITTLE_ENDIAN 0
#  else
#   define WALNUT_GB_IS_LITTLE_ENDIAN 1
#  endif /* __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__ */
# elif defined(_WIN32)
/* We assume that Windows is always little endian by default. */
#  define WALNUT_GB_IS_LITTLE_ENDIAN 1
# elif !defined(WALNUT_GB_IS_LITTLE_ENDIAN)
#  error "Could not detect target platform endian. Please define WALNUT_GB_IS_LITTLE_ENDIAN"
# endif
#endif /* !defined(WALNUT_GB_IS_LITTLE_ENDIAN) */

#if WALNUT_GB_IS_LITTLE_ENDIAN == 0
# error "Walnut-GB only supports little endian targets"
/* This is because the logic has been written with assumption of little
 * endian byte order. */
#endif

/** Definitions for compile-time setting of features. **/
/**
 * Sound support must be provided by an external library. When audio_read() and
 * audio_write() functions are provided, define ENABLE_SOUND to a non-zero value
 * before including walnut_cgb.h in order for these functions to be used.
 */
#ifndef ENABLE_SOUND
# define ENABLE_SOUND 1
#endif

/* Enable LCD drawing. On by default. May be turned off for testing purposes. */
#ifndef ENABLE_LCD
# define ENABLE_LCD 1
#endif

/* Cache OAM membership per scanline in static storage. */
#ifndef WALNUT_GB_OAM_CACHE
# define WALNUT_GB_OAM_CACHE 1
#endif

#ifndef WALNUT_GB_DIRTY_FRAME_SKIP
# define WALNUT_GB_DIRTY_FRAME_SKIP 0
#endif

#ifndef WALNUT_GB_DUALFETCH_HOT_OPS
# define WALNUT_GB_DUALFETCH_HOT_OPS 1
#endif

/* GCC labels-as-values keep frame execution inside one dispatch loop. */
#ifndef WALNUT_GB_THREADED_CPU
# define WALNUT_GB_THREADED_CPU 0
#endif

#if WALNUT_GB_THREADED_CPU && defined(__GNUC__)
# define WALNUT_GB_THREADED_CPU_ACTIVE 1
#else
# define WALNUT_GB_THREADED_CPU_ACTIVE 0
#endif

#ifndef WALNUT_GB_THREADED_LOCAL_CPU_REGS
# define WALNUT_GB_THREADED_LOCAL_CPU_REGS 1
#endif

#ifndef WALNUT_GB_THREADED_FAST_MEM
# define WALNUT_GB_THREADED_FAST_MEM 1
#endif

#ifndef WALNUT_GB_THREADED_COMPACT_DENSE
# define WALNUT_GB_THREADED_COMPACT_DENSE 1
#endif

#ifndef WALNUT_GB_THREADED_FAST_TABLES
# define WALNUT_GB_THREADED_FAST_TABLES 1
#endif

#if WALNUT_GB_THREADED_FAST_TABLES && defined(DRAM_ATTR)
# define WALNUT_GB_THREADED_TABLE_ATTR DRAM_ATTR
#else
# define WALNUT_GB_THREADED_TABLE_ATTR
#endif

#ifndef WALNUT_GB_CPU_EVENT_COUNTDOWN
// Disabled: event countdown still breaks timing-sensitive games.
# define WALNUT_GB_CPU_EVENT_COUNTDOWN 0
#endif

#ifndef WALNUT_GB_CPU_EVENT_MINI_COUNTDOWN
# define WALNUT_GB_CPU_EVENT_MINI_COUNTDOWN 1
#endif

#ifndef WALNUT_GB_HALT_STEP_CAP
# define WALNUT_GB_HALT_STEP_CAP 64
#endif

#ifndef WALNUT_GB_SKIP_FRAME_HEADLESS
# define WALNUT_GB_SKIP_FRAME_HEADLESS 1
#endif

/* Threaded runner batches instruction cycles and posts them to the timing
 * counters in one go. Batches never cross a timing event (cpu_event_count)
 * and are flushed before anything that observes or reschedules timing. */
#ifndef WALNUT_GB_THREADED_LAZY_POST
# define WALNUT_GB_THREADED_LAZY_POST 1
#endif

/* Must be >= the largest cycle count a single instruction can add between
 * two lazy post checks (conditional CALL taken = 24). */
#ifndef WALNUT_GB_LAZY_POST_MARGIN
# define WALNUT_GB_LAZY_POST_MARGIN 32u
#endif

#define WALNUT_GB_LAZY_POST_ACTIVE (WALNUT_GB_THREADED_LAZY_POST && \
	WALNUT_GB_THREADED_CPU_ACTIVE && \
	(WALNUT_GB_CPU_EVENT_MINI_COUNTDOWN || WALNUT_GB_CPU_EVENT_COUNTDOWN))

/* Enable 16 bit colour palette. If disabled, only four colour shades are set in
 * pixel data. */
#ifndef WALNUT_GB_12_COLOUR
# define WALNUT_GB_12_COLOUR 1
#endif

/* Adds more code to improve LCD rendering accuracy. */
#ifndef WALNUT_GB_HIGH_LCD_ACCURACY
# define WALNUT_GB_HIGH_LCD_ACCURACY 1
#endif

/* Skips common software loops that poll STAT until LCD mode is VRAM-safe. */
#ifndef WALNUT_GB_FAST_STAT_POLL
# define WALNUT_GB_FAST_STAT_POLL 0
#endif

#if WALNUT_GB_FAST_STAT_POLL && !WALNUT_GB_HIGH_LCD_ACCURACY
# define WALNUT_GB_FAST_STAT_POLL_ACTIVE 1
#else
# define WALNUT_GB_FAST_STAT_POLL_ACTIVE 0
#endif

#ifndef WALNUT_GB_FAST_STAT_POLL_DIRECT_READS
# define WALNUT_GB_FAST_STAT_POLL_DIRECT_READS 0
#endif

/* Converts side-effect-free interrupt wait loops into synthetic idle waits. */
#ifndef WALNUT_GB_FAST_POLL_WAIT
# define WALNUT_GB_FAST_POLL_WAIT 0
#endif

#if WALNUT_GB_FAST_POLL_WAIT && !WALNUT_GB_HIGH_LCD_ACCURACY
# define WALNUT_GB_FAST_POLL_WAIT_ACTIVE 1
#else
# define WALNUT_GB_FAST_POLL_WAIT_ACTIVE 0
#endif

/* Use intrinsic functions. This may produce smaller and faster code. */
#ifndef WALNUT_GB_USE_INTRINSICS
# define WALNUT_GB_USE_INTRINSICS 1
#endif

/* Enables Gameboy Color support, requires WALNUT_GB_12_COLOUR; similarly if disabled, WALNUT_GB_12_COLOUR must also be disbaled*/
#ifndef WALNUT_FULL_GBC_SUPPORT
# define WALNUT_FULL_GBC_SUPPORT 1
#endif

#if (WALNUT_GB_12_COLOUR != WALNUT_FULL_GBC_SUPPORT)
#error "WALNUT_GB_12_COLOUR and WALNUT_FULL_GBC_SUPPORT must both be enabled or both be disabled"
#endif

/* Only include function prototypes. At least one file must *not* have this
 * defined. */
// #define WALNUT_GB_HEADER_ONLY

/** Internal source code. **/
/* Interrupt masks */
#define VBLANK_INTR	0x01
#define LCDC_INTR	0x02
#define TIMER_INTR	0x04
#define SERIAL_INTR	0x08
#define CONTROL_INTR	0x10
#define ANY_INTR	0x1F

/* Memory section sizes for DMG */
#if WALNUT_FULL_GBC_SUPPORT
#define WRAM_SIZE	0x8000
#define VRAM_SIZE	0x4000
#else
#define WRAM_SIZE	0x2000
#define VRAM_SIZE	0x2000
#endif
#define HRAM_IO_SIZE	0x0100
#define OAM_SIZE	0x00A0

/* Memory addresses */
#define ROM_0_ADDR      0x0000
#define ROM_N_ADDR      0x4000
#define VRAM_ADDR       0x8000
#define CART_RAM_ADDR   0xA000
#define WRAM_0_ADDR     0xC000
#define WRAM_1_ADDR     0xD000
#define ECHO_ADDR       0xE000
#define OAM_ADDR        0xFE00
#define UNUSED_ADDR     0xFEA0
#define IO_ADDR         0xFF00
#define HRAM_ADDR       0xFF80
#define INTR_EN_ADDR    0xFFFF

/* Cart section sizes */
#define ROM_BANK_SIZE   0x4000
#define WRAM_BANK_SIZE  0x1000
#define CRAM_BANK_SIZE  0x2000
#define VRAM_BANK_SIZE  0x2000

/* DIV Register is incremented at rate of 16384Hz.
 * 4194304 / 16384 = 256 clock cycles for one increment. */
#define DIV_CYCLES          256
static const uint_fast16_t WALNUT_GB_TAC_CYCLES[4] = {1024, 16, 64, 256};

/* Serial clock locked to 8192Hz on DMG.
 * 4194304 / (8192 / 8) = 4096 clock cycles for sending 1 byte. */
#define SERIAL_CYCLES       4096
#define SERIAL_CYCLES_1KB   (SERIAL_CYCLES/1ul)
#define SERIAL_CYCLES_2KB   (SERIAL_CYCLES/2ul)
#define SERIAL_CYCLES_32KB  (SERIAL_CYCLES/32ul)
#define SERIAL_CYCLES_64KB  (SERIAL_CYCLES/64ul)

/* Calculating VSYNC. */
#define DMG_CLOCK_FREQ      4194304.0
#define SCREEN_REFRESH_CYCLES 70224.0
#define VERTICAL_SYNC       (DMG_CLOCK_FREQ/SCREEN_REFRESH_CYCLES)

/* Real Time Clock is locked to 1Hz. */
#define RTC_CYCLES	((uint_fast32_t)DMG_CLOCK_FREQ)

/* SERIAL SC register masks. */
#define SERIAL_SC_TX_START  0x80
#define SERIAL_SC_CLOCK_SRC 0x01

/* STAT register masks */
#define STAT_LYC_INTR       0x40
#define STAT_MODE_2_INTR    0x20
#define STAT_MODE_1_INTR    0x10
#define STAT_MODE_0_INTR    0x08
#define STAT_LYC_COINC      0x04
#define STAT_MODE           0x03
#define STAT_USER_BITS      0xF8

/* LCDC control masks */
#define LCDC_ENABLE         0x80
#define LCDC_WINDOW_MAP     0x40
#define LCDC_WINDOW_ENABLE  0x20
#define LCDC_TILE_SELECT    0x10
#define LCDC_BG_MAP         0x08
#define LCDC_OBJ_SIZE       0x04
#define LCDC_OBJ_ENABLE     0x02
#define LCDC_BG_ENABLE      0x01

/** LCD characteristics **/
/* There are 154 scanlines. LY < 154. */
#define LCD_VERT_LINES      154
#define LCD_WIDTH           160
#define LCD_HEIGHT          144
/* PPU cycles through modes every 456 cycles. */
#define LCD_LINE_CYCLES     456
#define LCD_MODE0_HBLANK_MAX_DRUATION	204
#define LCD_MODE0_HBLANK_MIN_DRUATION	87
#define LCD_MODE2_OAM_SCAN_DURATION	80
#define LCD_MODE3_LCD_DRAW_MIN_DURATION	172
#define LCD_MODE3_LCD_DRAW_MAX_DURATION	289
#define LCD_MODE1_VBLANK_DURATION	(LCD_LINE_CYCLES * (LCD_VERT_LINES - LCD_HEIGHT))
#define LCD_FRAME_CYCLES		(LCD_LINE_CYCLES * LCD_VERT_LINES)
/* The following assumes that Hblank starts on cycle 0. */
/* Mode 2 (OAM Scan) starts on cycle 204 (although this is dependent on the
 * duration of Mode 3 (LCD Draw). */
#define LCD_MODE_2_CYCLES   LCD_MODE0_HBLANK_MAX_DRUATION
/* Mode 3 starts on cycle 284. */
#define LCD_MODE_3_CYCLES   (LCD_MODE_2_CYCLES + LCD_MODE2_OAM_SCAN_DURATION)
/* Mode 0 starts on cycle 376. */
#define LCD_MODE_0_CYCLES   (LCD_MODE_3_CYCLES + LCD_MODE3_LCD_DRAW_MIN_DURATION)

#define LCD_MODE2_OAM_SCAN_START	0
#define LCD_MODE2_OAM_SCAN_END		(LCD_MODE2_OAM_SCAN_DURATION)
#define LCD_MODE3_LCD_DRAW_END		(LCD_MODE2_OAM_SCAN_END + LCD_MODE3_LCD_DRAW_MIN_DURATION)
#define LCD_MODE0_HBLANK_END		(LCD_MODE3_LCD_DRAW_END + LCD_MODE0_HBLANK_MAX_DRUATION)
#if LCD_MODE0_HBLANK_END != LCD_LINE_CYCLES
#error "LCD length not equal"
#endif

/* VRAM Locations */
#define VRAM_TILES_1        (0x8000 - VRAM_ADDR)
#define VRAM_TILES_2        (0x8800 - VRAM_ADDR)
#define VRAM_BMAP_1         (0x9800 - VRAM_ADDR)
#define VRAM_BMAP_2         (0x9C00 - VRAM_ADDR)
#define VRAM_TILES_3        (0x8000 - VRAM_ADDR + VRAM_BANK_SIZE)
#define VRAM_TILES_4        (0x8800 - VRAM_ADDR + VRAM_BANK_SIZE)

/* Interrupt jump addresses */
#define VBLANK_INTR_ADDR    0x0040
#define LCDC_INTR_ADDR      0x0048
#define TIMER_INTR_ADDR     0x0050
#define SERIAL_INTR_ADDR    0x0058
#define CONTROL_INTR_ADDR   0x0060

/* SPRITE controls */
#define NUM_SPRITES         0x28
#define MAX_SPRITES_LINE    0x0A
#define OBJ_PRIORITY        0x80
#define OBJ_FLIP_Y          0x40
#define OBJ_FLIP_X          0x20
#define OBJ_PALETTE         0x10
#if WALNUT_FULL_GBC_SUPPORT
#define OBJ_BANK            0x08
#define OBJ_CGB_PALETTE     0x07
#endif

#if ENABLE_LCD && WALNUT_GB_OAM_CACHE
struct walnut_gb_oam_cache_s
{
	const struct gb_s *owner;
	uint8_t valid;
	uint8_t obj_size;
#if WALNUT_FULL_GBC_SUPPORT
	uint8_t cgb_mode;
#endif
#if WALNUT_GB_HIGH_LCD_ACCURACY
	uint8_t line_count[LCD_HEIGHT];
	uint8_t line_sprites[LCD_HEIGHT][MAX_SPRITES_LINE + 1];
#else
	uint32_t line_mask_lo[LCD_HEIGHT];
	uint8_t line_mask_hi[LCD_HEIGHT];
#endif
};

static struct walnut_gb_oam_cache_s s_walnut_gb_oam_cache;

static inline void __gb_oam_cache_invalidate(struct gb_s *gb)
{
	if(s_walnut_gb_oam_cache.owner == NULL ||
			s_walnut_gb_oam_cache.owner == gb)
		s_walnut_gb_oam_cache.valid = 0;
}
#else
#define __gb_oam_cache_invalidate(gb) do { (void)(gb); } while(0)
#endif

/* Joypad buttons */
#define JOYPAD_A            0x01
#define JOYPAD_B            0x02
#define JOYPAD_SELECT       0x04
#define JOYPAD_START        0x08
#define JOYPAD_RIGHT        0x10
#define JOYPAD_LEFT         0x20
#define JOYPAD_UP           0x40
#define JOYPAD_DOWN         0x80

#define ROM_HEADER_CHECKSUM_LOC	0x014D

/* Local macros. */
#ifndef MIN
# define MIN(a, b)          ((a) < (b) ? (a) : (b))
#endif

#define WALNUT_GB_ARRAYSIZE(array)    (sizeof(array)/sizeof(array[0]))

/** Allow setting deprecated functions and variables. */
#if (defined(__GNUC__) && __GNUC__ >= 6) || (defined(__clang__) && __clang_major__ >= 4)
# define WGB_DEPRECATED(msg) __attribute__((deprecated(msg)))
#else
# define WGB_DEPRECATED(msg)
#endif

#if !defined(__has_builtin)
/* Stub __has_builtin if it isn't available. */
# define __has_builtin(x) 0
#endif

/* The WGB_UNREACHABLE() macro tells the compiler that the code path will never
 * be reached, allowing for further optimisation. */
#if !defined(WGB_UNREACHABLE)
# if __has_builtin(__builtin_unreachable)
#  define WGB_UNREACHABLE() __builtin_unreachable()
# elif defined(_MSC_VER) && _MSC_VER >= 1200
#  /* __assume is not available before VC6. */
#  define WGB_UNREACHABLE() __assume(0)
# else
#  define WGB_UNREACHABLE() abort()
# endif
#endif /* !defined(WGB_UNREACHABLE) */

#if !defined(WGB_UNLIKELY)
# if __has_builtin(__builtin_expect)
#  define WGB_UNLIKELY(expr) __builtin_expect(!!(expr), 0)
# else
#  define WGB_UNLIKELY(expr) (expr)
# endif
#endif /* !defined(WGB_UNLIKELY) */
#if !defined(WGB_LIKELY)
# if __has_builtin(__builtin_expect)
#  define WGB_LIKELY(expr) __builtin_expect(!!(expr), 1)
# else
#  define WGB_LIKELY(expr) (expr)
# endif
#endif /* !defined(WGB_LIKELY) */
#if WALNUT_GB_USE_INTRINSICS
/* If using MSVC, only enable intrinsics for x86 platforms*/
# if defined(_MSC_VER) && __has_include("intrin.h") && \
	(defined(_M_IX86_FP) || defined(_M_AMD64) || defined(_M_X64))
/* Define intrinsic functions for MSVC. */
#  include <intrin.h>
#  define WGB_INTRIN_SBC(x,y,cin,res) _subborrow_u8(cin,x,y,&res)
#  define WGB_INTRIN_ADC(x,y,cin,res) _addcarry_u8(cin,x,y,&res)
# endif /* MSVC */

/* Check for intrinsic functions in GCC and Clang. */
# if __has_builtin(__builtin_sub_overflow)
#  define WGB_INTRIN_SBC(x,y,cin,res) __builtin_sub_overflow(x,y+cin,&res)
#  define WGB_INTRIN_ADC(x,y,cin,res) __builtin_add_overflow(x,y+cin,&res)
# endif
#endif /* WALNUT_GB_USE_INTRINSICS */

#ifndef WGB_CPU_REG
# define WGB_CPU_REG gb->cpu_reg
#endif

#if defined(WGB_INTRIN_SBC)
# define WGB_INSTR_SBC_R8(r,cin)						\
	{									\
		uint8_t temp;							\
		WGB_CPU_REG.f.f_bits.c = WGB_INTRIN_SBC(WGB_CPU_REG.a,r,cin,temp);\
		WGB_CPU_REG.f.f_bits.h = ((WGB_CPU_REG.a ^ r ^ temp) & 0x10) > 0;\
		WGB_CPU_REG.f.f_bits.n = 1;					\
		WGB_CPU_REG.f.f_bits.z = (temp == 0x00);			\
		WGB_CPU_REG.a = temp;						\
	}

# define WGB_INSTR_CP_R8(r)							\
	{									\
		uint8_t temp;							\
		WGB_CPU_REG.f.f_bits.c = WGB_INTRIN_SBC(WGB_CPU_REG.a,r,0,temp);\
		WGB_CPU_REG.f.f_bits.h = ((WGB_CPU_REG.a ^ r ^ temp) & 0x10) > 0;\
		WGB_CPU_REG.f.f_bits.n = 1;					\
		WGB_CPU_REG.f.f_bits.z = (temp == 0x00);			\
	}
#else
# define WGB_INSTR_SBC_R8(r,cin)						\
	{									\
		uint16_t temp = WGB_CPU_REG.a - (r + cin);			\
		WGB_CPU_REG.f.f_bits.c = (temp & 0xFF00) ? 1 : 0;		\
		WGB_CPU_REG.f.f_bits.h = ((WGB_CPU_REG.a ^ r ^ temp) & 0x10) > 0; \
		WGB_CPU_REG.f.f_bits.n = 1;					\
		WGB_CPU_REG.f.f_bits.z = ((temp & 0xFF) == 0x00);		\
		WGB_CPU_REG.a = (temp & 0xFF);					\
	}

# define WGB_INSTR_CP_R8(r)							\
	{									\
		uint16_t temp = WGB_CPU_REG.a - r;				\
		WGB_CPU_REG.f.f_bits.c = (temp & 0xFF00) ? 1 : 0;		\
		WGB_CPU_REG.f.f_bits.h = ((WGB_CPU_REG.a ^ r ^ temp) & 0x10) > 0; \
		WGB_CPU_REG.f.f_bits.n = 1;					\
		WGB_CPU_REG.f.f_bits.z = ((temp & 0xFF) == 0x00);		\
	}
#endif  /* WGB_INTRIN_SBC */

#if defined(WGB_INTRIN_ADC)
# define WGB_INSTR_ADC_R8(r,cin)						\
	{									\
		uint8_t temp;							\
		WGB_CPU_REG.f.f_bits.c = WGB_INTRIN_ADC(WGB_CPU_REG.a,r,cin,temp);\
		WGB_CPU_REG.f.f_bits.h = ((WGB_CPU_REG.a ^ r ^ temp) & 0x10) > 0; \
		WGB_CPU_REG.f.f_bits.n = 0;					\
		WGB_CPU_REG.f.f_bits.z = (temp == 0x00);			\
		WGB_CPU_REG.a = temp;						\
	}
#else
# define WGB_INSTR_ADC_R8(r,cin)						\
	{									\
		uint16_t temp = WGB_CPU_REG.a + r + cin;			\
		WGB_CPU_REG.f.f_bits.c = (temp & 0xFF00) ? 1 : 0;		\
		WGB_CPU_REG.f.f_bits.h = ((WGB_CPU_REG.a ^ r ^ temp) & 0x10) > 0; \
		WGB_CPU_REG.f.f_bits.n = 0;					\
		WGB_CPU_REG.f.f_bits.z = ((temp & 0xFF) == 0x00);		\
		WGB_CPU_REG.a = (temp & 0xFF);					\
	}
#endif /* WGB_INTRIN_ADC */

#define WGB_INSTR_INC_R8(r)							\
	r++;									\
	WGB_CPU_REG.f.f_bits.h = ((r & 0x0F) == 0x00);				\
	WGB_CPU_REG.f.f_bits.n = 0;						\
	WGB_CPU_REG.f.f_bits.z = (r == 0x00)

#define WGB_INSTR_DEC_R8(r)							\
	r--;									\
	WGB_CPU_REG.f.f_bits.h = ((r & 0x0F) == 0x0F);				\
	WGB_CPU_REG.f.f_bits.n = 1;						\
	WGB_CPU_REG.f.f_bits.z = (r == 0x00)

#define WGB_INSTR_XOR_R8(r)							\
	WGB_CPU_REG.a ^= r;							\
	WGB_CPU_REG.f.reg = 0;							\
	WGB_CPU_REG.f.f_bits.z = (WGB_CPU_REG.a == 0x00)

#define WGB_INSTR_OR_R8(r)							\
	WGB_CPU_REG.a |= r;							\
        WGB_CPU_REG.f.reg = 0;							\
	WGB_CPU_REG.f.f_bits.z = (WGB_CPU_REG.a == 0x00)

#define WGB_INSTR_AND_R8(r)							\
	WGB_CPU_REG.a &= r;							\
	WGB_CPU_REG.f.reg = 0;							\
	WGB_CPU_REG.f.f_bits.z = (WGB_CPU_REG.a == 0x00);			\
	WGB_CPU_REG.f.f_bits.h = 1

#if WALNUT_GB_IS_LITTLE_ENDIAN
# define WALNUT_GB_GET_LSB16(x) (x & 0xFF)
# define WALNUT_GB_GET_MSB16(x) (x >> 8)
# define WALNUT_GB_GET_MSN16(x) (x >> 12)
# define WALNUT_GB_U8_TO_U16(h,l) ((l) | ((h) << 8))
#else
# define WALNUT_GB_GET_LSB16(x) (x >> 8)
# define WALNUT_GB_GET_MSB16(x) (x & 0xFF)
# define WALNUT_GB_GET_MSN16(x) ((x & 0xF0) >> 4)
# define WALNUT_GB_U8_TO_U16(h,l) ((h) | ((l) << 8))
#endif

static inline uint16_t bgr555_to_rgb565BE_accurate(uint16_t c)
{
	uint16_t r = c & 0x001F;        // bits 4–0
	uint16_t g = (c >> 5) & 0x001F;        // bits 9–5
	uint16_t b = (c >> 10) & 0x001F;        // bits 14–10

	g = (g << 1) | (g >> 4);                // 5 → 6-bit expansion

	uint16_t tempRGB565 = (r << 11) | (g << 5) | b;
    	return (tempRGB565 << 8) | (tempRGB565 >> 8); // swap bytes
}

static inline uint16_t bgr555_to_rgb565_accurate(uint16_t c)
{
	uint16_t r = c & 0x001F;        // bits 4–0
	uint16_t g = (c >> 5) & 0x001F;        // bits 9–5
	uint16_t b = (c >> 10) & 0x001F;        // bits 14–10

	g = (g << 1) | (g >> 4);                // 5 → 6-bit expansion

	return (r << 11) | (g << 5) | b;
}

static inline uint16_t bgr555_to_rgb565_fast(uint16_t c)
{
    return ((c & 0x7C00) >> 10) | ((c & 0x03E0) << 1) | ((c & 0x001F) << 11); // Green → middle 6 bits (shift left by 1), loose 1.56% brightness fidelity loss on the green channel in exchange for faster conversion, can replace with more accurate conversion if desired - this is faster but may cause issues for gamma filters
}

struct cpu_registers_s
{
/* Change register order if big endian.
 * Macro receives registers in little endian order. */
#if WALNUT_GB_IS_LITTLE_ENDIAN
# define WALNUT_GB_LE_REG(x,y) x,y
#else
# define WALNUT_GB_LE_REG(x,y) y,x
#endif
	/* Define specific bits of Flag register. */
	union {
		struct {
			uint8_t  : 4; /* Unused. */
			uint8_t c: 1; /* Carry flag. */
			uint8_t h: 1; /* Half carry flag. */
			uint8_t n: 1; /* Add/sub flag. */
			uint8_t z: 1; /* Zero flag. */
		} f_bits;
		uint8_t reg;
	} f;
	uint8_t a;

	union
	{
		struct
		{
			uint8_t WALNUT_GB_LE_REG(c,b);
		} bytes;
		uint16_t reg;
	} bc;

	union
	{
		struct
		{
			uint8_t WALNUT_GB_LE_REG(e,d);
		} bytes;
		uint16_t reg;
	} de;

	union
	{
		struct
		{
			uint8_t WALNUT_GB_LE_REG(l,h);
		} bytes;
		uint16_t reg;
	} hl;

	/* Stack pointer */
	union
	{
		struct
		{
			uint8_t WALNUT_GB_LE_REG(p, s);
		} bytes;
		uint16_t reg;
	} sp;

	/* Program counter */
	union
	{
		struct
		{
			uint8_t WALNUT_GB_LE_REG(c, p);
		} bytes;
		uint16_t reg;
	} pc;
#undef WALNUT_GB_LE_REG
};

#if WALNUT_GB_THREADED_COMPACT_DENSE
typedef char walnut_gb_dense_register_layout_must_match[
	offsetof(struct cpu_registers_s, f) == 0 &&
	offsetof(struct cpu_registers_s, a) == 1 &&
	offsetof(struct cpu_registers_s, bc) == 2 &&
	offsetof(struct cpu_registers_s, de) == 4 &&
	offsetof(struct cpu_registers_s, hl) == 6 ? 1 : -1];
#endif

struct count_s
{
	uint_fast16_t lcd_count;	/* LCD Timing */
	uint_fast16_t div_count;	/* Divider Register Counter */
	uint_fast16_t tima_count;	/* Timer Counter */
	uint_fast16_t serial_count;	/* Serial Counter */
	uint_fast32_t rtc_count;	/* RTC Counter */
	uint_fast32_t lcd_off_count;	/* Cycles LCD has been disabled */
	uint_fast32_t lcd_event_count;	/* LCD cycles until next scheduled event */
	uint_fast32_t cpu_event_count;	/* CPU cycles until next slow timing event */
	uint_fast8_t cpu_event_flags;	/* Cached active timing flags for cpu_event_count */
};

#if ENABLE_LCD
	/* Bit mask for the shade of pixel to display */
	#define LCD_COLOUR	0x03

# if WALNUT_GB_12_COLOUR
	/**
	* Bit mask for whether a pixel is OBJ0, OBJ1, or BG. Each may have a different
	* palette when playing a DMG game on CGB.
	*/
	#define LCD_PALETTE_OBJ	0x10
	#define LCD_PALETTE_BG	0x20
	/**
	* Bit mask for the two bits listed above.
	* LCD_PALETTE_ALL == 0b00 --> OBJ0
	* LCD_PALETTE_ALL == 0b01 --> OBJ1
	* LCD_PALETTE_ALL == 0b10 --> BG
	* LCD_PALETTE_ALL == 0b11 --> NOT POSSIBLE
	*/
	#define LCD_PALETTE_ALL 0x30
# endif
#endif

/**
 * Errors that may occur during emulation.
 */
enum gb_error_e
{
	GB_UNKNOWN_ERROR = 0,
	GB_INVALID_OPCODE = 1,
	GB_INVALID_READ = 2,
	GB_INVALID_WRITE = 3,

	/* GB_HALT_FOREVER is deprecated and will no longer be issued as an
	 * error by Walnut-GB. */
	GB_HALT_FOREVER WGB_DEPRECATED("Error no longer issued by Walnut-GB") = 4,

	GB_INVALID_MAX = 5
};

/**
 * Errors that may occur during library initialisation.
 */
enum gb_init_error_e
{
	GB_INIT_NO_ERROR = 0,
	GB_INIT_CARTRIDGE_UNSUPPORTED,
	GB_INIT_INVALID_CHECKSUM,

	GB_INIT_INVALID_MAX
};

/**
 * Return codes for serial receive function, mainly for clarity.
 */
enum gb_serial_rx_ret_e
{
	GB_SERIAL_RX_SUCCESS = 0,
	GB_SERIAL_RX_NO_CONNECTION = 1
};

union cart_rtc
{
	struct
	{
		uint8_t sec;
		uint8_t min;
		uint8_t hour;
		uint8_t yday;
		uint8_t high;
	} reg;
	uint8_t bytes[5];
};

/**
 * Emulator context.
 *
 * Only values within the `direct` struct may be modified directly by the
 * front-end implementation. Other variables must not be modified.
 */
struct gb_s
{

#if (WALNUT_GB_SAFE_DUALFETCH_DMA || WALNUT_GB_SAFE_DUALFETCH_MBC)
	bool prefetch_invalid;
#endif
	/**
	 * Return byte from ROM at given address.
	 *
	 * \param gb_s	emulator context
	 * \param addr	address
	 * \return		byte at address in ROM
	 */
	uint8_t (*gb_rom_read)(struct gb_s*, const uint_fast32_t addr);

	/**
	 * Return 2 bytes from ROM at given address.
	 *
	 * \param gb_s	emulator context
	 * \param addr	address
	 * \return		byte at address in ROM
	 */
	uint16_t (*gb_rom_read_16bit)(struct gb_s*, const uint_fast32_t addr);

	/**
	 * Return 4 bytes from ROM at given address.
	 *
	 * \param gb_s	emulator context
	 * \param addr	address
	 * \return		byte at address in ROM
	 */
	uint32_t (*gb_rom_read_32bit)(struct gb_s*, const uint_fast32_t addr);

	/**
	 * Return byte from cart RAM at given address.
	 *
	 * \param gb_s	emulator context
	 * \param addr	address
	 * \return		byte at address in RAM
	 */
	uint8_t (*gb_cart_ram_read)(struct gb_s*, const uint_fast32_t addr);

	/**
	 * Write byte to cart RAM at given address.
	 *
	 * \param gb_s	emulator context
	 * \param addr	address
	 * \param val	value to write to address in RAM
	 */
	void (*gb_cart_ram_write)(struct gb_s*, const uint_fast32_t addr,
				  const uint8_t val);

	/**
	 * Notify front-end of error.
	 *
	 * \param gb_s		emulator context
	 * \param gb_error_e	error code
	 * \param addr		address of where error occurred
	 */
	void (*gb_error)(struct gb_s*, const enum gb_error_e, const uint16_t addr);

	/* Transmit one byte and return the received byte. */
	void (*gb_serial_tx)(struct gb_s*, const uint8_t tx);
	enum gb_serial_rx_ret_e (*gb_serial_rx)(struct gb_s*, uint8_t* rx);

	/* Read byte from boot ROM at given address. */
	uint8_t (*gb_bootrom_read)(struct gb_s*, const uint_fast16_t addr);

	struct
	{
		bool gb_halt	: 1;
		bool gb_ime	: 1;
		/* gb_frame is set when 0.016742706298828125 seconds have
		 * passed. It is likely that a new frame has been drawn since
		 * then, but it is possible that the LCD was switched off and
		 * nothing was drawn. */
		bool gb_frame	: 1;
		bool lcd_blank	: 1;
		/* Set if MBC3O cart is used. */
		bool cart_is_mbc3O : 1;
		/* Set if Pocket Camera cartridge type $FC is used. */
		bool cart_is_pocket_camera : 1;
	};

	/* Cartridge information:
	 * Memory Bank Controller (MBC) type. */
	int8_t mbc;
	/* Whether the MBC has internal RAM. */
	uint8_t cart_ram;
	/* Number of ROM banks in cartridge. */
	uint16_t num_rom_banks_mask;
	/* Number of RAM banks in cartridge. Ignore for MBC2. */
	uint8_t num_ram_banks;

	uint16_t selected_rom_bank;
	uint32_t selected_rom_bank_offset;
	/* WRAM and VRAM bank selection not available. */
	uint8_t cart_ram_bank;
	uint8_t enable_cart_ram;
	/* Cartridge ROM/RAM mode select. */
	uint8_t cart_mode_select;
	uint8_t mbc5_ram_bank_mask;
	uint8_t pocket_camera_regs[0x40];

	union cart_rtc rtc_latched, rtc_real;

	struct cpu_registers_s cpu_reg;
	//struct gb_registers_s gb_reg;
	struct count_s counter;

	/* Cycles executed by the threaded runner but not yet applied to
	 * counter (lazy post-step batch). NULL outside
	 * gb_run_frame_threaded(); helpers flush through this pointer before
	 * observing or rescheduling timing state. */
	uint_fast32_t *lazy_post_pending;

	/* TODO: Allow implementation to allocate WRAM, VRAM and Frame Buffer. */
	uint8_t wram[WRAM_SIZE];
	uint8_t vram[VRAM_SIZE];
	uint8_t oam[OAM_SIZE];
	uint8_t hram_io[HRAM_IO_SIZE];

	struct
	{
		/**
		 * Draw line on screen.
		 *
		 * \param gb_s		emulator context
		 * \param pixels	The 160 pixels to draw.
		 * 			Bits 1-0 are the colour to draw.
		 * 			Bits 5-4 are the palette, where:
		 * 				OBJ0 = 0b00,
		 * 				OBJ1 = 0b01,
		 * 				BG = 0b10
		 * 			Other bits are undefined.
		 * 			Bits 5-4 are only required by front-ends
		 * 			which want to use a different colour for
		 * 			different object palettes. This is what
		 * 			the Game Boy Color (CGB) does to DMG
		 * 			games.
		 * \param line		Line to draw pixels on. This is
		 * guaranteed to be between 0-144 inclusive.
		 */
		void (*lcd_draw_line)(struct gb_s *gb,
				const uint8_t *pixels,
				const uint_fast8_t line);

		/* Palettes */
		uint8_t bg_palette[4];
		uint8_t sp_palette[8];

		uint8_t window_clear;
		uint8_t WY;

		/* Only support 30fps frame skip. */
		bool frame_skip_count : 1;
		bool interlace_count : 1;
#if WALNUT_GB_DIRTY_FRAME_SKIP
		bool frame_dirty : 1;
		bool frame_dirty_active : 1;
#endif
	} display;

#if WALNUT_FULL_GBC_SUPPORT
	/* Game Boy Color Mode*/
	struct {
		uint8_t cgbMode;
		uint8_t doubleSpeed;
		uint8_t doubleSpeedPrep;
		uint8_t wramBank;
		uint16_t wramBankOffset;
		uint8_t vramBank;
		uint16_t vramBankOffset;
		uint16_t fixPalette[0x40];  //BG then OAM palettes fixed for the screen
		uint8_t OAMPalette[0x40];
		uint8_t BGPalette[0x40];
		uint8_t OAMPaletteID;
		uint8_t BGPaletteID;
		uint8_t OAMPaletteInc;
		uint8_t BGPaletteInc;
		uint8_t dmaActive;
		uint8_t dmaMode;
		uint8_t dmaSize;
		uint16_t dmaSource;
		uint16_t dmaDest;
	} cgb;
#endif

	/**
	 * Variables that may be modified directly by the front-end.
	 * This method seems to be easier and possibly less overhead than
	 * calling a function to modify these variables each time.
	 *
	 * None of this is thread-safe.
	 */
	struct
	{
		/* Set to enable interlacing. Interlacing will start immediately
		 * (at the next line drawing).
		 */
		bool interlace : 1;
		bool frame_skip : 1;
		bool suppress_lcd_stat_interrupts : 1;

		union
		{
			struct
			{
				/* Using this bitfield is deprecated due to
				 * portability concerns. It is recommended to
				 * use the JOYPAD_* defines instead.
				 */
				bool a		: 1;
				bool b		: 1;
				bool select	: 1;
				bool start	: 1;
				bool right	: 1;
				bool left	: 1;
				bool up		: 1;
				bool down	: 1;
			} joypad_bits;
			uint8_t joypad;
		};

		/* Implementation defined data. Set to NULL if not required. */
		void *priv;

#if WALNUT_FULL_GBC_SUPPORT
		/* Optional notification when a CGB palette entry changes. */
		void (*palette_changed)(struct gb_s*, uint8_t index, uint16_t rgb565);
#endif

		/* Optional mapped ROM prefix for direct instruction/data fetches. */
		const uint8_t *rom_data;
		uint32_t rom_data_len;
	} direct;
};

enum
{
	WGB_POLLWAIT_ACCEPT = 0,
	WGB_POLLWAIT_REJ_NOT_BACKWARD = 1,
	WGB_POLLWAIT_REJ_NOT_ROM = 2,
	WGB_POLLWAIT_REJ_TOO_LONG = 3,
	WGB_POLLWAIT_REJ_PEEK = 4,
	WGB_POLLWAIT_REJ_IME_OFF = 5,
	WGB_POLLWAIT_REJ_NO_IE = 6,
	WGB_POLLWAIT_REJ_IRQ_PENDING = 7,
	WGB_POLLWAIT_REJ_UNSAFE_ADDR = 8,
	WGB_POLLWAIT_REJ_UNSUPPORTED = 9
};

enum
{
	WGB_POLLWAIT_ADDR_UNKNOWN = 0,
	WGB_POLLWAIT_ADDR_WRAM = 1,
	WGB_POLLWAIT_ADDR_IO = 2,
	WGB_POLLWAIT_ADDR_HRAM = 3,
	WGB_POLLWAIT_ADDR_EXT = 4,
	WGB_POLLWAIT_ADDR_OTHER = 5
};

enum
{
	WGB_LOOP_BRANCH_NZ = 0,
	WGB_LOOP_BRANCH_Z = 1,
	WGB_LOOP_BRANCH_NC = 2,
	WGB_LOOP_BRANCH_C = 3
};

enum
{
	WGB_LOOP_CLASS_OTHER = 0,
	WGB_LOOP_CLASS_FILL_DEC = 1,
	WGB_LOOP_CLASS_CALL = 2,
	WGB_LOOP_CLASS_SCAN_RET = 3,
	WGB_LOOP_CLASS_HLI_SCAN = 4,
	WGB_LOOP_CLASS_WRITE = 5,
	WGB_LOOP_CLASS_READ_TEST = 6,
	WGB_LOOP_CLASS_BRANCH = 7
};


#ifndef WALNUT_GB_HEADER_ONLY

#define IO_JOYP	0x00
#define IO_SB	0x01
#define IO_SC	0x02
#define IO_DIV	0x04
#define IO_TIMA	0x05
#define IO_TMA	0x06
#define IO_TAC	0x07
#define IO_IF	0x0F
#define IO_LCDC	0x40
#define IO_STAT	0x41
#define IO_SCY	0x42
#define IO_SCX	0x43
#define IO_LY	0x44
#define IO_LYC	0x45
#define	IO_DMA	0x46
#define	IO_BGP	0x47
#define	IO_OBP0	0x48
#define IO_OBP1	0x49
#define IO_WY	0x4A
#define IO_WX	0x4B
#define IO_BOOT	0x50
#define IO_IE	0xFF

#define IO_TAC_RATE_MASK	0x3
#define IO_TAC_ENABLE_MASK	0x4

/* LCD Mode defines. */
#define IO_STAT_MODE_HBLANK		0
#define IO_STAT_MODE_VBLANK		1
#define IO_STAT_MODE_OAM_SCAN		2
#define IO_STAT_MODE_LCD_DRAW		3
#define IO_STAT_MODE_VBLANK_OR_TRANSFER_MASK 0x1

static inline void __gb_display_mark_dirty(struct gb_s *gb)
{
#if WALNUT_GB_DIRTY_FRAME_SKIP
	gb->display.frame_dirty = true;
#else
	(void)gb;
#endif
}

static inline bool __gb_direct_rom_contains(const struct gb_s *gb,
		uint32_t addr, uint32_t len)
{
	/* rom_data_len is 0 when rom_data is NULL, and addr + len can't wrap
	 * for bank-window addresses. */
	return addr + len <= gb->direct.rom_data_len;
}

static inline uint8_t __gb_direct_rom_read8(const struct gb_s *gb, uint32_t addr)
{
	return gb->direct.rom_data[addr];
}

static inline uint16_t __gb_direct_rom_read16(const struct gb_s *gb, uint32_t addr)
{
	const uint8_t *src = gb->direct.rom_data + addr;
	if(WGB_LIKELY(((uintptr_t)src & 1u) == 0))
		return *(const uint16_t *)src;
	return (uint16_t)src[0] | ((uint16_t)src[1] << 8);
}

static inline uint32_t __gb_direct_rom_read32(const struct gb_s *gb, uint32_t addr)
{
	const uint8_t *src = gb->direct.rom_data + addr;
	if(WGB_LIKELY(((uintptr_t)src & 3u) == 0))
		return *(const uint32_t *)src;
	return (uint32_t)src[0] |
		((uint32_t)src[1] << 8) |
		((uint32_t)src[2] << 16) |
		((uint32_t)src[3] << 24);
}

static inline uint32_t __gb_compute_rom_bank_offset(const struct gb_s *gb)
{
	uint16_t bank = gb->selected_rom_bank;

	if(gb->mbc == 1 && gb->cart_mode_select)
		bank &= 0x1F;

	return ((uint32_t)bank - 1u) * ROM_BANK_SIZE;
}

static inline void __gb_refresh_rom_bank_offset(struct gb_s *gb)
{
	gb->selected_rom_bank_offset = __gb_compute_rom_bank_offset(gb);
}

static inline uint32_t __gb_fetch_rom_bank_offset(const struct gb_s *gb)
{
	return gb->selected_rom_bank_offset;
}

#define POCKET_CAMERA_REG_BANK_MASK 0x10
#define POCKET_CAMERA_IMAGE_OFFSET 0x0100u
#define POCKET_CAMERA_IMAGE_TILE_COLS 16u
#define POCKET_CAMERA_IMAGE_TILE_ROWS 14u
#define POCKET_CAMERA_TILE_BYTES 16u

static inline bool __gb_pocket_camera_regs_selected(const struct gb_s *gb)
{
	return gb->cart_is_pocket_camera &&
		(gb->cart_ram_bank & POCKET_CAMERA_REG_BANK_MASK) != 0;
}

static inline void __gb_pocket_camera_write_dummy_frame(struct gb_s *gb)
{
	if(!gb->gb_cart_ram_write)
		return;

	for(uint_fast8_t tile_y = 0; tile_y < POCKET_CAMERA_IMAGE_TILE_ROWS; ++tile_y)
	{
		for(uint_fast8_t tile_x = 0; tile_x < POCKET_CAMERA_IMAGE_TILE_COLS; ++tile_x)
		{
			const uint_fast32_t tile_offset = POCKET_CAMERA_IMAGE_OFFSET +
				((uint_fast32_t)tile_y * POCKET_CAMERA_IMAGE_TILE_COLS + tile_x) *
				POCKET_CAMERA_TILE_BYTES;

			for(uint_fast8_t row = 0; row < 8; ++row)
			{
				uint8_t lo = 0;
				uint8_t hi = 0;
				const uint_fast8_t y = (uint_fast8_t)(tile_y * 8 + row);

				for(uint_fast8_t col = 0; col < 8; ++col)
				{
					const uint_fast8_t x = (uint_fast8_t)(tile_x * 8 + col);
					uint8_t colour;
					if(x < 2 || x >= 126 || y < 2 || y >= 110)
						colour = 3;
					else
						colour = (uint8_t)(((x >> 4) + (y >> 4) + ((x ^ y) >> 5)) & 3);

					const uint8_t bit = (uint8_t)(0x80u >> col);
					if(colour & 1)
						lo |= bit;
					if(colour & 2)
						hi |= bit;
				}

				gb->gb_cart_ram_write(gb, tile_offset + row * 2, lo);
				gb->gb_cart_ram_write(gb, tile_offset + row * 2 + 1, hi);
			}
		}
	}
}

static inline uint8_t __gb_pocket_camera_read(struct gb_s *gb,
		uint_fast16_t addr)
{
	const uint_fast16_t reg = (addr - CART_RAM_ADDR) & 0x7F;
	if(reg == 0)
		return gb->pocket_camera_regs[0] & 0x07;

	/* Registers other than A000 are write-only on the camera controller. */
	return 0x00;
}

static inline uint16_t __gb_pocket_camera_read16(struct gb_s *gb,
		uint_fast16_t addr)
{
	return (uint16_t)__gb_pocket_camera_read(gb, addr) |
		((uint16_t)__gb_pocket_camera_read(gb, addr + 1) << 8);
}

static inline uint32_t __gb_pocket_camera_read32(struct gb_s *gb,
		uint_fast16_t addr)
{
	return (uint32_t)__gb_pocket_camera_read(gb, addr) |
		((uint32_t)__gb_pocket_camera_read(gb, addr + 1) << 8) |
		((uint32_t)__gb_pocket_camera_read(gb, addr + 2) << 16) |
		((uint32_t)__gb_pocket_camera_read(gb, addr + 3) << 24);
}

static inline void __gb_pocket_camera_write(struct gb_s *gb,
		uint_fast16_t addr, uint8_t val)
{
	const uint_fast16_t reg = (addr - CART_RAM_ADDR) & 0x7F;
	if(reg >= WALNUT_GB_ARRAYSIZE(gb->pocket_camera_regs))
		return;

	if(reg == 0 && (val & 0x01))
	{
		__gb_pocket_camera_write_dummy_frame(gb);
		/* Bit 0 starts capture on real hardware. Complete immediately for now. */
		val &= (uint8_t)~0x01u;
	}
	gb->pocket_camera_regs[reg] = val;
}

#if WALNUT_GB_16BIT_ALIGNED
uint16_t WALNUT_GB_HOT_ATTR __gb_read16(struct gb_s *gb, uint16_t addr)
{
    switch (WALNUT_GB_GET_MSN16(addr))
    {
        // --- Boot ROM / Fixed ROM 0
        case 0x0:
            if (gb->hram_io[IO_BOOT] == 0 && addr < 0x0100)
                return (uint16_t)gb->gb_bootrom_read(gb, addr)
                     | ((uint16_t)gb->gb_bootrom_read(gb, addr + 1) << 8);
#if WALNUT_FULL_GBC_SUPPORT
            else if (gb->cgb.cgbMode && gb->hram_io[IO_BOOT] == 0 && addr >= 0x0200 && addr < 0x0900)
                return (uint16_t)gb->gb_bootrom_read(gb, addr)
                     | ((uint16_t)gb->gb_bootrom_read(gb, addr + 1) << 8);
#endif
            /* fallthrough */
        case 0x1:
        case 0x2:
        case 0x3:
            if (__gb_direct_rom_contains(gb, addr, 2))
                return __gb_direct_rom_read16(gb, addr);
            return gb->gb_rom_read_16bit(gb, addr);

        // --- Switchable ROM banks (0x4000–0x7FFF)
        case 0x4:
        case 0x5:
        case 0x6:
        case 0x7:
        {
            uint32_t bank_offset = __gb_fetch_rom_bank_offset(gb);
            if (__gb_direct_rom_contains(gb, addr + bank_offset, 2))
                return __gb_direct_rom_read16(gb, addr + bank_offset);
            return gb->gb_rom_read_16bit(gb, addr + bank_offset);
        }

        // --- VRAM (0x8000–0x9FFF)
        case 0x8:
        case 0x9:
        {
            uint8_t *vram_base =
#if WALNUT_FULL_GBC_SUPPORT
                &gb->vram[addr - gb->cgb.vramBankOffset];
#else
                &gb->vram[addr - VRAM_ADDR];
#endif
            if (addr + 1 < 0xA000)
            {
                if (((uintptr_t)vram_base & 1) == 0)
                    return *(uint16_t *)vram_base;
                else
                    return (uint16_t)vram_base[0] | ((uint16_t)vram_base[1] << 8);
            }
            return vram_base[0]; // last byte fallback
        }

        // --- External RAM / RTC (0xA000–0xBFFF)
        case 0xA:
        case 0xB:
            if (__gb_pocket_camera_regs_selected(gb))
                return __gb_pocket_camera_read16(gb, addr);
            else if (gb->mbc == 3 && gb->cart_ram_bank >= 0x08)
                return gb->rtc_latched.bytes[gb->cart_ram_bank - 0x08]
                     | ((uint16_t)gb->rtc_latched.bytes[gb->cart_ram_bank - 0x08 + 1] << 8);
            else if (gb->cart_ram &&
                     (gb->enable_cart_ram || gb->cart_is_pocket_camera))
            {
                uint16_t offset;
                if (gb->mbc == 2)
                    offset = addr & 0x1FF;
                else if ((gb->cart_mode_select || gb->mbc != 1) && gb->cart_ram_bank < gb->num_ram_banks)
                    offset = addr - CART_RAM_ADDR + (gb->cart_ram_bank * CRAM_BANK_SIZE);
                else
                    offset = addr - CART_RAM_ADDR;

                return gb->gb_cart_ram_read(gb, offset)
                     | ((uint16_t)gb->gb_cart_ram_read(gb, offset + 1) << 8);
            }
            return 0xFFFF;

        // --- Work RAM (0xC000–0xDFFF)
        case 0xC:
        case 0xD:
        {
            uint8_t *wram_ptr =
#if WALNUT_FULL_GBC_SUPPORT
                (gb->cgb.cgbMode && addr >= WRAM_1_ADDR)
                    ? &gb->wram[addr - gb->cgb.wramBankOffset]
                    : &gb->wram[addr - WRAM_0_ADDR];
#else
                &gb->wram[addr - WRAM_0_ADDR];
#endif
            if (((uintptr_t)wram_ptr & 1) == 0)
                return *(uint16_t *)wram_ptr;
            else
                return (uint16_t)wram_ptr[0] | ((uint16_t)wram_ptr[1] << 8);
        }

        // --- Echo RAM (0xE000–0xFDFF)
        case 0xE:
        {
            uint8_t *echo_ptr = &gb->wram[addr - ECHO_ADDR];
            if (((uintptr_t)echo_ptr & 1) == 0)
                return *(uint16_t *)echo_ptr;
            else
                return (uint16_t)echo_ptr[0] | ((uint16_t)echo_ptr[1] << 8);
        }

        // --- OAM / HRAM / IO (0xFE00–0xFFFF)
        case 0xF:
            if (addr < 0xFEA0)
            {
                uint8_t *oam_ptr = &gb->oam[addr - OAM_ADDR];
                if (((uintptr_t)oam_ptr & 1) == 0)
                    return *(uint16_t *)oam_ptr;
                else
                    return (uint16_t)oam_ptr[0] | ((uint16_t)oam_ptr[1] << 8);
            }
            else if (addr >= IO_ADDR)
            {
                // Some special registers require manual byte combine
#if ENABLE_SOUND
                if (addr >= 0xFF10 && addr <= 0xFF3F)
                {
                    uint8_t lo = audio_read(addr);
                    uint8_t hi = audio_read(addr + 1);
                    return lo | ((uint16_t)hi << 8);
                }
#endif
                return (uint16_t)gb->hram_io[addr - IO_ADDR]
                     | ((uint16_t)gb->hram_io[addr + 1 - IO_ADDR] << 8);
            }
            else
                return 0xFFFF;
    }

    (gb->gb_error)(gb, GB_INVALID_READ, addr);
    WGB_UNREACHABLE();
}
#else
uint16_t WALNUT_GB_HOT_ATTR __gb_read16(struct gb_s *gb, uint16_t addr)
{
    switch(WALNUT_GB_GET_MSN16(addr))
    {
        // --- Boot ROM / Fixed ROM 0
        case 0x0:
            if(gb->hram_io[IO_BOOT] == 0 && addr < 0x0100)
                return (uint16_t)gb->gb_bootrom_read(gb, addr)
                     | ((uint16_t)gb->gb_bootrom_read(gb, addr + 1) << 8);
#if WALNUT_FULL_GBC_SUPPORT
            else if(gb->cgb.cgbMode && gb->hram_io[IO_BOOT] == 0 && addr >= 0x0200 && addr < 0x0900)
                return (uint16_t)gb->gb_bootrom_read(gb, addr)
                     | ((uint16_t)gb->gb_bootrom_read(gb, addr + 1) << 8);
#endif
            /* fallthrough */
        case 0x1:
        case 0x2:
        case 0x3:
            if (__gb_direct_rom_contains(gb, addr, 2))
                return __gb_direct_rom_read16(gb, addr);
            return gb->gb_rom_read_16bit(gb,addr);

        // --- Switchable ROM banks (0x4000–0x7FFF)
        case 0x4:
        case 0x5:
        case 0x6:
        case 0x7:
        {
            uint32_t bank_offset = __gb_fetch_rom_bank_offset(gb);
            if (__gb_direct_rom_contains(gb, addr + bank_offset, 2))
                return __gb_direct_rom_read16(gb, addr + bank_offset);
            return gb->gb_rom_read_16bit(gb,addr + bank_offset);
        }

        // --- VRAM (0x8000–0x9FFF)
        case 0x8:
        case 0x9:
#if WALNUT_FULL_GBC_SUPPORT
            if (addr + 1 < 0xA000)
                return *(uint16_t *)&gb->vram[addr - gb->cgb.vramBankOffset];
            else
                return gb->vram[addr - gb->cgb.vramBankOffset]; // last byte fallback
#else
            if (addr + 1 < 0xA000)
                return *(uint16_t *)&gb->vram[addr - VRAM_ADDR];
            else
                return gb->vram[addr - VRAM_ADDR];
#endif

        // --- External RAM / RTC (0xA000–0xBFFF)
        case 0xA:
        case 0xB:
            if (__gb_pocket_camera_regs_selected(gb))
                return __gb_pocket_camera_read16(gb, addr);
            else if (gb->mbc == 3 && gb->cart_ram_bank >= 0x08)
            {
                // RTC bytes are separate; combine manually
                return gb->rtc_latched.bytes[gb->cart_ram_bank - 0x08]
                     | ((uint16_t)gb->rtc_latched.bytes[gb->cart_ram_bank - 0x08 + 1] << 8);
            }
            else if (gb->cart_ram &&
                     (gb->enable_cart_ram || gb->cart_is_pocket_camera))
            {
                uint16_t offset;
                if (gb->mbc == 2)
                {
                    addr &= 0x1FF;
                    offset = addr;
                }
                else if ((gb->cart_mode_select || gb->mbc != 1) && gb->cart_ram_bank < gb->num_ram_banks)
                {
                    offset = addr - CART_RAM_ADDR + (gb->cart_ram_bank * CRAM_BANK_SIZE);
                }
                else
                {
                    offset = addr - CART_RAM_ADDR;
                }
                return gb->gb_cart_ram_read(gb, offset) | ((uint16_t)gb->gb_cart_ram_read(gb, offset + 1) << 8);
            }
            return 0xFFFF;

        // --- Work RAM (0xC000–0xDFFF)
        case 0xC:
        case 0xD:
#if WALNUT_FULL_GBC_SUPPORT
            if(gb->cgb.cgbMode && addr >= WRAM_1_ADDR)
                return *(uint16_t *)&gb->wram[addr - gb->cgb.wramBankOffset];
#endif
            return *(uint16_t *)&gb->wram[addr - WRAM_0_ADDR];

        // --- Echo RAM (0xE000–0xFDFF)
        case 0xE:
            return *(uint16_t *)&gb->wram[addr - ECHO_ADDR];

        // --- OAM (0xFE00–0xFE9F)
        case 0xF:
            if (addr < 0xFEA0)
                return *(uint16_t *)&gb->oam[addr - OAM_ADDR];
            // --- HRAM / IO (0xFF00–0xFFFF)
            else if (addr >= IO_ADDR)
            {
                // Some registers are not contiguous, must combine manually
#if ENABLE_SOUND
                if (addr >= 0xFF10 && addr <= 0xFF3F)
                {
                    uint8_t lo = audio_read(addr);
                    uint8_t hi = audio_read(addr + 1);
                    return lo | ((uint16_t)hi << 8);
                }
#endif
#if WALNUT_FULL_GBC_SUPPORT
                switch (addr & 0xFF)
                {
                    case 0x4D: // Speed switch
                        return gb->cgb.doubleSpeed | ((uint16_t)gb->cgb.doubleSpeedPrep << 8);
                    case 0x4F: // VRAM bank
                        return gb->cgb.vramBank | 0xFE00;
                    case 0x51: case 0x52: case 0x53: case 0x54: case 0x55:
                        {
                            uint8_t lo = gb->hram_io[addr - IO_ADDR];
                            uint8_t hi = gb->hram_io[addr + 1 - IO_ADDR];
                            return lo | ((uint16_t)hi << 8);
                        }
                    case 0x56:
                        return gb->hram_io[0x56] | ((uint16_t)gb->hram_io[0x57] << 8);
                    case 0x68: case 0x69: case 0x6A: case 0x6B:
                        {
                            uint8_t lo = gb->hram_io[addr - IO_ADDR];
                            uint8_t hi = gb->hram_io[addr + 1 - IO_ADDR];
                            return lo | ((uint16_t)hi << 8);
                        }
                    case 0x70:
                        return gb->cgb.wramBank | 0x00; // upper byte not used
                    default:
                        return *(uint16_t *)&gb->hram_io[addr - IO_ADDR];
                }
#else
                return *(uint16_t *)&gb->hram_io[addr - IO_ADDR];
#endif
            }
            else
            {
                // Unusable memory or unknown region; return 0xFFFF
                return 0xFFFF;
            }
    }

    // Should never reach here
    (gb->gb_error)(gb, GB_INVALID_READ, addr);
    WGB_UNREACHABLE();
}
#endif

#if WALNUT_GB_32BIT_ALIGNED
uint32_t __gb_read32(struct gb_s *gb, uint16_t addr)
{
    switch (WALNUT_GB_GET_MSN16(addr))
    {
        // --- Boot ROM / Fixed ROM 0
        case 0x0:
            if (gb->hram_io[IO_BOOT] == 0 && addr < 0x0100)
                return (uint32_t)gb->gb_bootrom_read(gb, addr)
                     | ((uint32_t)gb->gb_bootrom_read(gb, addr + 1) << 8)
                     | ((uint32_t)gb->gb_bootrom_read(gb, addr + 2) << 16)
                     | ((uint32_t)gb->gb_bootrom_read(gb, addr + 3) << 24);
#if WALNUT_FULL_GBC_SUPPORT
            else if (gb->cgb.cgbMode && gb->hram_io[IO_BOOT] == 0 &&
                     addr >= 0x0200 && addr < 0x0900)
                return (uint32_t)gb->gb_bootrom_read(gb, addr)
                     | ((uint32_t)gb->gb_bootrom_read(gb, addr + 1) << 8)
                     | ((uint32_t)gb->gb_bootrom_read(gb, addr + 2) << 16)
                     | ((uint32_t)gb->gb_bootrom_read(gb, addr + 3) << 24);
#endif
            /* fallthrough */
        case 0x1:
        case 0x2:
        case 0x3:
            if (__gb_direct_rom_contains(gb, addr, 4))
                return __gb_direct_rom_read32(gb, addr);
            return gb->gb_rom_read_32bit(gb, addr);

        // --- Switchable ROM banks
        case 0x4:
        case 0x5:
        case 0x6:
        case 0x7:
        {
            uint32_t bank_offset = __gb_fetch_rom_bank_offset(gb);
            if (__gb_direct_rom_contains(gb, addr + bank_offset, 4))
                return __gb_direct_rom_read32(gb, addr + bank_offset);
            return gb->gb_rom_read_32bit(gb, addr + bank_offset);
        }

        // --- VRAM
        case 0x8:
        case 0x9:
        {
#if WALNUT_FULL_GBC_SUPPORT
            uint8_t *p = &gb->vram[addr - gb->cgb.vramBankOffset];
#else
            uint8_t *p = &gb->vram[addr - VRAM_ADDR];
#endif
            if (addr + 3 < 0xA000 && (((uintptr_t)p & 3) == 0))
                return *(uint32_t *)p;

            return (uint32_t)p[0]
                 | ((uint32_t)p[1] << 8)
                 | ((uint32_t)p[2] << 16)
                 | ((uint32_t)p[3] << 24);
        }

        // --- External RAM / RTC
        case 0xA:
        case 0xB:
            if (__gb_pocket_camera_regs_selected(gb))
                return __gb_pocket_camera_read32(gb, addr);
            else if (gb->mbc == 3 && gb->cart_ram_bank >= 0x08)
            {
                const uint8_t *p =
                    &gb->rtc_latched.bytes[gb->cart_ram_bank - 0x08];
                return (uint32_t)p[0]
                     | ((uint32_t)p[1] << 8)
                     | ((uint32_t)p[2] << 16)
                     | ((uint32_t)p[3] << 24);
            }
            else if (gb->cart_ram &&
                     (gb->enable_cart_ram || gb->cart_is_pocket_camera))
            {
                uint16_t offset;
                if (gb->mbc == 2)
                {
                    addr &= 0x1FF;
                    offset = addr;
                }
                else if ((gb->cart_mode_select || gb->mbc != 1) &&
                         gb->cart_ram_bank < gb->num_ram_banks)
                {
                    offset = addr - CART_RAM_ADDR +
                             (gb->cart_ram_bank * CRAM_BANK_SIZE);
                }
                else
                {
                    offset = addr - CART_RAM_ADDR;
                }

                return (uint32_t)gb->gb_cart_ram_read(gb, offset)
                     | ((uint32_t)gb->gb_cart_ram_read(gb, offset + 1) << 8)
                     | ((uint32_t)gb->gb_cart_ram_read(gb, offset + 2) << 16)
                     | ((uint32_t)gb->gb_cart_ram_read(gb, offset + 3) << 24);
            }
            return 0xFFFFFFFF;

        // --- WRAM
        case 0xC:
        case 0xD:
        {
#if WALNUT_FULL_GBC_SUPPORT
            if (gb->cgb.cgbMode && addr >= WRAM_1_ADDR)
            {
                uint8_t *p = &gb->wram[addr - gb->cgb.wramBankOffset];
                if (((uintptr_t)p & 3) == 0)
                    return *(uint32_t *)p;
                return (uint32_t)p[0]
                     | ((uint32_t)p[1] << 8)
                     | ((uint32_t)p[2] << 16)
                     | ((uint32_t)p[3] << 24);
            }
#endif
            uint8_t *p = &gb->wram[addr - WRAM_0_ADDR];
            if (((uintptr_t)p & 3) == 0)
                return *(uint32_t *)p;
            return (uint32_t)p[0]
                 | ((uint32_t)p[1] << 8)
                 | ((uint32_t)p[2] << 16)
                 | ((uint32_t)p[3] << 24);
        }

        // --- Echo RAM
        case 0xE:
        {
            uint8_t *p = &gb->wram[addr - ECHO_ADDR];
            if (((uintptr_t)p & 3) == 0)
                return *(uint32_t *)p;
            return (uint32_t)p[0]
                 | ((uint32_t)p[1] << 8)
                 | ((uint32_t)p[2] << 16)
                 | ((uint32_t)p[3] << 24);
        }

        // --- OAM / HRAM / IO
        case 0xF:
            if (addr < 0xFEA0)
            {
                uint8_t *p = &gb->oam[addr - OAM_ADDR];
                if (((uintptr_t)p & 3) == 0)
                    return *(uint32_t *)p;
                return (uint32_t)p[0]
                     | ((uint32_t)p[1] << 8)
                     | ((uint32_t)p[2] << 16)
                     | ((uint32_t)p[3] << 24);
            }
            else if (addr >= IO_ADDR)
            {
#if ENABLE_SOUND
                if (addr >= 0xFF10 && addr <= 0xFF3F)
                {
                    uint32_t v = 0;
                    for (int i = 0; i < 4; ++i)
                        v |= ((uint32_t)audio_read(addr + i)) << (8 * i);
                    return v;
                }
#endif
                uint8_t *p = &gb->hram_io[addr - IO_ADDR];
                if (((uintptr_t)p & 3) == 0)
                    return *(uint32_t *)p;
                return (uint32_t)p[0]
                     | ((uint32_t)p[1] << 8)
                     | ((uint32_t)p[2] << 16)
                     | ((uint32_t)p[3] << 24);
            }
            return 0xFFFFFFFF;
    }

    (gb->gb_error)(gb, GB_INVALID_READ, addr);
    WGB_UNREACHABLE();
}
#else
uint32_t __gb_read32(struct gb_s *gb, uint16_t addr)
{
    switch (WALNUT_GB_GET_MSN16(addr))
    {
        // --- Boot ROM / Fixed ROM 0
        case 0x0:
            if (gb->hram_io[IO_BOOT] == 0 && addr < 0x0100)
                return (uint32_t)gb->gb_bootrom_read(gb, addr)
                     | ((uint32_t)gb->gb_bootrom_read(gb, addr + 1) << 8)
                     | ((uint32_t)gb->gb_bootrom_read(gb, addr + 2) << 16)
                     | ((uint32_t)gb->gb_bootrom_read(gb, addr + 3) << 24);
#if WALNUT_FULL_GBC_SUPPORT
            else if (gb->cgb.cgbMode && gb->hram_io[IO_BOOT] == 0 &&
                     addr >= 0x0200 && addr < 0x0900)
                return (uint32_t)gb->gb_bootrom_read(gb, addr)
                     | ((uint32_t)gb->gb_bootrom_read(gb, addr + 1) << 8)
                     | ((uint32_t)gb->gb_bootrom_read(gb, addr + 2) << 16)
                     | ((uint32_t)gb->gb_bootrom_read(gb, addr + 3) << 24);
#endif
            /* fallthrough */
        case 0x1:
        case 0x2:
        case 0x3:
            if (__gb_direct_rom_contains(gb, addr, 4))
                return __gb_direct_rom_read32(gb, addr);
            return gb->gb_rom_read_32bit(gb,addr);

        // --- Switchable ROM banks (0x4000–0x7FFF)
        case 0x4:
        case 0x5:
        case 0x6:
        case 0x7:
        {
            uint32_t bank_offset = __gb_fetch_rom_bank_offset(gb);
            if (__gb_direct_rom_contains(gb, addr + bank_offset, 4))
                return __gb_direct_rom_read32(gb, addr + bank_offset);
            return gb->gb_rom_read_32bit(gb,addr + bank_offset);
        }

        // --- VRAM (0x8000–0x9FFF)
        case 0x8:
        case 0x9:
#if WALNUT_FULL_GBC_SUPPORT
            if (addr + 3 < 0xA000)
                return *(uint32_t *)&gb->vram[addr - gb->cgb.vramBankOffset];
            else
                return *(uint16_t *)&gb->vram[addr - gb->cgb.vramBankOffset]; // fallback
#else
            if (addr + 3 < 0xA000)
                return *(uint32_t *)&gb->vram[addr - VRAM_ADDR];
            else
                return *(uint16_t *)&gb->vram[addr - VRAM_ADDR];
#endif

        // --- External RAM / RTC (0xA000–0xBFFF)
        case 0xA:
        case 0xB:
            if (__gb_pocket_camera_regs_selected(gb))
                return __gb_pocket_camera_read32(gb, addr);
            else if (gb->mbc == 3 && gb->cart_ram_bank >= 0x08)
            {
                // RTC bytes; manual combination
                return (uint32_t)gb->rtc_latched.bytes[gb->cart_ram_bank - 0x08]
                     | ((uint32_t)gb->rtc_latched.bytes[gb->cart_ram_bank - 0x08 + 1] << 8)
                     | ((uint32_t)gb->rtc_latched.bytes[gb->cart_ram_bank - 0x08 + 2] << 16)
                     | ((uint32_t)gb->rtc_latched.bytes[gb->cart_ram_bank - 0x08 + 3] << 24);
            }
            else if (gb->cart_ram &&
                     (gb->enable_cart_ram || gb->cart_is_pocket_camera))
            {
                uint16_t offset;
                if (gb->mbc == 2)
                {
                    addr &= 0x1FF;
                    offset = addr;
                }
                else if ((gb->cart_mode_select || gb->mbc != 1) &&
                         gb->cart_ram_bank < gb->num_ram_banks)
                {
                    offset = addr - CART_RAM_ADDR +
                             (gb->cart_ram_bank * CRAM_BANK_SIZE);
                }
                else
                {
                    offset = addr - CART_RAM_ADDR;
                }
                return (uint32_t)gb->gb_cart_ram_read(gb, offset)
                     | ((uint32_t)gb->gb_cart_ram_read(gb, offset + 1) << 8)
                     | ((uint32_t)gb->gb_cart_ram_read(gb, offset + 2) << 16)
                     | ((uint32_t)gb->gb_cart_ram_read(gb, offset + 3) << 24);
            }
            return 0xFFFFFFFF;

        // --- Work RAM (0xC000–0xDFFF)
        case 0xC:
        case 0xD:
#if WALNUT_FULL_GBC_SUPPORT
            if (gb->cgb.cgbMode && addr >= WRAM_1_ADDR)
                return *(uint32_t *)&gb->wram[addr - gb->cgb.wramBankOffset];
#endif
            return *(uint32_t *)&gb->wram[addr - WRAM_0_ADDR];

        // --- Echo RAM (0xE000–0xFDFF)
        case 0xE:
            return *(uint32_t *)&gb->wram[addr - ECHO_ADDR];

        // --- OAM / HRAM / IO
        case 0xF:
            if (addr < 0xFEA0)
                return *(uint32_t *)&gb->oam[addr - OAM_ADDR];
            else if (addr >= IO_ADDR)
            {
#if ENABLE_SOUND
                if (addr >= 0xFF10 && addr <= 0xFF3F)
                {
                    uint32_t v = 0;
                    for (int i = 0; i < 4; ++i)
                        v |= ((uint32_t)audio_read(addr + i)) << (8 * i);
                    return v;
                }
#endif
#if WALNUT_FULL_GBC_SUPPORT
                switch (addr & 0xFF)
                {
                    case 0x4D:
                        return gb->cgb.doubleSpeed |
                               ((uint32_t)gb->cgb.doubleSpeedPrep << 8);
                    case 0x4F:
                        return gb->cgb.vramBank | 0xFE00;
                    case 0x51: case 0x52: case 0x53: case 0x54: case 0x55:
                    case 0x56: case 0x68: case 0x69: case 0x6A: case 0x6B:
                    case 0x70:
                        return *(uint32_t *)&gb->hram_io[addr - IO_ADDR];
                    default:
                        return *(uint32_t *)&gb->hram_io[addr - IO_ADDR];
                }
#else
                return *(uint32_t *)&gb->hram_io[addr - IO_ADDR];
#endif
            }
            return 0xFFFFFFFF;
    }

    (gb->gb_error)(gb, GB_INVALID_READ, addr);
    WGB_UNREACHABLE();
}
#endif


/**
 * Internal function used to read bytes.
 * addr is host platform endian.
 */
uint8_t WALNUT_GB_HOT_ATTR __gb_read(struct gb_s *gb, uint16_t addr)
{
	if(WGB_LIKELY(addr < ROM_N_ADDR))
	{
		if(gb->hram_io[IO_BOOT] == 0 && addr < 0x0100)
		{
			return gb->gb_bootrom_read(gb, addr);
		}
#if WALNUT_FULL_GBC_SUPPORT
		if(gb->cgb.cgbMode && gb->hram_io[IO_BOOT] == 0 &&
				addr >= 0x0200 && addr < 0x0900)
		{
			return gb->gb_bootrom_read(gb, addr);
		}
#endif
		if(__gb_direct_rom_contains(gb, addr, 1))
			return __gb_direct_rom_read8(gb, addr);
		return gb->gb_rom_read(gb, addr);
	}

	if(WGB_LIKELY(addr < VRAM_ADDR))
	{
		const uint32_t bank_addr = addr + __gb_fetch_rom_bank_offset(gb);
		if(__gb_direct_rom_contains(gb, bank_addr, 1))
			return __gb_direct_rom_read8(gb, bank_addr);
		return gb->gb_rom_read(gb, bank_addr);
	}

	if(WGB_LIKELY(addr >= WRAM_0_ADDR && addr < ECHO_ADDR))
	{
#if WALNUT_FULL_GBC_SUPPORT
		if(gb->cgb.cgbMode && addr >= WRAM_1_ADDR)
			return gb->wram[addr - gb->cgb.wramBankOffset];
#endif
		return gb->wram[addr - WRAM_0_ADDR];
	}

	if(WGB_LIKELY(addr >= ECHO_ADDR && addr < OAM_ADDR))
	{
#if WALNUT_FULL_GBC_SUPPORT
		if(addr >= 0xF000 && gb->cgb.cgbMode)
			return gb->wram[(addr - 0x2000) - gb->cgb.wramBankOffset];
#endif
		return gb->wram[addr - ECHO_ADDR];
	}

	if(WGB_LIKELY(addr >= HRAM_ADDR))
	{
		return gb->hram_io[addr - IO_ADDR];
	}

	switch(WALNUT_GB_GET_MSN16(addr))
	{
	case 0x0:
		/* IO_BOOT is only set to 1 if gb->gb_bootrom_read was not NULL
		 * on reset. */
		if(gb->hram_io[IO_BOOT] == 0 && addr < 0x0100)
		{
			return gb->gb_bootrom_read(gb, addr);
		}
#if WALNUT_FULL_GBC_SUPPORT
		else if (gb->cgb.cgbMode && gb->hram_io[IO_BOOT] == 0 && addr < 0x0900 && addr >= 0x0200)
		{
			return gb->gb_bootrom_read(gb, addr);
		}
#endif
		/* Fallthrough */
	case 0x1:
	case 0x2:
	case 0x3:
		if(__gb_direct_rom_contains(gb, addr, 1))
			return __gb_direct_rom_read8(gb, addr);
		return gb->gb_rom_read(gb, addr);

	case 0x4:
	case 0x5:
	case 0x6:
	case 0x7:
		{
			const uint32_t bank_addr = addr + __gb_fetch_rom_bank_offset(gb);
			if(__gb_direct_rom_contains(gb, bank_addr, 1))
				return __gb_direct_rom_read8(gb, bank_addr);
			return gb->gb_rom_read(gb, bank_addr);
		}

	case 0x8:
	case 0x9:
#if WALNUT_FULL_GBC_SUPPORT
		return gb->vram[addr - gb->cgb.vramBankOffset];
#else
		return gb->vram[addr - VRAM_ADDR];
#endif
	case 0xA:
	case 0xB:
		if(__gb_pocket_camera_regs_selected(gb))
		{
			return __gb_pocket_camera_read(gb, addr);
		}
		else if(gb->mbc == 3 && gb->cart_ram_bank >= 0x08)
		{
			return gb->rtc_latched.bytes[gb->cart_ram_bank - 0x08];
		}
		else if(gb->cart_ram &&
				(gb->enable_cart_ram || gb->cart_is_pocket_camera))
		{
			if(gb->mbc == 2)
			{
				/* Only 9 bits are available in address. */
				addr &= 0x1FF;
				return gb->gb_cart_ram_read(gb, addr);
			}
			else if((gb->cart_mode_select || gb->mbc != 1) &&
					gb->cart_ram_bank < gb->num_ram_banks)
			{
				return gb->gb_cart_ram_read(gb, addr - CART_RAM_ADDR +
							(gb->cart_ram_bank * CRAM_BANK_SIZE));
			}
			else
			{
				return gb->gb_cart_ram_read(gb, addr - CART_RAM_ADDR);
			}
		}

		return 0xFF;

	case 0xC:
	case 0xD:
#if WALNUT_FULL_GBC_SUPPORT
	if(gb->cgb.cgbMode && addr >= WRAM_1_ADDR)
		return gb->wram[addr - gb->cgb.wramBankOffset];
#endif
		return gb->wram[addr - WRAM_0_ADDR];

	case 0xE:
		return gb->wram[addr - ECHO_ADDR];

	case 0xF:
		if(addr < OAM_ADDR)
		{
#if WALNUT_FULL_GBC_SUPPORT
			return gb->wram[(addr - 0x2000) - gb->cgb.wramBankOffset];
#else
			return gb->wram[addr - ECHO_ADDR];
#endif
		}

		if(addr < UNUSED_ADDR)
		{
			return gb->oam[addr - OAM_ADDR];
		}

		/* Unusable memory area. Reading from this area returns 0xFF.*/
		if(addr < IO_ADDR)
		{
			return 0xFF;
		}

		/* APU registers. */
		if((addr >= 0xFF10) && (addr <= 0xFF3F))
		{
#if ENABLE_SOUND
			return audio_read(addr);
#else
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
			return gb->hram_io[addr - IO_ADDR] | ortab[addr - IO_ADDR];
#endif
		}

#if WALNUT_FULL_GBC_SUPPORT
		/* IO and Interrupts. */
		switch (addr & 0xFF)
		{
		/* Speed Switch*/
		case 0x4D:
			return (gb->cgb.doubleSpeed << 7) + gb->cgb.doubleSpeedPrep;
		/* CGB VRAM Bank*/
		case 0x4F:
			return gb->cgb.vramBank | 0xFE;
		/* CGB DMA*/
		case 0x51:
			return (gb->cgb.dmaSource >> 8);
		case 0x52:
			return (gb->cgb.dmaSource & 0xF0);
		case 0x53:
			return (gb->cgb.dmaDest >> 8);
		case 0x54:
			return (gb->cgb.dmaDest & 0xF0);
		case 0x55:
			return (gb->cgb.dmaActive << 7) | (gb->cgb.dmaSize - 1);
		/* IR Register*/
		case 0x56:
			return gb->hram_io[0x56];
		/* CGB BG Palette Index*/
		case 0x68:
			return (gb->cgb.BGPaletteID & 0x3F) + (gb->cgb.BGPaletteInc << 7);
		/* CGB BG Palette*/
		case 0x69:
			return gb->cgb.BGPalette[(gb->cgb.BGPaletteID & 0x3F)];
		/* CGB OAM Palette Index*/
		case 0x6A:
			return (gb->cgb.OAMPaletteID & 0x3F) + (gb->cgb.OAMPaletteInc << 7);
		/* CGB OAM Palette*/
		case 0x6B:
			return gb->cgb.OAMPalette[(gb->cgb.OAMPaletteID & 0x3F)];
		/* CGB WRAM Bank*/
		case 0x70:
			return gb->cgb.wramBank;
		default:
#endif
			/* HRAM */
			if(addr >= IO_ADDR)
				return gb->hram_io[addr - IO_ADDR];
#if WALNUT_FULL_GBC_SUPPORT
		}
#endif
	}


	/* Return address that caused read error. */
	(gb->gb_error)(gb, GB_INVALID_READ, addr);
	WGB_UNREACHABLE();
}

static inline uint16_t WALNUT_GB_HOT_ATTR __gb_fetch_opcode_pair(
		struct gb_s *gb, uint16_t pc)
{
	if(WGB_LIKELY(pc < ROM_N_ADDR))
	{
		if(WGB_UNLIKELY(gb->hram_io[IO_BOOT] == 0 &&
				(pc < 0x0100
#if WALNUT_FULL_GBC_SUPPORT
				 || (gb->cgb.cgbMode && pc >= 0x0200 && pc < 0x0900)
#endif
				)))
			goto slow;
		if(WGB_LIKELY(pc <= 0x3FFE &&
				__gb_direct_rom_contains(gb, pc, 2)))
		{
			return __gb_direct_rom_read16(gb, pc);
		}
		goto slow;
	}

	if(WGB_LIKELY(pc < VRAM_ADDR))
	{
		if(WGB_LIKELY(pc <= 0x7FFE))
		{
			const uint32_t bank_addr = pc + __gb_fetch_rom_bank_offset(gb);
			if(WGB_LIKELY(__gb_direct_rom_contains(gb, bank_addr, 2)))
			{
				return __gb_direct_rom_read16(gb, bank_addr);
			}
		}
		goto slow;
	}

slow:
	return (uint16_t)__gb_read(gb, pc) |
		((uint16_t)__gb_read(gb, (uint16_t)(pc + 1)) << 8);
}

#if WALNUT_GB_FAST_STAT_POLL_ACTIVE || WALNUT_GB_FAST_POLL_WAIT_ACTIVE
static inline bool __gb_code_peek8(const struct gb_s *gb, uint16_t pc, uint8_t *out)
{
	switch(WALNUT_GB_GET_MSN16(pc))
	{
	case 0x0:
		if(gb->hram_io[IO_BOOT] == 0 && pc < 0x0100)
		{
			*out = gb->gb_bootrom_read((struct gb_s *)gb, pc);
			return true;
		}
#if WALNUT_FULL_GBC_SUPPORT
		if(gb->cgb.cgbMode && gb->hram_io[IO_BOOT] == 0 &&
				pc >= 0x0200 && pc < 0x0900)
		{
			*out = gb->gb_bootrom_read((struct gb_s *)gb, pc);
			return true;
		}
#endif
		/* Fallthrough */
	case 0x1:
	case 0x2:
	case 0x3:
		if(__gb_direct_rom_contains(gb, pc, 1))
			*out = gb->direct.rom_data[pc];
		else
			*out = gb->gb_rom_read((struct gb_s *)gb, pc);
		return true;

	case 0x4:
	case 0x5:
	case 0x6:
	case 0x7:
	{
		const uint32_t bank_addr = pc + __gb_fetch_rom_bank_offset(gb);
		if(__gb_direct_rom_contains(gb, bank_addr, 1))
			*out = gb->direct.rom_data[bank_addr];
		else
			*out = gb->gb_rom_read((struct gb_s *)gb, bank_addr);
		return true;
	}

	case 0xC:
		*out = gb->wram[pc - WRAM_0_ADDR];
		return true;

	case 0xD:
#if WALNUT_FULL_GBC_SUPPORT
		if(gb->cgb.cgbMode)
			*out = gb->wram[pc - gb->cgb.wramBankOffset];
		else
#endif
			*out = gb->wram[pc - WRAM_0_ADDR];
		return true;

	case 0xE:
#if WALNUT_FULL_GBC_SUPPORT
		if(gb->cgb.cgbMode)
			*out = gb->wram[(pc - 0x2000) - gb->cgb.wramBankOffset];
		else
#endif
			*out = gb->wram[pc - ECHO_ADDR];
		return true;

	case 0xF:
		if(pc < OAM_ADDR)
		{
#if WALNUT_FULL_GBC_SUPPORT
			if(gb->cgb.cgbMode)
				*out = gb->wram[(pc - 0x2000) - gb->cgb.wramBankOffset];
			else
#endif
				*out = gb->wram[pc - ECHO_ADDR];
			return true;
		}
		if(pc >= HRAM_ADDR)
		{
			*out = gb->hram_io[pc - IO_ADDR];
			return true;
		}
		break;
	}

	return false;
}

static inline bool __gb_fast_stat_poll_branch_to(uint16_t branch_pc,
		int8_t rel, uint16_t target_pc)
{
	return (uint16_t)(branch_pc + 2 + rel) == target_pc;
}

static inline uint8_t __gb_effective_enabled_interrupts(const struct gb_s *gb)
{
	uint8_t enabled = gb->hram_io[IO_IE] & ANY_INTR;
	if(gb->direct.suppress_lcd_stat_interrupts)
		enabled &= (uint8_t)~LCDC_INTR;
	return enabled;
}

static inline uint8_t __gb_effective_pending_interrupts(const struct gb_s *gb)
{
	return gb->hram_io[IO_IF] & __gb_effective_enabled_interrupts(gb);
}

static inline void __gb_fast_poll_wait_advance_cycles(struct gb_s *gb,
		uint_fast32_t cycles, bool stop_on_irq);

static inline bool __gb_fast_stat_poll_and_reg_mask(
		const struct gb_s *gb, uint8_t opcode, uint8_t *mask)
{
	switch(opcode)
	{
	case 0xA0: /* AND B */
		*mask = gb->cpu_reg.bc.bytes.b;
		return true;
	case 0xA1: /* AND C */
		*mask = gb->cpu_reg.bc.bytes.c;
		return true;
	case 0xA2: /* AND D */
		*mask = gb->cpu_reg.de.bytes.d;
		return true;
	case 0xA3: /* AND E */
		*mask = gb->cpu_reg.de.bytes.e;
		return true;
	case 0xA4: /* AND H */
		*mask = gb->cpu_reg.hl.bytes.h;
		return true;
	case 0xA5: /* AND L */
		*mask = gb->cpu_reg.hl.bytes.l;
		return true;
	default:
		return false;
	}
}


#if WALNUT_GB_FAST_POLL_WAIT_ACTIVE
enum wgb_fast_poll_lcd_kind_e
{
	WGB_FAST_POLL_LCD_NONE = 0,
	WGB_FAST_POLL_LCD_LY_CP,
	WGB_FAST_POLL_LCD_STAT_AND,
	WGB_FAST_POLL_LCD_STAT_AND_CP,
	WGB_FAST_POLL_LCD_STAT_BIT_A,
	WGB_FAST_POLL_LCD_STAT_BIT_HL
};

struct wgb_fast_poll_lcd_match_s
{
	uint8_t kind;
	uint8_t branch_opcode;
	uint8_t read_cycles;
	uint8_t final_test_cycles;
	uint8_t cmp;
	uint8_t mask;
	uint8_t bit;
};

static inline bool __gb_fast_poll_wait_branch_condition(
		bool z, bool c, uint8_t branch_opcode)
{
	switch(branch_opcode)
	{
	case 0x20:
		return !z;
	case 0x28:
		return z;
	case 0x30:
		return !c;
	case 0x38:
		return c;
	default:
		return false;
	}
}

static inline bool __gb_fast_poll_wait_zero_branch(uint8_t branch_opcode)
{
	return branch_opcode == 0x20 || branch_opcode == 0x28;
}

static inline bool __gb_fast_poll_wait_io_read_form(
		const struct gb_s *gb, uint16_t pc, uint8_t io,
		uint16_t *next_pc, uint8_t *cycles)
{
	uint8_t b0, b1, b2;

	if(!__gb_code_peek8(gb, pc, &b0))
		return false;
	if(b0 == 0xF0)
	{
		if(!__gb_code_peek8(gb, (uint16_t)(pc + 1), &b1) || b1 != io)
			return false;
		*next_pc = (uint16_t)(pc + 2);
		*cycles = 12;
		return true;
	}
	if(b0 == 0xFA)
	{
		if(!__gb_code_peek8(gb, (uint16_t)(pc + 1), &b1) ||
				!__gb_code_peek8(gb, (uint16_t)(pc + 2), &b2) ||
				b1 != io || b2 != 0xFF)
			return false;
		*next_pc = (uint16_t)(pc + 3);
		*cycles = 16;
		return true;
	}
	if(b0 == 0xF2 && gb->cpu_reg.bc.bytes.c == io)
	{
		*next_pc = (uint16_t)(pc + 1);
		*cycles = 8;
		return true;
	}
	return false;
}

static inline bool __gb_fast_poll_wait_match_io_cp_jr(
		const struct gb_s *gb, uint16_t target_pc, uint16_t branch_pc,
		uint8_t branch_opcode, uint8_t io,
		struct wgb_fast_poll_lcd_match_s *match)
{
	uint16_t pc_after_read;
	uint8_t read_cycles, op, cmp, jr, rel;

	if(!__gb_fast_poll_wait_io_read_form(gb, target_pc, io,
			&pc_after_read, &read_cycles))
		return false;
	if(branch_pc != (uint16_t)(pc_after_read + 2))
		return false;
	if(!__gb_code_peek8(gb, pc_after_read, &op) || op != 0xFE ||
			!__gb_code_peek8(gb, (uint16_t)(pc_after_read + 1), &cmp) ||
			!__gb_code_peek8(gb, (uint16_t)(pc_after_read + 2), &jr) ||
			!__gb_code_peek8(gb, (uint16_t)(pc_after_read + 3), &rel))
		return false;
	if(jr != branch_opcode ||
			!__gb_fast_stat_poll_branch_to(branch_pc, (int8_t)rel, target_pc))
		return false;

	match->branch_opcode = branch_opcode;
	match->read_cycles = read_cycles;
	match->final_test_cycles = (uint8_t)(read_cycles + 8);
	match->cmp = cmp;
	match->mask = 0;
	match->bit = 0;
	return true;
}

static inline bool __gb_fast_poll_wait_match_io_and_jr(
		const struct gb_s *gb, uint16_t target_pc, uint16_t branch_pc,
		uint8_t branch_opcode, uint8_t io,
		struct wgb_fast_poll_lcd_match_s *match)
{
	uint16_t pc_after_read;
	uint8_t read_cycles, op, mask, jr, rel;

	if(!__gb_fast_poll_wait_zero_branch(branch_opcode))
		return false;
	if(!__gb_fast_poll_wait_io_read_form(gb, target_pc, io,
			&pc_after_read, &read_cycles))
		return false;
	if(branch_pc != (uint16_t)(pc_after_read + 2))
		return false;
	if(!__gb_code_peek8(gb, pc_after_read, &op) || op != 0xE6 ||
			!__gb_code_peek8(gb, (uint16_t)(pc_after_read + 1), &mask) ||
			!__gb_code_peek8(gb, (uint16_t)(pc_after_read + 2), &jr) ||
			!__gb_code_peek8(gb, (uint16_t)(pc_after_read + 3), &rel))
		return false;
	if(jr != branch_opcode ||
			!__gb_fast_stat_poll_branch_to(branch_pc, (int8_t)rel, target_pc))
		return false;

	match->branch_opcode = branch_opcode;
	match->read_cycles = read_cycles;
	match->final_test_cycles = (uint8_t)(read_cycles + 8);
	match->cmp = 0;
	match->mask = mask;
	match->bit = 0;
	return true;
}

static inline bool __gb_fast_poll_wait_match_io_and_cp_jr(
		const struct gb_s *gb, uint16_t target_pc, uint16_t branch_pc,
		uint8_t branch_opcode, uint8_t io,
		struct wgb_fast_poll_lcd_match_s *match)
{
	uint16_t pc_after_read;
	uint8_t read_cycles, op0, mask, op1, cmp, jr, rel;

	if(!__gb_fast_poll_wait_io_read_form(gb, target_pc, io,
			&pc_after_read, &read_cycles))
		return false;
	if(branch_pc != (uint16_t)(pc_after_read + 4))
		return false;
	if(!__gb_code_peek8(gb, pc_after_read, &op0) || op0 != 0xE6 ||
			!__gb_code_peek8(gb, (uint16_t)(pc_after_read + 1), &mask) ||
			!__gb_code_peek8(gb, (uint16_t)(pc_after_read + 2), &op1) ||
			op1 != 0xFE ||
			!__gb_code_peek8(gb, (uint16_t)(pc_after_read + 3), &cmp) ||
			!__gb_code_peek8(gb, (uint16_t)(pc_after_read + 4), &jr) ||
			!__gb_code_peek8(gb, (uint16_t)(pc_after_read + 5), &rel))
		return false;
	if(jr != branch_opcode ||
			!__gb_fast_stat_poll_branch_to(branch_pc, (int8_t)rel, target_pc))
		return false;

	match->branch_opcode = branch_opcode;
	match->read_cycles = read_cycles;
	match->final_test_cycles = (uint8_t)(read_cycles + 16);
	match->cmp = cmp;
	match->mask = mask;
	match->bit = 0;
	return true;
}

static inline bool __gb_fast_poll_wait_match_io_bit_a_jr(
		const struct gb_s *gb, uint16_t target_pc, uint16_t branch_pc,
		uint8_t branch_opcode, uint8_t io,
		struct wgb_fast_poll_lcd_match_s *match)
{
	uint16_t pc_after_read;
	uint8_t read_cycles, cb, bitop, jr, rel;

	if(!__gb_fast_poll_wait_zero_branch(branch_opcode))
		return false;
	if(!__gb_fast_poll_wait_io_read_form(gb, target_pc, io,
			&pc_after_read, &read_cycles))
		return false;
	if(branch_pc != (uint16_t)(pc_after_read + 2))
		return false;
	if(!__gb_code_peek8(gb, pc_after_read, &cb) || cb != 0xCB ||
			!__gb_code_peek8(gb, (uint16_t)(pc_after_read + 1), &bitop) ||
			!__gb_code_peek8(gb, (uint16_t)(pc_after_read + 2), &jr) ||
			!__gb_code_peek8(gb, (uint16_t)(pc_after_read + 3), &rel))
		return false;
	if((bitop & 0xC7) != 0x47 || jr != branch_opcode ||
			!__gb_fast_stat_poll_branch_to(branch_pc, (int8_t)rel, target_pc))
		return false;

	match->branch_opcode = branch_opcode;
	match->read_cycles = read_cycles;
	match->final_test_cycles = (uint8_t)(read_cycles + 8);
	match->cmp = 0;
	match->mask = 0;
	match->bit = (uint8_t)((bitop >> 3) & 0x07);
	return true;
}

static inline bool __gb_fast_poll_wait_match_stat_bit_hl_jr(
		const struct gb_s *gb, uint16_t target_pc, uint16_t branch_pc,
		uint8_t branch_opcode, struct wgb_fast_poll_lcd_match_s *match)
{
	uint8_t cb, bitop, jr, rel;

	if(!__gb_fast_poll_wait_zero_branch(branch_opcode))
		return false;
	if(gb->cpu_reg.hl.reg != (uint16_t)(IO_ADDR + IO_STAT))
		return false;
	if(branch_pc != (uint16_t)(target_pc + 2))
		return false;
	if(!__gb_code_peek8(gb, target_pc, &cb) || cb != 0xCB ||
			!__gb_code_peek8(gb, (uint16_t)(target_pc + 1), &bitop) ||
			!__gb_code_peek8(gb, (uint16_t)(target_pc + 2), &jr) ||
			!__gb_code_peek8(gb, (uint16_t)(target_pc + 3), &rel))
		return false;
	if((bitop & 0xC7) != 0x46 || jr != branch_opcode ||
			!__gb_fast_stat_poll_branch_to(branch_pc, (int8_t)rel, target_pc))
		return false;

	match->branch_opcode = branch_opcode;
	match->read_cycles = 0;
	match->final_test_cycles = 12;
	match->cmp = 0;
	match->mask = 0;
	match->bit = (uint8_t)((bitop >> 3) & 0x07);
	return true;
}

static inline bool __gb_fast_poll_wait_match_lcd(
		const struct gb_s *gb, uint16_t target_pc, uint16_t branch_pc,
		uint8_t branch_opcode, struct wgb_fast_poll_lcd_match_s *match)
{
	match->kind = WGB_FAST_POLL_LCD_NONE;
	if(__gb_fast_poll_wait_match_io_cp_jr(gb, target_pc, branch_pc,
			branch_opcode, IO_LY, match))
	{
		if(match->cmp >= LCD_VERT_LINES)
			return false;
		match->kind = WGB_FAST_POLL_LCD_LY_CP;
		return true;
	}
	if(__gb_fast_poll_wait_match_io_and_cp_jr(gb, target_pc, branch_pc,
			branch_opcode, IO_STAT, match))
	{
		if(match->mask == 0 || (match->mask & (uint8_t)~STAT_MODE) != 0 ||
				(match->cmp & (uint8_t)~match->mask) != 0)
			return false;
		match->kind = WGB_FAST_POLL_LCD_STAT_AND_CP;
		return true;
	}
	if(__gb_fast_poll_wait_match_io_and_jr(gb, target_pc, branch_pc,
			branch_opcode, IO_STAT, match))
	{
		if(match->mask == 0 || (match->mask & (uint8_t)~STAT_MODE) != 0)
			return false;
		match->kind = WGB_FAST_POLL_LCD_STAT_AND;
		return true;
	}
	if(__gb_fast_poll_wait_match_io_bit_a_jr(gb, target_pc, branch_pc,
			branch_opcode, IO_STAT, match))
	{
		if(match->bit > 1)
			return false;
		match->kind = WGB_FAST_POLL_LCD_STAT_BIT_A;
		return true;
	}
	if(__gb_fast_poll_wait_match_stat_bit_hl_jr(gb, target_pc, branch_pc,
			branch_opcode, match))
	{
		if(match->bit > 1)
			return false;
		match->kind = WGB_FAST_POLL_LCD_STAT_BIT_HL;
		return true;
	}
	return false;
}

static inline bool __gb_fast_poll_wait_cp_condition(
		uint8_t a, uint8_t cmp, uint8_t branch_opcode)
{
	const bool z = a == cmp;
	const bool c = a < cmp;
	return __gb_fast_poll_wait_branch_condition(z, c, branch_opcode);
}

static inline bool __gb_fast_poll_wait_and_condition(
		uint8_t a, uint8_t mask, uint8_t branch_opcode)
{
	const bool z = (a & mask) == 0;
	return __gb_fast_poll_wait_branch_condition(z, false, branch_opcode);
}

static inline bool __gb_fast_poll_wait_and_cp_condition(
		uint8_t a, uint8_t mask, uint8_t cmp, uint8_t branch_opcode)
{
	const uint8_t result = a & mask;
	const bool z = result == cmp;
	const bool c = result < cmp;
	return __gb_fast_poll_wait_branch_condition(z, c, branch_opcode);
}

static inline bool __gb_fast_poll_wait_bit_condition(
		uint8_t a, uint8_t bit, uint8_t branch_opcode)
{
	const bool z = (a & (uint8_t)(1u << bit)) == 0;
	return __gb_fast_poll_wait_branch_condition(z, false, branch_opcode);
}

static inline uint_fast32_t __gb_fast_poll_wait_lcd_to_cpu_cycles(
		const struct gb_s *gb, uint_fast32_t lcd_cycles)
{
#if WALNUT_FULL_GBC_SUPPORT
	return gb->cgb.doubleSpeed ? (lcd_cycles << 1) : lcd_cycles;
#else
	(void)gb;
	return lcd_cycles;
#endif
}

static inline void __gb_fast_poll_wait_min_cycles(
		uint_fast32_t *limit, uint_fast32_t cycles)
{
	if(cycles < *limit)
		*limit = cycles;
}

static inline bool __gb_fast_poll_wait_can_skip_wait(
		const struct gb_s *gb, uint_fast32_t wait_cycles)
{
	if(wait_cycles == 0 || !gb->gb_ime)
		return true;

	const uint8_t enabled = __gb_effective_enabled_interrupts(gb);
	if(enabled == 0)
		return true;
	if(gb->hram_io[IO_IF] & enabled)
		return false;
	if(enabled & CONTROL_INTR)
		return false;

	uint_fast32_t irq_cycles = (uint_fast32_t)~(uint_fast32_t)0;

	if((enabled & VBLANK_INTR) && (gb->hram_io[IO_LCDC] & LCDC_ENABLE))
	{
		const uint8_t ly = gb->hram_io[IO_LY];
		const uint_fast16_t lcd_count = gb->counter.lcd_count;
		uint_fast32_t lines;
		if(ly < LCD_HEIGHT)
			lines = LCD_HEIGHT - ly;
		else
			lines = (LCD_VERT_LINES - ly) + LCD_HEIGHT;
		const uint_fast32_t lcd_cycles =
			(lines * LCD_LINE_CYCLES) - lcd_count;
		__gb_fast_poll_wait_min_cycles(&irq_cycles,
			__gb_fast_poll_wait_lcd_to_cpu_cycles(gb, lcd_cycles));
	}

	if((enabled & LCDC_INTR) && (gb->hram_io[IO_LCDC] & LCDC_ENABLE) &&
			(gb->hram_io[IO_STAT] &
			 (STAT_LYC_INTR | STAT_MODE_2_INTR |
			  STAT_MODE_1_INTR | STAT_MODE_0_INTR)))
		return false;

	if((enabled & TIMER_INTR) &&
			(gb->hram_io[IO_TAC] & IO_TAC_ENABLE_MASK))
	{
		const uint_fast16_t tac_cycles =
			WALNUT_GB_TAC_CYCLES[gb->hram_io[IO_TAC] & IO_TAC_RATE_MASK];
		const uint_fast16_t tick_cycles =
			(gb->counter.tima_count < tac_cycles) ?
			(tac_cycles - gb->counter.tima_count) : 1;
		const uint_fast16_t increments =
			(uint_fast16_t)(0x100u - gb->hram_io[IO_TIMA]);
		const uint_fast32_t timer_cycles =
			tick_cycles + ((uint_fast32_t)(increments - 1u) * tac_cycles);
		__gb_fast_poll_wait_min_cycles(&irq_cycles, timer_cycles);
	}

	if((enabled & SERIAL_INTR) &&
			(gb->hram_io[IO_SC] & SERIAL_SC_TX_START))
	{
		uint_fast16_t serial_cycles = SERIAL_CYCLES_1KB;
#if WALNUT_FULL_GBC_SUPPORT
		if(gb->hram_io[IO_SC] & 0x3)
			serial_cycles = SERIAL_CYCLES_32KB;
#endif
		const uint_fast16_t serial_left =
			(gb->counter.serial_count < serial_cycles) ?
			(serial_cycles - gb->counter.serial_count) : 1;
		__gb_fast_poll_wait_min_cycles(&irq_cycles, serial_left);
	}

	return wait_cycles < irq_cycles;
}

static inline bool __gb_fast_poll_wait_ly_cycles(
		const struct gb_s *gb, const struct wgb_fast_poll_lcd_match_s *match,
		uint_fast32_t *cycles)
{
	if(!(gb->hram_io[IO_LCDC] & LCDC_ENABLE))
		return false;

	uint8_t ly = gb->hram_io[IO_LY];
	uint_fast16_t lcd_count = gb->counter.lcd_count;
	if(!__gb_fast_poll_wait_cp_condition(ly, match->cmp,
			match->branch_opcode))
	{
		*cycles = 0;
		return true;
	}

	uint_fast32_t lcd_cycles = 0;
	for(uint_fast8_t i = 0; i <= LCD_VERT_LINES; i++)
	{
		uint_fast16_t delta = LCD_LINE_CYCLES - lcd_count;
		if(delta == 0)
			delta = LCD_LINE_CYCLES;
		lcd_cycles += delta;
		lcd_count = 0;
		ly++;
		if(ly == LCD_VERT_LINES)
			ly = 0;
		if(!__gb_fast_poll_wait_cp_condition(ly, match->cmp,
				match->branch_opcode))
		{
			*cycles = __gb_fast_poll_wait_lcd_to_cpu_cycles(gb, lcd_cycles);
			return true;
		}
	}
	return false;
}

static inline uint8_t __gb_fast_poll_wait_stat_after_lcd_event(
		uint8_t stat_mode, uint8_t *ly, uint_fast16_t *lcd_count,
		uint_fast16_t delta)
{
	*lcd_count = (uint_fast16_t)(*lcd_count + delta);
	if(*lcd_count >= LCD_LINE_CYCLES)
	{
		*lcd_count = (uint_fast16_t)(*lcd_count - LCD_LINE_CYCLES);
		(*ly)++;
		if(*ly == LCD_VERT_LINES)
			*ly = 0;
		if(*ly == LCD_HEIGHT)
			return IO_STAT_MODE_VBLANK;
		if(*ly < LCD_HEIGHT)
		{
			*lcd_count = 0;
			return IO_STAT_MODE_OAM_SCAN;
		}
		return IO_STAT_MODE_VBLANK;
	}
	if(stat_mode == IO_STAT_MODE_LCD_DRAW &&
			*lcd_count >= LCD_MODE3_LCD_DRAW_END)
		return IO_STAT_MODE_HBLANK;
	if(stat_mode == IO_STAT_MODE_OAM_SCAN &&
			*lcd_count >= LCD_MODE2_OAM_SCAN_END)
		return IO_STAT_MODE_LCD_DRAW;
	return stat_mode;
}

static inline uint_fast16_t __gb_fast_poll_wait_stat_event_delta(
		uint8_t stat_mode, uint_fast16_t lcd_count)
{
	uint_fast16_t next_event = LCD_LINE_CYCLES;
	if(stat_mode == IO_STAT_MODE_OAM_SCAN)
		next_event = LCD_MODE2_OAM_SCAN_END;
	else if(stat_mode == IO_STAT_MODE_LCD_DRAW)
		next_event = LCD_MODE3_LCD_DRAW_END;
	if(lcd_count < next_event)
		return next_event - lcd_count;
	return 1;
}

static inline bool __gb_fast_poll_wait_stat_condition(
		const struct wgb_fast_poll_lcd_match_s *match, uint8_t stat)
{
	switch(match->kind)
	{
	case WGB_FAST_POLL_LCD_STAT_AND:
		return __gb_fast_poll_wait_and_condition(stat, match->mask,
			match->branch_opcode);
	case WGB_FAST_POLL_LCD_STAT_AND_CP:
		return __gb_fast_poll_wait_and_cp_condition(stat, match->mask,
			match->cmp, match->branch_opcode);
	case WGB_FAST_POLL_LCD_STAT_BIT_A:
	case WGB_FAST_POLL_LCD_STAT_BIT_HL:
		return __gb_fast_poll_wait_bit_condition(stat, match->bit,
			match->branch_opcode);
	default:
		return false;
	}
}

static inline bool __gb_fast_poll_wait_stat_cycles(
		const struct gb_s *gb, const struct wgb_fast_poll_lcd_match_s *match,
		uint_fast32_t *cycles)
{
	if(!(gb->hram_io[IO_LCDC] & LCDC_ENABLE))
		return false;

	const uint8_t stat_base = gb->hram_io[IO_STAT] & (uint8_t)~STAT_MODE;
	uint8_t stat_mode = gb->hram_io[IO_STAT] & STAT_MODE;
	uint8_t ly = gb->hram_io[IO_LY];
	uint_fast16_t lcd_count = gb->counter.lcd_count;
	uint8_t stat = stat_base | stat_mode;
	if(!__gb_fast_poll_wait_stat_condition(match, stat))
	{
		*cycles = 0;
		return true;
	}

	uint_fast32_t lcd_cycles = 0;
	for(uint_fast16_t i = 0; i < (LCD_VERT_LINES * 3u + 8u); i++)
	{
		const uint_fast16_t delta =
			__gb_fast_poll_wait_stat_event_delta(stat_mode, lcd_count);
		lcd_cycles += delta;
		stat_mode = __gb_fast_poll_wait_stat_after_lcd_event(stat_mode,
			&ly, &lcd_count, delta);
		stat = stat_base | stat_mode;
		if(!__gb_fast_poll_wait_stat_condition(match, stat))
		{
			*cycles = __gb_fast_poll_wait_lcd_to_cpu_cycles(gb, lcd_cycles);
			return true;
		}
	}
	return false;
}

static inline void __gb_fast_poll_wait_set_cp_flags(
		struct gb_s *gb, uint8_t a, uint8_t cmp)
{
	gb->cpu_reg.a = a;
	gb->cpu_reg.f.reg = 0;
	gb->cpu_reg.f.f_bits.z = a == cmp;
	gb->cpu_reg.f.f_bits.n = 1;
	gb->cpu_reg.f.f_bits.h = (a & 0x0F) < (cmp & 0x0F);
	gb->cpu_reg.f.f_bits.c = a < cmp;
}

static inline void __gb_fast_poll_wait_set_and_flags(
		struct gb_s *gb, uint8_t result)
{
	gb->cpu_reg.a = result;
	gb->cpu_reg.f.reg = 0;
	gb->cpu_reg.f.f_bits.z = result == 0;
	gb->cpu_reg.f.f_bits.h = 1;
}

static inline void __gb_fast_poll_wait_set_bit_flags(
		struct gb_s *gb, uint8_t value, uint8_t bit, bool write_a)
{
	const uint8_t old_c = gb->cpu_reg.f.f_bits.c;
	if(write_a)
		gb->cpu_reg.a = value;
	gb->cpu_reg.f.reg = 0;
	gb->cpu_reg.f.f_bits.z = (value & (uint8_t)(1u << bit)) == 0;
	gb->cpu_reg.f.f_bits.h = 1;
	gb->cpu_reg.f.f_bits.c = old_c;
}

static inline bool __gb_fast_lcd_poll_wait_apply(struct gb_s *gb,
		uint16_t target_pc, uint16_t branch_pc, uint8_t branch_opcode)
{
	struct wgb_fast_poll_lcd_match_s match;
	uint_fast32_t wait_cycles = 0;

	if(!__gb_fast_poll_wait_match_lcd(gb, target_pc, branch_pc,
			branch_opcode, &match))
		return false;

	if(match.kind == WGB_FAST_POLL_LCD_LY_CP)
	{
		if(!__gb_fast_poll_wait_ly_cycles(gb, &match, &wait_cycles))
			return true;
	}
	else
	{
		if(!__gb_fast_poll_wait_stat_cycles(gb, &match, &wait_cycles))
			return true;
	}

	if(!__gb_fast_poll_wait_can_skip_wait(gb, wait_cycles))
		return true;

	if(wait_cycles != 0)
		__gb_fast_poll_wait_advance_cycles(gb, wait_cycles, false);

	if(match.kind == WGB_FAST_POLL_LCD_LY_CP)
	{
		const uint8_t ly = gb->hram_io[IO_LY];
		if(__gb_fast_poll_wait_cp_condition(ly, match.cmp,
				match.branch_opcode))
			return true;
		__gb_fast_poll_wait_set_cp_flags(gb, ly, match.cmp);
	}
	else
	{
		const uint8_t stat = gb->hram_io[IO_STAT];
		if(__gb_fast_poll_wait_stat_condition(&match, stat))
			return true;
		switch(match.kind)
		{
		case WGB_FAST_POLL_LCD_STAT_AND:
		{
			const uint8_t result = stat & match.mask;
			__gb_fast_poll_wait_set_and_flags(gb, result);
			break;
		}
		case WGB_FAST_POLL_LCD_STAT_AND_CP:
		{
			const uint8_t result = stat & match.mask;
			__gb_fast_poll_wait_set_cp_flags(gb, result, match.cmp);
			break;
		}
		case WGB_FAST_POLL_LCD_STAT_BIT_A:
			__gb_fast_poll_wait_set_bit_flags(gb, stat, match.bit, true);
			break;
		case WGB_FAST_POLL_LCD_STAT_BIT_HL:
			__gb_fast_poll_wait_set_bit_flags(gb, stat, match.bit, false);
			break;
		default:
			return true;
		}
	}

	const uint_fast32_t tail_cycles =
		(match.final_test_cycles + 8u > 12u) ?
		(match.final_test_cycles + 8u - 12u) : 0u;
	if(tail_cycles != 0)
		__gb_fast_poll_wait_advance_cycles(gb, tail_cycles, false);
	gb->cpu_reg.pc.reg = (uint16_t)(branch_pc + 2u);
	return true;
}

static inline bool __gb_fast_ly_poll_wait_apply(struct gb_s *gb,
		uint16_t target_pc, uint16_t branch_pc, uint8_t branch_opcode)
{
	return __gb_fast_lcd_poll_wait_apply(gb, target_pc, branch_pc,
		branch_opcode);
}

static inline bool __gb_fast_poll_wait_lcd_pattern(const struct gb_s *gb,
		uint16_t target_pc, uint16_t branch_pc, uint8_t branch_opcode)
{
	struct wgb_fast_poll_lcd_match_s match;
	return __gb_fast_poll_wait_match_lcd(gb, target_pc, branch_pc,
		branch_opcode, &match);
}

static inline bool __gb_fast_poll_wait_hram_counter_addr(uint16_t addr)
{
	return addr >= HRAM_ADDR && addr < INTR_EN_ADDR;
}

static inline bool __gb_fast_poll_wait_frame_counter_body(
		const struct gb_s *gb, uint16_t target_pc, uint16_t tail_pc)
{
	bool saw_call = false;
	bool saw_hram_read = false;
	bool saw_bit_test = false;

	for(uint16_t pc = target_pc; pc < tail_pc; pc++)
	{
		uint8_t op;
		if(!__gb_code_peek8(gb, pc, &op))
			return false;
		if(op == 0xCD)
			saw_call = true;
		else if(op == 0xF0)
			saw_hram_read = true;
		else if(op == 0xCB)
			saw_bit_test = true;
	}

	return saw_call && saw_hram_read && saw_bit_test;
}

static inline bool __gb_fast_poll_wait_can_sleep_on_frame_counter(
		const struct gb_s *gb, uint16_t counter_addr)
{
	const uint8_t enabled = __gb_effective_enabled_interrupts(gb);

	if(!gb->gb_ime || !(enabled & VBLANK_INTR) ||
			(gb->hram_io[IO_IF] & enabled))
		return false;
	if(!(gb->hram_io[IO_LCDC] & LCDC_ENABLE))
		return false;
	if((enabled & CONTROL_INTR) || gb->direct.joypad != 0xFF)
		return false;
	if(gb->hram_io[IO_SC] & SERIAL_SC_TX_START)
		return false;
	if((enabled & LCDC_INTR) && (gb->hram_io[IO_STAT] &
			(STAT_LYC_INTR | STAT_MODE_2_INTR |
			 STAT_MODE_1_INTR | STAT_MODE_0_INTR)))
		return false;
	if(!__gb_fast_poll_wait_hram_counter_addr(counter_addr))
		return false;
	return gb->hram_io[counter_addr - IO_ADDR] != 0;
}

static inline bool __gb_fast_poll_wait_frame_counter_pattern(
		const struct gb_s *gb, uint16_t target_pc, uint16_t branch_pc,
		uint8_t branch_opcode)
{
	if(branch_opcode != 0x20 || target_pc >= branch_pc)
		return false;

	const uint16_t back_delta = (uint16_t)(branch_pc - target_pc);
	if(back_delta < 7 || back_delta > 48)
		return false;

	uint8_t jr, rel;
	if(!__gb_code_peek8(gb, branch_pc, &jr) || jr != branch_opcode ||
			!__gb_code_peek8(gb, (uint16_t)(branch_pc + 1), &rel) ||
			!__gb_fast_stat_poll_branch_to(branch_pc, (int8_t)rel, target_pc))
		return false;

	uint8_t op0, op1, op2;
	if(branch_pc >= 3 &&
			__gb_code_peek8(gb, (uint16_t)(branch_pc - 3), &op0) &&
			__gb_code_peek8(gb, (uint16_t)(branch_pc - 2), &op1) &&
			__gb_code_peek8(gb, (uint16_t)(branch_pc - 1), &op2) &&
			op0 == 0xF0 && (op2 == 0xA7 || op2 == 0xB7))
	{
		const uint16_t counter_addr = (uint16_t)(IO_ADDR + op1);
		if(!__gb_fast_poll_wait_can_sleep_on_frame_counter(gb, counter_addr))
			return false;
		return __gb_fast_poll_wait_frame_counter_body(gb, target_pc,
			(uint16_t)(branch_pc - 3));
	}

	if(branch_pc >= 4 &&
			__gb_code_peek8(gb, (uint16_t)(branch_pc - 4), &op0) &&
			__gb_code_peek8(gb, (uint16_t)(branch_pc - 3), &op1) &&
			__gb_code_peek8(gb, (uint16_t)(branch_pc - 2), &op2))
	{
		uint8_t test;
		if(op0 != 0xFA || op2 != 0xFF ||
				!__gb_code_peek8(gb, (uint16_t)(branch_pc - 1), &test) ||
				(test != 0xA7 && test != 0xB7))
			return false;
		const uint16_t counter_addr = (uint16_t)op1 | ((uint16_t)op2 << 8);
		if(!__gb_fast_poll_wait_can_sleep_on_frame_counter(gb, counter_addr))
			return false;
		return __gb_fast_poll_wait_frame_counter_body(gb, target_pc,
			(uint16_t)(branch_pc - 4));
	}

	return false;
}

static inline bool __gb_fast_poll_wait_carry_ly_prefilter(
		const struct gb_s *gb, uint16_t target_pc, uint16_t branch_pc)
{
	uint8_t b0, b1, b2;

	if(target_pc >= branch_pc || (uint16_t)(branch_pc - target_pc) != 4)
		return false;
	if(!gb->gb_ime || !__gb_effective_enabled_interrupts(gb) ||
			__gb_effective_pending_interrupts(gb))
		return false;
	if(!(gb->hram_io[IO_LCDC] & LCDC_ENABLE))
		return false;
	if(!__gb_code_peek8(gb, target_pc, &b0) || b0 != 0xF0)
		return false;
	if(!__gb_code_peek8(gb, (uint16_t)(target_pc + 1), &b1) ||
			b1 != IO_LY)
		return false;
	if(!__gb_code_peek8(gb, (uint16_t)(target_pc + 2), &b2) ||
			b2 != 0xFE)
		return false;
	return true;
}

static inline bool __gb_fast_poll_wait_legacy_pattern(const struct gb_s *gb,
		uint16_t target_pc, uint16_t branch_pc, uint8_t branch_opcode,
		uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
		uint8_t b5)
{
	const bool zero_branch = branch_opcode == 0x20 || branch_opcode == 0x28;
	if(!zero_branch)
		return false;

	const uint16_t polled_addr = (uint16_t)b1 | ((uint16_t)b2 << 8);
	if(b0 == 0xFA && (b3 == 0xB7 || b3 == 0xA7) &&
			b4 == branch_opcode &&
			__gb_fast_stat_poll_branch_to(branch_pc, (int8_t)b5, target_pc))
	{
		if(polled_addr < WRAM_0_ADDR)
			return false;
		return true;
	}
	return false;
}

static inline uint8_t __gb_fast_poll_wait_reason(const struct gb_s *gb,
		uint16_t target_pc, uint16_t branch_pc, uint8_t branch_opcode)
{
	uint8_t b0, b1, b2, b3, b4, b5;

	const bool zero_branch = branch_opcode == 0x20 || branch_opcode == 0x28;
	const bool carry_branch = branch_opcode == 0x30 || branch_opcode == 0x38;
	if(!zero_branch && !carry_branch)
		return WGB_POLLWAIT_REJ_UNSUPPORTED;
	if(target_pc >= branch_pc)
		return WGB_POLLWAIT_REJ_NOT_BACKWARD;
	if(target_pc >= VRAM_ADDR)
		return WGB_POLLWAIT_REJ_NOT_ROM;
	const uint16_t back_delta = (uint16_t)(branch_pc - target_pc);
	if(back_delta != 4)
	{
		if(__gb_fast_poll_wait_frame_counter_pattern(gb, target_pc,
				branch_pc, branch_opcode))
			return WGB_POLLWAIT_ACCEPT;
		return WGB_POLLWAIT_REJ_UNSUPPORTED;
	}

	if(__gb_fast_poll_wait_lcd_pattern(gb, target_pc, branch_pc,
			branch_opcode))
		return WGB_POLLWAIT_ACCEPT;

	if(!gb->gb_ime)
		return WGB_POLLWAIT_REJ_IME_OFF;
	if(!__gb_effective_enabled_interrupts(gb))
		return WGB_POLLWAIT_REJ_NO_IE;
	if(__gb_effective_pending_interrupts(gb))
		return WGB_POLLWAIT_REJ_IRQ_PENDING;
	if(!__gb_code_peek8(gb, target_pc, &b0) ||
			!__gb_code_peek8(gb, (uint16_t)(target_pc + 1), &b1) ||
			!__gb_code_peek8(gb, (uint16_t)(target_pc + 2), &b2) ||
			!__gb_code_peek8(gb, (uint16_t)(target_pc + 3), &b3) ||
			!__gb_code_peek8(gb, (uint16_t)(target_pc + 4), &b4) ||
			!__gb_code_peek8(gb, (uint16_t)(target_pc + 5), &b5))
		return WGB_POLLWAIT_REJ_PEEK;


	if(back_delta != 4)
		return WGB_POLLWAIT_REJ_UNSUPPORTED;

	if(carry_branch && b0 == 0xF0 && b1 == IO_LY && b2 == 0xFE &&
			b4 == branch_opcode &&
			__gb_fast_stat_poll_branch_to(branch_pc, (int8_t)b5, target_pc))
	{
		if(b3 >= LCD_VERT_LINES)
			return WGB_POLLWAIT_REJ_UNSUPPORTED;
		return WGB_POLLWAIT_ACCEPT;
	}

	const uint16_t polled_addr = (uint16_t)b1 | ((uint16_t)b2 << 8);
	if(zero_branch && b0 == 0xFA && (b3 == 0xB7 || b3 == 0xA7) &&
			b4 == branch_opcode &&
			__gb_fast_stat_poll_branch_to(branch_pc, (int8_t)b5, target_pc))
	{
		if(polled_addr < WRAM_0_ADDR)
			return WGB_POLLWAIT_REJ_UNSAFE_ADDR;
		return WGB_POLLWAIT_ACCEPT;
	}

	return WGB_POLLWAIT_REJ_UNSUPPORTED;
}

static inline void __gb_fast_poll_wait_maybe(struct gb_s *gb,
		uint16_t target_pc, uint16_t branch_pc, uint8_t branch_opcode)
{
	if(__gb_fast_poll_wait_reason(gb, target_pc, branch_pc, branch_opcode) ==
			WGB_POLLWAIT_ACCEPT)
	{
		/* The wait-skip below computes distances from live counters. */
		__gb_lazy_post_flush_gb(gb);
		if(!__gb_fast_ly_poll_wait_apply(gb, target_pc, branch_pc, branch_opcode))
			gb->gb_halt = true;
	}
}
#else
static inline void __gb_fast_poll_wait_maybe(struct gb_s *gb,
		uint16_t target_pc, uint16_t branch_pc, uint8_t branch_opcode)
{
	(void)gb;
	(void)target_pc;
	(void)branch_pc;
	(void)branch_opcode;
}
#endif

static inline bool __gb_fast_stat_poll_pattern(const struct gb_s *gb,
		uint16_t stat_read_pc, uint16_t next_pc)
{
	uint8_t b0, b1, b2, b3, b4, b5;

	if(!__gb_code_peek8(gb, next_pc, &b0))
		return false;

	if(b0 == 0xE6)
	{
		if(!__gb_code_peek8(gb, (uint16_t)(next_pc + 1), &b1) ||
				!__gb_code_peek8(gb, (uint16_t)(next_pc + 2), &b2) ||
				!__gb_code_peek8(gb, (uint16_t)(next_pc + 3), &b3))
			return false;

		if(b2 == 0x20 && (b1 & 0x02) &&
				__gb_fast_stat_poll_branch_to((uint16_t)(next_pc + 2),
					(int8_t)b3, stat_read_pc))
			return true;

		/* Common VRAM-safe wait loops also use:
		 *   ld a,(STAT); and mode_mask; cp mode; jr cc,loop
		 * Only accelerate cases where returning HBlank makes the branch exit.
		 */
		if(b2 == 0xFE &&
				__gb_code_peek8(gb, (uint16_t)(next_pc + 4), &b4) &&
				__gb_code_peek8(gb, (uint16_t)(next_pc + 5), &b5) &&
				(b1 & STAT_MODE) != 0)
		{
			const uint8_t compare_mode = (uint8_t)(b3 & (b1 & STAT_MODE));
			if(b4 == 0x28 && compare_mode != IO_STAT_MODE_HBLANK &&
					__gb_fast_stat_poll_branch_to((uint16_t)(next_pc + 4),
						(int8_t)b5, stat_read_pc))
				return true;
			if(b4 == 0x20 && compare_mode == IO_STAT_MODE_HBLANK &&
					__gb_fast_stat_poll_branch_to((uint16_t)(next_pc + 4),
						(int8_t)b5, stat_read_pc))
				return true;
		}

		return false;
	}

	if(b0 == 0xCB)
	{
		if(!__gb_code_peek8(gb, (uint16_t)(next_pc + 1), &b1) ||
				!__gb_code_peek8(gb, (uint16_t)(next_pc + 2), &b2) ||
				!__gb_code_peek8(gb, (uint16_t)(next_pc + 3), &b3))
			return false;

		const bool bit_test_a = b1 >= 0x40 && b1 <= 0x7F && (b1 & 0x07) == 0x07;
		const uint8_t bit = (uint8_t)((b1 >> 3) & 0x07);
		return bit_test_a && bit == 1 && b2 == 0x20 &&
			__gb_fast_stat_poll_branch_to((uint16_t)(next_pc + 2),
				(int8_t)b3, stat_read_pc);
	}

	uint8_t mask;
	if(__gb_fast_stat_poll_and_reg_mask(gb, b0, &mask))
	{
		if(!__gb_code_peek8(gb, (uint16_t)(next_pc + 1), &b1) ||
				!__gb_code_peek8(gb, (uint16_t)(next_pc + 2), &b2))
			return false;

		/* Oracles' LCD ISR uses ld a,(STAT); and c; jr nz,loop with
		 * C=$03 to wait until mode 0. Treat pure mode masks like immediates. */
		return (mask & STAT_MODE) != 0 && (mask & (uint8_t)~STAT_MODE) == 0 &&
			b1 == 0x20 &&
			__gb_fast_stat_poll_branch_to((uint16_t)(next_pc + 1),
				(int8_t)b2, stat_read_pc);
	}

	return false;
}

static inline bool __gb_fast_stat_poll_c_pattern(const struct gb_s *gb,
		uint16_t stat_read_pc, uint16_t next_pc)
{
	uint8_t b0, b1, b2;

	if((gb->cpu_reg.bc.bytes.b & STAT_MODE) == 0)
		return false;
	if(!__gb_code_peek8(gb, next_pc, &b0) ||
			!__gb_code_peek8(gb, (uint16_t)(next_pc + 1), &b1) ||
			!__gb_code_peek8(gb, (uint16_t)(next_pc + 2), &b2))
		return false;

	return b0 == 0xA0 && b1 == 0x20 &&
		__gb_fast_stat_poll_branch_to((uint16_t)(next_pc + 1),
			(int8_t)b2, stat_read_pc);
}

static inline bool __gb_fast_stat_poll_hblank_cycles(
		const struct gb_s *gb, uint_fast32_t *cycles)
{
	const uint8_t mode = gb->hram_io[IO_STAT] & STAT_MODE;
	uint_fast16_t target;

	if(mode == IO_STAT_MODE_OAM_SCAN)
		target = LCD_MODE3_LCD_DRAW_END;
	else if(mode == IO_STAT_MODE_LCD_DRAW)
		target = LCD_MODE3_LCD_DRAW_END;
	else
		return false;

	const uint_fast16_t lcd_count = gb->counter.lcd_count;
	const uint_fast32_t lcd_cycles = (lcd_count < target) ?
		(target - lcd_count) : 1u;
	*cycles = __gb_fast_poll_wait_lcd_to_cpu_cycles(gb, lcd_cycles);
	return true;
}

static inline uint8_t __gb_fast_stat_poll_read_hblank(struct gb_s *gb,
		uint8_t fallback_stat)
{
	uint_fast32_t wait_cycles;

	/* The hblank distance below is computed from live counters. */
	__gb_lazy_post_flush_gb(gb);

	if(!__gb_fast_stat_poll_hblank_cycles(gb, &wait_cycles))
		return fallback_stat;
	if(wait_cycles != 0 &&
			!__gb_fast_poll_wait_can_skip_wait(gb, wait_cycles))
		return fallback_stat;
	if(wait_cycles != 0)
		__gb_fast_poll_wait_advance_cycles(gb, wait_cycles, false);

	return (uint8_t)((gb->hram_io[IO_STAT] & (uint8_t)~STAT_MODE) |
		IO_STAT_MODE_HBLANK);
}

static inline uint8_t __gb_fast_stat_poll_read(struct gb_s *gb,
		uint16_t stat_read_pc, uint16_t next_pc)
{
	const uint8_t stat = gb->hram_io[IO_STAT];
	const uint8_t mode = stat & STAT_MODE;


	if(!(gb->hram_io[IO_LCDC] & LCDC_ENABLE))
		return stat;
	if(mode != IO_STAT_MODE_OAM_SCAN && mode != IO_STAT_MODE_LCD_DRAW)
		return stat;
	if(!__gb_fast_stat_poll_pattern(gb, stat_read_pc, next_pc))
		return stat;

	return __gb_fast_stat_poll_read_hblank(gb, stat);
}

static inline uint8_t __gb_fast_stat_poll_c_read(struct gb_s *gb,
		uint16_t stat_read_pc, uint16_t next_pc)
{
	const uint8_t stat = gb->hram_io[IO_STAT];
	const uint8_t mode = stat & STAT_MODE;


	if(!(gb->hram_io[IO_LCDC] & LCDC_ENABLE))
		return stat;
	if(mode != IO_STAT_MODE_OAM_SCAN && mode != IO_STAT_MODE_LCD_DRAW)
		return stat;
	if(!__gb_fast_stat_poll_c_pattern(gb, stat_read_pc, next_pc))
		return stat;

	return __gb_fast_stat_poll_read_hblank(gb, stat);
}

#endif

#if !WALNUT_GB_FAST_STAT_POLL_ACTIVE && !WALNUT_GB_FAST_POLL_WAIT_ACTIVE
static inline void __gb_fast_poll_wait_maybe(struct gb_s *gb,
		uint16_t target_pc, uint16_t branch_pc, uint8_t branch_opcode)
{
	(void)gb;
	(void)target_pc;
	(void)branch_pc;
	(void)branch_opcode;
}
#endif

static inline bool __gb_fast_io_read_noapu(const struct gb_s *gb,
		uint8_t io, uint8_t *out)
{
	if(io >= 0x10 && io <= 0x3F)
		return false;
#if WALNUT_FULL_GBC_SUPPORT
	switch(io)
	{
	case 0x4D:
	case 0x4F:
	case 0x51:
	case 0x52:
	case 0x53:
	case 0x54:
	case 0x55:
	case 0x56:
	case 0x68:
	case 0x69:
	case 0x6A:
	case 0x6B:
	case 0x70:
		return false;
	default:
		break;
	}
#endif

	*out = gb->hram_io[io];
	return true;
}

static inline bool __gb_fast_hram_write(struct gb_s *gb, uint8_t io, uint8_t val)
{
	if(io < 0x80)
		return false;

	gb->hram_io[io] = val;
	return true;
}

static inline bool __gb_fast_abs_read_common(const struct gb_s *gb,
		uint16_t addr, uint8_t *out)
{
	switch(WALNUT_GB_GET_MSN16(addr))
	{
	case 0xC:
		*out = gb->wram[addr - WRAM_0_ADDR];
		return true;

	case 0xD:
#if WALNUT_FULL_GBC_SUPPORT
		if(gb->cgb.cgbMode)
			*out = gb->wram[addr - gb->cgb.wramBankOffset];
		else
#endif
			*out = gb->wram[addr - WRAM_0_ADDR];
		return true;

	case 0xE:
#if WALNUT_FULL_GBC_SUPPORT
		if(gb->cgb.cgbMode)
			*out = gb->wram[(addr - 0x2000) - gb->cgb.wramBankOffset];
		else
#endif
			*out = gb->wram[addr - ECHO_ADDR];
		return true;

	case 0xF:
		if(addr < OAM_ADDR)
		{
#if WALNUT_FULL_GBC_SUPPORT
			if(gb->cgb.cgbMode)
				*out = gb->wram[(addr - 0x2000) - gb->cgb.wramBankOffset];
			else
#endif
				*out = gb->wram[addr - ECHO_ADDR];
			return true;
		}
		if(addr >= HRAM_ADDR)
		{
			*out = gb->hram_io[addr - IO_ADDR];
			return true;
		}
		if(addr >= IO_ADDR)
		{
			const uint8_t io = (uint8_t)(addr - IO_ADDR);
			if(io >= 0x10 && io <= 0x3F)
				return false;
			*out = gb->hram_io[io];
			return true;
		}
		break;
	}

	return false;
}

static inline bool WALNUT_GB_HOT_ATTR __gb_fast_abs_read_hot(
		const struct gb_s *gb, uint16_t addr, uint8_t *out)
{
	if(WGB_LIKELY(addr >= WRAM_0_ADDR && addr < ECHO_ADDR))
	{
#if WALNUT_FULL_GBC_SUPPORT
		if(gb->cgb.cgbMode && addr >= WRAM_1_ADDR)
			*out = gb->wram[addr - gb->cgb.wramBankOffset];
		else
#endif
			*out = gb->wram[addr - WRAM_0_ADDR];
		return true;
	}

	if(WGB_LIKELY(addr >= HRAM_ADDR))
	{
		*out = gb->hram_io[addr - IO_ADDR];
		return true;
	}

	return __gb_fast_abs_read_common(gb, addr, out);
}

static inline uint8_t WALNUT_GB_HOT_ATTR __gb_fetch8(struct gb_s *gb)
{
	const uint16_t pc = gb->cpu_reg.pc.reg++;

	switch(WALNUT_GB_GET_MSN16(pc))
	{
	case 0x0:
		if(gb->hram_io[IO_BOOT] == 0 && pc < 0x0100)
		{
			return gb->gb_bootrom_read(gb, pc);
		}
#if WALNUT_FULL_GBC_SUPPORT
		if(gb->cgb.cgbMode && gb->hram_io[IO_BOOT] == 0 &&
				pc >= 0x0200 && pc < 0x0900)
		{
			return gb->gb_bootrom_read(gb, pc);
		}
#endif
		/* Fallthrough */
	case 0x1:
	case 0x2:
	case 0x3:
		if(WGB_LIKELY(__gb_direct_rom_contains(gb, pc, 1)))
			return __gb_direct_rom_read8(gb, pc);
		return gb->gb_rom_read(gb, pc);

	case 0x4:
	case 0x5:
	case 0x6:
	case 0x7:
	{
		const uint32_t bank_addr = pc + __gb_fetch_rom_bank_offset(gb);
		if(WGB_LIKELY(__gb_direct_rom_contains(gb, bank_addr, 1)))
			return __gb_direct_rom_read8(gb, bank_addr);
		return gb->gb_rom_read(gb, bank_addr);
	}

	case 0xC:
		return gb->wram[pc - WRAM_0_ADDR];

	case 0xD:
#if WALNUT_FULL_GBC_SUPPORT
		if(gb->cgb.cgbMode)
			return gb->wram[pc - gb->cgb.wramBankOffset];
#endif
		return gb->wram[pc - WRAM_0_ADDR];

	case 0xE:
		return gb->wram[pc - ECHO_ADDR];

	case 0xF:
		if(pc < OAM_ADDR)
		{
#if WALNUT_FULL_GBC_SUPPORT
			if(gb->cgb.cgbMode)
				return gb->wram[(pc - 0x2000) - gb->cgb.wramBankOffset];
#endif
			return gb->wram[pc - ECHO_ADDR];
		}
		if(pc >= HRAM_ADDR)
		{
			return gb->hram_io[pc - IO_ADDR];
		}
		break;
	}

	return __gb_read(gb, pc);
}

static inline uint16_t __gb_fetch16(struct gb_s *gb)
{
	const uint16_t pc = gb->cpu_reg.pc.reg;

	switch(WALNUT_GB_GET_MSN16(pc))
	{
	case 0x0:
		if(gb->hram_io[IO_BOOT] == 0 && pc < 0x0100)
			break;
#if WALNUT_FULL_GBC_SUPPORT
		if(gb->cgb.cgbMode && gb->hram_io[IO_BOOT] == 0 &&
				pc >= 0x0200 && pc < 0x0900)
			break;
#endif
		/* Fallthrough */
	case 0x1:
	case 0x2:
	case 0x3:
		if(WGB_LIKELY(pc <= 0x3FFE))
		{
			gb->cpu_reg.pc.reg = pc + 2;
			if(WGB_LIKELY(__gb_direct_rom_contains(gb, pc, 2)))
				return __gb_direct_rom_read16(gb, pc);
			return gb->gb_rom_read_16bit(gb, pc);
		}
		break;

	case 0x4:
	case 0x5:
	case 0x6:
	case 0x7:
		if(WGB_LIKELY(pc <= 0x7FFE))
		{
			const uint32_t bank_addr = pc + __gb_fetch_rom_bank_offset(gb);
			gb->cpu_reg.pc.reg = pc + 2;
			if(WGB_LIKELY(__gb_direct_rom_contains(gb, bank_addr, 2)))
				return __gb_direct_rom_read16(gb, bank_addr);
			return gb->gb_rom_read_16bit(gb, bank_addr);
		}
		break;

	case 0xC:
		if(pc <= 0xCFFE)
		{
			const uint16_t idx = pc - WRAM_0_ADDR;
			gb->cpu_reg.pc.reg = pc + 2;
			return (uint16_t)gb->wram[idx] | ((uint16_t)gb->wram[idx + 1] << 8);
		}
		break;

	case 0xD:
		if(pc <= 0xDFFE)
		{
			gb->cpu_reg.pc.reg = pc + 2;
#if WALNUT_FULL_GBC_SUPPORT
			if(gb->cgb.cgbMode)
			{
				const uint16_t idx = pc - gb->cgb.wramBankOffset;
				return (uint16_t)gb->wram[idx] |
					((uint16_t)gb->wram[idx + 1] << 8);
			}
#endif
			{
				const uint16_t idx = pc - WRAM_0_ADDR;
				return (uint16_t)gb->wram[idx] |
					((uint16_t)gb->wram[idx + 1] << 8);
			}
		}
		break;

	case 0xE:
		if(pc <= 0xEFFE)
		{
			const uint16_t idx = pc - ECHO_ADDR;
			gb->cpu_reg.pc.reg = pc + 2;
			return (uint16_t)gb->wram[idx] | ((uint16_t)gb->wram[idx + 1] << 8);
		}
		break;

	case 0xF:
		if(pc < OAM_ADDR)
		{
			if(pc <= 0xFDFE)
			{
				gb->cpu_reg.pc.reg = pc + 2;
#if WALNUT_FULL_GBC_SUPPORT
				if(gb->cgb.cgbMode)
				{
					const uint16_t idx = (pc - 0x2000) - gb->cgb.wramBankOffset;
					return (uint16_t)gb->wram[idx] |
						((uint16_t)gb->wram[idx + 1] << 8);
				}
#endif
				{
					const uint16_t idx = pc - ECHO_ADDR;
					return (uint16_t)gb->wram[idx] |
						((uint16_t)gb->wram[idx + 1] << 8);
				}
			}
			break;
		}
		if(pc >= HRAM_ADDR && pc <= 0xFFFE)
		{
			const uint16_t idx = pc - IO_ADDR;
			gb->cpu_reg.pc.reg = pc + 2;
			return (uint16_t)gb->hram_io[idx] |
				((uint16_t)gb->hram_io[idx + 1] << 8);
		}
		break;
	}

	return (uint16_t)__gb_fetch8(gb) | ((uint16_t)__gb_fetch8(gb) << 8);
}



static inline void WALNUT_GB_HOT_ATTR
__gb_mbc3_write_fast(struct gb_s *gb, uint_fast16_t addr, uint8_t val)
{

	switch(WALNUT_GB_GET_MSN16(addr))
	{
	case 0x0:
	case 0x1:
		if(gb->cart_ram)
			gb->enable_cart_ram = ((val & 0x0F) == 0x0A);
		return;

	case 0x2:
	case 0x3:
	{
		const uint16_t old_rom_bank = gb->selected_rom_bank;
		uint16_t new_rom_bank = gb->cart_is_mbc3O ? val : (val & 0x7F);
		if(!new_rom_bank)
			new_rom_bank++;
		new_rom_bank &= gb->num_rom_banks_mask;

		if(new_rom_bank != old_rom_bank)
		{
			gb->selected_rom_bank = new_rom_bank;
			__gb_refresh_rom_bank_offset(gb);
		}
#if WALNUT_GB_SAFE_DUALFETCH_MBC
		gb->prefetch_invalid = true;
#endif
		return;
	}

	case 0x4:
	case 0x5:
	{
		gb->cart_ram_bank = val;
		if(!gb->cart_is_mbc3O && gb->cart_ram_bank < 0x8)
			gb->cart_ram_bank &= 0x3;
#if WALNUT_GB_SAFE_DUALFETCH_MBC
		gb->prefetch_invalid = true;
#endif
		return;
	}

	case 0x6:
	case 0x7:
	{
		const uint8_t old_mode = gb->cart_mode_select;
		val &= 1;
		if(val && gb->cart_mode_select == 0)
			memcpy(&gb->rtc_latched.bytes, &gb->rtc_real.bytes,
				sizeof(gb->rtc_latched.bytes));
		gb->cart_mode_select = val;

		if(gb->cart_mode_select != old_mode)
			__gb_refresh_rom_bank_offset(gb);
#if WALNUT_GB_SAFE_DUALFETCH_MBC
		gb->prefetch_invalid = true;
#endif
		return;
	}
	}
}

static inline void WALNUT_GB_HOT_ATTR
__gb_mbc_write(struct gb_s *gb, uint_fast16_t addr, uint8_t val)
{
	if(WGB_LIKELY(gb->mbc == 3))
	{
		__gb_mbc3_write_fast(gb, addr, val);
		return;
	}

	const uint16_t old_rom_bank = gb->selected_rom_bank;
	const uint8_t old_mode = gb->cart_mode_select;
	bool invalidate_prefetch = false;

	switch(WALNUT_GB_GET_MSN16(addr))
	{
	case 0x0:
	case 0x1:
		/* Set RAM enable bit. MBC2 is handled in fall-through. */
		if(gb->mbc > 0 && gb->mbc != 2)
		{
			if(gb->cart_ram)
				gb->enable_cart_ram = ((val & 0x0F) == 0x0A);
			break;
		}

		/* Intentional fall through. */
	case 0x2:
		if(gb->mbc == 5)
		{
			gb->selected_rom_bank =
				((gb->selected_rom_bank & 0x100) | val) &
				gb->num_rom_banks_mask;
			invalidate_prefetch = true;
			break;
		}

		/* Intentional fall through. */
	case 0x3:
		if(gb->mbc == 1)
		{
			gb->selected_rom_bank =
				(val & 0x1F) | (gb->selected_rom_bank & 0x60);
			if((gb->selected_rom_bank & 0x1F) == 0x00)
				gb->selected_rom_bank++;
		}
		else if(gb->mbc == 2)
		{
			/* Bit 8 distinguishes ROM bank selection from RAM enable. */
			if(addr & 0x100)
			{
				gb->selected_rom_bank = val & 0x0F;
				if(!gb->selected_rom_bank)
					gb->selected_rom_bank++;
			}
			else
			{
				gb->enable_cart_ram = ((val & 0x0F) == 0x0A);
				break;
			}
		}
		else if(gb->mbc == 3)
		{
			gb->selected_rom_bank = gb->cart_is_mbc3O ? val : (val & 0x7F);
			if(!gb->selected_rom_bank)
				gb->selected_rom_bank++;
		}
		else if(gb->mbc == 5)
		{
			gb->selected_rom_bank =
				((val & 0x01) << 8) | (gb->selected_rom_bank & 0xFF);
		}

		gb->selected_rom_bank &= gb->num_rom_banks_mask;
		invalidate_prefetch = true;
		break;

	case 0x4:
	case 0x5:
		if(gb->mbc == 1)
		{
			gb->cart_ram_bank = val & 3;
			gb->selected_rom_bank =
				((val & 3) << 5) | (gb->selected_rom_bank & 0x1F);
			gb->selected_rom_bank &= gb->num_rom_banks_mask;
		}
		else if(gb->mbc == 3)
		{
			gb->cart_ram_bank = val;
			/* Standard MBC3 exposes four RAM banks and RTC registers 08-0C. */
			if(!gb->cart_is_mbc3O && gb->cart_ram_bank < 0x8)
				gb->cart_ram_bank &= 0x3;
		}
		else if(gb->mbc == 5)
		{
			if(gb->cart_is_pocket_camera)
				gb->cart_ram_bank = val & 0x1F;
			else
				gb->cart_ram_bank = val & gb->mbc5_ram_bank_mask;
		}
		invalidate_prefetch = true;
		break;

	case 0x6:
	case 0x7:
		val &= 1;
		if(gb->mbc == 3 && val && gb->cart_mode_select == 0)
			memcpy(&gb->rtc_latched.bytes, &gb->rtc_real.bytes,
				sizeof(gb->rtc_latched.bytes));
		gb->cart_mode_select = val;
		invalidate_prefetch = true;
		break;
	}

	if(gb->selected_rom_bank != old_rom_bank || gb->cart_mode_select != old_mode)
		__gb_refresh_rom_bank_offset(gb);

#if WALNUT_GB_SAFE_DUALFETCH_MBC
	if(invalidate_prefetch)
		gb->prefetch_invalid = true;
#else
	(void)invalidate_prefetch;
#endif
}

void WALNUT_GB_HOT_ATTR __gb_write(struct gb_s *gb, uint_fast16_t addr, uint8_t val)
{
	if(WGB_LIKELY(addr >= WRAM_0_ADDR && addr < ECHO_ADDR))
	{
#if WALNUT_FULL_GBC_SUPPORT
		if(addr >= WRAM_1_ADDR)
			gb->wram[addr - gb->cgb.wramBankOffset] = val;
		else
#endif
			gb->wram[addr - WRAM_0_ADDR] = val;
		return;
	}

	if(WGB_LIKELY(addr >= ECHO_ADDR && addr < OAM_ADDR))
	{
#if WALNUT_FULL_GBC_SUPPORT
		if(addr >= 0xF000)
			gb->wram[(addr - 0x2000) - gb->cgb.wramBankOffset] = val;
		else
#endif
			gb->wram[addr - ECHO_ADDR] = val;
		return;
	}

	if(WGB_LIKELY(HRAM_ADDR <= addr && addr < INTR_EN_ADDR))
	{
		gb->hram_io[addr - IO_ADDR] = val;
		return;
	}

	if(WALNUT_GB_GET_MSN16(addr) <= 0x7)
	{
		__gb_mbc_write(gb, addr, val);
		return;
	}

	switch(WALNUT_GB_GET_MSN16(addr))
	{
	case 0x8:
	case 0x9:
		__gb_display_mark_dirty(gb);
#if WALNUT_FULL_GBC_SUPPORT
		gb->vram[addr - gb->cgb.vramBankOffset] = val;
#else
		gb->vram[addr - VRAM_ADDR] = val;
#endif
		return;

	case 0xA:
	case 0xB:
		if(__gb_pocket_camera_regs_selected(gb))
		{
			__gb_pocket_camera_write(gb, addr, val);
		}
		else if(gb->mbc == 3 && gb->cart_ram_bank >= 0x08)
		{
			const uint8_t rtc_reg_mask[5] = {
				0x3F, 0x3F, 0x1F, 0xFF, 0xC1
			};
			uint8_t reg = gb->cart_ram_bank - 0x08;
			//if(reg == 0) gb->counter.rtc_count = 0;

			gb->rtc_real.bytes[reg] = val & rtc_reg_mask[reg];
			gb->counter.cpu_event_count = 0;
			gb->counter.cpu_event_flags = 0;
		}
		/* Do not write to RAM if unavailable or disabled. */
		else if(gb->cart_ram && gb->enable_cart_ram)
		{
			if(gb->mbc == 2)
			{
				/* Only 9 bits are available in address. */
				addr &= 0x1FF;
				/* Data is only 4 bits wide in MBC2 RAM. */
				val &= 0x0F;
				/* Upper nibble is set to high. */
				val |= 0xF0;
				gb->gb_cart_ram_write(gb, addr, val);
			}
			/* If cart has RAM, use this. If MBC1, only the first
			 * RAM bank can be written to if the advanced banking
			 * mode is selected. */
			else if(((gb->mbc == 1 && gb->cart_mode_select) || gb->mbc != 1) &&
					gb->cart_ram_bank < gb->num_ram_banks)
			{
				gb->gb_cart_ram_write(gb,
					addr - CART_RAM_ADDR + (gb->cart_ram_bank * CRAM_BANK_SIZE), val);
			}
			else if(gb->num_ram_banks)
			{
				gb->gb_cart_ram_write(gb, addr - CART_RAM_ADDR, val);
			}
		}

		return;

	case 0xC:
		gb->wram[addr - WRAM_0_ADDR] = val;
		return;

	case 0xD:
#if WALNUT_FULL_GBC_SUPPORT
		gb->wram[addr - gb->cgb.wramBankOffset] = val;
#else
		gb->wram[addr - WRAM_1_ADDR + WRAM_BANK_SIZE] = val;
#endif
		return;

	case 0xE:
		gb->wram[addr - ECHO_ADDR] = val;
		return;

	case 0xF:
		if(addr < OAM_ADDR)
		{
#if WALNUT_FULL_GBC_SUPPORT
			gb->wram[(addr - 0x2000) - gb->cgb.wramBankOffset] = val;
#else
			gb->wram[addr - ECHO_ADDR] = val;
#endif
			return;
		}

		if(addr < UNUSED_ADDR)
		{
			__gb_display_mark_dirty(gb);
			WGB_GB_GLITCH_APPLY_OAM_WRITE((uint16_t)addr, &val);
			gb->oam[addr - OAM_ADDR] = val;
			__gb_oam_cache_invalidate(gb);
			return;
		}

		/* Unusable memory area. */
		if(addr < IO_ADDR)
		{
			return;
		}

		if(HRAM_ADDR <= addr && addr < INTR_EN_ADDR)
		{
			gb->hram_io[addr - IO_ADDR] = val;
			return;
		}

		if((addr >= 0xFF10) && (addr <= 0xFF3F))
		{
			if(WGB_GB_GLITCH_APPLY_APU_WRITE((uint16_t)addr, &val))
				return;
#if ENABLE_SOUND
			audio_write(addr, val);
#else
			gb->hram_io[addr - IO_ADDR] = val;
#endif
			return;
		}

		/* IO and Interrupts. Apply any lazily batched cycles first so the
		 * register handler sees exact counter state. */
		__gb_lazy_post_flush_gb(gb);
		gb->counter.cpu_event_count = 0;
		gb->counter.cpu_event_flags = 0;
		switch(WALNUT_GB_GET_LSB16(addr))
		{
		/* Joypad */
		case 0x00:
			/* Only bits 5 and 4 are R/W.
			 * The lower bits are overwritten later, and the two most
			 * significant bits are unused. */
			gb->hram_io[IO_JOYP] = val;

			/* Direction keys selected */
			if((gb->hram_io[IO_JOYP] & 0x10) == 0)
				gb->hram_io[IO_JOYP] |= (gb->direct.joypad >> 4);
			/* Button keys selected */
			else
				gb->hram_io[IO_JOYP] |= (gb->direct.joypad & 0x0F);

			return;

		/* Serial */
		case 0x01:
			gb->hram_io[IO_SB] = val;
			return;

		case 0x02:
			gb->hram_io[IO_SC] = val;
			return;

		/* Timer Registers */
		case 0x04:
			gb->hram_io[IO_DIV] = 0x00;
			return;

		case 0x05:
			gb->hram_io[IO_TIMA] = val;
			return;

		case 0x06:
			gb->hram_io[IO_TMA] = val;
			return;

		case 0x07:
			gb->hram_io[IO_TAC] = val;
			return;

		/* Interrupt Flag Register */
		case 0x0F:
			gb->hram_io[IO_IF] = (val | 0xE0);
			return;

		/* LCD Registers */
		case 0x40:
		{
			uint8_t lcd_enabled;

			/* Check if LCD is already enabled. */
			lcd_enabled = (gb->hram_io[IO_LCDC] & LCDC_ENABLE);

			if(gb->hram_io[IO_LCDC] != val)
				__gb_display_mark_dirty(gb);
			gb->hram_io[IO_LCDC] = val;

			/* Check if LCD is going to be switched on. */
			if (!lcd_enabled && (val & LCDC_ENABLE))
			{
				gb->lcd_blank = true;
			}
			/* Check if LCD is being switched off. */
			else if (lcd_enabled && !(val & LCDC_ENABLE))
			{
				/* Walnut-GB will happily turn off LCD outside
				 * of VBLANK even though this damages real
				 * hardware. */

				/* Set LCD to Mode 0. */
				gb->hram_io[IO_STAT] =
					(gb->hram_io[IO_STAT] & ~STAT_MODE) |
					IO_STAT_MODE_HBLANK;
				/* LY fixed to 0 when LCD turned off. */
				gb->hram_io[IO_LY] = 0;
				/* Keep track of lcd_count to correctly track
				 * passing time. */
				gb->counter.lcd_off_count += gb->counter.lcd_count;
				/* Reset LCD timer, since the LCD starts from
				 * the beginning on power on. */
				gb->counter.lcd_count = 0;
			}
			__gb_lcd_reschedule(gb);
			return;
		}

		case 0x41:
			gb->hram_io[IO_STAT] = (val & STAT_USER_BITS) | (gb->hram_io[IO_STAT] & STAT_MODE) | 0x80;
			return;

		case 0x42:
			if(gb->hram_io[IO_SCY] != val)
				__gb_display_mark_dirty(gb);
			gb->hram_io[IO_SCY] = val;
			return;

		case 0x43:
			if(gb->hram_io[IO_SCX] != val)
				__gb_display_mark_dirty(gb);
			gb->hram_io[IO_SCX] = val;
			return;

		/* LY (0xFF44) is read only. */
		case 0x45:
			gb->hram_io[IO_LYC] = val;
			return;

		/* DMA Register */
		case 0x46:
		{
			uint16_t dma_addr;
			uint16_t i;
#if WALNUT_FULL_GBC_SUPPORT
			dma_addr = (uint_fast16_t)(val % 0xF1) << 8;
			gb->hram_io[IO_DMA] = (val % 0xF1);
#else
			dma_addr = (uint_fast16_t)val << 8;
			gb->hram_io[IO_DMA] = val;
#endif
#if WALNUT_GB_32BIT_DMA
#if WALNUT_GB_32BIT_ALIGNED
		/* Alignment aware 32-bit read path: fetch four bytes at a time */
		uint8_t *oam = gb->oam;

		if (((uintptr_t)oam & 3))
				for (i = 0; i < OAM_SIZE; i += 4)
				{
						uint32_t v = __gb_read32(gb, dma_addr + i);
						oam[i+0] = v;
						oam[i+1] = v >> 8;
						oam[i+2] = v >> 16;
						oam[i+3] = v >> 24;
				}
		else
			for (i = 0; i < OAM_SIZE; i += 4)
			{
					uint32_t v = __gb_read32(gb, dma_addr + i);
					*(uint32_t *)(oam + i) = v;
			}
#else
    /* 32-bit read path: fetch four bytes at a time */
    for (i = 0; i < OAM_SIZE; i += 4)
    {
        uint32_t v = __gb_read32(gb, dma_addr + i);
				*((uint32_t *)(gb->oam + i)) = v;
    }
#endif
#elif WALNUT_GB_16BIT_DMA
    /* 16-bit read path: fetch two bytes at a time */
    for (i = 0; i < OAM_SIZE; i += 2)
    {
        uint16_t v = __gb_read16(gb, dma_addr + i);
        gb->oam[i]   = v & 0xFF;
        gb->oam[i+1] = v >> 8;
    }
#else
    /* Original 8-bit read path */
    for (i = 0; i < OAM_SIZE; i++) {
        gb->oam[i] = __gb_read(gb, dma_addr + i);
    }
#endif
			WGB_GB_GLITCH_APPLY_OAM_DMA(gb->oam);
			__gb_oam_cache_invalidate(gb);
			__gb_display_mark_dirty(gb);
#if WALNUT_GB_SAFE_DUALFETCH_DMA
		gb->prefetch_invalid=true;
#endif
			return;
		}

		/* DMG Palette Registers */
		case 0x47:
			if(gb->hram_io[IO_BGP] != val)
				__gb_display_mark_dirty(gb);
			gb->hram_io[IO_BGP] = val;
			gb->display.bg_palette[0] = (gb->hram_io[IO_BGP] & 0x03);
			gb->display.bg_palette[1] = (gb->hram_io[IO_BGP] >> 2) & 0x03;
			gb->display.bg_palette[2] = (gb->hram_io[IO_BGP] >> 4) & 0x03;
			gb->display.bg_palette[3] = (gb->hram_io[IO_BGP] >> 6) & 0x03;
			return;

		case 0x48:
			if(gb->hram_io[IO_OBP0] != val)
				__gb_display_mark_dirty(gb);
			gb->hram_io[IO_OBP0] = val;
			gb->display.sp_palette[0] = (gb->hram_io[IO_OBP0] & 0x03);
			gb->display.sp_palette[1] = (gb->hram_io[IO_OBP0] >> 2) & 0x03;
			gb->display.sp_palette[2] = (gb->hram_io[IO_OBP0] >> 4) & 0x03;
			gb->display.sp_palette[3] = (gb->hram_io[IO_OBP0] >> 6) & 0x03;
			return;

		case 0x49:
			if(gb->hram_io[IO_OBP1] != val)
				__gb_display_mark_dirty(gb);
			gb->hram_io[IO_OBP1] = val;
			gb->display.sp_palette[4] = (gb->hram_io[IO_OBP1] & 0x03);
			gb->display.sp_palette[5] = (gb->hram_io[IO_OBP1] >> 2) & 0x03;
			gb->display.sp_palette[6] = (gb->hram_io[IO_OBP1] >> 4) & 0x03;
			gb->display.sp_palette[7] = (gb->hram_io[IO_OBP1] >> 6) & 0x03;
			return;

		/* Window Position Registers */
		case 0x4A:
			if(gb->hram_io[IO_WY] != val)
				__gb_display_mark_dirty(gb);
			gb->hram_io[IO_WY] = val;
			return;

		case 0x4B:
			if(gb->hram_io[IO_WX] != val)
				__gb_display_mark_dirty(gb);
			gb->hram_io[IO_WX] = val;
			return;

#if WALNUT_FULL_GBC_SUPPORT
		/* Prepare Speed Switch*/
		case 0x4D:
			gb->cgb.doubleSpeedPrep = val & 1;
			return;

		/* CGB VRAM Bank*/
		case 0x4F:
			/* VBK remains writable during HDMA; software is expected not to
			 * change it mid-transfer, but the write itself still takes effect. */
			gb->cgb.vramBank = val & 0x01;
			if(gb->cgb.cgbMode) gb->cgb.vramBankOffset = VRAM_ADDR - (gb->cgb.vramBank << 13);
			return;
#endif
		/* Turn off boot ROM */
		case 0x50:
			gb->hram_io[IO_BOOT] = 0x01;
			return;
#if WALNUT_FULL_GBC_SUPPORT
		/* DMA Register */
		case 0x51:
			gb->cgb.dmaSource = (gb->cgb.dmaSource & 0xFF) + (val << 8);
			return;
		case 0x52:
			gb->cgb.dmaSource = (gb->cgb.dmaSource & 0xFF00) + val;
			return;
		case 0x53:
			gb->cgb.dmaDest = (gb->cgb.dmaDest & 0xFF) + (val << 8);
			return;
		case 0x54:
			gb->cgb.dmaDest = (gb->cgb.dmaDest & 0xFF00) + val;
			return;

		/* DMA Register*/
		case 0x55:
			gb->cgb.dmaSize = (val & 0x7F) + 1;
			gb->cgb.dmaMode = val >> 7;
			//DMA GBC
			if(gb->cgb.dmaActive)
			{  // Only transfer if dma is not active (=1) otherwise treat it as a termination
#if	WALNUT_GB_32BIT_DMA
	      if (gb->cgb.cgbMode && !gb->cgb.dmaMode)
        {
            uint16_t src = gb->cgb.dmaSource & 0xFFF0;
            uint16_t dst = (gb->cgb.dmaDest & 0x1FF0) | 0x8000;
            uint16_t count = gb->cgb.dmaSize << 4;

            for (uint16_t i = 0; i < count; i += 4)
            {
                // 32-bit read from source
                uint32_t val32 = __gb_read32(gb, src + i);
                // 8-bit writes to destination (4 bytes)
								__gb_write32(gb, dst + i, val32);
								//__gb_write16(gb, dst + i + 2, val32 >> 16);
                // __gb_write(gb, dst + i, val32 );
                // __gb_write(gb, dst + i + 1, val32 >> 8);
	              // __gb_write(gb, dst + i + 2, val32 >> 16);
                // __gb_write(gb, dst + i + 3, val32 >> 24);
            }

            gb->cgb.dmaSource += count;
            gb->cgb.dmaDest += count;
            gb->cgb.dmaSize = 0;
#if WALNUT_GB_SAFE_DUALFETCH_DMA
						gb->prefetch_invalid=true;
#endif
        }
#elif WALNUT_GB_16BIT_DMA
        if (gb->cgb.cgbMode && !gb->cgb.dmaMode)
        {
            uint16_t src = gb->cgb.dmaSource & 0xFFF0;
            uint16_t dst = (gb->cgb.dmaDest & 0x1FF0) | 0x8000;
            uint16_t count = gb->cgb.dmaSize << 4;

            for (uint16_t i = 0; i < count; i += 2)
            {
                // 16-bit read from source
                uint16_t val16 = __gb_read16(gb, src + i);
								__gb_write16(gb, dst + i, val16);
                // // 8-bit writes to destination (two bytes)
                // __gb_write(gb, dst + i, val16 & 0xFF);
                // __gb_write(gb, dst + i + 1, val16 >> 8);
            }

            gb->cgb.dmaSource += count;
            gb->cgb.dmaDest += count;
            gb->cgb.dmaSize = 0;
#if WALNUT_GB_SAFE_DUALFETCH_DMA
						gb->prefetch_invalid=true;
#endif
        }
#else
				if(gb->cgb.cgbMode && (!gb->cgb.dmaMode))
				{
					for (int i = 0; i < (gb->cgb.dmaSize << 4); i++)
					{
						__gb_write(gb, ((gb->cgb.dmaDest & 0x1FF0) | 0x8000) + i, __gb_read(gb, (gb->cgb.dmaSource & 0xFFF0) + i));
					}
					gb->cgb.dmaSource += (gb->cgb.dmaSize << 4);
					gb->cgb.dmaDest += (gb->cgb.dmaSize << 4);
					gb->cgb.dmaSize = 0;
#if WALNUT_GB_SAFE_DUALFETCH_DMA
					gb->prefetch_invalid=true;
#endif
				}
#endif
			}
			gb->cgb.dmaActive = gb->cgb.dmaMode ^ 1;  // set active if it's an HBlank DMA
			return;

		/* IR Register*/
		case 0x56:
			gb->hram_io[0x56] = val;
			return;

		/* CGB BG Palette Index*/
		case 0x68:
			gb->cgb.BGPaletteID = val & 0x3F;
			gb->cgb.BGPaletteInc = val >> 7;
			return;

		/* CGB BG Palette*/
		case 0x69:
			// native rgb565 version
			if(gb->cgb.BGPalette[gb->cgb.BGPaletteID & 0x3F] != val)
				__gb_display_mark_dirty(gb);
	    gb->cgb.BGPalette[gb->cgb.BGPaletteID & 0x3F] = val;
#if WALNUT_GB_RGB565_BIGENDIAN
			gb->cgb.fixPalette[(gb->cgb.BGPaletteID & 0x3E) >> 1] = bgr555_to_rgb565BE_accurate((gb->cgb.BGPalette[(gb->cgb.BGPaletteID & 0x3E) + 1] << 8) | (gb->cgb.BGPalette[(gb->cgb.BGPaletteID & 0x3E)])); // convert native bgr 555 to rgb565 for native LCD panel rendering
#else
  	  gb->cgb.fixPalette[(gb->cgb.BGPaletteID & 0x3E) >> 1] = bgr555_to_rgb565_accurate((gb->cgb.BGPalette[(gb->cgb.BGPaletteID & 0x3E) + 1] << 8) | (gb->cgb.BGPalette[(gb->cgb.BGPaletteID & 0x3E)])); // convert native bgr 555 to rgb565 for native LCD panel rendering
#endif
			if(gb->direct.palette_changed)
				gb->direct.palette_changed(gb,
					(uint8_t)((gb->cgb.BGPaletteID & 0x3E) >> 1),
					gb->cgb.fixPalette[(gb->cgb.BGPaletteID & 0x3E) >> 1]);
			if(gb->cgb.BGPaletteInc) {
				gb->cgb.BGPaletteID++;
				gb->cgb.BGPaletteID = (gb->cgb.BGPaletteID) & 0x3F;
			}
    	return;

		/* CGB OAM Palette Index*/
		case 0x6A:
			gb->cgb.OAMPaletteID = val & 0x3F;
			gb->cgb.OAMPaletteInc = val >> 7;
			return;

		/* CGB OAM Palette*/
		case 0x6B:
			if(gb->cgb.OAMPalette[gb->cgb.OAMPaletteID & 0x3F] != val)
				__gb_display_mark_dirty(gb);
			gb->cgb.OAMPalette[(gb->cgb.OAMPaletteID & 0x3F)] = val;
#if WALNUT_GB_RGB565_BIGENDIAN
			gb->cgb.fixPalette[0x20 + ((gb->cgb.OAMPaletteID & 0x3E) >> 1)] = bgr555_to_rgb565BE_accurate((gb->cgb.OAMPalette[(gb->cgb.OAMPaletteID & 0x3E) + 1] << 8) + (gb->cgb.OAMPalette[(gb->cgb.OAMPaletteID & 0x3E)]));
#else
			gb->cgb.fixPalette[0x20 + ((gb->cgb.OAMPaletteID & 0x3E) >> 1)] = bgr555_to_rgb565_accurate((gb->cgb.OAMPalette[(gb->cgb.OAMPaletteID & 0x3E) + 1] << 8) + (gb->cgb.OAMPalette[(gb->cgb.OAMPaletteID & 0x3E)]));
#endif
			if(gb->direct.palette_changed)
				gb->direct.palette_changed(gb,
					(uint8_t)(0x20 + ((gb->cgb.OAMPaletteID & 0x3E) >> 1)),
					gb->cgb.fixPalette[0x20 + ((gb->cgb.OAMPaletteID & 0x3E) >> 1)]);
			if(gb->cgb.OAMPaletteInc) {
				gb->cgb.OAMPaletteID++;
				gb->cgb.OAMPaletteID = (gb->cgb.OAMPaletteID) & 0x3F;
			}
			return;

		/* CGB WRAM Bank*/
		case 0x70:
			gb->cgb.wramBank = val;
			gb->cgb.wramBankOffset = WRAM_1_ADDR - (1 << 12);
			if(gb->cgb.cgbMode && (gb->cgb.wramBank & 7) > 0) gb->cgb.wramBankOffset = WRAM_1_ADDR - ((gb->cgb.wramBank & 7) << 12);
			return;
#endif

		/* Interrupt Enable Register */
		case 0xFF:
			gb->hram_io[IO_IE] = val;
			return;
		}
	}

	/* Invalid writes are ignored. */
	return;
}

static WALNUT_GB_NOINLINE void __gb_stack_push16(struct gb_s *gb,
		uint16_t *sp, uint16_t val)
{
	const uint16_t addr = (uint16_t)(*sp - 2);
	uint8_t *dst = NULL;

	*sp = addr;
	if(WGB_LIKELY(addr >= WRAM_0_ADDR && addr <= 0xDFFE &&
			addr != 0xCFFF))
	{
#if WALNUT_FULL_GBC_SUPPORT
		dst = (addr >= WRAM_1_ADDR) ?
			&gb->wram[addr - gb->cgb.wramBankOffset] :
			&gb->wram[addr - WRAM_0_ADDR];
#else
		dst = &gb->wram[addr - WRAM_0_ADDR];
#endif
	}
	else if(WGB_LIKELY(addr >= ECHO_ADDR && addr <= 0xFDFE &&
			addr != 0xEFFF))
	{
#if WALNUT_FULL_GBC_SUPPORT
		dst = (addr >= 0xF000) ?
			&gb->wram[(addr - 0x2000) - gb->cgb.wramBankOffset] :
			&gb->wram[addr - ECHO_ADDR];
#else
		dst = &gb->wram[addr - ECHO_ADDR];
#endif
	}
	else if(WGB_LIKELY(addr >= HRAM_ADDR && addr <= 0xFFFD))
	{
		dst = &gb->hram_io[addr - IO_ADDR];
	}

	if(WGB_LIKELY(dst != NULL))
	{
		dst[0] = (uint8_t)val;
		dst[1] = (uint8_t)(val >> 8);
		return;
	}

	/* Keep byte order and side effects for uncommon boundary addresses. */
	__gb_write(gb, (uint16_t)(addr + 1), (uint8_t)(val >> 8));
	__gb_write(gb, addr, (uint8_t)val);
}

static WALNUT_GB_NOINLINE uint16_t __gb_stack_pop16(struct gb_s *gb,
		uint16_t *sp)
{
	const uint16_t addr = *sp;
	const uint8_t *src = NULL;

	*sp = (uint16_t)(addr + 2);
	if(WGB_LIKELY(addr >= WRAM_0_ADDR && addr <= 0xDFFE &&
			addr != 0xCFFF))
	{
#if WALNUT_FULL_GBC_SUPPORT
		src = (gb->cgb.cgbMode && addr >= WRAM_1_ADDR) ?
			&gb->wram[addr - gb->cgb.wramBankOffset] :
			&gb->wram[addr - WRAM_0_ADDR];
#else
		src = &gb->wram[addr - WRAM_0_ADDR];
#endif
	}
	else if(WGB_LIKELY(addr >= ECHO_ADDR && addr <= 0xFDFE &&
			addr != 0xEFFF))
	{
#if WALNUT_FULL_GBC_SUPPORT
		src = (addr >= 0xF000 && gb->cgb.cgbMode) ?
			&gb->wram[(addr - 0x2000) - gb->cgb.wramBankOffset] :
			&gb->wram[addr - ECHO_ADDR];
#else
		src = &gb->wram[addr - ECHO_ADDR];
#endif
	}
	else if(WGB_LIKELY(addr >= HRAM_ADDR && addr <= 0xFFFE))
	{
		src = &gb->hram_io[addr - IO_ADDR];
	}

	if(WGB_LIKELY(src != NULL))
	{
		const uint16_t val = (uint16_t)src[0] | ((uint16_t)src[1] << 8);
		return val;
	}

	const uint16_t val = (uint16_t)__gb_read(gb, addr) |
		((uint16_t)__gb_read(gb, (uint16_t)(addr + 1)) << 8);
	return val;
}

#if WALNUT_GB_32BIT_ALIGNED
void __gb_write32(struct gb_s *gb, uint16_t addr, uint32_t val) {
    uint8_t *dst = NULL;
    bool display_write = false;
    bool oam_write = false;

    switch (WALNUT_GB_GET_MSN16(addr)) {
        case 0x8: // VRAM
        case 0x9:
            display_write = true;
#if WALNUT_FULL_GBC_SUPPORT
            dst = &gb->vram[addr - gb->cgb.vramBankOffset];
#else
            dst = &gb->vram[addr - VRAM_ADDR];
#endif
            break;

        case 0xC: // WRAM bank 0
        case 0xD:
#if WALNUT_FULL_GBC_SUPPORT
            dst = &gb->wram[addr - gb->cgb.wramBankOffset];
#else
            dst = &gb->wram[addr - WRAM_1_ADDR + WRAM_BANK_SIZE];
#endif
            break;

        case 0xE: // Echo RAM
            dst = &gb->wram[addr - ECHO_ADDR];
            break;

        case 0xF: // HRAM / OAM
            if (addr < OAM_ADDR) {
#if WALNUT_FULL_GBC_SUPPORT
                dst = &gb->wram[(addr - 0x2000) - gb->cgb.wramBankOffset];
#else
                dst = &gb->wram[addr - ECHO_ADDR];
#endif
            } else if (addr < UNUSED_ADDR) {
                dst = &gb->oam[addr - OAM_ADDR];
                display_write = true;
                oam_write = true;
            }
            break;

        default:
            break;
    }

    if (dst) {
        if (((uintptr_t)dst & 3) == 0) {
            /* Aligned 32-bit store */
            *(uint32_t *)dst = val;
        } else {
            /* Byte-wise fallback. */
            dst[0] = (uint8_t)(val);
            dst[1] = (uint8_t)(val >> 8);
            dst[2] = (uint8_t)(val >> 16);
            dst[3] = (uint8_t)(val >> 24);
        }
        if(oam_write)
            __gb_oam_cache_invalidate(gb);
        if(display_write)
            __gb_display_mark_dirty(gb);
        return;
    }

    /* Preserve side effects on special addresses. */
    __gb_write(gb, addr + 0, (uint8_t)(val));
    __gb_write(gb, addr + 1, (uint8_t)(val >> 8));
    __gb_write(gb, addr + 2, (uint8_t)(val >> 16));
    __gb_write(gb, addr + 3, (uint8_t)(val >> 24));
}
#else
void __gb_write32(struct gb_s *gb, uint16_t addr, uint32_t val) {
    switch (WALNUT_GB_GET_MSN16(addr)) {
        case 0x8: // VRAM
        case 0x9:
            __gb_display_mark_dirty(gb);
#if WALNUT_FULL_GBC_SUPPORT
            *(uint32_t*)&gb->vram[addr - gb->cgb.vramBankOffset] = val;
#else
            *(uint32_t*)&gb->vram[addr - VRAM_ADDR] = val;
#endif
            return;

        case 0xC: // WRAM bank 0
        case 0xD:
#if WALNUT_FULL_GBC_SUPPORT
            *(uint32_t*)&gb->wram[addr - gb->cgb.wramBankOffset] = val;
#else
            *(uint32_t*)&gb->wram[addr - WRAM_1_ADDR + WRAM_BANK_SIZE] = val;
#endif
            return;

        case 0xE: // Echo RAM
            *(uint32_t*)&gb->wram[addr - ECHO_ADDR] = val;
            return;

        case 0xF: // HRAM / OAM
            if(addr < OAM_ADDR) {
#if WALNUT_FULL_GBC_SUPPORT
                *(uint32_t*)&gb->wram[(addr - 0x2000) - gb->cgb.wramBankOffset] = val;
#else
                *(uint32_t*)&gb->wram[addr - ECHO_ADDR] = val;
#endif
                return;
            }
            if(addr < UNUSED_ADDR) {
                *(uint32_t*)&gb->oam[addr - OAM_ADDR] = val;
                __gb_oam_cache_invalidate(gb);
                __gb_display_mark_dirty(gb);
                return;
            }
            break;

        default:
            break;
    }

    // Preserve side effects on special addresses.
    __gb_write(gb, addr, val & 0xFF);
    __gb_write(gb, addr + 1, (val >> 8) & 0xFF);
    __gb_write(gb, addr + 2, (val >> 16) & 0xFF);
    __gb_write(gb, addr + 3, (val >> 24) & 0xFF);
}
#endif

// DMA writes use the 16-bit path where direct access is safe.
void __gb_write16(struct gb_s *gb, uint_fast16_t addr, uint16_t val)
{
    switch(WALNUT_GB_GET_MSN16(addr))
    {
        case 0x8: // VRAM
        case 0x9:
            __gb_display_mark_dirty(gb);
#if WALNUT_FULL_GBC_SUPPORT
            *((uint16_t *)(gb->vram + (addr - gb->cgb.vramBankOffset))) = val;
#else
            *((uint16_t *)(gb->vram + (addr - VRAM_ADDR))) = val;
#endif
            return;

        case 0xA: // Cartridge RAM / RTC
        case 0xB:
            if(__gb_pocket_camera_regs_selected(gb))
            {
                __gb_pocket_camera_write(gb, addr, val & 0xFF);
                __gb_pocket_camera_write(gb, addr + 1, val >> 8);
            }
            else if(gb->mbc == 3 && gb->cart_ram_bank >= 0x08)
            {
                // RTC writes need __gb_write side effects.
                __gb_write(gb, addr, val & 0xFF);
                __gb_write(gb, addr + 1, val >> 8);
            }
            else if(gb->cart_ram && gb->enable_cart_ram)
            {
                gb->gb_cart_ram_write(gb, addr - CART_RAM_ADDR + (gb->cart_ram_bank * CRAM_BANK_SIZE), val & 0xFF);
                gb->gb_cart_ram_write(gb, addr - CART_RAM_ADDR + 1 + (gb->cart_ram_bank * CRAM_BANK_SIZE), val >> 8);
            }
            return;

        case 0xC: // WRAM bank 0
#if WALNUT_FULL_GBC_SUPPORT
        case 0xD: // WRAM bank 1..N
            *((uint16_t *)(gb->wram + (addr - gb->cgb.wramBankOffset))) = val;
#else
        case 0xD:
            *((uint16_t *)(gb->wram + (addr - WRAM_1_ADDR + WRAM_BANK_SIZE))) = val;
#endif
            return;

        case 0xE: // Echo RAM
            *((uint16_t *)(gb->wram + (addr - ECHO_ADDR))) = val;
            return;

        case 0xF:
            if(addr < OAM_ADDR) // Echo / WRAM mirrored
            {
#if WALNUT_FULL_GBC_SUPPORT
                *((uint16_t *)(gb->wram + (addr - 0x2000 - gb->cgb.wramBankOffset))) = val;
#else
                *((uint16_t *)(gb->wram + (addr - ECHO_ADDR))) = val;
#endif
            }
            else if(addr < UNUSED_ADDR || (addr >= HRAM_ADDR && addr < INTR_EN_ADDR))
            {
                // Use byte writes for special regions.
                if(addr < UNUSED_ADDR)
                    __gb_display_mark_dirty(gb);
                __gb_write(gb, addr, val & 0xFF);
                __gb_write(gb, addr + 1, val >> 8);
            }
            else
            {
                // IO, DMA, palette, and timer regions.
                __gb_write(gb, addr, val & 0xFF);
                __gb_write(gb, addr + 1, val >> 8);
            }
            return;

        default:
            // Byte-write fallback.
            __gb_write(gb, addr, val & 0xFF);
            __gb_write(gb, addr + 1, val >> 8);
            return;
    }
}

uint8_t __gb_execute_cb(struct gb_s *gb)
{
	uint8_t inst_cycles;
	uint8_t cbop = __gb_fetch8(gb);
	uint8_t r = (cbop & 0x7);
	uint8_t b = (cbop >> 3) & 0x7;
	uint8_t d = (cbop >> 3) & 0x1;
	uint8_t val;
	uint8_t writeback = 1;

	inst_cycles = 8;
	/* Add an additional 8 cycles to these sets of instructions. */
	switch(cbop & 0xC7)
	{
	case 0x06:
	case 0x86:
    	case 0xC6:
		inst_cycles += 8;
    	break;
    	case 0x46:
		inst_cycles += 4;
    	break;
	}

	switch(r)
	{
	case 0:
		val = gb->cpu_reg.bc.bytes.b;
		break;

	case 1:
		val = gb->cpu_reg.bc.bytes.c;
		break;

	case 2:
		val = gb->cpu_reg.de.bytes.d;
		break;

	case 3:
		val = gb->cpu_reg.de.bytes.e;
		break;

	case 4:
		val = gb->cpu_reg.hl.bytes.h;
		break;

	case 5:
		val = gb->cpu_reg.hl.bytes.l;
		break;

	case 6:
		val = __gb_read(gb, gb->cpu_reg.hl.reg);
		break;

	/* Only values 0-7 are possible here, so we make the final case
	 * default to satisfy -Wmaybe-uninitialized warning. */
	default:
		val = gb->cpu_reg.a;
		break;
	}

	switch(cbop >> 6)
	{
	case 0x0:
		cbop = (cbop >> 4) & 0x3;

		switch(cbop)
		{
		case 0x0: /* RdC R */
		case 0x1: /* Rd R */
			if(d) /* RRC R / RR R */
			{
				uint8_t temp = val;
				val = (val >> 1);
				val |= cbop ? (gb->cpu_reg.f.f_bits.c << 7) : (temp << 7);
				gb->cpu_reg.f.reg = 0;
				gb->cpu_reg.f.f_bits.z = (val == 0x00);
				gb->cpu_reg.f.f_bits.c = (temp & 0x01);
			}
			else /* RLC R / RL R */
			{
				uint8_t temp = val;
				val = (val << 1);
				val |= cbop ? gb->cpu_reg.f.f_bits.c : (temp >> 7);
				gb->cpu_reg.f.reg = 0;
				gb->cpu_reg.f.f_bits.z = (val == 0x00);
				gb->cpu_reg.f.f_bits.c = (temp >> 7);
			}

			break;

		case 0x2:
			if(d) /* SRA R */
			{
				gb->cpu_reg.f.reg = 0;
				gb->cpu_reg.f.f_bits.c = val & 0x01;
				val = (val >> 1) | (val & 0x80);
				gb->cpu_reg.f.f_bits.z = (val == 0x00);
			}
			else /* SLA R */
			{
				gb->cpu_reg.f.reg = 0;
				gb->cpu_reg.f.f_bits.c = (val >> 7);
				val = val << 1;
				gb->cpu_reg.f.f_bits.z = (val == 0x00);
			}

			break;

		case 0x3:
			if(d) /* SRL R */
			{
				gb->cpu_reg.f.reg = 0;
				gb->cpu_reg.f.f_bits.c = val & 0x01;
				val = val >> 1;
				gb->cpu_reg.f.f_bits.z = (val == 0x00);
			}
			else /* SWAP R */
			{
				uint8_t temp = (val >> 4) & 0x0F;
				temp |= (val << 4) & 0xF0;
				val = temp;
				gb->cpu_reg.f.reg = 0;
				gb->cpu_reg.f.f_bits.z = (val == 0x00);
			}

			break;
		}

		break;

	case 0x1: /* BIT B, R */
		gb->cpu_reg.f.f_bits.z = !((val >> b) & 0x1);
		gb->cpu_reg.f.f_bits.n = 0;
		gb->cpu_reg.f.f_bits.h = 1;
		writeback = 0;
		break;

	case 0x2: /* RES B, R */
		val &= (0xFE << b) | (0xFF >> (8 - b));
		break;

	case 0x3: /* SET B, R */
		val |= (0x1 << b);
		break;
	}

	if(writeback)
	{
		switch(r)
		{
		case 0:
			gb->cpu_reg.bc.bytes.b = val;
			break;

		case 1:
			gb->cpu_reg.bc.bytes.c = val;
			break;

		case 2:
			gb->cpu_reg.de.bytes.d = val;
			break;

		case 3:
			gb->cpu_reg.de.bytes.e = val;
			break;

		case 4:
			gb->cpu_reg.hl.bytes.h = val;
			break;

		case 5:
			gb->cpu_reg.hl.bytes.l = val;
			break;

		case 6:
			__gb_write(gb, gb->cpu_reg.hl.reg, val);
			break;

		case 7:
			gb->cpu_reg.a = val;
			break;
		}
	}
	return inst_cycles;
}

#if ENABLE_LCD
struct sprite_data {
	uint8_t sprite_number;
	uint8_t x;
};

#if WALNUT_GB_HIGH_LCD_ACCURACY
static int compare_sprites(const struct sprite_data *const sd1, const struct sprite_data *const sd2)
{
	int x_res;

	x_res = (int)sd1->x - (int)sd2->x;
	if(x_res != 0)
		return x_res;

	return (int)sd1->sprite_number - (int)sd2->sprite_number;
}
#endif

#if WALNUT_GB_OAM_CACHE
#if WALNUT_GB_HIGH_LCD_ACCURACY
static inline void __gb_oam_cache_add_line_sprite(struct gb_s *gb,
		uint8_t line, uint8_t sprite_number, uint8_t sprite_x,
		uint8_t cgbMode)
{
	uint8_t count = s_walnut_gb_oam_cache.line_count[line];
#if WALNUT_FULL_GBC_SUPPORT
	if(cgbMode)
	{
		if(count < MAX_SPRITES_LINE)
		{
			s_walnut_gb_oam_cache.line_sprites[line][count] = sprite_number;
			s_walnut_gb_oam_cache.line_count[line] = count + 1;
		}
		return;
	}
#else
	(void)cgbMode;
#endif

	struct sprite_data current;
	current.sprite_number = sprite_number;
	current.x = sprite_x;

	uint8_t place;
	for(place = count; place != 0; place--)
	{
		struct sprite_data prev;
		prev.sprite_number = s_walnut_gb_oam_cache.line_sprites[line][place - 1];
		prev.x = gb->oam[4 * prev.sprite_number + 1];
		if(compare_sprites(&prev, &current) < 0)
			break;
	}
	if(place >= MAX_SPRITES_LINE)
		return;

	for(uint8_t i = count; i > place; --i)
		s_walnut_gb_oam_cache.line_sprites[line][i] =
			s_walnut_gb_oam_cache.line_sprites[line][i - 1];

	if(count < MAX_SPRITES_LINE)
		s_walnut_gb_oam_cache.line_count[line] = count + 1;
	s_walnut_gb_oam_cache.line_sprites[line][place] = sprite_number;
}
#endif

static inline void __gb_oam_cache_build(struct gb_s *gb, uint8_t lcdc,
		uint8_t cgbMode)
{
	const uint8_t obj_size = (lcdc & LCDC_OBJ_SIZE) ? 1 : 0;
	const int sprite_height = obj_size ? 16 : 8;
	uint8_t sprite_number;

#if WALNUT_GB_HIGH_LCD_ACCURACY
	memset(s_walnut_gb_oam_cache.line_count, 0,
		sizeof(s_walnut_gb_oam_cache.line_count));
#else
	memset(s_walnut_gb_oam_cache.line_mask_lo, 0,
		sizeof(s_walnut_gb_oam_cache.line_mask_lo));
	memset(s_walnut_gb_oam_cache.line_mask_hi, 0,
		sizeof(s_walnut_gb_oam_cache.line_mask_hi));
#endif

	for(sprite_number = 0; sprite_number < NUM_SPRITES; sprite_number++)
	{
		const uint8_t OY = gb->oam[4 * sprite_number + 0];
		const uint8_t OX = gb->oam[4 * sprite_number + 1];
		int line_start = (int)OY - 16;
		int line_end = line_start + sprite_height;

		if(line_end <= 0 || line_start >= LCD_HEIGHT)
			continue;
		if(line_start < 0)
			line_start = 0;
		if(line_end > LCD_HEIGHT)
			line_end = LCD_HEIGHT;

		for(int line = line_start; line < line_end; ++line)
		{
#if WALNUT_GB_HIGH_LCD_ACCURACY
			__gb_oam_cache_add_line_sprite(gb, (uint8_t)line,
				sprite_number, OX, cgbMode);
#else
			(void)OX;
			if(sprite_number < 32)
				s_walnut_gb_oam_cache.line_mask_lo[line] |=
					(1UL << sprite_number);
			else
				s_walnut_gb_oam_cache.line_mask_hi[line] |=
					(uint8_t)(1U << (sprite_number - 32));
#endif
		}
	}

	s_walnut_gb_oam_cache.owner = gb;
	s_walnut_gb_oam_cache.obj_size = obj_size;
#if WALNUT_FULL_GBC_SUPPORT
	s_walnut_gb_oam_cache.cgb_mode = cgbMode;
#else
	(void)cgbMode;
#endif
	s_walnut_gb_oam_cache.valid = 1;
}

static inline void __gb_oam_cache_ensure(struct gb_s *gb, uint8_t lcdc,
		uint8_t cgbMode)
{
	const uint8_t obj_size = (lcdc & LCDC_OBJ_SIZE) ? 1 : 0;
	if(!s_walnut_gb_oam_cache.valid ||
			s_walnut_gb_oam_cache.owner != gb ||
			s_walnut_gb_oam_cache.obj_size != obj_size
#if WALNUT_FULL_GBC_SUPPORT
			|| s_walnut_gb_oam_cache.cgb_mode != cgbMode
#endif
			)
	{
		__gb_oam_cache_build(gb, lcdc, cgbMode);
	}
}

#if !WALNUT_GB_HIGH_LCD_ACCURACY
static inline uint8_t __gb_oam_cache_pop_highest32(uint32_t *mask)
{
	const uint32_t value = *mask;
#if defined(__GNUC__)
	const uint8_t bit = (uint8_t)(31 - __builtin_clz(value));
#else
	uint8_t bit = 31;
	while(((value >> bit) & 1U) == 0)
		--bit;
#endif
	*mask = value & ~(1UL << bit);
	return bit;
}
#endif
#endif

static inline uint8_t __gb_ppu_vram_read(struct gb_s *gb, uint16_t addr)
{
	const uint16_t mask = WGB_GB_GLITCH_PPU_MASK();
	return gb->vram[mask ? (uint16_t)(addr & (uint16_t)~mask) : addr];
}

static inline uint8_t __gb_ppu_bg_vram_read(struct gb_s *gb, uint16_t addr)
{
	const uint16_t mask = WGB_GB_GLITCH_BG_MASK();
	return gb->vram[mask ? (uint16_t)(addr & (uint16_t)~mask) : addr];
}

#if WALNUT_FULL_GBC_SUPPORT
static const uint32_t __gb_expand_nibble_msb[16] = {
	0x00000000u, 0x01000000u, 0x00010000u, 0x01010000u,
	0x00000100u, 0x01000100u, 0x00010100u, 0x01010100u,
	0x00000001u, 0x01000001u, 0x00010001u, 0x01010001u,
	0x00000101u, 0x01000101u, 0x00010101u, 0x01010101u
};

static const uint32_t __gb_expand_nibble_lsb[16] = {
	0x00000000u, 0x00000001u, 0x00000100u, 0x00000101u,
	0x00010000u, 0x00010001u, 0x00010100u, 0x00010101u,
	0x01000000u, 0x01000001u, 0x01000100u, 0x01000101u,
	0x01010000u, 0x01010001u, 0x01010100u, 0x01010101u
};

static inline void __gb_cgb_emit_full_bg_tile(uint8_t *pixels,
		uint8_t *pixelsPrio, uint8_t plane0, uint8_t plane1, uint8_t idxAtt)
{
	const uint32_t palette = (uint32_t)((idxAtt & 0x07) << 2) * 0x01010101u;
	const uint32_t prio = (uint32_t)(idxAtt >> 7) * 0x01010101u;
	uint32_t first;
	uint32_t second;

	if(idxAtt & 0x20)
	{
		first = __gb_expand_nibble_lsb[plane0 & 0x0F]
			| (__gb_expand_nibble_lsb[plane1 & 0x0F] << 1);
		second = __gb_expand_nibble_lsb[plane0 >> 4]
			| (__gb_expand_nibble_lsb[plane1 >> 4] << 1);
	}
	else
	{
		first = __gb_expand_nibble_msb[plane0 >> 4]
			| (__gb_expand_nibble_msb[plane1 >> 4] << 1);
		second = __gb_expand_nibble_msb[plane0 & 0x0F]
			| (__gb_expand_nibble_msb[plane1 & 0x0F] << 1);
	}

	first |= palette;
	second |= palette;
	memcpy(pixels, &first, sizeof(first));
	memcpy(pixels + 4, &second, sizeof(second));
	memcpy(pixelsPrio, &prio, sizeof(prio));
	memcpy(pixelsPrio + 4, &prio, sizeof(prio));
}

static inline void __gb_cgb_emit_bg_chunk(uint8_t *pixels, uint8_t *pixelsPrio,
		uint8_t plane0, uint8_t plane1, uint8_t idxAtt,
		uint8_t start_px, uint8_t count)
{
	const uint8_t palette = (uint8_t)((idxAtt & 0x07) << 2);
	const uint8_t prio = (uint8_t)(idxAtt >> 7);
	uint8_t i;

	if(((plane0 == 0x00) || (plane0 == 0xFF)) &&
			((plane1 == 0x00) || (plane1 == 0xFF)))
	{
		const uint8_t c = (uint8_t)((plane0 ? 0x01 : 0x00) |
				(plane1 ? 0x02 : 0x00));
		memset(pixels, (uint8_t)(palette + c), count);
		memset(pixelsPrio, prio, count);
		return;
	}

	if(count == 8 && start_px == 0)
	{
		__gb_cgb_emit_full_bg_tile(pixels, pixelsPrio, plane0, plane1, idxAtt);
		return;
	}

	if(idxAtt & 0x20)
	{
		for(i = 0; i < count; ++i)
		{
			const uint8_t bit = (uint8_t)(start_px + i);
			const uint8_t c =
				(uint8_t)(((plane0 >> bit) & 0x01) |
				(((plane1 >> bit) & 0x01) << 1));
			pixels[i] = (uint8_t)(palette + c);
			pixelsPrio[i] = prio;
		}
	}
	else
	{
		for(i = 0; i < count; ++i)
		{
			const uint8_t bit = (uint8_t)(7 - (start_px + i));
			const uint8_t c =
				(uint8_t)(((plane0 >> bit) & 0x01) |
				(((plane1 >> bit) & 0x01) << 1));
			pixels[i] = (uint8_t)(palette + c);
			pixelsPrio[i] = prio;
		}
	}
}
#endif

void __gb_draw_line(struct gb_s *gb)
{
	uint8_t pixels[160];
	const uint8_t hram_io_ly = gb->hram_io[IO_LY];
#if WALNUT_FULL_GBC_SUPPORT
	const uint8_t cgbMode = gb->cgb.cgbMode;
#endif
	const uint8_t lcdc = gb->hram_io[IO_LCDC];
	/* If LCD not initialised by front-end, don't render anything. */
	if(gb->display.lcd_draw_line == NULL)
		return;

	if(gb->direct.frame_skip && !gb->display.frame_skip_count)
	{
		return;
	}

#if WALNUT_GB_DIRTY_FRAME_SKIP
	if(hram_io_ly == 0)
	{
		gb->display.frame_dirty_active = gb->display.frame_dirty;
		gb->display.frame_dirty = false;
	}
	if(!gb->display.frame_dirty_active)
	{
		return;
	}
#endif

#if WALNUT_FULL_GBC_SUPPORT
	uint8_t pixelsPrio[160];  //do these pixels have priority over OAM?
	if(!cgbMode && !(lcdc & LCDC_BG_ENABLE))
		memset(pixels, 0, sizeof(pixels));
#else
	if(!(lcdc & LCDC_BG_ENABLE))
		memset(pixels, 0, sizeof(pixels));
#endif
	/* If interlaced mode is activated, check if we need to draw the current
	 * line. */
	if(gb->direct.interlace)
	{
			if((!gb->display.interlace_count
				&& (hram_io_ly & 1) == 0)
				|| (gb->display.interlace_count
				    && (hram_io_ly & 1) == 1))
		{
			/* Compensate for missing window draw if required. */
			if(lcdc & LCDC_WINDOW_ENABLE
					&& hram_io_ly >= gb->display.WY
					&& gb->hram_io[IO_WX] <= 166)
				gb->display.window_clear++;

			return;
		}
	}

	/* If background is enabled, draw it. */
#if WALNUT_FULL_GBC_SUPPORT
	if(cgbMode || lcdc & LCDC_BG_ENABLE)
#else
	if(lcdc & LCDC_BG_ENABLE)
#endif
	{
		uint8_t bg_y;
		uint16_t bg_map;

		/* Calculate current background line to draw. Constant because
		 * this function draws only this one line each time it is
		 * called. */
		bg_y = hram_io_ly + gb->hram_io[IO_SCY];

		/* Get selected background map address for first tile
		 * corresponding to current line.
		 * 0x20 (32) is the width of a background tile, and the bit
		 * shift is to calculate the address. */
		bg_map =
			((lcdc & LCDC_BG_MAP) ?
			 VRAM_BMAP_2 : VRAM_BMAP_1)
			+ (bg_y >> 3) * 0x20;

#if WALNUT_FULL_GBC_SUPPORT
		if(cgbMode)
		{
			const uint8_t py = (uint8_t)(bg_y & 0x07);
			uint8_t disp_x = 0;

			while(disp_x < LCD_WIDTH)
			{
				const uint8_t bg_x = (uint8_t)(disp_x + gb->hram_io[IO_SCX]);
				const uint16_t map_addr = (uint16_t)(bg_map + (bg_x >> 3));
				const uint8_t idx = __gb_ppu_bg_vram_read(gb, map_addr);
				const uint8_t idxAtt = __gb_ppu_bg_vram_read(gb, map_addr + 0x2000);
				uint16_t tile;
				uint8_t tile_py = py;
				uint8_t chunk = (uint8_t)(8 - (bg_x & 0x07));
				uint8_t remaining = (uint8_t)(LCD_WIDTH - disp_x);
				uint8_t t1, t2;

				if(chunk > remaining)
					chunk = remaining;

				if(lcdc & LCDC_TILE_SELECT)
					tile = VRAM_TILES_1 + idx * 0x10;
				else
					tile = VRAM_TILES_2 + ((idx + 0x80) % 0x100) * 0x10;

				if(idxAtt & 0x08)
					tile += 0x2000;
				if(idxAtt & 0x40)
					tile_py = (uint8_t)(7 - tile_py);

				tile += 2 * tile_py;
				t1 = __gb_ppu_bg_vram_read(gb, tile);
				t2 = __gb_ppu_bg_vram_read(gb, tile + 1);

				__gb_cgb_emit_bg_chunk(&pixels[disp_x], &pixelsPrio[disp_x],
					t1, t2, idxAtt, (uint8_t)(bg_x & 0x07), chunk);
				disp_x += chunk;
			}
		}
		else
#endif
		{
			uint8_t disp_x, bg_x, idx, py, px, t1, t2;
			uint16_t tile;

			/* The displays (what the player sees) X coordinate, drawn right
			 * to left. */
			disp_x = LCD_WIDTH - 1;

			/* The X coordinate to begin drawing the background at. */
			bg_x = disp_x + gb->hram_io[IO_SCX];

			/* Get tile index for current background tile. */
			idx = __gb_ppu_bg_vram_read(gb, bg_map + (bg_x >> 3));
			/* Y coordinate of tile pixel to draw. */
			py = (bg_y & 0x07);
			/* X coordinate of tile pixel to draw. */
			px = 7 - (bg_x & 0x07);

			/* Select addressing mode. */
			if(lcdc & LCDC_TILE_SELECT)
				tile = VRAM_TILES_1 + idx * 0x10;
			else
				tile = VRAM_TILES_2 + ((idx + 0x80) % 0x100) * 0x10;

			tile += 2 * py;

			/* fetch first tile */
			t1 = __gb_ppu_bg_vram_read(gb, tile) >> px;
			t2 = __gb_ppu_bg_vram_read(gb, tile + 1) >> px;

			for(; disp_x != 0xFF; disp_x--)
			{
				uint8_t c;

				if(px == 8)
				{
					/* fetch next tile */
					px = 0;
					bg_x = disp_x + gb->hram_io[IO_SCX];
					idx = __gb_ppu_bg_vram_read(gb, bg_map + (bg_x >> 3));
					if(lcdc & LCDC_TILE_SELECT)
						tile = VRAM_TILES_1 + idx * 0x10;
					else
						tile = VRAM_TILES_2 + ((idx + 0x80) % 0x100) * 0x10;
					tile += 2 * py;
					t1 = __gb_ppu_bg_vram_read(gb, tile);
					t2 = __gb_ppu_bg_vram_read(gb, tile + 1);
				}

				c = (t1 & 0x1) | ((t2 & 0x1) << 1);
				pixels[disp_x] = gb->display.bg_palette[c];
#if WALNUT_GB_12_COLOUR
				pixels[disp_x] |= LCD_PALETTE_BG;
#endif
				t1 = t1 >> 1;
				t2 = t2 >> 1;
				px++;
			}
		}
	}

	/* draw window */
	if(lcdc & LCDC_WINDOW_ENABLE
			&& hram_io_ly >= gb->display.WY
			&& gb->hram_io[IO_WX] <= 166)
	{
		uint8_t window_drawn_fast = 0;
#if WALNUT_FULL_GBC_SUPPORT
		if(cgbMode)
		{
			uint16_t win_line;
			uint8_t disp_x;
			const uint8_t wx = gb->hram_io[IO_WX];
			const uint8_t py = gb->display.window_clear & 0x07;

			win_line = (lcdc & LCDC_WINDOW_MAP) ?
				    VRAM_BMAP_2 : VRAM_BMAP_1;
			win_line += (gb->display.window_clear >> 3) * 0x20;

			disp_x = (wx < 7) ? 0 : (uint8_t)(wx - 7);
			while(disp_x < LCD_WIDTH)
			{
				const uint8_t win_x = (uint8_t)(disp_x - wx + 7);
				const uint16_t map_addr = (uint16_t)(win_line + (win_x >> 3));
				const uint8_t idx = __gb_ppu_bg_vram_read(gb, map_addr);
				const uint8_t idxAtt = __gb_ppu_bg_vram_read(gb, map_addr + 0x2000);
				uint8_t tile_py = py;
				uint8_t chunk = (uint8_t)(8 - (win_x & 0x07));
				uint8_t remaining = (uint8_t)(LCD_WIDTH - disp_x);
				uint16_t tile;
				uint8_t t1, t2;

				if(chunk > remaining)
					chunk = remaining;

				if(lcdc & LCDC_TILE_SELECT)
					tile = VRAM_TILES_1 + idx * 0x10;
				else
					tile = VRAM_TILES_2 + ((idx + 0x80) % 0x100) * 0x10;

				if(idxAtt & 0x08)
					tile += 0x2000;
				if(idxAtt & 0x40)
					tile_py = (uint8_t)(7 - tile_py);

				tile += 2 * tile_py;
				t1 = __gb_ppu_bg_vram_read(gb, tile);
				t2 = __gb_ppu_bg_vram_read(gb, tile + 1);

				__gb_cgb_emit_bg_chunk(&pixels[disp_x], &pixelsPrio[disp_x],
					t1, t2, idxAtt, (uint8_t)(win_x & 0x07), chunk);
				disp_x = (uint8_t)(disp_x + chunk);
			}

			gb->display.window_clear++;
			window_drawn_fast = 1;
		}
#endif
		if(!window_drawn_fast)
		{
		uint16_t win_line, tile;
		uint8_t disp_x, win_x, py, px, idx, t1, t2, end;

		/* Calculate Window Map Address. */
		win_line = (lcdc & LCDC_WINDOW_MAP) ?
				    VRAM_BMAP_2 : VRAM_BMAP_1;
		win_line += (gb->display.window_clear >> 3) * 0x20;

		disp_x = LCD_WIDTH - 1;
		win_x = disp_x - gb->hram_io[IO_WX] + 7;

		// look up tile
		py = gb->display.window_clear & 0x07;
		px = 7 - (win_x & 0x07);
		idx = __gb_ppu_bg_vram_read(gb, win_line + (win_x >> 3));
#if WALNUT_FULL_GBC_SUPPORT
		uint8_t idxAtt = __gb_ppu_bg_vram_read(gb, win_line + (win_x >> 3) + 0x2000);
#endif

		if(lcdc & LCDC_TILE_SELECT)
			tile = VRAM_TILES_1 + idx * 0x10;
		else
			tile = VRAM_TILES_2 + ((idx + 0x80) % 0x100) * 0x10;

#if WALNUT_FULL_GBC_SUPPORT
		if(cgbMode)
		{
			if(idxAtt & 0x08) tile += 0x2000; //VRAM bank 2
			if(idxAtt & 0x40) tile += 2 * (7 - py);
		}
		if(!(idxAtt & 0x40))
		{
			tile += 2 * py;
		}

		// fetch first tile
		if(cgbMode && (idxAtt & 0x20))
		{  //Horizantal Flip
			t1 = __gb_ppu_bg_vram_read(gb, tile) << px;
			t2 = __gb_ppu_bg_vram_read(gb, tile + 1) << px;
		}
		else
		{
			t1 = __gb_ppu_bg_vram_read(gb, tile) >> px;
			t2 = __gb_ppu_bg_vram_read(gb, tile + 1) >> px;
		}
#else
		tile += 2 * py;

		// fetch first tile
		t1 = __gb_ppu_bg_vram_read(gb, tile) >> px;
		t2 = __gb_ppu_bg_vram_read(gb, tile + 1) >> px;
#endif
		// loop & copy window
		end = (gb->hram_io[IO_WX] < 7 ? 0 : gb->hram_io[IO_WX] - 7) - 1;

		for(; disp_x != end; disp_x--)
		{
			uint8_t c;

			if(px == 8)
			{
				// fetch next tile
				px = 0;
				win_x = disp_x - gb->hram_io[IO_WX] + 7;
				idx = __gb_ppu_bg_vram_read(gb, win_line + (win_x >> 3));
#if WALNUT_FULL_GBC_SUPPORT
				idxAtt = __gb_ppu_bg_vram_read(gb, win_line + (win_x >> 3) + 0x2000);
#endif

				if(lcdc & LCDC_TILE_SELECT)
					tile = VRAM_TILES_1 + idx * 0x10;
				else
					tile = VRAM_TILES_2 + ((idx + 0x80) % 0x100) * 0x10;

#if WALNUT_FULL_GBC_SUPPORT
				if(cgbMode)
				{
					if(idxAtt & 0x08) tile += 0x2000; //VRAM bank 2
					if(idxAtt & 0x40) tile += 2 * (7 - py);
				}
				if(!(idxAtt & 0x40))
				{
					tile += 2 * py;
				}
#else
				tile += 2 * py;
#endif
				t1 = __gb_ppu_bg_vram_read(gb, tile);
				t2 = __gb_ppu_bg_vram_read(gb, tile + 1);
			}

			// copy window
#if WALNUT_FULL_GBC_SUPPORT
			if(idxAtt & 0x20)
			{  //Horizantal Flip
				c = (((t1 & 0x80) >> 1) | (t2 & 0x80)) >> 6;
				pixels[disp_x] = ((idxAtt & 0x07) << 2) + c;
				pixelsPrio[disp_x] = (idxAtt >> 7);
				t1 = t1 << 1;
				t2 = t2 << 1;
			}
			else
			{
				c = (t1 & 0x1) | ((t2 & 0x1) << 1);
				if(cgbMode)
				{
					pixels[disp_x] = ((idxAtt & 0x07) << 2) + c;
					pixelsPrio[disp_x] = (idxAtt >> 7);
				}
				else
				{
					pixels[disp_x] = gb->display.bg_palette[c];
#if WALNUT_GB_12_COLOUR
					pixels[disp_x] |= LCD_PALETTE_BG;
#endif
				}
				t1 = t1 >> 1;
				t2 = t2 >> 1;
			}
#else
			c = (t1 & 0x1) | ((t2 & 0x1) << 1);
			pixels[disp_x] = gb->display.bg_palette[c];
#if WALNUT_GB_12_COLOUR
			pixels[disp_x] |= LCD_PALETTE_BG;
#endif
			t1 = t1 >> 1;
			t2 = t2 >> 1;
#endif
			px++;
		}

		gb->display.window_clear++; // advance window line
		}
	}

	// draw sprites
	if(lcdc & LCDC_OBJ_ENABLE)
	{
		uint8_t sprite_number;
#if WALNUT_GB_OAM_CACHE
		__gb_oam_cache_ensure(gb, lcdc,
#if WALNUT_FULL_GBC_SUPPORT
			cgbMode
#else
			0
#endif
			);
#if WALNUT_GB_HIGH_LCD_ACCURACY
		uint8_t cached_sprite_index =
			s_walnut_gb_oam_cache.line_count[hram_io_ly];
#else
		uint32_t cached_sprite_mask_lo =
			s_walnut_gb_oam_cache.line_mask_lo[hram_io_ly];
		uint32_t cached_sprite_mask_hi =
			s_walnut_gb_oam_cache.line_mask_hi[hram_io_ly];
#endif
#elif WALNUT_GB_HIGH_LCD_ACCURACY
		uint8_t number_of_sprites = 0;

		struct sprite_data sprites_to_render[MAX_SPRITES_LINE + 1]; // Requires one extra slot for sorting

		/* Record number of sprites on the line being rendered, limited
		 * to the maximum number sprites that the Game Boy is able to
		 * render on each line (10 sprites). */
		for(sprite_number = 0;
				sprite_number < NUM_SPRITES;
				sprite_number++)
		{
			/* Sprite Y position. */
			uint8_t OY = gb->oam[4 * sprite_number + 0];
			/* Sprite X position. */
			uint8_t OX = gb->oam[4 * sprite_number + 1];

			/* If sprite isn't on this line, continue. */
			if(hram_io_ly +
				(lcdc & LCDC_OBJ_SIZE ? 0 : 8) >= OY
					|| hram_io_ly + 16 < OY)
				continue;

#if WALNUT_FULL_GBC_SUPPORT
			if (!cgbMode)
			{
#endif
			struct sprite_data current;

			current.sprite_number = sprite_number;
			current.x = OX;

			uint8_t place;
			for (place = number_of_sprites; place != 0; place--)
			{
				if(compare_sprites(&sprites_to_render[place - 1], &current) < 0)
					break;
			}
			if(place >= MAX_SPRITES_LINE)
				continue;
			for (uint8_t i = number_of_sprites; i > place; --i) {
				sprites_to_render[i] = sprites_to_render[i - 1];
			}
			if(number_of_sprites < MAX_SPRITES_LINE)
				number_of_sprites++;
			sprites_to_render[place] = current;
#if WALNUT_FULL_GBC_SUPPORT
			}
			else
			{
				// CGB does not care about the X coordinate of the sprite when it comes to render priority, only the OAM order.
				// Skip the reordering and just fill sprites_to_render until it is full.
				if (number_of_sprites >= MAX_SPRITES_LINE) continue;
				sprites_to_render[number_of_sprites].sprite_number = sprite_number;
				sprites_to_render[number_of_sprites].x = OX;
				number_of_sprites++;
			}
#endif
		}
#endif

		/* Render each sprite, from low priority to high priority. */
#if WALNUT_GB_OAM_CACHE
		for(;;)
		{
			uint8_t s;
#if WALNUT_GB_HIGH_LCD_ACCURACY
			if(cached_sprite_index == 0)
				break;
			s = s_walnut_gb_oam_cache.line_sprites[hram_io_ly]
				[--cached_sprite_index];
#else
			if(cached_sprite_mask_hi)
				s = (uint8_t)(32 +
					__gb_oam_cache_pop_highest32(&cached_sprite_mask_hi));
			else if(cached_sprite_mask_lo)
				s = __gb_oam_cache_pop_highest32(&cached_sprite_mask_lo);
			else
				break;
#endif
#elif WALNUT_GB_HIGH_LCD_ACCURACY
		/* Render the top ten prioritised sprites on this scanline. */
		for(sprite_number = number_of_sprites - 1;
				sprite_number != 0xFF;
				sprite_number--)
		{
			uint8_t s = sprites_to_render[sprite_number].sprite_number;
#else
		for (sprite_number = NUM_SPRITES - 1;
			sprite_number != 0xFF;
			sprite_number--)
		{
			uint8_t s = sprite_number;
#endif
			uint8_t py, t1, t2, dir, start, end, shift, disp_x;
			/* Sprite Y position. */
			uint8_t OY = gb->oam[4 * s + 0];
			/* Sprite X position. */
			uint8_t OX = gb->oam[4 * s + 1];
			/* Sprite Tile/Pattern Number. */
			uint8_t OT = gb->oam[4 * s + 2]
				     & (lcdc & LCDC_OBJ_SIZE ? 0xFE : 0xFF);
			/* Additional attributes. */
			uint8_t OF = gb->oam[4 * s + 3];

#if !WALNUT_GB_HIGH_LCD_ACCURACY && !WALNUT_GB_OAM_CACHE
			/* If sprite isn't on this line, continue. */
			if(hram_io_ly +
					(lcdc & LCDC_OBJ_SIZE ? 0 : 8) >= OY ||
					hram_io_ly + 16 < OY)
				continue;
#endif

			/* Continue if sprite not visible. */
			if(OX == 0 || OX >= 168)
				continue;

			// y flip
			py = hram_io_ly - OY + 16;

			if(OF & OBJ_FLIP_Y)
				py = (lcdc & LCDC_OBJ_SIZE ? 15 : 7) - py;

			// fetch the tile
#if WALNUT_FULL_GBC_SUPPORT
			if(cgbMode)
			{
				t1 = __gb_ppu_vram_read(gb, ((OF & OBJ_BANK) << 10) + VRAM_TILES_1 + OT * 0x10 + 2 * py);
				t2 = __gb_ppu_vram_read(gb, ((OF & OBJ_BANK) << 10) + VRAM_TILES_1 + OT * 0x10 + 2 * py + 1);
			}
			else
#endif
			{
				t1 = __gb_ppu_vram_read(gb, VRAM_TILES_1 + OT * 0x10 + 2 * py);
				t2 = __gb_ppu_vram_read(gb, VRAM_TILES_1 + OT * 0x10 + 2 * py + 1);
			}

			// handle x flip
			if(OF & OBJ_FLIP_X)
			{
				dir = 1;
				start = (OX < 8 ? 0 : OX - 8);
				end = MIN(OX, LCD_WIDTH);
				shift = 8 - OX + start;
			}
			else
			{
				dir = (uint8_t)-1;
				start = MIN(OX, LCD_WIDTH) - 1;
				end = (OX < 8 ? 0 : OX - 8) - 1;
				shift = OX - (start + 1);
			}

			// copy tile
			t1 >>= shift;
			t2 >>= shift;

			/* TODO: Put for loop within the to if statements
			 * because the BG priority bit will be the same for
			 * all the pixels in the tile. */
			for(disp_x = start; disp_x != end; disp_x += dir)
			{
				uint8_t c = (t1 & 0x1) | ((t2 & 0x1) << 1);
				// check transparency / sprite overlap / background overlap
#if WALNUT_FULL_GBC_SUPPORT
				if(cgbMode)
				{
					uint8_t isBackgroundDisabled = c && !(lcdc & LCDC_BG_ENABLE);
					uint8_t isPixelPriorityNonConflicting = c &&
															!(pixelsPrio[disp_x] && (pixels[disp_x] & 0x3)) &&
															!((OF & OBJ_PRIORITY) && (pixels[disp_x] & 0x3));

					if(isBackgroundDisabled || isPixelPriorityNonConflicting)
					{
						/* Set pixel colour. */
						pixels[disp_x] = ((OF & OBJ_CGB_PALETTE) << 2) + c + 0x20;  // add 0x20 to differentiate from BG
					}
				}
				else
#endif
				if(c && !(OF & OBJ_PRIORITY && !((pixels[disp_x] & 0x3) == gb->display.bg_palette[0])))
				{
					/* Set pixel colour. */
					pixels[disp_x] = (OF & OBJ_PALETTE)
						? gb->display.sp_palette[c + 4]
						: gb->display.sp_palette[c];
#if WALNUT_GB_12_COLOUR
					/* Set pixel palette (OBJ0 or OBJ1). */
					pixels[disp_x] |= (OF & OBJ_PALETTE);
#endif
#if WALNUT_FULL_GBC_SUPPORT
					/* Deselect BG palette. */
					pixels[disp_x] &= ~LCD_PALETTE_BG;
#endif
				}
				t1 = t1 >> 1;
				t2 = t2 >> 1;
			}
		}
	}

	gb->display.lcd_draw_line(gb, pixels, hram_io_ly);
}
#endif

static inline uint_fast16_t __gb_lcd_next_event_target(const struct gb_s *gb)
{
	switch(gb->hram_io[IO_STAT] & STAT_MODE)
	{
	case IO_STAT_MODE_OAM_SCAN:
		return LCD_MODE2_OAM_SCAN_END;
	case IO_STAT_MODE_LCD_DRAW:
		return LCD_MODE3_LCD_DRAW_END;
	default:
		return LCD_LINE_CYCLES;
	}
}

static inline uint_fast32_t __gb_lcd_next_event_delta(const struct gb_s *gb)
{
	if(WGB_UNLIKELY(!(gb->hram_io[IO_LCDC] & LCDC_ENABLE)))
	{
		if(gb->counter.lcd_off_count < LCD_FRAME_CYCLES)
			return LCD_FRAME_CYCLES - gb->counter.lcd_off_count;
		return 1;
	}

	const uint_fast16_t next_event = __gb_lcd_next_event_target(gb);
	if(gb->counter.lcd_count < next_event)
		return next_event - gb->counter.lcd_count;
	return 1;
}

static inline void __gb_lcd_reschedule(struct gb_s *gb)
{
	gb->counter.lcd_event_count = __gb_lcd_next_event_delta(gb);
	gb->counter.cpu_event_count = 0;
	gb->counter.cpu_event_flags = 0;
}

static inline void __gb_cpu_timing_reschedule(struct gb_s *gb)
{
	gb->counter.cpu_event_count = 0;
	gb->counter.cpu_event_flags = 0;
}

static inline bool __gb_post_lcd_step(struct gb_s *gb,
		uint_fast16_t *inst_cycles_io, bool invalidate_prefetch_on_hdma)
{
	uint_fast16_t inst_cycles = *inst_cycles_io;

	/* If LCD is off, don't update LCD state or increase the LCD ticks. */
	if(WGB_UNLIKELY(!(gb->hram_io[IO_LCDC] & LCDC_ENABLE)))
	{
		uint_fast32_t lcd_event_count = gb->counter.lcd_event_count;
		if(WGB_UNLIKELY(lcd_event_count == 0))
			lcd_event_count = __gb_lcd_next_event_delta(gb);

		gb->counter.lcd_off_count += inst_cycles;
		if(WGB_LIKELY(lcd_event_count > inst_cycles))
		{
			gb->counter.lcd_event_count = lcd_event_count - inst_cycles;
			return false;
		}
		gb->counter.lcd_off_count -= LCD_FRAME_CYCLES;
		gb->gb_frame = true;
		__gb_lcd_reschedule(gb);
		return false;
	}

	/* LCD Timing */
#if WALNUT_FULL_GBC_SUPPORT
	const uint_fast16_t lcd_cycles = (inst_cycles >> gb->cgb.doubleSpeed);
#else
	const uint_fast16_t lcd_cycles = inst_cycles;
#endif
	uint_fast32_t lcd_event_count = gb->counter.lcd_event_count;
	if(WGB_UNLIKELY(lcd_event_count == 0))
		lcd_event_count = __gb_lcd_next_event_delta(gb);

	gb->counter.lcd_count += lcd_cycles;
	if(WGB_LIKELY(lcd_event_count > lcd_cycles))
	{
		gb->counter.lcd_event_count = lcd_event_count - lcd_cycles;
		return false;
	}

	/* New Scanline. HBlank -> VBlank or OAM Scan */
	if(gb->counter.lcd_count >= LCD_LINE_CYCLES)
	{
		gb->counter.lcd_count -= LCD_LINE_CYCLES;

		/* Next line */
		gb->hram_io[IO_LY] = gb->hram_io[IO_LY] + 1;
		if (gb->hram_io[IO_LY] == LCD_VERT_LINES)
			gb->hram_io[IO_LY] = 0;

		/* LYC Update */
		if(gb->hram_io[IO_LY] == gb->hram_io[IO_LYC])
		{
			gb->hram_io[IO_STAT] |= STAT_LYC_COINC;

			if(!gb->direct.suppress_lcd_stat_interrupts &&
					(gb->hram_io[IO_STAT] & STAT_LYC_INTR))
				gb->hram_io[IO_IF] |= LCDC_INTR;
		}
		else
			gb->hram_io[IO_STAT] &= 0xFB;

		/* Check if LCD should be in Mode 1 (VBLANK) state */
		if(gb->hram_io[IO_LY] == LCD_HEIGHT)
		{
			gb->hram_io[IO_STAT] =
				(gb->hram_io[IO_STAT] & ~STAT_MODE) | IO_STAT_MODE_VBLANK;
			gb->gb_frame = true;
			gb->hram_io[IO_IF] |= VBLANK_INTR;
			gb->lcd_blank = false;

			if(!gb->direct.suppress_lcd_stat_interrupts &&
					(gb->hram_io[IO_STAT] & STAT_MODE_1_INTR))
				gb->hram_io[IO_IF] |= LCDC_INTR;

#if ENABLE_LCD
			/* If frame skip is activated, check if we need to draw
			 * the frame or skip it. */
			if(gb->direct.frame_skip)
			{
				gb->display.frame_skip_count =
					!gb->display.frame_skip_count;
			}

			/* If interlaced is activated, change which lines get
			 * updated. Also, only update lines on frames that are
			 * actually drawn when frame skip is enabled. */
			if(gb->direct.interlace &&
					(!gb->direct.frame_skip ||
					 gb->display.frame_skip_count))
			{
				gb->display.interlace_count =
					!gb->display.interlace_count;
			}
#endif
			/* If halted forever, then return on VBLANK. */
			if(gb->gb_halt && !gb->hram_io[IO_IE])
			{
				__gb_lcd_reschedule(gb);
				return true;
			}
		}
		/* Start of normal Line (not in VBLANK) */
		else if(gb->hram_io[IO_LY] < LCD_HEIGHT)
		{
			if(gb->hram_io[IO_LY] == 0)
			{
				/* Clear Screen */
				gb->display.WY = gb->hram_io[IO_WY];
				gb->display.window_clear = 0;
			}

			/* OAM Search occurs at the start of the line. */
			gb->hram_io[IO_STAT] = (gb->hram_io[IO_STAT] & ~STAT_MODE) | IO_STAT_MODE_OAM_SCAN;
			gb->counter.lcd_count = 0;

#if WALNUT_FULL_GBC_SUPPORT
			/* DMA GBC */
			if(gb->cgb.cgbMode && !gb->cgb.dmaActive && gb->cgb.dmaMode)
			{
#if WALNUT_GB_32BIT_DMA
				for (uint8_t i = 0; i < 0x10; i += 4)
				{
					uint32_t val = __gb_read32(gb, (gb->cgb.dmaSource & 0xFFF0) + i);
					__gb_write32(gb, ((gb->cgb.dmaDest & 0x1FF0) | 0x8000) + i, val);
				}
#elif WALNUT_GB_16BIT_DMA
				for (uint8_t i = 0; i < 0x10; i += 2)
				{
					uint16_t val = __gb_read16(gb, (gb->cgb.dmaSource & 0xFFF0) + i);
					__gb_write16(gb, ((gb->cgb.dmaDest & 0x1FF0) | 0x8000) + i, val);
				}
#else
				for (uint8_t i = 0; i < 0x10; i++)
				{
					__gb_write(gb, ((gb->cgb.dmaDest & 0x1FF0) | 0x8000) + i,
									__gb_read(gb, (gb->cgb.dmaSource & 0xFFF0) + i));
				}
#endif

				gb->cgb.dmaSource += 0x10;
				gb->cgb.dmaDest += 0x10;
				if(!(--gb->cgb.dmaSize)) {gb->cgb.dmaActive = 1;
				}
#if WALNUT_GB_SAFE_DUALFETCH_DMA
				if(invalidate_prefetch_on_hdma)
					gb->prefetch_invalid=true;
#else
				(void)invalidate_prefetch_on_hdma;
#endif
			}
#else
			(void)invalidate_prefetch_on_hdma;
#endif
			if(!gb->direct.suppress_lcd_stat_interrupts &&
					(gb->hram_io[IO_STAT] & STAT_MODE_2_INTR))
				gb->hram_io[IO_IF] |= LCDC_INTR;

			/* If halted immediately jump to next LCD mode.
			 * From OAM Search to LCD Draw. */
			*inst_cycles_io = LCD_MODE2_OAM_SCAN_DURATION;
		}
		__gb_lcd_reschedule(gb);
		return false;
	}

	const uint8_t stat = gb->hram_io[IO_STAT];
	const uint8_t stat_mode = stat & STAT_MODE;

	/* Go from Mode 3 (LCD Draw) to Mode 0 (HBLANK). */
	if(stat_mode == IO_STAT_MODE_LCD_DRAW &&
			gb->counter.lcd_count >= LCD_MODE3_LCD_DRAW_END)
	{
		gb->hram_io[IO_STAT] = (stat & ~STAT_MODE) | IO_STAT_MODE_HBLANK;

		if(!gb->direct.suppress_lcd_stat_interrupts &&
				(stat & STAT_MODE_0_INTR))
			gb->hram_io[IO_IF] |= LCDC_INTR;

		/* If halted immediately, jump from OAM Scan to LCD Draw. */
		if (gb->counter.lcd_count < LCD_MODE0_HBLANK_MAX_DRUATION)
			*inst_cycles_io = LCD_MODE0_HBLANK_MAX_DRUATION - gb->counter.lcd_count;
	}
	/* Go from Mode 2 (OAM Scan) to Mode 3 (LCD Draw). */
	else if(stat_mode == IO_STAT_MODE_OAM_SCAN &&
			gb->counter.lcd_count >= LCD_MODE2_OAM_SCAN_END)
	{
		gb->hram_io[IO_STAT] = (stat & ~STAT_MODE) | IO_STAT_MODE_LCD_DRAW;
#if ENABLE_LCD
		if(!gb->lcd_blank
#if WALNUT_GB_SKIP_FRAME_HEADLESS
				&& (!gb->direct.frame_skip || gb->display.frame_skip_count)
#endif
				)
			__gb_draw_line(gb);
#endif
		/* If halted immediately jump to next LCD mode. */
		if (gb->counter.lcd_count < LCD_MODE3_LCD_DRAW_MIN_DURATION)
			*inst_cycles_io = LCD_MODE3_LCD_DRAW_MIN_DURATION - gb->counter.lcd_count;
	}
	__gb_lcd_reschedule(gb);
	return false;
}

static inline uint_fast16_t __gb_serial_transfer_cycle_limit(uint8_t serial_control)
{
#if WALNUT_FULL_GBC_SUPPORT
	if(serial_control & 0x3)
		return SERIAL_CYCLES_32KB;
#else
	(void)serial_control;
#endif
	return SERIAL_CYCLES_1KB;
}

static inline bool __gb_post_step_fast_path(struct gb_s *gb,
		uint_fast16_t inst_cycles);

static inline uint_fast32_t __gb_lcd_delta_to_cpu_cycles(
		const struct gb_s *gb, uint_fast32_t lcd_cycles)
{
#if WALNUT_FULL_GBC_SUPPORT
	return gb->cgb.doubleSpeed ? (lcd_cycles << 1) : lcd_cycles;
#else
	(void)gb;
	return lcd_cycles;
#endif
}

static inline uint_fast32_t __gb_cpu_cycles_to_next_vblank(
		const struct gb_s *gb)
{
	const uint8_t ly = gb->hram_io[IO_LY];
	const uint_fast16_t lcd_count = gb->counter.lcd_count;
	uint_fast32_t lines;

	if(ly < LCD_HEIGHT)
		lines = LCD_HEIGHT - ly;
	else
		lines = (LCD_VERT_LINES - ly) + LCD_HEIGHT;

	uint_fast32_t lcd_cycles = (lines * LCD_LINE_CYCLES) - lcd_count;
	if(lcd_cycles == 0)
		lcd_cycles = LCD_FRAME_CYCLES;
	return __gb_lcd_delta_to_cpu_cycles(gb, lcd_cycles);
}

static inline bool __gb_timer_irq_before_cycles(
		const struct gb_s *gb, uint_fast32_t cycles)
{
	if(!(gb->hram_io[IO_IE] & TIMER_INTR) ||
			!(gb->hram_io[IO_TAC] & IO_TAC_ENABLE_MASK))
		return false;

	const uint_fast16_t tac_cycles =
		WALNUT_GB_TAC_CYCLES[gb->hram_io[IO_TAC] & IO_TAC_RATE_MASK];
	const uint_fast16_t tick_cycles =
		(gb->counter.tima_count < tac_cycles) ?
		(tac_cycles - gb->counter.tima_count) : 1;
	const uint_fast16_t increments =
		(uint_fast16_t)(0x100u - gb->hram_io[IO_TIMA]);
	const uint_fast32_t irq_cycles =
		tick_cycles + ((uint_fast32_t)(increments - 1u) * tac_cycles);
	return irq_cycles <= cycles;
}

static inline void __gb_fast_forward_div_timer(struct gb_s *gb,
		uint_fast32_t cycles)
{
	uint_fast32_t div_count = gb->counter.div_count + cycles;
	gb->hram_io[IO_DIV] = (uint8_t)(gb->hram_io[IO_DIV] +
		(uint8_t)(div_count / DIV_CYCLES));
	gb->counter.div_count = (uint_fast16_t)(div_count % DIV_CYCLES);

	const uint8_t tac = gb->hram_io[IO_TAC];
	if(!(tac & IO_TAC_ENABLE_MASK))
		return;

	const uint_fast16_t tac_cycles =
		WALNUT_GB_TAC_CYCLES[tac & IO_TAC_RATE_MASK];
	uint_fast32_t tima_count = gb->counter.tima_count + cycles;
	uint_fast32_t ticks = tima_count / tac_cycles;
	gb->counter.tima_count = (uint_fast16_t)(tima_count % tac_cycles);

	while(ticks--)
	{
		if(++gb->hram_io[IO_TIMA] == 0)
		{
			gb->hram_io[IO_IF] |= TIMER_INTR;
			gb->hram_io[IO_TIMA] = gb->hram_io[IO_TMA];
		}
	}
}

static inline bool __gb_halt_fast_forward_to_vblank(struct gb_s *gb)
{
	if(!gb->direct.frame_skip || gb->display.frame_skip_count)
		return false;
	if(!gb->gb_ime || !(gb->hram_io[IO_IE] & VBLANK_INTR) ||
			__gb_effective_pending_interrupts(gb))
		return false;
	if(!(gb->hram_io[IO_LCDC] & LCDC_ENABLE))
		return false;
	if(!gb->direct.suppress_lcd_stat_interrupts &&
			(gb->hram_io[IO_STAT] & (STAT_LYC_INTR | STAT_MODE_2_INTR |
			STAT_MODE_1_INTR | STAT_MODE_0_INTR)))
		return false;
	if(gb->hram_io[IO_SC] & SERIAL_SC_TX_START)
		return false;
	if(WGB_UNLIKELY(gb->mbc == 3) &&
			(gb->rtc_real.reg.high & 0x40) == 0)
		return false;
#if WALNUT_FULL_GBC_SUPPORT
	if(gb->cgb.cgbMode && gb->cgb.dmaMode && !gb->cgb.dmaActive)
		return false;
#endif

	/* The distance computations below read live counters. */
	__gb_lazy_post_flush_gb(gb);

	const uint_fast32_t cycles = __gb_cpu_cycles_to_next_vblank(gb);
	if(cycles == 0 || __gb_timer_irq_before_cycles(gb, cycles))
		return false;

	__gb_fast_forward_div_timer(gb, cycles);

	gb->counter.lcd_count = 0;
	gb->hram_io[IO_LY] = LCD_HEIGHT;
	if(gb->hram_io[IO_LY] == gb->hram_io[IO_LYC])
		gb->hram_io[IO_STAT] |= STAT_LYC_COINC;
	else
		gb->hram_io[IO_STAT] &= 0xFB;
	gb->hram_io[IO_STAT] =
		(gb->hram_io[IO_STAT] & ~STAT_MODE) | IO_STAT_MODE_VBLANK;
	gb->gb_frame = true;
	gb->hram_io[IO_IF] |= VBLANK_INTR;
	gb->lcd_blank = false;

#if ENABLE_LCD
	if(gb->direct.frame_skip)
		gb->display.frame_skip_count = !gb->display.frame_skip_count;
	if(gb->direct.interlace &&
			(!gb->direct.frame_skip || gb->display.frame_skip_count))
		gb->display.interlace_count = !gb->display.interlace_count;
#endif
	gb->display.WY = gb->hram_io[IO_WY];
	gb->display.window_clear = 0;
	__gb_lcd_reschedule(gb);
	return true;
}

static inline void __gb_fast_poll_wait_advance_cycles(struct gb_s *gb,
		uint_fast32_t cycles, bool stop_on_irq)
{
	__gb_lazy_post_flush_gb(gb);
	while(cycles != 0)
	{
		uint_fast16_t chunk = cycles > 0x3FFFu ? 0x3FFFu : (uint_fast16_t)cycles;
		const uint_fast32_t lcd_delta = __gb_lcd_next_event_delta(gb);
		const uint_fast32_t lcd_cpu_delta =
			__gb_lcd_delta_to_cpu_cycles(gb, lcd_delta);
		if(lcd_cpu_delta != 0 && lcd_cpu_delta < chunk)
			chunk = (uint_fast16_t)lcd_cpu_delta;
#if WALNUT_FULL_GBC_SUPPORT
		if(gb->cgb.doubleSpeed && (gb->hram_io[IO_LCDC] & LCDC_ENABLE) &&
				chunk == 1 && cycles > 1)
			chunk = 2;
#endif
		if(chunk == 0)
			chunk = 1;

		if(__gb_post_step_fast_path(gb, chunk))
		{
			cycles -= chunk;
			if(stop_on_irq && __gb_effective_pending_interrupts(gb))
				break;
			continue;
		}

		gb->counter.div_count += chunk;
		while(gb->counter.div_count >= DIV_CYCLES)
		{
			gb->hram_io[IO_DIV]++;
			gb->counter.div_count -= DIV_CYCLES;
		}

		if(WGB_UNLIKELY(gb->mbc == 3) &&
				(gb->rtc_real.reg.high & 0x40) == 0)
		{
			gb->counter.rtc_count += chunk;
			while(WGB_UNLIKELY(gb->counter.rtc_count >= RTC_CYCLES))
			{
				gb->counter.rtc_count -= RTC_CYCLES;
				if(WGB_UNLIKELY(gb->rtc_real.reg.sec == 63))
				{
					gb->rtc_real.reg.sec = 0;
					continue;
				}
				if(++gb->rtc_real.reg.sec != 60)
					continue;
				gb->rtc_real.reg.sec = 0;
				if(gb->rtc_real.reg.min == 63)
				{
					gb->rtc_real.reg.min = 0;
					continue;
				}
				if(++gb->rtc_real.reg.min != 60)
					continue;
				gb->rtc_real.reg.min = 0;
				if(gb->rtc_real.reg.hour == 31)
				{
					gb->rtc_real.reg.hour = 0;
					continue;
				}
				if(++gb->rtc_real.reg.hour != 24)
					continue;
				gb->rtc_real.reg.hour = 0;
				if(++gb->rtc_real.reg.yday != 0)
					continue;
				if(gb->rtc_real.reg.high & 1)
					gb->rtc_real.reg.high |= 0x80;
				gb->rtc_real.reg.high ^= 1;
			}
		}

		const uint8_t serial_control = gb->hram_io[IO_SC];
		if(WGB_UNLIKELY(serial_control & SERIAL_SC_TX_START))
		{
			if(gb->counter.serial_count == 0 && gb->gb_serial_tx != NULL)
				(gb->gb_serial_tx)(gb, gb->hram_io[IO_SB]);

			gb->counter.serial_count += chunk;
			if(gb->counter.serial_count >=
					__gb_serial_transfer_cycle_limit(serial_control))
			{
				uint8_t rx;
				if(gb->gb_serial_rx != NULL &&
						(gb->gb_serial_rx(gb, &rx) == GB_SERIAL_RX_SUCCESS))
				{
					gb->hram_io[IO_SB] = rx;
					gb->hram_io[IO_SC] &= 0x01;
					gb->hram_io[IO_IF] |= SERIAL_INTR;
				}
				else if(serial_control & SERIAL_SC_CLOCK_SRC)
				{
					gb->hram_io[IO_SB] = 0xFF;
					gb->hram_io[IO_SC] &= 0x01;
					gb->hram_io[IO_IF] |= SERIAL_INTR;
				}
				gb->counter.serial_count = 0;
			}
		}

		const uint8_t tac = gb->hram_io[IO_TAC];
		if(tac & IO_TAC_ENABLE_MASK)
		{
			const uint_fast16_t tac_cycles =
				WALNUT_GB_TAC_CYCLES[tac & IO_TAC_RATE_MASK];
			gb->counter.tima_count += chunk;
			while(gb->counter.tima_count >= tac_cycles)
			{
				gb->counter.tima_count -= tac_cycles;
				if(++gb->hram_io[IO_TIMA] == 0)
				{
					gb->hram_io[IO_IF] |= TIMER_INTR;
					gb->hram_io[IO_TIMA] = gb->hram_io[IO_TMA];
				}
			}
		}

		uint_fast16_t lcd_chunk = chunk;
		__gb_post_lcd_step(gb, &lcd_chunk, true);
		cycles -= chunk;
		if(stop_on_irq && __gb_effective_pending_interrupts(gb))
			break;
	}
}

static inline void __gb_min_cpu_event_cycles(uint_fast32_t *limit,
		uint_fast32_t cycles)
{
	if(cycles != 0 && cycles < *limit)
		*limit = cycles;
}

enum walnut_gb_cpu_event_flags_e
{
	WGB_CPU_EVENT_TIMER = 0x01,
	WGB_CPU_EVENT_SERIAL = 0x02,
	WGB_CPU_EVENT_RTC = 0x04,
	WGB_CPU_EVENT_LCD_OFF = 0x08,
	WGB_CPU_EVENT_LCD_DOUBLE = 0x10
};

static inline uint_fast32_t __gb_cpu_next_event_delta(struct gb_s *gb,
		uint_fast8_t *flags_io)
{
	uint_fast8_t flags = 0;
	uint_fast32_t next = (gb->counter.div_count < DIV_CYCLES) ?
		(DIV_CYCLES - gb->counter.div_count) : 1u;

	const uint8_t tac = gb->hram_io[IO_TAC];
	if(tac & IO_TAC_ENABLE_MASK)
	{
		flags |= WGB_CPU_EVENT_TIMER;
		const uint_fast16_t tac_cycles =
			WALNUT_GB_TAC_CYCLES[tac & IO_TAC_RATE_MASK];
		const uint_fast32_t timer_left =
			(gb->counter.tima_count < tac_cycles) ?
			(tac_cycles - gb->counter.tima_count) : 1u;
		__gb_min_cpu_event_cycles(&next, timer_left);
	}

	const uint8_t serial_control = gb->hram_io[IO_SC];
	if(serial_control & SERIAL_SC_TX_START)
	{
		flags |= WGB_CPU_EVENT_SERIAL;
		const uint_fast16_t serial_cycles =
			__gb_serial_transfer_cycle_limit(serial_control);
		const uint_fast32_t serial_left =
			(gb->counter.serial_count < serial_cycles) ?
			(serial_cycles - gb->counter.serial_count) : 1u;
		__gb_min_cpu_event_cycles(&next, serial_left);
	}

	if(WGB_UNLIKELY(gb->mbc == 3) &&
			(gb->rtc_real.reg.high & 0x40) == 0)
	{
		flags |= WGB_CPU_EVENT_RTC;
		const uint_fast32_t rtc_left =
			(gb->counter.rtc_count < RTC_CYCLES) ?
			(RTC_CYCLES - gb->counter.rtc_count) : 1u;
		__gb_min_cpu_event_cycles(&next, rtc_left);
	}

	uint_fast32_t lcd_event_count = gb->counter.lcd_event_count;
	if(WGB_UNLIKELY(lcd_event_count == 0))
	{
		lcd_event_count = __gb_lcd_next_event_delta(gb);
		gb->counter.lcd_event_count = lcd_event_count;
	}
	if(WGB_UNLIKELY(!(gb->hram_io[IO_LCDC] & LCDC_ENABLE)))
	{
		flags |= WGB_CPU_EVENT_LCD_OFF;
		__gb_min_cpu_event_cycles(&next, lcd_event_count);
	}
	else
	{
#if WALNUT_FULL_GBC_SUPPORT
		if(gb->cgb.doubleSpeed)
			flags |= WGB_CPU_EVENT_LCD_DOUBLE;
#endif
		__gb_min_cpu_event_cycles(&next,
			__gb_lcd_delta_to_cpu_cycles(gb, lcd_event_count));
	}

	*flags_io = flags;
	return next ? next : 1u;
}

static inline void __gb_cpu_apply_no_event_fast(struct gb_s *gb,
		uint_fast16_t inst_cycles, uint_fast8_t flags)
{
	gb->counter.div_count += inst_cycles;

	if(flags & WGB_CPU_EVENT_TIMER)
		gb->counter.tima_count += inst_cycles;

	if(WGB_UNLIKELY(flags & WGB_CPU_EVENT_SERIAL))
	{
		if(gb->counter.serial_count == 0 && gb->gb_serial_tx != NULL)
			(gb->gb_serial_tx)(gb, gb->hram_io[IO_SB]);
		gb->counter.serial_count += inst_cycles;
	}

	if(WGB_UNLIKELY(flags & WGB_CPU_EVENT_RTC))
		gb->counter.rtc_count += inst_cycles;

	if(WGB_UNLIKELY(flags & WGB_CPU_EVENT_LCD_OFF))
	{
		gb->counter.lcd_off_count += inst_cycles;
		gb->counter.lcd_event_count -= inst_cycles;
		return;
	}

#if WALNUT_FULL_GBC_SUPPORT
	const uint_fast16_t lcd_cycles =
		(flags & WGB_CPU_EVENT_LCD_DOUBLE) ? (inst_cycles >> 1) : inst_cycles;
#else
	const uint_fast16_t lcd_cycles = inst_cycles;
#endif
	gb->counter.lcd_count += lcd_cycles;
	gb->counter.lcd_event_count -= lcd_cycles;
}

static inline bool __gb_post_step_fast_path(struct gb_s *gb,
		uint_fast16_t inst_cycles)
{
	if(WGB_UNLIKELY(gb->gb_halt))
	{
		return false;
	}

#if WALNUT_GB_CPU_EVENT_MINI_COUNTDOWN
	uint_fast32_t cpu_event_count = gb->counter.cpu_event_count;
	uint_fast8_t cpu_event_flags = gb->counter.cpu_event_flags;
	if(WGB_UNLIKELY(cpu_event_count == 0))
	{
		cpu_event_count = __gb_cpu_next_event_delta(gb, &cpu_event_flags);
		gb->counter.cpu_event_flags = cpu_event_flags;
	}
	if(WGB_LIKELY(cpu_event_count > inst_cycles))
	{
		gb->counter.cpu_event_count = cpu_event_count - inst_cycles;
		__gb_cpu_apply_no_event_fast(gb, inst_cycles, cpu_event_flags);
		return true;
	}
	__gb_cpu_timing_reschedule(gb);
	return false;
#elif WALNUT_GB_CPU_EVENT_COUNTDOWN
	uint_fast8_t cpu_event_flags;
	uint_fast32_t cpu_event_count =
		__gb_cpu_next_event_delta(gb, &cpu_event_flags);
	if(WGB_LIKELY(cpu_event_count > inst_cycles))
	{
		gb->counter.cpu_event_count = cpu_event_count - inst_cycles;
		__gb_cpu_apply_no_event_fast(gb, inst_cycles, cpu_event_flags);
		return true;
	}
	__gb_cpu_timing_reschedule(gb);
	return false;
#endif

	const bool rtc_active = (gb->mbc == 3 && (gb->rtc_real.reg.high & 0x40) == 0);
	uint_fast32_t rtc_count = gb->counter.rtc_count;
	if(WGB_UNLIKELY(rtc_active))
	{
		rtc_count += inst_cycles;
		if(WGB_UNLIKELY(rtc_count >= RTC_CYCLES))
		{
			return false;
		}
	}

	const uint8_t serial_control = gb->hram_io[IO_SC];
	const bool serial_active = (serial_control & SERIAL_SC_TX_START) != 0;
	uint_fast16_t serial_count = gb->counter.serial_count;
	if(WGB_UNLIKELY(serial_active))
	{
		serial_count += inst_cycles;
		if(WGB_UNLIKELY(serial_count >=
				__gb_serial_transfer_cycle_limit(serial_control)))
		{
			return false;
		}
	}

	const uint_fast16_t div_count = gb->counter.div_count + inst_cycles;
	if(WGB_UNLIKELY(div_count >= DIV_CYCLES))
	{
		return false;
	}

	const uint8_t tac = gb->hram_io[IO_TAC];
	uint_fast16_t tima_count = gb->counter.tima_count;
	if(tac & IO_TAC_ENABLE_MASK)
	{
		tima_count += inst_cycles;
		if(WGB_UNLIKELY(tima_count >= WALNUT_GB_TAC_CYCLES[tac & IO_TAC_RATE_MASK]))
		{
			return false;
		}
	}

	if(WGB_UNLIKELY(!(gb->hram_io[IO_LCDC] & LCDC_ENABLE)))
	{
		uint_fast32_t lcd_event_count = gb->counter.lcd_event_count;
		if(WGB_UNLIKELY(lcd_event_count == 0))
			lcd_event_count = __gb_lcd_next_event_delta(gb);
		if(WGB_UNLIKELY(lcd_event_count <= inst_cycles))
		{
			return false;
		}

		gb->counter.lcd_off_count += inst_cycles;
		gb->counter.lcd_event_count = lcd_event_count - inst_cycles;
	}
	else
	{
#if WALNUT_FULL_GBC_SUPPORT
		const uint_fast16_t lcd_cycles = (inst_cycles >> gb->cgb.doubleSpeed);
#else
		const uint_fast16_t lcd_cycles = inst_cycles;
#endif
		uint_fast32_t lcd_event_count = gb->counter.lcd_event_count;
		if(WGB_UNLIKELY(lcd_event_count == 0))
			lcd_event_count = __gb_lcd_next_event_delta(gb);
		if(WGB_UNLIKELY(lcd_event_count <= lcd_cycles))
		{
			return false;
		}

		gb->counter.lcd_count += lcd_cycles;
		gb->counter.lcd_event_count = lcd_event_count - lcd_cycles;
	}

	if(WGB_UNLIKELY(rtc_active))
		gb->counter.rtc_count = rtc_count;
	if(WGB_UNLIKELY(serial_active))
	{
		if(gb->counter.serial_count == 0 && gb->gb_serial_tx != NULL)
			(gb->gb_serial_tx)(gb, gb->hram_io[IO_SB]);
		gb->counter.serial_count = serial_count;
	}
	gb->counter.div_count = div_count;
	if(tac & IO_TAC_ENABLE_MASK)
		gb->counter.tima_count = tima_count;

	return true;
}

static inline bool __gb_post_cpu_step_slow_once(struct gb_s *gb,
		uint_fast16_t *inst_cycles_io, bool invalidate_prefetch_on_hdma)
{
	uint_fast16_t inst_cycles = *inst_cycles_io;
	__gb_cpu_timing_reschedule(gb);

	gb->counter.div_count += inst_cycles;
	while(gb->counter.div_count >= DIV_CYCLES)
	{
		gb->hram_io[IO_DIV]++;
		gb->counter.div_count -= DIV_CYCLES;
	}

	if(WGB_UNLIKELY(gb->mbc == 3) &&
			(gb->rtc_real.reg.high & 0x40) == 0)
	{
		gb->counter.rtc_count += inst_cycles;
		while(WGB_UNLIKELY(gb->counter.rtc_count >= RTC_CYCLES))
		{
			gb->counter.rtc_count -= RTC_CYCLES;
			if(WGB_UNLIKELY(gb->rtc_real.reg.sec == 63))
			{
				gb->rtc_real.reg.sec = 0;
				continue;
			}
			if(++gb->rtc_real.reg.sec != 60)
				continue;
			gb->rtc_real.reg.sec = 0;
			if(gb->rtc_real.reg.min == 63)
			{
				gb->rtc_real.reg.min = 0;
				continue;
			}
			if(++gb->rtc_real.reg.min != 60)
				continue;
			gb->rtc_real.reg.min = 0;
			if(gb->rtc_real.reg.hour == 31)
			{
				gb->rtc_real.reg.hour = 0;
				continue;
			}
			if(++gb->rtc_real.reg.hour != 24)
				continue;
			gb->rtc_real.reg.hour = 0;
			if(++gb->rtc_real.reg.yday != 0)
				continue;
			if(gb->rtc_real.reg.high & 1)
				gb->rtc_real.reg.high |= 0x80;
			gb->rtc_real.reg.high ^= 1;
		}
	}

	const uint8_t serial_control = gb->hram_io[IO_SC];
	if(WGB_UNLIKELY(serial_control & SERIAL_SC_TX_START))
	{
		if(gb->counter.serial_count == 0 && gb->gb_serial_tx != NULL)
			(gb->gb_serial_tx)(gb, gb->hram_io[IO_SB]);

		gb->counter.serial_count += inst_cycles;
		if(gb->counter.serial_count >=
				__gb_serial_transfer_cycle_limit(serial_control))
		{
			uint8_t rx;
			if(gb->gb_serial_rx != NULL &&
					(gb->gb_serial_rx(gb, &rx) == GB_SERIAL_RX_SUCCESS))
			{
				gb->hram_io[IO_SB] = rx;
				gb->hram_io[IO_SC] &= 0x01;
				gb->hram_io[IO_IF] |= SERIAL_INTR;
			}
			else if(serial_control & SERIAL_SC_CLOCK_SRC)
			{
				gb->hram_io[IO_SB] = 0xFF;
				gb->hram_io[IO_SC] &= 0x01;
				gb->hram_io[IO_IF] |= SERIAL_INTR;
			}
			gb->counter.serial_count = 0;
		}
	}

	const uint8_t tac = gb->hram_io[IO_TAC];
	if(tac & IO_TAC_ENABLE_MASK)
	{
		const uint_fast16_t tac_cycles =
			WALNUT_GB_TAC_CYCLES[tac & IO_TAC_RATE_MASK];
		gb->counter.tima_count += inst_cycles;
		while(gb->counter.tima_count >= tac_cycles)
		{
			gb->counter.tima_count -= tac_cycles;
			if(++gb->hram_io[IO_TIMA] == 0)
			{
				gb->hram_io[IO_IF] |= TIMER_INTR;
				gb->hram_io[IO_TIMA] = gb->hram_io[IO_TMA];
			}
		}
	}

	return __gb_post_lcd_step(gb, inst_cycles_io,
		invalidate_prefetch_on_hdma);
}

static inline void __gb_post_cpu_step(struct gb_s *gb,
		uint_fast16_t *inst_cycles_io, bool invalidate_prefetch_on_hdma)
{
	uint_fast16_t inst_cycles = *inst_cycles_io;
	if(__gb_post_step_fast_path(gb, inst_cycles))
		return;

	__gb_post_cpu_step_slow_once(gb, inst_cycles_io,
		invalidate_prefetch_on_hdma);
}

/* Post any cycles batched by the threaded runner. No-op outside it. */
static inline void __gb_lazy_post_flush_gb(struct gb_s *gb)
{
#if WALNUT_GB_LAZY_POST_ACTIVE
	uint_fast32_t *const pending = gb->lazy_post_pending;

	if(pending != NULL && *pending != 0)
	{
		uint_fast16_t cycles = (uint_fast16_t)*pending;
		*pending = 0;
		__gb_post_cpu_step(gb, &cycles, false);
	}
#else
	(void)gb;
#endif
}

enum walnut_gb_halt_boundary_e
{
	WGB_HALT_BOUNDARY_CAP = 0,
	WGB_HALT_BOUNDARY_DIV,
	WGB_HALT_BOUNDARY_TIMER,
	WGB_HALT_BOUNDARY_LCD,
	WGB_HALT_BOUNDARY_SERIAL,
	WGB_HALT_BOUNDARY_RTC
};

static inline void __gb_halt_consider_boundary(uint_fast32_t *limit,
		enum walnut_gb_halt_boundary_e *boundary,
		uint_fast32_t cycles, enum walnut_gb_halt_boundary_e candidate)
{
	if(cycles == 0)
		cycles = 1;
	if(cycles <= *limit)
	{
		*limit = cycles;
		*boundary = candidate;
	}
}

static inline uint_fast16_t __gb_halt_next_safe_step(struct gb_s *gb,
		enum walnut_gb_halt_boundary_e *boundary)
{
	uint_fast32_t next = WALNUT_GB_HALT_STEP_CAP;
	*boundary = WGB_HALT_BOUNDARY_CAP;

	__gb_halt_consider_boundary(&next, boundary,
		DIV_CYCLES - gb->counter.div_count, WGB_HALT_BOUNDARY_DIV);

	const uint8_t tac = gb->hram_io[IO_TAC];
	if(tac & IO_TAC_ENABLE_MASK)
	{
		const uint_fast16_t tac_cycles =
			WALNUT_GB_TAC_CYCLES[tac & IO_TAC_RATE_MASK];
		const uint_fast32_t timer_left =
			(gb->counter.tima_count < tac_cycles) ?
			(tac_cycles - gb->counter.tima_count) : 1u;
		__gb_halt_consider_boundary(&next, boundary, timer_left,
			WGB_HALT_BOUNDARY_TIMER);
	}

	uint_fast32_t lcd_event_count = gb->counter.lcd_event_count;
	if(WGB_UNLIKELY(lcd_event_count == 0))
	{
		lcd_event_count = __gb_lcd_next_event_delta(gb);
		gb->counter.lcd_event_count = lcd_event_count;
	}
	const uint_fast32_t lcd_cpu_delta =
		(gb->hram_io[IO_LCDC] & LCDC_ENABLE) ?
		__gb_lcd_delta_to_cpu_cycles(gb, lcd_event_count) : lcd_event_count;
	__gb_halt_consider_boundary(&next, boundary, lcd_cpu_delta,
		WGB_HALT_BOUNDARY_LCD);

	const uint8_t serial_control = gb->hram_io[IO_SC];
	if(serial_control & SERIAL_SC_TX_START)
	{
		const uint_fast16_t serial_cycles =
			__gb_serial_transfer_cycle_limit(serial_control);
		const uint_fast32_t serial_left =
			(gb->counter.serial_count < serial_cycles) ?
			(serial_cycles - gb->counter.serial_count) : 1u;
		__gb_halt_consider_boundary(&next, boundary, serial_left,
			WGB_HALT_BOUNDARY_SERIAL);
	}

	if(WGB_UNLIKELY(gb->mbc == 3) &&
			(gb->rtc_real.reg.high & 0x40) == 0)
	{
		const uint_fast32_t rtc_left =
			(gb->counter.rtc_count < RTC_CYCLES) ?
			(RTC_CYCLES - gb->counter.rtc_count) : 1u;
		__gb_halt_consider_boundary(&next, boundary, rtc_left,
			WGB_HALT_BOUNDARY_RTC);
	}

	return (uint_fast16_t)next;
}

static inline void __gb_halt_advance_no_event_fast(struct gb_s *gb,
		uint_fast16_t cycles)
{
	__gb_cpu_timing_reschedule(gb);

	gb->counter.div_count += cycles;

	const uint8_t tac = gb->hram_io[IO_TAC];
	if(tac & IO_TAC_ENABLE_MASK)
		gb->counter.tima_count += cycles;

	const uint8_t serial_control = gb->hram_io[IO_SC];
	if(WGB_UNLIKELY(serial_control & SERIAL_SC_TX_START))
	{
		if(gb->counter.serial_count == 0 && gb->gb_serial_tx != NULL)
			(gb->gb_serial_tx)(gb, gb->hram_io[IO_SB]);
		gb->counter.serial_count += cycles;
	}

	if(WGB_UNLIKELY(gb->mbc == 3) &&
			(gb->rtc_real.reg.high & 0x40) == 0)
		gb->counter.rtc_count += cycles;

	if(WGB_UNLIKELY(!(gb->hram_io[IO_LCDC] & LCDC_ENABLE)))
	{
		gb->counter.lcd_off_count += cycles;
		gb->counter.lcd_event_count -= cycles;
	}
	else
	{
#if WALNUT_FULL_GBC_SUPPORT
		const uint_fast16_t lcd_cycles = (cycles >> gb->cgb.doubleSpeed);
#else
		const uint_fast16_t lcd_cycles = cycles;
#endif
		gb->counter.lcd_count += lcd_cycles;
		gb->counter.lcd_event_count -= lcd_cycles;
	}
}

static inline void __gb_service_halt_until_event_or_irq(struct gb_s *gb,
		bool invalidate_prefetch_on_hdma)
{
	if(WGB_LIKELY(!gb->gb_halt))
		return;

	__gb_lazy_post_flush_gb(gb);

	if(__gb_halt_fast_forward_to_vblank(gb))
	{
		return;
	}

	while(gb->gb_halt && __gb_effective_pending_interrupts(gb) == 0 &&
			!gb->gb_frame)
	{
		enum walnut_gb_halt_boundary_e boundary;
		uint_fast16_t step = __gb_halt_next_safe_step(gb, &boundary);
		if(WGB_UNLIKELY(step == 0))
		{
			step = 4;
		}

		if(WGB_LIKELY(boundary == WGB_HALT_BOUNDARY_CAP))
		{
			__gb_halt_advance_no_event_fast(gb, step);
			continue;
		}

		uint_fast16_t exact_step = step;
		if(__gb_post_cpu_step_slow_once(gb, &exact_step,
				invalidate_prefetch_on_hdma))
			break;
	}
}

#if WALNUT_GB_THREADED_CPU_ACTIVE && WALNUT_GB_THREADED_FAST_MEM
static inline uint8_t WALNUT_GB_HOT_ATTR
__gb_read_threaded_fast(struct gb_s *gb, uint16_t addr)
{
	if(WGB_LIKELY(addr < ROM_N_ADDR))
	{
		if(WGB_UNLIKELY(gb->hram_io[IO_BOOT] == 0 &&
				(addr < 0x0100
#if WALNUT_FULL_GBC_SUPPORT
				 || (gb->cgb.cgbMode && addr >= 0x0200 && addr < 0x0900)
#endif
				)))
			return __gb_read(gb, addr);
		if(WGB_LIKELY(__gb_direct_rom_contains(gb, addr, 1)))
			return __gb_direct_rom_read8(gb, addr);
		return gb->gb_rom_read(gb, addr);
	}

	if(WGB_LIKELY(addr < VRAM_ADDR))
	{
		const uint32_t bank_addr = addr + __gb_fetch_rom_bank_offset(gb);
		if(WGB_LIKELY(__gb_direct_rom_contains(gb, bank_addr, 1)))
			return __gb_direct_rom_read8(gb, bank_addr);
		return gb->gb_rom_read(gb, bank_addr);
	}

	if(WGB_LIKELY(addr >= WRAM_0_ADDR && addr < ECHO_ADDR))
	{
#if WALNUT_FULL_GBC_SUPPORT
		if(gb->cgb.cgbMode && addr >= WRAM_1_ADDR)
			return gb->wram[addr - gb->cgb.wramBankOffset];
#endif
		return gb->wram[addr - WRAM_0_ADDR];
	}

	if(WGB_LIKELY(addr >= ECHO_ADDR && addr < OAM_ADDR))
	{
#if WALNUT_FULL_GBC_SUPPORT
		if(addr >= 0xF000 && gb->cgb.cgbMode)
			return gb->wram[(addr - 0x2000) - gb->cgb.wramBankOffset];
#endif
		return gb->wram[addr - ECHO_ADDR];
	}

	if(WGB_LIKELY(addr >= HRAM_ADDR))
	{
		return gb->hram_io[addr - IO_ADDR];
	}

	if(WGB_LIKELY(addr >= VRAM_ADDR && addr < CART_RAM_ADDR))
	{
#if WALNUT_FULL_GBC_SUPPORT
		return gb->vram[addr - gb->cgb.vramBankOffset];
#else
		return gb->vram[addr - VRAM_ADDR];
#endif
	}

	if(WGB_LIKELY(addr >= OAM_ADDR && addr < UNUSED_ADDR))
	{
		return gb->oam[addr - OAM_ADDR];
	}

	return __gb_read(gb, addr);
}

static inline uint16_t WALNUT_GB_HOT_ATTR
__gb_read16_threaded_fast(struct gb_s *gb, uint16_t addr)
{
	if(WGB_LIKELY(addr < ROM_N_ADDR))
	{
		if(WGB_UNLIKELY(gb->hram_io[IO_BOOT] == 0 &&
				(addr < 0x0100
#if WALNUT_FULL_GBC_SUPPORT
				 || (gb->cgb.cgbMode && addr >= 0x0200 && addr < 0x0900)
#endif
				)))
			return __gb_read16(gb, addr);
		if(WGB_LIKELY(addr <= 0x3FFE &&
				__gb_direct_rom_contains(gb, addr, 2)))
			return __gb_direct_rom_read16(gb, addr);
		return __gb_read16(gb, addr);
	}

	if(WGB_LIKELY(addr < VRAM_ADDR))
	{
		if(WGB_LIKELY(addr <= 0x7FFE))
		{
			const uint32_t bank_addr = addr + __gb_fetch_rom_bank_offset(gb);
			if(WGB_LIKELY(__gb_direct_rom_contains(gb, bank_addr, 2)))
				return __gb_direct_rom_read16(gb, bank_addr);
		}
		return __gb_read16(gb, addr);
	}

	if(WGB_LIKELY(addr >= WRAM_0_ADDR && addr <= 0xDFFE))
	{
#if WALNUT_FULL_GBC_SUPPORT
		if(gb->cgb.cgbMode && addr >= WRAM_1_ADDR)
		{
			const uint16_t idx = addr - gb->cgb.wramBankOffset;
			return (uint16_t)gb->wram[idx] |
				((uint16_t)gb->wram[idx + 1] << 8);
		}
#endif
		const uint16_t idx = addr - WRAM_0_ADDR;
		return (uint16_t)gb->wram[idx] | ((uint16_t)gb->wram[idx + 1] << 8);
	}

	if(WGB_LIKELY(addr >= ECHO_ADDR && addr <= 0xFDFE))
	{
#if WALNUT_FULL_GBC_SUPPORT
		if(addr >= 0xF000 && gb->cgb.cgbMode)
		{
			const uint16_t idx = (addr - 0x2000) - gb->cgb.wramBankOffset;
			return (uint16_t)gb->wram[idx] |
				((uint16_t)gb->wram[idx + 1] << 8);
		}
#endif
		const uint16_t idx = addr - ECHO_ADDR;
		return (uint16_t)gb->wram[idx] | ((uint16_t)gb->wram[idx + 1] << 8);
	}

	if(WGB_LIKELY(addr >= HRAM_ADDR && addr <= 0xFFFE))
	{
		const uint16_t idx = addr - IO_ADDR;
		return (uint16_t)gb->hram_io[idx] |
			((uint16_t)gb->hram_io[idx + 1] << 8);
	}

	if(WGB_LIKELY(addr >= VRAM_ADDR && addr <= 0x9FFE))
	{
#if WALNUT_FULL_GBC_SUPPORT
		const uint16_t idx = addr - gb->cgb.vramBankOffset;
#else
		const uint16_t idx = addr - VRAM_ADDR;
#endif
		return (uint16_t)gb->vram[idx] | ((uint16_t)gb->vram[idx + 1] << 8);
	}

	if(WGB_LIKELY(addr >= OAM_ADDR && addr <= 0xFE9E))
	{
		const uint16_t idx = addr - OAM_ADDR;
		return (uint16_t)gb->oam[idx] | ((uint16_t)gb->oam[idx + 1] << 8);
	}

	return __gb_read16(gb, addr);
}

static inline void WALNUT_GB_HOT_ATTR
__gb_write_threaded_fast(struct gb_s *gb, uint_fast16_t addr, uint8_t val)
{
	if(WGB_LIKELY(addr >= WRAM_0_ADDR && addr < ECHO_ADDR))
	{
#if WALNUT_FULL_GBC_SUPPORT
		if(addr >= WRAM_1_ADDR)
			gb->wram[addr - gb->cgb.wramBankOffset] = val;
		else
#endif
			gb->wram[addr - WRAM_0_ADDR] = val;
		return;
	}

	if(WGB_LIKELY(addr >= ECHO_ADDR && addr < OAM_ADDR))
	{
#if WALNUT_FULL_GBC_SUPPORT
		if(addr >= 0xF000)
			gb->wram[(addr - 0x2000) - gb->cgb.wramBankOffset] = val;
		else
#endif
			gb->wram[addr - ECHO_ADDR] = val;
		return;
	}

	if(WGB_LIKELY(HRAM_ADDR <= addr && addr < INTR_EN_ADDR))
	{
		gb->hram_io[addr - IO_ADDR] = val;
		return;
	}

	if(WGB_LIKELY(addr >= VRAM_ADDR && addr < CART_RAM_ADDR))
	{
		__gb_display_mark_dirty(gb);
#if WALNUT_FULL_GBC_SUPPORT
		gb->vram[addr - gb->cgb.vramBankOffset] = val;
#else
		gb->vram[addr - VRAM_ADDR] = val;
#endif
		return;
	}

	if(WGB_LIKELY(addr >= OAM_ADDR && addr < UNUSED_ADDR))
	{
		__gb_display_mark_dirty(gb);
		WGB_GB_GLITCH_APPLY_OAM_WRITE((uint16_t)addr, &val);
		gb->oam[addr - OAM_ADDR] = val;
		__gb_oam_cache_invalidate(gb);
		return;
	}

	__gb_write(gb, addr, val);
}
#endif

#if WALNUT_GB_THREADED_CPU_ACTIVE && WALNUT_GB_THREADED_LOCAL_CPU_REGS
static inline uint8_t WALNUT_GB_HOT_ATTR
__gb_fetch8_threaded_local_cpu(struct gb_s *gb, struct cpu_registers_s *cpu_reg)
{
	const uint16_t pc = cpu_reg->pc.reg++;

	switch(WALNUT_GB_GET_MSN16(pc))
	{
	case 0x0:
		if(gb->hram_io[IO_BOOT] == 0 && pc < 0x0100)
		{
			return gb->gb_bootrom_read(gb, pc);
		}
#if WALNUT_FULL_GBC_SUPPORT
		if(gb->cgb.cgbMode && gb->hram_io[IO_BOOT] == 0 &&
				pc >= 0x0200 && pc < 0x0900)
		{
			return gb->gb_bootrom_read(gb, pc);
		}
#endif
		/* Fallthrough */
	case 0x1:
	case 0x2:
	case 0x3:
		if(WGB_LIKELY(__gb_direct_rom_contains(gb, pc, 1)))
			return __gb_direct_rom_read8(gb, pc);
		return gb->gb_rom_read(gb, pc);

	case 0x4:
	case 0x5:
	case 0x6:
	case 0x7:
	{
		const uint32_t bank_addr = pc + __gb_fetch_rom_bank_offset(gb);
		if(WGB_LIKELY(__gb_direct_rom_contains(gb, bank_addr, 1)))
			return __gb_direct_rom_read8(gb, bank_addr);
		return gb->gb_rom_read(gb, bank_addr);
	}

	case 0xC:
		return gb->wram[pc - WRAM_0_ADDR];

	case 0xD:
#if WALNUT_FULL_GBC_SUPPORT
		if(gb->cgb.cgbMode)
			return gb->wram[pc - gb->cgb.wramBankOffset];
#endif
		return gb->wram[pc - WRAM_0_ADDR];

	case 0xE:
		return gb->wram[pc - ECHO_ADDR];

	case 0xF:
		if(pc < OAM_ADDR)
		{
#if WALNUT_FULL_GBC_SUPPORT
			if(gb->cgb.cgbMode)
				return gb->wram[(pc - 0x2000) - gb->cgb.wramBankOffset];
#endif
			return gb->wram[pc - ECHO_ADDR];
		}
		if(pc >= HRAM_ADDR)
		{
			return gb->hram_io[pc - IO_ADDR];
		}
		break;
	}

	return __gb_read(gb, pc);
}

static inline uint16_t WALNUT_GB_HOT_ATTR
__gb_fetch16_threaded_local_cpu(struct gb_s *gb, struct cpu_registers_s *cpu_reg)
{
	const uint16_t pc = cpu_reg->pc.reg;

	switch(WALNUT_GB_GET_MSN16(pc))
	{
	case 0x0:
		if(gb->hram_io[IO_BOOT] == 0 && pc < 0x0100)
			break;
#if WALNUT_FULL_GBC_SUPPORT
		if(gb->cgb.cgbMode && gb->hram_io[IO_BOOT] == 0 &&
				pc >= 0x0200 && pc < 0x0900)
			break;
#endif
		/* Fallthrough */
	case 0x1:
	case 0x2:
	case 0x3:
		if(WGB_LIKELY(pc <= 0x3FFE))
		{
			cpu_reg->pc.reg = pc + 2;
			if(WGB_LIKELY(__gb_direct_rom_contains(gb, pc, 2)))
				return __gb_direct_rom_read16(gb, pc);
			return gb->gb_rom_read_16bit(gb, pc);
		}
		break;

	case 0x4:
	case 0x5:
	case 0x6:
	case 0x7:
		if(WGB_LIKELY(pc <= 0x7FFE))
		{
			const uint32_t bank_addr = pc + __gb_fetch_rom_bank_offset(gb);
			cpu_reg->pc.reg = pc + 2;
			if(WGB_LIKELY(__gb_direct_rom_contains(gb, bank_addr, 2)))
				return __gb_direct_rom_read16(gb, bank_addr);
			return gb->gb_rom_read_16bit(gb, bank_addr);
		}
		break;

	case 0xC:
		if(pc <= 0xCFFE)
		{
			const uint16_t idx = pc - WRAM_0_ADDR;
			cpu_reg->pc.reg = pc + 2;
			return (uint16_t)gb->wram[idx] | ((uint16_t)gb->wram[idx + 1] << 8);
		}
		break;

	case 0xD:
		if(pc <= 0xDFFE)
		{
			cpu_reg->pc.reg = pc + 2;
#if WALNUT_FULL_GBC_SUPPORT
			if(gb->cgb.cgbMode)
			{
				const uint16_t idx = pc - gb->cgb.wramBankOffset;
				return (uint16_t)gb->wram[idx] |
					((uint16_t)gb->wram[idx + 1] << 8);
			}
#endif
			{
				const uint16_t idx = pc - WRAM_0_ADDR;
				return (uint16_t)gb->wram[idx] |
					((uint16_t)gb->wram[idx + 1] << 8);
			}
		}
		break;

	case 0xE:
		if(pc <= 0xEFFE)
		{
			const uint16_t idx = pc - ECHO_ADDR;
			cpu_reg->pc.reg = pc + 2;
			return (uint16_t)gb->wram[idx] | ((uint16_t)gb->wram[idx + 1] << 8);
		}
		break;

	case 0xF:
		if(pc < OAM_ADDR)
		{
			if(pc <= 0xFDFE)
			{
				cpu_reg->pc.reg = pc + 2;
#if WALNUT_FULL_GBC_SUPPORT
				if(gb->cgb.cgbMode)
				{
					const uint16_t idx = (pc - 0x2000) - gb->cgb.wramBankOffset;
					return (uint16_t)gb->wram[idx] |
						((uint16_t)gb->wram[idx + 1] << 8);
				}
#endif
				{
					const uint16_t idx = pc - ECHO_ADDR;
					return (uint16_t)gb->wram[idx] |
						((uint16_t)gb->wram[idx + 1] << 8);
				}
			}
			break;
		}
		if(pc >= HRAM_ADDR && pc <= 0xFFFE)
		{
			const uint16_t idx = pc - IO_ADDR;
			cpu_reg->pc.reg = pc + 2;
			return (uint16_t)gb->hram_io[idx] |
				((uint16_t)gb->hram_io[idx + 1] << 8);
		}
		break;
	}

	return (uint16_t)__gb_fetch8_threaded_local_cpu(gb, cpu_reg) |
		((uint16_t)__gb_fetch8_threaded_local_cpu(gb, cpu_reg) << 8);
}

static inline uint8_t WALNUT_GB_HOT_ATTR
__gb_execute_cb_threaded_local_cpu(struct gb_s *gb,
		struct cpu_registers_s *cpu_reg)
{
	uint8_t inst_cycles;
	uint8_t cbop = __gb_fetch8_threaded_local_cpu(gb, cpu_reg);
	uint8_t r = (cbop & 0x7);
	uint8_t b = (cbop >> 3) & 0x7;
	uint8_t d = (cbop >> 3) & 0x1;
	uint8_t val;
	uint8_t writeback = 1;

	inst_cycles = 8;
	/* Add an additional 8 cycles to these sets of instructions. */
	switch(cbop & 0xC7)
	{
	case 0x06:
	case 0x86:
	case 0xC6:
		inst_cycles += 8;
		break;
	case 0x46:
		inst_cycles += 4;
		break;
	}

	switch(r)
	{
	case 0:
		val = cpu_reg->bc.bytes.b;
		break;

	case 1:
		val = cpu_reg->bc.bytes.c;
		break;

	case 2:
		val = cpu_reg->de.bytes.d;
		break;

	case 3:
		val = cpu_reg->de.bytes.e;
		break;

	case 4:
		val = cpu_reg->hl.bytes.h;
		break;

	case 5:
		val = cpu_reg->hl.bytes.l;
		break;

	case 6:
#if WALNUT_GB_THREADED_FAST_MEM
		val = __gb_read_threaded_fast(gb, cpu_reg->hl.reg);
#else
		val = __gb_read(gb, cpu_reg->hl.reg);
#endif
		break;

	default:
		val = cpu_reg->a;
		break;
	}

	switch(cbop >> 6)
	{
	case 0x0:
		cbop = (cbop >> 4) & 0x3;

		switch(cbop)
		{
		case 0x0: /* RdC R */
		case 0x1: /* Rd R */
			if(d) /* RRC R / RR R */
			{
				uint8_t temp = val;
				val = (val >> 1);
				val |= cbop ? (cpu_reg->f.f_bits.c << 7) : (temp << 7);
				cpu_reg->f.reg = 0;
				cpu_reg->f.f_bits.z = (val == 0x00);
				cpu_reg->f.f_bits.c = (temp & 0x01);
			}
			else /* RLC R / RL R */
			{
				uint8_t temp = val;
				val = (val << 1);
				val |= cbop ? cpu_reg->f.f_bits.c : (temp >> 7);
				cpu_reg->f.reg = 0;
				cpu_reg->f.f_bits.z = (val == 0x00);
				cpu_reg->f.f_bits.c = (temp >> 7);
			}

			break;

		case 0x2:
			if(d) /* SRA R */
			{
				cpu_reg->f.reg = 0;
				cpu_reg->f.f_bits.c = val & 0x01;
				val = (val >> 1) | (val & 0x80);
				cpu_reg->f.f_bits.z = (val == 0x00);
			}
			else /* SLA R */
			{
				cpu_reg->f.reg = 0;
				cpu_reg->f.f_bits.c = (val >> 7);
				val = val << 1;
				cpu_reg->f.f_bits.z = (val == 0x00);
			}

			break;

		case 0x3:
			if(d) /* SRL R */
			{
				cpu_reg->f.reg = 0;
				cpu_reg->f.f_bits.c = val & 0x01;
				val = val >> 1;
				cpu_reg->f.f_bits.z = (val == 0x00);
			}
			else /* SWAP R */
			{
				uint8_t temp = (val >> 4) & 0x0F;
				temp |= (val << 4) & 0xF0;
				val = temp;
				cpu_reg->f.reg = 0;
				cpu_reg->f.f_bits.z = (val == 0x00);
			}

			break;
		}

		break;

	case 0x1: /* BIT B, R */
		cpu_reg->f.f_bits.z = !((val >> b) & 0x1);
		cpu_reg->f.f_bits.n = 0;
		cpu_reg->f.f_bits.h = 1;
		writeback = 0;
		break;

	case 0x2: /* RES B, R */
		val &= (0xFE << b) | (0xFF >> (8 - b));
		break;

	case 0x3: /* SET B, R */
		val |= (0x1 << b);
		break;
	}

	if(writeback)
	{
		switch(r)
		{
		case 0:
			cpu_reg->bc.bytes.b = val;
			break;

		case 1:
			cpu_reg->bc.bytes.c = val;
			break;

		case 2:
			cpu_reg->de.bytes.d = val;
			break;

		case 3:
			cpu_reg->de.bytes.e = val;
			break;

		case 4:
			cpu_reg->hl.bytes.h = val;
			break;

		case 5:
			cpu_reg->hl.bytes.l = val;
			break;

		case 6:
#if WALNUT_GB_THREADED_FAST_MEM
			__gb_write_threaded_fast(gb, cpu_reg->hl.reg, val);
#else
			__gb_write(gb, cpu_reg->hl.reg, val);
#endif
			break;

		case 7:
			cpu_reg->a = val;
			break;
		}
	}
	return inst_cycles;
}

#if WALNUT_GB_FAST_STAT_POLL_ACTIVE || WALNUT_GB_FAST_POLL_WAIT_ACTIVE
static inline void
__gb_fast_poll_wait_maybe_threaded_local_cpu(struct gb_s *gb,
		struct cpu_registers_s *cpu_reg, uint16_t target_pc,
		uint16_t branch_pc, uint8_t branch_opcode)
{
	/* Reject forward and non-ROM branches before syncing CPU state. */
	if(target_pc >= branch_pc || target_pc >= VRAM_ADDR)
		return;
	gb->cpu_reg = *cpu_reg;
	__gb_fast_poll_wait_maybe(gb, target_pc, branch_pc, branch_opcode);
	*cpu_reg = gb->cpu_reg;
}
#endif

#if WALNUT_GB_FAST_STAT_POLL_ACTIVE
static inline uint8_t
__gb_fast_stat_poll_read_threaded_local_cpu(struct gb_s *gb,
		struct cpu_registers_s *cpu_reg, uint16_t stat_read_pc,
		uint16_t next_pc)
{
	gb->cpu_reg = *cpu_reg;
	const uint8_t value = __gb_fast_stat_poll_read(gb, stat_read_pc, next_pc);
	*cpu_reg = gb->cpu_reg;
	return value;
}

static inline uint8_t
__gb_fast_stat_poll_c_read_threaded_local_cpu(struct gb_s *gb,
		struct cpu_registers_s *cpu_reg, uint16_t stat_read_pc,
		uint16_t next_pc)
{
	gb->cpu_reg = *cpu_reg;
	const uint8_t value = __gb_fast_stat_poll_c_read(gb, stat_read_pc, next_pc);
	*cpu_reg = gb->cpu_reg;
	return value;
}
#endif
#endif

/**
 * Internal function used to step the CPU twice (dual fetch/16-bit).
 */
static inline void __gb_step_cpu(struct gb_s *gb)
{
	uint16_t oppair;
	uint8_t opcode;
	uint_fast16_t inst_cycles;
	static const uint8_t op_cycles[0x100] =
	{
		/* *INDENT-OFF* */
		/*0 1 2  3  4  5  6  7  8  9  A  B  C  D  E  F	*/
		4,12, 8, 8, 4, 4, 8, 4,20, 8, 8, 8, 4, 4, 8, 4,	/* 0x00 */
		4,12, 8, 8, 4, 4, 8, 4,12, 8, 8, 8, 4, 4, 8, 4,	/* 0x10 */
		8,12, 8, 8, 4, 4, 8, 4, 8, 8, 8, 8, 4, 4, 8, 4,	/* 0x20 */
		8,12, 8, 8,12,12,12, 4, 8, 8, 8, 8, 4, 4, 8, 4,	/* 0x30 */
		4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,	/* 0x40 */
		4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,	/* 0x50 */
		4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,	/* 0x60 */
		8, 8, 8, 8, 8, 8, 4, 8, 4, 4, 4, 4, 4, 4, 8, 4, /* 0x70 */
		4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,	/* 0x80 */
		4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,	/* 0x90 */
		4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,	/* 0xA0 */
		4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,	/* 0xB0 */
		8,12,12,16,12,16, 8,16, 8,16,12, 8,12,24, 8,16,	/* 0xC0 */
		8,12,12, 0,12,16, 8,16, 8,16,12, 0,12, 0, 8,16,	/* 0xD0 */
		12,12,8, 0, 0,16, 8,16,16, 4,16, 0, 0, 0, 8,16,	/* 0xE0 */
		12,12,8, 4, 0,16, 8,16,12, 8,16, 4, 0, 0, 8,16	/* 0xF0 */
		/* *INDENT-ON* */
	};
	static const uint_fast16_t TAC_CYCLES[4] = {1024, 16, 64, 256};

	/* Handle interrupts */
	/* If gb_halt is positive, then an interrupt must have occurred by the
	 * time we reach here, because on HALT, we jump to the next interrupt
	 * immediately. */
	while((gb->gb_halt || gb->gb_ime) && __gb_effective_pending_interrupts(gb))
	{
		gb->gb_halt = false;

		if(!gb->gb_ime)
			break;

		/* Disable interrupts */
		gb->gb_ime = false;

		/* Push Program Counter */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);

		/* Call interrupt handler if required. */
		if(__gb_effective_pending_interrupts(gb) & VBLANK_INTR)
		{
			gb->cpu_reg.pc.reg = VBLANK_INTR_ADDR;
			gb->hram_io[IO_IF] ^= VBLANK_INTR;
		}
		else if(__gb_effective_pending_interrupts(gb) & LCDC_INTR)
		{
			gb->cpu_reg.pc.reg = LCDC_INTR_ADDR;
			gb->hram_io[IO_IF] ^= LCDC_INTR;
		}
		else if(__gb_effective_pending_interrupts(gb) & TIMER_INTR)
		{
			gb->cpu_reg.pc.reg = TIMER_INTR_ADDR;
			gb->hram_io[IO_IF] ^= TIMER_INTR;
		}
		else if(__gb_effective_pending_interrupts(gb) & SERIAL_INTR)
		{
			gb->cpu_reg.pc.reg = SERIAL_INTR_ADDR;
			gb->hram_io[IO_IF] ^= SERIAL_INTR;
		}
		else if(__gb_effective_pending_interrupts(gb) & CONTROL_INTR)
		{
			gb->cpu_reg.pc.reg = CONTROL_INTR_ADDR;
			gb->hram_io[IO_IF] ^= CONTROL_INTR;
		}

		break;
	}

	/* Obtain opcode */

	const uint16_t hotpc_fetch_pc = gb->cpu_reg.pc.reg;
	oppair = __gb_fetch_opcode_pair(gb, gb->cpu_reg.pc.reg++);
	opcode = (uint8_t)oppair; // auto-truncate
#if (WALNUT_GB_SAFE_DUALFETCH_DMA || WALNUT_GB_SAFE_DUALFETCH_MBC)
  gb->prefetch_invalid=false;
#endif
	inst_cycles = op_cycles[opcode];

#if WALNUT_GB_DUALFETCH_HOT_OPS
	switch(opcode)
	{
	case 0x13: /* INC DE */
		gb->cpu_reg.de.reg++;
		goto dualfetch_post_opcode;

	case 0x20: /* JR NZ, imm */
		gb->cpu_reg.pc.reg++;
		if(!gb->cpu_reg.f.f_bits.z)
		{
			gb->cpu_reg.pc.reg += (int8_t)(uint8_t)(oppair >> 8);
			inst_cycles += 4;
			__gb_fast_poll_wait_maybe(gb, gb->cpu_reg.pc.reg, hotpc_fetch_pc, 0x20);
		}
		goto dualfetch_post_opcode;

	case 0x21: /* LD HL, imm */
		gb->cpu_reg.hl.bytes.l = (uint8_t)(oppair >> 8);
		gb->cpu_reg.hl.bytes.h = __gb_read(gb, gb->cpu_reg.pc.reg + 1);
		gb->cpu_reg.pc.reg += 2;
		goto dualfetch_post_opcode;

	case 0x28: /* JR Z, imm */
		gb->cpu_reg.pc.reg++;
		if(gb->cpu_reg.f.f_bits.z)
		{
			gb->cpu_reg.pc.reg += (int8_t)(uint8_t)(oppair >> 8);
			inst_cycles += 4;
			__gb_fast_poll_wait_maybe(gb, gb->cpu_reg.pc.reg, hotpc_fetch_pc, 0x28);
		}
		goto dualfetch_post_opcode;

	case 0x2A: /* LD A, (HL+) */
		gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.hl.reg++);
		goto dualfetch_post_opcode;

	case 0x7E: /* LD A, (HL) */
		gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.hl.reg);
		goto dualfetch_post_opcode;

	case 0xA7: /* AND A */
		gb->cpu_reg.f.reg = 0x20;
		gb->cpu_reg.f.f_bits.z = (gb->cpu_reg.a == 0x00);
		goto dualfetch_post_opcode;

	default:
		break;
	}
#endif

	/* Execute opcode */
	switch(opcode)
	{
	case 0x00: /* NOP */
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x01: /* LD BC, imm */
#if WALNUT_GB_16_BIT_OPS_DUALFETCH
    gb->cpu_reg.bc.bytes.c = (uint8_t)(oppair >> 8); // C was already partially loaded in oppair
    oppair = __gb_read16(gb, gb->cpu_reg.pc.reg + 1); // Read 16-bit immediate starting from PC+1
    gb->cpu_reg.bc.bytes.b = (uint8_t)(oppair);// Store lower byte of immediate into B
    gb->cpu_reg.pc.reg += 2;// Increment PC by 2 to skip over the 16-bit immediate
    // Prefetch next opcode from upper byte of read16
    opcode = (uint8_t)(oppair >> 8);
#else
    // Original 8-bit fetch path
    gb->cpu_reg.bc.bytes.c = (uint8_t)(oppair >> 8);
    gb->cpu_reg.pc.reg++;
    gb->cpu_reg.bc.bytes.b = __gb_fetch8(gb);
    opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
#endif
		break;
	case 0x02: /* LD (BC), A */
		__gb_write(gb, gb->cpu_reg.bc.reg, gb->cpu_reg.a);
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg); // GTA SAFE TEST
#else
		opcode = (uint8_t)(oppair >> 8);
#endif
		break;

	case 0x03: /* INC BC */
		gb->cpu_reg.bc.reg++;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x04: /* INC B */
		WGB_INSTR_INC_R8(gb->cpu_reg.bc.bytes.b);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x05: /* DEC B */
		WGB_INSTR_DEC_R8(gb->cpu_reg.bc.bytes.b);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x06: /* LD B, imm */
		gb->cpu_reg.bc.bytes.b = (uint8_t)(oppair >> 8);
	  gb->cpu_reg.pc.reg++;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;
	case 0x07: /* RLCA */
		gb->cpu_reg.a = (gb->cpu_reg.a << 1) | (gb->cpu_reg.a >> 7);
		gb->cpu_reg.f.reg = 0;
		gb->cpu_reg.f.f_bits.c = (gb->cpu_reg.a & 0x01);
		opcode = (uint8_t)(oppair >> 8);
		break;
	case 0x08: /* LD (imm), SP */
	{
#if WALNUT_GB_16_BIT_OPS_DUALFETCH
		uint8_t l = (uint8_t)(oppair >> 8);
		gb->cpu_reg.pc.reg++;
    oppair = __gb_read16(gb, gb->cpu_reg.pc.reg++);
		uint8_t h = oppair;
		uint16_t temp = WALNUT_GB_U8_TO_U16(h, l);
		__gb_write16(gb,temp,gb->cpu_reg.sp.reg);
    // gb->cpu_reg.pc.reg+=2;
		opcode = oppair >> 8;
#else
    uint8_t l = (uint8_t)(oppair >> 8);
	  gb->cpu_reg.pc.reg++;
    uint8_t h = __gb_fetch8(gb);
    uint16_t temp = WALNUT_GB_U8_TO_U16(h, l);
    __gb_write(gb, temp++, gb->cpu_reg.sp.bytes.p);
    __gb_write(gb, temp, gb->cpu_reg.sp.bytes.s);
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
#endif
    break;
	}

	case 0x09: /* ADD HL, BC */
	{
		uint_fast32_t temp = gb->cpu_reg.hl.reg + gb->cpu_reg.bc.reg;
		gb->cpu_reg.f.f_bits.n = 0;
		gb->cpu_reg.f.f_bits.h =
			(temp ^ gb->cpu_reg.hl.reg ^ gb->cpu_reg.bc.reg) & 0x1000 ? 1 : 0;
		gb->cpu_reg.f.f_bits.c = (temp & 0xFFFF0000) ? 1 : 0;
		gb->cpu_reg.hl.reg = (temp & 0x0000FFFF);
		opcode = (uint8_t)(oppair >> 8);
		break;
	}

	case 0x0A: /* LD A, (BC) */
		gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.bc.reg);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x0B: /* DEC BC */
		gb->cpu_reg.bc.reg--;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x0C: /* INC C */
		WGB_INSTR_INC_R8(gb->cpu_reg.bc.bytes.c);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x0D: /* DEC C */
		WGB_INSTR_DEC_R8(gb->cpu_reg.bc.bytes.c);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x0E: /* LD C, imm */
		gb->cpu_reg.bc.bytes.c = (uint8_t)(oppair >> 8);
		gb->cpu_reg.pc.reg++;
	  opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0x0F: /* RRCA */
		gb->cpu_reg.f.reg = 0;
		gb->cpu_reg.f.f_bits.c = gb->cpu_reg.a & 0x01;
		gb->cpu_reg.a = (gb->cpu_reg.a >> 1) | (gb->cpu_reg.a << 7);
		opcode = (uint8_t)(oppair >> 8);
		break;
	case 0x10: /* STOP */
		//gb->gb_halt = true;
#if WALNUT_FULL_GBC_SUPPORT
		if(gb->cgb.cgbMode & gb->cgb.doubleSpeedPrep)
		{
			gb->cgb.doubleSpeedPrep = 0;
			gb->cgb.doubleSpeed ^= 1;
			__gb_cpu_timing_reschedule(gb);
		}
#endif
		/* STOP is two bytes; skip the padding byte. */
		gb->cpu_reg.pc.reg++;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0x11: /* LD DE, imm */
#if WALNUT_GB_16_BIT_OPS_DUALFETCH
    gb->cpu_reg.de.bytes.e = (uint8_t)(oppair >> 8);
	  gb->cpu_reg.pc.reg++;
		oppair = __gb_read16(gb, gb->cpu_reg.pc.reg++);
    gb->cpu_reg.de.bytes.d = oppair;
		opcode = oppair >> 8;
#else
    gb->cpu_reg.de.bytes.e = (uint8_t)(oppair >> 8);
	  gb->cpu_reg.pc.reg++;
    gb->cpu_reg.de.bytes.d = __gb_fetch8(gb);
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
#endif
		break;
	case 0x12: /* LD (DE), A */
		__gb_write(gb, gb->cpu_reg.de.reg, gb->cpu_reg.a);
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg); // GTA SAFE TEST
#else
		opcode = (uint8_t)(oppair >> 8);
#endif
		break;

	case 0x13: /* INC DE */
		gb->cpu_reg.de.reg++;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x14: /* INC D */
		WGB_INSTR_INC_R8(gb->cpu_reg.de.bytes.d);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x15: /* DEC D */
		WGB_INSTR_DEC_R8(gb->cpu_reg.de.bytes.d);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x16: /* LD D, imm */
		gb->cpu_reg.de.bytes.d = (uint8_t)(oppair >> 8);
	  gb->cpu_reg.pc.reg++;
	  opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0x17: /* RLA */
	{
		uint8_t temp = gb->cpu_reg.a;
		gb->cpu_reg.a = (gb->cpu_reg.a << 1) | gb->cpu_reg.f.f_bits.c;
		gb->cpu_reg.f.reg = 0;
		gb->cpu_reg.f.f_bits.c = (temp >> 7) & 0x01;
		opcode = (uint8_t)(oppair >> 8);
		break;
	}

	case 0x18: /* JR imm */
	{
		int8_t temp = (int8_t) (uint8_t)(oppair >> 8);
	  gb->cpu_reg.pc.reg++;
		gb->cpu_reg.pc.reg += temp;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;
	}
	case 0x19: /* ADD HL, DE */
	{
		uint_fast32_t temp = gb->cpu_reg.hl.reg + gb->cpu_reg.de.reg;
		gb->cpu_reg.f.f_bits.n = 0;
		gb->cpu_reg.f.f_bits.h =
			(temp ^ gb->cpu_reg.hl.reg ^ gb->cpu_reg.de.reg) & 0x1000 ? 1 : 0;
		gb->cpu_reg.f.f_bits.c = (temp & 0xFFFF0000) ? 1 : 0;
		gb->cpu_reg.hl.reg = (temp & 0x0000FFFF);
		opcode = (uint8_t)(oppair >> 8);
		break;
	}
	case 0x1A: /* LD A, (DE) */
		gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.de.reg);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x1B: /* DEC DE */
		gb->cpu_reg.de.reg--;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x1C: /* INC E */
		WGB_INSTR_INC_R8(gb->cpu_reg.de.bytes.e);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x1D: /* DEC E */
		WGB_INSTR_DEC_R8(gb->cpu_reg.de.bytes.e);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x1E: /* LD E, imm */
		gb->cpu_reg.de.bytes.e =  (uint8_t)(oppair >> 8);
	  gb->cpu_reg.pc.reg++;
	  opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0x1F: /* RRA */
	{
		uint8_t temp = gb->cpu_reg.a;
		gb->cpu_reg.a = gb->cpu_reg.a >> 1 | (gb->cpu_reg.f.f_bits.c << 7);
		gb->cpu_reg.f.reg = 0;
		gb->cpu_reg.f.f_bits.c = temp & 0x1;
		opcode = (uint8_t)(oppair >> 8);
		break;
	}

	case 0x20: /* JR NZ, imm */
		if(!gb->cpu_reg.f.f_bits.z)
		{
			int8_t temp = (int8_t) (oppair >> 8);
	    gb->cpu_reg.pc.reg++;
			gb->cpu_reg.pc.reg += temp;
			inst_cycles += 4;
			__gb_fast_poll_wait_maybe(gb, gb->cpu_reg.pc.reg, hotpc_fetch_pc, 0x20);
		}
		else
			gb->cpu_reg.pc.reg++;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0x21: /* LD HL, imm */
#if WALNUT_GB_16_BIT_OPS_DUALFETCH
		gb->cpu_reg.hl.bytes.l =(uint8_t)(oppair >> 8);
		gb->cpu_reg.pc.reg++;
		oppair = __gb_read16(gb, gb->cpu_reg.pc.reg++);
    gb->cpu_reg.hl.bytes.h = oppair;
		opcode = oppair >> 8;
#else
    gb->cpu_reg.hl.bytes.l =(uint8_t)(oppair >> 8);
	  gb->cpu_reg.pc.reg++;
    gb->cpu_reg.hl.bytes.h = __gb_fetch8(gb);
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
#endif
		break;

	case 0x22: /* LDI (HL), A */
		__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.a);
		gb->cpu_reg.hl.reg++;
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg); // GTA SAFE TEST
#else
		opcode = (uint8_t)(oppair >> 8);
#endif
		break;

	case 0x23: /* INC HL */
		gb->cpu_reg.hl.reg++;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x24: /* INC H */
		WGB_INSTR_INC_R8(gb->cpu_reg.hl.bytes.h);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x25: /* DEC H */
		WGB_INSTR_DEC_R8(gb->cpu_reg.hl.bytes.h);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x26: /* LD H, imm */
		gb->cpu_reg.hl.bytes.h =(uint8_t)(oppair >> 8);
	  gb->cpu_reg.pc.reg++;
	  opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0x27: /* DAA */
	{
		/* The following is from SameBoy. MIT License. */
		int16_t a = gb->cpu_reg.a;

		if(gb->cpu_reg.f.f_bits.n)
		{
			if(gb->cpu_reg.f.f_bits.h)
				a = (a - 0x06) & 0xFF;

			if(gb->cpu_reg.f.f_bits.c)
				a -= 0x60;
		}
		else
		{
			if(gb->cpu_reg.f.f_bits.h || (a & 0x0F) > 9)
				a += 0x06;

			if(gb->cpu_reg.f.f_bits.c || a > 0x9F)
				a += 0x60;
		}

		if((a & 0x100) == 0x100)
			gb->cpu_reg.f.f_bits.c = 1;

		gb->cpu_reg.a = (uint8_t)a;
		gb->cpu_reg.f.f_bits.z = (gb->cpu_reg.a == 0);
		gb->cpu_reg.f.f_bits.h = 0;
		opcode = (uint8_t)(oppair >> 8);
		break;
	}

	case 0x28: /* JR Z, imm */
		if(gb->cpu_reg.f.f_bits.z)
		{
			int8_t temp = (int8_t) (oppair >> 8);
	    gb->cpu_reg.pc.reg++;
			gb->cpu_reg.pc.reg += temp;
			inst_cycles += 4;
			__gb_fast_poll_wait_maybe(gb, gb->cpu_reg.pc.reg, hotpc_fetch_pc, 0x28);
		}
		else
			gb->cpu_reg.pc.reg++;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0x29: /* ADD HL, HL */
	{
		gb->cpu_reg.f.f_bits.c = (gb->cpu_reg.hl.reg & 0x8000) > 0;
		gb->cpu_reg.hl.reg <<= 1;
		gb->cpu_reg.f.f_bits.n = 0;
		gb->cpu_reg.f.f_bits.h = (gb->cpu_reg.hl.reg & 0x1000) > 0;
		opcode = (uint8_t)(oppair >> 8);
		break;
	}

	case 0x2A: /* LD A, (HL+) */
		gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.hl.reg++);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x2B: /* DEC HL */
		gb->cpu_reg.hl.reg--;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x2C: /* INC L */
		WGB_INSTR_INC_R8(gb->cpu_reg.hl.bytes.l);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x2D: /* DEC L */
		WGB_INSTR_DEC_R8(gb->cpu_reg.hl.bytes.l);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x2E: /* LD L, imm */
		gb->cpu_reg.hl.bytes.l = (uint8_t)(oppair >> 8);
	  gb->cpu_reg.pc.reg++;
	  opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0x2F: /* CPL */
		gb->cpu_reg.a = ~gb->cpu_reg.a;
		gb->cpu_reg.f.f_bits.n = 1;
		gb->cpu_reg.f.f_bits.h = 1;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x30: /* JR NC, imm */
		if(!gb->cpu_reg.f.f_bits.c)
		{
			int8_t temp = (int8_t) (oppair >> 8);
	    gb->cpu_reg.pc.reg++;
			gb->cpu_reg.pc.reg += temp;
			inst_cycles += 4;
#if WALNUT_GB_FAST_POLL_WAIT_ACTIVE
			if(__gb_fast_poll_wait_carry_ly_prefilter(gb,
					gb->cpu_reg.pc.reg, hotpc_fetch_pc))
				__gb_fast_poll_wait_maybe(gb, gb->cpu_reg.pc.reg,
					hotpc_fetch_pc, 0x30);
#endif
		}
		else
			gb->cpu_reg.pc.reg++;
    opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0x31: /* LD SP, imm */
#if WALNUT_GB_16_BIT_OPS_DUALFETCH
		gb->cpu_reg.sp.bytes.p = (uint8_t)(oppair >> 8);
		gb->cpu_reg.pc.reg ++;
    oppair = __gb_read16(gb, gb->cpu_reg.pc.reg++);
		gb->cpu_reg.sp.bytes.s = oppair; // auto-truncated
		opcode = oppair >> 8;
#else
    gb->cpu_reg.sp.bytes.p = (uint8_t)(oppair >> 8);
	  gb->cpu_reg.pc.reg++;
    gb->cpu_reg.sp.bytes.s = __gb_fetch8(gb);
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
#endif
		break;

	case 0x32: /* LD (HL), A */
		__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.a);
		gb->cpu_reg.hl.reg--;
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg); // GTA SAFE TEST
#else
		opcode = (uint8_t)(oppair >> 8);
#endif
		break;

	case 0x33: /* INC SP */
		gb->cpu_reg.sp.reg++;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x34: /* INC (HL) */
	{
		uint8_t temp = __gb_read(gb, gb->cpu_reg.hl.reg);
		WGB_INSTR_INC_R8(temp);
		__gb_write(gb, gb->cpu_reg.hl.reg, temp);
		opcode = (uint8_t)(oppair >> 8);
		break;
	}

	case 0x35: /* DEC (HL) */
	{
		uint8_t temp = __gb_read(gb, gb->cpu_reg.hl.reg);
		WGB_INSTR_DEC_R8(temp);
		__gb_write(gb, gb->cpu_reg.hl.reg, temp);
		opcode = (uint8_t)(oppair >> 8);
		break;
	}

	case 0x36: /* LD (HL), imm */
		__gb_write(gb, gb->cpu_reg.hl.reg,(uint8_t)(oppair >> 8));
		gb->cpu_reg.pc.reg++;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0x37: /* SCF */
		gb->cpu_reg.f.f_bits.n = 0;
		gb->cpu_reg.f.f_bits.h = 0;
		gb->cpu_reg.f.f_bits.c = 1;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x38: /* JR C, imm */
		if(gb->cpu_reg.f.f_bits.c)
		{
			int8_t temp = (int8_t)(oppair >> 8);
	    gb->cpu_reg.pc.reg++;
			gb->cpu_reg.pc.reg += temp;
			inst_cycles += 4;
#if WALNUT_GB_FAST_POLL_WAIT_ACTIVE
			if(__gb_fast_poll_wait_carry_ly_prefilter(gb,
					gb->cpu_reg.pc.reg, hotpc_fetch_pc))
				__gb_fast_poll_wait_maybe(gb, gb->cpu_reg.pc.reg,
					hotpc_fetch_pc, 0x38);
#endif
		}
		else
			gb->cpu_reg.pc.reg++;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0x39: /* ADD HL, SP */
	{
		uint_fast32_t temp = gb->cpu_reg.hl.reg + gb->cpu_reg.sp.reg;
		gb->cpu_reg.f.f_bits.n = 0;
		gb->cpu_reg.f.f_bits.h =
			((gb->cpu_reg.hl.reg & 0xFFF) + (gb->cpu_reg.sp.reg & 0xFFF)) & 0x1000 ? 1 : 0;
		gb->cpu_reg.f.f_bits.c = temp & 0x10000 ? 1 : 0;
		gb->cpu_reg.hl.reg = (uint16_t)temp;
		opcode = (uint8_t)(oppair >> 8);
		break;
	}

	case 0x3A: /* LD A, (HL) */
		gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.hl.reg--);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x3B: /* DEC SP */
		gb->cpu_reg.sp.reg--;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x3C: /* INC A */
		WGB_INSTR_INC_R8(gb->cpu_reg.a);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x3D: /* DEC A */
		WGB_INSTR_DEC_R8(gb->cpu_reg.a);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x3E: /* LD A, imm */
		gb->cpu_reg.a = (uint8_t)(oppair >> 8);
	  gb->cpu_reg.pc.reg++;
	  opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0x3F: /* CCF */
		gb->cpu_reg.f.f_bits.n = 0;
		gb->cpu_reg.f.f_bits.h = 0;
		gb->cpu_reg.f.f_bits.c = ~gb->cpu_reg.f.f_bits.c;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x40: /* LD B, B */
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x41: /* LD B, C */
		gb->cpu_reg.bc.bytes.b = gb->cpu_reg.bc.bytes.c;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x42: /* LD B, D */
		gb->cpu_reg.bc.bytes.b = gb->cpu_reg.de.bytes.d;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x43: /* LD B, E */
		gb->cpu_reg.bc.bytes.b = gb->cpu_reg.de.bytes.e;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x44: /* LD B, H */
		gb->cpu_reg.bc.bytes.b = gb->cpu_reg.hl.bytes.h;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x45: /* LD B, L */
		gb->cpu_reg.bc.bytes.b = gb->cpu_reg.hl.bytes.l;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x46: /* LD B, (HL) */
		gb->cpu_reg.bc.bytes.b = __gb_read(gb, gb->cpu_reg.hl.reg);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x47: /* LD B, A */
		gb->cpu_reg.bc.bytes.b = gb->cpu_reg.a;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x48: /* LD C, B */
		gb->cpu_reg.bc.bytes.c = gb->cpu_reg.bc.bytes.b;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x49: /* LD C, C */
	  opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x4A: /* LD C, D */
		gb->cpu_reg.bc.bytes.c = gb->cpu_reg.de.bytes.d;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x4B: /* LD C, E */
		gb->cpu_reg.bc.bytes.c = gb->cpu_reg.de.bytes.e;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x4C: /* LD C, H */
		gb->cpu_reg.bc.bytes.c = gb->cpu_reg.hl.bytes.h;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x4D: /* LD C, L */
		gb->cpu_reg.bc.bytes.c = gb->cpu_reg.hl.bytes.l;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x4E: /* LD C, (HL) */
		gb->cpu_reg.bc.bytes.c = __gb_read(gb, gb->cpu_reg.hl.reg);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x4F: /* LD C, A */
		gb->cpu_reg.bc.bytes.c = gb->cpu_reg.a;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x50: /* LD D, B */
		gb->cpu_reg.de.bytes.d = gb->cpu_reg.bc.bytes.b;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x51: /* LD D, C */
		gb->cpu_reg.de.bytes.d = gb->cpu_reg.bc.bytes.c;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x52: /* LD D, D */
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x53: /* LD D, E */
		gb->cpu_reg.de.bytes.d = gb->cpu_reg.de.bytes.e;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x54: /* LD D, H */
		gb->cpu_reg.de.bytes.d = gb->cpu_reg.hl.bytes.h;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x55: /* LD D, L */
		gb->cpu_reg.de.bytes.d = gb->cpu_reg.hl.bytes.l;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x56: /* LD D, (HL) */
		gb->cpu_reg.de.bytes.d = __gb_read(gb, gb->cpu_reg.hl.reg);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x57: /* LD D, A */
		gb->cpu_reg.de.bytes.d = gb->cpu_reg.a;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x58: /* LD E, B */
		gb->cpu_reg.de.bytes.e = gb->cpu_reg.bc.bytes.b;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x59: /* LD E, C */
		gb->cpu_reg.de.bytes.e = gb->cpu_reg.bc.bytes.c;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x5A: /* LD E, D */
		gb->cpu_reg.de.bytes.e = gb->cpu_reg.de.bytes.d;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x5B: /* LD E, E */
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x5C: /* LD E, H */
		gb->cpu_reg.de.bytes.e = gb->cpu_reg.hl.bytes.h;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x5D: /* LD E, L */
		gb->cpu_reg.de.bytes.e = gb->cpu_reg.hl.bytes.l;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x5E: /* LD E, (HL) */
		gb->cpu_reg.de.bytes.e = __gb_read(gb, gb->cpu_reg.hl.reg);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x5F: /* LD E, A */
		gb->cpu_reg.de.bytes.e = gb->cpu_reg.a;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x60: /* LD H, B */
		gb->cpu_reg.hl.bytes.h = gb->cpu_reg.bc.bytes.b;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x61: /* LD H, C */
		gb->cpu_reg.hl.bytes.h = gb->cpu_reg.bc.bytes.c;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x62: /* LD H, D */
		gb->cpu_reg.hl.bytes.h = gb->cpu_reg.de.bytes.d;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x63: /* LD H, E */
		gb->cpu_reg.hl.bytes.h = gb->cpu_reg.de.bytes.e;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x64: /* LD H, H */
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x65: /* LD H, L */
		gb->cpu_reg.hl.bytes.h = gb->cpu_reg.hl.bytes.l;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x66: /* LD H, (HL) */
		gb->cpu_reg.hl.bytes.h = __gb_read(gb, gb->cpu_reg.hl.reg);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x67: /* LD H, A */
		gb->cpu_reg.hl.bytes.h = gb->cpu_reg.a;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x68: /* LD L, B */
		gb->cpu_reg.hl.bytes.l = gb->cpu_reg.bc.bytes.b;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x69: /* LD L, C */
		gb->cpu_reg.hl.bytes.l = gb->cpu_reg.bc.bytes.c;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x6A: /* LD L, D */
		gb->cpu_reg.hl.bytes.l = gb->cpu_reg.de.bytes.d;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x6B: /* LD L, E */
		gb->cpu_reg.hl.bytes.l = gb->cpu_reg.de.bytes.e;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x6C: /* LD L, H */
		gb->cpu_reg.hl.bytes.l = gb->cpu_reg.hl.bytes.h;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x6D: /* LD L, L */
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x6E: /* LD L, (HL) */
		gb->cpu_reg.hl.bytes.l = __gb_read(gb, gb->cpu_reg.hl.reg);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x6F: /* LD L, A */
		gb->cpu_reg.hl.bytes.l = gb->cpu_reg.a;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x70: /* LD (HL), B */
		__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.bc.bytes.b);
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg); // GTA SAFE TEST
#else
		opcode = (uint8_t)(oppair >> 8);
#endif
		break;

	case 0x71: /* LD (HL), C */
		__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.bc.bytes.c);
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg); // GTA SAFE TEST
#else
		opcode = (uint8_t)(oppair >> 8);
#endif
		break;

	case 0x72: /* LD (HL), D */
		__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.de.bytes.d);
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg); // GTA SAFE TEST
#else
		opcode = (uint8_t)(oppair >> 8);
#endif
		break;

	case 0x73: /* LD (HL), E */
		__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.de.bytes.e);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x74: /* LD (HL), H */
		__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.hl.bytes.h);
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg); // GTA SAFE TEST
#else
		opcode = (uint8_t)(oppair >> 8);
#endif
		break;

	case 0x75: /* LD (HL), L */
		__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.hl.bytes.l);
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg); // GTA SAFE TEST
#else
		opcode = (uint8_t)(oppair >> 8);
#endif
		break;

	case 0x76: /* HALT */
	{
		int_fast16_t halt_cycles = INT_FAST16_MAX;

		/* TODO: Emulate HALT bug? */
		gb->gb_halt = true;

		if(__gb_halt_fast_forward_to_vblank(gb))
		{
			inst_cycles = 0;
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
			opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
#else
			opcode = (uint8_t)(oppair >> 8);
#endif
			break;
		}

		if(gb->hram_io[IO_SC] & SERIAL_SC_TX_START)
		{
			int serial_cycles = SERIAL_CYCLES -
				gb->counter.serial_count;

			if(serial_cycles < halt_cycles)
				halt_cycles = serial_cycles;
		}

		if(gb->hram_io[IO_TAC] & IO_TAC_ENABLE_MASK)
		{
			int tac_cycles = TAC_CYCLES[gb->hram_io[IO_TAC] & IO_TAC_RATE_MASK] -
				gb->counter.tima_count;

			if(tac_cycles < halt_cycles)
				halt_cycles = tac_cycles;
		}

		if((gb->hram_io[IO_LCDC] & LCDC_ENABLE))
		{
			int lcd_cycles;

			/* If LCD is in HBlank, calculate the number of cycles
			 * until the end of HBlank and the start of mode 2 or
			 * mode 1. */
			if((gb->hram_io[IO_STAT] & STAT_MODE) == IO_STAT_MODE_HBLANK)
			{
				lcd_cycles = LCD_MODE0_HBLANK_MAX_DRUATION - gb->counter.lcd_count;
			}
			else if((gb->hram_io[IO_STAT] & STAT_MODE) == IO_STAT_MODE_OAM_SCAN)
			{
				lcd_cycles = LCD_MODE3_LCD_DRAW_MIN_DURATION - gb->counter.lcd_count;
			}
			else if((gb->hram_io[IO_STAT] & STAT_MODE) == IO_STAT_MODE_LCD_DRAW)
			{
				lcd_cycles = LCD_MODE0_HBLANK_MAX_DRUATION - gb->counter.lcd_count;
			}
			else
			{
				/* VBlank */
				lcd_cycles = LCD_LINE_CYCLES - gb->counter.lcd_count;
			}

			if(lcd_cycles < halt_cycles)
				halt_cycles = lcd_cycles;
		}

		/* Some halt cycles may already be very high, so make sure we
		 * don't underflow here. */
		if(halt_cycles <= 0)
			halt_cycles = 4;

		inst_cycles = (uint_fast16_t)halt_cycles;

#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg); // possibly changes mbc behaviour
#else
    opcode = (uint8_t)(oppair >> 8);
#endif
		break;
	}


	case 0x77: /* LD (HL), A */
		__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.a);
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg); // GTA SAFE TEST
#else
		opcode = (uint8_t)(oppair >> 8);
#endif
		break;

	case 0x78: /* LD A, B */
		gb->cpu_reg.a = gb->cpu_reg.bc.bytes.b;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x79: /* LD A, C */
		gb->cpu_reg.a = gb->cpu_reg.bc.bytes.c;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x7A: /* LD A, D */
		gb->cpu_reg.a = gb->cpu_reg.de.bytes.d;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x7B: /* LD A, E */
		gb->cpu_reg.a = gb->cpu_reg.de.bytes.e;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x7C: /* LD A, H */
		gb->cpu_reg.a = gb->cpu_reg.hl.bytes.h;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x7D: /* LD A, L */
		gb->cpu_reg.a = gb->cpu_reg.hl.bytes.l;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x7E: /* LD A, (HL) */
		gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.hl.reg);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x7F: /* LD A, A */
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x80: /* ADD A, B */
		WGB_INSTR_ADC_R8(gb->cpu_reg.bc.bytes.b, 0);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x81: /* ADD A, C */
		WGB_INSTR_ADC_R8(gb->cpu_reg.bc.bytes.c, 0);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x82: /* ADD A, D */
		WGB_INSTR_ADC_R8(gb->cpu_reg.de.bytes.d, 0);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x83: /* ADD A, E */
		WGB_INSTR_ADC_R8(gb->cpu_reg.de.bytes.e, 0);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x84: /* ADD A, H */
		WGB_INSTR_ADC_R8(gb->cpu_reg.hl.bytes.h, 0);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x85: /* ADD A, L */
		WGB_INSTR_ADC_R8(gb->cpu_reg.hl.bytes.l, 0);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x86: /* ADD A, (HL) */
		WGB_INSTR_ADC_R8(__gb_read(gb, gb->cpu_reg.hl.reg), 0);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x87: /* ADD A, A */
		WGB_INSTR_ADC_R8(gb->cpu_reg.a, 0);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x88: /* ADC A, B */
		WGB_INSTR_ADC_R8(gb->cpu_reg.bc.bytes.b, gb->cpu_reg.f.f_bits.c);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x89: /* ADC A, C */
		WGB_INSTR_ADC_R8(gb->cpu_reg.bc.bytes.c, gb->cpu_reg.f.f_bits.c);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x8A: /* ADC A, D */
		WGB_INSTR_ADC_R8(gb->cpu_reg.de.bytes.d, gb->cpu_reg.f.f_bits.c);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x8B: /* ADC A, E */
		WGB_INSTR_ADC_R8(gb->cpu_reg.de.bytes.e, gb->cpu_reg.f.f_bits.c);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x8C: /* ADC A, H */
		WGB_INSTR_ADC_R8(gb->cpu_reg.hl.bytes.h, gb->cpu_reg.f.f_bits.c);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x8D: /* ADC A, L */
		WGB_INSTR_ADC_R8(gb->cpu_reg.hl.bytes.l, gb->cpu_reg.f.f_bits.c);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x8E: /* ADC A, (HL) */
		WGB_INSTR_ADC_R8(__gb_read(gb, gb->cpu_reg.hl.reg), gb->cpu_reg.f.f_bits.c);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x8F: /* ADC A, A */
		WGB_INSTR_ADC_R8(gb->cpu_reg.a, gb->cpu_reg.f.f_bits.c);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x90: /* SUB B */
		WGB_INSTR_SBC_R8(gb->cpu_reg.bc.bytes.b, 0);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x91: /* SUB C */
		WGB_INSTR_SBC_R8(gb->cpu_reg.bc.bytes.c, 0);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x92: /* SUB D */
		WGB_INSTR_SBC_R8(gb->cpu_reg.de.bytes.d, 0);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x93: /* SUB E */
		WGB_INSTR_SBC_R8(gb->cpu_reg.de.bytes.e, 0);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x94: /* SUB H */
		WGB_INSTR_SBC_R8(gb->cpu_reg.hl.bytes.h, 0);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x95: /* SUB L */
		WGB_INSTR_SBC_R8(gb->cpu_reg.hl.bytes.l, 0);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x96: /* SUB (HL) */
		WGB_INSTR_SBC_R8(__gb_read(gb, gb->cpu_reg.hl.reg), 0);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x97: /* SUB A */
		gb->cpu_reg.a = 0;
		gb->cpu_reg.f.reg = 0;
		gb->cpu_reg.f.f_bits.z = 1;
		gb->cpu_reg.f.f_bits.n = 1;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x98: /* SBC A, B */
		WGB_INSTR_SBC_R8(gb->cpu_reg.bc.bytes.b, gb->cpu_reg.f.f_bits.c);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x99: /* SBC A, C */
		WGB_INSTR_SBC_R8(gb->cpu_reg.bc.bytes.c, gb->cpu_reg.f.f_bits.c);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x9A: /* SBC A, D */
		WGB_INSTR_SBC_R8(gb->cpu_reg.de.bytes.d, gb->cpu_reg.f.f_bits.c);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x9B: /* SBC A, E */
		WGB_INSTR_SBC_R8(gb->cpu_reg.de.bytes.e, gb->cpu_reg.f.f_bits.c);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x9C: /* SBC A, H */
		WGB_INSTR_SBC_R8(gb->cpu_reg.hl.bytes.h, gb->cpu_reg.f.f_bits.c);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x9D: /* SBC A, L */
		WGB_INSTR_SBC_R8(gb->cpu_reg.hl.bytes.l, gb->cpu_reg.f.f_bits.c);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x9E: /* SBC A, (HL) */
		WGB_INSTR_SBC_R8(__gb_read(gb, gb->cpu_reg.hl.reg), gb->cpu_reg.f.f_bits.c);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0x9F: /* SBC A, A */
		gb->cpu_reg.a = gb->cpu_reg.f.f_bits.c ? 0xFF : 0x00;
		gb->cpu_reg.f.f_bits.z = !gb->cpu_reg.f.f_bits.c;
		gb->cpu_reg.f.f_bits.n = 1;
		gb->cpu_reg.f.f_bits.h = gb->cpu_reg.f.f_bits.c;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xA0: /* AND B */
		WGB_INSTR_AND_R8(gb->cpu_reg.bc.bytes.b);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xA1: /* AND C */
		WGB_INSTR_AND_R8(gb->cpu_reg.bc.bytes.c);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xA2: /* AND D */
		WGB_INSTR_AND_R8(gb->cpu_reg.de.bytes.d);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xA3: /* AND E */
		WGB_INSTR_AND_R8(gb->cpu_reg.de.bytes.e);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xA4: /* AND H */
		WGB_INSTR_AND_R8(gb->cpu_reg.hl.bytes.h);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xA5: /* AND L */
		WGB_INSTR_AND_R8(gb->cpu_reg.hl.bytes.l);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xA6: /* AND (HL) */
		WGB_INSTR_AND_R8(__gb_read(gb, gb->cpu_reg.hl.reg));
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xA7: /* AND A */
		WGB_INSTR_AND_R8(gb->cpu_reg.a);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xA8: /* XOR B */
		WGB_INSTR_XOR_R8(gb->cpu_reg.bc.bytes.b);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xA9: /* XOR C */
		WGB_INSTR_XOR_R8(gb->cpu_reg.bc.bytes.c);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xAA: /* XOR D */
		WGB_INSTR_XOR_R8(gb->cpu_reg.de.bytes.d);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xAB: /* XOR E */
		WGB_INSTR_XOR_R8(gb->cpu_reg.de.bytes.e);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xAC: /* XOR H */
		WGB_INSTR_XOR_R8(gb->cpu_reg.hl.bytes.h);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xAD: /* XOR L */
		WGB_INSTR_XOR_R8(gb->cpu_reg.hl.bytes.l);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xAE: /* XOR (HL) */
		WGB_INSTR_XOR_R8(__gb_read(gb, gb->cpu_reg.hl.reg));
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xAF: /* XOR A */
		WGB_INSTR_XOR_R8(gb->cpu_reg.a);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xB0: /* OR B */
		WGB_INSTR_OR_R8(gb->cpu_reg.bc.bytes.b);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xB1: /* OR C */
		WGB_INSTR_OR_R8(gb->cpu_reg.bc.bytes.c);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xB2: /* OR D */
		WGB_INSTR_OR_R8(gb->cpu_reg.de.bytes.d);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xB3: /* OR E */
		WGB_INSTR_OR_R8(gb->cpu_reg.de.bytes.e);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xB4: /* OR H */
		WGB_INSTR_OR_R8(gb->cpu_reg.hl.bytes.h);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xB5: /* OR L */
		WGB_INSTR_OR_R8(gb->cpu_reg.hl.bytes.l);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xB6: /* OR (HL) */
		WGB_INSTR_OR_R8(__gb_read(gb, gb->cpu_reg.hl.reg));
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xB7: /* OR A */
		WGB_INSTR_OR_R8(gb->cpu_reg.a);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xB8: /* CP B */
		WGB_INSTR_CP_R8(gb->cpu_reg.bc.bytes.b);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xB9: /* CP C */
		WGB_INSTR_CP_R8(gb->cpu_reg.bc.bytes.c);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xBA: /* CP D */
		WGB_INSTR_CP_R8(gb->cpu_reg.de.bytes.d);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xBB: /* CP E */
		WGB_INSTR_CP_R8(gb->cpu_reg.de.bytes.e);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xBC: /* CP H */
		WGB_INSTR_CP_R8(gb->cpu_reg.hl.bytes.h);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xBD: /* CP L */
		WGB_INSTR_CP_R8(gb->cpu_reg.hl.bytes.l);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xBE: /* CP (HL) */
		WGB_INSTR_CP_R8(__gb_read(gb, gb->cpu_reg.hl.reg));
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xBF: /* CP A */
		gb->cpu_reg.f.reg = 0;
		gb->cpu_reg.f.f_bits.z = 1;
		gb->cpu_reg.f.f_bits.n = 1;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xC0: /* RET NZ */
		if(!gb->cpu_reg.f.f_bits.z)
		{
        gb->cpu_reg.pc.reg = __gb_stack_pop16(gb, &gb->cpu_reg.sp.reg);
        inst_cycles += 12;
		}
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0xC1: /* POP BC */
    gb->cpu_reg.bc.reg = __gb_stack_pop16(gb, &gb->cpu_reg.sp.reg);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xC2: /* JP NZ, imm */
		if(!gb->cpu_reg.f.f_bits.z)
		{		// best without 16-bit read when used in first half of dual fetch chain
        uint8_t c = (uint8_t)(oppair >> 8);
				gb->cpu_reg.pc.reg++;
        uint8_t p = __gb_fetch8(gb);
        gb->cpu_reg.pc.bytes.c = c;
        gb->cpu_reg.pc.bytes.p = p;
			inst_cycles += 4;
		}
		else
			gb->cpu_reg.pc.reg += 2;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0xC3: /* JP imm */
		{ // best without 16-bit read when used in first half of dual fetch chain
    uint8_t c = (uint8_t)(oppair >> 8);
	  gb->cpu_reg.pc.reg++;
    uint8_t p = __gb_fetch8(gb);
    gb->cpu_reg.pc.bytes.c = c;
    gb->cpu_reg.pc.bytes.p = p;
		}
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0xC4: /* CALL NZ imm */
		if(!gb->cpu_reg.f.f_bits.z)
		{
			uint8_t c, p;
			c = (uint8_t)(oppair >> 8);
	    gb->cpu_reg.pc.reg++;
			p = __gb_fetch8(gb);
			__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
			gb->cpu_reg.pc.bytes.c = c;
			gb->cpu_reg.pc.bytes.p = p;
			inst_cycles += 12;
		}
		else
			gb->cpu_reg.pc.reg += 2;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0xC5: /* PUSH BC */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.bc.reg);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xC6: /* ADD A, imm */
	{
		uint8_t val = (uint8_t)(oppair >> 8);
	  gb->cpu_reg.pc.reg++;
		WGB_INSTR_ADC_R8(val, 0);
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;
	}

	case 0xC7: /* RST 0x0000 */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
		gb->cpu_reg.pc.reg = 0x0000;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0xC8: /* RET Z */
		if(gb->cpu_reg.f.f_bits.z)
		{
        gb->cpu_reg.pc.reg = __gb_stack_pop16(gb, &gb->cpu_reg.sp.reg);
			inst_cycles += 12;
			opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		}
		else
		  opcode=(uint8_t)(oppair >> 8);
		break;

	case 0xC9: /* RET */
	{
    gb->cpu_reg.pc.reg = __gb_stack_pop16(gb, &gb->cpu_reg.sp.reg);
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;
	}

	case 0xCA: /* JP Z, imm */
		if(gb->cpu_reg.f.f_bits.z)
		{
        uint8_t c = (uint8_t)(oppair >> 8);
				gb->cpu_reg.pc.reg++;
        uint8_t p = __gb_fetch8(gb);
        gb->cpu_reg.pc.bytes.c = c;
        gb->cpu_reg.pc.bytes.p = p;
			inst_cycles += 4;
		}
		else
			gb->cpu_reg.pc.reg += 2;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0xCB: /* CB INST */
		inst_cycles = __gb_execute_cb(gb);
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg); // can cb change pc??? * revise
		//opcode = (uint8_t)(oppair >> 8); // things stopped working for megaman, rtype dx, etc with chaining when I made this change unless cgabmaactive was checked and opcode reloaded with gb_read
		break;

	case 0xCC: /* CALL Z, imm */
		if(gb->cpu_reg.f.f_bits.z)
		{
			uint8_t c = (uint8_t)(oppair >> 8);
			gb->cpu_reg.pc.reg++;
			uint8_t p = __gb_fetch8(gb);
			__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
			gb->cpu_reg.pc.bytes.c = c;
			gb->cpu_reg.pc.bytes.p = p;
			inst_cycles += 12;
		}
		else
			gb->cpu_reg.pc.reg += 2;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0xCD: /* CALL imm */
	{
		uint8_t c = (uint8_t)(oppair >> 8);
		gb->cpu_reg.pc.reg++;
		uint8_t p = __gb_fetch8(gb);
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
		gb->cpu_reg.pc.bytes.c = c;
		gb->cpu_reg.pc.bytes.p = p;
	}
	opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
	break;

	case 0xCE: /* ADC A, imm */
	{
		uint8_t val = (uint8_t)(oppair >> 8);
	  gb->cpu_reg.pc.reg++;
		WGB_INSTR_ADC_R8(val, gb->cpu_reg.f.f_bits.c);
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;
	}

	case 0xCF: /* RST 0x0008 */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
		gb->cpu_reg.pc.reg = 0x0008;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0xD0: /* RET NC */
    if (!gb->cpu_reg.f.f_bits.c)
    {
        gb->cpu_reg.pc.reg = __gb_stack_pop16(gb, &gb->cpu_reg.sp.reg);
        inst_cycles += 12;
    }
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0xD1: /* POP DE */
    gb->cpu_reg.de.reg = __gb_stack_pop16(gb, &gb->cpu_reg.sp.reg);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xD2: /* JP NC, imm */
		if(!gb->cpu_reg.f.f_bits.c)
		{
#if WALNUT_GB_16_BIT_OPS_DUALFETCH_DISABLED
        gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
#else
        uint8_t c = __gb_fetch8(gb);
        uint8_t p = __gb_fetch8(gb);
        gb->cpu_reg.pc.bytes.c = c;
        gb->cpu_reg.pc.bytes.p = p;
#endif
			inst_cycles += 4;
		}
		else
			gb->cpu_reg.pc.reg += 2;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0xD4: /* CALL NC, imm */
		if(!gb->cpu_reg.f.f_bits.c)
		{
        uint8_t c =(uint8_t)(oppair >> 8);
				gb->cpu_reg.pc.reg++;
        uint8_t p = __gb_fetch8(gb);
        __gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
        gb->cpu_reg.pc.bytes.c = c;
        gb->cpu_reg.pc.bytes.p = p;
				inst_cycles += 12;
		}
		else
			gb->cpu_reg.pc.reg += 2;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0xD5: /* PUSH DE */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.de.reg);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xD6: /* SUB imm */
	{
		uint8_t val =(uint8_t)(oppair >> 8);
		gb->cpu_reg.pc.reg++;
		uint16_t temp = gb->cpu_reg.a - val;
		gb->cpu_reg.f.f_bits.z = ((temp & 0xFF) == 0x00);
		gb->cpu_reg.f.f_bits.n = 1;
		gb->cpu_reg.f.f_bits.h =
			(gb->cpu_reg.a ^ val ^ temp) & 0x10 ? 1 : 0;
		gb->cpu_reg.f.f_bits.c = (temp & 0xFF00) ? 1 : 0;
		gb->cpu_reg.a = (temp & 0xFF);
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;
	}

	case 0xD7: /* RST 0x0010 */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
		gb->cpu_reg.pc.reg = 0x0010;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0xD8: /* RET C */
		if(gb->cpu_reg.f.f_bits.c)
		{
        gb->cpu_reg.pc.reg = __gb_stack_pop16(gb, &gb->cpu_reg.sp.reg);
			inst_cycles += 12;
			opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		}
		else
			opcode = (uint8_t)(oppair >> 8);

		break;

	case 0xD9: /* RETI */
	{
    gb->cpu_reg.pc.reg = __gb_stack_pop16(gb, &gb->cpu_reg.sp.reg);
		gb->gb_ime = true;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
	}
	break;

	case 0xDA: /* JP C, imm */
		if(gb->cpu_reg.f.f_bits.c)
		{
        uint8_t c = (uint8_t)(oppair >> 8);
				gb->cpu_reg.pc.reg++;
        uint8_t p = __gb_fetch8(gb);
        gb->cpu_reg.pc.bytes.c = c;
        gb->cpu_reg.pc.bytes.p = p;
				inst_cycles += 4;
		}
		else
			gb->cpu_reg.pc.reg += 2;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0xDC: /* CALL C, imm */
		if(gb->cpu_reg.f.f_bits.c)
		{
        uint8_t c = (uint8_t)(oppair >> 8);
				gb->cpu_reg.pc.reg++;
        uint8_t p = __gb_fetch8(gb);
        __gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
        gb->cpu_reg.pc.bytes.c = c;
        gb->cpu_reg.pc.bytes.p = p;
				inst_cycles += 12;
		}
		else
			gb->cpu_reg.pc.reg += 2;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0xDE: /* SBC A, imm */
	{
		uint8_t val = (uint8_t)(oppair >> 8);
		gb->cpu_reg.pc.reg++;
		WGB_INSTR_SBC_R8(val, gb->cpu_reg.f.f_bits.c);
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;
	}
	case 0xDF: /* RST 0x0018 */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
		gb->cpu_reg.pc.reg = 0x0018;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0xE0: /* LD (0xFF00+imm), A */
	{
		const uint8_t io = (uint8_t)(oppair >> 8);
		if(!__gb_fast_hram_write(gb, io, gb->cpu_reg.a))
			__gb_write(gb, 0xFF00 | io, gb->cpu_reg.a);
		gb->cpu_reg.pc.reg++;
	  opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;
	}

	case 0xE1: /* POP HL */
    gb->cpu_reg.hl.reg = __gb_stack_pop16(gb, &gb->cpu_reg.sp.reg);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xE2: /* LD (C), A */
		if(!__gb_fast_hram_write(gb, gb->cpu_reg.bc.bytes.c, gb->cpu_reg.a))
			__gb_write(gb, 0xFF00 | gb->cpu_reg.bc.bytes.c, gb->cpu_reg.a);
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES_DISABLED
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
#else
		opcode = (uint8_t)(oppair >> 8);
#endif
		break;

	case 0xE5: /* PUSH HL */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.hl.reg);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xE6: /* AND imm */
	{
		uint8_t temp = (uint8_t)(oppair >> 8);
		gb->cpu_reg.pc.reg++;
		WGB_INSTR_AND_R8(temp);
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;
	}

	case 0xE7: /* RST 0x0020 */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
		gb->cpu_reg.pc.reg = 0x0020;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0xE8: /* ADD SP, imm */
	{
		int8_t offset = (int8_t)(oppair >> 8);
		gb->cpu_reg.pc.reg++;
		gb->cpu_reg.f.reg = 0;
		gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.sp.reg & 0xF) + (offset & 0xF) > 0xF) ? 1 : 0;
		gb->cpu_reg.f.f_bits.c = ((gb->cpu_reg.sp.reg & 0xFF) + (offset & 0xFF) > 0xFF);
		gb->cpu_reg.sp.reg += offset;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;
	}

	case 0xE9: /* JP (HL) */
		gb->cpu_reg.pc.reg = gb->cpu_reg.hl.reg;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0xEA: /* LD (imm), A */
	{
#if WALNUT_GB_16_BIT_OPS_DUALFETCH_DISABLED
    uint8_t l = (uint8_t)(oppair >> 8);
		gb->cpu_reg.pc.reg++;
		oppair = __gb_read16(gb, gb->cpu_reg.pc.reg++);
    uint8_t h = oppair; // autotruncate
    uint16_t addr = WALNUT_GB_U8_TO_U16(h, l);
		__gb_write(gb, addr, gb->cpu_reg.a);
		opcode = oppair >> 8;
#else
    uint8_t l = (uint8_t)(oppair >> 8);
		gb->cpu_reg.pc.reg++;
    uint8_t h = __gb_fetch8(gb);
    uint16_t addr = WALNUT_GB_U8_TO_U16(h, l);
		__gb_write(gb, addr, gb->cpu_reg.a);
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
#endif
		break;
	}

	case 0xEE: /* XOR imm */
		WGB_INSTR_XOR_R8((uint8_t)(oppair >> 8));
		gb->cpu_reg.pc.reg++;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0xEF: /* RST 0x0028 */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
		gb->cpu_reg.pc.reg = 0x0028;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0xF0: /* LD A, (0xFF00+imm) */
	{
		const uint8_t io = (uint8_t)(oppair >> 8);
#if WALNUT_GB_FAST_STAT_POLL_ACTIVE && WALNUT_GB_FAST_STAT_POLL_DIRECT_READS
		if(io == IO_STAT)
			gb->cpu_reg.a = __gb_fast_stat_poll_read(gb,
				(uint16_t)(gb->cpu_reg.pc.reg - 1),
				(uint16_t)(gb->cpu_reg.pc.reg + 1));
		else
#endif
			if(!__gb_fast_io_read_noapu(gb, io, &gb->cpu_reg.a))
				gb->cpu_reg.a = __gb_read(gb, 0xFF00 | io);
		gb->cpu_reg.pc.reg++;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;
	}

	case 0xF1: /* POP AF */
	{
		const uint16_t temp_16 =
			__gb_stack_pop16(gb, &gb->cpu_reg.sp.reg);
		uint8_t temp_8 = (uint8_t)temp_16;
		gb->cpu_reg.f.f_bits.z = (temp_8 >> 7) & 1;
		gb->cpu_reg.f.f_bits.n = (temp_8 >> 6) & 1;
		gb->cpu_reg.f.f_bits.h = (temp_8 >> 5) & 1;
		gb->cpu_reg.f.f_bits.c = (temp_8 >> 4) & 1;
		gb->cpu_reg.a = (uint8_t)(temp_16 >> 8);
		opcode = (uint8_t)(oppair >> 8);
		break;
	}

	case 0xF2: /* LD A, (C) */
	{
#if WALNUT_GB_FAST_STAT_POLL_ACTIVE
		if(gb->cpu_reg.bc.bytes.c == IO_STAT)
			gb->cpu_reg.a = __gb_fast_stat_poll_c_read(gb,
				(uint16_t)(gb->cpu_reg.pc.reg - 1), gb->cpu_reg.pc.reg);
		else
#endif
			if(!__gb_fast_io_read_noapu(gb, gb->cpu_reg.bc.bytes.c, &gb->cpu_reg.a))
				gb->cpu_reg.a = __gb_read(gb, 0xFF00 | gb->cpu_reg.bc.bytes.c);
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
#else
	opcode = (uint8_t)(oppair >> 8);
#endif
		break;
	}

	case 0xF3: /* DI */
		gb->gb_ime = false;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xF5: /* PUSH AF */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg,
			((uint16_t)gb->cpu_reg.a << 8) |
			gb->cpu_reg.f.f_bits.z << 7 | gb->cpu_reg.f.f_bits.n << 6 |
			gb->cpu_reg.f.f_bits.h << 5 | gb->cpu_reg.f.f_bits.c << 4);
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xF6: /* OR imm */
		WGB_INSTR_OR_R8((uint8_t)(oppair >> 8));
		gb->cpu_reg.pc.reg++;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0xF7: /* PUSH AF */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
		gb->cpu_reg.pc.reg = 0x0030;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	case 0xF8: /* LD HL, SP+/-imm */
	{
		/* Taken from SameBoy, which is released under MIT Licence. */
		int8_t offset = (int8_t) (oppair >> 8);
		gb->cpu_reg.pc.reg++;
		gb->cpu_reg.hl.reg = gb->cpu_reg.sp.reg + offset;
		gb->cpu_reg.f.reg = 0;
		gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.sp.reg & 0xF) + (offset & 0xF) > 0xF) ? 1 : 0;
		gb->cpu_reg.f.f_bits.c = ((gb->cpu_reg.sp.reg & 0xFF) + (offset & 0xFF) > 0xFF) ? 1 : 0;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;
	}

	case 0xF9: /* LD SP, HL */
		gb->cpu_reg.sp.reg = gb->cpu_reg.hl.reg;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xFA: /* LD A, (imm) */
	{
#if WALNUT_GB_16_BIT_OPS_DUALFETCH_DISABLED
    uint8_t l = (uint8_t)(oppair >> 8); // we dont increment pc because a interrupt might change it an invalidate this byte, we increment if no interrupt between 1st and 2nd opcode handlers
		gb->cpu_reg.pc.reg++;
		oppair = __gb_read16(gb, gb->cpu_reg.pc.reg++);
    uint8_t h = oppair; // auto-truncate
    uint16_t addr = WALNUT_GB_U8_TO_U16(h, l);
#else
    uint8_t l = (uint8_t)(oppair >> 8); // we dont increment pc because a interrupt might change it an invalidate this byte, we increment if no interrupt between 1st and 2nd opcode handlers
		gb->cpu_reg.pc.reg++;
    uint8_t h = __gb_fetch8(gb);
    uint16_t addr = WALNUT_GB_U8_TO_U16(h, l);
#endif
#if WALNUT_GB_FAST_STAT_POLL_ACTIVE && WALNUT_GB_FAST_STAT_POLL_DIRECT_READS
		if(addr == (uint16_t)(IO_ADDR | IO_STAT))
			gb->cpu_reg.a = __gb_fast_stat_poll_read(gb,
				(uint16_t)(gb->cpu_reg.pc.reg - 3), gb->cpu_reg.pc.reg);
		else
#endif
			if(!__gb_fast_abs_read_hot(gb, addr, &gb->cpu_reg.a))
				gb->cpu_reg.a = __gb_read(gb, addr);
#if WALNUT_GB_16_BIT_OPS_DUALFETCH_DISABLED
		opcode = oppair >> 8;
#else
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
#endif
		break;
	}

	case 0xFB: /* EI */
		gb->gb_ime = true;
		opcode = (uint8_t)(oppair >> 8);
		break;

	case 0xFE: /* CP imm */
	{
		uint8_t val = (uint8_t)(oppair >> 8);
		gb->cpu_reg.pc.reg++;
		WGB_INSTR_CP_R8(val);
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;
	}

	case 0xFF: /* RST 0x0038 */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
		gb->cpu_reg.pc.reg = 0x0038;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
		break;

	default:
		/* Return address where invalid opcode that was read. */
		(gb->gb_error)(gb, GB_INVALID_OPCODE, gb->cpu_reg.pc.reg - 1);
		WGB_UNREACHABLE();
		return;
	}


	do
	{
		if(__gb_post_step_fast_path(gb, inst_cycles))
		{
			break;
		}


		/* DIV register timing */
		gb->counter.div_count += inst_cycles;
		while(gb->counter.div_count >= DIV_CYCLES)
		{
			gb->hram_io[IO_DIV]++;
			gb->counter.div_count -= DIV_CYCLES;
		}

		/* Check for RTC tick. */
		if(WGB_UNLIKELY(gb->mbc == 3) && (gb->rtc_real.reg.high & 0x40) == 0)
		{
			gb->counter.rtc_count += inst_cycles;
			while(WGB_UNLIKELY(gb->counter.rtc_count >= RTC_CYCLES))
			{
				gb->counter.rtc_count -= RTC_CYCLES;

				/* Detect invalid rollover. */
				if(WGB_UNLIKELY(gb->rtc_real.reg.sec == 63))
				{
					gb->rtc_real.reg.sec = 0;
					continue;
				}

				if(++gb->rtc_real.reg.sec != 60)
					continue;

				gb->rtc_real.reg.sec = 0;
				if(gb->rtc_real.reg.min == 63)
				{
					gb->rtc_real.reg.min = 0;
					continue;
				}
				if(++gb->rtc_real.reg.min != 60)
					continue;

				gb->rtc_real.reg.min = 0;
				if(gb->rtc_real.reg.hour == 31)
				{
					gb->rtc_real.reg.hour = 0;
					continue;
				}
				if(++gb->rtc_real.reg.hour != 24)
					continue;

				gb->rtc_real.reg.hour = 0;
				if(++gb->rtc_real.reg.yday != 0)
					continue;

				if(gb->rtc_real.reg.high & 1)  /* Bit 8 of days*/
					gb->rtc_real.reg.high |= 0x80; /* Overflow bit */

				gb->rtc_real.reg.high ^= 1;
			}
		}

		/* Check serial transmission. */
		const uint8_t serial_control = gb->hram_io[IO_SC];
		if(WGB_UNLIKELY(serial_control & SERIAL_SC_TX_START))
		{
			unsigned int serial_cycles = SERIAL_CYCLES_1KB;

			/* If new transfer, call TX function. */
			if(gb->counter.serial_count == 0 &&
				gb->gb_serial_tx != NULL)
				(gb->gb_serial_tx)(gb, gb->hram_io[IO_SB]);

#if WALNUT_FULL_GBC_SUPPORT
			if(serial_control & 0x3)
				serial_cycles = SERIAL_CYCLES_32KB;
#endif

			gb->counter.serial_count += inst_cycles;

			/* If it's time to receive byte, call RX function. */
			if(gb->counter.serial_count >= serial_cycles)
			{
				/* If RX can be done, do it. */
				/* If RX failed, do not change SB if using external
				 * clock, or set to 0xFF if using internal clock. */
				uint8_t rx;

				if(gb->gb_serial_rx != NULL &&
					(gb->gb_serial_rx(gb, &rx) ==
						GB_SERIAL_RX_SUCCESS))
				{
					gb->hram_io[IO_SB] = rx;

					/* Inform game of serial TX/RX completion. */
					gb->hram_io[IO_SC] &= 0x01;
					gb->hram_io[IO_IF] |= SERIAL_INTR;
				}
				else if(serial_control & SERIAL_SC_CLOCK_SRC)
				{
					/* If using internal clock, and console is not
					 * attached to any external peripheral, shifted
					 * bits are replaced with logic 1. */
					gb->hram_io[IO_SB] = 0xFF;

					/* Inform game of serial TX/RX completion. */
					gb->hram_io[IO_SC] &= 0x01;
					gb->hram_io[IO_IF] |= SERIAL_INTR;
				}
				else
				{
					/* If using external clock, and console is not
					 * attached to any external peripheral, bits are
					 * not shifted, so SB is not modified. */
				}

				gb->counter.serial_count = 0;
			}
		}

		/* TIMA register timing */
		/* TODO: Change tac_enable to struct of TAC timer control bits. */
		const uint8_t tac = gb->hram_io[IO_TAC];
		if(tac & IO_TAC_ENABLE_MASK)
		{
			const uint_fast16_t tac_cycles = TAC_CYCLES[tac & IO_TAC_RATE_MASK];
			gb->counter.tima_count += inst_cycles;

			while(gb->counter.tima_count >= tac_cycles)
			{
				gb->counter.tima_count -= tac_cycles;

				if(++gb->hram_io[IO_TIMA] == 0)
				{
					gb->hram_io[IO_IF] |= TIMER_INTR;
					/* On overflow, set TMA to TIMA. */
					gb->hram_io[IO_TIMA] = gb->hram_io[IO_TMA];
				}
			}
		}

		const bool lcd_break = __gb_post_lcd_step(gb, &inst_cycles, true);
		if(lcd_break)
			break;
	} while(gb->gb_halt && __gb_effective_pending_interrupts(gb) == 0);
	/* If halted, loop until an interrupt occurs. */


	// **** 2nd instruction processing ****
#if (WALNUT_GB_SAFE_DUALFETCH_DMA || WALNUT_GB_SAFE_DUALFETCH_MBC)
	if (gb->prefetch_invalid)
	{
		gb->prefetch_invalid=false;
		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);// opcode was invalidated so we reload
	}
#endif
	/* Handle interrupts */
	/* If gb_halt is positive, then an interrupt must have occurred by the
	 * time we reach here, because on HALT, we jump to the next interrupt
	 * immediately. We also load opcode here because it may be replacing the previously loaded one from the dual fetch logic's first pass */
	while((gb->gb_halt || gb->gb_ime) && __gb_effective_pending_interrupts(gb))
	{
		gb->gb_halt = false;

		if(!gb->gb_ime)
			break;

		/* Disable interrupts */
		gb->gb_ime = false;

		/* Push Program Counter */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);

		/* Call interrupt handler if required. */
		if(__gb_effective_pending_interrupts(gb) & VBLANK_INTR)
		{
			gb->cpu_reg.pc.reg = VBLANK_INTR_ADDR;
			gb->hram_io[IO_IF] ^= VBLANK_INTR;
		}
		else if(__gb_effective_pending_interrupts(gb) & LCDC_INTR)
		{
			gb->cpu_reg.pc.reg = LCDC_INTR_ADDR;
			gb->hram_io[IO_IF] ^= LCDC_INTR;
		}
		else if(__gb_effective_pending_interrupts(gb) & TIMER_INTR)
		{
			gb->cpu_reg.pc.reg = TIMER_INTR_ADDR;
			gb->hram_io[IO_IF] ^= TIMER_INTR;
		}
		else if(__gb_effective_pending_interrupts(gb) & SERIAL_INTR)
		{
			gb->cpu_reg.pc.reg = SERIAL_INTR_ADDR;
			gb->hram_io[IO_IF] ^= SERIAL_INTR;
		}
		else if(__gb_effective_pending_interrupts(gb) & CONTROL_INTR)
		{
			gb->cpu_reg.pc.reg = CONTROL_INTR_ADDR;
			gb->hram_io[IO_IF] ^= CONTROL_INTR;
		}

		opcode = __gb_read(gb, gb->cpu_reg.pc.reg);

		break;
	}
	// at this point opcode always has the next instruction but pc hasn't yet been incremented so we increment here collectively for all cases
	// that can lead us to this point.
	gb->cpu_reg.pc.reg++;
	inst_cycles = op_cycles[opcode];
	/* Execute opcode */
	switch(opcode)
	{
	case 0x00: /* NOP */
		break;

	case 0x01: /* LD BC, imm */
#if WALNUT_GB_16_BIT_OPS
    gb->cpu_reg.bc.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
    gb->cpu_reg.pc.reg += 2;
#else
    gb->cpu_reg.bc.bytes.c = __gb_fetch8(gb);
    gb->cpu_reg.bc.bytes.b = __gb_fetch8(gb);
#endif
		break;
	case 0x02: /* LD (BC), A */
		__gb_write(gb, gb->cpu_reg.bc.reg, gb->cpu_reg.a);
		break;

	case 0x03: /* INC BC */
		gb->cpu_reg.bc.reg++;
		break;

	case 0x04: /* INC B */
		WGB_INSTR_INC_R8(gb->cpu_reg.bc.bytes.b);
		break;

	case 0x05: /* DEC B */
		WGB_INSTR_DEC_R8(gb->cpu_reg.bc.bytes.b);
		break;

	case 0x06: /* LD B, imm */
		gb->cpu_reg.bc.bytes.b = __gb_fetch8(gb);
		break;

	case 0x07: /* RLCA */
		gb->cpu_reg.a = (gb->cpu_reg.a << 1) | (gb->cpu_reg.a >> 7);
		gb->cpu_reg.f.reg = 0;
		gb->cpu_reg.f.f_bits.c = (gb->cpu_reg.a & 0x01);
		break;

	case 0x08: /* LD (imm), SP */
	{
#if WALNUT_GB_16_BIT_OPS
    uint16_t addr = __gb_read16(gb, gb->cpu_reg.pc.reg);
    gb->cpu_reg.pc.reg += 2;
    __gb_write(gb, addr++, gb->cpu_reg.sp.bytes.p);
    __gb_write(gb, addr, gb->cpu_reg.sp.bytes.s);
#else
    uint8_t l = __gb_fetch8(gb);
    uint8_t h = __gb_fetch8(gb);
    uint16_t addr = WALNUT_GB_U8_TO_U16(h, l);
    __gb_write(gb, addr++, gb->cpu_reg.sp.bytes.p);
    __gb_write(gb, addr, gb->cpu_reg.sp.bytes.s);
#endif
    break;
	}

	case 0x09: /* ADD HL, BC */
	{
		uint_fast32_t temp = gb->cpu_reg.hl.reg + gb->cpu_reg.bc.reg;
		gb->cpu_reg.f.f_bits.n = 0;
		gb->cpu_reg.f.f_bits.h =
			(temp ^ gb->cpu_reg.hl.reg ^ gb->cpu_reg.bc.reg) & 0x1000 ? 1 : 0;
		gb->cpu_reg.f.f_bits.c = (temp & 0xFFFF0000) ? 1 : 0;
		gb->cpu_reg.hl.reg = (temp & 0x0000FFFF);
		break;
	}

	case 0x0A: /* LD A, (BC) */
		gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.bc.reg);
		break;

	case 0x0B: /* DEC BC */
		gb->cpu_reg.bc.reg--;
		break;

	case 0x0C: /* INC C */
		WGB_INSTR_INC_R8(gb->cpu_reg.bc.bytes.c);
		break;

	case 0x0D: /* DEC C */
		WGB_INSTR_DEC_R8(gb->cpu_reg.bc.bytes.c);
		break;

	case 0x0E: /* LD C, imm */
		gb->cpu_reg.bc.bytes.c = __gb_fetch8(gb);
		break;

	case 0x0F: /* RRCA */
		gb->cpu_reg.f.reg = 0;
		gb->cpu_reg.f.f_bits.c = gb->cpu_reg.a & 0x01;
		gb->cpu_reg.a = (gb->cpu_reg.a >> 1) | (gb->cpu_reg.a << 7);
		break;

	case 0x10: /* STOP */
		//gb->gb_halt = true;
#if WALNUT_FULL_GBC_SUPPORT
		if(gb->cgb.cgbMode & gb->cgb.doubleSpeedPrep)
		{
			gb->cgb.doubleSpeedPrep = 0;
			gb->cgb.doubleSpeed ^= 1;
			__gb_cpu_timing_reschedule(gb);
		}
#endif
		/* STOP is two bytes; skip the padding byte. */
		gb->cpu_reg.pc.reg++;
		break;

	case 0x11: /* LD DE, imm */
#if WALNUT_GB_16_BIT_OPS
    gb->cpu_reg.de.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
    gb->cpu_reg.pc.reg += 2;
#else
    gb->cpu_reg.de.bytes.e = __gb_fetch8(gb);
    gb->cpu_reg.de.bytes.d = __gb_fetch8(gb);
#endif
		break;

	case 0x12: /* LD (DE), A */
		__gb_write(gb, gb->cpu_reg.de.reg, gb->cpu_reg.a);
		break;

	case 0x13: /* INC DE */
		gb->cpu_reg.de.reg++;
		break;

	case 0x14: /* INC D */
		WGB_INSTR_INC_R8(gb->cpu_reg.de.bytes.d);
		break;

	case 0x15: /* DEC D */
		WGB_INSTR_DEC_R8(gb->cpu_reg.de.bytes.d);
		break;

	case 0x16: /* LD D, imm */
		gb->cpu_reg.de.bytes.d = __gb_fetch8(gb);
		break;

	case 0x17: /* RLA */
	{
		uint8_t temp = gb->cpu_reg.a;
		gb->cpu_reg.a = (gb->cpu_reg.a << 1) | gb->cpu_reg.f.f_bits.c;
		gb->cpu_reg.f.reg = 0;
		gb->cpu_reg.f.f_bits.c = (temp >> 7) & 0x01;
		break;
	}

	case 0x18: /* JR imm */
	{
		int8_t temp = (int8_t) __gb_fetch8(gb);
		gb->cpu_reg.pc.reg += temp;
		break;
	}

	case 0x19: /* ADD HL, DE */
	{
		uint_fast32_t temp = gb->cpu_reg.hl.reg + gb->cpu_reg.de.reg;
		gb->cpu_reg.f.f_bits.n = 0;
		gb->cpu_reg.f.f_bits.h =
			(temp ^ gb->cpu_reg.hl.reg ^ gb->cpu_reg.de.reg) & 0x1000 ? 1 : 0;
		gb->cpu_reg.f.f_bits.c = (temp & 0xFFFF0000) ? 1 : 0;
		gb->cpu_reg.hl.reg = (temp & 0x0000FFFF);
		break;
	}

	case 0x1A: /* LD A, (DE) */
		gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.de.reg);
		break;

	case 0x1B: /* DEC DE */
		gb->cpu_reg.de.reg--;
		break;

	case 0x1C: /* INC E */
		WGB_INSTR_INC_R8(gb->cpu_reg.de.bytes.e);
		break;

	case 0x1D: /* DEC E */
		WGB_INSTR_DEC_R8(gb->cpu_reg.de.bytes.e);
		break;

	case 0x1E: /* LD E, imm */
		gb->cpu_reg.de.bytes.e = __gb_fetch8(gb);
		break;

	case 0x1F: /* RRA */
	{
		uint8_t temp = gb->cpu_reg.a;
		gb->cpu_reg.a = gb->cpu_reg.a >> 1 | (gb->cpu_reg.f.f_bits.c << 7);
		gb->cpu_reg.f.reg = 0;
		gb->cpu_reg.f.f_bits.c = temp & 0x1;
		break;
	}

	case 0x20: /* JR NZ, imm */
		if(!gb->cpu_reg.f.f_bits.z)
		{
			const uint16_t branch_pc = (uint16_t)(gb->cpu_reg.pc.reg - 1);
			int8_t temp = (int8_t) __gb_fetch8(gb);
			gb->cpu_reg.pc.reg += temp;
			inst_cycles += 4;
			__gb_fast_poll_wait_maybe(gb, gb->cpu_reg.pc.reg, branch_pc, 0x20);
		}
		else
			gb->cpu_reg.pc.reg++;

		break;

	case 0x21: /* LD HL, imm */
#if WALNUT_GB_16_BIT_OPS
    gb->cpu_reg.hl.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
    gb->cpu_reg.pc.reg += 2;
#else
    gb->cpu_reg.hl.bytes.l = __gb_fetch8(gb);
    gb->cpu_reg.hl.bytes.h = __gb_fetch8(gb);
#endif
		break;

	case 0x22: /* LDI (HL), A */
		__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.a);
		gb->cpu_reg.hl.reg++;
		break;

	case 0x23: /* INC HL */
		gb->cpu_reg.hl.reg++;
		break;

	case 0x24: /* INC H */
		WGB_INSTR_INC_R8(gb->cpu_reg.hl.bytes.h);
		break;

	case 0x25: /* DEC H */
		WGB_INSTR_DEC_R8(gb->cpu_reg.hl.bytes.h);
		break;

	case 0x26: /* LD H, imm */
		gb->cpu_reg.hl.bytes.h = __gb_fetch8(gb);
		break;

	case 0x27: /* DAA */
	{
		/* The following is from SameBoy. MIT License. */
		int16_t a = gb->cpu_reg.a;

		if(gb->cpu_reg.f.f_bits.n)
		{
			if(gb->cpu_reg.f.f_bits.h)
				a = (a - 0x06) & 0xFF;

			if(gb->cpu_reg.f.f_bits.c)
				a -= 0x60;
		}
		else
		{
			if(gb->cpu_reg.f.f_bits.h || (a & 0x0F) > 9)
				a += 0x06;

			if(gb->cpu_reg.f.f_bits.c || a > 0x9F)
				a += 0x60;
		}

		if((a & 0x100) == 0x100)
			gb->cpu_reg.f.f_bits.c = 1;

		gb->cpu_reg.a = (uint8_t)a;
		gb->cpu_reg.f.f_bits.z = (gb->cpu_reg.a == 0);
		gb->cpu_reg.f.f_bits.h = 0;

		break;
	}

	case 0x28: /* JR Z, imm */
		if(gb->cpu_reg.f.f_bits.z)
		{
			const uint16_t branch_pc = (uint16_t)(gb->cpu_reg.pc.reg - 1);
			int8_t temp = (int8_t) __gb_fetch8(gb);
			gb->cpu_reg.pc.reg += temp;
			inst_cycles += 4;
			__gb_fast_poll_wait_maybe(gb, gb->cpu_reg.pc.reg, branch_pc, 0x28);
		}
		else
			gb->cpu_reg.pc.reg++;

		break;

	case 0x29: /* ADD HL, HL */
	{
		gb->cpu_reg.f.f_bits.c = (gb->cpu_reg.hl.reg & 0x8000) > 0;
		gb->cpu_reg.hl.reg <<= 1;
		gb->cpu_reg.f.f_bits.n = 0;
		gb->cpu_reg.f.f_bits.h = (gb->cpu_reg.hl.reg & 0x1000) > 0;
		break;
	}

	case 0x2A: /* LD A, (HL+) */
		gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.hl.reg++);
		break;

	case 0x2B: /* DEC HL */
		gb->cpu_reg.hl.reg--;
		break;

	case 0x2C: /* INC L */
		WGB_INSTR_INC_R8(gb->cpu_reg.hl.bytes.l);
		break;

	case 0x2D: /* DEC L */
		WGB_INSTR_DEC_R8(gb->cpu_reg.hl.bytes.l);
		break;

	case 0x2E: /* LD L, imm */
		gb->cpu_reg.hl.bytes.l = __gb_fetch8(gb);
		break;

	case 0x2F: /* CPL */
		gb->cpu_reg.a = ~gb->cpu_reg.a;
		gb->cpu_reg.f.f_bits.n = 1;
		gb->cpu_reg.f.f_bits.h = 1;
		break;

	case 0x30: /* JR NC, imm */
		if(!gb->cpu_reg.f.f_bits.c)
		{
			const uint16_t branch_pc = (uint16_t)(gb->cpu_reg.pc.reg - 1);
			int8_t temp = (int8_t) __gb_fetch8(gb);
			gb->cpu_reg.pc.reg += temp;
			inst_cycles += 4;
#if WALNUT_GB_FAST_POLL_WAIT_ACTIVE
			if(__gb_fast_poll_wait_carry_ly_prefilter(gb,
					gb->cpu_reg.pc.reg, branch_pc))
				__gb_fast_poll_wait_maybe(gb, gb->cpu_reg.pc.reg,
					branch_pc, 0x30);
#endif
		}
		else
			gb->cpu_reg.pc.reg++;

		break;

	case 0x31: /* LD SP, imm */
#if WALNUT_GB_16_BIT_OPS
    gb->cpu_reg.sp.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
    gb->cpu_reg.pc.reg += 2;
#else
    gb->cpu_reg.sp.bytes.p = __gb_fetch8(gb);
    gb->cpu_reg.sp.bytes.s = __gb_fetch8(gb);
#endif
		break;

	case 0x32: /* LD (HL), A */
		__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.a);
		gb->cpu_reg.hl.reg--;
		break;

	case 0x33: /* INC SP */
		gb->cpu_reg.sp.reg++;
		break;

	case 0x34: /* INC (HL) */
	{
		uint8_t temp = __gb_read(gb, gb->cpu_reg.hl.reg);
		WGB_INSTR_INC_R8(temp);
		__gb_write(gb, gb->cpu_reg.hl.reg, temp);
		break;
	}

	case 0x35: /* DEC (HL) */
	{
		uint8_t temp = __gb_read(gb, gb->cpu_reg.hl.reg);
		WGB_INSTR_DEC_R8(temp);
		__gb_write(gb, gb->cpu_reg.hl.reg, temp);
		break;
	}

	case 0x36: /* LD (HL), imm */
		__gb_write(gb, gb->cpu_reg.hl.reg, __gb_fetch8(gb));
		break;

	case 0x37: /* SCF */
		gb->cpu_reg.f.f_bits.n = 0;
		gb->cpu_reg.f.f_bits.h = 0;
		gb->cpu_reg.f.f_bits.c = 1;
		break;

	case 0x38: /* JR C, imm */
		if(gb->cpu_reg.f.f_bits.c)
		{
			const uint16_t branch_pc = (uint16_t)(gb->cpu_reg.pc.reg - 1);
			int8_t temp = (int8_t) __gb_fetch8(gb);
			gb->cpu_reg.pc.reg += temp;
			inst_cycles += 4;
#if WALNUT_GB_FAST_POLL_WAIT_ACTIVE
			if(__gb_fast_poll_wait_carry_ly_prefilter(gb,
					gb->cpu_reg.pc.reg, branch_pc))
				__gb_fast_poll_wait_maybe(gb, gb->cpu_reg.pc.reg,
					branch_pc, 0x38);
#endif
		}
		else
			gb->cpu_reg.pc.reg++;

		break;

	case 0x39: /* ADD HL, SP */
	{
		uint_fast32_t temp = gb->cpu_reg.hl.reg + gb->cpu_reg.sp.reg;
		gb->cpu_reg.f.f_bits.n = 0;
		gb->cpu_reg.f.f_bits.h =
			((gb->cpu_reg.hl.reg & 0xFFF) + (gb->cpu_reg.sp.reg & 0xFFF)) & 0x1000 ? 1 : 0;
		gb->cpu_reg.f.f_bits.c = temp & 0x10000 ? 1 : 0;
		gb->cpu_reg.hl.reg = (uint16_t)temp;
		break;
	}

	case 0x3A: /* LD A, (HL) */
		gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.hl.reg--);
		break;

	case 0x3B: /* DEC SP */
		gb->cpu_reg.sp.reg--;
		break;

	case 0x3C: /* INC A */
		WGB_INSTR_INC_R8(gb->cpu_reg.a);
		break;

	case 0x3D: /* DEC A */
		WGB_INSTR_DEC_R8(gb->cpu_reg.a);
		break;

	case 0x3E: /* LD A, imm */
		gb->cpu_reg.a = __gb_fetch8(gb);
		break;

	case 0x3F: /* CCF */
		gb->cpu_reg.f.f_bits.n = 0;
		gb->cpu_reg.f.f_bits.h = 0;
		gb->cpu_reg.f.f_bits.c = ~gb->cpu_reg.f.f_bits.c;
		break;

	case 0x40: /* LD B, B */
		break;

	case 0x41: /* LD B, C */
		gb->cpu_reg.bc.bytes.b = gb->cpu_reg.bc.bytes.c;
		break;

	case 0x42: /* LD B, D */
		gb->cpu_reg.bc.bytes.b = gb->cpu_reg.de.bytes.d;
		break;

	case 0x43: /* LD B, E */
		gb->cpu_reg.bc.bytes.b = gb->cpu_reg.de.bytes.e;
		break;

	case 0x44: /* LD B, H */
		gb->cpu_reg.bc.bytes.b = gb->cpu_reg.hl.bytes.h;
		break;

	case 0x45: /* LD B, L */
		gb->cpu_reg.bc.bytes.b = gb->cpu_reg.hl.bytes.l;
		break;

	case 0x46: /* LD B, (HL) */
		gb->cpu_reg.bc.bytes.b = __gb_read(gb, gb->cpu_reg.hl.reg);
		break;

	case 0x47: /* LD B, A */
		gb->cpu_reg.bc.bytes.b = gb->cpu_reg.a;
		break;

	case 0x48: /* LD C, B */
		gb->cpu_reg.bc.bytes.c = gb->cpu_reg.bc.bytes.b;
		break;

	case 0x49: /* LD C, C */
		break;

	case 0x4A: /* LD C, D */
		gb->cpu_reg.bc.bytes.c = gb->cpu_reg.de.bytes.d;
		break;

	case 0x4B: /* LD C, E */
		gb->cpu_reg.bc.bytes.c = gb->cpu_reg.de.bytes.e;
		break;

	case 0x4C: /* LD C, H */
		gb->cpu_reg.bc.bytes.c = gb->cpu_reg.hl.bytes.h;
		break;

	case 0x4D: /* LD C, L */
		gb->cpu_reg.bc.bytes.c = gb->cpu_reg.hl.bytes.l;
		break;

	case 0x4E: /* LD C, (HL) */
		gb->cpu_reg.bc.bytes.c = __gb_read(gb, gb->cpu_reg.hl.reg);
		break;

	case 0x4F: /* LD C, A */
		gb->cpu_reg.bc.bytes.c = gb->cpu_reg.a;
		break;

	case 0x50: /* LD D, B */
		gb->cpu_reg.de.bytes.d = gb->cpu_reg.bc.bytes.b;
		break;

	case 0x51: /* LD D, C */
		gb->cpu_reg.de.bytes.d = gb->cpu_reg.bc.bytes.c;
		break;

	case 0x52: /* LD D, D */
		break;

	case 0x53: /* LD D, E */
		gb->cpu_reg.de.bytes.d = gb->cpu_reg.de.bytes.e;
		break;

	case 0x54: /* LD D, H */
		gb->cpu_reg.de.bytes.d = gb->cpu_reg.hl.bytes.h;
		break;

	case 0x55: /* LD D, L */
		gb->cpu_reg.de.bytes.d = gb->cpu_reg.hl.bytes.l;
		break;

	case 0x56: /* LD D, (HL) */
		gb->cpu_reg.de.bytes.d = __gb_read(gb, gb->cpu_reg.hl.reg);
		break;

	case 0x57: /* LD D, A */
		gb->cpu_reg.de.bytes.d = gb->cpu_reg.a;
		break;

	case 0x58: /* LD E, B */
		gb->cpu_reg.de.bytes.e = gb->cpu_reg.bc.bytes.b;
		break;

	case 0x59: /* LD E, C */
		gb->cpu_reg.de.bytes.e = gb->cpu_reg.bc.bytes.c;
		break;

	case 0x5A: /* LD E, D */
		gb->cpu_reg.de.bytes.e = gb->cpu_reg.de.bytes.d;
		break;

	case 0x5B: /* LD E, E */
		break;

	case 0x5C: /* LD E, H */
		gb->cpu_reg.de.bytes.e = gb->cpu_reg.hl.bytes.h;
		break;

	case 0x5D: /* LD E, L */
		gb->cpu_reg.de.bytes.e = gb->cpu_reg.hl.bytes.l;
		break;

	case 0x5E: /* LD E, (HL) */
		gb->cpu_reg.de.bytes.e = __gb_read(gb, gb->cpu_reg.hl.reg);
		break;

	case 0x5F: /* LD E, A */
		gb->cpu_reg.de.bytes.e = gb->cpu_reg.a;
		break;

	case 0x60: /* LD H, B */
		gb->cpu_reg.hl.bytes.h = gb->cpu_reg.bc.bytes.b;
		break;

	case 0x61: /* LD H, C */
		gb->cpu_reg.hl.bytes.h = gb->cpu_reg.bc.bytes.c;
		break;

	case 0x62: /* LD H, D */
		gb->cpu_reg.hl.bytes.h = gb->cpu_reg.de.bytes.d;
		break;

	case 0x63: /* LD H, E */
		gb->cpu_reg.hl.bytes.h = gb->cpu_reg.de.bytes.e;
		break;

	case 0x64: /* LD H, H */
		break;

	case 0x65: /* LD H, L */
		gb->cpu_reg.hl.bytes.h = gb->cpu_reg.hl.bytes.l;
		break;

	case 0x66: /* LD H, (HL) */
		gb->cpu_reg.hl.bytes.h = __gb_read(gb, gb->cpu_reg.hl.reg);
		break;

	case 0x67: /* LD H, A */
		gb->cpu_reg.hl.bytes.h = gb->cpu_reg.a;
		break;

	case 0x68: /* LD L, B */
		gb->cpu_reg.hl.bytes.l = gb->cpu_reg.bc.bytes.b;
		break;

	case 0x69: /* LD L, C */
		gb->cpu_reg.hl.bytes.l = gb->cpu_reg.bc.bytes.c;
		break;

	case 0x6A: /* LD L, D */
		gb->cpu_reg.hl.bytes.l = gb->cpu_reg.de.bytes.d;
		break;

	case 0x6B: /* LD L, E */
		gb->cpu_reg.hl.bytes.l = gb->cpu_reg.de.bytes.e;
		break;

	case 0x6C: /* LD L, H */
		gb->cpu_reg.hl.bytes.l = gb->cpu_reg.hl.bytes.h;
		break;

	case 0x6D: /* LD L, L */
		break;

	case 0x6E: /* LD L, (HL) */
		gb->cpu_reg.hl.bytes.l = __gb_read(gb, gb->cpu_reg.hl.reg);
		break;

	case 0x6F: /* LD L, A */
		gb->cpu_reg.hl.bytes.l = gb->cpu_reg.a;
		break;

	case 0x70: /* LD (HL), B */
		__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.bc.bytes.b);
		break;

	case 0x71: /* LD (HL), C */
		__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.bc.bytes.c);
		break;

	case 0x72: /* LD (HL), D */
		__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.de.bytes.d);
		break;

	case 0x73: /* LD (HL), E */
		__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.de.bytes.e);
		break;

	case 0x74: /* LD (HL), H */
		__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.hl.bytes.h);
		break;

	case 0x75: /* LD (HL), L */
		__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.hl.bytes.l);
		break;

	case 0x76: /* HALT */
	{
		int_fast16_t halt_cycles = INT_FAST16_MAX;

		/* TODO: Emulate HALT bug? */
		gb->gb_halt = true;

		if(__gb_halt_fast_forward_to_vblank(gb))
		{
			inst_cycles = 0;
			break;
		}

		if(gb->hram_io[IO_SC] & SERIAL_SC_TX_START)
		{
			int serial_cycles = SERIAL_CYCLES -
				gb->counter.serial_count;

			if(serial_cycles < halt_cycles)
				halt_cycles = serial_cycles;
		}

		if(gb->hram_io[IO_TAC] & IO_TAC_ENABLE_MASK)
		{
			int tac_cycles = TAC_CYCLES[gb->hram_io[IO_TAC] & IO_TAC_RATE_MASK] -
				gb->counter.tima_count;

			if(tac_cycles < halt_cycles)
				halt_cycles = tac_cycles;
		}

		if((gb->hram_io[IO_LCDC] & LCDC_ENABLE))
		{
			int lcd_cycles;

			/* If LCD is in HBlank, calculate the number of cycles
			 * until the end of HBlank and the start of mode 2 or
			 * mode 1. */
			if((gb->hram_io[IO_STAT] & STAT_MODE) == IO_STAT_MODE_HBLANK)
			{
				lcd_cycles = LCD_MODE0_HBLANK_MAX_DRUATION - gb->counter.lcd_count;
			}
			else if((gb->hram_io[IO_STAT] & STAT_MODE) == IO_STAT_MODE_OAM_SCAN)
			{
				lcd_cycles = LCD_MODE3_LCD_DRAW_MIN_DURATION - gb->counter.lcd_count;
			}
			else if((gb->hram_io[IO_STAT] & STAT_MODE) == IO_STAT_MODE_LCD_DRAW)
			{
				lcd_cycles = LCD_MODE0_HBLANK_MAX_DRUATION - gb->counter.lcd_count;
			}
			else
			{
				/* VBlank */
				lcd_cycles = LCD_LINE_CYCLES - gb->counter.lcd_count;
			}

			if(lcd_cycles < halt_cycles)
				halt_cycles = lcd_cycles;
		}

		/* Some halt cycles may already be very high, so make sure we
		 * don't underflow here. */
		if(halt_cycles <= 0)
			halt_cycles = 4;

		inst_cycles = (uint_fast16_t)halt_cycles;
		break;
	}

	case 0x77: /* LD (HL), A */
		__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.a);
		break;

	case 0x78: /* LD A, B */
		gb->cpu_reg.a = gb->cpu_reg.bc.bytes.b;
		break;

	case 0x79: /* LD A, C */
		gb->cpu_reg.a = gb->cpu_reg.bc.bytes.c;
		break;

	case 0x7A: /* LD A, D */
		gb->cpu_reg.a = gb->cpu_reg.de.bytes.d;
		break;

	case 0x7B: /* LD A, E */
		gb->cpu_reg.a = gb->cpu_reg.de.bytes.e;
		break;

	case 0x7C: /* LD A, H */
		gb->cpu_reg.a = gb->cpu_reg.hl.bytes.h;
		break;

	case 0x7D: /* LD A, L */
		gb->cpu_reg.a = gb->cpu_reg.hl.bytes.l;
		break;

	case 0x7E: /* LD A, (HL) */
		gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.hl.reg);
		break;

	case 0x7F: /* LD A, A */
		break;

	case 0x80: /* ADD A, B */
		WGB_INSTR_ADC_R8(gb->cpu_reg.bc.bytes.b, 0);
		break;

	case 0x81: /* ADD A, C */
		WGB_INSTR_ADC_R8(gb->cpu_reg.bc.bytes.c, 0);
		break;

	case 0x82: /* ADD A, D */
		WGB_INSTR_ADC_R8(gb->cpu_reg.de.bytes.d, 0);
		break;

	case 0x83: /* ADD A, E */
		WGB_INSTR_ADC_R8(gb->cpu_reg.de.bytes.e, 0);
		break;

	case 0x84: /* ADD A, H */
		WGB_INSTR_ADC_R8(gb->cpu_reg.hl.bytes.h, 0);
		break;

	case 0x85: /* ADD A, L */
		WGB_INSTR_ADC_R8(gb->cpu_reg.hl.bytes.l, 0);
		break;

	case 0x86: /* ADD A, (HL) */
		WGB_INSTR_ADC_R8(__gb_read(gb, gb->cpu_reg.hl.reg), 0);
		break;

	case 0x87: /* ADD A, A */
		WGB_INSTR_ADC_R8(gb->cpu_reg.a, 0);
		break;

	case 0x88: /* ADC A, B */
		WGB_INSTR_ADC_R8(gb->cpu_reg.bc.bytes.b, gb->cpu_reg.f.f_bits.c);
		break;

	case 0x89: /* ADC A, C */
		WGB_INSTR_ADC_R8(gb->cpu_reg.bc.bytes.c, gb->cpu_reg.f.f_bits.c);
		break;

	case 0x8A: /* ADC A, D */
		WGB_INSTR_ADC_R8(gb->cpu_reg.de.bytes.d, gb->cpu_reg.f.f_bits.c);
		break;

	case 0x8B: /* ADC A, E */
		WGB_INSTR_ADC_R8(gb->cpu_reg.de.bytes.e, gb->cpu_reg.f.f_bits.c);
		break;

	case 0x8C: /* ADC A, H */
		WGB_INSTR_ADC_R8(gb->cpu_reg.hl.bytes.h, gb->cpu_reg.f.f_bits.c);
		break;

	case 0x8D: /* ADC A, L */
		WGB_INSTR_ADC_R8(gb->cpu_reg.hl.bytes.l, gb->cpu_reg.f.f_bits.c);
		break;

	case 0x8E: /* ADC A, (HL) */
		WGB_INSTR_ADC_R8(__gb_read(gb, gb->cpu_reg.hl.reg), gb->cpu_reg.f.f_bits.c);
		break;

	case 0x8F: /* ADC A, A */
		WGB_INSTR_ADC_R8(gb->cpu_reg.a, gb->cpu_reg.f.f_bits.c);
		break;

	case 0x90: /* SUB B */
		WGB_INSTR_SBC_R8(gb->cpu_reg.bc.bytes.b, 0);
		break;

	case 0x91: /* SUB C */
		WGB_INSTR_SBC_R8(gb->cpu_reg.bc.bytes.c, 0);
		break;

	case 0x92: /* SUB D */
		WGB_INSTR_SBC_R8(gb->cpu_reg.de.bytes.d, 0);
		break;

	case 0x93: /* SUB E */
		WGB_INSTR_SBC_R8(gb->cpu_reg.de.bytes.e, 0);
		break;

	case 0x94: /* SUB H */
		WGB_INSTR_SBC_R8(gb->cpu_reg.hl.bytes.h, 0);
		break;

	case 0x95: /* SUB L */
		WGB_INSTR_SBC_R8(gb->cpu_reg.hl.bytes.l, 0);
		break;

	case 0x96: /* SUB (HL) */
		WGB_INSTR_SBC_R8(__gb_read(gb, gb->cpu_reg.hl.reg), 0);
		break;

	case 0x97: /* SUB A */
		gb->cpu_reg.a = 0;
		gb->cpu_reg.f.reg = 0;
		gb->cpu_reg.f.f_bits.z = 1;
		gb->cpu_reg.f.f_bits.n = 1;
		break;

	case 0x98: /* SBC A, B */
		WGB_INSTR_SBC_R8(gb->cpu_reg.bc.bytes.b, gb->cpu_reg.f.f_bits.c);
		break;

	case 0x99: /* SBC A, C */
		WGB_INSTR_SBC_R8(gb->cpu_reg.bc.bytes.c, gb->cpu_reg.f.f_bits.c);
		break;

	case 0x9A: /* SBC A, D */
		WGB_INSTR_SBC_R8(gb->cpu_reg.de.bytes.d, gb->cpu_reg.f.f_bits.c);
		break;

	case 0x9B: /* SBC A, E */
		WGB_INSTR_SBC_R8(gb->cpu_reg.de.bytes.e, gb->cpu_reg.f.f_bits.c);
		break;

	case 0x9C: /* SBC A, H */
		WGB_INSTR_SBC_R8(gb->cpu_reg.hl.bytes.h, gb->cpu_reg.f.f_bits.c);
		break;

	case 0x9D: /* SBC A, L */
		WGB_INSTR_SBC_R8(gb->cpu_reg.hl.bytes.l, gb->cpu_reg.f.f_bits.c);
		break;

	case 0x9E: /* SBC A, (HL) */
		WGB_INSTR_SBC_R8(__gb_read(gb, gb->cpu_reg.hl.reg), gb->cpu_reg.f.f_bits.c);
		break;

	case 0x9F: /* SBC A, A */
		gb->cpu_reg.a = gb->cpu_reg.f.f_bits.c ? 0xFF : 0x00;
		gb->cpu_reg.f.f_bits.z = !gb->cpu_reg.f.f_bits.c;
		gb->cpu_reg.f.f_bits.n = 1;
		gb->cpu_reg.f.f_bits.h = gb->cpu_reg.f.f_bits.c;
		break;

	case 0xA0: /* AND B */
		WGB_INSTR_AND_R8(gb->cpu_reg.bc.bytes.b);
		break;

	case 0xA1: /* AND C */
		WGB_INSTR_AND_R8(gb->cpu_reg.bc.bytes.c);
		break;

	case 0xA2: /* AND D */
		WGB_INSTR_AND_R8(gb->cpu_reg.de.bytes.d);
		break;

	case 0xA3: /* AND E */
		WGB_INSTR_AND_R8(gb->cpu_reg.de.bytes.e);
		break;

	case 0xA4: /* AND H */
		WGB_INSTR_AND_R8(gb->cpu_reg.hl.bytes.h);
		break;

	case 0xA5: /* AND L */
		WGB_INSTR_AND_R8(gb->cpu_reg.hl.bytes.l);
		break;

	case 0xA6: /* AND (HL) */
		WGB_INSTR_AND_R8(__gb_read(gb, gb->cpu_reg.hl.reg));
		break;

	case 0xA7: /* AND A */
		WGB_INSTR_AND_R8(gb->cpu_reg.a);
		break;

	case 0xA8: /* XOR B */
		WGB_INSTR_XOR_R8(gb->cpu_reg.bc.bytes.b);
		break;

	case 0xA9: /* XOR C */
		WGB_INSTR_XOR_R8(gb->cpu_reg.bc.bytes.c);
		break;

	case 0xAA: /* XOR D */
		WGB_INSTR_XOR_R8(gb->cpu_reg.de.bytes.d);
		break;

	case 0xAB: /* XOR E */
		WGB_INSTR_XOR_R8(gb->cpu_reg.de.bytes.e);
		break;

	case 0xAC: /* XOR H */
		WGB_INSTR_XOR_R8(gb->cpu_reg.hl.bytes.h);
		break;

	case 0xAD: /* XOR L */
		WGB_INSTR_XOR_R8(gb->cpu_reg.hl.bytes.l);
		break;

	case 0xAE: /* XOR (HL) */
		WGB_INSTR_XOR_R8(__gb_read(gb, gb->cpu_reg.hl.reg));
		break;

	case 0xAF: /* XOR A */
		WGB_INSTR_XOR_R8(gb->cpu_reg.a);
		break;

	case 0xB0: /* OR B */
		WGB_INSTR_OR_R8(gb->cpu_reg.bc.bytes.b);
		break;

	case 0xB1: /* OR C */
		WGB_INSTR_OR_R8(gb->cpu_reg.bc.bytes.c);
		break;

	case 0xB2: /* OR D */
		WGB_INSTR_OR_R8(gb->cpu_reg.de.bytes.d);
		break;

	case 0xB3: /* OR E */
		WGB_INSTR_OR_R8(gb->cpu_reg.de.bytes.e);
		break;

	case 0xB4: /* OR H */
		WGB_INSTR_OR_R8(gb->cpu_reg.hl.bytes.h);
		break;

	case 0xB5: /* OR L */
		WGB_INSTR_OR_R8(gb->cpu_reg.hl.bytes.l);
		break;

	case 0xB6: /* OR (HL) */
		WGB_INSTR_OR_R8(__gb_read(gb, gb->cpu_reg.hl.reg));
		break;

	case 0xB7: /* OR A */
		WGB_INSTR_OR_R8(gb->cpu_reg.a);
		break;

	case 0xB8: /* CP B */
		WGB_INSTR_CP_R8(gb->cpu_reg.bc.bytes.b);
		break;

	case 0xB9: /* CP C */
		WGB_INSTR_CP_R8(gb->cpu_reg.bc.bytes.c);
		break;

	case 0xBA: /* CP D */
		WGB_INSTR_CP_R8(gb->cpu_reg.de.bytes.d);
		break;

	case 0xBB: /* CP E */
		WGB_INSTR_CP_R8(gb->cpu_reg.de.bytes.e);
		break;

	case 0xBC: /* CP H */
		WGB_INSTR_CP_R8(gb->cpu_reg.hl.bytes.h);
		break;

	case 0xBD: /* CP L */
		WGB_INSTR_CP_R8(gb->cpu_reg.hl.bytes.l);
		break;

	case 0xBE: /* CP (HL) */
		WGB_INSTR_CP_R8(__gb_read(gb, gb->cpu_reg.hl.reg));
		break;

	case 0xBF: /* CP A */
		gb->cpu_reg.f.reg = 0;
		gb->cpu_reg.f.f_bits.z = 1;
		gb->cpu_reg.f.f_bits.n = 1;
		break;

	case 0xC0: /* RET NZ */
		if(!gb->cpu_reg.f.f_bits.z)
		{
        gb->cpu_reg.pc.reg = __gb_stack_pop16(gb, &gb->cpu_reg.sp.reg);
        inst_cycles += 12;
		}

		break;

	case 0xC1: /* POP BC */
    gb->cpu_reg.bc.reg = __gb_stack_pop16(gb, &gb->cpu_reg.sp.reg);
		break;

	case 0xC2: /* JP NZ, imm */
		if(!gb->cpu_reg.f.f_bits.z)
		{
#if WALNUT_GB_16_BIT_OPS
        gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
#else
        uint8_t c = __gb_fetch8(gb);
        uint8_t p = __gb_fetch8(gb);
        gb->cpu_reg.pc.bytes.c = c;
        gb->cpu_reg.pc.bytes.p = p;
#endif
			inst_cycles += 4;
		}
		else
			gb->cpu_reg.pc.reg += 2;

		break;

	case 0xC3: /* JP imm */
#if WALNUT_GB_16_BIT_OPS
    gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
#else
{
    uint8_t c = __gb_fetch8(gb);
    uint8_t p = __gb_fetch8(gb);
    gb->cpu_reg.pc.bytes.c = c;
    gb->cpu_reg.pc.bytes.p = p;
}
#endif
		break;

	case 0xC4: /* CALL NZ imm */
		if(!gb->cpu_reg.f.f_bits.z)
		{
#if WALNUT_GB_16_BIT_OPS
			uint16_t addr = __gb_read16(gb, gb->cpu_reg.pc.reg);
			gb->cpu_reg.pc.reg += 2;
			__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
			gb->cpu_reg.pc.reg = addr;
#else
			uint8_t c, p;
			c = __gb_fetch8(gb);
			p = __gb_fetch8(gb);
			__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
			gb->cpu_reg.pc.bytes.c = c;
			gb->cpu_reg.pc.bytes.p = p;
#endif
			inst_cycles += 12;
		}
		else
			gb->cpu_reg.pc.reg += 2;

		break;

	case 0xC5: /* PUSH BC */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.bc.reg);
		break;

	case 0xC6: /* ADD A, imm */
	{
		uint8_t val = __gb_fetch8(gb);
		WGB_INSTR_ADC_R8(val, 0);
		break;
	}

	case 0xC7: /* RST 0x0000 */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
		gb->cpu_reg.pc.reg = 0x0000;
		break;

	case 0xC8: /* RET Z */
		if(gb->cpu_reg.f.f_bits.z)
		{
        gb->cpu_reg.pc.reg = __gb_stack_pop16(gb, &gb->cpu_reg.sp.reg);
			inst_cycles += 12;
		}
		break;

	case 0xC9: /* RET */
	{
    gb->cpu_reg.pc.reg = __gb_stack_pop16(gb, &gb->cpu_reg.sp.reg);
		break;
	}

	case 0xCA: /* JP Z, imm */
		if(gb->cpu_reg.f.f_bits.z)
		{
#if WALNUT_GB_16_BIT_OPS
        gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.pc.reg); // no need to advance pc because of jump
#else
        uint8_t c = __gb_fetch8(gb);
        uint8_t p = __gb_fetch8(gb);
        gb->cpu_reg.pc.bytes.c = c;
        gb->cpu_reg.pc.bytes.p = p;
#endif
			inst_cycles += 4;
		}
		else
			gb->cpu_reg.pc.reg += 2;

		break;

	case 0xCB: /* CB INST */
		inst_cycles = __gb_execute_cb(gb);
		break;

	case 0xCC: /* CALL Z, imm */
		if(gb->cpu_reg.f.f_bits.z)
		{
#if WALNUT_GB_16_BIT_OPS
        uint16_t addr = __gb_read16(gb, gb->cpu_reg.pc.reg);
        gb->cpu_reg.pc.reg += 2; // advance past immediate

        // push old PC
        __gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);

        gb->cpu_reg.pc.reg = addr;
#else
        uint8_t c = __gb_fetch8(gb);
        uint8_t p = __gb_fetch8(gb);
        __gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
        gb->cpu_reg.pc.bytes.c = c;
        gb->cpu_reg.pc.bytes.p = p;
#endif
			inst_cycles += 12;
		}
		else
			gb->cpu_reg.pc.reg += 2;

		break;

	case 0xCD: /* CALL imm */
#if WALNUT_GB_16_BIT_OPS
{
    uint16_t addr = __gb_read16(gb, gb->cpu_reg.pc.reg);
    gb->cpu_reg.pc.reg += 2; // advance past immediate
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
    gb->cpu_reg.pc.reg = addr;
}
#else
{
    uint8_t c = __gb_fetch8(gb);
    uint8_t p = __gb_fetch8(gb);
    __gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
    gb->cpu_reg.pc.bytes.c = c;
    gb->cpu_reg.pc.bytes.p = p;
}
#endif
	break;

	case 0xCE: /* ADC A, imm */
	{
		uint8_t val = __gb_fetch8(gb);
		WGB_INSTR_ADC_R8(val, gb->cpu_reg.f.f_bits.c);
		break;
	}

	case 0xCF: /* RST 0x0008 */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
		gb->cpu_reg.pc.reg = 0x0008;
		break;

	case 0xD0: /* RET NC */

    if (!gb->cpu_reg.f.f_bits.c)
    {
        gb->cpu_reg.pc.reg = __gb_stack_pop16(gb, &gb->cpu_reg.sp.reg);
        inst_cycles += 12;
    }

		break;

	case 0xD1: /* POP DE */
    gb->cpu_reg.de.reg = __gb_stack_pop16(gb, &gb->cpu_reg.sp.reg);
		break;

	case 0xD2: /* JP NC, imm */
		if(!gb->cpu_reg.f.f_bits.c)
		{
#if WALNUT_GB_16_BIT_OPS
        gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.pc.reg); // jump so no need to advance pc
#else
        uint8_t c = __gb_fetch8(gb);
        uint8_t p = __gb_fetch8(gb);
        gb->cpu_reg.pc.bytes.c = c;
        gb->cpu_reg.pc.bytes.p = p;
#endif
			inst_cycles += 4;
		}
		else
			gb->cpu_reg.pc.reg += 2;

		break;

	case 0xD4: /* CALL NC, imm */
		if(!gb->cpu_reg.f.f_bits.c)
		{
#if WALNUT_GB_16_BIT_OPS
        uint16_t addr = __gb_read16(gb, gb->cpu_reg.pc.reg);
        gb->cpu_reg.pc.reg += 2; // advance PC past immediate
        __gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
        gb->cpu_reg.pc.reg = addr;
#else
        uint8_t c = __gb_fetch8(gb);
        uint8_t p = __gb_fetch8(gb);
        __gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
        gb->cpu_reg.pc.bytes.c = c;
        gb->cpu_reg.pc.bytes.p = p;
#endif
			inst_cycles += 12;
		}
		else
			gb->cpu_reg.pc.reg += 2;

		break;

	case 0xD5: /* PUSH DE */
	// this was revised but working backwards from this 16-bit op to find the one thats broken
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.de.reg);
		break;

	case 0xD6: /* SUB imm */
	{
		uint8_t val = __gb_fetch8(gb);
		uint16_t temp = gb->cpu_reg.a - val;
		gb->cpu_reg.f.f_bits.z = ((temp & 0xFF) == 0x00);
		gb->cpu_reg.f.f_bits.n = 1;
		gb->cpu_reg.f.f_bits.h =
			(gb->cpu_reg.a ^ val ^ temp) & 0x10 ? 1 : 0;
		gb->cpu_reg.f.f_bits.c = (temp & 0xFF00) ? 1 : 0;
		gb->cpu_reg.a = (temp & 0xFF);
		break;
	}

	case 0xD7: /* RST 0x0010 */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
		gb->cpu_reg.pc.reg = 0x0010;
		break;

	case 0xD8: /* RET C */
		if(gb->cpu_reg.f.f_bits.c)
		{
        gb->cpu_reg.pc.reg = __gb_stack_pop16(gb, &gb->cpu_reg.sp.reg);
			inst_cycles += 12;
		}

		break;

	case 0xD9: /* RETI */
	{
    gb->cpu_reg.pc.reg = __gb_stack_pop16(gb, &gb->cpu_reg.sp.reg);
		gb->gb_ime = true;
	}
	break;

	case 0xDA: /* JP C, imm */
		if(gb->cpu_reg.f.f_bits.c)
		{
#if WALNUT_GB_16_BIT
        gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
        gb->cpu_reg.pc.reg += 2; // advance PC past the immediate word
#else
        uint8_t c = __gb_fetch8(gb);
        uint8_t p = __gb_fetch8(gb);
        gb->cpu_reg.pc.bytes.c = c;
        gb->cpu_reg.pc.bytes.p = p;
#endif
			inst_cycles += 4;
		}
		else
			gb->cpu_reg.pc.reg += 2;

		break;

	case 0xDC: /* CALL C, imm */
		if(gb->cpu_reg.f.f_bits.c)
		{
#if WALNUT_GB_16_BIT
        uint16_t target = __gb_read16(gb, gb->cpu_reg.pc.reg);
        gb->cpu_reg.pc.reg += 2; // advance PC past the immediate word
        // push current PC onto stack
        __gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
        gb->cpu_reg.pc.reg = target;
#else
        uint8_t c = __gb_fetch8(gb);
        uint8_t p = __gb_fetch8(gb);
        __gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
        gb->cpu_reg.pc.bytes.c = c;
        gb->cpu_reg.pc.bytes.p = p;
#endif
			inst_cycles += 12;
		}
		else
			gb->cpu_reg.pc.reg += 2;

		break;

	case 0xDE: /* SBC A, imm */
	{
		uint8_t val = __gb_fetch8(gb);
		WGB_INSTR_SBC_R8(val, gb->cpu_reg.f.f_bits.c);
		break;
	}

	case 0xDF: /* RST 0x0018 */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
		gb->cpu_reg.pc.reg = 0x0018;
		break;

	case 0xE0: /* LD (0xFF00+imm), A */
	{
		const uint8_t io = __gb_fetch8(gb);
		if(!__gb_fast_hram_write(gb, io, gb->cpu_reg.a))
			__gb_write(gb, 0xFF00 | io, gb->cpu_reg.a);
		break;
	}

	case 0xE1: /* POP HL */
    gb->cpu_reg.hl.reg = __gb_stack_pop16(gb, &gb->cpu_reg.sp.reg);
		break;

	case 0xE2: /* LD (C), A */
		if(!__gb_fast_hram_write(gb, gb->cpu_reg.bc.bytes.c, gb->cpu_reg.a))
			__gb_write(gb, 0xFF00 | gb->cpu_reg.bc.bytes.c, gb->cpu_reg.a);
		break;

	case 0xE5: /* PUSH HL */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.hl.reg);
		break;

	case 0xE6: /* AND imm */
	{
		uint8_t temp = __gb_fetch8(gb);
		WGB_INSTR_AND_R8(temp);
		break;
	}

	case 0xE7: /* RST 0x0020 */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
		gb->cpu_reg.pc.reg = 0x0020;
		break;

	case 0xE8: /* ADD SP, imm */
	{
		int8_t offset = (int8_t) __gb_fetch8(gb);
		gb->cpu_reg.f.reg = 0;
		gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.sp.reg & 0xF) + (offset & 0xF) > 0xF) ? 1 : 0;
		gb->cpu_reg.f.f_bits.c = ((gb->cpu_reg.sp.reg & 0xFF) + (offset & 0xFF) > 0xFF);
		gb->cpu_reg.sp.reg += offset;
		break;
	}

	case 0xE9: /* JP (HL) */
		gb->cpu_reg.pc.reg = gb->cpu_reg.hl.reg;
		break;

	case 0xEA: /* LD (imm), A */
	{
#if WALNUT_GB_16_BIT
    uint16_t addr = __gb_read16(gb, gb->cpu_reg.pc.reg);
    gb->cpu_reg.pc.reg += 2; // advance past the immediate
#else
    uint8_t l = __gb_fetch8(gb);
    uint8_t h = __gb_fetch8(gb);
    uint16_t addr = WALNUT_GB_U8_TO_U16(h, l);
#endif
		__gb_write(gb, addr, gb->cpu_reg.a);
		break;
	}

	case 0xEE: /* XOR imm */
		WGB_INSTR_XOR_R8(__gb_fetch8(gb));
		break;

	case 0xEF: /* RST 0x0028 */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
		gb->cpu_reg.pc.reg = 0x0028;
		break;

	case 0xF0: /* LD A, (0xFF00+imm) */
	{
		const uint16_t stat_read_pc = (uint16_t)(gb->cpu_reg.pc.reg - 1);
		const uint8_t io = __gb_fetch8(gb);
#if WALNUT_GB_FAST_STAT_POLL_ACTIVE && WALNUT_GB_FAST_STAT_POLL_DIRECT_READS
		if(io == IO_STAT)
			gb->cpu_reg.a = __gb_fast_stat_poll_read(gb,
				stat_read_pc, gb->cpu_reg.pc.reg);
		else
#endif
			if(!__gb_fast_io_read_noapu(gb, io, &gb->cpu_reg.a))
				gb->cpu_reg.a = __gb_read(gb, 0xFF00 | io);
		break;
	}

	case 0xF1: /* POP AF */
	{
		const uint16_t temp_16 =
			__gb_stack_pop16(gb, &gb->cpu_reg.sp.reg);
		uint8_t temp_8 = (uint8_t)temp_16;
		gb->cpu_reg.f.f_bits.z = (temp_8 >> 7) & 1;
		gb->cpu_reg.f.f_bits.n = (temp_8 >> 6) & 1;
		gb->cpu_reg.f.f_bits.h = (temp_8 >> 5) & 1;
		gb->cpu_reg.f.f_bits.c = (temp_8 >> 4) & 1;
		gb->cpu_reg.a = (uint8_t)(temp_16 >> 8);
		break;
	}

	case 0xF2: /* LD A, (C) */
	{
#if WALNUT_GB_FAST_STAT_POLL_ACTIVE
		if(gb->cpu_reg.bc.bytes.c == IO_STAT)
			gb->cpu_reg.a = __gb_fast_stat_poll_c_read(gb,
				(uint16_t)(gb->cpu_reg.pc.reg - 1), gb->cpu_reg.pc.reg);
		else
#endif
			if(!__gb_fast_io_read_noapu(gb, gb->cpu_reg.bc.bytes.c, &gb->cpu_reg.a))
				gb->cpu_reg.a = __gb_read(gb, 0xFF00 | gb->cpu_reg.bc.bytes.c);
		break;
	}

	case 0xF3: /* DI */
		gb->gb_ime = false;
		break;

	case 0xF5: /* PUSH AF */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg,
			((uint16_t)gb->cpu_reg.a << 8) |
			gb->cpu_reg.f.f_bits.z << 7 | gb->cpu_reg.f.f_bits.n << 6 |
			gb->cpu_reg.f.f_bits.h << 5 | gb->cpu_reg.f.f_bits.c << 4);
		break;

	case 0xF6: /* OR imm */
		WGB_INSTR_OR_R8(__gb_fetch8(gb));
		break;

	case 0xF7: /* PUSH AF */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
		gb->cpu_reg.pc.reg = 0x0030;
		break;

	case 0xF8: /* LD HL, SP+/-imm */
	{
		/* Taken from SameBoy, which is released under MIT Licence. */
		int8_t offset = (int8_t) __gb_fetch8(gb);
		gb->cpu_reg.hl.reg = gb->cpu_reg.sp.reg + offset;
		gb->cpu_reg.f.reg = 0;
		gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.sp.reg & 0xF) + (offset & 0xF) > 0xF) ? 1 : 0;
		gb->cpu_reg.f.f_bits.c = ((gb->cpu_reg.sp.reg & 0xFF) + (offset & 0xFF) > 0xFF) ? 1 : 0;
		break;
	}

	case 0xF9: /* LD SP, HL */
		gb->cpu_reg.sp.reg = gb->cpu_reg.hl.reg;
		break;

	case 0xFA: /* LD A, (imm) */
	{
		const uint16_t stat_read_pc = (uint16_t)(gb->cpu_reg.pc.reg - 1);
#if WALNUT_GB_16_BIT
    uint16_t addr = __gb_read16(gb, gb->cpu_reg.pc.reg);
    gb->cpu_reg.pc.reg += 2; // advance past the immediate
#else
    uint8_t l = __gb_fetch8(gb);
    uint8_t h = __gb_fetch8(gb);
    uint16_t addr = WALNUT_GB_U8_TO_U16(h, l);
#endif
#if WALNUT_GB_FAST_STAT_POLL_ACTIVE && WALNUT_GB_FAST_STAT_POLL_DIRECT_READS
		if(addr == (uint16_t)(IO_ADDR | IO_STAT))
			gb->cpu_reg.a = __gb_fast_stat_poll_read(gb,
				stat_read_pc, gb->cpu_reg.pc.reg);
		else
#endif
			if(!__gb_fast_abs_read_hot(gb, addr, &gb->cpu_reg.a))
				gb->cpu_reg.a = __gb_read(gb, addr);
		break;
	}

	case 0xFB: /* EI */
		gb->gb_ime = true;
		break;

	case 0xFE: /* CP imm */
	{
		uint8_t val = __gb_fetch8(gb);
		WGB_INSTR_CP_R8(val);
		break;
	}

	case 0xFF: /* RST 0x0038 */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
		gb->cpu_reg.pc.reg = 0x0038;
		break;

	default:
		/* Return address where invalid opcode that was read. */
		(gb->gb_error)(gb, GB_INVALID_OPCODE, gb->cpu_reg.pc.reg - 1);
		WGB_UNREACHABLE();
	}
dualfetch_post_opcode:

	// *** post opcode handling ***

	do
	{
		if(__gb_post_step_fast_path(gb, inst_cycles))
		{
			break;
		}


		/* DIV register timing */
		gb->counter.div_count += inst_cycles;
		while(gb->counter.div_count >= DIV_CYCLES)
		{
			gb->hram_io[IO_DIV]++;
			gb->counter.div_count -= DIV_CYCLES;
		}

		/* Check for RTC tick. */
		if(WGB_UNLIKELY(gb->mbc == 3) && (gb->rtc_real.reg.high & 0x40) == 0)
		{
			gb->counter.rtc_count += inst_cycles;
			while(WGB_UNLIKELY(gb->counter.rtc_count >= RTC_CYCLES))
			{
				gb->counter.rtc_count -= RTC_CYCLES;

				/* Detect invalid rollover. */
				if(WGB_UNLIKELY(gb->rtc_real.reg.sec == 63))
				{
					gb->rtc_real.reg.sec = 0;
					continue;
				}

				if(++gb->rtc_real.reg.sec != 60)
					continue;

				gb->rtc_real.reg.sec = 0;
				if(gb->rtc_real.reg.min == 63)
				{
					gb->rtc_real.reg.min = 0;
					continue;
				}
				if(++gb->rtc_real.reg.min != 60)
					continue;

				gb->rtc_real.reg.min = 0;
				if(gb->rtc_real.reg.hour == 31)
				{
					gb->rtc_real.reg.hour = 0;
					continue;
				}
				if(++gb->rtc_real.reg.hour != 24)
					continue;

				gb->rtc_real.reg.hour = 0;
				if(++gb->rtc_real.reg.yday != 0)
					continue;

				if(gb->rtc_real.reg.high & 1)  /* Bit 8 of days*/
					gb->rtc_real.reg.high |= 0x80; /* Overflow bit */

				gb->rtc_real.reg.high ^= 1;
			}
		}

		/* Check serial transmission. */
		const uint8_t serial_control = gb->hram_io[IO_SC];
		if(WGB_UNLIKELY(serial_control & SERIAL_SC_TX_START))
		{
			unsigned int serial_cycles = SERIAL_CYCLES_1KB;

			/* If new transfer, call TX function. */
			if(gb->counter.serial_count == 0 &&
				gb->gb_serial_tx != NULL)
				(gb->gb_serial_tx)(gb, gb->hram_io[IO_SB]);

#if WALNUT_FULL_GBC_SUPPORT
			if(serial_control & 0x3)
				serial_cycles = SERIAL_CYCLES_32KB;
#endif

			gb->counter.serial_count += inst_cycles;

			/* If it's time to receive byte, call RX function. */
			if(gb->counter.serial_count >= serial_cycles)
			{
				/* If RX can be done, do it. */
				/* If RX failed, do not change SB if using external
				 * clock, or set to 0xFF if using internal clock. */
				uint8_t rx;

				if(gb->gb_serial_rx != NULL &&
					(gb->gb_serial_rx(gb, &rx) ==
						GB_SERIAL_RX_SUCCESS))
				{
					gb->hram_io[IO_SB] = rx;

					/* Inform game of serial TX/RX completion. */
					gb->hram_io[IO_SC] &= 0x01;
					gb->hram_io[IO_IF] |= SERIAL_INTR;
				}
				else if(serial_control & SERIAL_SC_CLOCK_SRC)
				{
					/* If using internal clock, and console is not
					 * attached to any external peripheral, shifted
					 * bits are replaced with logic 1. */
					gb->hram_io[IO_SB] = 0xFF;

					/* Inform game of serial TX/RX completion. */
					gb->hram_io[IO_SC] &= 0x01;
					gb->hram_io[IO_IF] |= SERIAL_INTR;
				}
				else
				{
					/* If using external clock, and console is not
					 * attached to any external peripheral, bits are
					 * not shifted, so SB is not modified. */
				}

				gb->counter.serial_count = 0;
			}
		}

		/* TIMA register timing */
		/* TODO: Change tac_enable to struct of TAC timer control bits. */
		const uint8_t tac = gb->hram_io[IO_TAC];
		if(tac & IO_TAC_ENABLE_MASK)
		{
			const uint_fast16_t tac_cycles = TAC_CYCLES[tac & IO_TAC_RATE_MASK];
			gb->counter.tima_count += inst_cycles;

			while(gb->counter.tima_count >= tac_cycles)
			{
				gb->counter.tima_count -= tac_cycles;

				if(++gb->hram_io[IO_TIMA] == 0)
				{
					gb->hram_io[IO_IF] |= TIMER_INTR;
					/* On overflow, set TMA to TIMA. */
					gb->hram_io[IO_TIMA] = gb->hram_io[IO_TMA];
				}
			}
		}

		const bool lcd_break = __gb_post_lcd_step(gb, &inst_cycles, false);
		if(lcd_break)
			break;
	} while(gb->gb_halt && __gb_effective_pending_interrupts(gb) == 0);
}

#if WALNUT_GB_THREADED_CPU_ACTIVE
void gb_run_frame_threaded(struct gb_s *gb)
{
	static const uint8_t WALNUT_GB_THREADED_TABLE_ATTR op_cycles[0x100] =
	{
		/* *INDENT-OFF* */
		/*0 1 2  3  4  5  6  7  8  9  A  B  C  D  E  F	*/
		4,12, 8, 8, 4, 4, 8, 4,20, 8, 8, 8, 4, 4, 8, 4,	/* 0x00 */
		4,12, 8, 8, 4, 4, 8, 4,12, 8, 8, 8, 4, 4, 8, 4,	/* 0x10 */
		8,12, 8, 8, 4, 4, 8, 4, 8, 8, 8, 8, 4, 4, 8, 4,	/* 0x20 */
		8,12, 8, 8,12,12,12, 4, 8, 8, 8, 8, 4, 4, 8, 4,	/* 0x30 */
		4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,	/* 0x40 */
		4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,	/* 0x50 */
		4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,	/* 0x60 */
		8, 8, 8, 8, 8, 8, 4, 8, 4, 4, 4, 4, 4, 4, 8, 4, /* 0x70 */
		4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,	/* 0x80 */
		4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,	/* 0x90 */
		4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,	/* 0xA0 */
		4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,	/* 0xB0 */
		8,12,12,16,12,16, 8,16, 8,16,12, 8,12,24, 8,16,	/* 0xC0 */
		8,12,12, 0,12,16, 8,16, 8,16,12, 0,12, 0, 8,16,	/* 0xD0 */
		12,12,8, 0, 0,16, 8,16,16, 4,16, 0, 0, 0, 8,16,	/* 0xE0 */
		12,12,8, 4, 0,16, 8,16,12, 8,16, 4, 0, 0, 8,16	/* 0xF0 */
		/* *INDENT-ON* */
	};
	static void *const WALNUT_GB_THREADED_TABLE_ATTR first_dispatch[0x100] =
	{
#include "walnut_cgb_threaded_dual_first_dispatch.h"
	};
	static void *const WALNUT_GB_THREADED_TABLE_ATTR second_dispatch[0x100] =
	{
#include "walnut_cgb_threaded_dual_second_dispatch.h"
	};
	uint16_t oppair;
	uint8_t opcode;
	uint_fast16_t inst_cycles;
	uint16_t hotpc_fetch_pc;
#if WALNUT_GB_LAZY_POST_ACTIVE
	uint_fast32_t wgb_lazy_pending = 0;
#endif
#if WALNUT_GB_THREADED_COMPACT_DENSE
	uint8_t dense_first_phase;
#endif
#if WALNUT_GB_THREADED_LOCAL_CPU_REGS
	struct cpu_registers_s wgb_cpu_reg = gb->cpu_reg;
#endif

	gb->gb_frame = false;
#if WALNUT_GB_LAZY_POST_ACTIVE
	gb->lazy_post_pending = &wgb_lazy_pending;
#endif
	/* suppress_lcd_stat_interrupts is constant for the whole frame. */
	const uint8_t wgb_irq_enable_mask =
		gb->direct.suppress_lcd_stat_interrupts ?
			(uint8_t)(ANY_INTR & (uint8_t)~LCDC_INTR) : (uint8_t)ANY_INTR;
#if (WALNUT_GB_SAFE_DUALFETCH_DMA || WALNUT_GB_SAFE_DUALFETCH_MBC)
	gb->prefetch_invalid = false;
#endif

#if WALNUT_GB_THREADED_LOCAL_CPU_REGS
#undef WGB_CPU_REG
#define WGB_CPU_REG wgb_cpu_reg
#define __gb_fetch8(gb_arg_) \
	__gb_fetch8_threaded_local_cpu((gb_arg_), &wgb_cpu_reg)
#define __gb_fetch16(gb_arg_) \
	__gb_fetch16_threaded_local_cpu((gb_arg_), &wgb_cpu_reg)
#define __gb_execute_cb(gb_arg_) \
	__gb_execute_cb_threaded_local_cpu((gb_arg_), &wgb_cpu_reg)
#if WALNUT_GB_FAST_STAT_POLL_ACTIVE || WALNUT_GB_FAST_POLL_WAIT_ACTIVE
#define __gb_fast_poll_wait_maybe(gb_arg_, target_pc_, branch_pc_, branch_opcode_) \
	__gb_fast_poll_wait_maybe_threaded_local_cpu((gb_arg_), &wgb_cpu_reg, \
		(target_pc_), (branch_pc_), (branch_opcode_))
#endif
#if WALNUT_GB_FAST_STAT_POLL_ACTIVE
#define __gb_fast_stat_poll_read(gb_arg_, stat_read_pc_, next_pc_) \
	__gb_fast_stat_poll_read_threaded_local_cpu((gb_arg_), &wgb_cpu_reg, \
		(stat_read_pc_), (next_pc_))
#define __gb_fast_stat_poll_c_read(gb_arg_, stat_read_pc_, next_pc_) \
	__gb_fast_stat_poll_c_read_threaded_local_cpu((gb_arg_), &wgb_cpu_reg, \
		(stat_read_pc_), (next_pc_))
#endif
#else
#undef WGB_CPU_REG
#define WGB_CPU_REG gb->cpu_reg
#endif

#if WALNUT_GB_THREADED_FAST_MEM
#define __gb_read(gb_arg_, addr_) \
	__gb_read_threaded_fast((gb_arg_), (addr_))
#define __gb_read16(gb_arg_, addr_) \
	__gb_read16_threaded_fast((gb_arg_), (addr_))
#define __gb_write(gb_arg_, addr_, val_) \
	__gb_write_threaded_fast((gb_arg_), (addr_), (val_))
#endif

/* Cycles accumulate in wgb_lazy_pending until the event countdown gets
 * within WALNUT_GB_LAZY_POST_MARGIN, then go through the normal post-step. */
#if WALNUT_GB_LAZY_POST_ACTIVE
#define WGB_THREADED_POST_CPU_STEP(inval_) \
	do { \
		wgb_lazy_pending += inst_cycles; \
		if(WGB_UNLIKELY(gb->counter.cpu_event_count <= \
				wgb_lazy_pending + WALNUT_GB_LAZY_POST_MARGIN)) \
		{ \
			uint_fast16_t wgb_lazy_cycles_ = \
				(uint_fast16_t)wgb_lazy_pending; \
			wgb_lazy_pending = 0; \
			__gb_post_cpu_step(gb, &wgb_lazy_cycles_, (inval_)); \
		} \
	} while(0)
#else
#define WGB_THREADED_POST_CPU_STEP(inval_) \
	__gb_post_cpu_step(gb, &inst_cycles, (inval_))
#endif

#define WGB_THREADED_SERVICE_INTERRUPT(reload_opcode_) \
	do { \
		while((gb->gb_halt || gb->gb_ime) && \
				(gb->hram_io[IO_IF] & gb->hram_io[IO_IE] & \
					wgb_irq_enable_mask)) \
		{ \
			gb->gb_halt = false; \
			if(!gb->gb_ime) \
				break; \
			gb->gb_ime = false; \
			__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg); \
			if((gb->hram_io[IO_IF] & gb->hram_io[IO_IE] & \
					wgb_irq_enable_mask) & VBLANK_INTR) \
			{ \
				WGB_CPU_REG.pc.reg = VBLANK_INTR_ADDR; \
				gb->hram_io[IO_IF] ^= VBLANK_INTR; \
			} \
			else if((gb->hram_io[IO_IF] & gb->hram_io[IO_IE] & \
					wgb_irq_enable_mask) & LCDC_INTR) \
			{ \
				WGB_CPU_REG.pc.reg = LCDC_INTR_ADDR; \
				gb->hram_io[IO_IF] ^= LCDC_INTR; \
			} \
			else if((gb->hram_io[IO_IF] & gb->hram_io[IO_IE] & \
					wgb_irq_enable_mask) & TIMER_INTR) \
			{ \
				WGB_CPU_REG.pc.reg = TIMER_INTR_ADDR; \
				gb->hram_io[IO_IF] ^= TIMER_INTR; \
			} \
			else if((gb->hram_io[IO_IF] & gb->hram_io[IO_IE] & \
					wgb_irq_enable_mask) & SERIAL_INTR) \
			{ \
				WGB_CPU_REG.pc.reg = SERIAL_INTR_ADDR; \
				gb->hram_io[IO_IF] ^= SERIAL_INTR; \
			} \
			else if((gb->hram_io[IO_IF] & gb->hram_io[IO_IE] & \
					wgb_irq_enable_mask) & CONTROL_INTR) \
			{ \
				WGB_CPU_REG.pc.reg = CONTROL_INTR_ADDR; \
				gb->hram_io[IO_IF] ^= CONTROL_INTR; \
			} \
			if(reload_opcode_) \
			{ \
				opcode = __gb_read(gb, WGB_CPU_REG.pc.reg); \
			} \
			break; \
		} \
	} while(0)

threaded_dual_first_dispatch:
	if(WGB_UNLIKELY(gb->gb_halt))
	{
		__gb_service_halt_until_event_or_irq(gb, false);
		if(gb->gb_frame)
			goto threaded_dual_exit;
	}
	WGB_THREADED_SERVICE_INTERRUPT(0);

	hotpc_fetch_pc = WGB_CPU_REG.pc.reg;
	oppair = __gb_fetch_opcode_pair(gb, WGB_CPU_REG.pc.reg++);
	opcode = (uint8_t)oppair;
#if (WALNUT_GB_SAFE_DUALFETCH_DMA || WALNUT_GB_SAFE_DUALFETCH_MBC)
	gb->prefetch_invalid = false;
#endif
#if WALNUT_GB_THREADED_COMPACT_DENSE
	if(WGB_LIKELY((uint8_t)(opcode - 0x40) < 0x80))
	{
		if(opcode < 0x80)
		{
			const uint8_t source = opcode & 7;
			const uint8_t destination = (opcode >> 3) & 7;
			inst_cycles = (opcode != 0x76 &&
				(source == 6 || destination == 6)) ? 8 : 4;
		}
		else
			inst_cycles = ((opcode & 7) == 6) ? 8 : 4;
		goto threaded_dual_first_dense;
	}
#endif
	inst_cycles = op_cycles[opcode];
	goto *first_dispatch[opcode];

#if WALNUT_GB_THREADED_COMPACT_DENSE
threaded_dual_first_dense:
	dense_first_phase = 1;
	goto threaded_dual_dense;

threaded_dual_second_dense:
	dense_first_phase = 0;

threaded_dual_dense:
	{
		const uint8_t dense_opcode = opcode;
		uint8_t dense_value;
		uint8_t *const dense_regs = (uint8_t *)&WGB_CPU_REG;

		if(WGB_LIKELY(dense_opcode >= 0x80))
		{
			const uint8_t source = dense_opcode & 7;
			if(WGB_UNLIKELY(source == 6))
				dense_value = __gb_read(gb, WGB_CPU_REG.hl.reg);
			else if(source == 7)
				dense_value = WGB_CPU_REG.a;
			else
				dense_value = dense_regs[(source ^ 1u) + 2u];

			const uint8_t operation = (dense_opcode >> 3) & 7;
			const uint8_t a = WGB_CPU_REG.a;
			uint8_t result;
			uint8_t flags;

			switch(operation)
			{
			case 0: /* ADD A, r */
			{
				const uint16_t wide = (uint16_t)a + dense_value;
				result = (uint8_t)wide;
				flags = (result == 0 ? 0x80 : 0) |
					(((a & 0x0F) + (dense_value & 0x0F)) > 0x0F ? 0x20 : 0) |
					(wide > 0xFF ? 0x10 : 0);
				WGB_CPU_REG.a = result;
				WGB_CPU_REG.f.reg = flags;
				break;
			}
			case 1: /* ADC A, r */
			{
				const uint8_t carry = (WGB_CPU_REG.f.reg >> 4) & 1;
				const uint16_t wide = (uint16_t)a + dense_value + carry;
				result = (uint8_t)wide;
				flags = (result == 0 ? 0x80 : 0) |
					(((a & 0x0F) + (dense_value & 0x0F) + carry) > 0x0F ?
						0x20 : 0) |
					(wide > 0xFF ? 0x10 : 0);
				WGB_CPU_REG.a = result;
				WGB_CPU_REG.f.reg = flags;
				break;
			}
			case 2: /* SUB r */
			{
				const uint16_t wide = (uint16_t)a - dense_value;
				result = (uint8_t)wide;
				flags = 0x40 |
					(result == 0 ? 0x80 : 0) |
					(((a ^ dense_value ^ result) & 0x10) ? 0x20 : 0) |
					((wide & 0xFF00) ? 0x10 : 0);
				WGB_CPU_REG.a = result;
				WGB_CPU_REG.f.reg = flags;
				break;
			}
			case 3: /* SBC A, r */
			{
				const uint8_t carry = (WGB_CPU_REG.f.reg >> 4) & 1;
				const uint16_t wide =
					(uint16_t)a - ((uint16_t)dense_value + carry);
				result = (uint8_t)wide;
				flags = 0x40 |
					(result == 0 ? 0x80 : 0) |
					(((a ^ dense_value ^ result) & 0x10) ? 0x20 : 0) |
					((wide & 0xFF00) ? 0x10 : 0);
				WGB_CPU_REG.a = result;
				WGB_CPU_REG.f.reg = flags;
				break;
			}
			case 4: /* AND r */
				result = a & dense_value;
				WGB_CPU_REG.a = result;
				WGB_CPU_REG.f.reg = (result == 0 ? 0x80 : 0) | 0x20;
				break;
			case 5: /* XOR r */
				result = a ^ dense_value;
				WGB_CPU_REG.a = result;
				WGB_CPU_REG.f.reg = (result == 0 ? 0x80 : 0);
				break;
			case 6: /* OR r */
				result = a | dense_value;
				WGB_CPU_REG.a = result;
				WGB_CPU_REG.f.reg = (result == 0 ? 0x80 : 0);
				break;
			default: /* CP r */
			{
				const uint16_t wide = (uint16_t)a - dense_value;
				result = (uint8_t)wide;
				WGB_CPU_REG.f.reg = 0x40 |
					(result == 0 ? 0x80 : 0) |
					(((a ^ dense_value ^ result) & 0x10) ? 0x20 : 0) |
					((wide & 0xFF00) ? 0x10 : 0);
				break;
			}
			}
		}
		else if(WGB_UNLIKELY(dense_opcode == 0x76))
		{
			gb->gb_halt = true;
			if(__gb_halt_fast_forward_to_vblank(gb))
				inst_cycles = 0;
		}
		else
		{
			const uint8_t source = dense_opcode & 7;
			const uint8_t destination = (dense_opcode >> 3) & 7;

			if(WGB_UNLIKELY(source == 6))
				dense_value = __gb_read(gb, WGB_CPU_REG.hl.reg);
			else if(source == 7)
				dense_value = WGB_CPU_REG.a;
			else
				dense_value = dense_regs[(source ^ 1u) + 2u];

			if(WGB_UNLIKELY(destination == 6))
				__gb_write(gb, WGB_CPU_REG.hl.reg, dense_value);
			else if(destination == 7)
				WGB_CPU_REG.a = dense_value;
			else
				dense_regs[(destination ^ 1u) + 2u] = dense_value;
		}

		if(dense_first_phase)
		{
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
			if(dense_opcode >= 0x70 && dense_opcode <= 0x77 &&
					dense_opcode != 0x76)
				opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
			else
#endif
				opcode = (uint8_t)(oppair >> 8);
			goto threaded_dual_first_done;
		}
		goto threaded_dual_second_done;
	}
#endif

#include "walnut_cgb_threaded_dual_first.h"

threaded_dual_first_done:
	WGB_THREADED_POST_CPU_STEP(true);
	if(WGB_UNLIKELY(gb->gb_halt))
		__gb_service_halt_until_event_or_irq(gb, true);
	if(gb->gb_frame)
		goto threaded_dual_exit;

#if (WALNUT_GB_SAFE_DUALFETCH_DMA || WALNUT_GB_SAFE_DUALFETCH_MBC)
	if(gb->prefetch_invalid)
	{
		gb->prefetch_invalid = false;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
	}
#endif

	WGB_THREADED_SERVICE_INTERRUPT(1);

	WGB_CPU_REG.pc.reg++;
#if WALNUT_GB_THREADED_COMPACT_DENSE
	if(WGB_LIKELY((uint8_t)(opcode - 0x40) < 0x80))
	{
		if(opcode < 0x80)
		{
			const uint8_t source = opcode & 7;
			const uint8_t destination = (opcode >> 3) & 7;
			inst_cycles = (opcode != 0x76 &&
				(source == 6 || destination == 6)) ? 8 : 4;
		}
		else
			inst_cycles = ((opcode & 7) == 6) ? 8 : 4;
		goto threaded_dual_second_dense;
	}
#endif
	inst_cycles = op_cycles[opcode];
	goto *second_dispatch[opcode];

#include "walnut_cgb_threaded_dual_second.h"

threaded_dual_second_done:
	WGB_THREADED_POST_CPU_STEP(false);
	if(!gb->gb_frame)
		goto threaded_dual_first_dispatch;

threaded_dual_exit:
#if WALNUT_GB_LAZY_POST_ACTIVE
	/* Counters must be exact for the host and the next frame entry. */
	__gb_lazy_post_flush_gb(gb);
	gb->lazy_post_pending = NULL;
#endif
#if WALNUT_GB_THREADED_LOCAL_CPU_REGS
	gb->cpu_reg = wgb_cpu_reg;
#undef __gb_fetch8
#undef __gb_fetch16
#undef __gb_execute_cb
#if WALNUT_GB_FAST_STAT_POLL_ACTIVE || WALNUT_GB_FAST_POLL_WAIT_ACTIVE
#undef __gb_fast_poll_wait_maybe
#endif
#if WALNUT_GB_FAST_STAT_POLL_ACTIVE
#undef __gb_fast_stat_poll_read
#undef __gb_fast_stat_poll_c_read
#endif
#undef WGB_CPU_REG
#define WGB_CPU_REG gb->cpu_reg
#endif
#if WALNUT_GB_THREADED_FAST_MEM
#undef __gb_read
#undef __gb_read16
#undef __gb_write
#endif
#undef WGB_THREADED_SERVICE_INTERRUPT
#undef WGB_THREADED_POST_CPU_STEP
}
#endif

void gb_run_frame(struct gb_s *gb)
{
	gb->gb_frame = false;
#if (WALNUT_GB_SAFE_DUALFETCH_DMA || WALNUT_GB_SAFE_DUALFETCH_MBC)
	gb->prefetch_invalid=false; // this is toggled internally, only needs to be set once - 60 times a second is no harm though
#endif
	while(!gb->gb_frame)
	{
		__gb_step_cpu_x(gb);
	}
}

void gb_run_frame_dualfetch(struct gb_s *gb)
{
	gb->gb_frame = false;
#if (WALNUT_GB_SAFE_DUALFETCH_DMA || WALNUT_GB_SAFE_DUALFETCH_MBC)
	gb->prefetch_invalid=false; // this is toggled internally, only needs to be set once - 60 times a second is no harm though
#endif
	while(!gb->gb_frame)
	{
			__gb_step_cpu(gb);
	}
}

int gb_get_save_size_s(struct gb_s *gb, size_t *ram_size)
{
	const uint_fast16_t ram_size_location = 0x0149;
	const uint_fast32_t ram_sizes[] =
	{
		/* 0,  2KiB,   8KiB,  32KiB,  128KiB,   64KiB */
		0x00, 0x800, 0x2000, 0x8000, 0x20000, 0x10000
	};
	uint8_t ram_size_code = gb->gb_rom_read(gb, ram_size_location);

	/* MBC2 always has 512 half-bytes of cart RAM.
	 * This assumes that only the lower nibble of each byte is used; the
	 * nibbles are not packed. */
	if(gb->mbc == 2)
	{
		*ram_size = 0x200;
		return 0;
	}

	/* Return -1 on invalid or unsupported RAM size. */
	if(ram_size_code >= WALNUT_GB_ARRAYSIZE(ram_sizes))
		return -1;

	*ram_size = ram_sizes[ram_size_code];
	return 0;
}

WGB_DEPRECATED("Does not return error code. Use gb_get_save_size_s instead.")
uint_fast32_t gb_get_save_size(struct gb_s *gb)
{
	const uint_fast16_t ram_size_location = 0x0149;
	const uint_fast32_t ram_sizes[] =
	{
		/* 0,  2KiB,   8KiB,  32KiB,  128KiB,   64KiB */
		0x00, 0x800, 0x2000, 0x8000, 0x20000, 0x10000
	};
	uint8_t ram_size_code = gb->gb_rom_read(gb, ram_size_location);

	/* MBC2 always has 512 half-bytes of cart RAM.
	 * This assumes that only the lower nibble of each byte is used; the
	 * nibbles are not packed. */
	if(gb->mbc == 2)
		return 0x200;

	/* Return 0 on invalid or unsupported RAM size. */
	if(ram_size_code >= WALNUT_GB_ARRAYSIZE(ram_sizes))
		return 0;

	return ram_sizes[ram_size_code];
}

void gb_init_serial(struct gb_s *gb,
		    void (*gb_serial_tx)(struct gb_s*, const uint8_t),
		    enum gb_serial_rx_ret_e (*gb_serial_rx)(struct gb_s*,
			    uint8_t*))
{
	gb->gb_serial_tx = gb_serial_tx;
	gb->gb_serial_rx = gb_serial_rx;
}

uint8_t gb_colour_hash(struct gb_s *gb)
{
#define ROM_TITLE_START_ADDR	0x0134
#define ROM_TITLE_END_ADDR	0x0143

	uint8_t x = 0;
	uint16_t i;

	for(i = ROM_TITLE_START_ADDR; i <= ROM_TITLE_END_ADDR; i++)
		x += gb->gb_rom_read(gb, i);

	return x;
}

/**
 * Resets the context, and initialises startup values for a DMG console.
 */
void gb_reset(struct gb_s *gb)
{
	__gb_oam_cache_invalidate(gb);
	gb->gb_halt = false;
	gb->gb_ime = true;

	/* Initialise MBC values. */
	gb->selected_rom_bank = 1;
	gb->cart_ram_bank = 0;
	gb->enable_cart_ram = 0;
	gb->cart_mode_select = 0;
	if(gb->cart_is_pocket_camera)
		memset(gb->pocket_camera_regs, 0, sizeof(gb->pocket_camera_regs));
	__gb_refresh_rom_bank_offset(gb);

	/* Use values as though the boot ROM was already executed. */
	if(gb->gb_bootrom_read == NULL)
	{
		uint8_t hdr_chk;
		hdr_chk = gb->gb_rom_read(gb, ROM_HEADER_CHECKSUM_LOC) != 0;

		gb->cpu_reg.a = 0x01;
		gb->cpu_reg.f.f_bits.z = 1;
		gb->cpu_reg.f.f_bits.n = 0;
		gb->cpu_reg.f.f_bits.h = hdr_chk;
		gb->cpu_reg.f.f_bits.c = hdr_chk;
		gb->cpu_reg.bc.reg = 0x0013;
		gb->cpu_reg.de.reg = 0x00D8;
		gb->cpu_reg.hl.reg = 0x014D;
		gb->cpu_reg.sp.reg = 0xFFFE;
		gb->cpu_reg.pc.reg = 0x0100;

		gb->hram_io[IO_DIV ] = 0xAB;
		gb->hram_io[IO_LCDC] = 0x91;
		gb->hram_io[IO_STAT] = 0x85;
		gb->hram_io[IO_BOOT] = 0x01;

		__gb_write(gb, 0xFF26, 0xF1);
#if WALNUT_FULL_GBC_SUPPORT
		if(gb->cgb.cgbMode)
		{
			gb->cpu_reg.a = 0x11;
			gb->cpu_reg.f.f_bits.z = 1;
			gb->cpu_reg.f.f_bits.n = 0;
			gb->cpu_reg.f.f_bits.h = hdr_chk;
			gb->cpu_reg.f.f_bits.c = hdr_chk;
			gb->cpu_reg.bc.reg = 0x0000;
			gb->cpu_reg.de.reg = 0x0008;
			gb->cpu_reg.hl.reg = 0x007C;
			gb->hram_io[IO_DIV] = 0xFF;
		}
#endif

		memset(gb->vram, 0x00, VRAM_SIZE);
	}
	else
	{
		/* Set value as though the console was just switched on.
		 * CPU registers are uninitialised. */
		gb->cpu_reg.pc.reg = 0x0000;
		gb->hram_io[IO_DIV ] = 0x00;
		gb->hram_io[IO_LCDC] = 0x00;
		gb->hram_io[IO_STAT] = 0x84;
		gb->hram_io[IO_BOOT] = 0x00;
	}

	gb->counter.lcd_count = 0;
	gb->counter.div_count = 0;
	gb->counter.tima_count = 0;
	gb->counter.serial_count = 0;
	gb->counter.rtc_count = 0;
	gb->counter.lcd_off_count = 0;
	gb->counter.lcd_event_count = 0;
	gb->counter.cpu_event_count = 0;
	gb->counter.cpu_event_flags = 0;

	gb->direct.joypad = 0xFF;
	gb->hram_io[IO_JOYP] = 0xCF;
	gb->hram_io[IO_SB  ] = 0x00;
	gb->hram_io[IO_SC  ] = 0x7E;
#if WALNUT_FULL_GBC_SUPPORT
	if(gb->cgb.cgbMode) gb->hram_io[IO_SC] = 0x7F;
#endif
	/* DIV */
	gb->hram_io[IO_TIMA] = 0x00;
	gb->hram_io[IO_TMA ] = 0x00;
	gb->hram_io[IO_TAC ] = 0xF8;
	gb->hram_io[IO_IF  ] = 0xE1;

	/* LCDC */
	/* STAT */
	gb->hram_io[IO_SCY ] = 0x00;
	gb->hram_io[IO_SCX ] = 0x00;
	gb->hram_io[IO_LY  ] = 0x00;
	gb->hram_io[IO_LYC ] = 0x00;
	__gb_write(gb, 0xFF47, 0xFC); // BGP
	__gb_write(gb, 0xFF48, 0xFF); // OBJP0
	__gb_write(gb, 0xFF49, 0xFF); // OBJP1
	gb->hram_io[IO_WY] = 0x00;
	gb->hram_io[IO_WX] = 0x00;
	gb->hram_io[IO_IE] = 0x00;
	gb->hram_io[IO_IF] = 0xE1;
#if WALNUT_FULL_GBC_SUPPORT
	/* Initialize some CGB registers */
	gb->cgb.doubleSpeed = 0;
	gb->cgb.doubleSpeedPrep = 0;
	gb->cgb.wramBank = 1;
	gb->cgb.wramBankOffset = WRAM_0_ADDR;
	gb->cgb.vramBank = 0;
	gb->cgb.vramBankOffset = VRAM_ADDR;
	for (int i = 0; i < 0x20; i++)
	{
		gb->cgb.OAMPalette[(i << 1)] = gb->cgb.BGPalette[(i << 1)] = 0x7F;
		gb->cgb.OAMPalette[(i << 1) + 1] = gb->cgb.BGPalette[(i << 1) + 1] = 0xFF;
	}
	gb->cgb.OAMPaletteID = 0;
	gb->cgb.BGPaletteID = 0;
	gb->cgb.OAMPaletteInc = 0;
	gb->cgb.BGPaletteInc = 0;
	gb->cgb.dmaActive = 1;  // Not active
	gb->cgb.dmaMode = 0;
	gb->cgb.dmaSize = 0;
	gb->cgb.dmaSource = 0;
	gb->cgb.dmaDest = 0;
#endif
#if WALNUT_GB_DIRTY_FRAME_SKIP
	gb->display.frame_dirty = true;
	gb->display.frame_dirty_active = true;
#endif
	__gb_lcd_reschedule(gb);
}

enum gb_init_error_e gb_init(struct gb_s *gb,
			     uint8_t (*gb_rom_read)(struct gb_s*, const uint_fast32_t),
					 uint16_t (*gb_rom_read16)(struct gb_s*, const uint_fast32_t),
					 uint32_t (*gb_rom_read32)(struct gb_s*, const uint_fast32_t),
			     uint8_t (*gb_cart_ram_read)(struct gb_s*, const uint_fast32_t),
			     void (*gb_cart_ram_write)(struct gb_s*, const uint_fast32_t, const uint8_t),
			     void (*gb_error)(struct gb_s*, const enum gb_error_e, const uint16_t),
			     void *priv)
{
#if WALNUT_FULL_GBC_SUPPORT
	const uint16_t cgb_flag = 0x0143;
#endif
	const uint16_t mbc_location = 0x0147;
	const uint16_t bank_count_location = 0x0148;
	const uint16_t ram_size_location = 0x0149;
	/**
	 * Table for cartridge type (MBC). -1 if invalid.
	 * TODO: MMM01 is untested.
	 * TODO: MBC6 is untested.
	 * TODO: MBC7 is unsupported.
	 * TODO: BANDAI TAMA5 is unsupported.
	 * TODO: HuC3 is unsupported.
	 * TODO: HuC1 is unsupported.
	 **/
	const int8_t cart_mbc[] =
	{
		0, 1, 1, 1, -1, 2, 2, -1, 0, 0, -1, 0, 0, 0, -1, 3,
		3, 3, 3, 3, -1, -1, -1, -1, -1, 5, 5, 5, 5, 5, 5, -1
	};
	/* Whether cart has RAM. */
	const uint8_t cart_ram[] =
	{
		0, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0,
		1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0
	};
	/* How large the ROM is in banks of 16 KiB. */
	const uint16_t num_rom_banks_mask[] =
	{
		2, 4, 8, 16, 32, 64, 128, 256, 512
	};
	/* How large the cart RAM is in banks of 8 KiB. Code $01 is unused, but
	 * some early homebrew ROMs supposedly may use this value. */
	const uint8_t num_ram_banks[] = { 0, 1, 1, 4, 16, 8 };

	gb->gb_rom_read = gb_rom_read;
	gb->gb_rom_read_16bit = gb_rom_read16;
	gb->gb_rom_read_32bit = gb_rom_read32;
	gb->gb_cart_ram_read = gb_cart_ram_read;
	gb->gb_cart_ram_write = gb_cart_ram_write;
	gb->gb_error = gb_error;
	gb->direct.priv = priv;
	gb->direct.rom_data = NULL;
	gb->direct.rom_data_len = 0;
	gb->cart_is_mbc3O = false;
	gb->cart_is_pocket_camera = false;

	/* Initialise serial transfer function to NULL. If the front-end does
	 * not provide serial support, Walnut-GB will emulate no cable connected
	 * automatically. */
	gb->gb_serial_tx = NULL;
	gb->gb_serial_rx = NULL;

	gb->gb_bootrom_read = NULL;
	gb->lazy_post_pending = NULL;

	/* Check valid ROM using checksum value. */
	{
		uint8_t x = 0;
		uint16_t i;

		for(i = 0x0134; i <= 0x014C; i++)
			x = x - gb->gb_rom_read(gb, i) - 1;

		if(x != gb->gb_rom_read(gb, ROM_HEADER_CHECKSUM_LOC))
			return GB_INIT_INVALID_CHECKSUM;
	}

	/* Check if cartridge type is supported, and set MBC type. */
	{
#if WALNUT_FULL_GBC_SUPPORT
		gb->cgb.cgbMode = (gb_rom_read(gb, cgb_flag) & 0x80) >> 7;
#endif
		const uint8_t mbc_value = gb->gb_rom_read(gb, mbc_location);
		const bool pocket_camera = mbc_value == 0xFC;

		if(pocket_camera)
		{
			gb->mbc = 5;
			gb->cart_ram = 1;
			gb->cart_is_pocket_camera = true;
		}
		else if(mbc_value > sizeof(cart_mbc) - 1 ||
				(gb->mbc = cart_mbc[mbc_value]) == -1)
		{
			return GB_INIT_CARTRIDGE_UNSUPPORTED;
		}
		else
		{
			gb->cart_ram = cart_ram[mbc_value];
		}

		gb->mbc5_ram_bank_mask =
			pocket_camera ? 0x1F :
			(mbc_value == 0x1C || mbc_value == 0x1D || mbc_value == 0x1E) ?
			0x07 : 0x0F;
	}

	gb->num_rom_banks_mask = num_rom_banks_mask[gb_rom_read(gb, bank_count_location)] - 1;
	gb->num_ram_banks = num_ram_banks[gb_rom_read(gb, ram_size_location)];

	/* If the ROM says that it support RAM, but has 0 RAM banks, then
	 * disable RAM reads from the cartridge. */
	if(gb->cart_ram == 0 || gb->num_ram_banks == 0)
	{
		gb->cart_ram = 0;
		gb->num_ram_banks = 0;
	}

	/* If MBC3 and number of ROM or RAM banks are larger than 128 or 8,
	 * respectively, then select MBC3O mode. */
	if(gb->mbc == 3)
		gb->cart_is_mbc3O = gb->num_rom_banks_mask > 128 || gb->num_ram_banks > 4;

	/* Note that MBC2 will appear to have no RAM banks, but it actually
	 * always has 512 half-bytes of RAM. Hence, gb->num_ram_banks must be
	 * ignored for MBC2. */

	gb->lcd_blank = false;
	gb->display.lcd_draw_line = NULL;

	gb_reset(gb);

	return GB_INIT_NO_ERROR;
}

const char* gb_get_rom_name(struct gb_s* gb, char *title_str)
{
	uint_fast16_t title_loc = 0x134;
	/* End of title may be 0x13E for newer games. */
	const uint_fast16_t title_end = 0x143;
	const char* title_start = title_str;

	for(; title_loc <= title_end; title_loc++)
	{
		const char title_char = gb->gb_rom_read(gb, title_loc);

		if(title_char >= ' ' && title_char <= '_')
		{
			*title_str = title_char;
			title_str++;
		}
		else
			break;
	}

	*title_str = '\0';
	return title_start;
}

#if ENABLE_LCD
void gb_init_lcd(struct gb_s *gb,
		void (*lcd_draw_line)(struct gb_s *gb,
			const uint8_t *pixels,
			const uint_fast8_t line))
{
	gb->display.lcd_draw_line = lcd_draw_line;

	gb->direct.interlace = false;
	gb->display.interlace_count = false;
	gb->direct.frame_skip = false;
	gb->direct.suppress_lcd_stat_interrupts = false;
	gb->display.frame_skip_count = false;

	gb->display.window_clear = 0;
	gb->display.WY = 0;
#if WALNUT_GB_DIRTY_FRAME_SKIP
	gb->display.frame_dirty = true;
	gb->display.frame_dirty_active = true;
#endif

	return;
}
#endif

void gb_set_bootrom(struct gb_s *gb,
		 uint8_t (*gb_bootrom_read)(struct gb_s*, const uint_fast16_t))
{
	gb->gb_bootrom_read = gb_bootrom_read;
}

/**
 * Deprecated. Will be removed in the next major version.
 */
WGB_DEPRECATED("RTC is now ticked internally; this function has no effect")
void gb_tick_rtc(struct gb_s *gb)
{
	(void) gb;
	return;
}

void gb_set_rtc(struct gb_s *gb, const struct tm * const time)
{
	gb->rtc_real.bytes[0] = time->tm_sec;
	gb->rtc_real.bytes[1] = time->tm_min;
	gb->rtc_real.bytes[2] = time->tm_hour;
	gb->rtc_real.bytes[3] = time->tm_yday & 0xFF; /* Low 8 bits of day counter. */
	gb->rtc_real.bytes[4] = time->tm_yday >> 8; /* High 1 bit of day counter. */
}
#endif // WALNUT_GB_HEADER_ONLY

/** Function prototypes: Required functions **/
/**
 * Initialises the emulator context to a known state. Call this before calling
 * any other walnut-gb function.
 * To reset the emulator, you can call gb_reset() instead.
 *
 * \param gb	Allocated emulator context. Must not be NULL.
 * \param gb_rom_read Pointer to function that reads ROM data. ROM banking is
 * 		already handled by Walnut-GB. Must not be NULL.
 * \param gb_rom_read16 Pointer to function that reads ROM data in 16-bit chunks. ROM banking is
 *      already handled by Walnut-GB. Must not be NULL.
  * \param gb_rom_read32 pointer to function that reads ROM data in 32-bit chunks. ROM banking is
 *      already handled by Walnut-GB. Must not be NULL.
 * \param gb_cart_ram_read Pointer to function that reads Cart RAM. Must not be
 * 		NULL.
 * \param gb_cart_ram_write Pointer to function to writes to Cart RAM. Must not
 * 		be NULL.
 * \param gb_error Pointer to function that is called when an unrecoverable
 *		error occurs. Must not be NULL. Returning from this
 *		function is undefined and will result in SIGABRT.
 * \param priv	Private data that is stored within the emulator context. Set to
 * 		NULL if unused.
 * \returns	0 on success or an enum that describes the error.
 */
enum gb_init_error_e gb_init(struct gb_s* gb,
	uint8_t(*gb_rom_read)(struct gb_s*, const uint_fast32_t),
	uint16_t(*gb_rom_read16)(struct gb_s*, const uint_fast32_t),
	uint32_t(*gb_rom_read32)(struct gb_s*, const uint_fast32_t),
	uint8_t(*gb_cart_ram_read)(struct gb_s*, const uint_fast32_t),
	void (*gb_cart_ram_write)(struct gb_s*, const uint_fast32_t, const uint8_t),
	void (*gb_error)(struct gb_s*, const enum gb_error_e, const uint16_t),
	void* priv);
/**
 * Executes the emulator and runs for the duration of time equal to one frame.
 *
 * \param	An initialised emulator context. Must not be NULL.
 */
void gb_run_frame(struct gb_s *gb);
#if WALNUT_GB_THREADED_CPU_ACTIVE
void gb_run_frame_threaded(struct gb_s *gb);
#endif

/**
 * Internal function used to step the CPU. Used mainly for testing.
 * Use gb_run_frame() instead.
 *
 * \param	An initialised emulator context. Must not be NULL.
 */
static inline void __gb_step_cpu(struct gb_s *gb);

/** Function prototypes: Optional Functions **/
/**
 * Reset the emulator, like turning the Game Boy off and on again.
 * This function can be called at any time.
 *
 * \param	An initialised emulator context. Must not be NULL.
 */
void gb_reset(struct gb_s *gb);

/**
 * Initialises the display context of the emulator. Only available when
 * ENABLE_LCD is defined to a non-zero value.
 * The pixel data sent to lcd_draw_line comes with both shade and layer data.
 * The first two least significant bits are the shade data (black, dark, light,
 * white). Bits 4 and 5 are layer data (OBJ0, OBJ1, BG), which can be used to
 * add more colours to the game in the same way that the Game Boy Color does to
 * older Game Boy games.
 * This function can be called at any time.
 *
 * \param gb	An initialised emulator context. Must not be NULL.
 * \param lcd_draw_line Pointer to function that draws the 2-bit pixel data on the line
 *		"line". Must not be NULL.
 */
#if ENABLE_LCD
void gb_init_lcd(struct gb_s *gb,
		void (*lcd_draw_line)(struct gb_s *gb,
			const uint8_t *pixels,
			const uint_fast8_t line));
#endif

/**
 * Initialises the serial connection of the emulator. This function is optional,
 * and if not called, the emulator will assume that no link cable is connected
 * to the game.
 *
 * \param gb	An initialised emulator context. Must not be NULL.
 * \param gb_serial_tx Pointer to function that transmits a byte of data over
 *		the serial connection. Must not be NULL.
 * \param gb_serial_rx Pointer to function that receives a byte of data over the
 *		serial connection. If no byte is received,
 *		return GB_SERIAL_RX_NO_CONNECTION. Must not be NULL.
 */
void gb_init_serial(struct gb_s *gb,
		    void (*gb_serial_tx)(struct gb_s*, const uint8_t),
		    enum gb_serial_rx_ret_e (*gb_serial_rx)(struct gb_s*,
			    uint8_t*));

/**
 * Obtains the save size of the game (size of the Cart RAM). Required by the
 * frontend to allocate enough memory for the Cart RAM.
 *
 * \param gb	An initialised emulator context. Must not be NULL.
 * \param ram_size Pointer to size_t variable that will be set to the size of
 *		the Cart RAM in bytes. Must not be NULL.
 *		If the Cart RAM is not battery backed, this will be set to 0.
 *		If the Cart RAM size is invalid or unknown, this will not be
 *		set.
 * \returns	0 on success, or -1 if the RAM size is invalid or unknown.
 */
int gb_get_save_size_s(struct gb_s *gb, size_t *ram_size);

/**
 * Deprecated. Use gb_get_save_size_s() instead.
 * Obtains the save size of the game (size of the Cart RAM). Required by the
 * frontend to allocate enough memory for the Cart RAM.
 *
 * \param gb	An initialised emulator context. Must not be NULL.
 * \returns	Size of the Cart RAM in bytes. 0 if Cartridge has not battery
 *		backed RAM.
 *		0 is also returned on invalid or unknown RAM size.
 */
uint_fast32_t gb_get_save_size(struct gb_s *gb);

/**
 * Calculates and returns a hash of the game header in the same way the Game
 * Boy Color does for colourising old Game Boy games. The frontend can use this
 * hash to automatically set a colour palette.
 *
 * \param gb	An initialised emulator context. Must not be NULL.
 * \returns	Hash of the game header.
 */
uint8_t gb_colour_hash(struct gb_s *gb);

/**
 * Returns the title of ROM.
 *
 * \param gb	An initialised emulator context. Must not be NULL.
 * \param title_str Allocated string at least 16 characters.
 * \returns	Pointer to start of string, null terminated.
 */
const char* gb_get_rom_name(struct gb_s* gb, char *title_str);

/**
 * Deprecated. Will be removed in the next major version.
 * RTC is ticked internally and this function has no effect.
 */
void gb_tick_rtc(struct gb_s *gb);

/**
 * Set initial values in RTC.
 * Should be called after gb_init().
 *
 * \param gb	An initialised emulator context. Must not be NULL.
 * \param time	Time structure with date and time.
 */
void gb_set_rtc(struct gb_s *gb, const struct tm * const time);

/**
 * Use boot ROM on reset. gb_reset() must be called for this to take affect.
 * \param gb 	An initialised emulator context. Must not be NULL.
 * \param gb_bootrom_read Function pointer to read boot ROM binary.
 */
void gb_set_bootrom(struct gb_s *gb,
	uint8_t (*gb_bootrom_read)(struct gb_s*, const uint_fast16_t));

/* Steps the cpu by one instruction, this is the original Peanut-GB dispatch method here for compatibility, or for burning cycles inefficiently if doing preemptive execution */
void __gb_step_cpu_x(struct gb_s *gb)
{
	uint8_t opcode;
	uint_fast16_t inst_cycles;
	static const uint8_t op_cycles[0x100] =
	{
		/* *INDENT-OFF* */
		/*0 1 2  3  4  5  6  7  8  9  A  B  C  D  E  F	*/
		4,12, 8, 8, 4, 4, 8, 4,20, 8, 8, 8, 4, 4, 8, 4,	/* 0x00 */
		4,12, 8, 8, 4, 4, 8, 4,12, 8, 8, 8, 4, 4, 8, 4,	/* 0x10 */
		8,12, 8, 8, 4, 4, 8, 4, 8, 8, 8, 8, 4, 4, 8, 4,	/* 0x20 */
		8,12, 8, 8,12,12,12, 4, 8, 8, 8, 8, 4, 4, 8, 4,	/* 0x30 */
		4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,	/* 0x40 */
		4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,	/* 0x50 */
		4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,	/* 0x60 */
		8, 8, 8, 8, 8, 8, 4, 8, 4, 4, 4, 4, 4, 4, 8, 4, /* 0x70 */
		4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,	/* 0x80 */
		4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,	/* 0x90 */
		4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,	/* 0xA0 */
		4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,	/* 0xB0 */
		8,12,12,16,12,16, 8,16, 8,16,12, 8,12,24, 8,16,	/* 0xC0 */
		8,12,12, 0,12,16, 8,16, 8,16,12, 0,12, 0, 8,16,	/* 0xD0 */
		12,12,8, 0, 0,16, 8,16,16, 4,16, 0, 0, 0, 8,16,	/* 0xE0 */
		12,12,8, 4, 0,16, 8,16,12, 8,16, 4, 0, 0, 8,16	/* 0xF0 */
		/* *INDENT-ON* */
	};
	static const uint_fast16_t TAC_CYCLES[4] = {1024, 16, 64, 256};

	/* Handle interrupts */
	/* If gb_halt is positive, then an interrupt must have occurred by the
	 * time we reach here, because on HALT, we jump to the next interrupt
	 * immediately. */
	while((gb->gb_halt || gb->gb_ime) && __gb_effective_pending_interrupts(gb))
	{
		gb->gb_halt = false;

		if(!gb->gb_ime)
			break;

		/* Disable interrupts */
		gb->gb_ime = false;

		/* Push Program Counter */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);

		/* Call interrupt handler if required. */
		if(__gb_effective_pending_interrupts(gb) & VBLANK_INTR)
		{
			gb->cpu_reg.pc.reg = VBLANK_INTR_ADDR;
			gb->hram_io[IO_IF] ^= VBLANK_INTR;
		}
		else if(__gb_effective_pending_interrupts(gb) & LCDC_INTR)
		{
			gb->cpu_reg.pc.reg = LCDC_INTR_ADDR;
			gb->hram_io[IO_IF] ^= LCDC_INTR;
		}
		else if(__gb_effective_pending_interrupts(gb) & TIMER_INTR)
		{
			gb->cpu_reg.pc.reg = TIMER_INTR_ADDR;
			gb->hram_io[IO_IF] ^= TIMER_INTR;
		}
		else if(__gb_effective_pending_interrupts(gb) & SERIAL_INTR)
		{
			gb->cpu_reg.pc.reg = SERIAL_INTR_ADDR;
			gb->hram_io[IO_IF] ^= SERIAL_INTR;
		}
		else if(__gb_effective_pending_interrupts(gb) & CONTROL_INTR)
		{
			gb->cpu_reg.pc.reg = CONTROL_INTR_ADDR;
			gb->hram_io[IO_IF] ^= CONTROL_INTR;
		}

		break;
	}

	/* Obtain opcode */
	opcode = __gb_fetch8(gb);
	inst_cycles = op_cycles[opcode];

	/* Execute opcode */
	switch(opcode)
	{
	case 0x00: /* NOP */
		break;

	case 0x01: /* LD BC, imm */
#if WALNUT_GB_16_BIT_DISABLED
    gb->cpu_reg.bc.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
    gb->cpu_reg.pc.reg += 2;
#else
    gb->cpu_reg.bc.reg = __gb_fetch16(gb);
#endif
		break;
	case 0x02: /* LD (BC), A */
		__gb_write(gb, gb->cpu_reg.bc.reg, gb->cpu_reg.a);
		break;

	case 0x03: /* INC BC */
		gb->cpu_reg.bc.reg++;
		break;

	case 0x04: /* INC B */
		WGB_INSTR_INC_R8(gb->cpu_reg.bc.bytes.b);
		break;

	case 0x05: /* DEC B */
		WGB_INSTR_DEC_R8(gb->cpu_reg.bc.bytes.b);
		break;

	case 0x06: /* LD B, imm */
		gb->cpu_reg.bc.bytes.b = __gb_fetch8(gb);
		break;

	case 0x07: /* RLCA */
		gb->cpu_reg.a = (gb->cpu_reg.a << 1) | (gb->cpu_reg.a >> 7);
		gb->cpu_reg.f.reg = 0;
		gb->cpu_reg.f.f_bits.c = (gb->cpu_reg.a & 0x01);
		break;

	case 0x08: /* LD (imm), SP */
	{
#if WALNUT_GB_16_BIT_DISABLED
    uint16_t temp = __gb_read16(gb, gb->cpu_reg.pc.reg);
    gb->cpu_reg.pc.reg += 2;
#else
    uint16_t temp = __gb_fetch16(gb);
#endif
    __gb_write(gb, temp++, gb->cpu_reg.sp.bytes.p);
    __gb_write(gb, temp, gb->cpu_reg.sp.bytes.s);
    break;
	}

	case 0x09: /* ADD HL, BC */
	{
		uint_fast32_t temp = gb->cpu_reg.hl.reg + gb->cpu_reg.bc.reg;
		gb->cpu_reg.f.f_bits.n = 0;
		gb->cpu_reg.f.f_bits.h =
			(temp ^ gb->cpu_reg.hl.reg ^ gb->cpu_reg.bc.reg) & 0x1000 ? 1 : 0;
		gb->cpu_reg.f.f_bits.c = (temp & 0xFFFF0000) ? 1 : 0;
		gb->cpu_reg.hl.reg = (temp & 0x0000FFFF);
		break;
	}

	case 0x0A: /* LD A, (BC) */
		gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.bc.reg);
		break;

	case 0x0B: /* DEC BC */
		gb->cpu_reg.bc.reg--;
		break;

	case 0x0C: /* INC C */
		WGB_INSTR_INC_R8(gb->cpu_reg.bc.bytes.c);
		break;

	case 0x0D: /* DEC C */
		WGB_INSTR_DEC_R8(gb->cpu_reg.bc.bytes.c);
		break;

	case 0x0E: /* LD C, imm */
		gb->cpu_reg.bc.bytes.c = __gb_fetch8(gb);
		break;

	case 0x0F: /* RRCA */
		gb->cpu_reg.f.reg = 0;
		gb->cpu_reg.f.f_bits.c = gb->cpu_reg.a & 0x01;
		gb->cpu_reg.a = (gb->cpu_reg.a >> 1) | (gb->cpu_reg.a << 7);
		break;

	case 0x10: /* STOP */
		//gb->gb_halt = true;
#if WALNUT_FULL_GBC_SUPPORT
		if(gb->cgb.cgbMode & gb->cgb.doubleSpeedPrep)
		{
			gb->cgb.doubleSpeedPrep = 0;
			gb->cgb.doubleSpeed ^= 1;
			__gb_cpu_timing_reschedule(gb);
		}
#endif
		/* STOP is two bytes; skip the padding byte. */
		gb->cpu_reg.pc.reg++;
		break;

	case 0x11: /* LD DE, imm */
#if WALNUT_GB_16_BIT_DISABLED
    gb->cpu_reg.de.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
    gb->cpu_reg.pc.reg += 2;
#else
    gb->cpu_reg.de.reg = __gb_fetch16(gb);
#endif
		break;

	case 0x12: /* LD (DE), A */
		__gb_write(gb, gb->cpu_reg.de.reg, gb->cpu_reg.a);
		break;

	case 0x13: /* INC DE */
		gb->cpu_reg.de.reg++;
		break;

	case 0x14: /* INC D */
		WGB_INSTR_INC_R8(gb->cpu_reg.de.bytes.d);
		break;

	case 0x15: /* DEC D */
		WGB_INSTR_DEC_R8(gb->cpu_reg.de.bytes.d);
		break;

	case 0x16: /* LD D, imm */
		gb->cpu_reg.de.bytes.d = __gb_fetch8(gb);
		break;

	case 0x17: /* RLA */
	{
		uint8_t temp = gb->cpu_reg.a;
		gb->cpu_reg.a = (gb->cpu_reg.a << 1) | gb->cpu_reg.f.f_bits.c;
		gb->cpu_reg.f.reg = 0;
		gb->cpu_reg.f.f_bits.c = (temp >> 7) & 0x01;
		break;
	}

	case 0x18: /* JR imm */
	{
		int8_t temp = (int8_t) __gb_fetch8(gb);
		gb->cpu_reg.pc.reg += temp;
		break;
	}

	case 0x19: /* ADD HL, DE */
	{
		uint_fast32_t temp = gb->cpu_reg.hl.reg + gb->cpu_reg.de.reg;
		gb->cpu_reg.f.f_bits.n = 0;
		gb->cpu_reg.f.f_bits.h =
			(temp ^ gb->cpu_reg.hl.reg ^ gb->cpu_reg.de.reg) & 0x1000 ? 1 : 0;
		gb->cpu_reg.f.f_bits.c = (temp & 0xFFFF0000) ? 1 : 0;
		gb->cpu_reg.hl.reg = (temp & 0x0000FFFF);
		break;
	}

	case 0x1A: /* LD A, (DE) */
		gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.de.reg);
		break;

	case 0x1B: /* DEC DE */
		gb->cpu_reg.de.reg--;
		break;

	case 0x1C: /* INC E */
		WGB_INSTR_INC_R8(gb->cpu_reg.de.bytes.e);
		break;

	case 0x1D: /* DEC E */
		WGB_INSTR_DEC_R8(gb->cpu_reg.de.bytes.e);
		break;

	case 0x1E: /* LD E, imm */
		gb->cpu_reg.de.bytes.e = __gb_fetch8(gb);
		break;

	case 0x1F: /* RRA */
	{
		uint8_t temp = gb->cpu_reg.a;
		gb->cpu_reg.a = gb->cpu_reg.a >> 1 | (gb->cpu_reg.f.f_bits.c << 7);
		gb->cpu_reg.f.reg = 0;
		gb->cpu_reg.f.f_bits.c = temp & 0x1;
		break;
	}

	case 0x20: /* JR NZ, imm */
		if(!gb->cpu_reg.f.f_bits.z)
		{
			const uint16_t branch_pc = (uint16_t)(gb->cpu_reg.pc.reg - 1);
			int8_t temp = (int8_t) __gb_fetch8(gb);
			gb->cpu_reg.pc.reg += temp;
			inst_cycles += 4;
			__gb_fast_poll_wait_maybe(gb, gb->cpu_reg.pc.reg, branch_pc, 0x20);
		}
		else
			gb->cpu_reg.pc.reg++;

		break;

	case 0x21: /* LD HL, imm */
#if WALNUT_GB_16_BIT_DISABLED
    gb->cpu_reg.hl.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
    gb->cpu_reg.pc.reg += 2;
#else
    gb->cpu_reg.hl.reg = __gb_fetch16(gb);
#endif
		break;

	case 0x22: /* LDI (HL), A */
		__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.a);
		gb->cpu_reg.hl.reg++;
		break;

	case 0x23: /* INC HL */
		gb->cpu_reg.hl.reg++;
		break;

	case 0x24: /* INC H */
		WGB_INSTR_INC_R8(gb->cpu_reg.hl.bytes.h);
		break;

	case 0x25: /* DEC H */
		WGB_INSTR_DEC_R8(gb->cpu_reg.hl.bytes.h);
		break;

	case 0x26: /* LD H, imm */
		gb->cpu_reg.hl.bytes.h = __gb_fetch8(gb);
		break;

	case 0x27: /* DAA */
	{
		/* The following is from SameBoy. MIT License. */
		int16_t a = gb->cpu_reg.a;

		if(gb->cpu_reg.f.f_bits.n)
		{
			if(gb->cpu_reg.f.f_bits.h)
				a = (a - 0x06) & 0xFF;

			if(gb->cpu_reg.f.f_bits.c)
				a -= 0x60;
		}
		else
		{
			if(gb->cpu_reg.f.f_bits.h || (a & 0x0F) > 9)
				a += 0x06;

			if(gb->cpu_reg.f.f_bits.c || a > 0x9F)
				a += 0x60;
		}

		if((a & 0x100) == 0x100)
			gb->cpu_reg.f.f_bits.c = 1;

		gb->cpu_reg.a = (uint8_t)a;
		gb->cpu_reg.f.f_bits.z = (gb->cpu_reg.a == 0);
		gb->cpu_reg.f.f_bits.h = 0;

		break;
	}

	case 0x28: /* JR Z, imm */
		if(gb->cpu_reg.f.f_bits.z)
		{
			const uint16_t branch_pc = (uint16_t)(gb->cpu_reg.pc.reg - 1);
			int8_t temp = (int8_t) __gb_fetch8(gb);
			gb->cpu_reg.pc.reg += temp;
			inst_cycles += 4;
			__gb_fast_poll_wait_maybe(gb, gb->cpu_reg.pc.reg, branch_pc, 0x28);
		}
		else
			gb->cpu_reg.pc.reg++;

		break;

	case 0x29: /* ADD HL, HL */
	{
		gb->cpu_reg.f.f_bits.c = (gb->cpu_reg.hl.reg & 0x8000) > 0;
		gb->cpu_reg.hl.reg <<= 1;
		gb->cpu_reg.f.f_bits.n = 0;
		gb->cpu_reg.f.f_bits.h = (gb->cpu_reg.hl.reg & 0x1000) > 0;
		break;
	}

	case 0x2A: /* LD A, (HL+) */
		gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.hl.reg++);
		break;

	case 0x2B: /* DEC HL */
		gb->cpu_reg.hl.reg--;
		break;

	case 0x2C: /* INC L */
		WGB_INSTR_INC_R8(gb->cpu_reg.hl.bytes.l);
		break;

	case 0x2D: /* DEC L */
		WGB_INSTR_DEC_R8(gb->cpu_reg.hl.bytes.l);
		break;

	case 0x2E: /* LD L, imm */
		gb->cpu_reg.hl.bytes.l = __gb_fetch8(gb);
		break;

	case 0x2F: /* CPL */
		gb->cpu_reg.a = ~gb->cpu_reg.a;
		gb->cpu_reg.f.f_bits.n = 1;
		gb->cpu_reg.f.f_bits.h = 1;
		break;

	case 0x30: /* JR NC, imm */
		if(!gb->cpu_reg.f.f_bits.c)
		{
			const uint16_t branch_pc = (uint16_t)(gb->cpu_reg.pc.reg - 1);
			int8_t temp = (int8_t) __gb_fetch8(gb);
			gb->cpu_reg.pc.reg += temp;
			inst_cycles += 4;
#if WALNUT_GB_FAST_POLL_WAIT_ACTIVE
			if(__gb_fast_poll_wait_carry_ly_prefilter(gb,
					gb->cpu_reg.pc.reg, branch_pc))
				__gb_fast_poll_wait_maybe(gb, gb->cpu_reg.pc.reg,
					branch_pc, 0x30);
#endif
		}
		else
			gb->cpu_reg.pc.reg++;

		break;

	case 0x31: /* LD SP, imm */
#if WALNUT_GB_16_BIT_DISABLED
    gb->cpu_reg.sp.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
    gb->cpu_reg.pc.reg += 2;
#else
    gb->cpu_reg.sp.reg = __gb_fetch16(gb);
#endif
		break;

	case 0x32: /* LD (HL), A */
		__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.a);
		gb->cpu_reg.hl.reg--;
		break;

	case 0x33: /* INC SP */
		gb->cpu_reg.sp.reg++;
		break;

	case 0x34: /* INC (HL) */
	{
		uint8_t temp = __gb_read(gb, gb->cpu_reg.hl.reg);
		WGB_INSTR_INC_R8(temp);
		__gb_write(gb, gb->cpu_reg.hl.reg, temp);
		break;
	}

	case 0x35: /* DEC (HL) */
	{
		uint8_t temp = __gb_read(gb, gb->cpu_reg.hl.reg);
		WGB_INSTR_DEC_R8(temp);
		__gb_write(gb, gb->cpu_reg.hl.reg, temp);
		break;
	}

	case 0x36: /* LD (HL), imm */
		__gb_write(gb, gb->cpu_reg.hl.reg, __gb_fetch8(gb));
		break;

	case 0x37: /* SCF */
		gb->cpu_reg.f.f_bits.n = 0;
		gb->cpu_reg.f.f_bits.h = 0;
		gb->cpu_reg.f.f_bits.c = 1;
		break;

	case 0x38: /* JR C, imm */
		if(gb->cpu_reg.f.f_bits.c)
		{
			const uint16_t branch_pc = (uint16_t)(gb->cpu_reg.pc.reg - 1);
			int8_t temp = (int8_t) __gb_fetch8(gb);
			gb->cpu_reg.pc.reg += temp;
			inst_cycles += 4;
#if WALNUT_GB_FAST_POLL_WAIT_ACTIVE
			if(__gb_fast_poll_wait_carry_ly_prefilter(gb,
					gb->cpu_reg.pc.reg, branch_pc))
				__gb_fast_poll_wait_maybe(gb, gb->cpu_reg.pc.reg,
					branch_pc, 0x38);
#endif
		}
		else
			gb->cpu_reg.pc.reg++;

		break;

	case 0x39: /* ADD HL, SP */
	{
		uint_fast32_t temp = gb->cpu_reg.hl.reg + gb->cpu_reg.sp.reg;
		gb->cpu_reg.f.f_bits.n = 0;
		gb->cpu_reg.f.f_bits.h =
			((gb->cpu_reg.hl.reg & 0xFFF) + (gb->cpu_reg.sp.reg & 0xFFF)) & 0x1000 ? 1 : 0;
		gb->cpu_reg.f.f_bits.c = temp & 0x10000 ? 1 : 0;
		gb->cpu_reg.hl.reg = (uint16_t)temp;
		break;
	}

	case 0x3A: /* LD A, (HL) */
		gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.hl.reg--);
		break;

	case 0x3B: /* DEC SP */
		gb->cpu_reg.sp.reg--;
		break;

	case 0x3C: /* INC A */
		WGB_INSTR_INC_R8(gb->cpu_reg.a);
		break;

	case 0x3D: /* DEC A */
		WGB_INSTR_DEC_R8(gb->cpu_reg.a);
		break;

	case 0x3E: /* LD A, imm */
		gb->cpu_reg.a = __gb_fetch8(gb);
		break;

	case 0x3F: /* CCF */
		gb->cpu_reg.f.f_bits.n = 0;
		gb->cpu_reg.f.f_bits.h = 0;
		gb->cpu_reg.f.f_bits.c = ~gb->cpu_reg.f.f_bits.c;
		break;

	case 0x40: /* LD B, B */
		break;

	case 0x41: /* LD B, C */
		gb->cpu_reg.bc.bytes.b = gb->cpu_reg.bc.bytes.c;
		break;

	case 0x42: /* LD B, D */
		gb->cpu_reg.bc.bytes.b = gb->cpu_reg.de.bytes.d;
		break;

	case 0x43: /* LD B, E */
		gb->cpu_reg.bc.bytes.b = gb->cpu_reg.de.bytes.e;
		break;

	case 0x44: /* LD B, H */
		gb->cpu_reg.bc.bytes.b = gb->cpu_reg.hl.bytes.h;
		break;

	case 0x45: /* LD B, L */
		gb->cpu_reg.bc.bytes.b = gb->cpu_reg.hl.bytes.l;
		break;

	case 0x46: /* LD B, (HL) */
		gb->cpu_reg.bc.bytes.b = __gb_read(gb, gb->cpu_reg.hl.reg);
		break;

	case 0x47: /* LD B, A */
		gb->cpu_reg.bc.bytes.b = gb->cpu_reg.a;
		break;

	case 0x48: /* LD C, B */
		gb->cpu_reg.bc.bytes.c = gb->cpu_reg.bc.bytes.b;
		break;

	case 0x49: /* LD C, C */
		break;

	case 0x4A: /* LD C, D */
		gb->cpu_reg.bc.bytes.c = gb->cpu_reg.de.bytes.d;
		break;

	case 0x4B: /* LD C, E */
		gb->cpu_reg.bc.bytes.c = gb->cpu_reg.de.bytes.e;
		break;

	case 0x4C: /* LD C, H */
		gb->cpu_reg.bc.bytes.c = gb->cpu_reg.hl.bytes.h;
		break;

	case 0x4D: /* LD C, L */
		gb->cpu_reg.bc.bytes.c = gb->cpu_reg.hl.bytes.l;
		break;

	case 0x4E: /* LD C, (HL) */
		gb->cpu_reg.bc.bytes.c = __gb_read(gb, gb->cpu_reg.hl.reg);
		break;

	case 0x4F: /* LD C, A */
		gb->cpu_reg.bc.bytes.c = gb->cpu_reg.a;
		break;

	case 0x50: /* LD D, B */
		gb->cpu_reg.de.bytes.d = gb->cpu_reg.bc.bytes.b;
		break;

	case 0x51: /* LD D, C */
		gb->cpu_reg.de.bytes.d = gb->cpu_reg.bc.bytes.c;
		break;

	case 0x52: /* LD D, D */
		break;

	case 0x53: /* LD D, E */
		gb->cpu_reg.de.bytes.d = gb->cpu_reg.de.bytes.e;
		break;

	case 0x54: /* LD D, H */
		gb->cpu_reg.de.bytes.d = gb->cpu_reg.hl.bytes.h;
		break;

	case 0x55: /* LD D, L */
		gb->cpu_reg.de.bytes.d = gb->cpu_reg.hl.bytes.l;
		break;

	case 0x56: /* LD D, (HL) */
		gb->cpu_reg.de.bytes.d = __gb_read(gb, gb->cpu_reg.hl.reg);
		break;

	case 0x57: /* LD D, A */
		gb->cpu_reg.de.bytes.d = gb->cpu_reg.a;
		break;

	case 0x58: /* LD E, B */
		gb->cpu_reg.de.bytes.e = gb->cpu_reg.bc.bytes.b;
		break;

	case 0x59: /* LD E, C */
		gb->cpu_reg.de.bytes.e = gb->cpu_reg.bc.bytes.c;
		break;

	case 0x5A: /* LD E, D */
		gb->cpu_reg.de.bytes.e = gb->cpu_reg.de.bytes.d;
		break;

	case 0x5B: /* LD E, E */
		break;

	case 0x5C: /* LD E, H */
		gb->cpu_reg.de.bytes.e = gb->cpu_reg.hl.bytes.h;
		break;

	case 0x5D: /* LD E, L */
		gb->cpu_reg.de.bytes.e = gb->cpu_reg.hl.bytes.l;
		break;

	case 0x5E: /* LD E, (HL) */
		gb->cpu_reg.de.bytes.e = __gb_read(gb, gb->cpu_reg.hl.reg);
		break;

	case 0x5F: /* LD E, A */
		gb->cpu_reg.de.bytes.e = gb->cpu_reg.a;
		break;

	case 0x60: /* LD H, B */
		gb->cpu_reg.hl.bytes.h = gb->cpu_reg.bc.bytes.b;
		break;

	case 0x61: /* LD H, C */
		gb->cpu_reg.hl.bytes.h = gb->cpu_reg.bc.bytes.c;
		break;

	case 0x62: /* LD H, D */
		gb->cpu_reg.hl.bytes.h = gb->cpu_reg.de.bytes.d;
		break;

	case 0x63: /* LD H, E */
		gb->cpu_reg.hl.bytes.h = gb->cpu_reg.de.bytes.e;
		break;

	case 0x64: /* LD H, H */
		break;

	case 0x65: /* LD H, L */
		gb->cpu_reg.hl.bytes.h = gb->cpu_reg.hl.bytes.l;
		break;

	case 0x66: /* LD H, (HL) */
		gb->cpu_reg.hl.bytes.h = __gb_read(gb, gb->cpu_reg.hl.reg);
		break;

	case 0x67: /* LD H, A */
		gb->cpu_reg.hl.bytes.h = gb->cpu_reg.a;
		break;

	case 0x68: /* LD L, B */
		gb->cpu_reg.hl.bytes.l = gb->cpu_reg.bc.bytes.b;
		break;

	case 0x69: /* LD L, C */
		gb->cpu_reg.hl.bytes.l = gb->cpu_reg.bc.bytes.c;
		break;

	case 0x6A: /* LD L, D */
		gb->cpu_reg.hl.bytes.l = gb->cpu_reg.de.bytes.d;
		break;

	case 0x6B: /* LD L, E */
		gb->cpu_reg.hl.bytes.l = gb->cpu_reg.de.bytes.e;
		break;

	case 0x6C: /* LD L, H */
		gb->cpu_reg.hl.bytes.l = gb->cpu_reg.hl.bytes.h;
		break;

	case 0x6D: /* LD L, L */
		break;

	case 0x6E: /* LD L, (HL) */
		gb->cpu_reg.hl.bytes.l = __gb_read(gb, gb->cpu_reg.hl.reg);
		break;

	case 0x6F: /* LD L, A */
		gb->cpu_reg.hl.bytes.l = gb->cpu_reg.a;
		break;

	case 0x70: /* LD (HL), B */
		__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.bc.bytes.b);
		break;

	case 0x71: /* LD (HL), C */
		__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.bc.bytes.c);
		break;

	case 0x72: /* LD (HL), D */
		__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.de.bytes.d);
		break;

	case 0x73: /* LD (HL), E */
		__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.de.bytes.e);
		break;

	case 0x74: /* LD (HL), H */
		__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.hl.bytes.h);
		break;

	case 0x75: /* LD (HL), L */
		__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.hl.bytes.l);
		break;

	case 0x76: /* HALT */
	{
		int_fast16_t halt_cycles = INT_FAST16_MAX;

		/* TODO: Emulate HALT bug? */
		gb->gb_halt = true;

		if(__gb_halt_fast_forward_to_vblank(gb))
		{
			inst_cycles = 0;
			break;
		}

		if(gb->hram_io[IO_SC] & SERIAL_SC_TX_START)
		{
			int serial_cycles = SERIAL_CYCLES -
				gb->counter.serial_count;

			if(serial_cycles < halt_cycles)
				halt_cycles = serial_cycles;
		}

		if(gb->hram_io[IO_TAC] & IO_TAC_ENABLE_MASK)
		{
			int tac_cycles = TAC_CYCLES[gb->hram_io[IO_TAC] & IO_TAC_RATE_MASK] -
				gb->counter.tima_count;

			if(tac_cycles < halt_cycles)
				halt_cycles = tac_cycles;
		}

		if((gb->hram_io[IO_LCDC] & LCDC_ENABLE))
		{
			int lcd_cycles;

			/* If LCD is in HBlank, calculate the number of cycles
			 * until the end of HBlank and the start of mode 2 or
			 * mode 1. */
			if((gb->hram_io[IO_STAT] & STAT_MODE) == IO_STAT_MODE_HBLANK)
			{
				lcd_cycles = LCD_MODE0_HBLANK_MAX_DRUATION - gb->counter.lcd_count;
			}
			else if((gb->hram_io[IO_STAT] & STAT_MODE) == IO_STAT_MODE_OAM_SCAN)
			{
				lcd_cycles = LCD_MODE3_LCD_DRAW_MIN_DURATION - gb->counter.lcd_count;
			}
			else if((gb->hram_io[IO_STAT] & STAT_MODE) == IO_STAT_MODE_LCD_DRAW)
			{
				lcd_cycles = LCD_MODE0_HBLANK_MAX_DRUATION - gb->counter.lcd_count;
			}
			else
			{
				/* VBlank */
				lcd_cycles = LCD_LINE_CYCLES - gb->counter.lcd_count;
			}

			if(lcd_cycles < halt_cycles)
				halt_cycles = lcd_cycles;
		}

		/* Some halt cycles may already be very high, so make sure we
		 * don't underflow here. */
		if(halt_cycles <= 0)
			halt_cycles = 4;

		inst_cycles = (uint_fast16_t)halt_cycles;
		break;
	}

	case 0x77: /* LD (HL), A */
		__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.a);
		break;

	case 0x78: /* LD A, B */
		gb->cpu_reg.a = gb->cpu_reg.bc.bytes.b;
		break;

	case 0x79: /* LD A, C */
		gb->cpu_reg.a = gb->cpu_reg.bc.bytes.c;
		break;

	case 0x7A: /* LD A, D */
		gb->cpu_reg.a = gb->cpu_reg.de.bytes.d;
		break;

	case 0x7B: /* LD A, E */
		gb->cpu_reg.a = gb->cpu_reg.de.bytes.e;
		break;

	case 0x7C: /* LD A, H */
		gb->cpu_reg.a = gb->cpu_reg.hl.bytes.h;
		break;

	case 0x7D: /* LD A, L */
		gb->cpu_reg.a = gb->cpu_reg.hl.bytes.l;
		break;

	case 0x7E: /* LD A, (HL) */
		gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.hl.reg);
		break;

	case 0x7F: /* LD A, A */
		break;

	case 0x80: /* ADD A, B */
		WGB_INSTR_ADC_R8(gb->cpu_reg.bc.bytes.b, 0);
		break;

	case 0x81: /* ADD A, C */
		WGB_INSTR_ADC_R8(gb->cpu_reg.bc.bytes.c, 0);
		break;

	case 0x82: /* ADD A, D */
		WGB_INSTR_ADC_R8(gb->cpu_reg.de.bytes.d, 0);
		break;

	case 0x83: /* ADD A, E */
		WGB_INSTR_ADC_R8(gb->cpu_reg.de.bytes.e, 0);
		break;

	case 0x84: /* ADD A, H */
		WGB_INSTR_ADC_R8(gb->cpu_reg.hl.bytes.h, 0);
		break;

	case 0x85: /* ADD A, L */
		WGB_INSTR_ADC_R8(gb->cpu_reg.hl.bytes.l, 0);
		break;

	case 0x86: /* ADD A, (HL) */
		WGB_INSTR_ADC_R8(__gb_read(gb, gb->cpu_reg.hl.reg), 0);
		break;

	case 0x87: /* ADD A, A */
		WGB_INSTR_ADC_R8(gb->cpu_reg.a, 0);
		break;

	case 0x88: /* ADC A, B */
		WGB_INSTR_ADC_R8(gb->cpu_reg.bc.bytes.b, gb->cpu_reg.f.f_bits.c);
		break;

	case 0x89: /* ADC A, C */
		WGB_INSTR_ADC_R8(gb->cpu_reg.bc.bytes.c, gb->cpu_reg.f.f_bits.c);
		break;

	case 0x8A: /* ADC A, D */
		WGB_INSTR_ADC_R8(gb->cpu_reg.de.bytes.d, gb->cpu_reg.f.f_bits.c);
		break;

	case 0x8B: /* ADC A, E */
		WGB_INSTR_ADC_R8(gb->cpu_reg.de.bytes.e, gb->cpu_reg.f.f_bits.c);
		break;

	case 0x8C: /* ADC A, H */
		WGB_INSTR_ADC_R8(gb->cpu_reg.hl.bytes.h, gb->cpu_reg.f.f_bits.c);
		break;

	case 0x8D: /* ADC A, L */
		WGB_INSTR_ADC_R8(gb->cpu_reg.hl.bytes.l, gb->cpu_reg.f.f_bits.c);
		break;

	case 0x8E: /* ADC A, (HL) */
		WGB_INSTR_ADC_R8(__gb_read(gb, gb->cpu_reg.hl.reg), gb->cpu_reg.f.f_bits.c);
		break;

	case 0x8F: /* ADC A, A */
		WGB_INSTR_ADC_R8(gb->cpu_reg.a, gb->cpu_reg.f.f_bits.c);
		break;

	case 0x90: /* SUB B */
		WGB_INSTR_SBC_R8(gb->cpu_reg.bc.bytes.b, 0);
		break;

	case 0x91: /* SUB C */
		WGB_INSTR_SBC_R8(gb->cpu_reg.bc.bytes.c, 0);
		break;

	case 0x92: /* SUB D */
		WGB_INSTR_SBC_R8(gb->cpu_reg.de.bytes.d, 0);
		break;

	case 0x93: /* SUB E */
		WGB_INSTR_SBC_R8(gb->cpu_reg.de.bytes.e, 0);
		break;

	case 0x94: /* SUB H */
		WGB_INSTR_SBC_R8(gb->cpu_reg.hl.bytes.h, 0);
		break;

	case 0x95: /* SUB L */
		WGB_INSTR_SBC_R8(gb->cpu_reg.hl.bytes.l, 0);
		break;

	case 0x96: /* SUB (HL) */
		WGB_INSTR_SBC_R8(__gb_read(gb, gb->cpu_reg.hl.reg), 0);
		break;

	case 0x97: /* SUB A */
		gb->cpu_reg.a = 0;
		gb->cpu_reg.f.reg = 0;
		gb->cpu_reg.f.f_bits.z = 1;
		gb->cpu_reg.f.f_bits.n = 1;
		break;

	case 0x98: /* SBC A, B */
		WGB_INSTR_SBC_R8(gb->cpu_reg.bc.bytes.b, gb->cpu_reg.f.f_bits.c);
		break;

	case 0x99: /* SBC A, C */
		WGB_INSTR_SBC_R8(gb->cpu_reg.bc.bytes.c, gb->cpu_reg.f.f_bits.c);
		break;

	case 0x9A: /* SBC A, D */
		WGB_INSTR_SBC_R8(gb->cpu_reg.de.bytes.d, gb->cpu_reg.f.f_bits.c);
		break;

	case 0x9B: /* SBC A, E */
		WGB_INSTR_SBC_R8(gb->cpu_reg.de.bytes.e, gb->cpu_reg.f.f_bits.c);
		break;

	case 0x9C: /* SBC A, H */
		WGB_INSTR_SBC_R8(gb->cpu_reg.hl.bytes.h, gb->cpu_reg.f.f_bits.c);
		break;

	case 0x9D: /* SBC A, L */
		WGB_INSTR_SBC_R8(gb->cpu_reg.hl.bytes.l, gb->cpu_reg.f.f_bits.c);
		break;

	case 0x9E: /* SBC A, (HL) */
		WGB_INSTR_SBC_R8(__gb_read(gb, gb->cpu_reg.hl.reg), gb->cpu_reg.f.f_bits.c);
		break;

	case 0x9F: /* SBC A, A */
		gb->cpu_reg.a = gb->cpu_reg.f.f_bits.c ? 0xFF : 0x00;
		gb->cpu_reg.f.f_bits.z = !gb->cpu_reg.f.f_bits.c;
		gb->cpu_reg.f.f_bits.n = 1;
		gb->cpu_reg.f.f_bits.h = gb->cpu_reg.f.f_bits.c;
		break;

	case 0xA0: /* AND B */
		WGB_INSTR_AND_R8(gb->cpu_reg.bc.bytes.b);
		break;

	case 0xA1: /* AND C */
		WGB_INSTR_AND_R8(gb->cpu_reg.bc.bytes.c);
		break;

	case 0xA2: /* AND D */
		WGB_INSTR_AND_R8(gb->cpu_reg.de.bytes.d);
		break;

	case 0xA3: /* AND E */
		WGB_INSTR_AND_R8(gb->cpu_reg.de.bytes.e);
		break;

	case 0xA4: /* AND H */
		WGB_INSTR_AND_R8(gb->cpu_reg.hl.bytes.h);
		break;

	case 0xA5: /* AND L */
		WGB_INSTR_AND_R8(gb->cpu_reg.hl.bytes.l);
		break;

	case 0xA6: /* AND (HL) */
		WGB_INSTR_AND_R8(__gb_read(gb, gb->cpu_reg.hl.reg));
		break;

	case 0xA7: /* AND A */
		WGB_INSTR_AND_R8(gb->cpu_reg.a);
		break;

	case 0xA8: /* XOR B */
		WGB_INSTR_XOR_R8(gb->cpu_reg.bc.bytes.b);
		break;

	case 0xA9: /* XOR C */
		WGB_INSTR_XOR_R8(gb->cpu_reg.bc.bytes.c);
		break;

	case 0xAA: /* XOR D */
		WGB_INSTR_XOR_R8(gb->cpu_reg.de.bytes.d);
		break;

	case 0xAB: /* XOR E */
		WGB_INSTR_XOR_R8(gb->cpu_reg.de.bytes.e);
		break;

	case 0xAC: /* XOR H */
		WGB_INSTR_XOR_R8(gb->cpu_reg.hl.bytes.h);
		break;

	case 0xAD: /* XOR L */
		WGB_INSTR_XOR_R8(gb->cpu_reg.hl.bytes.l);
		break;

	case 0xAE: /* XOR (HL) */
		WGB_INSTR_XOR_R8(__gb_read(gb, gb->cpu_reg.hl.reg));
		break;

	case 0xAF: /* XOR A */
		WGB_INSTR_XOR_R8(gb->cpu_reg.a);
		break;

	case 0xB0: /* OR B */
		WGB_INSTR_OR_R8(gb->cpu_reg.bc.bytes.b);
		break;

	case 0xB1: /* OR C */
		WGB_INSTR_OR_R8(gb->cpu_reg.bc.bytes.c);
		break;

	case 0xB2: /* OR D */
		WGB_INSTR_OR_R8(gb->cpu_reg.de.bytes.d);
		break;

	case 0xB3: /* OR E */
		WGB_INSTR_OR_R8(gb->cpu_reg.de.bytes.e);
		break;

	case 0xB4: /* OR H */
		WGB_INSTR_OR_R8(gb->cpu_reg.hl.bytes.h);
		break;

	case 0xB5: /* OR L */
		WGB_INSTR_OR_R8(gb->cpu_reg.hl.bytes.l);
		break;

	case 0xB6: /* OR (HL) */
		WGB_INSTR_OR_R8(__gb_read(gb, gb->cpu_reg.hl.reg));
		break;

	case 0xB7: /* OR A */
		WGB_INSTR_OR_R8(gb->cpu_reg.a);
		break;

	case 0xB8: /* CP B */
		WGB_INSTR_CP_R8(gb->cpu_reg.bc.bytes.b);
		break;

	case 0xB9: /* CP C */
		WGB_INSTR_CP_R8(gb->cpu_reg.bc.bytes.c);
		break;

	case 0xBA: /* CP D */
		WGB_INSTR_CP_R8(gb->cpu_reg.de.bytes.d);
		break;

	case 0xBB: /* CP E */
		WGB_INSTR_CP_R8(gb->cpu_reg.de.bytes.e);
		break;

	case 0xBC: /* CP H */
		WGB_INSTR_CP_R8(gb->cpu_reg.hl.bytes.h);
		break;

	case 0xBD: /* CP L */
		WGB_INSTR_CP_R8(gb->cpu_reg.hl.bytes.l);
		break;

	case 0xBE: /* CP (HL) */
		WGB_INSTR_CP_R8(__gb_read(gb, gb->cpu_reg.hl.reg));
		break;

	case 0xBF: /* CP A */
		gb->cpu_reg.f.reg = 0;
		gb->cpu_reg.f.f_bits.z = 1;
		gb->cpu_reg.f.f_bits.n = 1;
		break;

	case 0xC0: /* RET NZ */
		if(!gb->cpu_reg.f.f_bits.z)
		{
        gb->cpu_reg.pc.reg = __gb_stack_pop16(gb, &gb->cpu_reg.sp.reg);
        inst_cycles += 12;
		}

		break;

	case 0xC1: /* POP BC */
    gb->cpu_reg.bc.reg = __gb_stack_pop16(gb, &gb->cpu_reg.sp.reg);
		break;

	case 0xC2: /* JP NZ, imm */
		if(!gb->cpu_reg.f.f_bits.z)
		{
#if WALNUT_GB_16_BIT_DISABLED
        gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
#else
        gb->cpu_reg.pc.reg = __gb_fetch16(gb);
#endif
			inst_cycles += 4;
		}
		else
			gb->cpu_reg.pc.reg += 2;

		break;

	case 0xC3: /* JP imm */
#if WALNUT_GB_16_BIT_DISABLED
    gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
#else
    gb->cpu_reg.pc.reg = __gb_fetch16(gb);
#endif
		break;

	case 0xC4: /* CALL NZ imm */
		if(!gb->cpu_reg.f.f_bits.z)
		{
#if WALNUT_GB_16_BIT_DISABLED
			uint16_t addr = __gb_read16(gb, gb->cpu_reg.pc.reg);
			gb->cpu_reg.pc.reg += 2;

			__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
			gb->cpu_reg.pc.reg = addr;
#else
			const uint16_t addr = __gb_fetch16(gb);
			__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
			gb->cpu_reg.pc.reg = addr;
#endif
			inst_cycles += 12;
		}
		else
			gb->cpu_reg.pc.reg += 2;

		break;

	case 0xC5: /* PUSH BC */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.bc.reg);
		break;

	case 0xC6: /* ADD A, imm */
	{
		uint8_t val = __gb_fetch8(gb);
		WGB_INSTR_ADC_R8(val, 0);
		break;
	}

	case 0xC7: /* RST 0x0000 */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
		gb->cpu_reg.pc.reg = 0x0000;
		break;

	case 0xC8: /* RET Z */
		if(gb->cpu_reg.f.f_bits.z)
		{
        gb->cpu_reg.pc.reg = __gb_stack_pop16(gb, &gb->cpu_reg.sp.reg);
			inst_cycles += 12;
		}
		break;

	case 0xC9: /* RET */
	{
    gb->cpu_reg.pc.reg = __gb_stack_pop16(gb, &gb->cpu_reg.sp.reg);
		break;
	}

	case 0xCA: /* JP Z, imm */
		if(gb->cpu_reg.f.f_bits.z)
		{
#if WALNUT_GB_16_BIT_DISABLED
        gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
        gb->cpu_reg.pc.reg += 2;  // advance past the immediate word
#else
        gb->cpu_reg.pc.reg = __gb_fetch16(gb);
#endif
			inst_cycles += 4;
		}
		else
			gb->cpu_reg.pc.reg += 2;

		break;

	case 0xCB: /* CB INST */
		inst_cycles = __gb_execute_cb(gb);
		break;

	case 0xCC: /* CALL Z, imm */
		if(gb->cpu_reg.f.f_bits.z)
		{
#if WALNUT_GB_16_BIT_DISABLED
        uint16_t addr = __gb_read16(gb, gb->cpu_reg.pc.reg);
        gb->cpu_reg.pc.reg += 2; // advance past immediate

        // push old PC
        __gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);

        gb->cpu_reg.pc.reg = addr;
#else
        const uint16_t addr = __gb_fetch16(gb);
        __gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
        gb->cpu_reg.pc.reg = addr;
#endif
			inst_cycles += 12;
		}
		else
			gb->cpu_reg.pc.reg += 2;

		break;

	case 0xCD: /* CALL imm */
#if WALNUT_GB_16_BIT_DISABLED
{
    uint16_t addr = __gb_read16(gb, gb->cpu_reg.pc.reg);
    gb->cpu_reg.pc.reg += 2; // advance past immediate

    // push old PC
    __gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);

    gb->cpu_reg.pc.reg = addr;
}
#else
{
    const uint16_t addr = __gb_fetch16(gb);
    __gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
    gb->cpu_reg.pc.reg = addr;
}
#endif
	break;

	case 0xCE: /* ADC A, imm */
	{
		uint8_t val = __gb_fetch8(gb);
		WGB_INSTR_ADC_R8(val, gb->cpu_reg.f.f_bits.c);
		break;
	}

	case 0xCF: /* RST 0x0008 */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
		gb->cpu_reg.pc.reg = 0x0008;
		break;

	case 0xD0: /* RET NC */

    if (!gb->cpu_reg.f.f_bits.c)
    {
        gb->cpu_reg.pc.reg = __gb_stack_pop16(gb, &gb->cpu_reg.sp.reg);
        inst_cycles += 12;
    }

		break;

	case 0xD1: /* POP DE */
    gb->cpu_reg.de.reg = __gb_stack_pop16(gb, &gb->cpu_reg.sp.reg);
		break;

	case 0xD2: /* JP NC, imm */
		if(!gb->cpu_reg.f.f_bits.c)
		{
#if WALNUT_GB_16_BIT_DISABLED
        gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
        gb->cpu_reg.pc.reg += 2; // advance PC past immediate
#else
        gb->cpu_reg.pc.reg = __gb_fetch16(gb);
#endif
			inst_cycles += 4;
		}
		else
			gb->cpu_reg.pc.reg += 2;

		break;

	case 0xD4: /* CALL NC, imm */
		if(!gb->cpu_reg.f.f_bits.c)
		{
#if WALNUT_GB_16_BIT_DISABLED
        uint16_t addr = __gb_read16(gb, gb->cpu_reg.pc.reg);
        gb->cpu_reg.pc.reg += 2; // advance PC past immediate
        __gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
        gb->cpu_reg.pc.reg = addr;
#else
        const uint16_t addr = __gb_fetch16(gb);
        __gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
        gb->cpu_reg.pc.reg = addr;
#endif
			inst_cycles += 12;
		}
		else
			gb->cpu_reg.pc.reg += 2;

		break;

	case 0xD5: /* PUSH DE */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.de.reg);
		break;

	case 0xD6: /* SUB imm */
	{
		uint8_t val = __gb_fetch8(gb);
		uint16_t temp = gb->cpu_reg.a - val;
		gb->cpu_reg.f.f_bits.z = ((temp & 0xFF) == 0x00);
		gb->cpu_reg.f.f_bits.n = 1;
		gb->cpu_reg.f.f_bits.h =
			(gb->cpu_reg.a ^ val ^ temp) & 0x10 ? 1 : 0;
		gb->cpu_reg.f.f_bits.c = (temp & 0xFF00) ? 1 : 0;
		gb->cpu_reg.a = (temp & 0xFF);
		break;
	}

	case 0xD7: /* RST 0x0010 */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
		gb->cpu_reg.pc.reg = 0x0010;
		break;

	case 0xD8: /* RET C */
		if(gb->cpu_reg.f.f_bits.c)
		{
        gb->cpu_reg.pc.reg = __gb_stack_pop16(gb, &gb->cpu_reg.sp.reg);
			inst_cycles += 12;
		}

		break;

	case 0xD9: /* RETI */
	{
    gb->cpu_reg.pc.reg = __gb_stack_pop16(gb, &gb->cpu_reg.sp.reg);
		gb->gb_ime = true;
	}
	break;

	case 0xDA: /* JP C, imm */
		if(gb->cpu_reg.f.f_bits.c)
		{
#if WALNUT_GB_16_BIT_DISABLED
        gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
        gb->cpu_reg.pc.reg += 2; // advance PC past the immediate word
#else
        gb->cpu_reg.pc.reg = __gb_fetch16(gb);
#endif
			inst_cycles += 4;
		}
		else
			gb->cpu_reg.pc.reg += 2;

		break;

	case 0xDC: /* CALL C, imm */
		if(gb->cpu_reg.f.f_bits.c)
		{
#if WALNUT_GB_16_BIT_DISABLED
        uint16_t target = __gb_read16(gb, gb->cpu_reg.pc.reg);
        gb->cpu_reg.pc.reg += 2; // advance PC past the immediate word
        // push current PC onto stack
        __gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
        gb->cpu_reg.pc.reg = target;
#else
        const uint16_t target = __gb_fetch16(gb);
        __gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
        gb->cpu_reg.pc.reg = target;
#endif
			inst_cycles += 12;
		}
		else
			gb->cpu_reg.pc.reg += 2;

		break;

	case 0xDE: /* SBC A, imm */
	{
		uint8_t val = __gb_fetch8(gb);
		WGB_INSTR_SBC_R8(val, gb->cpu_reg.f.f_bits.c);
		break;
	}

	case 0xDF: /* RST 0x0018 */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
		gb->cpu_reg.pc.reg = 0x0018;
		break;

	case 0xE0: /* LD (0xFF00+imm), A */
	{
		const uint8_t io = __gb_fetch8(gb);
		if(!__gb_fast_hram_write(gb, io, gb->cpu_reg.a))
			__gb_write(gb, 0xFF00 | io, gb->cpu_reg.a);
		break;
	}

	case 0xE1: /* POP HL */
    gb->cpu_reg.hl.reg = __gb_stack_pop16(gb, &gb->cpu_reg.sp.reg);
		break;

	case 0xE2: /* LD (C), A */
		if(!__gb_fast_hram_write(gb, gb->cpu_reg.bc.bytes.c, gb->cpu_reg.a))
			__gb_write(gb, 0xFF00 | gb->cpu_reg.bc.bytes.c, gb->cpu_reg.a);
		break;

	case 0xE5: /* PUSH HL */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.hl.reg);
		break;

	case 0xE6: /* AND imm */
	{
		uint8_t temp = __gb_fetch8(gb);
		WGB_INSTR_AND_R8(temp);
		break;
	}

	case 0xE7: /* RST 0x0020 */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
		gb->cpu_reg.pc.reg = 0x0020;
		break;

	case 0xE8: /* ADD SP, imm */
	{
		int8_t offset = (int8_t) __gb_fetch8(gb);
		gb->cpu_reg.f.reg = 0;
		gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.sp.reg & 0xF) + (offset & 0xF) > 0xF) ? 1 : 0;
		gb->cpu_reg.f.f_bits.c = ((gb->cpu_reg.sp.reg & 0xFF) + (offset & 0xFF) > 0xFF);
		gb->cpu_reg.sp.reg += offset;
		break;
	}

	case 0xE9: /* JP (HL) */
		gb->cpu_reg.pc.reg = gb->cpu_reg.hl.reg;
		break;

	case 0xEA: /* LD (imm), A */
	{
#if WALNUT_GB_16_BIT_DISABLED
    uint16_t addr = __gb_read16(gb, gb->cpu_reg.pc.reg);
    gb->cpu_reg.pc.reg += 2; // advance past the immediate
#else
    uint16_t addr = __gb_fetch16(gb);
#endif
		__gb_write(gb, addr, gb->cpu_reg.a);
		break;
	}

	case 0xEE: /* XOR imm */
		WGB_INSTR_XOR_R8(__gb_fetch8(gb));
		break;

	case 0xEF: /* RST 0x0028 */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
		gb->cpu_reg.pc.reg = 0x0028;
		break;

	case 0xF0: /* LD A, (0xFF00+imm) */
	{
		const uint16_t stat_read_pc = (uint16_t)(gb->cpu_reg.pc.reg - 1);
		const uint8_t io = __gb_fetch8(gb);
#if WALNUT_GB_FAST_STAT_POLL_ACTIVE && WALNUT_GB_FAST_STAT_POLL_DIRECT_READS
		if(io == IO_STAT)
			gb->cpu_reg.a = __gb_fast_stat_poll_read(gb,
				stat_read_pc, gb->cpu_reg.pc.reg);
		else
#endif
			if(!__gb_fast_io_read_noapu(gb, io, &gb->cpu_reg.a))
				gb->cpu_reg.a = __gb_read(gb, 0xFF00 | io);
		break;
	}

	case 0xF1: /* POP AF */
	{
		const uint16_t temp_16 =
			__gb_stack_pop16(gb, &gb->cpu_reg.sp.reg);
		uint8_t temp_8 = (uint8_t)temp_16;
		gb->cpu_reg.f.f_bits.z = (temp_8 >> 7) & 1;
		gb->cpu_reg.f.f_bits.n = (temp_8 >> 6) & 1;
		gb->cpu_reg.f.f_bits.h = (temp_8 >> 5) & 1;
		gb->cpu_reg.f.f_bits.c = (temp_8 >> 4) & 1;
		gb->cpu_reg.a = (uint8_t)(temp_16 >> 8);
		break;
	}

	case 0xF2: /* LD A, (C) */
	{
#if WALNUT_GB_FAST_STAT_POLL_ACTIVE
		if(gb->cpu_reg.bc.bytes.c == IO_STAT)
			gb->cpu_reg.a = __gb_fast_stat_poll_c_read(gb,
				(uint16_t)(gb->cpu_reg.pc.reg - 1), gb->cpu_reg.pc.reg);
		else
#endif
			if(!__gb_fast_io_read_noapu(gb, gb->cpu_reg.bc.bytes.c, &gb->cpu_reg.a))
				gb->cpu_reg.a = __gb_read(gb, 0xFF00 | gb->cpu_reg.bc.bytes.c);
		break;
	}

	case 0xF3: /* DI */
		gb->gb_ime = false;
		break;

	case 0xF5: /* PUSH AF */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg,
			((uint16_t)gb->cpu_reg.a << 8) |
			gb->cpu_reg.f.f_bits.z << 7 | gb->cpu_reg.f.f_bits.n << 6 |
			gb->cpu_reg.f.f_bits.h << 5 | gb->cpu_reg.f.f_bits.c << 4);
		break;

	case 0xF6: /* OR imm */
		WGB_INSTR_OR_R8(__gb_fetch8(gb));
		break;

	case 0xF7: /* PUSH AF */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
		gb->cpu_reg.pc.reg = 0x0030;
		break;

	case 0xF8: /* LD HL, SP+/-imm */
	{
		/* Taken from SameBoy, which is released under MIT Licence. */
		int8_t offset = (int8_t) __gb_fetch8(gb);
		gb->cpu_reg.hl.reg = gb->cpu_reg.sp.reg + offset;
		gb->cpu_reg.f.reg = 0;
		gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.sp.reg & 0xF) + (offset & 0xF) > 0xF) ? 1 : 0;
		gb->cpu_reg.f.f_bits.c = ((gb->cpu_reg.sp.reg & 0xFF) + (offset & 0xFF) > 0xFF) ? 1 : 0;
		break;
	}

	case 0xF9: /* LD SP, HL */
		gb->cpu_reg.sp.reg = gb->cpu_reg.hl.reg;
		break;

	case 0xFA: /* LD A, (imm) */
	{
		const uint16_t stat_read_pc = (uint16_t)(gb->cpu_reg.pc.reg - 1);
#if WALNUT_GB_16_BIT_DISABLED
    uint16_t addr = __gb_read16(gb, gb->cpu_reg.pc.reg);
    gb->cpu_reg.pc.reg += 2; // advance past the immediate
#else
    uint16_t addr = __gb_fetch16(gb);
#endif
#if WALNUT_GB_FAST_STAT_POLL_ACTIVE && WALNUT_GB_FAST_STAT_POLL_DIRECT_READS
		if(addr == (uint16_t)(IO_ADDR | IO_STAT))
			gb->cpu_reg.a = __gb_fast_stat_poll_read(gb,
				stat_read_pc, gb->cpu_reg.pc.reg);
		else
#endif
			if(!__gb_fast_abs_read_hot(gb, addr, &gb->cpu_reg.a))
				gb->cpu_reg.a = __gb_read(gb, addr);
		break;
	}

	case 0xFB: /* EI */
		gb->gb_ime = true;
		break;

	case 0xFE: /* CP imm */
	{
		uint8_t val = __gb_fetch8(gb);
		WGB_INSTR_CP_R8(val);
		break;
	}

	case 0xFF: /* RST 0x0038 */
		__gb_stack_push16(gb, &gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
		gb->cpu_reg.pc.reg = 0x0038;
		break;

	default:
		/* Return address where invalid opcode that was read. */
		(gb->gb_error)(gb, GB_INVALID_OPCODE, gb->cpu_reg.pc.reg - 1);
		WGB_UNREACHABLE();
	}

	do
	{
		/* DIV register timing */
		gb->counter.div_count += inst_cycles;
		while(gb->counter.div_count >= DIV_CYCLES)
		{
			gb->hram_io[IO_DIV]++;
			gb->counter.div_count -= DIV_CYCLES;
		}

		/* Check for RTC tick. */
		if(WGB_UNLIKELY(gb->mbc == 3) && (gb->rtc_real.reg.high & 0x40) == 0)
		{
			gb->counter.rtc_count += inst_cycles;
			while(WGB_UNLIKELY(gb->counter.rtc_count >= RTC_CYCLES))
			{
				gb->counter.rtc_count -= RTC_CYCLES;

				/* Detect invalid rollover. */
				if(WGB_UNLIKELY(gb->rtc_real.reg.sec == 63))
				{
					gb->rtc_real.reg.sec = 0;
					continue;
				}

				if(++gb->rtc_real.reg.sec != 60)
					continue;

				gb->rtc_real.reg.sec = 0;
				if(gb->rtc_real.reg.min == 63)
				{
					gb->rtc_real.reg.min = 0;
					continue;
				}
				if(++gb->rtc_real.reg.min != 60)
					continue;

				gb->rtc_real.reg.min = 0;
				if(gb->rtc_real.reg.hour == 31)
				{
					gb->rtc_real.reg.hour = 0;
					continue;
				}
				if(++gb->rtc_real.reg.hour != 24)
					continue;

				gb->rtc_real.reg.hour = 0;
				if(++gb->rtc_real.reg.yday != 0)
					continue;

				if(gb->rtc_real.reg.high & 1)  /* Bit 8 of days*/
					gb->rtc_real.reg.high |= 0x80; /* Overflow bit */

				gb->rtc_real.reg.high ^= 1;
			}
		}

		/* Check serial transmission. */
		const uint8_t serial_control = gb->hram_io[IO_SC];
		if(WGB_UNLIKELY(serial_control & SERIAL_SC_TX_START))
		{
			unsigned int serial_cycles = SERIAL_CYCLES_1KB;

			/* If new transfer, call TX function. */
			if(gb->counter.serial_count == 0 &&
				gb->gb_serial_tx != NULL)
				(gb->gb_serial_tx)(gb, gb->hram_io[IO_SB]);

#if WALNUT_FULL_GBC_SUPPORT
			if(serial_control & 0x3)
				serial_cycles = SERIAL_CYCLES_32KB;
#endif

			gb->counter.serial_count += inst_cycles;

			/* If it's time to receive byte, call RX function. */
			if(gb->counter.serial_count >= serial_cycles)
			{
				/* If RX can be done, do it. */
				/* If RX failed, do not change SB if using external
				 * clock, or set to 0xFF if using internal clock. */
				uint8_t rx;

				if(gb->gb_serial_rx != NULL &&
					(gb->gb_serial_rx(gb, &rx) ==
						GB_SERIAL_RX_SUCCESS))
				{
					gb->hram_io[IO_SB] = rx;

					/* Inform game of serial TX/RX completion. */
					gb->hram_io[IO_SC] &= 0x01;
					gb->hram_io[IO_IF] |= SERIAL_INTR;
				}
				else if(serial_control & SERIAL_SC_CLOCK_SRC)
				{
					/* If using internal clock, and console is not
					 * attached to any external peripheral, shifted
					 * bits are replaced with logic 1. */
					gb->hram_io[IO_SB] = 0xFF;

					/* Inform game of serial TX/RX completion. */
					gb->hram_io[IO_SC] &= 0x01;
					gb->hram_io[IO_IF] |= SERIAL_INTR;
				}
				else
				{
					/* If using external clock, and console is not
					 * attached to any external peripheral, bits are
					 * not shifted, so SB is not modified. */
				}

				gb->counter.serial_count = 0;
			}
		}

		/* TIMA register timing */
		/* TODO: Change tac_enable to struct of TAC timer control bits. */
		const uint8_t tac = gb->hram_io[IO_TAC];
		if(tac & IO_TAC_ENABLE_MASK)
		{
			const uint_fast16_t tac_cycles = TAC_CYCLES[tac & IO_TAC_RATE_MASK];
			gb->counter.tima_count += inst_cycles;

			while(gb->counter.tima_count >= tac_cycles)
			{
				gb->counter.tima_count -= tac_cycles;

				if(++gb->hram_io[IO_TIMA] == 0)
				{
					gb->hram_io[IO_IF] |= TIMER_INTR;
					/* On overflow, set TMA to TIMA. */
					gb->hram_io[IO_TIMA] = gb->hram_io[IO_TMA];
				}
			}
		}

		/* If LCD is off, don't update LCD state or increase the LCD
		 * ticks. Instead, keep track of the amount of time that is
		 * being passed. */
		if(!(gb->hram_io[IO_LCDC] & LCDC_ENABLE))
		{
			gb->counter.lcd_off_count += inst_cycles;
			if(gb->counter.lcd_off_count >= LCD_FRAME_CYCLES)
			{
				gb->counter.lcd_off_count -= LCD_FRAME_CYCLES;
				gb->gb_frame = true;
			}
			continue;
		}

		/* LCD Timing */
#if WALNUT_FULL_GBC_SUPPORT
		gb->counter.lcd_count += (inst_cycles >> gb->cgb.doubleSpeed);
#else
		gb->counter.lcd_count += inst_cycles;
#endif

		/* New Scanline. HBlank -> VBlank or OAM Scan */
		if(gb->counter.lcd_count >= LCD_LINE_CYCLES)
		{
			gb->counter.lcd_count -= LCD_LINE_CYCLES;

			/* Next line */
			gb->hram_io[IO_LY] = gb->hram_io[IO_LY] + 1;
			if (gb->hram_io[IO_LY] == LCD_VERT_LINES)
				gb->hram_io[IO_LY] = 0;

			/* LYC Update */
			if(gb->hram_io[IO_LY] == gb->hram_io[IO_LYC])
			{
				gb->hram_io[IO_STAT] |= STAT_LYC_COINC;

				if(!gb->direct.suppress_lcd_stat_interrupts &&
						(gb->hram_io[IO_STAT] & STAT_LYC_INTR))
					gb->hram_io[IO_IF] |= LCDC_INTR;
			}
			else
				gb->hram_io[IO_STAT] &= 0xFB;

			/* Check if LCD should be in Mode 1 (VBLANK) state */
			if(gb->hram_io[IO_LY] == LCD_HEIGHT)
			{
				gb->hram_io[IO_STAT] =
					(gb->hram_io[IO_STAT] & ~STAT_MODE) | IO_STAT_MODE_VBLANK;
				gb->gb_frame = true;
				gb->hram_io[IO_IF] |= VBLANK_INTR;
				gb->lcd_blank = false;

				if(!gb->direct.suppress_lcd_stat_interrupts &&
						(gb->hram_io[IO_STAT] & STAT_MODE_1_INTR))
					gb->hram_io[IO_IF] |= LCDC_INTR;

#if ENABLE_LCD
				/* If frame skip is activated, check if we need to draw
				 * the frame or skip it. */
				if(gb->direct.frame_skip)
				{
					gb->display.frame_skip_count =
						!gb->display.frame_skip_count;
				}

				/* If interlaced is activated, change which lines get
				 * updated. Also, only update lines on frames that are
				 * actually drawn when frame skip is enabled. */
				if(gb->direct.interlace &&
						(!gb->direct.frame_skip ||
						 gb->display.frame_skip_count))
				{
					gb->display.interlace_count =
						!gb->display.interlace_count;
				}
#endif
                                /* If halted forever, then return on VBLANK. */
                                if(gb->gb_halt && !gb->hram_io[IO_IE])
					break;
			}
			/* Start of normal Line (not in VBLANK) */
			else if(gb->hram_io[IO_LY] < LCD_HEIGHT)
			{
				if(gb->hram_io[IO_LY] == 0)
				{
					/* Clear Screen */
					gb->display.WY = gb->hram_io[IO_WY];
					gb->display.window_clear = 0;
				}

				/* OAM Search occurs at the start of the line. */
				gb->hram_io[IO_STAT] = (gb->hram_io[IO_STAT] & ~STAT_MODE) | IO_STAT_MODE_OAM_SCAN;
				gb->counter.lcd_count = 0;

#if WALNUT_FULL_GBC_SUPPORT
				//DMA GBC
				if(gb->cgb.cgbMode && !gb->cgb.dmaActive && gb->cgb.dmaMode)
				{
#if WALNUT_GB_32BIT_DMA
					// Optimized 16-bit path
					for (uint8_t i = 0; i < 0x10; i += 4)
					{
							uint32_t val = __gb_read32(gb, (gb->cgb.dmaSource & 0xFFF0) + i);
							__gb_write32(gb, ((gb->cgb.dmaDest & 0x1FF0) | 0x8000) + i, val);
							// 8-bit logic if there is some cause to fall back
							// __gb_write(gb, ((gb->cgb.dmaDest & 0x1FF0) | 0x8000) + i, val);
							// __gb_write(gb, ((gb->cgb.dmaDest & 0x1FF0) | 0x8000) + i + 1, val >> 8);
							// __gb_write(gb, ((gb->cgb.dmaDest & 0x1FF0) | 0x8000) + i + 2, val >> 16);
							// __gb_write(gb, ((gb->cgb.dmaDest & 0x1FF0) | 0x8000) + i + 3, val >> 24);
					}
#elif WALNUT_GB_16BIT_DMA
					// Optimized 16-bit path
					for (uint8_t i = 0; i < 0x10; i += 2)
					{
							uint16_t val = __gb_read16(gb, (gb->cgb.dmaSource & 0xFFF0) + i);
							__gb_write16(gb, ((gb->cgb.dmaDest & 0x1FF0) | 0x8000) + i, val);
							// 8-bit logic if there is some cause to fall back
							// __gb_write(gb, ((gb->cgb.dmaDest & 0x1FF0) | 0x8000) + i, val & 0xFF);
							// __gb_write(gb, ((gb->cgb.dmaDest & 0x1FF0) | 0x8000) + i + 1, val >> 8);
					}
#else
			    // Original 8-bit path
					for (uint8_t i = 0; i < 0x10; i++)
					{
						__gb_write(gb, ((gb->cgb.dmaDest & 0x1FF0) | 0x8000) + i,
										__gb_read(gb, (gb->cgb.dmaSource & 0xFFF0) + i));
					}
#endif

					gb->cgb.dmaSource += 0x10;
					gb->cgb.dmaDest += 0x10;
					if(!(--gb->cgb.dmaSize)) {gb->cgb.dmaActive = 1;
					}
				}
#endif
				if(!gb->direct.suppress_lcd_stat_interrupts &&
						(gb->hram_io[IO_STAT] & STAT_MODE_2_INTR))
					gb->hram_io[IO_IF] |= LCDC_INTR;

				/* If halted immediately jump to next LCD mode.
				 * From OAM Search to LCD Draw. */
				//if(gb->counter.lcd_count < LCD_MODE2_OAM_SCAN_END)
				//	inst_cycles = LCD_MODE2_OAM_SCAN_END - gb->counter.lcd_count;
				inst_cycles = LCD_MODE2_OAM_SCAN_DURATION;
			}
		}
		/* Go from Mode 3 (LCD Draw) to Mode 0 (HBLANK). */
		else if((gb->hram_io[IO_STAT] & STAT_MODE) == IO_STAT_MODE_LCD_DRAW &&
				gb->counter.lcd_count >= LCD_MODE3_LCD_DRAW_END)
		{
			gb->hram_io[IO_STAT] = (gb->hram_io[IO_STAT] & ~STAT_MODE) | IO_STAT_MODE_HBLANK;

			if(!gb->direct.suppress_lcd_stat_interrupts &&
					(gb->hram_io[IO_STAT] & STAT_MODE_0_INTR))
				gb->hram_io[IO_IF] |= LCDC_INTR;

			/* If halted immediately, jump from OAM Scan to LCD Draw. */
			if (gb->counter.lcd_count < LCD_MODE0_HBLANK_MAX_DRUATION)
				inst_cycles = LCD_MODE0_HBLANK_MAX_DRUATION - gb->counter.lcd_count;
		}
		/* Go from Mode 2 (OAM Scan) to Mode 3 (LCD Draw). */
		else if((gb->hram_io[IO_STAT] & STAT_MODE) == IO_STAT_MODE_OAM_SCAN &&
				gb->counter.lcd_count >= LCD_MODE2_OAM_SCAN_END)
		{
			gb->hram_io[IO_STAT] = (gb->hram_io[IO_STAT] & ~STAT_MODE) | IO_STAT_MODE_LCD_DRAW;
#if ENABLE_LCD
			if(!gb->lcd_blank)
				__gb_draw_line(gb);
#endif
			/* If halted immediately jump to next LCD mode. */
			if (gb->counter.lcd_count < LCD_MODE3_LCD_DRAW_MIN_DURATION)
				inst_cycles = LCD_MODE3_LCD_DRAW_MIN_DURATION - gb->counter.lcd_count;
		}
	} while(gb->gb_halt && __gb_effective_pending_interrupts(gb) == 0);
	/* If halted, loop until an interrupt occurs. */
}

/* Undefine CPU Flag helper functions. */
#undef WALNUT_GB_CPUFLAG_MASK_CARRY
#undef WALNUT_GB_CPUFLAG_MASK_HALFC
#undef WALNUT_GB_CPUFLAG_MASK_ARITH
#undef WALNUT_GB_CPUFLAG_MASK_ZERO
#undef WALNUT_GB_CPUFLAG_BIT_CARRY
#undef WALNUT_GB_CPUFLAG_BIT_HALFC
#undef WALNUT_GB_CPUFLAG_BIT_ARITH
#undef WALNUT_GB_CPUFLAG_BIT_ZERO
#undef WGB_SET_CARRY
#undef WGB_SET_HALFC
#undef WGB_SET_ARITH
#undef WGB_SET_ZERO
#undef WGB_GET_CARRY
#undef WGB_GET_HALFC
#undef WGB_GET_ARITH
#undef WGB_GET_ZERO
#endif //WALNUT_GB_H
