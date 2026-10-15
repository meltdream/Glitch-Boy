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
** map5.c
**
** mapper 5 interface
** $Id: map005.c,v 1.2 2001/04/27 14:37:11 neil Exp $
*/

#include <string.h>

#include "noftypes.h"
#include "nes_mmc.h"
#include "nes.h"
#include "nes_ppu.h"
#include "log.h"
#include "mmc5_snd.h"
#include "wram.h"
#include "mmc5.h"

/* Covers PRG modes ($5100, $5114-$5117), separate BG/sprite CHR banks via a
 * PPU A12 hook, PRG-RAM banking and protect ($5113, $5102/$5103), ExRAM and
 * fill-mode nametables ($5104-$5107), upper CHR bits ($5130), vertical split,
 * ExGFX, the multiplier ($5205/$5206) and scanline IRQs via triple
 * nametable-read detection, with the HBlank IRQ path as a fallback.
 */

static struct
{
   int counter, enabled;
   int reset, latch;
} irq;

static struct
{
   uint8 mode;
   uint8 bg_banks[8];
   uint8 spr_banks[4];
} mmc5_chr;

/* PRG banking */
static uint8 prg_mode = 3;      /* $5100 & 3: default 8K pages across all */
static uint8 prg_regs[4] = { 0, 0, 0, 0 }; /* $5114-$5117 */

/* CHR upper bits ($5130 low 2 bits extend bank) */
static uint16 chr_hi = 0; /* (value & 3) << 8, applied to 1KB indices */

/* Minimal ExRAM + Fill-mode support */
static uint8 exram[0x400];         /* $5C00-$5FFF */
static uint8 exram_mode = 0;       /* $5104 */
static uint8 nt_map = 0;           /* $5105 nametable mapping */
static uint8 fill_tile = 0;        /* $5106 */
static uint8 fill_color = 0;       /* $5107 low 2 bits -> replicated */
static uint8 fill_nt[0x400];       /* prebuilt fill nametable page */

/* WRAM Protect handshake ($5102/$5103) */
static uint8 prg_wp1 = 0, prg_wp2 = 0;

/* Multiplier ($5205/$5206) */
static uint8 mul_a = 0, mul_b = 0;

/* PPU CHR switching optimization: remember which half is mapped */
static int chr_left_is_bg = -1;    /* -1 = unknown, 0 = SPR mapped, 1 = BG mapped */
static int chr_right_is_spr = -1;  /* -1 = unknown, 0 = BG mapped, 1 = SPR mapped */

/* MMC5 scanline detect via triple-$2xxx read tracking */
static uint16 last_nt_addr = 0xFFFF;
static int    nt_repeat = 0;
static bool   after_triple = false;
static bool   in_frame = false;
static uint8  sc_counter = 0; /* internal scanline counter */

/* Vertical Split registers */
static uint8 vs_ctrl = 0;    /* $5200: bit7 enable, bit6 right/left, low5 threshold */
static uint8 vs_scroll = 0;  /* $5201: horizontal tile scroll within split nametable */

/* Public helpers for PPU overlay */
bool mmc5_exgfx_enabled(void)
{
   /* ExGFX when $5104 low 2 bits == 01 */
   return ((exram_mode & 0x03) == 0x01);
}

uint8 *mmc5_get_exram(void)
{
   return exram;
}

uint8 mmc5_get_chr_hi2(void)
{
   /* Extract the two raw bits from chr_hi shadow (we stored <<8 earlier) */
   return (uint8)((chr_hi >> 8) & 0x03);
}

bool mmc5_split_enabled(void)
{
   return (vs_ctrl & 0x80) != 0;
}

uint8 mmc5_split_ctrl(void)
{
   return vs_ctrl;
}

uint8 mmc5_split_scroll(void)
{
   return vs_scroll;
}

static int mmc5_chr_page_size(uint8 config)
{
   switch (config & 3)
   {
   case 0:
      return 8;

   case 1:
      return 4;

   case 2:
      return 2;

   default:
      return 1;
   }
}

static int mmc5_bg_page_size(void)
{
   return mmc5_chr_page_size(mmc5_chr.mode & 3);
}

static int mmc5_sprite_page_size(void)
{
   uint8 config = (mmc5_chr.mode >> 2) & 3;

   if (0 == config && (mmc5_chr.mode & 0x30))
      config = (mmc5_chr.mode >> 4) & 3;

   return mmc5_chr_page_size(config);
}

static void mmc5_map_chr_page(int size, uint32 address, int bank)
{
   rominfo_t *cart = mmc_getinfo();

   if (cart->vrom_banks > 0)
   {
      mmc_bankvrom(size, address, bank);
      return;
   }

   if ((NULL == cart->vram) || (cart->vram_banks <= 0))
      return;

   switch (size)
   {
   case 1:
      {
         int banks = cart->vram_banks * 8;
         uint8 *base;

         if (0 == banks)
            return;

         base = cart->vram + ((bank % banks) << 10);
         ppu_setpage(1, address >> 10, base - address);
      }
      break;

   case 2:
      {
         int banks = cart->vram_banks * 4;
         uint8 *base;

         if (0 == banks)
            return;

         base = cart->vram + ((bank % banks) << 11);
         ppu_setpage(2, address >> 10, base - address);
      }
      break;

   case 4:
      {
         int banks = cart->vram_banks * 2;
         uint8 *base;

         if (0 == banks)
            return;

         base = cart->vram + ((bank % banks) << 12);
         ppu_setpage(4, address >> 10, base - address);
      }
      break;

   case 8:
      {
         int banks = cart->vram_banks;
         uint8 *base;

         if (0 == banks)
            return;

         base = cart->vram + ((bank % banks) << 13);
         ppu_setpage(8, 0, base);
      }
      break;

   default:
      break;
   }
}

static void mmc5_sync_chr(void)
{
   int size = mmc5_bg_page_size();

   switch (size)
   {
   case 8:
      mmc5_map_chr_page(8, 0x0000, (chr_hi | mmc5_chr.bg_banks[7]));
      break;

   case 4:
      mmc5_map_chr_page(4, 0x0000, (chr_hi | mmc5_chr.bg_banks[3]));
      mmc5_map_chr_page(4, 0x1000, (chr_hi | mmc5_chr.bg_banks[7]));
      break;

   case 2:
      mmc5_map_chr_page(2, 0x0000, (chr_hi | mmc5_chr.bg_banks[1]));
      mmc5_map_chr_page(2, 0x0800, (chr_hi | mmc5_chr.bg_banks[3]));
      mmc5_map_chr_page(2, 0x1000, (chr_hi | mmc5_chr.bg_banks[5]));
      mmc5_map_chr_page(2, 0x1800, (chr_hi | mmc5_chr.bg_banks[7]));
      break;

   default:
      {
         int i;

         for (i = 0; i < 8; i++)
            mmc5_map_chr_page(1, i << 10, (chr_hi | mmc5_chr.bg_banks[i]));
      }
      break;
   }

   size = mmc5_sprite_page_size();

   switch (size)
   {
   case 8:
      mmc5_map_chr_page(8, 0x0000, (chr_hi | mmc5_chr.spr_banks[3]));
      break;

   case 4:
      mmc5_map_chr_page(4, 0x0000, (chr_hi | mmc5_chr.spr_banks[3]));
      mmc5_map_chr_page(4, 0x1000, (chr_hi | mmc5_chr.spr_banks[3]));
      break;

   case 2:
      mmc5_map_chr_page(2, 0x0000, (chr_hi | mmc5_chr.spr_banks[1]));
      mmc5_map_chr_page(2, 0x0800, (chr_hi | mmc5_chr.spr_banks[3]));
      mmc5_map_chr_page(2, 0x1000, (chr_hi | mmc5_chr.spr_banks[1]));
      mmc5_map_chr_page(2, 0x1800, (chr_hi | mmc5_chr.spr_banks[3]));
      break;

   default:
      {
         int i;

         for (i = 0; i < 8; i++)
            mmc5_map_chr_page(1, i << 10, (chr_hi | mmc5_chr.spr_banks[i & 3]));
      }
      break;
   }
}

static void mmc5_set_chr_mode(uint8 mode)
{
   mmc5_chr.mode = mode;
   mmc5_sync_chr();
}

/* Build/refresh the fill-mode nametable image */
static void mmc5_build_fill_nt(void)
{
   /* Tiles area ($2000-$23BF / $2400-$27BF / ...) */
   memset(fill_nt, fill_tile, 0x3C0);
   /* Attribute area ($xxC0-$xxFF): replicate 2-bit color across all nibbles */
   uint8 a = (fill_color & 0x03);
   uint8 attr = (a | (a << 2));   /* 2 bits -> 4 bits */
   attr |= (attr << 4);           /* 4 bits -> 8 bits */
   memset(fill_nt + 0x3C0, attr, 0x40);
}

/* Map a single nametable quadrant 0..3 as per $5105 */
static void mmc5_map_nametable_quadrant(int q, uint8 sel)
{
   /* Base PPU pages: 8..11 map $2000/$2400/$2800/$2C00 */
   int page = 8 + q;
   switch (sel & 3)
   {
      case 0: /* CIRAM pages 0/1 are already mapped by ppu_mirror */
      case 1:
         break;
      case 2: /* ExRAM */
         /* Point this page to exram[0..3FF] */
         ppu_setpage(1, page, exram - (0x2000 + (q << 10)));
         ppu_setpage(1, page + 4, exram - (0x3000 + (q << 10))); /* mirrors $3000-$3BFF */
         break;
      case 3: /* Fill-mode */
         ppu_setpage(1, page, fill_nt - (0x2000 + (q << 10)));
         ppu_setpage(1, page + 4, fill_nt - (0x3000 + (q << 10)));
         break;
   }
}

static void mmc5_update_nametables(void)
{
   /* First, map CIRAM quadrants via ppu_mirror (0 or 1)
      Then, override any pages selected as ExRAM (2) or Fill (3). */
   int nt0 = (nt_map >> 0) & 3;
   int nt1 = (nt_map >> 2) & 3;
   int nt2 = (nt_map >> 4) & 3;
   int nt3 = (nt_map >> 6) & 3;

   /* For ppu_mirror, coerce 2/3 to 0 (we'll override below) */
   int m0 = (nt0 <= 1) ? nt0 : 0;
   int m1 = (nt1 <= 1) ? nt1 : 0;
   int m2 = (nt2 <= 1) ? nt2 : 0;
   int m3 = (nt3 <= 1) ? nt3 : 0;
   ppu_mirror(m0, m1, m2, m3);

   /* Now override selected quadrants */
   mmc5_map_nametable_quadrant(0, nt0);
   mmc5_map_nametable_quadrant(1, nt1);
   mmc5_map_nametable_quadrant(2, nt2);
   mmc5_map_nametable_quadrant(3, nt3);
}

/* PRG sync: map 8K windows according to mode + regs */
static void mmc5_sync_prg(void)
{
   switch (prg_mode & 3)
   {
      case 0: /* 32K at $8000: use $5115 as 8K index (4× contiguous) */
      {
         uint8 base = prg_regs[1]; /* $5115 */
         /* Align to 32K (4×8K) boundary by masking low 2 bits */
         int bank32 = base >> 2;
         mmc_bankrom(32, 0x8000, bank32);
      }
      break;

      case 1: /* 16K @ $8000 (from $5115), 16K @ $C000 (from $5117) */
      {
         int b0 = prg_regs[1] >> 1; /* $5115 */
         int b2 = prg_regs[3] >> 1; /* $5117 */
         mmc_bankrom(16, 0x8000, b0);
         mmc_bankrom(16, 0xC000, b2);
      }
      break;

      case 2: /* 8K @ $8000 ($5114), $A000 ($5115); last 16K fixed */
      {
         mmc_bankrom(8, 0x8000, prg_regs[0]);
         mmc_bankrom(8, 0xA000, prg_regs[1]);
         /* Fixed penultimate and last */
         mmc_bankrom(8, 0xC000, mmc_getinfo()->rom_banks * 2 - 2);
         mmc_bankrom(8, 0xE000, mmc_getinfo()->rom_banks * 2 - 1);
      }
      break;

      default: /* 3: 8K @ all four windows */
         mmc_bankrom(8, 0x8000, prg_regs[0]);
         mmc_bankrom(8, 0xA000, prg_regs[1]);
         mmc_bankrom(8, 0xC000, prg_regs[2]);
         mmc_bankrom(8, 0xE000, prg_regs[3]);
         break;
   }
}

/* PPU access hook: select BG vs SPR CHR banks by A12 half */
static void mmc5_ppu_tick(uint16 addr)
{
   /* Only pattern space (<$2000) events are seen here */
   if (addr & 0x1000)
   {
      /* Right half -> favor SPR banks */
      if (chr_right_is_spr != 1)
      {
         int size = mmc5_sprite_page_size();
         switch (size)
         {
            case 8:
               mmc5_map_chr_page(8, 0x1000, (chr_hi | mmc5_chr.spr_banks[3]));
               break;
            case 4:
               mmc5_map_chr_page(4, 0x1000, (chr_hi | mmc5_chr.spr_banks[3]));
               break;
            case 2:
               mmc5_map_chr_page(2, 0x1000, (chr_hi | mmc5_chr.spr_banks[1]));
               mmc5_map_chr_page(2, 0x1800, (chr_hi | mmc5_chr.spr_banks[3]));
               break;
            default:
               for (int i = 4; i < 8; i++)
                  mmc5_map_chr_page(1, i << 10, (chr_hi | mmc5_chr.spr_banks[i & 3]));
               break;
         }
         chr_right_is_spr = 1;
      }
   }
   else
   {
      /* Left half -> favor BG banks */
      if (chr_left_is_bg != 1)
      {
         int size = mmc5_bg_page_size();
         switch (size)
         {
            case 8:
               mmc5_map_chr_page(8, 0x0000, (chr_hi | mmc5_chr.bg_banks[7]));
               break;
            case 4:
               mmc5_map_chr_page(4, 0x0000, (chr_hi | mmc5_chr.bg_banks[3]));
               break;
            case 2:
               mmc5_map_chr_page(2, 0x0000, (chr_hi | mmc5_chr.bg_banks[1]));
               mmc5_map_chr_page(2, 0x0800, (chr_hi | mmc5_chr.bg_banks[3]));
               break;
            default:
               for (int i = 0; i < 4; i++)
                  mmc5_map_chr_page(1, i << 10, (chr_hi | mmc5_chr.bg_banks[i]));
               break;
         }
         chr_left_is_bg = 1;
      }
   }
}

/* Nametable read hook for MMC5 scanline detection */
static void mmc5_nt_tick(uint16 addr)
{
   nes_t *nes = nes_getcontextptr();
   int sl = nes ? nes->scanline : -1;

   /* Clear in-frame at or after post-render scanline */
   if (sl >= 240) {
      in_frame = false;
      sc_counter = 0;
   }

   /* Track triples of identical NT reads */
   if (addr == last_nt_addr) {
      if (nt_repeat < 3) nt_repeat++;
   } else {
      last_nt_addr = addr;
      nt_repeat = 1;
   }

   if (!after_triple) {
      if (nt_repeat >= 3) {
         after_triple = true;
         nt_repeat = 0;
      }
      return;
   }

   /* Next read after triple -> scanline detected (PPU cycle ~4) */
   after_triple = false;

   if (sl >= 0 && sl < 240) {
      if (!in_frame) {
         in_frame = true;
         sc_counter = 0;
      } else {
         if (sc_counter < 0xFF)
            sc_counter++;
         if (irq.latch != 0 && sc_counter == irq.latch) {
            irq.reset = true; /* pending */
            if (irq.enabled)
               nes_irq();
         }
      }
   }
}

/* MMC5 - Castlevania III, etc */
static void map5_hblank(int vblank)
{
   UNUSED(vblank);

   if (irq.counter == nes_getcontextptr()->scanline)
   {
      if (true == irq.enabled)
      {
         nes_irq();
         irq.reset = true;
      }
      //   irq.reset = false;
      irq.counter = irq.latch;
   }
}

static void map5_write(uint32 address, uint8 value)
{
   /* ex-ram memory-- bleh! */
   if (address >= 0x5C00 && address <= 0x5FFF)
   {
      /* Modes 2/3 allow CPU read/write (3 = write-protect) */
      if ((exram_mode & 0x03) == 0x03)
         return; /* write-protected */
      exram[address - 0x5C00] = value;
      return;
   }

   switch (address)
   {
   case 0x5100:
      /* PRG page size setting */
      /* 0:32k 1:16k 2,3:8k */
      prg_mode = (value & 3);
      mmc5_sync_prg();
      break;

   case 0x5101:
      /* CHR page size setting */
      /* 0:8k 1:4k 2:2k 3:1k */
      mmc5_set_chr_mode(value);
      break;

   case 0x5102: /* PRG-RAM Protect 1 */
      prg_wp1 = value;
      /* Enable when $AA/$55; disable when $00/$00 */
      if (prg_wp1 == 0x00 && prg_wp2 == 0x00)
      {
         nes_set_wram_enable(false);
      }
      else if (prg_wp1 == 0xAA && prg_wp2 == 0x55)
      {
         nes_set_wram_enable(true);
      }
      break;

   case 0x5103: /* PRG-RAM Protect 2 */
      prg_wp2 = value;
      if (prg_wp1 == 0xAA && prg_wp2 == 0x55)
      {
         nes_set_wram_enable(true);
      }
      else if (prg_wp1 == 0x00 && prg_wp2 == 0x00)
      {
         nes_set_wram_enable(false);
      }
      break;

   case 0x5104:
      /* GFX mode setting */
      /*
      00:split mode
      01:split & exgraffix
      10:ex-ram
      11:exram + write protect
      */
      exram_mode = value;
      break;

   case 0x5105:
      nt_map = value;
      mmc5_update_nametables();
      break;

   case 0x5106:
      fill_tile = value;
      mmc5_build_fill_nt();
      mmc5_update_nametables();
      break;
   case 0x5107:
      fill_color = value;
      mmc5_build_fill_nt();
      mmc5_update_nametables();
      break;

   case 0x5113:
      /* 8KB PRG-RAM bank at $6000-$7FFF */
      mmc_bankwram(8, 0x6000, value);
      break;

   case 0x5114:
      prg_regs[0] = value;
      mmc5_sync_prg();
      break;

   case 0x5115:
      prg_regs[1] = value;
      mmc5_sync_prg();
      break;

   case 0x5116:
      prg_regs[2] = value;
      mmc5_sync_prg();
      break;

   case 0x5117:
      prg_regs[3] = value;
      mmc5_sync_prg();
      break;

   case 0x5120:
   case 0x5121:
   case 0x5122:
   case 0x5123:
   case 0x5124:
   case 0x5125:
   case 0x5126:
   case 0x5127:
      mmc5_chr.bg_banks[address - 0x5120] = value;
      mmc5_sync_chr();
      break;

   case 0x5128:
   case 0x5129:
   case 0x512A:
   case 0x512B:
      mmc5_chr.spr_banks[address - 0x5128] = value;
      mmc5_sync_chr();
      break;

   case 0x5130: /* Upper CHR bank bits */
      chr_hi = (value & 0x03) << 8;
      mmc5_sync_chr();
      break;

   case 0x5203:
      irq.counter = value;
      irq.latch = value;
//      irq.reset = false;
      break;

   case 0x5204:
      /* bit7: enable; bit0: ack pending */
      irq.enabled = (value & 0x80) ? true : false;
      if (value & 0x01)
         irq.reset = false;
//      irq.reset = false;
      break;

   case 0x5200:
      vs_ctrl = value;
      break;
   case 0x5201:
      vs_scroll = value;
      break;
   case 0x5202: /* split CHR bank: not implemented */
      break;

   case 0x5205: /* multiplier A */
      mul_a = value;
      break;
   case 0x5206: /* multiplier B */
      mul_b = value;
      break;

   default:
#ifdef NOFRENDO_DEBUG
      nofrendo_log_printf("unknown mmc5 write: $%02X to $%04X\n", value, address);
#endif /* NOFRENDO_DEBUG */
      break;
   }
}

static uint8 map5_read(uint32 address)
{
   /* Castlevania 3 IRQ counter */
   if (address == 0x5204)
   {
      /* bit7=in-frame, bit6=irq pending */
      uint8 val = 0;
      if (in_frame)
         val |= 0x80;
      if (irq.reset)
         val |= 0x40;
      return val;
   }
   else if (address >= 0x5C00 && address <= 0x5FFF)
   {
      return exram[address - 0x5C00];
   }
   else if (address == 0x5205)
   {
      uint16 prod = (uint16)mul_a * (uint16)mul_b;
      return (uint8)(prod & 0xFF);
   }
   else if (address == 0x5206)
   {
      uint16 prod = (uint16)mul_a * (uint16)mul_b;
      return (uint8)(prod >> 8);
   }
   else
   {
#ifdef NOFRENDO_DEBUG
      nofrendo_log_printf("invalid MMC5 read: $%04X", address);
#endif
      return 0xFF;
   }
}

static void map5_init(void)
{
   mmc_bankrom(8, 0x8000, MMC_LASTBANK);
   mmc_bankrom(8, 0xA000, MMC_LASTBANK);
   mmc_bankrom(8, 0xC000, MMC_LASTBANK);
   mmc_bankrom(8, 0xE000, MMC_LASTBANK);

   irq.counter = irq.enabled = 0;
   irq.reset = irq.latch = 0;

   memset(&mmc5_chr, 0, sizeof(mmc5_chr));
   mmc5_set_chr_mode(0);

   /* PRG/CHR defaults */
   prg_mode = 3;
   prg_regs[0] = prg_regs[1] = prg_regs[2] = prg_regs[3] = 0;
   chr_hi = 0;

   /* ExRAM/Fill defaults */
   memset(exram, 0, sizeof(exram));
   exram_mode = 0;
   nt_map = 0; /* all CIRAM0 */
   fill_tile = 0;
   fill_color = 0;
   mmc5_build_fill_nt();
   mmc5_update_nametables();

   /* PPU hooks: CHR separation and NT triple-read scanline detection */
   ppu_set_mapper_hook(mmc5_ppu_tick);
   ppu_set_nt_hook(mmc5_nt_tick);
}

static map_memwrite map5_memwrite[] =
{
   /* $5000 - $5015 handled by sound */
   { 0x5016, 0x5FFF, map5_write },
   { 0x8000, 0xFFFF, map5_write },
   {     -1,     -1, NULL }
};

static map_memread map5_memread[] =
{
   { 0x5204, 0x5206, map5_read },
   { 0x5C00, 0x5FFF, map5_read },
   {     -1,     -1, NULL }
};

mapintf_t map5_intf =
{
   5, /* mapper number */
   "MMC5", /* mapper name */
   map5_init, /* init routine */
   NULL, /* vblank callback */
   map5_hblank, /* hblank callback */
   map5_memread, /* memory read structure */
   map5_memwrite, /* memory write structure */
   &mmc5_ext /* external sound device */
};
/*
** $Log: map005.c,v $
** Revision 1.2  2001/04/27 14:37:11  neil
** wheeee
**
** Revision 1.1  2001/04/27 12:54:40  neil
** blah
**
** Revision 1.1.1.1  2001/04/27 07:03:54  neil
** initial
**
** Revision 1.2  2000/11/25 20:32:33  matt
** scanline interface change
**
** Revision 1.1  2000/10/24 12:19:32  matt
** changed directory structure
**
** Revision 1.11  2000/10/22 19:17:46  matt
** mapper cleanups galore
**
** Revision 1.10  2000/10/21 19:33:37  matt
** many more cleanups
**
** Revision 1.9  2000/10/17 03:23:16  matt
** added mmc5 sound interface
**
** Revision 1.8  2000/10/10 13:58:17  matt
** stroustrup squeezing his way in the door
**
** Revision 1.7  2000/08/16 02:50:11  matt
** random mapper cleanups
**
** Revision 1.6  2000/07/15 23:52:19  matt
** rounded out a bunch more mapper interfaces
**
** Revision 1.5  2000/07/10 13:51:25  matt
** using generic nes_irq() routine now
**
** Revision 1.4  2000/07/10 05:29:03  matt
** cleaned up some mirroring issues
**
** Revision 1.3  2000/07/06 02:48:43  matt
** clearly labelled structure members
**
** Revision 1.2  2000/07/05 05:04:51  matt
** fixed h-blank callback
**
** Revision 1.1  2000/07/04 23:11:45  matt
** initial revision
**
*/
