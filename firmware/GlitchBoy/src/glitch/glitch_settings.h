#ifndef GLITCH_SETTINGS_H
#define GLITCH_SETTINGS_H

#ifndef __cplusplus
#include "../nofrendo/noftypes.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

void glitch_settings_set_apu_enabled(bool enabled);
bool glitch_settings_is_apu_enabled(void);

#ifdef __cplusplus
}
#endif

#endif /* GLITCH_SETTINGS_H */
