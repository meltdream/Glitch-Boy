/* MMC3 IRQ hooks used by the ESP32 integration. */

#include "noftypes.h"
#include "nes.h"
#include "nes_mmc.h"

#ifdef ESP_PLATFORM
#include <esp_attr.h>
#define MAPPER_SHIM_HOT_ATTR IRAM_ATTR
#else
#define MAPPER_SHIM_HOT_ATTR
#endif

void MAPPER_SHIM_HOT_ATTR nes_irq(void)
{
    ext_irq_line = 1;
    nes6502_irq();
}

/* Release the IRQ line immediately. */
void MAPPER_SHIM_HOT_ATTR nes_irq_ack(void)
{
    ext_irq_line   = 0;
    nes6502_clear_pending_irq();
}
