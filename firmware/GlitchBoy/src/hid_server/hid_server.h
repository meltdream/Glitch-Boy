
/* Copyright (c) 2020, Peter Barrett
**
** Permission to use, copy, modify, and/or distribute this software for
** any purpose with or without fee is hereby granted, provided that the
** above copyright notice and this permission notice appear in all copies.
**
** THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
** BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
** OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
** WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
** ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
** SOFTWARE.
*/

#ifndef hid_server_hpp
#define hid_server_hpp

#include <stdio.h>
#include "hci_server.h"

#ifdef __cplusplus
extern "C" {
#endif

// https://w3c.github.io/gamepad/#dfn-standard-gamepad-layout <- might be useful at some point

// big report 0x37
// https://web.archive.org/web/20080630032911/http://wiki.wiimoteproject.com/Reports
// ?    X1    X0    PLUS    UP    DOWN    RIGHT    LEFT
// HOME Z1    Y1    MINUS   A     B       ONE       TWO
// x,y,z
// ir0..9
// ext0..5
// classic controller
// BDR    BDD    BLT    B-    BH    B+    BRT    1
// BZL    BB     BY     BA    BX    BZR   BDL    BDU

enum wii_flags {
    wiimote = 0x01,
    nunchuk = 0x02,
    classic_controller = 0x04,
    drums_guitar = 0x08,
    motion = 0x10,
    extension_read = 0x100,
    encryption_disabled = 0x200,
    extension_polled = 0x10000,
    extension_poll_pending = 0x20000,

    // ?    XLRLSB    XLRLSB    PLUS    UP    DOWN    RIGHT    LEFT
    // HOME XLRLSB    XLRLSB    MINUS   A     B       ONE       TWO
    wii_plus = 0x1000,
    wii_up = 0x0800,
    wii_down = 0x0400,
    wii_right = 0x0200,
    wii_left = 0x0100,
    wii_home = 0x0080,
    wii_minus = 0x0010,
    wii_a = 0x0008,
    wii_b = 0x0004,
    wii_one = 0x0002,
    wii_two = 0x0001,

    // BDR    BDD    BLT    B-    BH    B+    BRT    1
    // BZL    BB     BY     BA    BX    BZR   BDL    BDU
    wii_classic_right = 0x8000,
    wii_classic_down = 0x4000,
    wii_classic_left_top = 0x2000,
    wii_classic_minus = 0x1000,

    wii_classic_home = 0x0800,
    wii_classic_plus = 0x0400,
    wii_classic_right_top = 0x0200,

    wii_classic_b = 0x0040,
    wii_classic_y = 0x0020,
    wii_classic_a = 0x0010,

    wii_classic_x = 0x0008,
    wii_classic_left = 0x0002,
    wii_classic_up = 0x0001,
};

typedef struct {
    uint32_t flags;
    uint8_t requested_report_mode;
    uint32_t report_mode_request_ms;
    uint32_t extension_poll_ms;
    uint8_t report[32];
    uint16_t common()   { return (report[2] << 8) | report[3]; }
    uint16_t classic()  {
        if (report[1] == 0x37)
            return ((report[21] << 8) | report[22]) ^ 0xFFFF;
        return ((report[8] << 8) | report[9]) ^ 0xFFFF;
    }
} wii_state;

// report all of the wii states
extern wii_state wii_states[4];
uint32_t wii_map(int index, const uint32_t* common, const uint32_t* classic);

// minimal hid interface
int hid_init(const char* local_name);
int hid_update();
int hid_close();
int hid_get(uint8_t* dst, int dst_len); //
int hid_set_pairing(int enabled);
int hid_get_pairing();
int hid_is_searching();
int hid_connected_count();

void gui_msg(const char* msg);                                  // temporarily display a msg

#ifdef __cplusplus
}
#endif

#endif /* hid_server_hpp */
