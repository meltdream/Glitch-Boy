/*
** Mapper 69: Sunsoft FME-7 / Sunsoft 5A/5B (no expansion audio)
** Minimal: PRG/CHR banking, mirroring, simple IRQ (scanline-approx).
*/

#include "noftypes.h"
#include "nes_mmc.h"
#include "nes_ppu.h"
#include "nes.h"
#include "wram.h"

/* IRQ ack from platform shim */
extern void nes_irq_ack(void);

static uint8  cmd;
static uint8  prg[3];        /* 8 KiB windows $8000/$A000/$C000 */
static uint8  chr_bank[8];   /* 1 KiB windows */

/* IRQ state (approximate: decremented each scanline by ~114 CPU cycles) */
static uint16 irq_counter;
static bool   irq_cnt_en;    /* counter enable */
static bool   irq_en;        /* IRQ enable */

static void fme7_sync_prg(void)
{
    mmc_bankrom(8, 0x8000, prg[0]);
    mmc_bankrom(8, 0xA000, prg[1]);
    mmc_bankrom(8, 0xC000, prg[2]);
    mmc_bankrom(8, 0xE000, (uint8)(mmc_getinfo()->rom_banks * 2 - 1));
}

static void fme7_sync_chr(void)
{
    if (mmc_getinfo()->vrom_banks > 0)
    {
        for (int i = 0; i < 8; i++)
            mmc_bankvrom(1, i << 10, chr_bank[i]);
    }
}

static void map69_init(void)
{
    cmd = 0;
    for (int i = 0; i < 8; i++) chr_bank[i] = 0;
    prg[0] = 0;
    prg[1] = 1;
    prg[2] = 2;
    fme7_sync_prg();
    fme7_sync_chr();
    irq_counter = 0;
    irq_cnt_en = false;
    irq_en = false;

    /* $6000 window controlled by command $08; leave unmapped until configured. */
}

static void map69_hblank(int vblank)
{
    UNUSED(vblank);
    if (irq_cnt_en && irq_en)
    {
        /* Approximate CPU-cycle decrement: ~114 cycles per scanline */
        if (irq_counter == 0)
        {
            /* generate and hold until ack */
            nes_irq();
        }
        else
        {
            if (irq_counter > 114)
                irq_counter -= 114;
            else
                irq_counter = 0;
        }
    }
}

static void map69_write(uint32 address, uint8 value)
{
    switch (address & 0xE000)
    {
        case 0x8000: /* Command register */
        case 0x9000:
            cmd = value & 0x0F;
            break;

        case 0xA000: /* Parameter register */
        case 0xB000:
        {
            switch (cmd)
            {
                /* CHR 1 KiB banks 0..7 */
                case 0x00: case 0x01: case 0x02: case 0x03:
                case 0x04: case 0x05: case 0x06: case 0x07:
                    chr_bank[cmd & 7] = value;
                    fme7_sync_chr();
                    break;

                /* PRG banking and $6000 window control */
                case 0x08:
                {
                    /* ERbb bbbb: bit7 RAM enable, bit6 RAM/ROM select (1=RAM), bits0-5 bank for $6000 if ROM */
                    bool ram_en = (value & 0x80) != 0;
                    bool ram_sel = (value & 0x40) != 0; /* 1 = PRG-RAM at $6000 */
                    uint8 bank = (uint8)(value & 0x3F);
                    nes_set_wram_enable(ram_en);
                    if (ram_sel)
                    {
                        mmc_bankwram(8, 0x6000, 0);
                    }
                    else
                    {
                        mmc_bankrom(8, 0x6000, bank);
                    }
                    break;
                }
                case 0x09: prg[0] = (value & 0x3F); fme7_sync_prg(); break; /* $8000 */
                case 0x0A: prg[1] = (value & 0x3F); fme7_sync_prg(); break; /* $A000 */
                case 0x0B: prg[2] = (value & 0x3F); fme7_sync_prg(); break; /* $C000 */

                case 0x0C: /* Name table mirroring */
                    switch (value & 0x03)
                    {
                        case 0: ppu_mirror(0,1,0,1); break;            /* vertical */
                        case 1: ppu_mirror(0,0,1,1); break;            /* horizontal */
                        case 2: ppu_mirror(0,0,0,0); break;            /* 1-screen A ($2000) */
                        case 3: ppu_mirror(1,1,1,1); break;            /* 1-screen B ($2400) */
                    }
                    break;

                case 0x0D: /* IRQ Control: acknowledge, set enables */
                    nes_irq_ack();
                    irq_cnt_en = (value & 0x80) != 0;  /* bit7: counter enable */
                    irq_en     = (value & 0x01) != 0;  /* bit0: IRQ enable */
                    break;

                case 0x0E: /* IRQ counter low */
                    irq_counter = (irq_counter & 0xFF00) | value;
                    break;

                case 0x0F: /* IRQ counter high */
                    irq_counter = (irq_counter & 0x00FF) | ((uint16)value << 8);
                    break;

                default:
                    break;
            }
            break;
        }

        default:
            break;
    }
}

static map_memwrite map69_memwrite[] =
{
    { 0x8000, 0xFFFF, map69_write },
    {     -1,     -1, NULL }
};

mapintf_t map69_intf =
{
    69, /* mapper number */
    "Sunsoft FME-7", /* mapper name */
    map69_init, /* init routine */
    NULL, /* vblank callback */
    map69_hblank, /* hblank callback */
    NULL, /* memory read structure */
    map69_memwrite, /* memory write structure */
    NULL /* external sound device */
};
