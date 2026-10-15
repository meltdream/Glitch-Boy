/* Switchable MMC3/MMC6 work RAM window at $6000-$7FFF. */

#include <stdint.h>
#include <stddef.h>
#include "nes.h"
#include "nes6502.h"
#include "wram.h"

/* Module state */
static uint8_t *base     = NULL;   /* full SRAM blob supplied by cart       */
static int      banks    = 1;      /* number of 8 KiB pages                 */
static int      cur_bank = 0;      /* currently mapped page index           */
static bool     wram_en  = false;  /* enable bit, false after reset         */
static bool     wram_wp  = false;  /* write-protect bit                     */

#define NES           (nes_getcontextptr())
#define WINDOW_START  0x6000
#define WINDOW_END    0x7FFF
#define PAGE_SIZE     0x2000      /* 8 KiB */

/* Local open-bus page. */
static uint8_t dead_page[0x1000] = { [0 ... 0x0FFF] = 0xFF };

/* Idle when another core runs this boot (see nes_ppu_idle_scratch). */
uint8_t *nes_wram_idle_scratch(size_t *len)
{
    if (len)
        *len = sizeof(dead_page);
    return dead_page;
}

/* Internal helpers */
static void remap_page(void)
{
    nes_t *ctx = nes_getcontextptr();
    if (!ctx)                            /* Core is not initialized yet. */
        return;

    if (wram_en && base)
    {
        uint8_t *page0 = base + cur_bank * PAGE_SIZE;
        ctx->cpu->mem_page[6] = page0;
        ctx->cpu->mem_page[7] = page0 + 0x1000;
    }
    else
    {
        ctx->cpu->mem_page[6] = dead_page;
        ctx->cpu->mem_page[7] = dead_page;
    }
}

/* $6000-$7FFF write gate */
static void wram_write(uint32_t addr, uint8_t val)
{
    if (!wram_en || !base)
        return;
    if (wram_wp)
        return;

    /* Inline bank_writebyte(). */
    NES->cpu->mem_page[addr >> NES6502_BANKSHIFT]
                     [addr &  NES6502_BANKMASK] = val;
}

/* Add the write gate once. */
static void add_write_handler(nes_t *nes)
{
    /* Skip an existing gate. */
    for (int j = 0; nes->writehandler[j].write_func; ++j)
        if (nes->writehandler[j].min_range  == WINDOW_START &&
            nes->writehandler[j].max_range  == WINDOW_END   &&
            nes->writehandler[j].write_func == wram_write)
            return;                       /* Already present. */

    int i = 0;
    while (nes->writehandler[i].write_func && i < MAX_MEM_HANDLERS - 1)
        ++i;

    nes->writehandler[i].min_range  = WINDOW_START;
    nes->writehandler[i].max_range  = WINDOW_END;
    nes->writehandler[i].write_func = wram_write;

    nes->writehandler[i + 1].min_range  = 0xFFFFFFFF;   /* sentinel */
    nes->writehandler[i + 1].write_func = NULL;
}

/* MMC3 API */
/* Called for writes to $A000/$A001. */
void mmc_bankwram(int size_kib, uint32_t addr, uint8_t bank)
{
    /* Accept 8 KiB and 4 KiB requests. */
    if (addr != WINDOW_START || (size_kib != 8 && size_kib != 4) || banks == 0)
        return;

    /* 4 KiB requests still select an 8 KiB-aligned bank. */
    cur_bank = bank % banks;
    remap_page();
}

void nes_set_wram_enable(bool enable)
{
    wram_en = enable;
    remap_page();
}

/* MMC3 uses active-high write protect. MMC6 callers invert its active-low bit. */
void nes_set_wram_write_protect(bool protect)
{
    wram_wp = protect;
}

/* Cartridge boot */
void wram_init(struct nes_s *nes)
{
    if (!nes->rominfo->sram)
        return;

    base     = nes->rominfo->sram;
    banks    = nes->rominfo->sram_banks ? nes->rominfo->sram_banks : 1;
    cur_bank = 0;

    /* Power-on state: disabled and writable. */
    wram_en  = false;
    wram_wp  = false;

    /* Install the write gate after SRAM is available. */
    add_write_handler(nes);

    /* Do not remap until the CPU context is ready. */
}
