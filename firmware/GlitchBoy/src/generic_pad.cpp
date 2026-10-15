#include "generic_pad.h"

struct GenericPadSlot {
    const void* owner;
    uint16_t state;
    uint64_t raw_buttons;
};

static GenericPadSlot _generic_pads[2] = {};
static uint16_t _generic_pad1_overlay = 0;

static int find_slot(const void* owner)
{
    if (!owner)
        return -1;
    for (int i = 0; i < 2; ++i) {
        if (_generic_pads[i].owner == owner)
            return i;
    }
    return -1;
}

int generic_pad_assign(const void* owner)
{
    int slot = find_slot(owner);
    if (slot >= 0)
        return slot;
    if (!owner)
        return -1;
    for (int i = 0; i < 2; ++i) {
        if (!_generic_pads[i].owner) {
            _generic_pads[i].owner = owner;
            _generic_pads[i].state = 0;
            _generic_pads[i].raw_buttons = 0;
            return i;
        }
    }
    return -1;
}

void generic_pad_release(const void* owner)
{
    int slot = find_slot(owner);
    if (slot < 0)
        return;
    _generic_pads[slot].owner = 0;
    _generic_pads[slot].state = 0;
    _generic_pads[slot].raw_buttons = 0;
}

void generic_pad_update(const void* owner, uint16_t state)
{
    generic_pad_update_ex(owner, state, (uint64_t)state);
}

void generic_pad_update_ex(const void* owner, uint16_t state, uint64_t raw_buttons)
{
    int slot = find_slot(owner);
    if (slot < 0)
        return;
    _generic_pads[slot].state = state;
    _generic_pads[slot].raw_buttons = raw_buttons;
}

void generic_pad_set_player1_overlay(uint16_t state)
{
    _generic_pad1_overlay = state;
}

int generic_pad_build_report(uint8_t* dst, int dst_len)
{
    if (!dst || dst_len < 6)
        return 0;
    const uint16_t player1 = _generic_pads[0].state | _generic_pad1_overlay;
    const uint16_t player2 = _generic_pads[1].state;
    dst[0] = 0xA1;
    dst[1] = 0x42;
    dst[2] = player1 & 0xFF;
    dst[3] = player1 >> 8;
    dst[4] = player2 & 0xFF;
    dst[5] = player2 >> 8;
    return 6;
}
