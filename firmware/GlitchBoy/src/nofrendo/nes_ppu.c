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
** nes_ppu.c
**
** NES PPU emulation
** $Id: nes_ppu.c,v 1.2 2001/04/27 14:37:11 neil Exp $
*/

#if defined(__GNUC__)
#pragma GCC optimize("O3")
#endif

#include "string.h"
#include "stdlib.h"
#include "noftypes.h"
#include "nes_ppu.h"
#include "nes.h"
#include "gui.h"
#include "nes6502.h"
#include "log.h"
#include "nes_mmc.h"
#include "mmc5.h"

#include "bitmap.h"
#include "vid_drv.h"
#include "nes_pal.h"
#include "nesinput.h"
#include "../glitch/nes_glitch.h"

#ifdef ESP_PLATFORM
#include <esp_attr.h>
#define PPU_SPRITE_HOT_ATTR IRAM_ATTR
#define PPU_SPRITE_HOT_INLINE static inline PPU_SPRITE_HOT_ATTR
#else
#define PPU_SPRITE_HOT_ATTR
#define PPU_SPRITE_HOT_INLINE static inline
#endif

#if defined(__GNUC__)
#define PPU_NOINLINE_ATTR __attribute__((noinline))
#else
#define PPU_NOINLINE_ATTR
#endif

/* Fast bit-expansion LUTs for 2bpp pattern decode */
static bool ppu_luts_inited = false;
/* Each entry expands a pattern byte into 8 bytes packed into a 64-bit value.
   exp1: bytes are 0x00 or 0x01 (bit plane 0)
   exp2: bytes are 0x00 or 0x02 (bit plane 1) */
static unsigned long long exp1_lut[256];
static unsigned long long exp2_lut[256];
/* Sprite rows use 4-bit lanes to avoid 64-bit shifts on ESP32. */
static uint32 exp1_sprite_lut[256];
static uint32 exp2_sprite_lut[256];
static uint32 exp1_sprite_hflip_lut[256];
static uint32 exp2_sprite_hflip_lut[256];

INLINE uint8 bitrev8(uint8 v)
{
    v = (uint8)((v >> 4) | (v << 4));
    v = (uint8)(((v & 0xCC) >> 2) | ((v & 0x33) << 2));
    v = (uint8)(((v & 0xAA) >> 1) | ((v & 0x55) << 1));
    return v;
}

static void ppu_init_luts(void)
{
    if (ppu_luts_inited)
        return;

    for (int n = 0; n < 256; n++)
    {
        uint8 rev = bitrev8((uint8)n);

        unsigned long long e1 = 0ULL;
        unsigned long long e2 = 0ULL;
        uint32 s1 = 0;
        uint32 s2 = 0;
        uint32 s1f = 0;
        uint32 s2f = 0;
        /* pixel 0 is MSB (bit 7), left to right */
        for (int i = 0; i < 8; i++)
        {
            int bit = (n >> (7 - i)) & 1;
            int bitf = (rev >> (7 - i)) & 1;
            unsigned long long b1 = (unsigned long long)(bit ? 0x01 : 0x00);
            unsigned long long b2 = (unsigned long long)(bit ? 0x02 : 0x00);
            int shift = (7 - i) * 8; /* place into byte lane */
            e1 |= (b1 << shift);
            e2 |= (b2 << shift);

            int sprite_shift = (7 - i) * 4;
            s1 |= ((uint32)(bit ? 0x1 : 0x0) << sprite_shift);
            s2 |= ((uint32)(bit ? 0x2 : 0x0) << sprite_shift);
            s1f |= ((uint32)(bitf ? 0x1 : 0x0) << sprite_shift);
            s2f |= ((uint32)(bitf ? 0x2 : 0x0) << sprite_shift);
        }
        exp1_lut[n] = e1;
        exp2_lut[n] = e2;
        exp1_sprite_lut[n] = s1;
        exp2_sprite_lut[n] = s2;
        exp1_sprite_hflip_lut[n] = s1f;
        exp2_sprite_hflip_lut[n] = s2f;
    }

    ppu_luts_inited = true;
}

/* PPU memory access goes through ppu_mem_ptr() so glitches and mapper
 * fetch hooks see every read. */
#undef  PPU_MEM
/* Accumulate PPU dots and burn 1 CPU cycle every 3 dots (~2 PPU clocks each) */
static int ppu_fetch_pending_dots = 0;

PPU_SPRITE_HOT_INLINE void ppu_fetch_delay_many(int fetches)
{
    int dots = ppu_fetch_pending_dots + (fetches << 1);
    int cycles = dots / 3;
    ppu_fetch_pending_dots = dots - (cycles * 3);

    if (cycles)
        nes6502_burn(cycles);
}

PPU_SPRITE_HOT_INLINE void ppu_fetch_delay(void)
{
    ppu_fetch_pending_dots += 2;
    if (ppu_fetch_pending_dots >= 3)
    {
        nes6502_burn(1);
        ppu_fetch_pending_dots -= 3;
    }
}

PPU_SPRITE_HOT_INLINE uint8 *ppu_mem_ptr(uint16 x);
PPU_SPRITE_HOT_INLINE uint8 *ppu_mem_wptr(uint16 x);

#define PPU_MEM_PTR(x)  ppu_mem_ptr(x)
#define PPU_MEM(x)      (*PPU_MEM_PTR(x))
/* Writes bypass glitches and mapper fetch hooks. */
#define PPU_MEM_WPTR(x) ppu_mem_wptr(x)
/* Background (color 0) and solid sprite pixel flags */
#define  BG_TRANS             0x80
#define  SP_PIXEL             0x40
#define  BG_CLEAR(V)          ((V) & BG_TRANS)
#define  BG_SOLID(V)          (0 == BG_CLEAR(V))
#define  SP_CLEAR(V)          (0 == ((V) & SP_PIXEL))

/* Full BG color */
#define  FULLBG               (ppu.palette[0] | BG_TRANS)

typedef struct ppu_scanline_sprite_s
{
   uint8 sprite_num;
   uint8 x_loc;
   uint8 tile;
   uint8 atr;
   uint8 row_offset;
} ppu_scanline_sprite_t;

/* the NES PPU */
static ppu_t ppu;
static ppu_scanline_sprite_t ppu_scanline_oam[NES_SCREEN_HEIGHT][PPU_MAXSPRITE];
static uint8 ppu_scanline_oam_count[NES_SCREEN_HEIGHT];

/* The emulator is chosen once per boot, so when another core is running this
** per-frame sprite table is idle internal RAM it can borrow. It is rebuilt at
** scanline 0 of every NES frame, so it needs no restore. */
uint8 *nes_ppu_idle_scratch(size_t *len)
{
   if (len)
      *len = sizeof(ppu_scanline_oam);
   return (uint8 *) ppu_scanline_oam;
}
static uint8 ppu_sprite0_sync_oam[4];
static uint8 ppu_sprite0_sync_line[NES_SCREEN_WIDTH + 16];
static bool ppu_sync_fetch_bypass = false;
static uint8 ppu_palette_glitch_base[32];
static uint8 ppu_palette_glitch_enabled = 0;
static uint8 ppu_palette_glitch_game_active = 1;
static uint8 ppu_palette_glitch_phase = 0;

#define PPU_PALETTE_GLITCH_PHASES 6
#define PPU_PALETTE_GLITCH_HUE_STEP 2

INLINE bool is_rendering(void)
{
    /* Rendering is active only on visible scanlines. */
    return ppu_enabled() && nes_getcontextptr()->scanline < 240;
}

INLINE void inc_h_rendering(void)   /* Loopy coarse X */
{
    if ((ppu.vaddr & 0x001F) == 31) {      /* coarse X == 31 ? */
        ppu.vaddr &= ~0x001F;              /* coarse X = 0          */
        ppu.vaddr ^= 0x0400;               /* flip horizontal NT bit*/
    } else {
        ppu.vaddr += 1;                    /* coarse X++            */
    }
}
INLINE void inc_v_rendering(void)
{
    /* Increment fine Y, then wrap into coarse Y. */
    if ((ppu.vaddr & 0x7000) != 0x7000) {     /* Fine Y < 7.                 */
        ppu.vaddr += 0x1000;                  /* Increment fine Y.           */
    } else {
        /* Fine Y wrapped; advance coarse Y. */
        ppu.vaddr &= ~0x7000;                 /* Fine Y = 0.                 */

        uint16_t y = (ppu.vaddr >> 5) & 0x1F; /* Coarse Y.                   */

        if (y == 29) {                        /* bottom row of a nametable?  */
            y = 0;                            /* Wrap to top.                */
            ppu.vaddr ^= 0x0800;              /* Flip vertical nametable.    */
        } else if (y == 31) {                 /* attribute row: skip NT flip */
            y = 0;                            /* wrap to top                 */
        } else {
            y++;                              /* ordinary increment          */
        }

        ppu.vaddr = (ppu.vaddr & ~0x03E0) | (y << 5);  /* Write back coarse Y. */
    }

    /* vaddr is 15 bits. */
    ppu.vaddr &= 0x7FFF;
}

/* Optional mapper callbacks (MMC3 A12 IRQ, MMC5, ...) */
static void (*mapper_ppu_hook)(uint16 addr) = NULL;      /* CHR $0000-$1FFF */
static void (*mapper_ppu_hook_nt)(uint16 addr) = NULL;   /* NT  $2000-$2FFF */
static bool mapper_sprite_fetch_coalescing = false;

PPU_SPRITE_HOT_INLINE uint8 *ppu_mem_ptr(uint16 x)
{
    uint16 orig = x;
    if (ppu_sync_fetch_bypass)
        return &ppu.page[x >> 10][x];

    x = nes_glitch_apply_ppu_address(x);

    /* Mapper hooks: CHR and NT taps */
    if ((orig & 0x2000) == 0) {
        if (mapper_ppu_hook) {
            ppu_fetch_delay();
            mapper_ppu_hook((uint16)(orig & 0x1FFF));
        }
    } else if ((orig & 0x3000) == 0x2000) {
        if (mapper_ppu_hook_nt) {
            mapper_ppu_hook_nt((uint16)(orig & 0x2FFF));
        }
    }

    return &ppu.page[x >> 10][x];
}

PPU_SPRITE_HOT_INLINE uint8 *ppu_mem_wptr(uint16 x)
{
    /* Writes are not glitched and do not clock mapper hooks. Mask to the
     * 14-bit PPU bus: loopy v is 15 bits and raster tricks can push it
     * past $3FFF, which real hardware mirrors down. */
    x &= 0x3FFF;
    return &ppu.page[x >> 10][x];
}

void ppu_set_mapper_hook(void (*fn)(uint16 addr))
{
    mapper_ppu_hook = fn;
    mapper_sprite_fetch_coalescing = false;
}

void ppu_set_nt_hook(void (*fn)(uint16 addr))
{
    mapper_ppu_hook_nt = fn;
}

void ppu_set_mapper_sprite_fetch_coalescing(bool enabled)
{
    mapper_sprite_fetch_coalescing = enabled && (NULL != mapper_ppu_hook);
}

int ppu_bg_base(void) { return ppu.bg_base; }
int ppu_obj_base(void) { return ppu.obj_base; }

void ppu_displaysprites(bool display)
{
   ppu.drawsprites = display;
}

static bool ppu_page_uses_internal_nametab(const ppu_t *ctx, int page)
{
   uint8 *base;

   if (NULL == ctx->page[page])
      return false;

   base = ctx->page[page] + (page << 10);
   return base >= ctx->nametab && base < (ctx->nametab + sizeof(ctx->nametab));
}

static void ppu_copy_page_pointer(ppu_t *dest, const ppu_t *src, int page)
{
   uint8 *base;

   if (NULL == src->page[page])
   {
      dest->page[page] = NULL;
      return;
   }

   if (!ppu_page_uses_internal_nametab(src, page))
   {
      dest->page[page] = src->page[page];
      return;
   }

   base = src->page[page] + (page << 10);
   dest->page[page] = dest->nametab + (base - src->nametab) - (page << 10);
}

static uint8 ppu_palette_glitch_shift_color(uint8 value)
{
   const uint8 color = value & 0x3F;
   uint8 luma = color & 0x30;
   uint8 hue = color & 0x0F;

   if (!ppu_palette_glitch_enabled || !ppu_palette_glitch_game_active ||
       0 == ppu_palette_glitch_phase)
      return color;

   if (ppu_palette_glitch_phase == 3 || ppu_palette_glitch_phase == 5)
   {
      if (hue >= 13)
         return (ppu_palette_glitch_phase == 3) ? 0x30 : 0x20;
      if (hue == 0)
      {
         if (luma >= 0x20)
            return 0x0D;
         if (luma == 0x00)
            return 0x30;
         return (ppu_palette_glitch_phase == 3) ? 0x20 : 0x30;
      }
   }

   if (ppu_palette_glitch_phase == 3)
      luma = (uint8)(0x30 - luma);
   else if (ppu_palette_glitch_phase == 5)
      luma = (uint8)(((luma + 0x20) & 0x30));

   /* NES hue columns 1..12 form the color wheel; neutral columns keep hue. */
   if (hue >= 1 && hue <= 12)
      hue = (uint8)(((hue - 1 + (ppu_palette_glitch_phase * PPU_PALETTE_GLITCH_HUE_STEP)) % 12) + 1);

   return (uint8)(luma | hue);
}

static uint8 ppu_palette_glitch_render_entry(int index)
{
   uint8 color = ppu_palette_glitch_shift_color(ppu_palette_glitch_base[index & 0x1F]);
   if ((index & 3) == 0)
      color |= BG_TRANS;
   return color;
}

static void ppu_palette_glitch_apply_all(void)
{
   int i;

   for (i = 0; i < 32; ++i)
      ppu.palette[i] = ppu_palette_glitch_render_entry(i);
}

static void ppu_palette_glitch_capture_base(void)
{
   int i;

   for (i = 0; i < 32; ++i)
      ppu_palette_glitch_base[i] = ppu.palette[i] & 0x3F;
}

static void ppu_palette_glitch_write_base(uint16 address, uint8 value)
{
   const uint8 color = value & 0x3F;

   if ((address & 0x0F) == 0)
   {
      int i;

      for (i = 0; i < 8; ++i)
         ppu_palette_glitch_base[i << 2] = color;
   }
   else if (address & 3)
   {
      ppu_palette_glitch_base[address & 0x1F] = color;
   }

   ppu_palette_glitch_apply_all();
}

void ppu_palette_glitch_set_enabled(int enabled)
{
   ppu_palette_glitch_enabled = enabled ? 1 : 0;
   if (!ppu_palette_glitch_enabled)
      ppu_palette_glitch_phase = 0;
   ppu_palette_glitch_apply_all();
}

void ppu_palette_glitch_set_game_active(int active)
{
   ppu_palette_glitch_game_active = active ? 1 : 0;
   ppu_palette_glitch_apply_all();
}

void ppu_palette_glitch_cycle(void)
{
   if (!ppu_palette_glitch_enabled || !ppu_palette_glitch_game_active)
      return;

   ppu_palette_glitch_phase = (uint8)((ppu_palette_glitch_phase + 1) % PPU_PALETTE_GLITCH_PHASES);
   ppu_palette_glitch_apply_all();
}

void ppu_setcontext(ppu_t *src_ppu)
{
   int page;
   ASSERT(src_ppu);
   ppu = *src_ppu;
   memcpy(ppu_sprite0_sync_oam, ppu.oam, sizeof(ppu_sprite0_sync_oam));

   for (page = 8; page < 16; page++)
      ppu_copy_page_pointer(&ppu, src_ppu, page);

   ppu_palette_glitch_capture_base();
   ppu_palette_glitch_apply_all();
}

void ppu_getcontext(ppu_t *dest_ppu)
{
   int page;

   ASSERT(dest_ppu);
   *dest_ppu = ppu;

   for (page = 8; page < 16; page++)
      ppu_copy_page_pointer(dest_ppu, &ppu, page);
}

ppu_t *ppu_create(void)
{
   static bool pal_generated = false;
   ppu_t *temp;

   temp = &ppu;
   memset(temp, 0, sizeof(ppu_t));

   temp->latchfunc = NULL;
   temp->vromswitch = NULL;
   temp->vram_present = false;
   temp->nt_3000_mirrors = true;
   temp->drawsprites = true;

   /* TODO: probably a better way to do this... */
   if (false == pal_generated)
   {
      pal_generate();
      pal_generated = true;
   }

   /* Initialize decode LUTs once */
   ppu_init_luts();

   ppu_setdefaultpal(temp);

   return temp;
}

void ppu_destroy(ppu_t **src_ppu)
{
   if (*src_ppu)
   {
      if (*src_ppu != &ppu)
         free(*src_ppu);
      else
         memset(&ppu, 0, sizeof(ppu));
      *src_ppu = NULL;
   }
}

void ppu_setpage(int size, int page_num, uint8 *location)
{
   /* deliberately fall through */
   switch (size)
   {
   case 8:
      ppu.page[page_num++] = location;
      ppu.page[page_num++] = location;
      ppu.page[page_num++] = location;
      ppu.page[page_num++] = location;
   case 4:
      ppu.page[page_num++] = location;
      ppu.page[page_num++] = location;
   case 2:
      ppu.page[page_num++] = location;
   case 1:
      ppu.page[page_num++] = location;
      break;
   }
}

INLINE void ppu_notify_addr(uint16 addr)
{
    if (mapper_ppu_hook && (addr & 0x2000) == 0)
        mapper_ppu_hook(addr & 0x1FFF);
}

/* $2007 access: step v like the rendering pipeline does while rendering,
 * otherwise by the $2000 increment; clock the mapper only when idle. */
INLINE void ppu_advance_vaddr(void)
{
   if (is_rendering())
   {
      inc_h_rendering();
      inc_v_rendering();
   }
   else
   {
      ppu.vaddr = (ppu.vaddr + ppu.vaddr_inc) & 0x3FFF;
   }

   if (!ppu_enabled())
      ppu_notify_addr(ppu.vaddr);
}

/* make sure $3000-$3F00 mirrors $2000-$2F00 */
void ppu_mirrorhipages(void)
{
   ppu.nt_3000_mirrors = true;
   ppu.page[12] = ppu.page[8] - 0x1000;
   ppu.page[13] = ppu.page[9] - 0x1000;
   ppu.page[14] = ppu.page[10] - 0x1000;
   ppu.page[15] = ppu.page[11] - 0x1000;
}

void ppu_mirror(int nt1, int nt2, int nt3, int nt4)
{
   ppu.nt_3000_mirrors = true;
   ppu.page[8] = ppu.nametab + (nt1 << 10) - 0x2000;
   ppu.page[9] = ppu.nametab + (nt2 << 10) - 0x2400;
   ppu.page[10] = ppu.nametab + (nt3 << 10) - 0x2800;
   ppu.page[11] = ppu.nametab + (nt4 << 10) - 0x2C00;
   ppu.page[12] = ppu.page[8] - 0x1000;
   ppu.page[13] = ppu.page[9] - 0x1000;
   ppu.page[14] = ppu.page[10] - 0x1000;
   ppu.page[15] = ppu.page[11] - 0x1000;
}

void ppu_setnametable_3000_mirror(bool enabled)
{
   ppu.nt_3000_mirrors = enabled;
}

static void mem_trash(uint8 *buffer, int length)
{
   int i;

   for (i = 0; i < length; i++)
      buffer[i] = (uint8) rand();
}

/* reset state of ppu */
void ppu_reset(int reset_type)
{
   if (HARD_RESET == reset_type)
      mem_trash(ppu.oam, 256);
   memcpy(ppu_sprite0_sync_oam, ppu.oam, sizeof(ppu_sprite0_sync_oam));

   ppu.ctrl0 = 0;
   ppu.ctrl1 = PPU_CTRL1F_OBJON | PPU_CTRL1F_BGON;
   ppu.stat = 0;
   ppu.flipflop = 0;
   ppu.vaddr = ppu.vaddr_latch = 0x2000;
   ppu.oam_addr = 0;
   ppu.tile_xofs = 0;

   ppu.latch = 0;
   ppu.vram_accessible = true;
   ppu.nt_3000_mirrors = true;
}

/* we render a scanline of graphics first so we know exactly
** where the sprite 0 strike is going to occur (in terms of
** cpu cycles), using the relation that 3 pixels == 1 cpu cycle
*/
static void PPU_SPRITE_HOT_ATTR ppu_setstrike(int x_abs)
{
   if (false == ppu.strikeflag)
   {
      const uint32 CYC_PER_SCAN = 341 / 3;

      /* Rewind to the start of the current scanline, then add
      ** ceil((x_abs + 2) / 3) cycles.
      */
      ppu.strikeflag = true;
      ppu.strike_cycle = nes6502_getcycles(false) - CYC_PER_SCAN +
                         (x_abs + 4) / 3;
   }
}

static void ppu_oamdma(uint8 value)
{
   uint32 cpu_address;
   uint8 oam_loc;

   cpu_address = (uint32) (value << 8);

   /* Sprite DMA starts at the current SPRRAM address */
   oam_loc = ppu.oam_addr;
   do
   {
      uint8 dma_value = nes6502_getbyte(cpu_address++);
      if (oam_loc < sizeof(ppu_sprite0_sync_oam))
         ppu_sprite0_sync_oam[oam_loc] = dma_value;
      nes_glitch_apply_oam_write(oam_loc, &dma_value);
      ppu.oam[oam_loc++] = dma_value;
   }
   while (oam_loc != ppu.oam_addr);

   /* TODO: enough with houdini */
   cpu_address -= 256;
   /* Odd address in $2003 */
   if ((ppu.oam_addr >> 2) & 1)
   {
      for (oam_loc = 4; oam_loc < 8; oam_loc++) {
         uint8 dma_value = nes6502_getbyte(cpu_address++);
         nes_glitch_apply_oam_write(oam_loc, &dma_value);
         ppu.oam[oam_loc] = dma_value;
      }
      cpu_address += 248;
      for (oam_loc = 0; oam_loc < 4; oam_loc++) {
         uint8 dma_value = nes6502_getbyte(cpu_address++);
         ppu_sprite0_sync_oam[oam_loc] = dma_value;
         nes_glitch_apply_oam_write(oam_loc, &dma_value);
         ppu.oam[oam_loc] = dma_value;
      }
   }
   /* Even address in $2003 */
   else
   {
      for (oam_loc = 0; oam_loc < 8; oam_loc++) {
         uint8 dma_value = nes6502_getbyte(cpu_address++);
         if (oam_loc < sizeof(ppu_sprite0_sync_oam))
            ppu_sprite0_sync_oam[oam_loc] = dma_value;
         nes_glitch_apply_oam_write(oam_loc, &dma_value);
         ppu.oam[oam_loc] = dma_value;
      }
   }

   /* make the CPU spin for DMA cycles */
   uint32 cycles = nes6502_getcycles(false);
   if (cycles & 1)
      nes6502_burn(514);
   else
      nes6502_burn(513);
   nes6502_release();
}

/* TODO: this isn't the PPU! */
void ppu_writehigh(uint32 address, uint8 value)
{
   switch (address)
   {
   case PPU_OAMDMA:
      ppu_oamdma(value);
      break;

   case PPU_JOY0:
      /* VS system VROM switching - bleh!*/
      if (ppu.vromswitch)
         ppu.vromswitch(value);

      /* see if we need to strobe them joypads */
      value &= 1;

      if (0 == value && ppu.strobe)
         input_strobe();

      ppu.strobe = value;
      break;

   case PPU_JOY1: /* frame IRQ control */
      nes_setfiq(value);
      break;

   default:
      break;
   }
}

/* TODO: this isn't the PPU! */
uint8 ppu_readhigh(uint32 address)
{
   uint8 value;

   switch (address)
   {
   case PPU_JOY0:
      value = input_get(INP_JOYPAD0);
      break;

   case PPU_JOY1:
      /* TODO: better input handling */
      value = input_get(INP_ZAPPER | INP_JOYPAD1
                        /*| INP_ARKANOID*/
                        /*| INP_POWERPAD*/);
      break;

   default:
      value = 0xFF;
      break;
   }

   return value;
}

/* Read from $2000-$2007 */
uint8 ppu_read(uint32 address)
{
   uint8 value;

   /* handle mirrored reads up to $3FFF */
   switch (address & 0x2007)
   {
   case PPU_STAT:
      value = (ppu.stat & 0xE0) | (ppu.latch & 0x1F);

      if (ppu.strikeflag && nes6502_getcycles(false) >= ppu.strike_cycle)
         value |= PPU_STATF_STRIKE;

      /* clear both vblank flag and vram address flipflop */
      ppu.stat &= ~PPU_STATF_VBLANK;
      ppu.flipflop = 0;
      break;

   case PPU_VDATA:
      /* buffered VRAM reads */
      value = ppu.latch = ppu.vdata_latch;

      /* VRAM only accessible during VBL */
      if ((ppu.bg_on || ppu.obj_on) && !ppu.vram_accessible)
      {
         ppu.vdata_latch = 0xFF;
         nofrendo_log_printf("VRAM read at $%04X, scanline %d\n",
                    ppu.vaddr, nes_getcontextptr()->scanline);
      }
      else
      {
         uint32_t addr = ppu.vaddr & 0x3FFF;
         if (addr >= 0x3000 && (addr >= 0x3F00 || ppu.nt_3000_mirrors))
            addr -= 0x1000;
         ppu.vdata_latch = PPU_MEM(addr);
      }

      ppu_advance_vaddr();
      break;

   case PPU_OAMDATA:
   case PPU_CTRL0:
   case PPU_CTRL1:
   case PPU_OAMADDR:
   case PPU_SCROLL:
   case PPU_VADDR:
   default:
      value = ppu.latch;
      break;
   }

   return value;
}

/* Write to $2000-$2007 */
void ppu_write(uint32 address, uint8 value)
{
   /* Store in latch */
   ppu.latch = value;

   switch (address & 0x2007)
   {
   case PPU_CTRL0:
      ppu.ctrl0      = value;
      ppu.obj_height = (value & PPU_CTRL0F_OBJ16) ? 16 : 8;
      ppu.bg_base    = (value & PPU_CTRL0F_BGADDR) ? 0x1000 : 0;
      ppu.obj_base   = (value & PPU_CTRL0F_OBJADDR) ? 0x1000 : 0;
      ppu.vaddr_inc  = (value & PPU_CTRL0F_ADDRINC) ? 32 : 1;
      ppu.tile_nametab = value & PPU_CTRL0F_NAMETAB;

      /* Update temporary address bits 10-11. */
      ppu.vaddr_latch = (ppu.vaddr_latch & ~0x0C00) | ((value & 3) << 10);
      break;

   case PPU_CTRL1:
      ppu.ctrl1   = value;
      ppu.obj_on  = (value & PPU_CTRL1F_OBJON)  != 0;
      ppu.bg_on   = (value & PPU_CTRL1F_BGON)   != 0;
      ppu.obj_mask= (value & PPU_CTRL1F_OBJMASK)==0;
      ppu.bg_mask = (value & PPU_CTRL1F_BGMASK)==0;
      break;

   case PPU_OAMADDR:
      ppu.oam_addr = value;
      break;

   case PPU_OAMDATA:
      if (ppu.oam_addr < sizeof(ppu_sprite0_sync_oam))
         ppu_sprite0_sync_oam[ppu.oam_addr] = value;
      nes_glitch_apply_oam_write(ppu.oam_addr, &value);
      ppu.oam[ppu.oam_addr++] = value;
      break;

   case PPU_SCROLL:
      if (!ppu.flipflop) {           /* first write */
         ppu.vaddr_latch  = (ppu.vaddr_latch & ~0x001F) | (value >> 3);
         ppu.tile_xofs    =  value & 7;
      } else {                       /* second write */
         ppu.vaddr_latch  = (ppu.vaddr_latch & ~0x73E0)
                          | ((value & 0xF8) << 2)       /* coarse Y + fine Y */
                          | ((value & 7) << 12);        /* fine Y */
      }
      /* $2005 changes the latch, not v. */
      ppu.flipflop ^= 1;
      break;

   case PPU_VADDR:
      if (!ppu.flipflop) {           /* First write: high 6 bits. */
         ppu.vaddr_latch = (ppu.vaddr_latch & 0x00FF)
                         | ((value & 0x3F) << 8);
      } else {                       /* Second write: low 8 bits. */
         ppu.vaddr_latch = (ppu.vaddr_latch & 0xFF00) | value;
         ppu.vaddr       =  ppu.vaddr_latch;

         if (!ppu_enabled())             /* BG+OBJ both off? */
            ppu_notify_addr(ppu.vaddr);  /* edge can happen now */
      }
      ppu.flipflop ^= 1;
      break;

   case PPU_VDATA:
      /* v is masked to the 14-bit bus; pattern/nametable space is below $3F00 */
      if ((ppu.vaddr & 0x3FFF) < 0x3F00)
      {
         /* VRAM only accessible during VBL */
         if ((ppu.bg_on || ppu.obj_on) && !ppu.vram_accessible)
         {
            nofrendo_log_printf("VRAM write %04X on active scan-line %d\n",
                     ppu.vaddr, nes_getcontextptr()->scanline);
         }
         else
         {
            uint32_t addr = ppu.vaddr & 0x3FFF;
            if (addr < 0x2000 && !ppu.vram_present)
               return;
            if (addr >= 0x3000 && addr < 0x3F00 && ppu.nt_3000_mirrors)
               addr -= 0x1000;
            *PPU_MEM_WPTR(addr) = value;
         }
      }
      else
      {
         ppu_palette_glitch_write_base(ppu.vaddr, value);
      }

      ppu_advance_vaddr();
      break;
   }
}


/* Builds a 256 color 8-bit palette based on a 64-color NES palette
** Note that we set it up 3 times so that we flip bits on the primary
** NES buffer for priorities
*/
static void ppu_buildpalette(ppu_t *src_ppu, rgb_t *pal)
{
   int i;

   /* Set it up 3 times, for sprite priority/BG transparency trickery */
   for (i = 0; i < 64; i++)
   {
      src_ppu->curpal[i].r = src_ppu->curpal[i + 64].r
                           = src_ppu->curpal[i + 128].r = pal[i].r;
      src_ppu->curpal[i].g = src_ppu->curpal[i + 64].g
                           = src_ppu->curpal[i + 128].g = pal[i].g;
      src_ppu->curpal[i].b = src_ppu->curpal[i + 64].b
                           = src_ppu->curpal[i + 128].b = pal[i].b;
   }

   for (i = 0; i < GUI_TOTALCOLORS; i++)
   {
      src_ppu->curpal[i + 192].r = gui_pal[i].r;
      src_ppu->curpal[i + 192].g = gui_pal[i].g;
      src_ppu->curpal[i + 192].b = gui_pal[i].b;
   }
}

/* build the emulator specific palette based on a 64-entry palette
** input palette can be either nes_palette or a 64-entry RGB palette
** read in from disk (i.e. for VS games)
*/
void ppu_setpal(ppu_t *src_ppu, rgb_t *pal)
{
   ppu_buildpalette(src_ppu, pal);
   vid_setpalette(src_ppu->curpal);
}

void ppu_setdefaultpal(ppu_t *src_ppu)
{
   ppu_setpal(src_ppu, nes_palette);
}

void ppu_setlatchfunc(ppulatchfunc_t func)
{
   ppu.latchfunc = func;
}

void ppu_setvromswitch(ppuvromswitch_t func)
{
   ppu.vromswitch = func;
}

/* rendering routines */
INLINE void draw_bgtile(uint8 *surface, uint8 pat1, uint8 pat2,
                        const uint8 *colors)
{
   unsigned long long pix = exp1_lut[pat1] | exp2_lut[pat2];
   /* Extract each byte and map through colors */
   surface[0] = colors[(uint8)(pix >> 56)];
   surface[1] = colors[(uint8)(pix >> 48)];
   surface[2] = colors[(uint8)(pix >> 40)];
   surface[3] = colors[(uint8)(pix >> 32)];
   surface[4] = colors[(uint8)(pix >> 24)];
   surface[5] = colors[(uint8)(pix >> 16)];
   surface[6] = colors[(uint8)(pix >> 8)];
   surface[7] = colors[(uint8)(pix >> 0)];
}

PPU_SPRITE_HOT_INLINE void ppu_latch_fetch(ppulatchfunc_t latchfunc, uint32 address, uint8 value)
{
   if (latchfunc)
      latchfunc(address & 0x1FFF, value);
}

static inline uint32 PPU_SPRITE_HOT_ATTR get_oamtile_pixels(uint8 attrib, uint8 pat1, uint8 pat2)
{
   const uint32 *exp1_tbl;
   const uint32 *exp2_tbl;

   /* HFLIP uses pre-expanded tables. */
   if (attrib & OAMF_HFLIP)
   {
      exp1_tbl = exp1_sprite_hflip_lut;
      exp2_tbl = exp2_sprite_hflip_lut;
   }
   else
   {
      exp1_tbl = exp1_sprite_lut;
      exp2_tbl = exp2_sprite_lut;
   }

   return exp1_tbl[pat1] | exp2_tbl[pat2];
}

static void PPU_SPRITE_HOT_ATTR PPU_NOINLINE_ATTR draw_oamtile(uint8 *surface, uint8 attrib, uint8 pat1,
                                                               uint8 pat2, const uint8 *col_tbl)
{
   uint32 pix;
   uint8 v;

   if ((pat1 | pat2) == 0)
      return;

   pix = get_oamtile_pixels(attrib, pat1, pat2);

   if (attrib & OAMF_BEHIND)
   {
      v = (uint8)((pix >> 28) & 3); if (v) surface[0] = SP_PIXEL | (BG_CLEAR(surface[0]) ? col_tbl[v] : surface[0]);
      v = (uint8)((pix >> 24) & 3); if (v) surface[1] = SP_PIXEL | (BG_CLEAR(surface[1]) ? col_tbl[v] : surface[1]);
      v = (uint8)((pix >> 20) & 3); if (v) surface[2] = SP_PIXEL | (BG_CLEAR(surface[2]) ? col_tbl[v] : surface[2]);
      v = (uint8)((pix >> 16) & 3); if (v) surface[3] = SP_PIXEL | (BG_CLEAR(surface[3]) ? col_tbl[v] : surface[3]);
      v = (uint8)((pix >> 12) & 3); if (v) surface[4] = SP_PIXEL | (BG_CLEAR(surface[4]) ? col_tbl[v] : surface[4]);
      v = (uint8)((pix >> 8 ) & 3); if (v) surface[5] = SP_PIXEL | (BG_CLEAR(surface[5]) ? col_tbl[v] : surface[5]);
      v = (uint8)((pix >> 4 ) & 3); if (v) surface[6] = SP_PIXEL | (BG_CLEAR(surface[6]) ? col_tbl[v] : surface[6]);
      v = (uint8)((pix >> 0 ) & 3); if (v) surface[7] = SP_PIXEL | (BG_CLEAR(surface[7]) ? col_tbl[v] : surface[7]);
   }
   else
   {
      v = (uint8)((pix >> 28) & 3); if (v && SP_CLEAR(surface[0])) surface[0] = SP_PIXEL | col_tbl[v];
      v = (uint8)((pix >> 24) & 3); if (v && SP_CLEAR(surface[1])) surface[1] = SP_PIXEL | col_tbl[v];
      v = (uint8)((pix >> 20) & 3); if (v && SP_CLEAR(surface[2])) surface[2] = SP_PIXEL | col_tbl[v];
      v = (uint8)((pix >> 16) & 3); if (v && SP_CLEAR(surface[3])) surface[3] = SP_PIXEL | col_tbl[v];
      v = (uint8)((pix >> 12) & 3); if (v && SP_CLEAR(surface[4])) surface[4] = SP_PIXEL | col_tbl[v];
      v = (uint8)((pix >> 8 ) & 3); if (v && SP_CLEAR(surface[5])) surface[5] = SP_PIXEL | col_tbl[v];
      v = (uint8)((pix >> 4 ) & 3); if (v && SP_CLEAR(surface[6])) surface[6] = SP_PIXEL | col_tbl[v];
      v = (uint8)((pix >> 0 ) & 3); if (v && SP_CLEAR(surface[7])) surface[7] = SP_PIXEL | col_tbl[v];
   }
}

static int PPU_SPRITE_HOT_ATTR PPU_NOINLINE_ATTR draw_oamtile_strike(uint8 *surface, uint8 attrib, uint8 pat1,
                                                                     uint8 pat2, const uint8 *col_tbl)
{
   uint32 pix;
   int strike_pixel;

   if ((pat1 | pat2) == 0)
      return -1;

   pix = get_oamtile_pixels(attrib, pat1, pat2);
   strike_pixel = -1;

   if (attrib & OAMF_BEHIND)
   {
      uint8 v;

      v = (uint8)((pix >> 28) & 3); if (v) { if (BG_SOLID(surface[0])) strike_pixel = 0; surface[0] = SP_PIXEL | (BG_CLEAR(surface[0]) ? col_tbl[v] : surface[0]); }
      v = (uint8)((pix >> 24) & 3); if (v) { if (strike_pixel < 0 && BG_SOLID(surface[1])) strike_pixel = 1; surface[1] = SP_PIXEL | (BG_CLEAR(surface[1]) ? col_tbl[v] : surface[1]); }
      v = (uint8)((pix >> 20) & 3); if (v) { if (strike_pixel < 0 && BG_SOLID(surface[2])) strike_pixel = 2; surface[2] = SP_PIXEL | (BG_CLEAR(surface[2]) ? col_tbl[v] : surface[2]); }
      v = (uint8)((pix >> 16) & 3); if (v) { if (strike_pixel < 0 && BG_SOLID(surface[3])) strike_pixel = 3; surface[3] = SP_PIXEL | (BG_CLEAR(surface[3]) ? col_tbl[v] : surface[3]); }
      v = (uint8)((pix >> 12) & 3); if (v) { if (strike_pixel < 0 && BG_SOLID(surface[4])) strike_pixel = 4; surface[4] = SP_PIXEL | (BG_CLEAR(surface[4]) ? col_tbl[v] : surface[4]); }
      v = (uint8)((pix >> 8 ) & 3); if (v) { if (strike_pixel < 0 && BG_SOLID(surface[5])) strike_pixel = 5; surface[5] = SP_PIXEL | (BG_CLEAR(surface[5]) ? col_tbl[v] : surface[5]); }
      v = (uint8)((pix >> 4 ) & 3); if (v) { if (strike_pixel < 0 && BG_SOLID(surface[6])) strike_pixel = 6; surface[6] = SP_PIXEL | (BG_CLEAR(surface[6]) ? col_tbl[v] : surface[6]); }
      v = (uint8)((pix >> 0 ) & 3); if (v) { if (strike_pixel < 0 && BG_SOLID(surface[7])) strike_pixel = 7; surface[7] = SP_PIXEL | (BG_CLEAR(surface[7]) ? col_tbl[v] : surface[7]); }
   }
   else
   {
      uint8 v;

      v = (uint8)((pix >> 28) & 3); if (v) { if (BG_SOLID(surface[0])) strike_pixel = 0; if (SP_CLEAR(surface[0])) surface[0] = SP_PIXEL | col_tbl[v]; }
      v = (uint8)((pix >> 24) & 3); if (v) { if (strike_pixel < 0 && BG_SOLID(surface[1])) strike_pixel = 1; if (SP_CLEAR(surface[1])) surface[1] = SP_PIXEL | col_tbl[v]; }
      v = (uint8)((pix >> 20) & 3); if (v) { if (strike_pixel < 0 && BG_SOLID(surface[2])) strike_pixel = 2; if (SP_CLEAR(surface[2])) surface[2] = SP_PIXEL | col_tbl[v]; }
      v = (uint8)((pix >> 16) & 3); if (v) { if (strike_pixel < 0 && BG_SOLID(surface[3])) strike_pixel = 3; if (SP_CLEAR(surface[3])) surface[3] = SP_PIXEL | col_tbl[v]; }
      v = (uint8)((pix >> 12) & 3); if (v) { if (strike_pixel < 0 && BG_SOLID(surface[4])) strike_pixel = 4; if (SP_CLEAR(surface[4])) surface[4] = SP_PIXEL | col_tbl[v]; }
      v = (uint8)((pix >> 8 ) & 3); if (v) { if (strike_pixel < 0 && BG_SOLID(surface[5])) strike_pixel = 5; if (SP_CLEAR(surface[5])) surface[5] = SP_PIXEL | col_tbl[v]; }
      v = (uint8)((pix >> 4 ) & 3); if (v) { if (strike_pixel < 0 && BG_SOLID(surface[6])) strike_pixel = 6; if (SP_CLEAR(surface[6])) surface[6] = SP_PIXEL | col_tbl[v]; }
      v = (uint8)((pix >> 0 ) & 3); if (v) { if (strike_pixel < 0 && BG_SOLID(surface[7])) strike_pixel = 7; if (SP_CLEAR(surface[7])) surface[7] = SP_PIXEL | col_tbl[v]; }
   }

   return strike_pixel;
}

static void ppu_renderbg(uint8 *vidbuf)
{
   uint8 *bmp_ptr, *data_ptr, *tile_ptr, *attrib_ptr;
   uint32 refresh_vaddr, bg_offset, attrib_base;
   int tile_count;
   uint8 tile_index, x_tile, y_tile;
   uint8 col_high, attrib, attrib_shift;
   bool exgfx, split_en;
   ppulatchfunc_t latchfunc;

   /* draw a line of transparent background color if bg is disabled */
   if (false == ppu.bg_on)
   {
      memset(vidbuf, FULLBG, NES_SCREEN_WIDTH);
      return;
   }

   bmp_ptr = vidbuf - ppu.tile_xofs; /* scroll x */
   refresh_vaddr = 0x2000 + (ppu.vaddr & 0x0FE0); /* mask out x tile */
   x_tile = ppu.vaddr & 0x1F;
   y_tile = (ppu.vaddr >> 5) & 0x1F; /* to simplify calculations */
   bg_offset = ((ppu.vaddr >> 12) & 7) + ppu.bg_base; /* offset in y tile */

   /* calculate initial values */
   tile_ptr = &PPU_MEM(refresh_vaddr + x_tile); /* pointer to tile index */
   attrib_base = (refresh_vaddr & 0x2C00) + 0x3C0 + ((y_tile & 0x1C) << 1);
   attrib_ptr = &PPU_MEM(attrib_base + (x_tile >> 2));
   attrib = *attrib_ptr++;
   attrib_shift = (x_tile & 2) + ((y_tile & 2) << 1);
   col_high = ((attrib >> attrib_shift) & 3) << 2;
   latchfunc = ppu.latchfunc;

   exgfx = mmc5_exgfx_enabled();
   split_en = mmc5_split_enabled();

   /* Fast path for the common case: no MMC5 split / ExGFX logic. */
   if (false == exgfx && false == split_en)
   {
      tile_count = 33;

      if (NULL == latchfunc)
      {
         while (tile_count--)
         {
            tile_index = *tile_ptr++;
            data_ptr = &PPU_MEM(bg_offset + (tile_index << 4));
            draw_bgtile(bmp_ptr, data_ptr[0], data_ptr[8], ppu.palette + col_high);
            bmp_ptr += 8;

            x_tile++;

            if (0 == (x_tile & 1))
            {
               if (0 == (x_tile & 3))
               {
                  if (32 == x_tile)
                  {
                     x_tile = 0;
                     refresh_vaddr ^= (1 << 10);
                     attrib_base ^= (1 << 10);
                     tile_ptr = &PPU_MEM(refresh_vaddr);
                     attrib_ptr = &PPU_MEM(attrib_base);
                  }

                  attrib = *attrib_ptr++;
               }

               attrib_shift ^= 2;
               col_high = ((attrib >> attrib_shift) & 3) << 2;
            }
         }
      }
      else
      {
         while (tile_count--)
         {
            uint32 pattern_addr;
            tile_index = *tile_ptr++;
            pattern_addr = bg_offset + ((uint32)tile_index << 4);
            data_ptr = &PPU_MEM(pattern_addr);
            draw_bgtile(bmp_ptr, data_ptr[0], data_ptr[8], ppu.palette + col_high);
            ppu_latch_fetch(latchfunc, pattern_addr + 8, data_ptr[8]);
            bmp_ptr += 8;

            x_tile++;

            if (0 == (x_tile & 1))
            {
               if (0 == (x_tile & 3))
               {
                  if (32 == x_tile)
                  {
                     x_tile = 0;
                     refresh_vaddr ^= (1 << 10);
                     attrib_base ^= (1 << 10);
                     tile_ptr = &PPU_MEM(refresh_vaddr);
                     attrib_ptr = &PPU_MEM(attrib_base);
                  }

                  attrib = *attrib_ptr++;
               }

               attrib_shift ^= 2;
               col_high = ((attrib >> attrib_shift) & 3) << 2;
            }
         }
      }

      goto blank_left_column;
   }

   /* MMC5 / special mapper background modes stay on the slower mixed path. */
   uint8 *mmc5_ex = NULL;
   uint8 mmc5_hi2 = 0;
   rominfo_t *mmc_cart = NULL;
   uint32 vrom_bytes = 0;
   uint8 *split_exram = NULL;
   uint8 vs_ctrl = 0;
   uint8 vs_scroll = 0;
   uint8 split_thresh = 0;
   bool split_right = false;
   if (exgfx)
   {
      mmc5_ex = mmc5_get_exram();
      mmc5_hi2 = mmc5_get_chr_hi2();
      mmc_cart = mmc_getinfo();
      if (mmc_cart)
      {
         if (mmc_cart->vrom_banks > 0)
            vrom_bytes = (uint32)mmc_cart->vrom_banks << 13; /* * 8192 */
         else if (mmc_cart->vram_banks > 0)
            vrom_bytes = (uint32)mmc_cart->vram_banks << 13;
      }
      if (NULL == mmc5_ex || NULL == mmc_cart || 0 == vrom_bytes)
         exgfx = false; /* fallback */
   }
   if (split_en)
   {
      split_exram = mmc5_get_exram();
      vs_ctrl = mmc5_split_ctrl();
      vs_scroll = mmc5_split_scroll();
      split_thresh = vs_ctrl & 0x1F;
      split_right = (vs_ctrl & 0x40) != 0;
      if (NULL == split_exram)
         split_en = false;
   }

   /* ppu fetches 33 tiles */
   tile_count = 33;
   int fetch_index = 0;
   while (tile_count--)
   {
      /* Tile number from nametable */
      tile_index = *tile_ptr++;

      /* Vertical split region? */
      bool in_split = false;
      if (split_en)
      {
         in_split = split_right ? (fetch_index >= split_thresh) : (fetch_index < split_thresh);
      }

      if (in_split)
      {
         /* ExRAM supplies tile and attribute data in split mode. */
         if (split_exram)
         {
            /* Compute tile coords in split region */
            int split_col = fetch_index - (split_right ? split_thresh : 0);
            if (!(vs_ctrl & 0x40)) /* left side */
               split_col = fetch_index; /* 0..thresh-1 */

            int ex_col = ((vs_scroll >> 3) + split_col) & 31; /* coarse X within split */
            int ex_row = y_tile & 31; /* same coarse Y */

            uint16 ex_tile_ofs = (uint16)(ex_row * 32 + ex_col);
            uint8 ex_tile = split_exram[ex_tile_ofs];

            /* attribute from ExRAM's attribute area */
            uint16 ex_attr_base = 960 + ((ex_row & 0x1C) << 1) + (ex_col >> 2);
            uint8 ex_attr = split_exram[ex_attr_base & 0x3FF];
            uint8 ex_shift = (ex_col & 2) + ((ex_row & 2) << 1);
            uint8 ex_col_high = ((ex_attr >> ex_shift) & 3) << 2;
            uint32 pattern_addr = bg_offset + ((uint32)ex_tile << 4);

            /* Pattern fetch via current BG mapping */
            data_ptr = &PPU_MEM(pattern_addr);
            draw_bgtile(bmp_ptr, data_ptr[0], data_ptr[8], ppu.palette + ex_col_high);
            ppu_latch_fetch(latchfunc, pattern_addr + 8, data_ptr[8]);
         }
         else
         {
            uint32 pattern_addr;
            /* Fallback: normal path */
            pattern_addr = bg_offset + ((uint32)tile_index << 4);
            data_ptr = &PPU_MEM(pattern_addr);
            draw_bgtile(bmp_ptr, data_ptr[0], data_ptr[8], ppu.palette + col_high);
            ppu_latch_fetch(latchfunc, pattern_addr + 8, data_ptr[8]);
         }
      }
      else if (exgfx)
      {
         /* Compute ExRAM offset for this tile within the nametable */
         uint16 exofs = (uint16)((refresh_vaddr & 0x03FF) + ((uint16)(x_tile)));
         uint8 exv = mmc5_ex[exofs & 0x03FF];
         uint8 pal_idx = (exv & 0x03);
         uint16 hi8 = ((uint16)(mmc5_hi2 & 0x03) << 6) | ((uint16)(exv >> 2) & 0x3F);
         uint32 tile_abs = ((uint32)hi8 << 8) | (uint32)tile_index;
         uint32 fine_row = (uint32)((ppu.vaddr >> 12) & 7);
         uint32 off = (tile_abs << 4) + fine_row; /* plane 0 at +0, plane 1 at +8 */
         const uint8 *base = (mmc_cart->vrom_banks > 0) ? mmc_cart->vrom : mmc_cart->vram;
         uint8 pat1 = base[off % vrom_bytes];
         uint8 pat2 = base[(off + 8) % vrom_bytes];
         draw_bgtile(bmp_ptr, pat1, pat2, ppu.palette + (pal_idx << 2));
      }
      else
      {
         uint32 pattern_addr = bg_offset + ((uint32)tile_index << 4);
         data_ptr = &PPU_MEM(pattern_addr);
         draw_bgtile(bmp_ptr, data_ptr[0], data_ptr[8], ppu.palette + col_high);
         ppu_latch_fetch(latchfunc, pattern_addr + 8, data_ptr[8]);
      }
      bmp_ptr += 8;

      x_tile++;
      fetch_index++;

      if (!exgfx && !in_split && (0 == (x_tile & 1)))     /* check every 2 tiles (attr handling) */
      {
         if (0 == (x_tile & 3))  /* check every 4 tiles */
         {
            if (32 == x_tile)    /* check every 32 tiles */
            {
               x_tile = 0;
               refresh_vaddr ^= (1 << 10); /* switch nametable */
               attrib_base ^= (1 << 10);

               /* recalculate pointers */
               tile_ptr = &PPU_MEM(refresh_vaddr);
               attrib_ptr = &PPU_MEM(attrib_base);
            }

            /* Get the attribute byte */
            attrib = *attrib_ptr++;
         }

         attrib_shift ^= 2;
         col_high = ((attrib >> attrib_shift) & 3) << 2;
      }
      else if (exgfx || in_split)
      {
         /* When ExGFX is active, we still need to handle NT wrap for tile_ptr */
         if (x_tile == 32)
         {
            x_tile = 0;
            refresh_vaddr ^= (1 << 10);
            tile_ptr = &PPU_MEM(refresh_vaddr);
            /* attrib_ptr/attrib ignored under ExGFX path */
         }
      }
   }

   /* Blank left hand column if need be */
blank_left_column:
   if (ppu.bg_mask)
   {
      uint32 *buf_ptr = (uint32 *) vidbuf;
      uint32 bg_clear = FULLBG | FULLBG << 8 | FULLBG << 16 | FULLBG << 24;

      ((uint32 *) buf_ptr)[0] = bg_clear;
      ((uint32 *) buf_ptr)[1] = bg_clear;
   }
}

/* OAM entry */
typedef struct obj_s
{
   uint8 y_loc;
   uint8 tile;
   uint8 atr;
   uint8 x_loc;
} obj_t;

static void PPU_SPRITE_HOT_ATTR ppu_build_scanline_oam(void)
{
   obj_t *sprite_ptr;
   uint8 sprite_height;
   int sprite_num;

   memset(ppu_scanline_oam_count, 0, sizeof(ppu_scanline_oam_count));

   if (false == ppu.obj_on)
      return;

   sprite_height = ppu.obj_height;
   sprite_ptr = (obj_t *) ppu.oam;

   for (sprite_num = 0; sprite_num < 64; sprite_num++, sprite_ptr++)
   {
      uint8 sprite_y;
      int line_end, line;

      sprite_y = sprite_ptr->y_loc + 1;
      if ((0 == sprite_y) || (sprite_y >= NES_SCREEN_HEIGHT))
         continue;

      line_end = sprite_y + sprite_height;
      if (line_end > NES_SCREEN_HEIGHT)
         line_end = NES_SCREEN_HEIGHT;

      for (line = sprite_y; line < line_end; line++)
      {
         uint8 count;

         count = ppu_scanline_oam_count[line];
         if (count < PPU_MAXSPRITE)
         {
            ppu_scanline_sprite_t *entry;
            int y_offset;
            int row_offset;

            y_offset = line - sprite_y;
            if (y_offset > 7)
               y_offset += 8;

            if (sprite_ptr->atr & OAMF_VFLIP)
            {
               if (16 == sprite_height)
                  y_offset -= 23;
               else
                  y_offset -= 7;

               row_offset = -y_offset;
            }
            else
            {
               row_offset = y_offset;
            }

            entry = &ppu_scanline_oam[line][count];
            entry->sprite_num = (uint8) sprite_num;
            entry->x_loc = sprite_ptr->x_loc;
            entry->tile = sprite_ptr->tile;
            entry->atr = sprite_ptr->atr;
            entry->row_offset = (uint8) row_offset;
         }
         if (count < 0xFF)
            ppu_scanline_oam_count[line] = count + 1;
      }
   }
}

PPU_SPRITE_HOT_INLINE uint8 *ppu_fetch_oam_row(uint8 *buf_ptr, const ppu_scanline_sprite_t *entry,
                                bool obj16, uint32 vram_offset, uint8 *attrib,
                                uint8 **bmp_ptr, const uint8 **col_tbl,
                                uint32 *pattern_addr)
{
   uint8 *data_ptr;
   uint32 vram_adr;
   uint8 tile_index;

   *attrib = entry->atr;
   *bmp_ptr = buf_ptr + entry->x_loc;
   *col_tbl = ppu.palette + 16 + (((*attrib) & 3) << 2);
   tile_index = entry->tile;

   /* 8x16 even sprites use $0000, odd use $1000 */
   if (obj16)
      vram_adr = ((tile_index & 1) << 12) | ((tile_index & 0xFE) << 4);
   else
      vram_adr = vram_offset + (tile_index << 4);

   /* Get the address of the tile */
   data_ptr = &PPU_MEM(vram_adr);

   data_ptr += entry->row_offset;
   if (pattern_addr)
      *pattern_addr = vram_adr + (uint32)entry->row_offset;

   return data_ptr;
}

PPU_SPRITE_HOT_INLINE uint16 ppu_oam_pattern_base(const ppu_scanline_sprite_t *entry,
                                                   bool obj16, uint32 vram_offset)
{
   uint8 tile_index = entry->tile;
   if (obj16)
      return (uint16)(((tile_index & 1) << 12) | ((tile_index & 0xFE) << 4));
   return (uint16)(vram_offset + (tile_index << 4));
}

PPU_SPRITE_HOT_INLINE uint8 *ppu_fetch_oam_row_prepared(uint8 *buf_ptr,
                                const ppu_scanline_sprite_t *entry,
                                uint8 *data_ptr, uint8 *attrib,
                                uint8 **bmp_ptr, const uint8 **col_tbl)
{
   *attrib = entry->atr;
   *bmp_ptr = buf_ptr + entry->x_loc;
   *col_tbl = ppu.palette + 16 + (((*attrib) & 3) << 2);
   return data_ptr;
}

/* MMC3 only consumes A12 level transitions from sprite CHR fetches. Preserve
 * the first fetch timing at each transition, aggregate repeated-level timing,
 * and omit the repeated no-op mapper callbacks. */
static void PPU_SPRITE_HOT_ATTR ppu_prepare_coalesced_sprite_fetches(
                                const ppu_scanline_sprite_t *entries,
                                int spritecount, bool obj16, uint32 vram_offset,
                                uint8 **prepared_rows)
{
   const uint16 glitch_mask = glitch_runtime_get_ppu_mask();
   int repeated_fetches = 0;
   int last_a12 = -1;

   /* 8x8 sprites all use the same pattern-table half. This is the dominant
    * MMC3 case and reduces the entire scanline to one meaningful callback. */
   if (false == obj16)
   {
      ppu_fetch_delay();
      mapper_ppu_hook((uint16)vram_offset);
      if (spritecount > 1)
         ppu_fetch_delay_many(spritecount - 1);

      for (int i = 0; i < spritecount; ++i)
      {
         uint16 addr = ppu_oam_pattern_base(&entries[i], false, vram_offset);
         uint16 fetch_addr = addr & (uint16)~glitch_mask;
         prepared_rows[i] = &ppu.page[fetch_addr >> 10][fetch_addr] +
                            entries[i].row_offset;
      }
      return;
   }

   for (int i = 0; i < spritecount; ++i)
   {
      uint16 addr = ppu_oam_pattern_base(&entries[i], obj16, vram_offset);
      uint16 fetch_addr = addr & (uint16)~glitch_mask;
      int a12 = addr & 0x1000;
      prepared_rows[i] = &ppu.page[fetch_addr >> 10][fetch_addr] +
                         entries[i].row_offset;

      if (a12 == last_a12)
      {
         repeated_fetches++;
         continue;
      }

      if (repeated_fetches)
      {
         ppu_fetch_delay_many(repeated_fetches);
         repeated_fetches = 0;
      }

      ppu_fetch_delay();
      mapper_ppu_hook(addr);
      last_a12 = a12;
   }

   if (repeated_fetches)
      ppu_fetch_delay_many(repeated_fetches);
}

/* TODO: fetch valid OAM a scanline before, like the Real Thing */
static void PPU_SPRITE_HOT_ATTR ppu_renderoam(uint8 *vidbuf, int scanline)
{
   uint8 *buf_ptr;
   uint32 vram_offset, savecol[2];
   int sprite_index, spritecount;
   const ppu_scanline_sprite_t *entry;
   uint8 sprite_height;
   bool obj16;
   bool coalesced_fetches;
   uint8 *prepared_rows[PPU_MAXSPRITE];
   ppulatchfunc_t latchfunc;

   if (false == ppu.obj_on)
      return;

   /* Get our buffer pointer */
   buf_ptr = vidbuf;

   /* Save left hand column? */
   if (ppu.obj_mask)
   {
      savecol[0] = ((uint32 *) buf_ptr)[0];
      savecol[1] = ((uint32 *) buf_ptr)[1];
   }

   sprite_height = ppu.obj_height;
   obj16 = (16 == sprite_height);
   vram_offset = ppu.obj_base;
   latchfunc = ppu.latchfunc;
   spritecount = ppu_scanline_oam_count[scanline];
   if (0 == spritecount)
      goto restore_obj_mask;

   if (spritecount >= PPU_MAXSPRITE)
   {
      ppu.stat |= PPU_STATF_MAXSPRITE;
      spritecount = PPU_MAXSPRITE;
   }

   entry = &ppu_scanline_oam[scanline][0];
   coalesced_fetches = mapper_sprite_fetch_coalescing && (NULL == latchfunc);
   if (coalesced_fetches)
      ppu_prepare_coalesced_sprite_fetches(entry, spritecount, obj16,
                                           vram_offset, prepared_rows);

   {
      uint8 *data_ptr, *bmp_ptr;
      uint8 attrib;
      const uint8 *col_tbl;
      uint32 pattern_addr;

      data_ptr = coalesced_fetches
         ? ppu_fetch_oam_row_prepared(buf_ptr, entry, prepared_rows[0],
                                      &attrib, &bmp_ptr, &col_tbl)
         : ppu_fetch_oam_row(buf_ptr, entry, obj16, vram_offset,
                             &attrib, &bmp_ptr, &col_tbl, &pattern_addr);

      if ((0 == entry->sprite_num) && (false == ppu.strikeflag))
      {
         int strike_pixel;

         strike_pixel = draw_oamtile_strike(bmp_ptr, attrib, data_ptr[0], data_ptr[8], col_tbl);
         if (strike_pixel >= 0)
            ppu_setstrike(strike_pixel);
      }
      else
      {
         draw_oamtile(bmp_ptr, attrib, data_ptr[0], data_ptr[8], col_tbl);
      }

      if (latchfunc)
         ppu_latch_fetch(latchfunc, pattern_addr + 8, data_ptr[8]);
   }

   if (NULL == latchfunc)
   {
      for (sprite_index = 1; sprite_index < spritecount; sprite_index++)
      {
         uint8 *data_ptr, *bmp_ptr;
         uint8 attrib;
         const uint8 *col_tbl;

         entry = &ppu_scanline_oam[scanline][sprite_index];
         data_ptr = coalesced_fetches
            ? ppu_fetch_oam_row_prepared(buf_ptr, entry,
                                         prepared_rows[sprite_index],
                                         &attrib, &bmp_ptr, &col_tbl)
            : ppu_fetch_oam_row(buf_ptr, entry, obj16, vram_offset,
                                &attrib, &bmp_ptr, &col_tbl, NULL);
         draw_oamtile(bmp_ptr, attrib, data_ptr[0], data_ptr[8], col_tbl);
      }
   }
   else
   {
      for (sprite_index = 1; sprite_index < spritecount; sprite_index++)
      {
         uint8 *data_ptr, *bmp_ptr;
         uint8 attrib;
         const uint8 *col_tbl;
         uint32 pattern_addr;

         entry = &ppu_scanline_oam[scanline][sprite_index];
         data_ptr = ppu_fetch_oam_row(buf_ptr, entry, obj16, vram_offset,
                                      &attrib, &bmp_ptr, &col_tbl, &pattern_addr);
         draw_oamtile(bmp_ptr, attrib, data_ptr[0], data_ptr[8], col_tbl);
         ppu_latch_fetch(latchfunc, pattern_addr + 8, data_ptr[8]);
      }
   }

   /* Restore lefthand column */
restore_obj_mask:
   if (ppu.obj_mask)
   {
      ((uint32 *) buf_ptr)[0] = savecol[0];
      ((uint32 *) buf_ptr)[1] = savecol[1];
   }
}

/* Fake rendering preserves sprite 0 hits on skipped frames. */
static void PPU_SPRITE_HOT_ATTR ppu_fakeoam(int scanline)
{
   uint8 *data_ptr;
   obj_t *sprite_ptr;
   uint32 vram_adr;
   uint32 pix;
   int y_offset;
   int row_offset;
   uint32 pattern_addr;
   uint8 tile_index, attrib, pat1, pat2;
   uint8 sprite_height, sprite_y, sprite_x;

   /* we don't need to be here if strike flag is set */

   if (false == ppu.obj_on || ppu.strikeflag)
      return;

   sprite_height = ppu.obj_height;
   sprite_ptr = (obj_t *) ppu.oam;
   sprite_y = sprite_ptr->y_loc + 1;

   /* Check to see if sprite is out of range */
   if ((sprite_y > scanline) || (sprite_y <= (scanline - sprite_height))
       || (0 == sprite_y) || (sprite_y > 240))
      return;

   sprite_x = sprite_ptr->x_loc;
   tile_index = sprite_ptr->tile;
   attrib = sprite_ptr->atr;

   /* 8x16 even sprites use $0000, odd use $1000 */
   if (16 == ppu.obj_height)
      vram_adr = ((tile_index & 1) << 12) | ((tile_index & 0xFE) << 4);
   else
      vram_adr = ppu.obj_base + (tile_index << 4);

   data_ptr = &PPU_MEM(vram_adr);
   pattern_addr = vram_adr;

   /* Calculate offset (line within the sprite) */
   y_offset = scanline - sprite_y;
   if (y_offset > 7)
      y_offset += 8;

   /* Account for vertical flippage */
   if (attrib & OAMF_VFLIP)
   {
      if (16 == ppu.obj_height)
         y_offset -= 23;
      else
         y_offset -= 7;
      row_offset = -y_offset;
   }
   else
   {
      row_offset = y_offset;
   }

   data_ptr += row_offset;
   pattern_addr += (uint32)row_offset;

   pat1 = data_ptr[0];
   pat2 = data_ptr[8];
   if (ppu.latchfunc)
      ppu_latch_fetch(ppu.latchfunc, pattern_addr + 8, pat2);

   if ((pat1 | pat2) == 0)
      return;

   /* check for a solid sprite 0 pixel */
   pix = get_oamtile_pixels(attrib, pat1, pat2);

   if ((uint8)((pix >> 28) & 3))
      ppu_setstrike(sprite_x + 0);
   else if ((uint8)((pix >> 24) & 3))
      ppu_setstrike(sprite_x + 1);
   else if ((uint8)((pix >> 20) & 3))
      ppu_setstrike(sprite_x + 2);
   else if ((uint8)((pix >> 16) & 3))
      ppu_setstrike(sprite_x + 3);
   else if ((uint8)((pix >> 12) & 3))
      ppu_setstrike(sprite_x + 4);
   else if ((uint8)((pix >> 8) & 3))
      ppu_setstrike(sprite_x + 5);
   else if ((uint8)((pix >> 4) & 3))
      ppu_setstrike(sprite_x + 6);
   else if ((uint8)((pix >> 0) & 3))
      ppu_setstrike(sprite_x + 7);
}

/* Glitches may remove the rendered sprite-0 overlap that older games poll as
 * a timing signal. Recalculate only that signal from clean fetches and the
 * clean OAM shadow; the visible scanline remains fully glitched. */
static void ppu_preserve_sprite0_sync(int scanline)
{
   ppu_scanline_sprite_t entry;
   obj_t *sprite_ptr;
   uint8 *data_ptr, *bmp_ptr;
   const uint8 *col_tbl;
   uint8 attrib;
   uint8 sprite_height, sprite_y;
   int y_offset, row_offset;
   int strike_pixel;

   if (ppu.strikeflag || false == ppu.bg_on || false == ppu.obj_on)
      return;
   if (false == nes_glitch_is_active() &&
       0 == glitch_runtime_get_ppu_mask())
      return;

   /* Clean timing fetches cannot clock mapper or latch state. */
   if (mapper_ppu_hook || mapper_ppu_hook_nt || ppu.latchfunc ||
       mmc5_exgfx_enabled() || mmc5_split_enabled())
      return;

   sprite_ptr = (obj_t *) ppu_sprite0_sync_oam;
   sprite_height = ppu.obj_height;
   sprite_y = sprite_ptr->y_loc + 1;

   if ((sprite_y > scanline) || (sprite_y <= (scanline - sprite_height)) ||
       (0 == sprite_y) || (sprite_y > 240))
      return;

   y_offset = scanline - sprite_y;
   if (y_offset > 7)
      y_offset += 8;

   if (sprite_ptr->atr & OAMF_VFLIP)
   {
      if (16 == sprite_height)
         y_offset -= 23;
      else
         y_offset -= 7;
      row_offset = -y_offset;
   }
   else
   {
      row_offset = y_offset;
   }

   entry.sprite_num = 0;
   entry.x_loc = sprite_ptr->x_loc;
   entry.tile = sprite_ptr->tile;
   entry.atr = sprite_ptr->atr;
   entry.row_offset = (uint8) row_offset;

   ppu_sync_fetch_bypass = true;
   ppu_renderbg(ppu_sprite0_sync_line + 8);
   data_ptr = ppu_fetch_oam_row(ppu_sprite0_sync_line + 8, &entry,
                                (16 == sprite_height), ppu.obj_base,
                                &attrib, &bmp_ptr, &col_tbl, NULL);
   strike_pixel = draw_oamtile_strike(bmp_ptr, attrib, data_ptr[0],
                                      data_ptr[8], col_tbl);
   ppu_sync_fetch_bypass = false;

   if (strike_pixel >= 0)
      ppu_setstrike((int)entry.x_loc + strike_pixel);
}

bool ppu_enabled(void)
{
   return (ppu.bg_on || ppu.obj_on);
}

static void ppu_renderscanline(bitmap_t *bmp, int scanline, bool draw_flag)
{
   if (NULL == bmp)
      return;

   uint8 *buf = bmp->line[scanline];

   /* start scanline - transfer ppu latch into vaddr */
   if (ppu.bg_on || ppu.obj_on)
   {
      if (0 == scanline)
      {
         ppu.vaddr = ppu.vaddr_latch;
      }
      else
      {
         ppu.vaddr &= ~0x041F;
         ppu.vaddr |= (ppu.vaddr_latch & 0x041F);
      }
   }

   if (draw_flag)
      ppu_renderbg(buf);

   /* TODO: fetch obj data 1 scanline before */
   if (true == ppu.drawsprites && true == draw_flag)
      ppu_renderoam(buf, scanline);
   else
      ppu_fakeoam(scanline);

   ppu_preserve_sprite0_sync(scanline);
}


/* The per-scanline routines below stay in flash to preserve IRAM. */
void ppu_endscanline(int scanline)
{
   /* modify vram address at end of scanline */
   if (scanline < 240 && (ppu.bg_on || ppu.obj_on))
   {
      int ytile;

      /* check for max 3 bit y tile offset */
      if (7 == (ppu.vaddr >> 12))
      {
         ppu.vaddr &= ~0x7000;      /* clear y tile offset */
         ytile = (ppu.vaddr >> 5) & 0x1F;

         if (29 == ytile)
         {
            ppu.vaddr &= ~0x03E0;   /* clear y tile */
            ppu.vaddr ^= 0x0800;    /* toggle nametable */
         }
         else if (31 == ytile)
         {
            ppu.vaddr &= ~0x03E0;   /* clear y tile */
         }
         else
         {
            ppu.vaddr += 0x20;      /* increment y tile */
         }
      }
      else
      {
         ppu.vaddr += 0x1000;       /* increment tile y offset */
      }
   }
}

void ppu_checknmi(void)
{
   if (ppu.ctrl0 & PPU_CTRL0F_NMI)
      nes_nmi();
}

void ppu_scanline(bitmap_t *bmp, int scanline, bool draw_flag)
{
   if (scanline == 0 && ppu.bg_base == 0x1000 && ppu.obj_base == 0x0000)
      ppu_notify_addr(0x1000);
   if (scanline < 240)
   {
      if (0 == scanline)
         ppu_build_scanline_oam();

      /* Lower the Max Sprite per scanline flag */
      ppu.stat &= ~PPU_STATF_MAXSPRITE;
      ppu_renderscanline(bmp, scanline, draw_flag);
   }
   else if (241 == scanline)
   {
      ppu.stat |= PPU_STATF_VBLANK;
      ppu.vram_accessible = true;
   }
   else if (261 == scanline)
   {
      ppu.stat &= ~PPU_STATF_VBLANK;
      ppu.strikeflag = false;
      ppu.strike_cycle = (uint32) -1;
      ppu.vram_accessible = false;
   }
}

/*************************************************/
/* TODO: all this stuff should go somewhere else */
/*************************************************/
INLINE void draw_box(bitmap_t *bmp, int x, int y, int height)
{
   int i;
   uint8 *vid;

   vid = bmp->line[y] + x;

   for (i = 0; i < 10; i++)
      *vid++ = GUI_GRAY;
   vid += (bmp->pitch - 10);
   for (i = 0; i < height; i++)
   {
      vid[0] = vid[9] = GUI_GRAY;
      vid += bmp->pitch;
   }
   for (i = 0; i < 10; i++)
      *vid++ = GUI_GRAY;
}

INLINE void draw_deadsprite(bitmap_t *bmp, int x, int y, int height)
{
   int i, j, index;
   uint8 *vid;
   uint8 colbuf[8] = { GUI_BLACK, GUI_BLACK, GUI_BLACK, GUI_BLACK,
                       GUI_BLACK, GUI_BLACK, GUI_BLACK, GUI_DKGRAY };

   vid = bmp->line[y] + x;

   for (i = 0; i < height; i++)
   {
      index = i;

      if (height == 16)
         index >>= 1;

      for (j = 0; j < 8; j++)
      {
         *(vid + j) = colbuf[index++];
         index &= 7;
      }

      vid += bmp->pitch;
   }
}


/* Stuff for the OAM viewer */
static void draw_sprite(bitmap_t *bmp, int x, int y, uint8 tile_num, uint8 attrib)
{
   int line, height;
   int col_high, vram_adr;
   uint8 *vid, *data_ptr;
   uint32 pattern_addr;

   vid = bmp->line[y] + x;

   /* Get upper two bits of color */
   col_high = ((attrib & 3) << 2);

   /* 8x16 even sprites use $0000, odd use $1000 */
   height = ppu.obj_height;
   if (16 == height)
      vram_adr = ((tile_num & 1) << 12) | ((tile_num & 0xFE) << 4);
   /* else just use the offset from $2000 */
   else
      vram_adr = ppu.obj_base + (tile_num << 4);

   data_ptr = &PPU_MEM(vram_adr);
   pattern_addr = (uint32)vram_adr;

   for (line = 0; line < height; line++)
   {
      if (line == 8) {
         data_ptr += 8;
         pattern_addr += 8;
      }

      draw_bgtile(vid, data_ptr[0], data_ptr[8], ppu.palette + 16 + col_high);
      if (ppu.latchfunc)
         ppu_latch_fetch(ppu.latchfunc, pattern_addr + 8, data_ptr[8]);
      //draw_oamtile(vid, attrib, data_ptr[0], data_ptr[8], ppu.palette + 16 + col_high);

      data_ptr++;
      pattern_addr++;
      vid += bmp->pitch;
   }
}

void ppu_dumpoam(bitmap_t *bmp, int x_loc, int y_loc)
{
   int sprite, x_pos, y_pos, height;
   obj_t *spr_ptr;

   spr_ptr = (obj_t *) ppu.oam;
   height = ppu.obj_height;

   for (sprite = 0; sprite < 64; sprite++)
   {
      x_pos = ((sprite & 0x0F) << 3) + (sprite & 0x0F) + x_loc;
      if (height == 16)
         y_pos = (sprite & 0xF0) + (sprite >> 4) + y_loc;
      else
         y_pos = ((sprite & 0xF0) >> 1) + (sprite >> 4) + y_loc;

      draw_box(bmp, x_pos, y_pos, height);

      if (spr_ptr->y_loc && spr_ptr->y_loc < 240)
         draw_sprite(bmp, x_pos + 1, y_pos + 1, spr_ptr->tile, spr_ptr->atr);
      else
         draw_deadsprite(bmp, x_pos + 1, y_pos + 1, height);

      spr_ptr++;
   }
}

/* More of a debugging thing than anything else */
void ppu_dumppattern(bitmap_t *bmp, int table_num, int x_loc, int y_loc, int col)
{
   int x_tile, y_tile;
   uint8 *bmp_ptr, *data_ptr, *ptr;
   int tile_num, line;
   uint8 col_high;

   tile_num = 0;
   col_high = col << 2;

   for (y_tile = 0; y_tile < 16; y_tile++)
   {
      /* Get our pointer to the bitmap */
      bmp_ptr = bmp->line[y_loc] + x_loc;

      for (x_tile = 0; x_tile < 16; x_tile++)
      {
         data_ptr = &PPU_MEM((table_num << 12) + (tile_num << 4));
         ptr = bmp_ptr;

         for (line = 0; line < 8; line ++)
         {
            draw_bgtile(ptr, data_ptr[0], data_ptr[8], ppu.palette + col_high);
            data_ptr++;
            ptr += bmp->pitch;
         }

         bmp_ptr += 8;
         tile_num++;
      }
      y_loc += 8;
   }
}

/*
** $Log: nes_ppu.c,v $
** Revision 1.2  2001/04/27 14:37:11  neil
** wheeee
**
** Revision 1.1.1.1  2001/04/27 07:03:54  neil
** initial
**
** Revision 1.14  2000/11/29 12:58:23  matt
** timing/fiq fixes
**
** Revision 1.13  2000/11/27 19:36:15  matt
** more timing fixes
**
** Revision 1.12  2000/11/26 15:51:13  matt
** frame IRQ emulation
**
** Revision 1.11  2000/11/25 20:30:39  matt
** scanline emulation simplifications/timing fixes
**
** Revision 1.10  2000/11/24 14:56:02  matt
** fixed a long-standing sprite 0 strike bug
**
** Revision 1.9  2000/11/20 13:23:17  matt
** PPU fixes
**
** Revision 1.8  2000/11/19 13:47:30  matt
** problem with frame irqs fixed
**
** Revision 1.7  2000/11/19 13:40:19  matt
** more accurate ppu behavior
**
** Revision 1.6  2000/11/14 12:09:37  matt
** only generate the palette once, please
**
** Revision 1.5  2000/11/11 14:51:43  matt
** context get/set fixed
**
** Revision 1.4  2000/11/09 12:35:50  matt
** fixed timing problem with VRAM reads/writes
**
** Revision 1.3  2000/11/05 16:35:41  matt
** rolled rgb.h into bitmap.h
**
** Revision 1.2  2000/10/27 12:55:03  matt
** palette generating functions now take *this pointers
**
** Revision 1.1  2000/10/24 12:20:28  matt
** changed directory structure
**
** Revision 1.33  2000/10/23 15:53:08  matt
** better system handling
**
** Revision 1.32  2000/10/22 15:02:32  matt
** simplified mirroring
**
** Revision 1.31  2000/10/21 21:36:04  matt
** ppu cleanups / fixes
**
** Revision 1.30  2000/10/21 19:26:59  matt
** many more cleanups
**
** Revision 1.29  2000/10/10 13:58:15  matt
** stroustrup squeezing his way in the door
**
** Revision 1.28  2000/10/08 17:54:32  matt
** reject VRAM access out of VINT period
**
** Revision 1.27  2000/09/15 04:58:07  matt
** simplifying and optimizing APU core
**
** Revision 1.26  2000/09/08 11:57:29  matt
** no more nes_fiq
**
** Revision 1.25  2000/09/07 21:57:31  matt
** api change
**
** Revision 1.24  2000/07/31 04:27:59  matt
** one million cleanups
**
** Revision 1.23  2000/07/30 06:13:12  matt
** default to no FIQs on startup
**
** Revision 1.22  2000/07/30 04:32:32  matt
** emulation of the NES frame IRQ
**
** Revision 1.21  2000/07/25 02:25:53  matt
** safer xxx_destroy calls
**
** Revision 1.20  2000/07/23 15:12:43  matt
** removed unused variables, changed INLINE
**
** Revision 1.19  2000/07/21 04:50:39  matt
** moved palette calls out of nofrendo.c and into ppu_create
**
** Revision 1.18  2000/07/17 05:12:55  matt
** nes_ppu.c is no longer a scary place to be-- cleaner & faster
**
** Revision 1.17  2000/07/17 01:52:28  matt
** made sure last line of all source files is a newline
**
** Revision 1.16  2000/07/11 04:42:39  matt
** updated for new screen dimension defines
**
** Revision 1.15  2000/07/10 19:10:16  matt
** should bomb out now if a game tries to write to VROM
**
** Revision 1.14  2000/07/10 05:28:30  matt
** moved joypad/oam dma from apu to ppu
**
** Revision 1.13  2000/07/10 03:03:16  matt
** added ppu_getcontext() routine
**
** Revision 1.12  2000/07/09 03:46:05  matt
** using pitch instead of width...
**
** Revision 1.11  2000/07/06 16:42:40  matt
** better palette setting interface
**
** Revision 1.10  2000/07/05 22:49:25  matt
** changed mmc2 (punchout) tile-access switching
**
** Revision 1.9  2000/07/04 23:13:26  matt
** added an irq line drawing debug feature hack
**
** Revision 1.8  2000/06/26 04:58:08  matt
** accuracy changes
**
** Revision 1.7  2000/06/22 02:13:49  matt
** more accurate emulation of $2002
**
** Revision 1.6  2000/06/20 20:42:47  matt
** accuracy changes
**
** Revision 1.5  2000/06/20 00:05:12  matt
** tested and verified STAT quirk, added code
**
** Revision 1.4  2000/06/09 15:12:26  matt
** initial revision
**
*/
