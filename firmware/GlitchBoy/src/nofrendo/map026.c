/*
** Nofrendo (c) 1998-2000 Matthew Conte (matt@conte.com)
**
** mapper 26 interface (Konami VRC6 variant with A0/A1 swapped)
*/

#include "noftypes.h"
#include "nes_mmc.h"
#include "nes.h"
#include "log.h"
#include "vrcvisnd.h"

extern void nes_irq_ack(void);

static struct
{
   int counter, enabled;
   int latch, wait_state;
} irq26;

static void map26_init(void)
{
   irq26.counter = irq26.enabled = 0;
   irq26.latch = irq26.wait_state = 0;

   mmc_bankrom(16, 0x8000, 0);
   mmc_bankrom(8,  0xC000, 0);
   mmc_bankrom(8,  0xE000, MMC_LASTBANK);
}

static void map26_hblank(int vblank)
{
   UNUSED(vblank);
   if (irq26.enabled)
   {
      irq26.counter = (irq26.counter + 1) & 0xFF;
      if (0 == irq26.counter)
      {
         nes_irq();
         irq26.counter = irq26.latch;
         irq26.enabled = irq26.wait_state;
      }
   }
}

static inline uint32 swap_a0_a1(uint32 address)
{
   /* Swap A0 and A1 for VRC6b */
   uint32 low = address & 3;
   low = ((low & 1) << 1) | ((low & 2) >> 1);
   return (address & ~3) | low;
}

static void map26_write(uint32 address, uint8 value)
{
   uint32 a = swap_a0_a1(address);
   switch (a & 0xF003)
   {
   case 0x8000:
   case 0x8001:
   case 0x8002:
   case 0x8003:
      mmc_bankrom(16, 0x8000, value);
      break;

   case 0xB003:
      if (value & 0x20)
      {
         switch (value & 0x0C)
         {
         case 0x00: ppu_mirror(0, 1, 0, 1); break; /* vertical */
         case 0x04: ppu_mirror(0, 0, 1, 1); break; /* horizontal */
         case 0x08: ppu_mirror(0, 0, 0, 0); break; /* 1-screen A */
         case 0x0C: ppu_mirror(1, 1, 1, 1); break; /* 1-screen B */
         }
      }
      break;

   case 0xC000:
   case 0xC001:
   case 0xC002:
   case 0xC003:
      mmc_bankrom(8, 0xC000, value);
      break;

   case 0xD000: mmc_bankvrom(1, 0x0000, value); break;
   case 0xD001: mmc_bankvrom(1, 0x0400, value); break;
   case 0xD002: mmc_bankvrom(1, 0x0800, value); break;
   case 0xD003: mmc_bankvrom(1, 0x0C00, value); break;
   case 0xE000: mmc_bankvrom(1, 0x1000, value); break;
   case 0xE001: mmc_bankvrom(1, 0x1400, value); break;
   case 0xE002: mmc_bankvrom(1, 0x1800, value); break;
   case 0xE003: mmc_bankvrom(1, 0x1C00, value); break;

   case 0xF000:
      irq26.latch = value;
      break;
   case 0xF001:
      nes_irq_ack();
      irq26.enabled = (value >> 1) & 1;
      irq26.wait_state = value & 1;
      if (irq26.enabled) irq26.counter = irq26.latch;
      break;
   case 0xF002:
   case 0xF003:
      nes_irq_ack();
      irq26.enabled = irq26.wait_state;
      break;

   default:
#ifdef NOFRENDO_DEBUG
      nofrendo_log_printf("invalid VRC6b write: $%02X to $%04X", value, address);
#endif
      break;
   }
}

static map_memwrite map26_memwrite[] =
{
   { 0x8000, 0xFFFF, map26_write },
   {     -1,     -1, NULL }
};

mapintf_t map26_intf =
{
   26, /* mapper number */
   "Konami VRC6b", /* mapper name */
   map26_init, /* init routine */
   NULL, /* vblank callback */
   map26_hblank, /* hblank callback */
   NULL, /* memory read structure */
   map26_memwrite, /* memory write structure */
   &vrcvi_ext /* external sound device */
};

