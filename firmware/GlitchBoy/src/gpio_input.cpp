#include "gpio_input.h"
#include "emu.h"
#include "generic_pad.h"
#include "midi_input.h"

#ifdef ESP_PLATFORM
#include "driver/gpio.h"

static gpio_button_state _gpio_buttons = {0};

void gpio_input_init() {
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << (gpio_num_t)GPIO_BTN_A) | (1ULL << (gpio_num_t)GPIO_BTN_B) |
                           (1ULL << (gpio_num_t)GPIO_BTN_SELECT) | (1ULL << (gpio_num_t)GPIO_BTN_START) |
                           (1ULL << (gpio_num_t)GPIO_BTN_UP) | (1ULL << (gpio_num_t)GPIO_BTN_DOWN) |
                           (1ULL << (gpio_num_t)GPIO_BTN_LEFT) | (1ULL << (gpio_num_t)GPIO_BTN_RIGHT);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);
}

int get_hid_gpio(uint8_t* dst) {
    uint16_t buttons = 0;
    if (!gpio_get_level((gpio_num_t)GPIO_BTN_A))      buttons |= GENERIC_FIRE_A;
    if (!gpio_get_level((gpio_num_t)GPIO_BTN_B))      buttons |= GENERIC_FIRE_B;
    if (!gpio_get_level((gpio_num_t)GPIO_BTN_SELECT)) buttons |= GENERIC_SELECT;
    if (!gpio_get_level((gpio_num_t)GPIO_BTN_START))  buttons |= GENERIC_START;
    if (!gpio_get_level((gpio_num_t)GPIO_BTN_UP))     buttons |= GENERIC_UP;
    if (!gpio_get_level((gpio_num_t)GPIO_BTN_DOWN))   buttons |= GENERIC_DOWN;
    if (!gpio_get_level((gpio_num_t)GPIO_BTN_LEFT))   buttons |= GENERIC_LEFT;
    if (!gpio_get_level((gpio_num_t)GPIO_BTN_RIGHT))  buttons |= GENERIC_RIGHT;

    if (buttons != _gpio_buttons.current_state) {
        _gpio_buttons.debounce_timer = 3;
        _gpio_buttons.current_state = buttons;
    }

    if (_gpio_buttons.debounce_timer > 0) {
        _gpio_buttons.debounce_timer--;
        return 0;
    }

    if (buttons != _gpio_buttons.last_state) {
        _gpio_buttons.last_state = buttons;
        generic_pad_set_player1_overlay(buttons);
        return generic_pad_build_report(dst, 6);
    }
    return 0;
}
#else

void gpio_input_init() {}

int get_hid_gpio(uint8_t* dst) {
    (void)dst;
    return 0;
}

#endif // ESP_PLATFORM

int get_hid_all(uint8_t* dst) {
    int n = 0;
    if ((n = get_hid_gpio(dst)))
        return n;
    if ((n = get_hid_midi(dst)))
        return n;
    if ((n = get_hid_ir(dst)))
        return n;
    if ((n = get_hid_ble(dst)))
        return n;
    return 0;
}
