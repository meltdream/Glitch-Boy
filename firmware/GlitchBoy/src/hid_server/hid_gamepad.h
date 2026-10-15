#ifndef HID_GAMEPAD_H
#define HID_GAMEPAD_H

#include <stdint.h>
#include <vector>

class HIDGamepad {
public:
    enum ParseHint {
        PARSE_HINT_NONE = 0,
        PARSE_HINT_STRONG_XBOX_BLE = 1 << 0,
    };

    enum DecodeQuirk {
        DECODE_QUIRK_NONE = 0,
        DECODE_QUIRK_XBOX_BLE = 1 << 0,
        // Android/Linux gamepad order: A B C X Y Z L R L2 R2 Select Start Home
        DECODE_QUIRK_ANDROID_LAYOUT = 1 << 1,
    };

    struct DecodedState {
        uint64_t raw_buttons;
        uint16_t buttons;
    };

    HIDGamepad();
    bool parse_descriptor(const uint8_t* data, int len, uint32_t parse_hints = PARSE_HINT_NONE);
    bool decode_report_state(const uint8_t* data, int len, DecodedState* out_state, uint32_t decode_quirks = DECODE_QUIRK_NONE) const;
    bool has_report_id() const;

private:
    struct Field {
        uint16_t usage_page;
        uint16_t usage;
        uint16_t usage_min;
        uint16_t usage_max;
        uint8_t report_id;
        uint16_t bit_offset;
        uint8_t bit_size;
        int32_t logical_min;
        int32_t logical_max;
        bool is_signed;
        bool is_array;
    };

    std::vector<Field> _fields;
    bool _is_gamepad;
    bool _has_report_id;
    uint16_t _max_button_usage;
};

#endif // HID_GAMEPAD_H
