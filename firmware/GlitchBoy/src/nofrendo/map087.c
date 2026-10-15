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
** map087.c
**
** Mapper #87 (8K CHR switch, fixed PRG)
** Implementation by Firebug
** $Id: map087.c,v 1.2 2001/04/27 14:37:11 neil Exp $
**
*/

#include "noftypes.h"
#include "nes_mmc.h"
#include "nes.h"
#include "log.h"

/******************************************/
/* Mapper #87 init                        */
/******************************************/
static void map87_init(void)
{
  /* PRG: fixed 16 KiB @ $8000 (bank 0) and $C000 (last bank or same if 16 KiB) */
  mmc_bankrom(16, 0x8000, 0);
  mmc_bankrom(16, 0xC000, MMC_LASTBANK);

  /* CHR: start with bank 0 */
  mmc_bankvrom(8, 0x0000, 0);
}

/* Mapper 87 write handler ($6000-$7FFF only; no bus conflicts) */
static void map87_write(uint32 address, uint8 value)
{
  UNUSED(address);
  /* Select 8 KiB CHR bank using bits [1:0] */
  mmc_bankvrom(8, 0x0000, (value & 0x03));
}

static map_memwrite map87_memwrite [] =
{
   { 0x6000, 0x7FFF, map87_write },
   {     -1,     -1, NULL }
};

mapintf_t map87_intf =
{
   87,                               /* Mapper number */
   "8K CHR switch (Mapper 87)",     /* Mapper name */
   map87_init,                       /* Initialization routine */
   NULL,                             /* VBlank callback */
   NULL,                             /* HBlank callback */
   NULL,                             /* Memory read structure */
   map87_memwrite,                   /* Memory write structure */
   NULL                              /* External sound device */
};

/*
** $Log: map087.c,v $
** Revision 1.2  2001/04/27 14:37:11  neil
** wheeee
**
** Revision 1.1  2001/04/27 12:54:40  neil
** blah
**
** Revision 1.1  2001/04/27 10:57:41  neil
** wheee
**
** Revision 1.1  2000/12/30 06:34:44  firebug
** Initial revision
**
**
*/
