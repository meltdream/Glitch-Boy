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
** map2.c
**
** Mapper 2: UxROM / UNROM
**
** UxROM uses a fixed 16 KiB PRG bank at $C000-$FFFF and a switchable
** 16 KiB bank at $8000-$BFFF.  Writes to $8000-$FFFF select the bank,
** and original boards exhibit bus conflicts when writing to ROM space.
*/

#include "noftypes.h"
#include "nes_mmc.h"

/* Optional bus conflict emulation.  Define to 0 to disable. */
#ifndef MAP2_EMULATE_BUS_CONFLICTS
#define MAP2_EMULATE_BUS_CONFLICTS 1
#endif

static int prg_bank;     /* currently mapped bank at $8000 */
static int last_bank;    /* last PRG bank for fixed $C000-$FFFF area */

#if MAP2_EMULATE_BUS_CONFLICTS
/* Return the value driven on the bus when writing to ROM space. */
static uint8 map2_effective_value(uint32 address, uint8 value)
{
   rominfo_t *cart = mmc_getinfo();
   int bank = (address < 0xC000) ? prg_bank : last_bank;
   uint32 offset = (bank << 14) | (address & 0x3FFF);

   return value & cart->rom[offset % (cart->rom_banks << 14)];
}
#else
#define map2_effective_value(addr, val) (val)
#endif

static void map2_init(void)
{
   rominfo_t *cart = mmc_getinfo();

   last_bank = (cart->rom_banks > 0) ? cart->rom_banks - 1 : 0;
   prg_bank  = 0;

   /* Map fixed last bank and initial bank 0. */
   mmc_bankrom(16, 0xC000, last_bank);
   mmc_bankrom(16, 0x8000, prg_bank);
}

static void map2_write(uint32 address, uint8 value)
{
   if (address < 0x8000)
      return;

   int banks = mmc_getinfo()->rom_banks;
   uint8 bank = map2_effective_value(address, value);
   if (banks)
      bank &= (uint8)(banks - 1);

   prg_bank = bank;
   mmc_bankrom(16, 0x8000, prg_bank);
}

static map_memwrite map2_memwrite[] =
{
   { 0x8000, 0xFFFF, map2_write },
   {     -1,     -1, NULL }
};

mapintf_t map2_intf =
{
   2,              /* mapper number */
   "UNROM",        /* mapper name */
   map2_init,      /* init routine */
   NULL,           /* vblank callback */
   NULL,           /* hblank callback */
   NULL,           /* memory read structure */
   map2_memwrite,  /* memory write structure */
   NULL            /* external sound device */
};

/*
** $Log: map002.c,v $
** Revision 1.2  2001/04/27 14:37:11  neil
** wheeee
**
** Revision 1.1  2001/04/27 12:54:40  neil
** blah
**
** Revision 1.1.1.1  2001/04/27 07:03:54  neil
** initial
**
** Revision 1.1  2000/10/24 12:19:32  matt
** changed directory structure
**
** Revision 1.5  2000/10/22 19:17:46  matt
** mapper cleanups galore
**
** Revision 1.4  2000/10/21 19:33:38  matt
** many more cleanups
**
** Revision 1.3  2000/08/16 02:50:11  matt
** random mapper cleanups
**
** Revision 1.2  2000/07/06 02:48:43  matt
** clearly labelled structure members
**
** Revision 1.1  2000/07/04 23:11:45  matt
** initial revision
**
*/
