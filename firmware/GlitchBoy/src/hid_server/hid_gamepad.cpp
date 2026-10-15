#include "hid_gamepad.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "../emu.h"

enum {
    HID_ITEM_TYPE_MAIN = 0,
    HID_ITEM_TYPE_GLOBAL = 1,
    HID_ITEM_TYPE_LOCAL = 2,
};

enum {
    HID_USAGE_PAGE_GENERIC_DESKTOP = 0x01,
    HID_USAGE_PAGE_BUTTON = 0x09,
    HID_USAGE_PAGE_CONSUMER = 0x0C,
};

enum {
    HID_USAGE_GD_JOYSTICK = 0x04,
    HID_USAGE_GD_GAMEPAD = 0x05,
    HID_USAGE_GD_MULTI_AXIS = 0x08,
    HID_USAGE_GD_X = 0x30,
    HID_USAGE_GD_Y = 0x31,
    HID_USAGE_GD_Z = 0x32,
    HID_USAGE_GD_RX = 0x33,
    HID_USAGE_GD_RY = 0x34,
    HID_USAGE_GD_RZ = 0x35,
    HID_USAGE_GD_HAT = 0x39,
    HID_USAGE_GD_START = 0x3D,
    HID_USAGE_GD_SELECT = 0x3E,
    HID_USAGE_GD_DPAD_UP = 0x90,
    HID_USAGE_GD_DPAD_DOWN = 0x91,
    HID_USAGE_GD_DPAD_RIGHT = 0x92,
    HID_USAGE_GD_DPAD_LEFT = 0x93,
};

enum {
    HID_CONSUMER_AC_HOME = 0x0223,
    HID_CONSUMER_AC_BACK = 0x0224,
};

struct GlobalState {
    uint16_t usage_page = 0;
    int32_t logical_min = 0;
    int32_t logical_max = 0;
    uint8_t report_size = 0;
    uint16_t report_count = 0;  // was uint8_t: counts > 255 corrupted later offsets
    uint8_t report_id = 0;
};

struct LocalState {
    std::vector<uint16_t> usages;
    bool has_usage_min = false;
    bool has_usage_max = false;
    uint16_t usage_min = 0;
    uint16_t usage_max = 0;

    void clear() {
        usages.clear();
        has_usage_min = false;
        has_usage_max = false;
        usage_min = 0;
        usage_max = 0;
    }
};

static int32_t sign_extend(uint32_t v, int bits) {
    if (bits <= 0 || bits >= 32) {
        return (int32_t)v;
    }
    uint32_t m = 1u << (bits - 1);
    return (int32_t)((v ^ m) - m);
}

static uint32_t extract_bits(const uint8_t* data, int bit_offset, int bit_size) {
    uint32_t v = 0;
    for (int i = 0; i < bit_size; i++) {
        int byte_index = (bit_offset + i) >> 3;
        int bit_index = (bit_offset + i) & 7;
        uint32_t bit = (data[byte_index] >> bit_index) & 1u;
        v |= (bit << i);
    }
    return v;
}

HIDGamepad::HIDGamepad() : _is_gamepad(false), _has_report_id(false), _max_button_usage(0) {
}

bool HIDGamepad::parse_descriptor(const uint8_t* data, int len, uint32_t parse_hints) {
    _fields.clear();
    _is_gamepad = false;
    _has_report_id = false;
    _max_button_usage = 0;
    if (!data || len <= 0) {
        return false;
    }

    struct ParseEvidence {
        uint16_t button_count = 0;
        uint16_t max_button_usage = 0;
        bool has_hat = false;
        bool has_dpad_bits = false;
        bool have_x = false;
        bool have_y = false;
        bool has_start = false;
        bool has_select = false;
    };

    auto bump_button_count = [](ParseEvidence& evidence, uint16_t count) {
        uint32_t total = (uint32_t)evidence.button_count + count;
        evidence.button_count = total > 0xFFFFu ? 0xFFFFu : (uint16_t)total;
    };

    auto track_button_usage = [&](ParseEvidence& evidence, uint16_t usage) {
        if (usage > evidence.max_button_usage) {
            evidence.max_button_usage = usage;
        }
        if (usage != 0) {
            bump_button_count(evidence, 1);
        }
    };

    auto track_button_range = [&](ParseEvidence& evidence, uint16_t usage_min, uint16_t usage_max) {
        if (usage_max > evidence.max_button_usage) {
            evidence.max_button_usage = usage_max;
        }
        uint16_t start = usage_min < 1 ? 1 : usage_min;
        if (usage_max >= start) {
            bump_button_count(evidence, (uint16_t)(usage_max - start + 1));
        }
    };

    auto is_supported_generic_desktop_usage = [](uint16_t usage) {
        switch (usage) {
            case HID_USAGE_GD_HAT:
            case HID_USAGE_GD_X:
            case HID_USAGE_GD_Y:
            case HID_USAGE_GD_Z:
            case HID_USAGE_GD_RX:
            case HID_USAGE_GD_RY:
            case HID_USAGE_GD_RZ:
            case HID_USAGE_GD_START:
            case HID_USAGE_GD_SELECT:
            case HID_USAGE_GD_DPAD_UP:
            case HID_USAGE_GD_DPAD_DOWN:
            case HID_USAGE_GD_DPAD_LEFT:
            case HID_USAGE_GD_DPAD_RIGHT:
                return true;
            default:
                return false;
        }
    };

    auto is_relevant_field = [&](const Field& field) {
        if (field.usage_page == HID_USAGE_PAGE_BUTTON) {
            return true;
        }
        if (field.is_array) {
            return false;
        }
        if (field.usage_page == HID_USAGE_PAGE_GENERIC_DESKTOP) {
            return is_supported_generic_desktop_usage(field.usage);
        }
        if (field.usage_page == HID_USAGE_PAGE_CONSUMER) {
            return field.usage == HID_CONSUMER_AC_HOME || field.usage == HID_CONSUMER_AC_BACK;
        }
        return false;
    };

    auto track_field_evidence = [&](const Field& field, ParseEvidence& evidence) {
        if (field.usage_page == HID_USAGE_PAGE_BUTTON) {
            if (field.is_array) {
                track_button_range(evidence, field.usage_min, field.usage_max);
            } else {
                track_button_usage(evidence, field.usage);
            }
            return;
        }

        if (field.usage_page == HID_USAGE_PAGE_GENERIC_DESKTOP) {
            switch (field.usage) {
                case HID_USAGE_GD_HAT:
                    evidence.has_hat = true;
                    break;
                case HID_USAGE_GD_X:
                    evidence.have_x = true;
                    break;
                case HID_USAGE_GD_Y:
                    evidence.have_y = true;
                    break;
                case HID_USAGE_GD_START:
                    evidence.has_start = true;
                    break;
                case HID_USAGE_GD_SELECT:
                    evidence.has_select = true;
                    break;
                case HID_USAGE_GD_DPAD_UP:
                case HID_USAGE_GD_DPAD_DOWN:
                case HID_USAGE_GD_DPAD_LEFT:
                case HID_USAGE_GD_DPAD_RIGHT:
                    evidence.has_dpad_bits = true;
                    break;
                default:
                    break;
            }
        }
    };

    auto looks_like_relaxed_gamepad = [](const ParseEvidence& evidence) {
        if ((evidence.has_hat || evidence.has_dpad_bits) && evidence.button_count >= 4) {
            return true;
        }
        if (evidence.have_x && evidence.have_y && evidence.button_count >= 6 &&
            (evidence.has_start || evidence.has_select || evidence.max_button_usage >= 8)) {
            return true;
        }
        return false;
    };

    auto looks_like_xbox_fallback = [](const ParseEvidence& evidence) {
        bool has_direction = evidence.has_hat || evidence.has_dpad_bits || (evidence.have_x && evidence.have_y);
        if (!has_direction) {
            return false;
        }
        if (evidence.button_count >= 8) {
            return true;
        }
        if (evidence.button_count >= 6 && evidence.has_start && evidence.has_select) {
            return true;
        }
        return false;
    };

    GlobalState g;
    LocalState l;
    std::vector<GlobalState> gstack;
    std::vector<bool> collection_stack;
    std::vector<Field> strict_fields;
    std::vector<Field> relaxed_fields;
    ParseEvidence relaxed_evidence;
    bool strict_has_report_id = false;
    bool relaxed_has_report_id = false;
    uint16_t strict_max_button_usage = 0;
    bool in_gamepad = false;
    uint16_t report_offset[256] = {0};

    int i = 0;
    while (i < len) {
        uint8_t b = data[i++];
        if (b == 0xFE) { // long item
            if (i + 2 > len) {
                break;
            }
            uint8_t size = data[i++];
            i++; // skip long item tag
            i += size;
            continue;
        }

        int size_code = b & 0x3;
        int size = (size_code == 3) ? 4 : size_code;
        int type = (b >> 2) & 0x3;
        int tag = (b >> 4) & 0xF;

        uint32_t val = 0;
        for (int n = 0; n < size && i < len; n++) {
            val |= ((uint32_t)data[i++]) << (8 * n);
        }

        int32_t sval = sign_extend(val, size * 8);

        if (type == HID_ITEM_TYPE_GLOBAL) {
            switch (tag) {
                case 0x0: g.usage_page = (uint16_t)val; break;
                case 0x1: g.logical_min = sval; break;
                case 0x2: g.logical_max = sval; break;
                case 0x7: g.report_size = (uint8_t)val; break;
                case 0x8: g.report_id = (uint8_t)val; break;
                case 0x9: g.report_count = (uint16_t)val; break;
                case 0xA: gstack.push_back(g); break;
                case 0xB:
                    if (!gstack.empty()) {
                        g = gstack.back();
                        gstack.pop_back();
                    }
                    break;
                default:
                    break;
            }
            continue;
        }

        if (type == HID_ITEM_TYPE_LOCAL) {
            switch (tag) {
                case 0x0: l.usages.push_back((uint16_t)val); break;
                case 0x1: l.usage_min = (uint16_t)val; l.has_usage_min = true; break;
                case 0x2: l.usage_max = (uint16_t)val; l.has_usage_max = true; break;
                default: break;
            }
            continue;
        }

        if (type != HID_ITEM_TYPE_MAIN) {
            continue;
        }

        switch (tag) {
            case 0x8: { // Input
                uint8_t rid = g.report_id;
                uint16_t& offset = report_offset[rid];
                uint16_t count = g.report_count ? g.report_count : 0;
                uint8_t size_bits = g.report_size;
                uint32_t total_bits = (uint32_t)count * size_bits;

                bool is_constant = (val & 0x01) != 0;
                bool is_variable = (val & 0x02) != 0;

                if (!is_constant && count && size_bits) {
                    if (is_variable) {
                        for (uint16_t idx = 0; idx < count; idx++) {
                            uint16_t usage = 0;
                            if (idx < l.usages.size()) {
                                usage = l.usages[idx];
                            } else if (l.has_usage_min && l.has_usage_max) {
                                usage = (uint16_t)(l.usage_min + idx);
                            }
                            Field f;
                            f.usage_page = g.usage_page;
                            f.usage = usage;
                            f.usage_min = 0;
                            f.usage_max = 0;
                            f.report_id = rid;
                            f.bit_offset = offset;
                            f.bit_size = size_bits;
                            f.logical_min = g.logical_min;
                            f.logical_max = g.logical_max;
                            f.is_signed = (g.logical_min < 0);
                            f.is_array = false;
                            // Only fields the decoder uses. Vendor data inside a
                            // gamepad collection (e.g. PS4 reports) produced
                            // hundreds of entries (KBs on a starved heap), and a
                            // vendor-only report then decoded as "no buttons",
                            // releasing held buttons.
                            if (in_gamepad && is_relevant_field(f)) {
                                strict_fields.push_back(f);
                                if (g.usage_page == HID_USAGE_PAGE_BUTTON && usage > strict_max_button_usage) {
                                    strict_max_button_usage = usage;
                                }
                                if (rid != 0) {
                                    strict_has_report_id = true;
                                }
                            }
                            if (is_relevant_field(f)) {
                                relaxed_fields.push_back(f);
                                if (!f.is_array || idx == 0) {
                                    track_field_evidence(f, relaxed_evidence);
                                }
                                if (rid != 0) {
                                    relaxed_has_report_id = true;
                                }
                            }
                            offset += size_bits;
                        }
                    } else {
                        uint16_t umin = 0;
                        uint16_t umax = 0;
                        if (l.has_usage_min && l.has_usage_max) {
                            umin = l.usage_min;
                            umax = l.usage_max;
                        } else if (!l.usages.empty()) {
                            umin = l.usages.front();
                            umax = l.usages.back();
                        }
                        for (uint16_t idx = 0; idx < count; idx++) {
                            Field f;
                            f.usage_page = g.usage_page;
                            f.usage = 0;
                            f.usage_min = umin;
                            f.usage_max = umax;
                            f.report_id = rid;
                            f.bit_offset = offset;
                            f.bit_size = size_bits;
                            f.logical_min = g.logical_min;
                            f.logical_max = g.logical_max;
                            f.is_signed = (g.logical_min < 0);
                            f.is_array = true;
                            if (in_gamepad && is_relevant_field(f)) {
                                strict_fields.push_back(f);
                                if (g.usage_page == HID_USAGE_PAGE_BUTTON && umax > strict_max_button_usage) {
                                    strict_max_button_usage = umax;
                                }
                                if (rid != 0) {
                                    strict_has_report_id = true;
                                }
                            }
                            if (is_relevant_field(f)) {
                                relaxed_fields.push_back(f);
                                if (idx == 0) {
                                    track_field_evidence(f, relaxed_evidence);
                                }
                                if (rid != 0) {
                                    relaxed_has_report_id = true;
                                }
                            }
                            offset += size_bits;
                        }
                    }
                } else {
                    offset += total_bits;
                }

                l.clear();
                break;
            }

            case 0xA: { // Collection
                uint16_t usage = 0;
                if (!l.usages.empty()) {
                    usage = l.usages.back();
                } else if (l.has_usage_min) {
                    usage = l.usage_min;
                }

                bool is_gp = in_gamepad;
                if (g.usage_page == HID_USAGE_PAGE_GENERIC_DESKTOP) {
                    if (usage == HID_USAGE_GD_GAMEPAD ||
                        usage == HID_USAGE_GD_JOYSTICK ||
                        usage == HID_USAGE_GD_MULTI_AXIS) {
                        is_gp = true;
                        _is_gamepad = true;
                    }
                }
                collection_stack.push_back(in_gamepad);
                in_gamepad = is_gp;
                l.clear();
                break;
            }

            case 0xC: { // End Collection
                if (!collection_stack.empty()) {
                    in_gamepad = collection_stack.back();
                    collection_stack.pop_back();
                }
                l.clear();
                break;
            }

            default:
                l.clear();
                break;
        }
    }

    if (_is_gamepad && !strict_fields.empty()) {
        _fields.swap(strict_fields);
        _has_report_id = strict_has_report_id;
        _max_button_usage = strict_max_button_usage;
        return true;
    }

    bool relaxed_ok = looks_like_relaxed_gamepad(relaxed_evidence);
    if (!relaxed_ok && (parse_hints & PARSE_HINT_STRONG_XBOX_BLE)) {
        relaxed_ok = looks_like_xbox_fallback(relaxed_evidence);
    }
    if (!relaxed_ok || relaxed_fields.empty()) {
        _is_gamepad = false;
        return false;
    }

    _fields.swap(relaxed_fields);
    _is_gamepad = true;
    _has_report_id = relaxed_has_report_id;
    _max_button_usage = relaxed_evidence.max_button_usage;
    return true;
}

bool HIDGamepad::has_report_id() const {
    return _has_report_id;
}

static void apply_hat(uint16_t& buttons, int hat_value) {
    switch (hat_value) {
        case 0: buttons |= GENERIC_UP; break;
        case 1: buttons |= GENERIC_UP | GENERIC_RIGHT; break;
        case 2: buttons |= GENERIC_RIGHT; break;
        case 3: buttons |= GENERIC_DOWN | GENERIC_RIGHT; break;
        case 4: buttons |= GENERIC_DOWN; break;
        case 5: buttons |= GENERIC_DOWN | GENERIC_LEFT; break;
        case 6: buttons |= GENERIC_LEFT; break;
        case 7: buttons |= GENERIC_UP | GENERIC_LEFT; break;
        default: break;
    }
}

static uint16_t map_button_usage(uint16_t usage, uint16_t max_button_usage, uint32_t decode_quirks) {
    if (decode_quirks & HIDGamepad::DECODE_QUIRK_XBOX_BLE) {
        switch (usage) {
            case 1: return GENERIC_FIRE_A;
            case 2: return GENERIC_FIRE_B;
            case 3: return GENERIC_FIRE_X;
            case 4: return GENERIC_FIRE_Y;
            case 5: return GENERIC_FIRE_C;
            case 6: return GENERIC_OTHER;
            case 7: return GENERIC_SELECT;
            case 8: return GENERIC_START;
            case 9: return GENERIC_OTHER;
            case 10: return GENERIC_OTHER;
            default: return 0;
        }
    }

    if (decode_quirks & HIDGamepad::DECODE_QUIRK_ANDROID_LAYOUT) {
        switch (usage) {
            case 1: return GENERIC_FIRE_A;
            case 2: return GENERIC_FIRE_B;
            case 4: return GENERIC_FIRE_X;
            case 5: return GENERIC_FIRE_Y;
            case 7: return GENERIC_FIRE_C;      // L
            case 8: return GENERIC_FIRE_Z;      // R
            case 9: return GENERIC_FIRE;        // L2
            case 10: return GENERIC_OTHER;      // R2
            case 11: return GENERIC_SELECT;
            case 12: return GENERIC_START;
            case 13: return GENERIC_MENU;       // Home
            default: return 0;
        }
    }

    // Extended layouts usually place Select, Start, and Menu at usages 9, 10, and 13.
    if (max_button_usage >= 10) {
        switch (usage) {
            case 1: return GENERIC_FIRE_A;
            case 2: return GENERIC_FIRE_B;
            case 3: return GENERIC_FIRE_X;
            case 4: return GENERIC_FIRE_Y;
            case 5: return GENERIC_FIRE_C;
            case 6: return GENERIC_OTHER;
            case 7: return GENERIC_SELECT;
            case 8: return GENERIC_START;
            case 9: return GENERIC_SELECT;
            case 10: return GENERIC_START;
            case 11: return GENERIC_MENU;
            case 13: return GENERIC_MENU;
            default: return 0;
        }
    }

    switch (usage) {
        case 1: return GENERIC_FIRE_A;
        case 2: return GENERIC_FIRE_B;
        case 3: return GENERIC_FIRE_X;
        case 4: return GENERIC_FIRE_Y;
        case 5: return GENERIC_FIRE_C;
        case 6: return GENERIC_OTHER;
        case 7: return GENERIC_SELECT;
        case 8: return GENERIC_START;
        case 9: return GENERIC_OTHER;
        case 10: return GENERIC_OTHER;
        case 11: return GENERIC_MENU;
        default: return 0;
    }
}

static inline void set_raw_button_bit(uint64_t& raw_buttons, uint16_t usage)
{
    if (usage < 1 || usage > 64)
        return;
    raw_buttons |= (1ULL << (usage - 1));
}

bool HIDGamepad::decode_report_state(const uint8_t* data, int len, DecodedState* out_state, uint32_t decode_quirks) const {
    if (!out_state) {
        return false;
    }
    out_state->raw_buttons = 0;
    out_state->buttons = 0;

    if (!_is_gamepad || _fields.empty() || !data || len < 2) {
        return false;
    }
    if (data[0] != 0xA1) {
        return false;
    }

    auto decode_with_layout = [&](uint8_t report_id,
                                  const uint8_t* payload,
                                  int payload_len,
                                  DecodedState* state_out) -> bool {
        if (!payload || payload_len <= 0 || !state_out) {
            return false;
        }

        int payload_bits = payload_len * 8;
        uint64_t raw_buttons = 0;
        uint16_t buttons = 0;
        bool has_any = false;
        bool has_hat = false;
        int hat_value = -1;
        int hat_min = 0;
        int hat_max = 0;
        int32_t x_val = 0;
        int32_t y_val = 0;
        bool have_x = false;
        bool have_y = false;
        bool have_dpad_bits = false;
        int32_t x_min = 0, x_max = 0, y_min = 0, y_max = 0;

        for (const auto& f : _fields) {
            if (f.report_id != report_id) {
                continue;
            }
            int bit_offset = f.bit_offset;
            if (bit_offset + f.bit_size > payload_bits) {
                continue;
            }
            uint32_t raw = extract_bits(payload, bit_offset, f.bit_size);
            int32_t value = f.is_signed ? sign_extend(raw, f.bit_size) : (int32_t)raw;
            has_any = true;

            if (f.is_array) {
                if (f.usage_page == HID_USAGE_PAGE_BUTTON) {
                    if (value >= (int32_t)f.usage_min && value <= (int32_t)f.usage_max) {
                        set_raw_button_bit(raw_buttons, (uint16_t)value);
                        buttons |= map_button_usage((uint16_t)value, _max_button_usage, decode_quirks);
                    }
                }
                continue;
            }

            if (f.usage_page == HID_USAGE_PAGE_BUTTON) {
                if (value) {
                    set_raw_button_bit(raw_buttons, f.usage);
                    buttons |= map_button_usage(f.usage, _max_button_usage, decode_quirks);
                }
                continue;
            }

            if (f.usage_page == HID_USAGE_PAGE_GENERIC_DESKTOP) {
                switch (f.usage) {
                    case HID_USAGE_GD_HAT:
                        has_hat = true;
                        hat_value = value;
                        hat_min = f.logical_min;
                        hat_max = f.logical_max;
                        break;
                    case HID_USAGE_GD_X:
                        x_val = value; have_x = true; x_min = f.logical_min; x_max = f.logical_max; break;
                    case HID_USAGE_GD_Y:
                        y_val = value; have_y = true; y_min = f.logical_min; y_max = f.logical_max; break;
                    case HID_USAGE_GD_Z:
                        if (f.logical_max > f.logical_min) {
                            int32_t range = f.logical_max - f.logical_min;
                            int32_t center = f.logical_min + (range / 2);
                            int32_t dead = range / 5;
                            if (decode_quirks & DECODE_QUIRK_XBOX_BLE) {
                                int32_t delta = value - center;
                                if (delta < 0)
                                    delta = -delta;
                                if (delta > dead) {
                                    buttons |= GENERIC_FIRE_C;
                                }
                            } else if (value > center + dead) {
                                buttons |= GENERIC_FIRE_C;
                            }
                        }
                        break;
                    case HID_USAGE_GD_RZ:
                    case HID_USAGE_GD_RX:
                        if (decode_quirks & DECODE_QUIRK_XBOX_BLE) {
                            break;
                        }
                        if (f.logical_max > f.logical_min) {
                            int32_t range = f.logical_max - f.logical_min;
                            int32_t center = f.logical_min + (range / 2);
                            int32_t dead = range / 5;
                            if (value > center + dead) {
                                buttons |= GENERIC_FIRE_C;
                            }
                        }
                        break;
                    case HID_USAGE_GD_START:
                        if (value) buttons |= GENERIC_START;
                        break;
                    case HID_USAGE_GD_SELECT:
                        if (value) buttons |= GENERIC_SELECT;
                        break;
                    case HID_USAGE_GD_DPAD_UP:
                        have_dpad_bits = true;
                        if (value) buttons |= GENERIC_UP;
                        break;
                    case HID_USAGE_GD_DPAD_DOWN:
                        have_dpad_bits = true;
                        if (value) buttons |= GENERIC_DOWN;
                        break;
                    case HID_USAGE_GD_DPAD_LEFT:
                        have_dpad_bits = true;
                        if (value) buttons |= GENERIC_LEFT;
                        break;
                    case HID_USAGE_GD_DPAD_RIGHT:
                        have_dpad_bits = true;
                        if (value) buttons |= GENERIC_RIGHT;
                        break;
                    default:
                        break;
                }
                continue;
            }

            if (f.usage_page == HID_USAGE_PAGE_CONSUMER) {
                if (value && (f.usage == HID_CONSUMER_AC_HOME || f.usage == HID_CONSUMER_AC_BACK)) {
                    buttons |= GENERIC_MENU;
                }
            }
        }

        if (!has_any) {
            return false;
        }

        if (has_hat) {
            int range = hat_max - hat_min;
            if (range == 0) {
                hat_min = 0;
                hat_max = 7;
                range = 7;
            }
            int rel = hat_value - hat_min;
            if (range >= 7) {
                if (rel >= 0 && rel <= 7) {
                    apply_hat(buttons, rel);
                }
            } else if (range == 3) {
                if (rel >= 0 && rel <= 3) {
                    static const int map4[4] = {0, 2, 4, 6};
                    apply_hat(buttons, map4[rel]);
                }
            } else if (rel >= 0 && rel <= 7) {
                apply_hat(buttons, rel);
            }
        }

        // 8BitDo D-input pads declare a hat that always reports its null
        // state; their D-pad is on the X/Y axes.
        const bool hat_idle_only =
            has_hat && (decode_quirks & DECODE_QUIRK_ANDROID_LAYOUT) &&
            (hat_value < hat_min || hat_value > hat_max);

        const bool left_stick_maps_to_dpad =
            have_x && have_y && (x_max != x_min) && (y_max != y_min) &&
            ((decode_quirks & DECODE_QUIRK_XBOX_BLE) ||
             ((!has_hat || hat_idle_only) && !have_dpad_bits));

        if (left_stick_maps_to_dpad) {
            int32_t xrange = x_max - x_min;
            int32_t xcenter = x_min + (xrange / 2);
            int32_t xdead = xrange / 6;
            if (x_val < xcenter - xdead) {
                buttons |= GENERIC_LEFT;
            } else if (x_val > xcenter + xdead) {
                buttons |= GENERIC_RIGHT;
            }

            int32_t yrange = y_max - y_min;
            int32_t ycenter = y_min + (yrange / 2);
            int32_t ydead = yrange / 6;
            if (y_val < ycenter - ydead) {
                buttons |= GENERIC_UP;
            } else if (y_val > ycenter + ydead) {
                buttons |= GENERIC_DOWN;
            }
        }

        state_out->raw_buttons = raw_buttons;
        state_out->buttons = buttons;
        return true;
    };

    DecodedState decoded;
    if (!_has_report_id) {
        if (decode_with_layout(0, data + 1, len - 1, &decoded)) {
            *out_state = decoded;
            return true;
        }
        if (len > 2 && decode_with_layout(data[1], data + 2, len - 2, &decoded)) {
            *out_state = decoded;
            return true;
        }
        return false;
    }

    if (len > 2 && decode_with_layout(data[1], data + 2, len - 2, &decoded)) {
        *out_state = decoded;
        return true;
    }

    if (decode_with_layout(0, data + 1, len - 1, &decoded)) {
        *out_state = decoded;
        return true;
    }

    uint8_t single_nonzero_report_id = 0;
    bool have_single_nonzero_report_id = false;
    bool has_conflicting_report_id = false;
    for (const auto& f : _fields) {
        if (f.report_id == 0)
            continue;
        if (!have_single_nonzero_report_id) {
            single_nonzero_report_id = f.report_id;
            have_single_nonzero_report_id = true;
            continue;
        }
        if (single_nonzero_report_id != f.report_id) {
            has_conflicting_report_id = true;
            break;
        }
    }
    if (have_single_nonzero_report_id && !has_conflicting_report_id) {
        if (decode_with_layout(single_nonzero_report_id, data + 1, len - 1, &decoded)) {
            *out_state = decoded;
            return true;
        }
    }

    if (len > 3 && data[1] == data[2]) {
        if (decode_with_layout(data[1], data + 3, len - 3, &decoded)) {
            *out_state = decoded;
            return true;
        }
    }

    return false;
}
