#ifndef GLITCH_CONTROLS_H
#define GLITCH_CONTROLS_H

#include <stdint.h>
#include "glitch_runtime.h"

#ifndef __cplusplus
#include "../nofrendo/noftypes.h"
#endif

/* ESP32 control hardware. */
#define GLITCH_POT_PIN 32   // ADC1_CHANNEL_4
#define GLITCH_POT2_PIN 36  // ADC1_CHANNEL_0
#define GLITCH_CV_PIN 34    // ADC1_CHANNEL_6
#define GLITCH_CV2_PIN 35   // ADC1_CHANNEL_7
#define GLITCH_AUDIO_PIN 39 // ADC1_CHANNEL_3
#define GLITCH_X_PIN 15     // panel X: latch toggle / momentary kill
#define GLITCH_Y_PIN 0      // panel Y: hold/repeat mask
#ifndef AUDIO_MCP4822_OUTPUT
#define AUDIO_MCP4822_OUTPUT 1
#endif
#ifndef GLITCH_LATCH_LED_ENABLED
#define GLITCH_LATCH_LED_ENABLED 0
#endif
#define GLITCH_LATCH_LED_PIN 2  // GPIO 2 is MCP4822 SCK on the current PCB
#define GLITCH_DEADZONE 100  // ADC value threshold for "off" position
#define GLITCH_KNOB_DEADZONE ((4095u + 19u) / 20u)  // First 1/20 turn maps to off
#define GLITCH_UPDATE_INTERVAL_MS 50  // Debounce/update rate limiting
#define GLITCH_CV_COUNT 2
#define GLITCH_CV_GATE_ON_THRESHOLD 2048
#define GLITCH_CV_GATE_OFF_THRESHOLD 1536
#define GLITCH_Y_HOLD_STEP_MS 500  // Y button hold time per slot
#define GLITCH_Y_DECAY_STEP_MS 300  // Y button decay time per slot
#define GLITCH_BUTTON_DEBOUNCE_MS 30  // Digital button debounce time

typedef enum glitch_cv_assignment {
    GLITCH_CV_ASSIGN_PPU = 0,
    GLITCH_CV_ASSIGN_CORRUPTION,
    GLITCH_CV_ASSIGN_BUTTON_LEFT,
    GLITCH_CV_ASSIGN_BUTTON_RIGHT,
    GLITCH_CV_ASSIGN_BUTTON_UP,
    GLITCH_CV_ASSIGN_BUTTON_DOWN,
    GLITCH_CV_ASSIGN_BUTTON_A,
    GLITCH_CV_ASSIGN_BUTTON_B,
    GLITCH_CV_ASSIGN_BUTTON_X,
    GLITCH_CV_ASSIGN_BUTTON_Y,
    GLITCH_CV_ASSIGN_BUTTON_START,
    GLITCH_CV_ASSIGN_BUTTON_SELECT,
    GLITCH_CV_ASSIGN_PALETTE,
    GLITCH_CV_ASSIGN_COUNT
} glitch_cv_assignment_t;

#define GLITCH_CV_A 0
#define GLITCH_CV_B 1
#define GLITCH_CV_ASSIGN_DEFAULT_A GLITCH_CV_ASSIGN_PPU
#define GLITCH_CV_ASSIGN_DEFAULT_B GLITCH_CV_ASSIGN_CORRUPTION

#define GLITCH_AUDIO_ASSIGN_PPU 0
#define GLITCH_AUDIO_ASSIGN_CORRUPTION 1
#define GLITCH_AUDIO_ASSIGN_DEFAULT GLITCH_AUDIO_ASSIGN_PPU

#ifdef __cplusplus
extern "C" {
#endif

void glitch_controls_init(void);
void glitch_controls_update(void);
void glitch_controls_set_ppu_midi_value(uint8_t value);
void glitch_controls_set_corruption_level(uint8_t level);
void glitch_controls_set_corruption_midi_value(uint8_t value);
void glitch_controls_set_virtual_buttons(bool x_pressed, bool y_pressed);
void glitch_controls_set_x_button_momentary(bool momentary);
bool glitch_controls_get_x_button_momentary(void);
void glitch_controls_set_y_palette_mode(bool palette_mode);
bool glitch_controls_get_y_palette_mode(void);
void glitch_controls_set_cv_assignment(uint8_t cv, uint8_t assignment);
uint8_t glitch_controls_get_cv_assignment(uint8_t cv);
void glitch_controls_set_audio_assignment(uint8_t assignment);
uint8_t glitch_controls_get_audio_assignment(void);
uint8_t glitch_controls_get_audio_level(void);
uint16_t glitch_controls_get_cv_gate_buttons(void);

/* Raw/effective input values used by diagnostics and the visualizer. */
uint16_t glitch_controls_get_pot1_adc_raw(void);
uint16_t glitch_controls_get_pot2_adc_raw(void);
uint16_t glitch_controls_get_cv1_adc_raw(void);
uint16_t glitch_controls_get_cv2_adc_raw(void);
uint16_t glitch_controls_get_effective_ppu_adc_raw(void);
uint16_t glitch_controls_get_effective_corruption_adc_raw(void);

/* Individual masks are exposed for diagnostics and MIDI auto-mask control. */
uint16_t glitch_controls_get_primary_ppu_mask(void);
uint16_t glitch_controls_get_auto_ppu_mask(void);
uint16_t glitch_controls_get_hold_ppu_mask(void);
bool glitch_controls_get_panel_x_pressed(void);
bool glitch_controls_get_panel_y_pressed(void);
void glitch_controls_set_enabled(bool enabled);
void glitch_controls_clear_auto_mask(void);
void glitch_controls_randomize_auto_mask(void);

#ifdef __cplusplus
}
#endif

#endif /* GLITCH_CONTROLS_H */
