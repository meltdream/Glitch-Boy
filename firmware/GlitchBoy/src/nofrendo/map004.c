#include "noftypes.h"
#include "nes_mmc.h"
#include "nes.h"
#include "nes_rom.h"
#include "nes_ppu.h"
#include <string.h>
#include "nes6502.h"
#ifdef ESP_PLATFORM
#include <esp_attr.h>
#define MAP4_HOT_ATTR IRAM_ATTR
#define MAP4_HOT_INLINE static inline MAP4_HOT_ATTR
#else
#define MAP4_HOT_ATTR
#define MAP4_HOT_INLINE static inline
#endif

/* Configuration */
#define MAP4_PPU_EDGE_IRQ  1   /* Use A12 edges instead of HBlank ticks. */

/* State */
typedef struct {
    uint8 counter, latch;
    bool  enabled;
    bool  reload_flag;
} irq_t;

static irq_t  irq;
static uint8  reg8000;          /* Last $8000 value.              */
static uint16 vrombase;         /* 0x0000 or 0x1000               */
static uint8  prg_bank6;        /* Last R6 value.                 */
static uint8  r7_prg_bank;      /* Last R7 value ($A000 bank).    */
static bool   fourscreen;
static bool   wram_en, wram_wp;
static uint8 chr_reg[6];

static uint8  prev_a12 = 0;
static int16  a12_generated_scanline = -1;

#define FIXED_LAST(c)   ((c)->rom_banks*2-1)
#define FIXED_PENULT(c) ((c)->rom_banks*2-2)

/* Set to 1 to emulate the older NEC behavior where IRQs only fire on a
   decrement to zero.  Default (0) matches the newer Sharp behavior where any
   clock resulting in counter==0 asserts the IRQ. */
#ifndef MMC3_ALT_IRQ_BEHAVIOR
#define MMC3_ALT_IRQ_BEHAVIOR 0
#endif

MAP4_HOT_INLINE void map4_clock_irq(void)
{
    uint8 prev_counter = irq.counter;

    if (irq.reload_flag || irq.counter == 0)
    {
        irq.counter     = irq.latch;
        irq.reload_flag = false;
    }
    else
    {
        irq.counter--;
    }

#if MMC3_ALT_IRQ_BEHAVIOR
    if (irq.enabled && prev_counter == 1 && irq.counter == 0)
#else
    if (irq.enabled && irq.counter == 0)
#endif
    {
        nes_irq();
    }
}


/* PPU bus hooks */
#if MAP4_PPU_EDGE_IRQ
/* Only CHR accesses matter for MMC3 IRQ timing. */
void MAP4_HOT_ATTR map4_ppu_tick(uint16 addr)
{
    uint8 curr_a12 = (addr & 0x1000) ? 1 : 0;   /* Current A12 level. */

    if (!curr_a12) {
        prev_a12 = 0;
        return;
    }

    if (prev_a12)
        return;

    prev_a12 = 1;

    int sc = nes_getcontextptr()->scanline;

    /* Allow one real MMC3 clock per scanline. */
    if ((unsigned)sc < 262u) {
        if (a12_generated_scanline != sc) {
            map4_clock_irq();
            a12_generated_scanline = (int16)sc;
        }
    }
}

/* Nametable accesses provide the A12-low period. */
void MAP4_HOT_ATTR map4_ppu_nt_tick(uint16 addr)
{
    (void)addr;
    prev_a12 = 0;
}
#endif /* MAP4_PPU_EDGE_IRQ */


/* CPU writes */
static void map4_write(uint32 a, uint8 v)
{
    switch (a & 0xE001)
    {
    /* $8000: bank select */
    case 0x8000: {
        uint8 old_d7   = reg8000 & 0x80;
        uint8 old_mode = reg8000 & 0x40;      /* D6: PRG mode. */

        reg8000  = v;
        vrombase = (v & 0x80) ? 0x1000 : 0x0000;

        /* Refresh CHR windows if D7 changed. */
        if (old_d7 != (v & 0x80)) {
            /* R0 and R1 are even 2 KiB windows. */
            uint8 r0 = chr_reg[0] & 0xFE;
            uint8 r1 = chr_reg[1] & 0xFE;
            mmc_bankvrom(1, vrombase ^ 0x0000, r0);
            mmc_bankvrom(1, vrombase ^ 0x0400, r0 + 1);
            mmc_bankvrom(1, vrombase ^ 0x0800, r1);
            mmc_bankvrom(1, vrombase ^ 0x0C00, r1 + 1);

            /* R2 through R5 are 1 KiB windows. */
            mmc_bankvrom(1, vrombase ^ 0x1000, chr_reg[2]);
            mmc_bankvrom(1, vrombase ^ 0x1400, chr_reg[3]);
            mmc_bankvrom(1, vrombase ^ 0x1800, chr_reg[4]);
            mmc_bankvrom(1, vrombase ^ 0x1C00, chr_reg[5]);
        }

        /* Keep the penultimate bank visible. */
        mmc_bankrom(8, (v & 0x40) ? 0x8000 : 0xC000,
                       FIXED_PENULT(mmc_getinfo()));

        /* Move R6 if D6 changed. */
        if (old_mode != (v & 0x40))
            mmc_bankrom(8, (v & 0x40) ? 0xC000 : 0x8000, prg_bank6);
        break;
    }

    /* $8001: bank data */
    case 0x8001: {
        switch (reg8000 & 7)
        {
        case 0: v &= 0xFE;
                chr_reg[0] = v;
                mmc_bankvrom(1, vrombase ^ 0x0000, v);
                mmc_bankvrom(1, vrombase ^ 0x0400, v+1);      break;
        case 1: v &= 0xFE;
                chr_reg[1] = v;
                mmc_bankvrom(1, vrombase ^ 0x0800, v);
                mmc_bankvrom(1, vrombase ^ 0x0C00, v+1);      break;
        case 2: chr_reg[2] = v; mmc_bankvrom(1, vrombase ^ 0x1000, v); break;
        case 3: chr_reg[3] = v; mmc_bankvrom(1, vrombase ^ 0x1400, v); break;
        case 4: chr_reg[4] = v; mmc_bankvrom(1, vrombase ^ 0x1800, v); break;
        case 5: chr_reg[5] = v; mmc_bankvrom(1, vrombase ^ 0x1C00, v); break;
        case 6: prg_bank6 = v;
                mmc_bankrom(8, (reg8000 & 0x40) ? 0xC000 : 0x8000, prg_bank6);
                break;
        case 7: r7_prg_bank = v; mmc_bankrom(8, 0xA000, v);  break;
        }
        break;
    }

    /* $A000: nametable mirroring */
    case 0xA000:
        if (!fourscreen)
            (v & 1) ? ppu_mirror(0,0,1,1) : ppu_mirror(0,1,0,1);
        break;

    /* $A001: WRAM enable and protect */
    case 0xA001:
        wram_en = !!(v & 0x80);
        wram_wp = !!(v & 0x40);
        nes_set_wram_enable(wram_en);
        nes_set_wram_write_protect(wram_wp);
        mmc_bankwram(8, 0x6000, v & 3);
        break;

    /* $C000: IRQ latch */
    case 0xC000:
        irq.latch = v;
        break;

    /* $C001: IRQ reload */
    case 0xC001:
        irq.reload_flag = true;
        break;

    /* $E000: IRQ disable and acknowledge */
    case 0xE000:
        irq.enabled = false;
        nes_irq_ack();
        break;

    /* $E001: IRQ enable */
    case 0xE001:
        irq.enabled = true;
        break;
    }
}

/* HBlank fallback */
static void map4_hblank(int vblank)
{
#if !MAP4_PPU_EDGE_IRQ
    if (!vblank)
        map4_clock_irq();
#else
    nes_t *nes = nes_getcontextptr();
    int scanline = nes->scanline;

    if (!vblank && (unsigned)scanline < 240u) {
        /* Force A12 low once per visible scanline. */
        prev_a12 = 0;

        /* Synthesize an edge when a rendered line missed one. */
        if (a12_generated_scanline != scanline) {
            int bg_is_1000  = (ppu_bg_base()  == 0x1000);
            int obj_is_1000 = (ppu_obj_base() == 0x1000);
            if (ppu_enabled() && (bg_is_1000 || obj_is_1000)) {
                map4_clock_irq();
                a12_generated_scanline = (int16)scanline;
            }
        }
    }

    /* Reset tracking at the end of the frame. */
    if (vblank && scanline == 261) {
        a12_generated_scanline = -1;
    }
#endif
}

/* Save-state helpers */

/* Power-on reset */
static void map4_init(void)
{
    rominfo_t *cart = mmc_getinfo();
    memset(&irq,    0, sizeof irq);
    memset(chr_reg, 0, sizeof chr_reg);
    a12_generated_scanline = -1;
#if MAP4_PPU_EDGE_IRQ
    prev_a12 = 0;
#endif

    reg8000    = 0;  vrombase = 0;  prg_bank6 = 0;  r7_prg_bank = 0;
    fourscreen = !!(cart->flags & ROM_FLAG_FOURSCREEN);
    wram_en    = false; wram_wp = false;

    /* PRG layout. */
    mmc_bankrom(8, 0xC000, FIXED_PENULT(cart));
    mmc_bankrom(8, 0xE000, FIXED_LAST(cart));
    mmc_bankrom(8, 0x8000, prg_bank6);
    mmc_bankrom(8, 0xA000, 0);

    /* CHR layout. */
    mmc_bankvrom(8, 0x0000, 0);

#if MAP4_PPU_EDGE_IRQ
    ppu_set_mapper_hook(map4_ppu_tick);
    ppu_set_nt_hook(map4_ppu_nt_tick);
    ppu_set_mapper_sprite_fetch_coalescing(true);
#endif
}

/* Mapper interface */
static map_memwrite map4_memwrite[] = {
    { 0x8000, 0xFFFF, map4_write },
    {    -1,     -1, NULL }
};

mapintf_t map4_intf = {
    4, "MMC3",
    map4_init, NULL,
    map4_hblank,
    NULL, map4_memwrite, NULL
};
