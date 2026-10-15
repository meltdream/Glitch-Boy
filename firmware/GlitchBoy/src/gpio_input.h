#ifndef GPIO_INPUT_H
#define GPIO_INPUT_H

#include <stdint.h>

// Button GPIOs. Also in use: 18 MCP4822 CS, 0 IR, 25 video DAC, 27 MIDI.
#define GPIO_BTN_A      19
#define GPIO_BTN_B      26
#define GPIO_BTN_SELECT 23
#define GPIO_BTN_START  21
#define GPIO_BTN_UP     4
#define GPIO_BTN_DOWN   22
#define GPIO_BTN_LEFT   17
#define GPIO_BTN_RIGHT  16

struct gpio_button_state {
    uint16_t current_state;
    uint16_t last_state;
    uint8_t debounce_timer;
};

void gpio_input_init();
int get_hid_gpio(uint8_t* dst);

#endif // GPIO_INPUT_H
