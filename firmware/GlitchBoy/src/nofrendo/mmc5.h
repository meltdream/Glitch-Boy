/* Minimal MMC5 helper interface for PPU overlay */
#ifndef NOFRENDO_MMC5_HELPERS_H
#define NOFRENDO_MMC5_HELPERS_H

#include "noftypes.h"

/* Returns true if MMC5 extended graphics (ExGFX) overlay is enabled */
bool mmc5_exgfx_enabled(void);

/* Returns pointer to MMC5 ExRAM window ($5C00-$5FFF), size 1KB */
uint8 *mmc5_get_exram(void);

/* Returns upper 2 CHR bank bits from $5130 (0..3) */
uint8 mmc5_get_chr_hi2(void);

/* Vertical split helpers */
bool  mmc5_split_enabled(void);
uint8 mmc5_split_ctrl(void);     /* bit7 enable, bit6 right/left, low5 threshold */
uint8 mmc5_split_scroll(void);   /* $5201 */

#endif
