/* MMC3/MMC6 work RAM pager. */
#ifndef WRAM_H
#define WRAM_H

#include <stdint.h>

/* Declared in nes.h. */
struct nes_s;

/* Initialize after loading cartridge SRAM. */
void wram_init(struct nes_s *nes);

/* Select the MMC3/MMC6 work RAM bank. */
void mmc_bankwram(int size, uint32_t addr, uint8_t bank);

#endif /* WRAM_H */
