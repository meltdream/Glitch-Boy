/*
** Nofrendo (c) 1998-2000 Matthew Conte (matt@conte.com)
**
** mapper 90: J.Y. Company (minimal implementation)
**
** Focus: stable PRG/CHR banking and basic mirroring to boot mapper 90 titles.
** - PRG: 4x 8 KiB windows selected at $8000-$8003
** - CHR: 8x 1 KiB windows; LSB at $9000-$9007, MSB at $A000-$A007
** - Mirroring: $D001 low 2 bits (00=V, 01=H, 10=1ScA, 11=1ScB)
**
** Advanced features (ROM nametables, extended mirroring, IRQ, outer bank)
** are not implemented here. This is sufficient to boot many JY carts.
*/

#include "noftypes.h"
#include "nes_mmc.h"
#include "nes_ppu.h"
#include "log.h"

static uint16 chr_bank[8];     /* up to 10 bits via LSB/MSB registers */
static uint16 chr_lsb[8];
static uint16 chr_msb[8];
static uint8  prg_bank[4];     /* 8 KiB banks for $8000/$A000/$C000/$E000 */

static void map90_sync_prg(void)
{
   mmc_bankrom(8, 0x8000, prg_bank[0]);
   mmc_bankrom(8, 0xA000, prg_bank[1]);
   mmc_bankrom(8, 0xC000, prg_bank[2]);
   mmc_bankrom(8, 0xE000, prg_bank[3]);
}

static void map90_sync_chr(void)
{
   /* Combine LSB/MSB (MSB assumed in bits 8-9) */
   for (int i = 0; i < 8; i++)
      chr_bank[i] = (chr_lsb[i] & 0xFF) | ((chr_msb[i] & 0x03) << 8);

   /* If VROM present, switch CHR banks. Otherwise VRAM is already mapped. */
   if (mmc_getinfo()->vrom_banks > 0)
   {
      for (int i = 0; i < 8; i++)
         mmc_bankvrom(1, i << 10, chr_bank[i]);
   }
}

static void map90_init(void)
{
   for (int i = 0; i < 8; i++) chr_lsb[i] = chr_msb[i] = chr_bank[i] = 0;
   prg_bank[0] = 0;
   prg_bank[1] = 1;
   prg_bank[2] = 2;
   /* Ensure reset vector is visible by default */
   prg_bank[3] = (uint8)(mmc_getinfo()->rom_banks * 2 - 1);

   map90_sync_prg();
   map90_sync_chr();
}

static void map90_write(uint32 address, uint8 value)
{
   switch (address & 0xF007)
   {
      /* PRG 8 KiB window selects */
      case 0x8000: prg_bank[0] = value; map90_sync_prg(); break;
      case 0x8001: prg_bank[1] = value; map90_sync_prg(); break;
      case 0x8002: prg_bank[2] = value; map90_sync_prg(); break;
      case 0x8003: prg_bank[3] = value; map90_sync_prg(); break;

      /* CHR LSB (1 KiB banks) */
      case 0x9000: case 0x9001: case 0x9002: case 0x9003:
      case 0x9004: case 0x9005: case 0x9006: case 0x9007:
      {
         int i = (int)(address & 0x7);
         chr_lsb[i] = value;
         map90_sync_chr();
         break;
      }
      /* CHR MSB (upper 2 bits) */
      case 0xA000: case 0xA001: case 0xA002: case 0xA003:
      case 0xA004: case 0xA005: case 0xA006: case 0xA007:
      {
         int i = (int)(address & 0x7);
         chr_msb[i] = value & 0x03;
         map90_sync_chr();
         break;
      }

      /* Simple mirroring control (low 2 bits), mapper 90 disables ROM-NT */
      case 0xD001:
      {
         switch (value & 0x03)
         {
            case 0: ppu_mirror(0,1,0,1); break;           /* vertical */
            case 1: ppu_mirror(0,0,1,1); break;           /* horizontal */
            case 2: ppu_mirror(0,0,0,0); break;           /* 1-screen A */
            case 3: ppu_mirror(1,1,1,1); break;           /* 1-screen B */
         }
         break;
      }

      default:
#ifdef NOFRENDO_DEBUG
         nofrendo_log_printf("mapper90: unhandled write %02X to %04X\n", value, address);
#endif
         break;
   }
}

static map_memwrite map90_memwrite[] =
{
   { 0x8000, 0xFFFF, map90_write },
   {     -1,     -1, NULL }
};

mapintf_t map90_intf =
{
   90, /* mapper number */
   "JY Company (minimal)", /* mapper name */
   map90_init, /* init routine */
   NULL, /* vblank callback */
   NULL, /* hblank callback */
   NULL, /* memory read structure */
   map90_memwrite, /* memory write structure */
   NULL /* external sound device */
};

