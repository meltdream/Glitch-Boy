/*
** Mapper 30: UNROM 512
**
** Implemented from NESdev's UNROM 512 / Mapper 30 documentation:
**  - 16 KiB switchable PRG at $8000, fixed last bank at $C000
**  - 8 KiB switchable CHR-RAM window into up to 32 KiB
**  - Header-controlled H/V, 1-screen, or 4-screen mirroring
**  - NES 2.0 submapper-specific bus conflicts and write ranges
**  - Instant flash programming/erase emulation for battery-backed variants
*/

#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "noftypes.h"
#include "nes_mmc.h"
#include "nes_ppu.h"
#include "nes_rom.h"
#include "osd.h"
#include "log.h"

#define MAP30_FLASH_EXT ".fls"
#define MAP30_FLASH_MANUFACTURER_ID 0xBF
#define MAP30_FLASH_DEVICE_ID 0xB7

typedef enum
{
   MAP30_NT_FIXED_HV = 0,
   MAP30_NT_SWITCH_1SCREEN,
   MAP30_NT_4SCREEN,
   MAP30_NT_SWITCH_HV
} map30_nt_mode_t;

typedef enum
{
   MAP30_FLASH_IDLE = 0,
   MAP30_FLASH_CMD_1,
   MAP30_FLASH_CMD_2,
   MAP30_FLASH_PROGRAM,
   MAP30_FLASH_ERASE_1,
   MAP30_FLASH_ERASE_2,
   MAP30_FLASH_ERASE_3,
   MAP30_FLASH_SOFT_ID
} map30_flash_state_t;

static int prg_bank;
static int chr_bank;
static int last_bank;
static uint8 reg_latch;
static uint8 led_latch;
static map30_nt_mode_t nt_mode;
static bool bus_conflicts;
static bool high_write_only;
static bool flash_enabled;
static map30_flash_state_t flash_state;
static bool flash_dirty;
static uint8 *flash_prg;
static int flash_prg_size;
static rominfo_t *flash_owner;
static uint8 *flash_rom_base;

static bool map30_flash_ensure_storage(void);

static bool map30_uses_flash_save(const rominfo_t *cart)
{
   int submapper;

   if ((NULL == cart) || (0 == (cart->flags & ROM_FLAG_BATTERY)))
      return false;

   submapper = cart->submapper_number;
   if (submapper < 0 || submapper > 4)
      submapper = 0;

   return 0 == submapper || 1 == submapper || 4 == submapper;
}

static void map30_apply_4screen(void)
{
   rominfo_t *cart = mmc_getinfo();
   uint8 *base;

   if ((NULL == cart) || (NULL == cart->vram) || (cart->vram_banks <= 0))
   {
      ppu_mirror(0, 1, 2, 3);
      return;
   }

   base = cart->vram + ((cart->vram_banks - 1) << 13);
   ppu_setnametable_3000_mirror(false);
   ppu_setpage(8, 8, base - 0x2000);
}

static void map30_sync_prg(void)
{
   mmc_bankrom(16, 0x8000, prg_bank);
   mmc_bankrom(16, 0xC000, last_bank);
}

static void map30_sync_chr(void)
{
   rominfo_t *cart = mmc_getinfo();
   int banks;
   uint8 *base;

   if ((NULL == cart) || (NULL == cart->vram))
      return;

   banks = (cart->vram_banks > 0) ? cart->vram_banks : 1;
   base = cart->vram + ((chr_bank % banks) << 13);
   ppu_setpage(8, 0, base);
}

static void map30_apply_mirroring(uint8 value)
{
   rominfo_t *cart = mmc_getinfo();

   if (NULL == cart)
      return;

   /* NESdev's mapper 30 H/V "arrangement" naming is the inverse of this
   ** core's MIRROR_VERT/MIRROR_HORIZ, hence the swapped ppu_mirror calls.
   */
   switch (nt_mode)
   {
   case MAP30_NT_FIXED_HV:
      if (MIRROR_VERT == cart->mirror)
         ppu_mirror(0, 0, 1, 1);
      else
         ppu_mirror(0, 1, 0, 1);
      break;

   case MAP30_NT_SWITCH_1SCREEN:
   {
      int mirror = (value & 0x80) ? 1 : 0;
      ppu_mirror(mirror, mirror, mirror, mirror);
      break;
   }

   case MAP30_NT_4SCREEN:
      map30_apply_4screen();
      break;

   case MAP30_NT_SWITCH_HV:
      if (value & 0x80)
         ppu_mirror(0, 0, 1, 1);
      else
         ppu_mirror(0, 1, 0, 1);
      break;
   }
}

static void map30_apply_latch(uint8 value)
{
   rominfo_t *cart = mmc_getinfo();

   reg_latch = value;

   if ((NULL != cart) && (cart->rom_banks > 0))
      prg_bank = (value & 0x1F) % cart->rom_banks;
   else
      prg_bank = 0;

   if ((NULL != cart) && (cart->vram_banks > 0))
      chr_bank = ((value >> 5) & 0x03) % cart->vram_banks;
   else
      chr_bank = 0;

   map30_sync_prg();
   map30_sync_chr();
   map30_apply_mirroring(value);
}

static uint8 map30_effective_value(uint32 address, uint8 value)
{
   rominfo_t *cart = mmc_getinfo();
   int bank;
   uint32 offset;

   if ((NULL == cart) || (NULL == cart->rom) || (cart->rom_banks <= 0))
      return value;

   bank = (address < 0xC000) ? prg_bank : last_bank;
   offset = ((uint32) bank << 14) | (address & 0x3FFF);

   return value & cart->rom[offset % ((uint32) cart->rom_banks << 14)];
}

static bool map30_flash_read_image(FILE *fp, const char *fn, const char *image_name)
{
   uint8 *image;
   size_t bytes_read;

   if ((NULL == fp) || !map30_flash_ensure_storage())
      return false;

   image = malloc(flash_prg_size);
   if (NULL == image)
   {
      nofrendo_log_printf("mapper 30: could not allocate %d-byte %s buffer\n",
                          flash_prg_size, image_name);
      return false;
   }

   bytes_read = fread(image, 1, flash_prg_size, fp);
   if (flash_prg_size != (int) bytes_read)
   {
      nofrendo_log_printf("mapper 30: incomplete %s, ignoring %s\n", image_name, fn);
      free(image);
      return false;
   }

   memcpy(flash_prg, image, flash_prg_size);
   free(image);
   return true;
}

static uint8 map30_read_low_window(uint32 address)
{
   rominfo_t *cart = mmc_getinfo();
   uint32 offset;

   if ((NULL == cart) || (NULL == cart->rom) || (cart->rom_banks <= 0))
      return 0xFF;

   offset = ((uint32) prg_bank << 14) | (address & 0x3FFF);
   return cart->rom[offset % ((uint32) cart->rom_banks << 14)];
}

static bool map30_flash_ensure_storage(void)
{
   rominfo_t *cart = mmc_getinfo();

   if ((NULL != flash_prg) && (flash_owner == cart))
   {
      cart->rom = flash_prg;
      return true;
   }

   if (NULL != flash_prg)
   {
      nofrendo_log_printf("mapper 30: flash shadow still attached to another cart\n");
      return false;
   }

   if ((NULL == cart) || (NULL == cart->rom) || (cart->rom_banks <= 0))
      return false;

   flash_prg_size = cart->rom_banks << 14;
   flash_prg = malloc(flash_prg_size);
   if (NULL == flash_prg)
   {
      nofrendo_log_printf("mapper 30: could not allocate %d-byte flash shadow\n", flash_prg_size);
      return false;
   }

   flash_owner = cart;
   flash_rom_base = cart->rom;
   memcpy(flash_prg, flash_rom_base, flash_prg_size);
   cart->rom = flash_prg;
   map30_sync_prg();
   return true;
}

static void map30_flash_chip_erase(void)
{
   if (map30_flash_ensure_storage())
   {
      memset(flash_prg, 0xFF, flash_prg_size);
      flash_dirty = true;
      map30_sync_prg();
   }
}

static void map30_flash_sector_erase(uint32 offset)
{
   uint32 sector;

   if (!map30_flash_ensure_storage())
      return;

   sector = offset & ~0x0FFFU;
   if (sector >= (uint32) flash_prg_size)
      return;

   memset(flash_prg + sector, 0xFF,
          ((sector + 0x1000U) <= (uint32) flash_prg_size) ? 0x1000U : ((uint32) flash_prg_size - sector));
   flash_dirty = true;
}

static void map30_flash_program(uint32 offset, uint8 value)
{
   if (!map30_flash_ensure_storage())
      return;

   if (offset >= (uint32) flash_prg_size)
      return;

   /* Flash programming can only clear bits until the sector is erased. */
   flash_prg[offset] &= value;
   flash_dirty = true;
   map30_sync_prg();
}

static uint32 map30_flash_offset(uint32 address)
{
   return ((uint32) prg_bank << 14) | (address & 0x3FFF);
}

static void map30_flash_write(uint32 address, uint8 value)
{
   uint32 offset = map30_flash_offset(address);

   switch (flash_state)
   {
   case MAP30_FLASH_IDLE:
      if (0x5555 == (offset & 0x7FFF) && 0xAA == value)
         flash_state = MAP30_FLASH_CMD_1;
      break;

   case MAP30_FLASH_CMD_1:
      if (0x2AAA == (offset & 0x7FFF) && 0x55 == value)
         flash_state = MAP30_FLASH_CMD_2;
      else
         flash_state = MAP30_FLASH_IDLE;
      break;

   case MAP30_FLASH_CMD_2:
      if (0x5555 == (offset & 0x7FFF))
      {
         if (0xA0 == value)
            flash_state = MAP30_FLASH_PROGRAM;
         else if (0x80 == value)
            flash_state = MAP30_FLASH_ERASE_1;
         else if (0x90 == value)
            flash_state = MAP30_FLASH_SOFT_ID;
         else
            flash_state = MAP30_FLASH_IDLE;
      }
      else
      {
         flash_state = MAP30_FLASH_IDLE;
      }
      break;

   case MAP30_FLASH_PROGRAM:
      map30_flash_program(offset, value);
      flash_state = MAP30_FLASH_IDLE;
      break;

   case MAP30_FLASH_ERASE_1:
      if (0x5555 == (offset & 0x7FFF) && 0xAA == value)
         flash_state = MAP30_FLASH_ERASE_2;
      else
         flash_state = MAP30_FLASH_IDLE;
      break;

   case MAP30_FLASH_ERASE_2:
      if (0x2AAA == (offset & 0x7FFF) && 0x55 == value)
         flash_state = MAP30_FLASH_ERASE_3;
      else
         flash_state = MAP30_FLASH_IDLE;
      break;

   case MAP30_FLASH_ERASE_3:
      if (0x30 == value)
         map30_flash_sector_erase(offset);
      else if (0x5555 == (offset & 0x7FFF) && 0x10 == value)
         map30_flash_chip_erase();
      flash_state = MAP30_FLASH_IDLE;
      break;

   case MAP30_FLASH_SOFT_ID:
      if (0xF0 == value)
         flash_state = MAP30_FLASH_IDLE;
      else if (0x5555 == (offset & 0x7FFF) && 0xAA == value)
         flash_state = MAP30_FLASH_CMD_1;
      else
         flash_state = MAP30_FLASH_IDLE;
      break;
   }
}

static void map30_load_flash_image(void)
{
   FILE *fp;
   char fn[PATH_MAX + 1];
   rominfo_t *cart = mmc_getinfo();
   bool need_disk_load;

   if (!flash_enabled || (NULL == cart))
      return;

   need_disk_load = (NULL == flash_prg) || (flash_owner != cart);

   if (!map30_flash_ensure_storage())
      return;

   if (!need_disk_load)
   {
      map30_sync_prg();
      return;
   }

   strncpy(fn, cart->filename, PATH_MAX);
   osd_newextension(fn, MAP30_FLASH_EXT);

   fp = fopen(fn, "rb");
   if (NULL == fp)
      return;

   if (!map30_flash_read_image(fp, fn, "flash image"))
   {
      fclose(fp);
      return;
   }

   fclose(fp);
   flash_dirty = false;
   map30_sync_prg();
}

static void map30_reset_runtime_state(void)
{
   prg_bank = 0;
   chr_bank = 0;
   last_bank = 0;
   reg_latch = 0;
   led_latch = 0xFF;
   nt_mode = MAP30_NT_FIXED_HV;
   bus_conflicts = false;
   high_write_only = false;
   flash_enabled = false;
   flash_state = MAP30_FLASH_IDLE;
}

static void map30_init(void)
{
   rominfo_t *cart = mmc_getinfo();
   int submapper;

   map30_reset_runtime_state();

   if (NULL == cart)
      return;

   last_bank = (cart->rom_banks > 0) ? (cart->rom_banks - 1) : 0;
   submapper = cart->submapper_number;
   if (submapper < 0 || submapper > 4)
      submapper = 0;

   flash_enabled = map30_uses_flash_save(cart);
   bus_conflicts = (2 == submapper) || ((0 == submapper) && !flash_enabled);
   high_write_only = !bus_conflicts;

   if (3 == submapper)
   {
      nt_mode = MAP30_NT_SWITCH_HV;
   }
   else if (cart->flags & ROM_FLAG_FOURSCREEN)
   {
      if (MIRROR_VERT == cart->mirror)
         nt_mode = MAP30_NT_4SCREEN;
      else
         nt_mode = MAP30_NT_SWITCH_1SCREEN;
   }
   else
   {
      nt_mode = MAP30_NT_FIXED_HV;
   }

   if (flash_enabled)
      map30_load_flash_image();

   map30_apply_latch(0);
}

static void map30_write(uint32 address, uint8 value)
{
   rominfo_t *cart = mmc_getinfo();
   uint8 effective = value;

   if ((address < 0x8000) || (NULL == cart))
      return;

   if (address < 0xC000 && 4 == cart->submapper_number)
      led_latch = value;

   if (flash_enabled && address < 0xC000)
   {
      map30_flash_write(address, value);
      return;
   }

   if (high_write_only && address < 0xC000)
      return;

   if (bus_conflicts)
      effective = map30_effective_value(address, value);

   map30_apply_latch(effective);
}

static uint8 map30_read(uint32 address)
{
   if (MAP30_FLASH_SOFT_ID == flash_state)
   {
      switch (address & 0x3FFF)
      {
      case 0x0000:
         return MAP30_FLASH_MANUFACTURER_ID;

      case 0x0001:
         return MAP30_FLASH_DEVICE_ID;

      default:
         return 0xFF;
      }
   }

   return map30_read_low_window(address);
}

void map30_rom_cleanup(rominfo_t *rominfo)
{
   FILE *fp;
   char fn[PATH_MAX + 1];

   if ((NULL == rominfo) || (flash_owner != rominfo))
      return;

   if ((NULL != flash_prg) && flash_dirty)
   {
      strncpy(fn, rominfo->filename, PATH_MAX);
      osd_newextension(fn, MAP30_FLASH_EXT);

      fp = fopen(fn, "wb");
      if (NULL == fp)
      {
         nofrendo_log_printf("mapper 30: could not save flash image to %s\n", fn);
      }
      else
      {
         fwrite(flash_prg, 1, flash_prg_size, fp);
         fclose(fp);
         flash_dirty = false;
      }
   }

   rominfo->rom = flash_rom_base;
   free(flash_prg);
   flash_prg = NULL;
   flash_prg_size = 0;
   flash_owner = NULL;
   flash_rom_base = NULL;
   flash_dirty = false;
   flash_state = MAP30_FLASH_IDLE;
}

static map_memread map30_memread[] =
{
   { 0x8000, 0xBFFF, map30_read },
   {     -1,     -1, NULL }
};

static map_memwrite map30_memwrite[] =
{
   { 0x8000, 0xFFFF, map30_write },
   {     -1,     -1, NULL }
};

mapintf_t map30_intf =
{
   30,
   "UNROM 512",
   map30_init,
   NULL,
   NULL,
   map30_memread,
   map30_memwrite,
   NULL
};
