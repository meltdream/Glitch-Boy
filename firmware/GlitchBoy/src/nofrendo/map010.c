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
** map10.c
**
** mapper 10 interface
*/

#include "string.h"
#include "noftypes.h"
#include "nes_mmc.h"
#include "nes_ppu.h"

static uint8 latch[2];
static uint8 regs[4];
static uint8 prg_bank;

/* MMC4 CHR latch handling */
static void mmc10_latchfunc(uint32 address, uint8 value)
{
   uint32 a = address & 0x1FFF;
   int which = (a >= 0x1000) ? 1 : 0;

   uint32 base = which ? 0x1FD8 : 0x0FD8;

   /* Latch on $xFD8/$xFE8 in either pattern table half */
   if ((a & 0x1FF8) == base)
      latch[which] = 0xFD;
   else if ((a & 0x1FF8) == (base + 0x10))
      latch[which] = 0xFE;
   else
      return;

   mmc_bankvrom(4, which ? 0x1000 : 0x0000,
                regs[which * 2 + (latch[which] - 0xFD)]);
}

/* mapper 10: MMC4 */
static void map10_write(uint32 address, uint8 value)
{
   switch ((address & 0xF000) >> 12)
   {
   case 0xA:
      /* 16KB PRG at $8000 (last bank fixed at $C000). Unmasked so
         mmc_bankrom wraps non-power-of-two ROM sizes. */
      prg_bank = value;
      mmc_bankrom(16, 0x8000, prg_bank);
      break;

   case 0xB:
      /* Left CHR 4KB when latch = $FD */
      regs[0] = value;
      if (0xFD == latch[0])
         mmc_bankvrom(4, 0x0000, regs[0]);
      break;

   case 0xC:
      /* Left CHR 4KB when latch = $FE */
      regs[1] = value;
      if (0xFE == latch[0])
         mmc_bankvrom(4, 0x0000, regs[1]);
      break;

   case 0xD:
      /* Right CHR 4KB when latch = $FD */
      regs[2] = value;
      if (0xFD == latch[1])
         mmc_bankvrom(4, 0x1000, regs[2]);
      break;

   case 0xE:
      /* Right CHR 4KB when latch = $FE */
      regs[3] = value;
      if (0xFE == latch[1])
         mmc_bankvrom(4, 0x1000, regs[3]);
      break;

   case 0xF:
      if (value & 1)
         ppu_mirror(0, 0, 1, 1); /* horizontal */
      else
         ppu_mirror(0, 1, 0, 1); /* vertical */
      break;

   default:
      break;
   }
}

static void map10_init(void)
{
   memset(regs, 0, sizeof(regs));

   prg_bank = 0;
   mmc_bankrom(16, 0x8000, prg_bank);
   mmc_bankrom(16, 0xC000, mmc_getinfo()->rom_banks - 1);

   latch[0] = 0xFE;
   latch[1] = 0xFE;

   ppu_setlatchfunc(mmc10_latchfunc);
}

static map_memwrite map10_memwrite[] =
{
   { 0x8000, 0xFFFF, map10_write },
   {     -1,     -1, NULL }
};

mapintf_t map10_intf =
{
   10, /* mapper number */
   "MMC4", /* mapper name */
   map10_init, /* init routine */
   NULL, /* vblank callback */
   NULL, /* hblank callback */
   NULL, /* memory read structure */
   map10_memwrite, /* memory write structure */
   NULL /* external sound device */
};

