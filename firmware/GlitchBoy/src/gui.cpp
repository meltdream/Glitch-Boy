
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

#include "emu.h"
#include "glitch/glitch_controls.h"
#include "glitch/nes_glitch.h"
#include "glitch/gb_glitch.h"
#include "glitch/glitch_settings.h"
#include "midi_input.h"
#include "midi_visualizer.h"
#include "gb_apu_midi.h"
#include "bt_mode.h"
#include "gui_font_cga.h"
#include "gui_menu_backdrop_nes.h"
#include <Arduino.h>
extern "C" {
#include "nofrendo/bitmap.h"
}
extern bitmap_t *primary_buffer;
extern uint8_t** _lines;
extern volatile int _line_counter;
extern volatile int _frame_counter;
#ifdef ESP_PLATFORM
#include "driver/gpio.h"
#include <esp_heap_caps.h>
#include <esp_system.h>
#endif
#include "nofrendo/nes_apu_midi.h"
#include <algorithm>
#include <stdlib.h>

using namespace std;

static int16_t s_gui_audio_work_buffer[2048];

#ifndef GUI_GB_AUDIO_BEFORE_VIDEO
#define GUI_GB_AUDIO_BEFORE_VIDEO 1
#endif


string get_ext(const string& s)
{
    string ext;
    auto i = s.find_last_of(".");
    if (i > 0) {
        ext = s.substr(i+1);
        for (i = 0; i < ext.length(); i++)
            ext[i] = tolower(ext[i]);
    }
    return ext;
}

// missing in arduino?
string to_string(int i)
{
    char buf[32];
    sprintf(buf,"%d",i);
    return buf;
}

const uint8_t _font[2048] = {
    // atari 256 char inverted set
    0x00,0x18,0x03,0x18,0x18,0x00,0x03,0xc0,
    0x01,0x00,0x80,0x0f,0xf0,0xff,0x00,0x00,
    0x24,0x28,0x08,0x10,0x10,0x10,0x08,0x08,
    0x08,0x08,0x08,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x18,0x00,0x1c,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x06,0x00,0x60,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x18,0x00,0x08,0x10,
    0xff,0xe7,0xfc,0xe7,0xe7,0xff,0xfc,0x3f,
    0xfe,0xff,0x7f,0xf0,0x0f,0x00,0xff,0xff,
    0xff,0xff,0xff,0xe7,0xff,0xff,0x3f,0xff,
    0xe7,0x0f,0xe7,0x87,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xe7,0xff,0xe3,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xf9,0xff,0x9f,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xe7,0xff,0xf7,0xef,

    0x00,0x18,0x03,0x18,0x18,0x00,0x07,0xe0,
    0x03,0x00,0xc0,0x0f,0xf0,0xff,0x00,0x00,
    0x00,0x00,0x10,0x08,0x08,0x08,0x14,0x14,
    0x14,0x14,0x14,0x00,0x18,0x18,0x18,0x18,
    0x00,0x18,0x66,0x66,0x3e,0x66,0x36,0x18,
    0x0e,0x70,0x66,0x18,0x00,0x00,0x00,0x06,
    0x3c,0x18,0x3c,0x7e,0x0c,0x7e,0x3c,0x7e,
    0x3c,0x3c,0x00,0x00,0x0c,0x00,0x30,0x3c,
    0x3c,0x18,0x7c,0x3c,0x78,0x7e,0x7e,0x3e,
    0x66,0x7e,0x06,0x66,0x60,0x63,0x66,0x3c,
    0x7c,0x3c,0x7c,0x3c,0x7e,0x66,0x66,0x63,
    0x66,0x66,0x7e,0x1e,0x40,0x78,0x08,0x00,
    0x18,0x00,0x60,0x00,0x06,0x00,0x0e,0x00,
    0x60,0x18,0x06,0x60,0x38,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x18,0x00,0x00,0x00,
    0x00,0x00,0x00,0x18,0x18,0x7e,0x18,0x18,
    0xff,0xe7,0xfc,0xe7,0xe7,0xff,0xf8,0x1f,
    0xfc,0xff,0x3f,0xf0,0x0f,0x00,0xff,0xff,
    0xe3,0xff,0xff,0xe7,0xff,0xff,0x3f,0xff,
    0xe7,0x0f,0xe7,0x9f,0xe7,0xe7,0xe7,0xe7,
    0xff,0xe7,0x99,0x99,0xc1,0x99,0xc9,0xe7,
    0xf1,0x8f,0x99,0xe7,0xff,0xff,0xff,0xf9,
    0xc3,0xe7,0xc3,0x81,0xf3,0x81,0xc3,0x81,
    0xc3,0xc3,0xff,0xff,0xf3,0xff,0xcf,0xc3,
    0xc3,0xe7,0x83,0xc3,0x87,0x81,0x81,0xc1,
    0x99,0x81,0xf9,0x99,0x9f,0x9c,0x99,0xc3,
    0x83,0xc3,0x83,0xc3,0x81,0x99,0x99,0x9c,
    0x99,0x99,0x81,0xe1,0xbf,0x87,0xf7,0xff,
    0xe7,0xff,0x9f,0xff,0xf9,0xff,0xf1,0xff,
    0x9f,0xe7,0xf9,0x9f,0xc7,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xe7,0xff,0xff,0xff,
    0xff,0xff,0xff,0xe7,0xe7,0x81,0xe7,0xe7,

    0x00,0x18,0x03,0x18,0x18,0x00,0x0e,0x70,
    0x07,0x00,0xe0,0x0f,0xf0,0x00,0x00,0x00,
    0x3c,0x38,0x3c,0x3c,0x3c,0x66,0x3c,0x3c,
    0x00,0x3c,0x62,0x3c,0x3c,0x18,0x30,0x0c,
    0x00,0x18,0x66,0xff,0x60,0x6c,0x1c,0x18,
    0x1c,0x38,0x3c,0x18,0x00,0x00,0x00,0x0c,
    0x66,0x38,0x66,0x0c,0x1c,0x60,0x60,0x06,
    0x66,0x66,0x18,0x18,0x18,0x7e,0x18,0x66,
    0x66,0x3c,0x66,0x66,0x6c,0x60,0x60,0x60,
    0x66,0x18,0x06,0x6c,0x60,0x77,0x76,0x66,
    0x66,0x66,0x66,0x60,0x18,0x66,0x66,0x63,
    0x66,0x66,0x0c,0x18,0x60,0x18,0x1c,0x00,
    0x3c,0x3c,0x60,0x3c,0x06,0x3c,0x18,0x3e,
    0x60,0x00,0x00,0x60,0x18,0x66,0x7c,0x3c,
    0x7c,0x3e,0x7c,0x3e,0x7e,0x66,0x66,0x63,
    0x66,0x66,0x7e,0x3c,0x18,0x78,0x38,0x1c,
    0xff,0xe7,0xfc,0xe7,0xe7,0xff,0xf1,0x8f,
    0xf8,0xff,0x1f,0xf0,0x0f,0xff,0xff,0xff,
    0xe3,0xff,0xff,0xe7,0xc3,0xff,0x3f,0xff,
    0xe7,0x0f,0xe7,0x87,0xc3,0xe7,0xcf,0xf3,
    0xff,0xe7,0x99,0x00,0x9f,0x93,0xe3,0xe7,
    0xe3,0xc7,0xc3,0xe7,0xff,0xff,0xff,0xf3,
    0x99,0xc7,0x99,0xf3,0xe3,0x9f,0x9f,0xf9,
    0x99,0x99,0xe7,0xe7,0xe7,0x81,0xe7,0x99,
    0x99,0xc3,0x99,0x99,0x93,0x9f,0x9f,0x9f,
    0x99,0xe7,0xf9,0x93,0x9f,0x88,0x89,0x99,
    0x99,0x99,0x99,0x9f,0xe7,0x99,0x99,0x9c,
    0x99,0x99,0xf3,0xe7,0x9f,0xe7,0xe3,0xff,
    0xc3,0xc3,0x9f,0xc3,0xf9,0xc3,0xe7,0xc1,
    0x9f,0xff,0xff,0x9f,0xe7,0x99,0x83,0xc3,
    0x83,0xc1,0x83,0xc1,0x81,0x99,0x99,0x9c,
    0x99,0x99,0x81,0xc3,0xe7,0x87,0xc7,0xe3,

    0x00,0x1f,0x03,0xf8,0xf8,0xf8,0x1c,0x38,
    0x0f,0x00,0xf0,0x0f,0xf0,0x00,0x00,0x00,
    0x66,0x18,0x66,0x06,0x66,0x66,0x06,0x66,
    0x38,0x66,0x66,0x60,0x7e,0x18,0x7e,0x7e,
    0x00,0x18,0x66,0x66,0x3c,0x18,0x38,0x18,
    0x18,0x18,0xff,0x7e,0x00,0x7e,0x00,0x18,
    0x6e,0x18,0x0c,0x18,0x3c,0x7c,0x7c,0x0c,
    0x3c,0x3e,0x18,0x18,0x30,0x00,0x0c,0x0c,
    0x6e,0x66,0x7c,0x60,0x66,0x7c,0x7c,0x60,
    0x7e,0x18,0x06,0x78,0x60,0x7f,0x7e,0x66,
    0x66,0x66,0x66,0x3c,0x18,0x66,0x66,0x6b,
    0x3c,0x3c,0x18,0x18,0x30,0x18,0x36,0x00,
    0x7e,0x06,0x7c,0x60,0x3e,0x66,0x3e,0x66,
    0x7c,0x38,0x06,0x6c,0x18,0x7f,0x66,0x66,
    0x66,0x66,0x66,0x60,0x18,0x66,0x66,0x6b,
    0x3c,0x66,0x0c,0x7e,0x18,0x7c,0x78,0x1e,
    0xff,0xe0,0xfc,0x07,0x07,0x07,0xe3,0xc7,
    0xf0,0xff,0x0f,0xf0,0x0f,0xff,0xff,0xff,
    0x88,0xe0,0x00,0x00,0x81,0xff,0x3f,0x00,
    0x00,0x0f,0xe0,0x9f,0x81,0xe7,0x81,0x81,
    0xff,0xe7,0x99,0x99,0xc3,0xe7,0xc7,0xe7,
    0xe7,0xe7,0x00,0x81,0xff,0x81,0xff,0xe7,
    0x91,0xe7,0xf3,0xe7,0xc3,0x83,0x83,0xf3,
    0xc3,0xc1,0xe7,0xe7,0xcf,0xff,0xf3,0xf3,
    0x91,0x99,0x83,0x9f,0x99,0x83,0x83,0x9f,
    0x81,0xe7,0xf9,0x87,0x9f,0x80,0x81,0x99,
    0x99,0x99,0x99,0xc3,0xe7,0x99,0x99,0x94,
    0xc3,0xc3,0xe7,0xe7,0xcf,0xe7,0xc9,0xff,
    0x81,0xf9,0x83,0x9f,0xc1,0x99,0xc1,0x99,
    0x83,0xc7,0xf9,0x93,0xe7,0x80,0x99,0x99,
    0x99,0x99,0x99,0x9f,0xe7,0x99,0x99,0x94,
    0xc3,0x99,0xf3,0x81,0xe7,0x83,0x87,0xe1,

    0x00,0x1f,0x03,0xf8,0xf8,0xf8,0x38,0x1c,
    0x1f,0x0f,0xf8,0x00,0x00,0x00,0x00,0xf0,
    0x7e,0x18,0x7e,0x3e,0x7e,0x66,0x3e,0x7e,
    0x18,0x66,0x66,0x60,0x18,0x7e,0x30,0x0c,
    0x00,0x18,0x00,0x66,0x06,0x30,0x6f,0x00,
    0x18,0x18,0x3c,0x18,0x00,0x00,0x00,0x30,
    0x76,0x18,0x18,0x0c,0x6c,0x06,0x66,0x18,
    0x66,0x06,0x00,0x00,0x18,0x00,0x18,0x18,
    0x6e,0x66,0x66,0x60,0x66,0x60,0x60,0x6e,
    0x66,0x18,0x06,0x78,0x60,0x6b,0x7e,0x66,
    0x7c,0x66,0x7c,0x06,0x18,0x66,0x66,0x7f,
    0x3c,0x18,0x30,0x18,0x18,0x18,0x63,0x00,
    0x7e,0x3e,0x66,0x60,0x66,0x7e,0x18,0x66,
    0x66,0x18,0x06,0x78,0x18,0x7f,0x66,0x66,
    0x66,0x66,0x60,0x3c,0x18,0x66,0x66,0x7f,
    0x18,0x66,0x18,0x7e,0x18,0x6e,0x38,0x1c,
    0xff,0xe0,0xfc,0x07,0x07,0x07,0xc7,0xe3,
    0xe0,0xf0,0x07,0xff,0xff,0xff,0xff,0x0f,
    0x88,0xe0,0x00,0x00,0x81,0x00,0x3f,0x00,
    0x00,0x0f,0xe0,0x81,0xe7,0x81,0xcf,0xf3,
    0xff,0xe7,0xff,0x99,0xf9,0xcf,0x90,0xff,
    0xe7,0xe7,0xc3,0xe7,0xff,0xff,0xff,0xcf,
    0x89,0xe7,0xe7,0xf3,0x93,0xf9,0x99,0xe7,
    0x99,0xf9,0xff,0xff,0xe7,0xff,0xe7,0xe7,
    0x91,0x99,0x99,0x9f,0x99,0x9f,0x9f,0x91,
    0x99,0xe7,0xf9,0x87,0x9f,0x94,0x81,0x99,
    0x83,0x99,0x83,0xf9,0xe7,0x99,0x99,0x80,
    0xc3,0xe7,0xcf,0xe7,0xe7,0xe7,0x9c,0xff,
    0x81,0xc1,0x99,0x9f,0x99,0x81,0xe7,0x99,
    0x99,0xe7,0xf9,0x87,0xe7,0x80,0x99,0x99,
    0x99,0x99,0x9f,0xc3,0xe7,0x99,0x99,0x80,
    0xe7,0x99,0xe7,0x81,0xe7,0x91,0xc7,0xe3,

    0x00,0x18,0x03,0x00,0x18,0x18,0x70,0x0e,
    0x3f,0x0f,0xfc,0x00,0x00,0x00,0x00,0xf0,
    0x60,0x18,0x60,0x66,0x60,0x66,0x66,0x60,
    0x18,0x66,0x66,0x60,0x18,0x3c,0x18,0x18,
    0x00,0x00,0x00,0xff,0x7c,0x66,0x66,0x00,
    0x1c,0x38,0x66,0x18,0x18,0x00,0x18,0x60,
    0x66,0x18,0x30,0x66,0x7e,0x66,0x66,0x30,
    0x66,0x0c,0x18,0x18,0x0c,0x7e,0x30,0x00,
    0x60,0x7e,0x66,0x66,0x6c,0x60,0x60,0x66,
    0x66,0x18,0x66,0x6c,0x60,0x63,0x6e,0x66,
    0x60,0x6c,0x6c,0x06,0x18,0x66,0x3c,0x77,
    0x66,0x18,0x60,0x18,0x0c,0x18,0x00,0x00,
    0x3c,0x66,0x66,0x60,0x66,0x60,0x18,0x3e,
    0x66,0x18,0x06,0x6c,0x18,0x6b,0x66,0x66,
    0x7c,0x3e,0x60,0x06,0x18,0x66,0x3c,0x3e,
    0x3c,0x3e,0x30,0x18,0x18,0x66,0x18,0x18,
    0xff,0xe7,0xfc,0xff,0xe7,0xe7,0x8f,0xf1,
    0xc0,0xf0,0x03,0xff,0xff,0xff,0xff,0x0f,
    0xf7,0xe7,0xff,0xe7,0x81,0x00,0x3f,0xe7,
    0xff,0x0f,0xff,0xe7,0xe7,0xc3,0xe7,0xe7,
    0xff,0xff,0xff,0x00,0x83,0x99,0x99,0xff,
    0xe3,0xc7,0x99,0xe7,0xe7,0xff,0xe7,0x9f,
    0x99,0xe7,0xcf,0x99,0x81,0x99,0x99,0xcf,
    0x99,0xf3,0xe7,0xe7,0xf3,0x81,0xcf,0xff,
    0x9f,0x81,0x99,0x99,0x93,0x9f,0x9f,0x99,
    0x99,0xe7,0x99,0x93,0x9f,0x9c,0x91,0x99,
    0x9f,0x93,0x93,0xf9,0xe7,0x99,0xc3,0x88,
    0x99,0xe7,0x9f,0xe7,0xf3,0xe7,0xff,0xff,
    0xc3,0x99,0x99,0x9f,0x99,0x9f,0xe7,0xc1,
    0x99,0xe7,0xf9,0x93,0xe7,0x94,0x99,0x99,
    0x83,0xc1,0x9f,0xf9,0xe7,0x99,0xc3,0xc1,
    0xc3,0xc1,0xcf,0xe7,0xe7,0x99,0xe7,0xe7,

    0x00,0x18,0x03,0x00,0x18,0x18,0xe0,0x07,
    0x7f,0x0f,0xfe,0x00,0x00,0x00,0xff,0xf0,
    0x3c,0x3c,0x3c,0x3e,0x3c,0x3e,0x3e,0x3c,
    0x3c,0x3c,0x3e,0x3c,0x18,0x18,0x00,0x00,
    0x00,0x18,0x00,0x66,0x18,0x46,0x3b,0x00,
    0x0e,0x70,0x00,0x00,0x18,0x00,0x18,0x40,
    0x3c,0x7e,0x7e,0x3c,0x0c,0x3c,0x3c,0x30,
    0x3c,0x38,0x18,0x18,0x06,0x00,0x60,0x18,
    0x3e,0x66,0x7c,0x3c,0x78,0x7e,0x60,0x3e,
    0x66,0x7e,0x3c,0x66,0x7e,0x63,0x66,0x3c,
    0x60,0x36,0x66,0x3c,0x18,0x7e,0x18,0x63,
    0x66,0x18,0x7e,0x1e,0x06,0x78,0x00,0xff,
    0x18,0x3e,0x7c,0x3c,0x3e,0x3c,0x18,0x06,
    0x66,0x3c,0x06,0x66,0x3c,0x63,0x66,0x3c,
    0x60,0x06,0x60,0x7c,0x0e,0x3e,0x18,0x36,
    0x66,0x0c,0x7e,0x3c,0x18,0x06,0x08,0x10,
    0xff,0xe7,0xfc,0xff,0xe7,0xe7,0x1f,0xf8,
    0x80,0xf0,0x01,0xff,0xff,0xff,0x00,0x0f,
    0xe3,0xe7,0xff,0xe7,0xc3,0x00,0x3f,0xe7,
    0xff,0x0f,0xff,0xe1,0xe7,0xe7,0xff,0xff,
    0xff,0xe7,0xff,0x99,0xe7,0xb9,0xc4,0xff,
    0xf1,0x8f,0xff,0xff,0xe7,0xff,0xe7,0xbf,
    0xc3,0x81,0x81,0xc3,0xf3,0xc3,0xc3,0xcf,
    0xc3,0xc7,0xe7,0xe7,0xf9,0xff,0x9f,0xe7,
    0xc1,0x99,0x83,0xc3,0x87,0x81,0x9f,0xc1,
    0x99,0x81,0xc3,0x99,0x81,0x9c,0x99,0xc3,
    0x9f,0xc9,0x99,0xc3,0xe7,0x81,0xe7,0x9c,
    0x99,0xe7,0x81,0xe1,0xf9,0x87,0xff,0x00,
    0xe7,0xc1,0x83,0xc3,0xc1,0xc3,0xe7,0xf9,
    0x99,0xc3,0xf9,0x99,0xc3,0x9c,0x99,0xc3,
    0x9f,0xf9,0x9f,0x83,0xf1,0xc1,0xe7,0xc9,
    0x99,0xf3,0x81,0xc3,0xe7,0xf9,0xf7,0xef,

    0x00,0x18,0x03,0x00,0x18,0x18,0xc0,0x03,
    0xff,0x0f,0xff,0x00,0x00,0x00,0xff,0xf0,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x30,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x30,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7c,
    0x00,0x00,0x3c,0x00,0x00,0x00,0x00,0x00,
    0x60,0x06,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x78,0x00,0x00,0x18,0x00,0x00,0x00,
    0xff,0xe7,0xfc,0xff,0xe7,0xe7,0x3f,0xfc,
    0x00,0xf0,0x00,0xff,0xff,0xff,0x00,0x0f,
    0xff,0xe7,0xff,0xe7,0xff,0x00,0x3f,0xe7,
    0xff,0x0f,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xcf,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xcf,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x83,
    0xff,0xff,0xc3,0xff,0xff,0xff,0xff,0xff,
    0x9f,0xf9,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0x87,0xff,0xff,0xe7,0xff,0xff,0xff,
};

const char* _menu[] = {
    "GAMES",
    "SYSTEM",
    "MIDI",
    NULL
};

static inline uint8_t gui_font_row(uint8_t c, int line)
{
    if (c >= 32 && c <= 126)
        return g_gui_font_cga[(c - 32) * 8 + line];
    if (c >= 160 && c <= 254)
        return (uint8_t)~g_gui_font_cga[(c - 160) * 8 + line];
    return _font[c + line * 256];
}

enum {
    TAB_GAMES = 0,
    TAB_SYSTEM,
    TAB_MIDI,
    TAB_COUNT
};

enum {
    OUTPUT_MODE_MUSIC = 0,
    OUTPUT_MODE_GAMEPLAY
};

enum {
    SYSTEM_ROW_MODE = 0,
    SYSTEM_ROW_BT,
    SYSTEM_ROW_EMULATOR,
    SYSTEM_ROW_STANDARD,
    SYSTEM_ROW_GB_DISPLAY,
    SYSTEM_ROW_CV_A,
    SYSTEM_ROW_CV_B,
    SYSTEM_ROW_AUDIO_IN,
    SYSTEM_ROW_MENU_TRIGGER,
    SYSTEM_ROW_X_BUTTON,
    SYSTEM_ROW_Y_BUTTON,
    SYSTEM_ROW_PALETTE_GLITCH,
    SYSTEM_ROW_APU_GLITCH,
    SYSTEM_ROW_COUNT
};

enum {
    SYSTEM_SECTION_VIDEO = 0,
    SYSTEM_SECTION_AUDIO,
    SYSTEM_SECTION_CONTROLS,
    SYSTEM_SECTION_COUNT
};

enum SystemRowKind {
    SYSTEM_VISIBLE_VALUE = 0,
    SYSTEM_VISIBLE_SECTION
};

struct SystemVisibleRow {
    SystemRowKind kind;
    int id;
    int section;
};

enum {
    MIDI_SECTION_VIDEO = 0,
    MIDI_SECTION_AUDIO,
    MIDI_SECTION_GAMEPLAY,
    MIDI_SECTION_COUNT
};

enum {
    MIDI_VALUE_BUTTONS_CHANNEL = 0,
    MIDI_VALUE_AUTO_MODE,
    MIDI_VALUE_CC_GLITCH_A,
    MIDI_VALUE_CC_GLITCH_B,
    MIDI_VALUE_VISUALIZER_SPRITE_CC,
    MIDI_VALUE_PULSE1_CHAN,
    MIDI_VALUE_PULSE2_CHAN,
    MIDI_VALUE_TRIANGLE_CHAN,
    MIDI_VALUE_NOISE_CHAN,
    MIDI_VALUE_PULSE1_DUTY_CC,
    MIDI_VALUE_PULSE2_DUTY_CC,
    MIDI_VALUE_PITCH_BEND_RANGE,
    MIDI_VALUE_NOISE_SHORT_MODE_CC,
    MIDI_VALUE_WAVEFORM_CC,
    MIDI_VALUE_ENV_MODE_CC,
    MIDI_VALUE_ENV_DECAY_CC,
    MIDI_VALUE_ENV_LOOP_CC,
    MIDI_VALUE_LENGTH_CC,
    MIDI_VALUE_NOTE_UP,
    MIDI_VALUE_NOTE_DOWN,
    MIDI_VALUE_NOTE_LEFT,
    MIDI_VALUE_NOTE_RIGHT,
    MIDI_VALUE_NOTE_A,
    MIDI_VALUE_NOTE_B,
    MIDI_VALUE_NOTE_START,
    MIDI_VALUE_NOTE_SELECT,
    MIDI_VALUE_FX_CHANNEL
};

enum MidiRowKind {
    MIDI_ROW_SECTION = 0,
    MIDI_ROW_VALUE,
    MIDI_ROW_RESET
};

struct MidiVisibleRow {
    MidiRowKind kind;
    int id;
    int section;
};

static const int GUI_KEY_FACE_X = 1001;
static const int GUI_KEY_FACE_Y = 1002;
static const int GUI_KEY_INFO = 1003;
static const int GUI_KEY_LETTER_X = 27;
static const int GUI_KEY_LETTER_Y = 28;
static const int MUSIC_GAME_BOOT_GRACE_FRAMES = 45;
static const int REBOOT_POPUP_DELAY_FRAMES = 40;
static const int VISUALIZER_POPUP_DELAY_FRAMES = 40;
static const int REBOOT_POPUP_WIDTH = 20;
static const int REBOOT_POPUP_HEIGHT = 8;
static const int GB_REBOOT_POPUP_WIDTH = 13;
static const int GB_REBOOT_POPUP_HEIGHT = 6;
static const int GB_VISUALIZER_POPUP_WIDTH = 15;
static const int GB_VISUALIZER_POPUP_HEIGHT = 6;
static const int GUI_MENU_BACKBUFFER_MAX_HEIGHT = 240;
static const uint8_t kSelectedBracketLeft[8] = {
    0b00010000,
    0b00001000,
    0b00000100,
    0b00000000,
    0b00000000,
    0b00000100,
    0b00001000,
    0b00010000
};
#ifdef ESP_PLATFORM
static const size_t GUI_MENU_BACKBUFFER_HEAP_RESERVE = 8192;
// The game list lives in internal heap for the whole session. Past the first
// GUI_MEDIA_LIST_MIN_ENTRIES names, stop adding more once the heap gets this low
// so a large SD collection cannot exhaust (or abort) the emulator. Kept small:
// GB mode normally runs with only ~4 KB free.
static const size_t GUI_MEDIA_LIST_HEAP_RESERVE = 4096;
static const size_t GUI_MEDIA_LIST_LARGEST_RESERVE = 2048;
// The largest-block query walks every heap block, so only run it periodically.
static const size_t GUI_MEDIA_LIST_LARGEST_CHECK_INTERVAL = 16;
#endif
static const size_t GUI_MEDIA_LIST_MIN_ENTRIES = 64;

class Overlay {
public:
    uint8_t* _buf;
    uint32_t _blit[16];

    uint8_t** _lines;
    uint8_t** _fallback_lines;
    uint8_t* _fallback_pixels;
    int _width;
    int _height;
    int _hilite;
    int _flavor;
    int OVERLAY_WIDTH;
    int OVERLAY_HEIGHT;
    int OVERLAY_ORIGIN_X;
    int OVERLAY_ORIGIN_Y;
    uint8_t _fg_color;
    uint8_t _bg_color;

    Overlay() : _buf(0),_lines(0),_fallback_lines(0),_fallback_pixels(0),_width(0),_height(0),_hilite(0),_flavor(0),
                OVERLAY_WIDTH(0),OVERLAY_HEIGHT(0),OVERLAY_ORIGIN_X(0),OVERLAY_ORIGIN_Y(0),
                _fg_color(0xFF),_bg_color(0x00)
    {
    }

    void set_colors(uint8_t fg, uint8_t bg)
    {
        _fg_color = fg;
        _bg_color = bg;
        for (int i = 0; i < 16; i++) {
            uint32_t m = 0;
            for (int j = 0; j < 4; j++) {
                m <<= 8;
                if ((1 << j) & i)
                    m |= fg;
                else
                    m |= bg;
            }
            _blit[i] = m;
        }
    }

    void init(uint8_t** lines, int width, int height, int flavor)
    {
        _width = width;
        _height = height;
        _flavor = flavor;
        switch (_flavor) {
            case EMU_NES:
                OVERLAY_WIDTH = 28;
                OVERLAY_HEIGHT = 18;
                set_colors(0x23,0x0D);  // nes: purple text on black
                break;
            case EMU_GB:
                OVERLAY_WIDTH = 18;
                OVERLAY_HEIGHT = 16;
                set_colors(0xFF,0x00);
                break;
        }

        if (_flavor == EMU_NES) {
            OVERLAY_ORIGIN_X = 2;
            OVERLAY_ORIGIN_Y = 5;
        } else {
            OVERLAY_ORIGIN_X = (_width - OVERLAY_WIDTH * 8) >> 4;
            OVERLAY_ORIGIN_Y = (_height - OVERLAY_HEIGHT * 8) >> 4;
        }

        _buf = new uint8_t[OVERLAY_WIDTH*OVERLAY_HEIGHT];
        memset(_buf,0,OVERLAY_WIDTH*OVERLAY_HEIGHT);
        set_lines(lines);
    }

    void set_lines(uint8_t** lines)
    {
        if (lines) {
            release_fallback();
            _lines = lines;
            return;
        }

        if (!_fallback_pixels) {
            _fallback_pixels = new uint8_t[_width * _height];
            _fallback_lines = new uint8_t*[_height];
            for (int y = 0; y < _height; y++)
                _fallback_lines[y] = _fallback_pixels + (y * _width);
            memset(_fallback_pixels, _flavor == EMU_NES ? 0x0D : 0x00, _width * _height);
        }

        _lines = _fallback_lines;
    }

    void release_fallback()
    {
        if (_fallback_pixels) {
            delete[] _fallback_pixels;
            _fallback_pixels = 0;
        }
        if (_fallback_lines) {
            delete[] _fallback_lines;
            _fallback_lines = 0;
        }
    }

    void set_hilite(int c)
    {
        _hilite = c;
    }

    void draw_char(int c, int x, int y)
    {
        x *= 8;
        y *= 8;
        const uint8_t glyph = (uint8_t)c;
        for (int line = 0; line < 8; line++) {
            uint32_t* dst = (uint32_t*)(x + _lines[y++]);
            uint8_t b = gui_font_row(glyph, line);
            dst[0] = _blit[b >> 4]; // long words
            dst[1] = _blit[b & 0xF];
        }
    }

    void draw_char_scaled(int c, int x, int y, int scale)
    {
        if (scale <= 1) {
            draw_char(c, x, y);
            return;
        }
        int px = x * 8 * scale;
        int py = y * 8 * scale;
        const uint8_t glyph = (uint8_t)c;
        for (int gy = 0; gy < 8; ++gy) {
            const uint8_t bits = gui_font_row(glyph, gy);
            for (int sy = 0; sy < scale; ++sy) {
                const int dy = py + gy * scale + sy;
                if (dy < 0 || dy >= _height)
                    continue;
                uint8_t* dst = _lines[dy] + px;
                for (int gx = 0; gx < 8; ++gx) {
                    const uint8_t color = (bits & (0x80u >> gx)) ? _fg_color : _bg_color;
                    for (int sx = 0; sx < scale; ++sx) {
                        const int dx = px + gx * scale + sx;
                        if (dx >= 0 && dx < _width)
                            dst[gx * scale + sx] = color;
                    }
                }
            }
        }
    }

    // draw a note at bottom of screen
    void draw_msg(const string& msg)
    {
        int n = (int)msg.size();
        for (int i = 0; i < n; i++) {
            int x = i - n/2 + _width/16;
            if (x >= _width)
                break;
            if (x >= 0)
                draw_char(msg[i],x,_height/8-2);
        }
    }

    void erase_msg()
    {
        const uint8_t clear = (_flavor == EMU_NES) ? 0x0D : 0x00;
        for (int i = _height-16; i < _height-8; i++)
        {
            uint32_t* d = (uint32_t*)_lines[i];
            uint32_t fill = ((uint32_t)clear << 24) |
                            ((uint32_t)clear << 16) |
                            ((uint32_t)clear << 8) |
                            (uint32_t)clear;
            int n = _width/4;
            while (n--)
                *d++ = fill;
        }
    }

    uint8_t clear_color() const
    {
        return (_flavor == EMU_NES) ? 0x0D : 0x00;
    }

    void clear_screen()
    {
        if (!_lines)
            return;
        const uint8_t clear = clear_color();
        for (int y = 0; y < _height; ++y) {
            if (_lines[y])
                memset(_lines[y], clear, (size_t)_width);
        }
    }

    void draw_center_msg(const char* msg, int scale = 1)
    {
        if (!_lines || !msg)
            return;
        if (scale < 1)
            scale = 1;
        const int len = (int)strlen(msg);
        const int cols = _width / (8 * scale);
        const int rows = _height / (8 * scale);
        const int x = max(0, (cols - len) / 2);
        const int y = max(0, (rows - 1) / 2);
        for (int i = 0; i < len && x + i < cols; ++i)
            draw_char_scaled(msg[i], x + i, y, scale);
    }

    void draw_center_bar_under_msg(const char* msg, int scale = 1, int percent = 0)
    {
        if (!_lines || !msg)
            return;
        if (scale < 1)
            scale = 1;
        if (percent < 0)
            percent = 0;
        if (percent > 100)
            percent = 100;

        const int len = (int)strlen(msg);
        const int text_w = len * 8 * scale;
        const int bar_w = min(_width - 16, text_w);
        if (bar_w <= 0)
            return;

        const int rows = _height / (8 * scale);
        const int text_y = max(0, (rows - 1) / 2) * 8 * scale;
        const int bar_x = max(0, (_width - bar_w) / 2);
        const int bar_y = min(_height - scale, text_y + 8 * scale + 6);
        const uint8_t color = (_flavor == EMU_NES) ? 0x30 : _fg_color;
        const uint8_t clear = clear_color();
        int fill_w = (bar_w * percent) / 100;
        if (percent > 0 && fill_w < 1)
            fill_w = 1;
        for (int y = 0; y < scale; ++y) {
            uint8_t* row = _lines[bar_y + y] + bar_x;
            memset(row, clear, (size_t)bar_w);
            if (fill_w > 0)
                memset(row, color, (size_t)fill_w);
        }
    }


    // 32 bit aligned access
    inline uint8_t getp(const uint8_t* src)
    {
        size_t s = (size_t)src;
        return *((uint32_t*)(s & ~3)) >> ((s & 3) << 3);
    }

    inline void setp(uint8_t* dst, uint8_t p)
    {
        size_t s = (size_t)dst;
        size_t shift = (s & 3) << 3;
        uint32_t* d = (uint32_t*)(s & ~3);
        d[0] = (d[0] & ~(0xFF << shift)) | (p << shift);
    }

    inline void luma(uint8_t* dst, int c)
    {
        uint8_t p = getp(dst);
        uint8_t r,g,b;
        switch (_flavor) {
            case EMU_NES:
                b = (((p >> 4) & 3) * c) >> 7;
                if (b > 3) b = 3;
                p = (b << 4) | (p & 0xCF);  // nes luma adjust
                break;
            case EMU_GB:
                r = ((p & 0xE0) * c >> 7) & 0xE0;
                g = ((p & 0x1C) * c >> 7) & 0x1C;
                b = ((p & 0x03) * c >> 7) & 0x03;
                p = r | g | b;
                break;
        }
        setp(dst,p);
    }

    void hline(int c, int x0, int x1, int y)
    {
        uint8_t* dst = _lines[y] + x0;
        if (c < 0) {
            for (int x = x0; x < x1; x++)
                 luma(dst++,-c);
        } else {
            for (int x = x0; x < x1; x++)
                 setp(dst++,c);
        }
    }

    void vline(int c, int x, int y0, int y1)
    {
        if (c < 0) {
            for (int y = y0; y < y1; y++)
                 luma(_lines[y] + x,-c);
        } else {
            for (int y = y0; y < y1; y++)
                 setp(_lines[y] + x,c);
        }
    }

    void frame()
    {
        int left = OVERLAY_ORIGIN_X * 8;
        int top = OVERLAY_ORIGIN_Y * 8;
        int bottom = top + OVERLAY_HEIGHT*8;
        int right = left + OVERLAY_WIDTH*8;

        for (int i = 0; i < 4; i++) {
            int c = -(i*32 + 16);
            hline(c,left+i-1,right+i+1,bottom+i);
            vline(c,right+i,top+i,bottom+i);
        }
        hline(-32,left-1,right+1,top-1);
        vline(-32,left-1,top-1,bottom+1);
    }

    void update()
    {
        update_from_row(0);
    }

    void update_from_row(int first_row)
    {
        if (first_row < 0)
            first_row = 0;
        if (first_row >= OVERLAY_HEIGHT)
            return;
        uint8_t* s = _buf + first_row * OVERLAY_WIDTH;
        for (int y = first_row; y < OVERLAY_HEIGHT; y++) {
            for (int x = 0; x < OVERLAY_WIDTH; x++) {
                draw_char(*s++,x+OVERLAY_ORIGIN_X,y+OVERLAY_ORIGIN_Y);
            }
        }
    }

    void update_row(int row)
    {
        if (row < 0 || row >= OVERLAY_HEIGHT)
            return;
        uint8_t* s = _buf + row * OVERLAY_WIDTH;
        for (int x = 0; x < OVERLAY_WIDTH; x++)
            draw_char(*s++,x+OVERLAY_ORIGIN_X,row+OVERLAY_ORIGIN_Y);
    }

    void plot_char(int c, int x, int y)
    {
        if (x < 0 || x >= OVERLAY_WIDTH)   //
            return;
        if (y < 0 || y >= OVERLAY_HEIGHT)
            return;
        _buf[x + y*OVERLAY_WIDTH] = c + (_hilite << 7);
    }

    void plot_str(const char* t, int x, int y)
    {
        int len = (int)strlen(t);
        for (int i = 0; i < len; i++)
            plot_char((uint8_t)*t++,x++,y);
    }

    void fill(int y, int x0, int x1, int c)
    {
        for (int x = x0; x < x1; x++)
            plot_char(c,x,y);
    }
};

string get_ext(const string& s);
static uint8_t** gui_base_video_buffer();
static uint8_t** gui_display_video_buffer();
static void gui_reset_visualizer_buffers();
static bool gui_render_visualizer_frame();
static bool gui_has_menu_backdrop(const Emu* emu);
static void gui_draw_menu_backdrop(uint8_t** lines, const Emu* emu,
                                   int skip_y0 = 0, int skip_y1 = 0);
// Wrap value into [lo, hi].
static int wrap_range(int value, int lo, int hi)
{
    if (lo > hi)
        return lo;
    const int span = hi - lo + 1;
    int offset = (value - lo) % span;
    if (offset < 0)
        offset += span;
    return lo + offset;
}

static const char* NES_DEFAULT_ROM = "Sliding Blaster.nes";
static const char* GB_DEFAULT_ROM = "Postie.gbc";

static void gui_restart_device()
{
#ifdef ESP_PLATFORM
    esp_restart();
#else
    printf("Restart requested\n");
#endif
}

class GUI {
public:

    string _path;
    string _browse_dir;     // open sub-folder; empty = top-level game list
    string _insert_path;
    vector<string> _files;
    vector<string> _info;
    int _tab_hilited[TAB_COUNT];
    int _tab_scroll[TAB_COUNT];
    int _hilited;
    int _scroll;
    int _tab;
    int _visible;
    bool _game_loaded;
    bool _dirty;
    bool _live_partial_dirty;
    bool _live_scrollbar_dirty;
    uint32_t _live_dirty_rows;
    Emu* _emu;
    Overlay* _overlay;

    // Key repeat for Up/Down navigation
    int _rep_key;         // 82=up, 81=down (SDL scancodes used here)
    int _rep_dir;         // -1 for up, +1 for down, 0=none
    int _rep_delay;       // frames until first repeat
    int _rep_rate;        // frames between repeats
    uint32_t _ui_tick_frame;

    string _msg;
    uint32_t _msg_ticks;
    bool _loading_active;
    bool _loading_commit_pending;
    uint8_t** _loading_lines;
    uint8_t* _loading_pixels;
    uint8_t* _loading_line_storage[GUI_MENU_BACKBUFFER_MAX_HEIGHT];
    int _loading_buffer_width;
    int _loading_buffer_height;
    int _loading_progress;
    int _loading_scale;
    // MIDI reset confirmation state
    bool _midi_confirm;
    bool _midi_confirm_yes;
    bool _info_popup;
    bool _reboot_popup;
    bool _reboot_popup_yes;
    int _reboot_popup_delay;
    bool _visualizer_popup;
    bool _visualizer_popup_yes;
    int _visualizer_popup_delay;
    bool _music_visualizer_declined;
    bt_transport_mode_t _system_bt_transport;
    int _system_emulator;
    int _system_standard;
    bt_transport_mode_t _reboot_base_bt_transport;
    int _reboot_base_standard;
    int _system_gb_display_fullscreen;
    bool _menu_start_select_trigger;
    int _visualizer_mode;
    int _output_mode;
    int _pending_output_mode;
    uint8_t* _menu_pixels[2];
    uint8_t** _menu_lines[2];
    uint8_t* _menu_line_storage[2][GUI_MENU_BACKBUFFER_MAX_HEIGHT];
    bool _menu_lines_owned[2];
    bool _menu_pixels_chunked[2];
    int _menu_buffer_width;
    int _menu_buffer_height;
    int _menu_front_buffer;
    bool _menu_front_valid;
    bool _menu_alloc_failed;
    bool _visualizer_commit_pending;
    bool _system_section_open[SYSTEM_SECTION_COUNT];
    bool _midi_section_open[MIDI_SECTION_COUNT];
    int _music_game_boot_grace;
    int _marquee_tab;
    int _marquee_hilited;
    int _marquee_frames;
    int _marquee_offset;

    GUI() : _hilited(0),_tab(0),_visible(0),_game_loaded(false),_dirty(true),
             _live_partial_dirty(false),_live_scrollbar_dirty(false),_live_dirty_rows(0),
             _emu(0),
             _rep_key(0),_rep_dir(0),_rep_delay(0),_rep_rate(0),
             _ui_tick_frame(UINT32_MAX),
             _midi_confirm(false),_midi_confirm_yes(false),
             _loading_active(false),_loading_commit_pending(false),_loading_lines(0),
             _loading_pixels(0),_loading_buffer_width(0),_loading_buffer_height(0),
             _loading_progress(0),_loading_scale(1),
             _info_popup(false),_reboot_popup(false),_reboot_popup_yes(false),
             _reboot_popup_delay(0),_visualizer_popup(false),_visualizer_popup_yes(true),
             _visualizer_popup_delay(0),_music_visualizer_declined(false),
             _system_bt_transport(BT_TRANSPORT_CLASSIC_ONLY),
             _system_emulator(EMU_NES),_system_standard(PAL),
             _reboot_base_bt_transport(BT_TRANSPORT_CLASSIC_ONLY),
             _reboot_base_standard(PAL),
             _system_gb_display_fullscreen(1),_menu_start_select_trigger(false),
             _visualizer_mode(0),
             _output_mode(OUTPUT_MODE_GAMEPLAY),_pending_output_mode(-1),
             _menu_buffer_width(0),_menu_buffer_height(0),_menu_front_buffer(0),_menu_front_valid(false),
             _menu_alloc_failed(false),_visualizer_commit_pending(false),
             _music_game_boot_grace(0),
             _marquee_tab(-1),_marquee_hilited(-1),_marquee_frames(0),
             _marquee_offset(0)
    {
        _menu_pixels[0] = _menu_pixels[1] = 0;
        _menu_lines[0] = _menu_lines[1] = 0;
        _menu_lines_owned[0] = _menu_lines_owned[1] = false;
        _menu_pixels_chunked[0] = _menu_pixels_chunked[1] = false;
        memset(_menu_line_storage, 0, sizeof(_menu_line_storage));
        memset(_loading_line_storage, 0, sizeof(_loading_line_storage));
        for (int i = 0; i < TAB_COUNT; i++) {
            _tab_hilited[i] = 0;
            _tab_scroll[i] = 0;
        }
        for (int i = 0; i < SYSTEM_SECTION_COUNT; ++i)
            _system_section_open[i] = false;
        for (int i = 0; i < MIDI_SECTION_COUNT; ++i)
            _midi_section_open[i] = false;
    }

    void msg(const char* m)
    {
        _msg = m;
        _msg_ticks = 120;
        _dirty = true;
    }

    void clear_loading_msg_after_insert()
    {
        if (_msg == "LOADING...") {
            _msg.clear();
            _msg_ticks = 0;
            _dirty = true;
        }
    }

    uint8_t** loading_lines() const
    {
        return (_loading_active && _loading_lines) ? _loading_lines : 0;
    }

    bool loading_commit_pending() const
    {
        return _loading_commit_pending;
    }

    void release_loading_buffer()
    {
        free(_loading_pixels);
        _loading_pixels = 0;
        _loading_buffer_width = 0;
        _loading_buffer_height = 0;
        memset(_loading_line_storage, 0, sizeof(_loading_line_storage));
    }

    uint8_t** loading_buffer_lines()
    {
        if (!_emu || _emu->height > GUI_MENU_BACKBUFFER_MAX_HEIGHT)
            return 0;

        if (_loading_pixels &&
            (_loading_buffer_width != _emu->width ||
             _loading_buffer_height != _emu->height)) {
            release_loading_buffer();
        }

        if (!_loading_pixels) {
            const size_t pixel_bytes = (size_t)_emu->width * (size_t)_emu->height;
#ifdef ESP_PLATFORM
            _loading_pixels = (uint8_t*)heap_caps_malloc(pixel_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#else
            _loading_pixels = (uint8_t*)malloc(pixel_bytes);
#endif
            if (!_loading_pixels)
                return 0;
            _loading_buffer_width = _emu->width;
            _loading_buffer_height = _emu->height;
        }

        for (int y = 0; y < _emu->height; ++y)
            _loading_line_storage[y] = _loading_pixels + ((size_t)y * (size_t)_emu->width);
        return _loading_line_storage;
    }

    static void loading_progress_callback(int percent, void* user)
    {
        if (user)
            static_cast<GUI*>(user)->set_loading_progress(percent);
    }

    void set_loading_progress(int percent)
    {
        if (percent < 0)
            percent = 0;
        if (percent > 100)
            percent = 100;
        if (!_loading_active || !_overlay || !_loading_lines || _loading_progress == percent)
            return;
        _loading_progress = percent;
        _overlay->draw_center_bar_under_msg(_msg.c_str(), _loading_scale, _loading_progress);
    }

    void begin_loading_screen()
    {
        loading_progress_clear_callback();
        if (!_overlay)
            return;

        _msg = "LOADING...";
        _msg_ticks = 120;
        _dirty = true;
        _loading_progress = 0;

        uint8_t** lines = loading_buffer_lines();
        if (!lines)
            lines = _overlay->_lines;
        if (!lines)
            lines = gui_display_video_buffer();
        if (!lines && _emu)
            lines = _emu->video_buffer();

        if (!lines)
            return;

        if (_overlay->_lines != lines)
            _overlay->set_lines(lines);
        _overlay->clear_screen();
        _loading_scale = (_emu && _emu->flavor == EMU_NES) ? 2 : 1;
        _overlay->draw_center_msg(_msg.c_str(), _loading_scale);
        _overlay->draw_center_bar_under_msg(_msg.c_str(), _loading_scale, _loading_progress);
        _loading_lines = _overlay->_lines;
        _loading_active = (_loading_lines != 0);
        _loading_commit_pending = false;
        if (_loading_active) {
            loading_progress_set_callback(loading_progress_callback, this);
            video_clear_queued_lines();
            _lines = _loading_lines;
        }
    }

    void end_loading_screen()
    {
        set_loading_progress(100);
        loading_progress_clear_callback();
        _loading_commit_pending = (_loading_lines != 0);
        _loading_active = false;
        _loading_lines = 0;
        clear_loading_msg_after_insert();
    }

    void finish_loading_commit()
    {
        _loading_commit_pending = false;
        release_loading_buffer();
    }

    void clear_live_partial_dirty()
    {
        _live_partial_dirty = false;
        _live_scrollbar_dirty = false;
        _live_dirty_rows = 0;
    }

    bool live_ntsc_menu_fallback() const
    {
        return use_menu_backbuffer() && _menu_alloc_failed;
    }

    void mark_live_dirty_row(int row)
    {
        if (row < 0 || row >= 32)
            return;
        _dirty = true;
        _live_partial_dirty = true;
        _live_dirty_rows |= (1u << row);
    }

    void mark_live_dirty_item_area()
    {
        for (int row = 4; row < _overlay->OVERLAY_HEIGHT - 2 && row < 32; ++row)
            mark_live_dirty_row(row);
    }

    void mark_live_dirty_scrollbar()
    {
        _dirty = true;
        _live_partial_dirty = true;
        _live_scrollbar_dirty = true;
    }

    void mark_current_adjust_dirty()
    {
        if (live_ntsc_menu_fallback() && !modal_popup_open()) {
            mark_live_dirty_row(item_y(_hilited));
        } else {
            _dirty = true;
        }
    }

    void force_full_menu_redraw()
    {
        _dirty = true;
        clear_live_partial_dirty();
    }

    int selected_visualizer_mode() const
    {
        int count = midi_visualizer_mode_count();
        if (count <= 0)
            return -1;
        int mode = _visualizer_mode;
        if (mode < 0 || mode >= count)
            mode = 0;
        return mode;
    }

    bool is_music_mode() const
    {
        return _output_mode == OUTPUT_MODE_MUSIC;
    }

    bool use_gb_apu_midi() const
    {
        return _emu && _emu->flavor == EMU_GB;
    }

    void apu_midi_disable_all()
    {
        nes_apu_midi_enable(0);
        gb_apu_midi_enable(0);
    }

    void apu_midi_enable_current(int enable)
    {
        if (use_gb_apu_midi()) {
            nes_apu_midi_enable(0);
            gb_apu_midi_enable(enable);
        } else {
            gb_apu_midi_enable(0);
            // Prepare the standalone NES APU before music mode activates.
            if (enable && _emu)
                _emu->prepare_music_mode_audio();
            nes_apu_midi_enable(enable);
        }
    }

    int apu_midi_current_enabled() const
    {
        return use_gb_apu_midi() ? gb_apu_midi_is_enabled() :
               nes_apu_midi_is_enabled();
    }

    bool apu_midi_cc_is_sound_control(uint8_t cc) const
    {
        if (use_gb_apu_midi())
            return gb_apu_midi_cc_is_sound_control(cc) != 0;
        return nes_apu_midi_cc_is_sound_control(cc) != 0;
    }

    void apu_midi_save_prefs()
    {
        nes_apu_midi_save_prefs();
        gb_apu_midi_save_prefs();
    }

    void apu_midi_get_channel_map(int8_t* a, int8_t* b, int8_t* c, int8_t* d) const
    {
        if (use_gb_apu_midi()) {
            gb_apu_midi_get_channel_map(a, b, c, d);
        } else {
            nes_apu_midi_get_channel_map(a, b, c, d);
        }
    }

    void apu_midi_set_channel_map(int8_t a, int8_t b, int8_t c, int8_t d)
    {
        if (use_gb_apu_midi()) {
            gb_apu_midi_set_channel_map(a, b, c, d);
        } else {
            nes_apu_midi_set_channel_map(a, b, c, d);
        }
    }

    void apu_midi_get_duty_cc(int8_t* a, int8_t* b) const
    {
        if (use_gb_apu_midi()) {
            gb_apu_midi_get_duty_cc(a, b);
        } else {
            nes_apu_midi_get_duty_cc(a, b);
        }
    }

    void apu_midi_set_duty_cc(int8_t a, int8_t b)
    {
        if (use_gb_apu_midi()) {
            gb_apu_midi_set_duty_cc(a, b);
        } else {
            nes_apu_midi_set_duty_cc(a, b);
        }
    }

    uint8_t apu_midi_get_pb_default_range() const
    {
        if (use_gb_apu_midi())
            return gb_apu_midi_get_pb_default_range();
        return nes_apu_midi_get_pb_default_range();
    }

    void apu_midi_set_pb_default_range(uint8_t semis)
    {
        if (use_gb_apu_midi())
            gb_apu_midi_set_pb_default_range(semis);
        else
            nes_apu_midi_set_pb_default_range(semis);
    }

    void apu_midi_get_noise_mode_cc(int8_t* cc) const
    {
        if (use_gb_apu_midi()) {
            gb_apu_midi_get_noise_mode_cc(cc);
        } else {
            nes_apu_midi_get_noise_mode_cc(cc);
        }
    }

    void apu_midi_set_noise_mode_cc(int8_t cc)
    {
        if (use_gb_apu_midi()) {
            gb_apu_midi_set_noise_mode_cc(cc);
        } else {
            nes_apu_midi_set_noise_mode_cc(cc);
        }
    }

    void gb_midi_get_waveform_cc(int8_t* cc) const
    {
        if (use_gb_apu_midi()) {
            gb_apu_midi_get_waveform_cc(cc);
        } else if (cc) {
            *cc = -1;
        }
    }

    void gb_midi_set_waveform_cc(int8_t cc)
    {
        if (use_gb_apu_midi())
            gb_apu_midi_set_waveform_cc(cc);
    }

    void apu_midi_get_env_cc(int8_t* mode, int8_t* decay, int8_t* loop, int8_t* length) const
    {
        if (use_gb_apu_midi()) {
            gb_apu_midi_get_env_cc(mode, decay, loop, length);
        } else {
            nes_apu_midi_get_env_cc(mode, decay, loop, length);
        }
    }

    void apu_midi_set_env_cc(int8_t mode, int8_t decay, int8_t loop, int8_t length)
    {
        if (use_gb_apu_midi()) {
            gb_apu_midi_set_env_cc(mode, decay, loop, length);
        } else {
            nes_apu_midi_set_env_cc(mode, decay, loop, length);
        }
    }

    int selected_output_mode() const
    {
        return _pending_output_mode >= 0 ? _pending_output_mode : _output_mode;
    }

    int output_mode_option_index() const
    {
        return selected_output_mode() == OUTPUT_MODE_MUSIC ? 1 : 0;
    }

    const char* output_mode_option_label(int index) const
    {
        return index == 0 ? "Gameplay" : "Music";
    }

    const char* cv_assignment_label(uint8_t assignment) const
    {
        switch (assignment) {
            case GLITCH_CV_ASSIGN_PPU:       return "Glitch A";
            case GLITCH_CV_ASSIGN_CORRUPTION:       return "Glitch B";
            case GLITCH_CV_ASSIGN_BUTTON_LEFT:   return "Left";
            case GLITCH_CV_ASSIGN_BUTTON_RIGHT:  return "Right";
            case GLITCH_CV_ASSIGN_BUTTON_UP:     return "Up";
            case GLITCH_CV_ASSIGN_BUTTON_DOWN:   return "Down";
            case GLITCH_CV_ASSIGN_BUTTON_A:      return "A";
            case GLITCH_CV_ASSIGN_BUTTON_B:      return "B";
            case GLITCH_CV_ASSIGN_BUTTON_X:      return "X";
            case GLITCH_CV_ASSIGN_BUTTON_Y:      return "Y";
            case GLITCH_CV_ASSIGN_BUTTON_START:  return "Start";
            case GLITCH_CV_ASSIGN_BUTTON_SELECT: return "Select";
            case GLITCH_CV_ASSIGN_PALETTE:       return "Palette";
        }
        return "Glitch A";
    }

    const char* audio_assignment_label(uint8_t assignment) const
    {
        return assignment == GLITCH_AUDIO_ASSIGN_CORRUPTION ? "Glitch B" : "Glitch A";
    }

    const char* system_section_title(int section) const
    {
        switch (section) {
            case SYSTEM_SECTION_VIDEO: return "Video";
            case SYSTEM_SECTION_AUDIO: return "Audio";
            case SYSTEM_SECTION_CONTROLS: return "Controls";
        }
        return "";
    }

    int system_section_item_count(int section) const
    {
        switch (section) {
            case SYSTEM_SECTION_VIDEO: return is_gb_gui() ? 3 : 2;
            case SYSTEM_SECTION_AUDIO: return 2;
            case SYSTEM_SECTION_CONTROLS: return 5;
        }
        return 0;
    }

    int system_section_item_id(int section, int offset) const
    {
        switch (section) {
            case SYSTEM_SECTION_VIDEO:
                if (offset == 0)
                    return SYSTEM_ROW_STANDARD;
                if (is_gb_gui()) {
                    if (offset == 1)
                        return SYSTEM_ROW_GB_DISPLAY;
                    if (offset == 2)
                        return SYSTEM_ROW_PALETTE_GLITCH;
                } else if (offset == 1) {
                    return SYSTEM_ROW_PALETTE_GLITCH;
                }
                break;
            case SYSTEM_SECTION_AUDIO:
                if (offset == 0)
                    return SYSTEM_ROW_AUDIO_IN;
                if (offset == 1)
                    return SYSTEM_ROW_APU_GLITCH;
                break;
            case SYSTEM_SECTION_CONTROLS:
                switch (offset) {
                    case 0: return SYSTEM_ROW_MENU_TRIGGER;
                    case 1: return SYSTEM_ROW_CV_A;
                    case 2: return SYSTEM_ROW_CV_B;
                    case 3: return SYSTEM_ROW_X_BUTTON;
                    case 4: return SYSTEM_ROW_Y_BUTTON;
                }
                break;
        }
        return -1;
    }

    const char* midi_section_title(int section) const
    {
        switch (section) {
            case MIDI_SECTION_GAMEPLAY: return "Gameplay MIDI";
            case MIDI_SECTION_VIDEO: return "Video MIDI";
            case MIDI_SECTION_AUDIO: return "Audio MIDI";
        }
        return "";
    }

    int midi_section_item_count(int section) const
    {
        switch (section) {
            case MIDI_SECTION_GAMEPLAY: return 9;
            case MIDI_SECTION_VIDEO: return 5;
            case MIDI_SECTION_AUDIO: return use_gb_apu_midi() ? 13 : 12;
        }
        return 0;
    }

    int midi_section_item_id(int section, int offset) const
    {
        switch (section) {
            case MIDI_SECTION_GAMEPLAY:
                switch (offset) {
                    case 0: return MIDI_VALUE_BUTTONS_CHANNEL;
                    case 1: return MIDI_VALUE_NOTE_UP;
                    case 2: return MIDI_VALUE_NOTE_DOWN;
                    case 3: return MIDI_VALUE_NOTE_LEFT;
                    case 4: return MIDI_VALUE_NOTE_RIGHT;
                    case 5: return MIDI_VALUE_NOTE_A;
                    case 6: return MIDI_VALUE_NOTE_B;
                    case 7: return MIDI_VALUE_NOTE_START;
                    case 8: return MIDI_VALUE_NOTE_SELECT;
                }
                break;
            case MIDI_SECTION_VIDEO:
                switch (offset) {
                    case 0: return MIDI_VALUE_FX_CHANNEL;
                    case 1: return MIDI_VALUE_AUTO_MODE;
                    case 2: return MIDI_VALUE_CC_GLITCH_A;
                    case 3: return MIDI_VALUE_CC_GLITCH_B;
                    case 4: return MIDI_VALUE_VISUALIZER_SPRITE_CC;
                }
                break;
            case MIDI_SECTION_AUDIO:
                if (use_gb_apu_midi()) {
                    switch (offset) {
                        case 0: return MIDI_VALUE_PULSE1_CHAN;
                        case 1: return MIDI_VALUE_PULSE2_CHAN;
                        case 2: return MIDI_VALUE_TRIANGLE_CHAN;
                        case 3: return MIDI_VALUE_NOISE_CHAN;
                        case 4: return MIDI_VALUE_PULSE1_DUTY_CC;
                        case 5: return MIDI_VALUE_PULSE2_DUTY_CC;
                        case 6: return MIDI_VALUE_WAVEFORM_CC;
                        case 7: return MIDI_VALUE_NOISE_SHORT_MODE_CC;
                        case 8: return MIDI_VALUE_ENV_MODE_CC;
                        case 9: return MIDI_VALUE_ENV_DECAY_CC;
                        case 10: return MIDI_VALUE_ENV_LOOP_CC;
                        case 11: return MIDI_VALUE_LENGTH_CC;
                        case 12: return MIDI_VALUE_PITCH_BEND_RANGE;
                    }
                } else {
                    switch (offset) {
                        case 0: return MIDI_VALUE_PULSE1_CHAN;
                        case 1: return MIDI_VALUE_PULSE2_CHAN;
                        case 2: return MIDI_VALUE_TRIANGLE_CHAN;
                        case 3: return MIDI_VALUE_NOISE_CHAN;
                        case 4: return MIDI_VALUE_PULSE1_DUTY_CC;
                        case 5: return MIDI_VALUE_PULSE2_DUTY_CC;
                        case 6: return MIDI_VALUE_NOISE_SHORT_MODE_CC;
                        case 7: return MIDI_VALUE_ENV_MODE_CC;
                        case 8: return MIDI_VALUE_ENV_DECAY_CC;
                        case 9: return MIDI_VALUE_ENV_LOOP_CC;
                        case 10: return MIDI_VALUE_LENGTH_CC;
                        case 11: return MIDI_VALUE_PITCH_BEND_RANGE;
                    }
                }
                break;
        }
        return -1;
    }

    int midi_visible_count() const
    {
        int total = MIDI_SECTION_COUNT + 1; // section headers + reset row
        for (int s = 0; s < MIDI_SECTION_COUNT; ++s) {
            if (_midi_section_open[s])
                total += midi_section_item_count(s);
        }
        return total;
    }

    bool midi_row_at(int index, MidiVisibleRow* out) const
    {
        int row = 0;
        for (int s = 0; s < MIDI_SECTION_COUNT; ++s) {
            if (index == row) {
                if (out) {
                    out->kind = MIDI_ROW_SECTION;
                    out->id = s;
                    out->section = s;
                }
                return true;
            }
            ++row;
            if (_midi_section_open[s]) {
                const int item_count = midi_section_item_count(s);
                for (int i = 0; i < item_count; ++i, ++row) {
                    if (index == row) {
                        if (out) {
                            out->kind = MIDI_ROW_VALUE;
                            out->id = midi_section_item_id(s, i);
                            out->section = s;
                        }
                        return true;
                    }
                }
            }
        }
        if (index == row) {
            if (out) {
                out->kind = MIDI_ROW_RESET;
                out->id = -1;
                out->section = -1;
            }
            return true;
        }
        return false;
    }

    int midi_section_header_index(int target_section) const
    {
        int row = 0;
        for (int s = 0; s < MIDI_SECTION_COUNT; ++s) {
            if (s == target_section)
                return row;
            ++row;
            if (_midi_section_open[s])
                row += midi_section_item_count(s);
        }
        return 0;
    }

    bool midi_selected_adjustable() const
    {
        MidiVisibleRow row = {};
        return midi_row_at(_hilited, &row) && row.kind == MIDI_ROW_VALUE;
    }

    bool midi_value_can_adjust(int idx) const
    {
        return idx >= MIDI_VALUE_BUTTONS_CHANNEL && idx <= MIDI_VALUE_FX_CHANNEL;
    }

    void close_midi_section(int section)
    {
        if (section < 0 || section >= MIDI_SECTION_COUNT || !_midi_section_open[section])
            return;
        MidiVisibleRow row = {};
        const bool had_row = midi_row_at(_hilited, &row);
        _midi_section_open[section] = false;
        if (had_row && row.section == section)
            _hilited = midi_section_header_index(section);
        clear_repeat();
        force_full_menu_redraw();
    }

    void toggle_midi_section(int section)
    {
        if (section < 0 || section >= MIDI_SECTION_COUNT)
            return;
        if (_midi_section_open[section]) {
            close_midi_section(section);
        } else {
            _midi_section_open[section] = true;
            clear_repeat();
            _dirty = true;
        }
    }

    bool midi_back()
    {
        MidiVisibleRow row = {};
        if (!midi_row_at(_hilited, &row))
            return false;
        if (row.section >= 0 && _midi_section_open[row.section]) {
            close_midi_section(row.section);
            return true;
        }
        return false;
    }

    bool system_row_adjustable(int row) const
    {
        return row == SYSTEM_ROW_MODE || row == SYSTEM_ROW_BT ||
               row == SYSTEM_ROW_EMULATOR ||
               row == SYSTEM_ROW_STANDARD ||
               row == SYSTEM_ROW_MENU_TRIGGER ||
               row == SYSTEM_ROW_CV_A || row == SYSTEM_ROW_CV_B ||
               row == SYSTEM_ROW_AUDIO_IN ||
               row == SYSTEM_ROW_X_BUTTON ||
               row == SYSTEM_ROW_Y_BUTTON ||
               row == SYSTEM_ROW_PALETTE_GLITCH ||
               row == SYSTEM_ROW_APU_GLITCH ||
               (row == SYSTEM_ROW_GB_DISPLAY && is_gb_gui());
    }

    int system_visible_count() const
    {
        int total = 3 + SYSTEM_SECTION_COUNT;
        for (int section = 0; section < SYSTEM_SECTION_COUNT; ++section) {
            if (_system_section_open[section])
                total += system_section_item_count(section);
        }
        return total;
    }

    bool system_visible_row_at(int index, SystemVisibleRow* out) const
    {
        if (index == 0 || index == 1 || index == 2) {
            if (out) {
                out->kind = SYSTEM_VISIBLE_VALUE;
                out->id = index == 0 ? SYSTEM_ROW_MODE : (index == 1 ? SYSTEM_ROW_EMULATOR : SYSTEM_ROW_BT);
                out->section = -1;
            }
            return true;
        }

        int row = 3;
        for (int section = 0; section < SYSTEM_SECTION_COUNT; ++section) {
            if (index == row) {
                if (out) {
                    out->kind = SYSTEM_VISIBLE_SECTION;
                    out->id = section;
                    out->section = section;
                }
                return true;
            }
            ++row;
            if (_system_section_open[section]) {
                const int item_count = system_section_item_count(section);
                for (int item = 0; item < item_count; ++item, ++row) {
                    if (index == row) {
                        if (out) {
                            out->kind = SYSTEM_VISIBLE_VALUE;
                            out->id = system_section_item_id(section, item);
                            out->section = section;
                        }
                        return true;
                    }
                }
            }
        }
        return false;
    }

    int system_hilited_row() const
    {
        SystemVisibleRow row = {};
        if (!system_visible_row_at(_hilited, &row) || row.kind != SYSTEM_VISIBLE_VALUE)
            return -1;
        return row.id;
    }

    bool system_selected_adjustable() const
    {
        return system_row_adjustable(system_hilited_row());
    }

    int system_section_header_index(int target_section) const
    {
        int row = 3;
        for (int section = 0; section < SYSTEM_SECTION_COUNT; ++section) {
            if (section == target_section)
                return row;
            ++row;
            if (_system_section_open[section])
                row += system_section_item_count(section);
        }
        return 2;
    }

    void close_system_section(int section)
    {
        if (section < 0 || section >= SYSTEM_SECTION_COUNT || !_system_section_open[section])
            return;
        SystemVisibleRow row = {};
        const bool had_row = system_visible_row_at(_hilited, &row);
        _system_section_open[section] = false;
        if (had_row && row.section == section)
            _hilited = system_section_header_index(section);
        clear_repeat();
        force_full_menu_redraw();
    }

    void toggle_system_section(int section)
    {
        if (section < 0 || section >= SYSTEM_SECTION_COUNT)
            return;
        if (_system_section_open[section]) {
            close_system_section(section);
        } else {
            _system_section_open[section] = true;
            clear_repeat();
            _dirty = true;
        }
    }

    bool system_back()
    {
        SystemVisibleRow row = {};
        if (!system_visible_row_at(_hilited, &row))
            return false;
        if (row.section >= 0 && _system_section_open[row.section]) {
            close_system_section(row.section);
            return true;
        }
        return false;
    }

    int active_emulator_setting() const
    {
        return (_emu && _emu->flavor == EMU_GB) ? EMU_GB : EMU_NES;
    }

    int active_standard_setting() const
    {
        return (_emu && _emu->standard == NTSC) ? NTSC : PAL;
    }

    void cycle_visualizer_mode(int delta)
    {
        int count = midi_visualizer_mode_count();
        if (count <= 0)
            return;
        int mode = selected_visualizer_mode();
        mode += delta;
        while (mode < 0)
            mode += count;
        while (mode >= count)
            mode -= count;
        _visualizer_mode = mode;
        if (midi_visualizer_is_active()) {
            midi_visualizer_activate(mode);
            gui_reset_visualizer_buffers();
            gui_render_visualizer_frame();
            _overlay->set_lines(gui_display_video_buffer());
        }
        _dirty = true;
    }

    void activate_music_mode()
    {
        _music_game_boot_grace = 0;
        // Live music starts with an empty audio queue.
        audio_queue_clear();
        apu_midi_enable_current(1);
        _output_mode = OUTPUT_MODE_MUSIC;
        midi_visualizer_activate(selected_visualizer_mode());
        _emu->set_visualizer_active(true);
        begin_visualizer_pages();
        video_palette_glitch_set_game_active(0);
        // The single-buffer path starts rendering at its stable scanline on
        // the next host frame. A hidden page can be rendered immediately.
        if (visualizer_backbuffer_ready()) {
            gui_reset_visualizer_buffers();
            gui_render_visualizer_frame();
        }
        _overlay->set_lines(gui_display_video_buffer());
        close_info_popup();
        close_midi_confirm();
        close_visualizer_popup();
        _visible = false;
        _dirty = true;
    }

    void activate_gameplay_mode()
    {
        const bool was_music = is_music_mode();
        _music_game_boot_grace = 0;
        apu_midi_disable_all();
        _output_mode = OUTPUT_MODE_GAMEPLAY;
        midi_visualizer_deactivate();
        _emu->set_visualizer_active(false);
        end_visualizer_pages();
        video_palette_glitch_set_game_active(1);
        if (was_music && _emu) {
            _emu->key(23, 1, 0);
            _emu->key(23, 0, 0);
        }
        _overlay->set_lines(gui_display_video_buffer());
        close_info_popup();
        close_midi_confirm();
        close_visualizer_popup();
        _visible = false;
        _dirty = true;
    }

    void apply_pending_output_mode(bool reveal_mode_scene)
    {
        if (_pending_output_mode < 0) {
            return;
        }

        const int target_mode = _pending_output_mode;
        _pending_output_mode = -1;
        const bool start_visualizer =
            target_mode == OUTPUT_MODE_MUSIC &&
            reveal_mode_scene &&
            (!_music_visualizer_declined || !_game_loaded);
        close_visualizer_popup();
        _music_visualizer_declined = false;

        if (target_mode == _output_mode)
            return;

        if (reveal_mode_scene) {
            if (start_visualizer)
                activate_music_mode();
            else if (target_mode == OUTPUT_MODE_GAMEPLAY)
                activate_gameplay_mode();
            if (start_visualizer || target_mode == OUTPUT_MODE_GAMEPLAY)
                return;
        }

        _output_mode = target_mode;
        midi_visualizer_deactivate();
        _emu->set_visualizer_active(false);
        end_visualizer_pages();
        video_palette_glitch_set_game_active(target_mode == OUTPUT_MODE_GAMEPLAY);
        _overlay->set_lines(gui_display_video_buffer());
        apply_output_mode_audio_state();
        _dirty = true;
    }

    void apply_output_mode_audio_state()
    {
        // Music mode drives the synth, except while a game is still booting.
        const int desired_enabled =
            is_music_mode() && (midi_visualizer_is_active() || _music_game_boot_grace <= 0);
        if (apu_midi_current_enabled() != desired_enabled)
            apu_midi_enable_current(desired_enabled);
    }

    void update_music_game_boot_grace()
    {
        if (!is_music_mode() || midi_visualizer_is_active()) {
            _music_game_boot_grace = 0;
            return;
        }
        if (_music_game_boot_grace <= 0)
            return;
        if (--_music_game_boot_grace == 0)
            apply_output_mode_audio_state();
    }

    int want(const char* ext)
    {
        auto exts = _emu->_ext;
        for (int i = 0; exts[i]; i++)
            if (strcmp(ext,exts[i]) == 0)
                return i;
        return -1;
    }

    string media_root_path() const
    {
        const size_t slash = _path.find_last_of('/');
        if (slash == string::npos || slash == 0)
            return _path;
        return _path.substr(0, slash);
    }

    bool media_entry_is_absolute(const string& entry) const
    {
        return !entry.empty() && entry[0] == '/';
    }

    string media_full_path(const string& entry) const
    {
        if (media_entry_is_absolute(entry))
            return entry;
        return _path + "/" + entry;
    }

    // Folder entries are absolute paths ending in '/'; "../" goes up a level.
    bool media_entry_is_folder(const string& entry) const
    {
        return !entry.empty() && entry[entry.length() - 1] == '/';
    }

    static bool path_is_inside(const string& entry, const string& dir)
    {
        return !dir.empty() &&
               entry.length() > dir.length() + 1 &&
               entry.compare(0, dir.length(), dir) == 0 &&
               entry[dir.length()] == '/';
    }

    string media_display_name(const string& entry) const
    {
        if (!media_entry_is_absolute(entry))
            return entry;
        if (media_entry_is_folder(entry)) {
            // Just the folder's own name: "RPG/", not "Games/RPG/".
            const size_t slash = entry.find_last_of('/', entry.length() - 2);
            return entry.substr(slash + 1);
        }
        if (path_is_inside(entry, _browse_dir))
            return entry.substr(_browse_dir.length() + 1);
        if (path_is_inside(entry, _path))
            return entry.substr(_path.length() + 1);

        const string root = media_root_path();
        if (!root.empty() &&
            entry.length() > root.length() + 1 &&
            entry.compare(0, root.length(), root) == 0 &&
            entry[root.length()] == '/') {
            string rel = entry.substr(root.length() + 1);
            if (rel.find('/') == string::npos)
                return "/" + rel;
            return rel;
        }
        return entry;
    }

    static bool filename_less(const string& a, const string& b)
    {
        size_t n = min(a.length(), b.length());
        for (size_t i = 0; i < n; ++i) {
            const int al = tolower((unsigned char)a[i]);
            const int bl = tolower((unsigned char)b[i]);
            if (al != bl)
                return al < bl;
        }
        if (a.length() != b.length())
            return a.length() < b.length();
        return a < b;
    }

    bool media_list_heap_ok(size_t count) const
    {
        if (count < GUI_MEDIA_LIST_MIN_ENTRIES)
            return true;
#ifdef ESP_PLATFORM
        if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) <
            GUI_MEDIA_LIST_HEAP_RESERVE)
            return false;
        if (count % GUI_MEDIA_LIST_LARGEST_CHECK_INTERVAL != 0)
            return true;
        return heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) >=
               GUI_MEDIA_LIST_LARGEST_RESERVE;
#else
        return true;
#endif
    }

    // Each scanned directory is distinct (read_directory skips repeats), so
    // entries cannot collide and no per-entry duplicate check is needed.
    // Returns false if the scan stopped early for lack of memory.
    // Sub-folders are added to *folders (skipped when folders is null).
    bool append_directory_media(const string& dir, bool store_absolute, vector<string>& entries,
                                vector<string>* folders = nullptr)
    {
        DIR* dirp = opendir(dir.c_str());
        if (!dirp)
            return true;
        vector<string> found;
        vector<string> found_folders;
        bool complete = true;
        struct dirent * dp;
        try {
            while ((dp = readdir(dirp)) != NULL) {
                // Skip ".", "..", hidden folders (.Trashes, .Spotlight-V100) and
                // macOS AppleDouble metadata (._Game.gb), which is never a ROM.
                if (dp->d_name[0] == '.' && (dp->d_type == DT_DIR || dp->d_name[1] == '_'))
                    continue;
                if (dp->d_type == DT_DIR) {
                    if (!folders)
                        continue;
                } else {
                    string ext = get_ext(dp->d_name);
                    if (want(ext.c_str()) == -1)
                        continue;
                }
                const size_t listed = entries.size() + found.size() + found_folders.size() +
                                      (folders ? folders->size() : 0);
                if (!media_list_heap_ok(listed)) {
                    complete = false;
                    break;
                }
                if (dp->d_type == DT_DIR)
                    found_folders.push_back(dir + "/" + dp->d_name + "/");
                else if (store_absolute)
                    found.push_back(dir + "/" + dp->d_name);
                else
                    found.push_back(dp->d_name);
            }
        } catch (...) {
            complete = false;
        }
        closedir(dirp);

        sort(found.begin(), found.end(), [](const string& a, const string& b) {
            return filename_less(a, b);
        });
        if (folders && !found_folders.empty()) {
            sort(found_folders.begin(), found_folders.end(), [](const string& a, const string& b) {
                return filename_less(a, b);
            });
            try {
                folders->reserve(folders->size() + found_folders.size());
                for (size_t i = 0; i < found_folders.size(); ++i)
                    folders->push_back(std::move(found_folders[i]));
            } catch (...) {
                complete = false;
            }
            vector<string>().swap(found_folders);
        }
        if (entries.empty()) {
            entries.swap(found);    // no reallocation for the first directory
            return complete;
        }
        try {
            entries.reserve(entries.size() + found.size());
            for (size_t i = 0; i < found.size(); ++i)
                entries.push_back(std::move(found[i]));
        } catch (...) {
            complete = false;
        }
        return complete;
    }

    string find_case_insensitive_child_dir(const string& parent, const char* wanted)
    {
        DIR* dirp = opendir(parent.c_str());
        if (!dirp)
            return "";
        struct dirent * dp;
        while ((dp = readdir(dirp)) != NULL) {
            if (dp->d_type != DT_DIR)
                continue;
            string name = dp->d_name;
            if (name == "." || name == "..")
                continue;
            if (name.length() != strlen(wanted))
                continue;
            bool match = true;
            for (size_t i = 0; i < name.length(); ++i) {
                if (tolower((unsigned char)name[i]) !=
                    tolower((unsigned char)wanted[i])) {
                    match = false;
                    break;
                }
            }
            if (match) {
                closedir(dirp);
                return parent + "/" + name;
            }
        }
        closedir(dirp);
        return "";
    }

    // map to sort
    void read_directory(const char* name)
    {
        _path = name;
        _browse_dir.clear();
        load_media_list();
    }

    // Only the open folder is listed, so a big collection split into
    // folders never has to fit in RAM at once.
    void load_media_list()
    {
        _files.clear();
        vector<string> entries;
        vector<string> folders;
        bool complete = true;

        if (!_browse_dir.empty()) {
            try {
                folders.push_back("../");
            } catch (...) {
            }
            complete = append_directory_media(_browse_dir, true, entries, &folders);
        } else {
            complete = append_directory_media(_path, false, entries, &folders);

            const string root = media_root_path();
            if (!root.empty() && root != _path &&
                !append_directory_media(root, true, entries))
                complete = false;

            const string games = find_case_insensitive_child_dir(root, "games");
            if (!games.empty() && games != _path && games != root &&
                !append_directory_media(games, true, entries, &folders))
                complete = false;
        }

        // Folders first, then games.
        if (!folders.empty()) {
            try {
                folders.reserve(folders.size() + entries.size());
                for (size_t i = 0; i < entries.size(); ++i)
                    folders.push_back(std::move(entries[i]));
                entries.swap(folders);
            } catch (...) {
                complete = false;   // reserve failed: keep the games
            }
            vector<string>().swap(folders);
        }

        try {
            entries.shrink_to_fit();
        } catch (...) {
        }
        _files.swap(entries);

        if (!complete) {
            printf("GUI::game list truncated at %u entries (low memory)\n",
                   (unsigned)_files.size());
            try {
                msg("GAME LIST FULL");
            } catch (...) {
            }
        }

        size_t max_path_len = 0;
        for (size_t i = 0; i < _files.size(); ++i) {
            const string& entry = _files[i];
            const size_t full_len = media_entry_is_absolute(entry)
                ? entry.size()
                : _path.size() + 1 + entry.size();
            max_path_len = max(max_path_len, full_len);
        }
        const size_t wanted = max_path_len;
        if (_insert_path.capacity() < wanted) {
            try {
                _insert_path.reserve(wanted + 16);
            } catch (...) {
                printf("GUI::path reserve failed len=%u cap=%u\n",
                       (unsigned)wanted,
                       (unsigned)_insert_path.capacity());
            }
        }
    }

    // Lists dir (empty = top level) and highlights select, or the first game.
    void show_folder(const string& dir, const string& select)
    {
        _browse_dir = dir;
        load_media_list();
        const int found = select.empty() ? -1 : find_file(select);
        if (found >= 0)
            _hilited = found;
        else
            _hilited = (!_browse_dir.empty() && _files.size() > 1) ? 1 : 0;
        _scroll = 0;
        scrollbar();
        force_full_menu_redraw();
    }

    // entry is passed by value: _files is rebuilt while it is used.
    void open_folder(string entry)
    {
        if (entry == "../") {
            folder_up();
            return;
        }
        show_folder(entry.substr(0, entry.length() - 1), "");
    }

    bool folder_up()
    {
        if (_browse_dir.empty())
            return false;
        const string came_from = _browse_dir + "/";
        const size_t slash = _browse_dir.find_last_of('/');
        string parent = (slash == string::npos) ? "" : _browse_dir.substr(0, slash);
        // The top-level folders (Games, the emulator folder) sit in the
        // media root; going up from one of them returns to the main list.
        const string root = media_root_path();
        const size_t parent_slash = parent.find_last_of('/');
        if (parent.length() <= root.length() ||
            (parent_slash != string::npos && parent.compare(0, parent_slash, root) == 0 &&
             parent_slash == root.length()))
            parent.clear();
        show_folder(parent, came_from);
        return true;
    }

    int media_index_for_entry(int index) const
    {
        return (index >= 0 && index < (int)_files.size()) ? index : -1;
    }

    void load_system_prefs()
    {
        _system_bt_transport = bt_transport_load_pref(BT_TRANSPORT_CLASSIC_ONLY);
        _system_emulator = active_emulator_setting();
        _system_standard = active_standard_setting();
        char buf[16] = {0};
        if (sys_get_pref("bootemu", buf, sizeof(buf) - 1)) {
            if (!strcmp(buf, "gb"))
                _system_emulator = EMU_GB;
            else if (!strcmp(buf, "nes"))
                _system_emulator = EMU_NES;
        }
        if (sys_get_pref("bootstd", buf, sizeof(buf) - 1)) {
            if (!strcmp(buf, "ntsc"))
                _system_standard = NTSC;
            else if (!strcmp(buf, "pal"))
                _system_standard = PAL;
        }
        _menu_start_select_trigger = false;
        if (sys_get_pref("menukey", buf, sizeof(buf) - 1)) {
            _menu_start_select_trigger = !strcmp(buf, "combo") ||
                                         !strcmp(buf, "startselect") ||
                                         !strcmp(buf, "1");
        }
        glitch_settings_set_apu_enabled(get_pref("apuglt") != "0");
        bool x_button_momentary = true;
        if (sys_get_pref("xbtn", buf, sizeof(buf) - 1)) {
            x_button_momentary = !strcmp(buf, "mom") ||
                                 !strcmp(buf, "momentary") ||
                                 !strcmp(buf, "1");
        }
        glitch_controls_set_x_button_momentary(x_button_momentary);

        bool y_button_palette = true;
        if (sys_get_pref("ybtn", buf, sizeof(buf) - 1)) {
            y_button_palette = !strcmp(buf, "pal") ||
                               !strcmp(buf, "palette") ||
                               !strcmp(buf, "1");
        }
        glitch_controls_set_y_palette_mode(y_button_palette);

        uint8_t audio_assignment = GLITCH_AUDIO_ASSIGN_DEFAULT;
        if (sys_get_pref("audin", buf, sizeof(buf) - 1)) {
            if (!strcmp(buf, "g2") || !strcmp(buf, "2") || !strcmp(buf, "glitch2"))
                audio_assignment = GLITCH_AUDIO_ASSIGN_CORRUPTION;
        }
        glitch_controls_set_audio_assignment(audio_assignment);

        bool palette_glitch_enabled = false;
        if (sys_get_pref("palgl", buf, sizeof(buf) - 1)) {
            palette_glitch_enabled = !strcmp(buf, "1") ||
                                     !strcmp(buf, "on") ||
                                     !strcmp(buf, "true");
        }
        if (y_button_palette) {
            palette_glitch_enabled = true;
            sys_set_pref("palgl", "1");
        }
        video_palette_glitch_set_enabled(palette_glitch_enabled);

        uint8_t cv_a = GLITCH_CV_ASSIGN_DEFAULT_A;
        uint8_t cv_b = GLITCH_CV_ASSIGN_DEFAULT_B;
        if (sys_get_pref("cvaasg", buf, sizeof(buf) - 1)) {
            int v = atoi(buf);
            if (v >= 0 && v < GLITCH_CV_ASSIGN_COUNT)
                cv_a = (uint8_t)v;
        }
        if (sys_get_pref("cvbasg", buf, sizeof(buf) - 1)) {
            int v = atoi(buf);
            if (v >= 0 && v < GLITCH_CV_ASSIGN_COUNT)
                cv_b = (uint8_t)v;
        }
        glitch_controls_set_cv_assignment(GLITCH_CV_A, cv_a);
        glitch_controls_set_cv_assignment(GLITCH_CV_B, cv_b);

        _system_gb_display_fullscreen = 1;
        if (sys_get_pref("gbdisp", buf, sizeof(buf) - 1)) {
            if (!strcmp(buf, "orig"))
                _system_gb_display_fullscreen = 0;
            else if (!strcmp(buf, "full"))
                _system_gb_display_fullscreen = 1;
        }
        video_set_gb_display_fullscreen(_system_gb_display_fullscreen);
        reset_reboot_selection_baseline();
    }

    void reset_reboot_selection_baseline()
    {
        _reboot_base_bt_transport = _system_bt_transport;
        _reboot_base_standard = _system_standard;
    }

    bool reboot_selection_changed() const
    {
        return _system_bt_transport != _reboot_base_bt_transport ||
               _system_emulator != active_emulator_setting() ||
               _system_standard != _reboot_base_standard;
    }

    void clear_repeat()
    {
        _rep_key = 0;
        _rep_dir = 0;
        _rep_delay = 0;
        _rep_rate = 0;
    }

    void close_info_popup()
    {
        _info_popup = false;
    }

    void close_midi_confirm()
    {
        _midi_confirm = false;
        _midi_confirm_yes = false;
    }

    void close_reboot_popup()
    {
        _reboot_popup = false;
        _reboot_popup_yes = false;
        _reboot_popup_delay = 0;
    }

    void close_visualizer_popup()
    {
        _visualizer_popup = false;
        _visualizer_popup_yes = true;
        _visualizer_popup_delay = 0;
    }

    bool modal_popup_open() const
    {
        return _info_popup || _reboot_popup || _visualizer_popup ||
               _midi_confirm;
    }

    bool reboot_popup_pending() const
    {
        return _reboot_popup_delay > 0;
    }

    void open_reboot_popup()
    {
        close_info_popup();
        close_midi_confirm();
        close_visualizer_popup();
        _reboot_popup = true;
        _reboot_popup_yes = true;
        _reboot_popup_delay = 0;
        _dirty = true;
        clear_live_partial_dirty();
        clear_repeat();
        _msg.clear();
        _msg_ticks = 0;
        if (_overlay)
            _overlay->erase_msg();
    }

    void schedule_reboot_popup()
    {
        close_info_popup();
        close_midi_confirm();
        close_visualizer_popup();
        _reboot_popup = false;
        _reboot_popup_yes = true;
        _reboot_popup_delay = REBOOT_POPUP_DELAY_FRAMES;
        _dirty = true;
        clear_live_partial_dirty();
        _msg.clear();
        _msg_ticks = 0;
        if (_overlay)
            _overlay->erase_msg();
    }

    void update_reboot_popup_for_system_selection()
    {
        if (reboot_selection_changed())
            schedule_reboot_popup();
        else
            close_reboot_popup();
    }

    void tick_reboot_popup_delay()
    {
        if (_reboot_popup_delay <= 0)
            return;
        if (!_visible) {
            _reboot_popup_delay = 0;
            return;
        }
        if (--_reboot_popup_delay == 0)
            open_reboot_popup();
    }

    void open_visualizer_popup()
    {
        close_info_popup();
        close_midi_confirm();
        close_reboot_popup();
        _visualizer_popup = true;
        _visualizer_popup_yes = true;
        _visualizer_popup_delay = 0;
        _dirty = true;
        clear_live_partial_dirty();
        clear_repeat();
        const bool had_message = !_msg.empty();
        _msg.clear();
        _msg_ticks = 0;
        if (_overlay && had_message)
            _overlay->erase_msg();
    }

    void schedule_visualizer_popup()
    {
        close_info_popup();
        close_midi_confirm();
        close_reboot_popup();
        _visualizer_popup = false;
        _visualizer_popup_yes = true;
        _visualizer_popup_delay = VISUALIZER_POPUP_DELAY_FRAMES;
        _music_visualizer_declined = false;
        _dirty = true;
        clear_live_partial_dirty();
        const bool had_message = !_msg.empty();
        _msg.clear();
        _msg_ticks = 0;
        if (_overlay && had_message)
            _overlay->erase_msg();
    }

    void tick_visualizer_popup_delay()
    {
        if (_visualizer_popup_delay <= 0)
            return;
        if (!_visible || _pending_output_mode != OUTPUT_MODE_MUSIC) {
            _visualizer_popup_delay = 0;
            return;
        }
        if (--_visualizer_popup_delay == 0)
            open_visualizer_popup();
    }

    void open_midi_confirm()
    {
        close_info_popup();
        close_reboot_popup();
        close_visualizer_popup();
        _midi_confirm = true;
        _midi_confirm_yes = false;
        _dirty = true;
        clear_live_partial_dirty();
        clear_repeat();
        _msg.clear();
        _msg_ticks = 0;
        if (_overlay)
            _overlay->erase_msg();
    }

    bool is_adjustable_tab() const
    {
        return _tab == TAB_MIDI || _tab == TAB_SYSTEM;
    }

    void move_tab(int dir, bool enter_unselected = false)
    {
        dir += _tab;
        if (dir < 0 || dir >= TAB_COUNT)
            return;
        // Save MIDI settings when leaving the tab.
        if (_tab == TAB_MIDI && dir != TAB_MIDI) {
            midi_save_prefs();
            apu_midi_save_prefs();
            close_midi_confirm();
            _overlay->erase_msg();
        }
        close_info_popup();
        _tab_scroll[_tab] = _scroll;
        if (_hilited >= 0)
            _tab_hilited[_tab] = _hilited;
        _tab = dir;
        _scroll = _tab_scroll[_tab] ;
        if (enter_unselected) {
            _hilited = -1;
        } else {
            _hilited = _tab_hilited[_tab];
            // Clamp the saved selection to the new tab.
            int c = count();
            if (is_adjustable_tab()) {
                if (_hilited < -1) _hilited = -1;
                if (_hilited >= c) _hilited = c-1;
            } else {
                if (_hilited < 0) _hilited = 0;
                if (_hilited >= c) _hilited = max(0,c-1);
            }
        }
        force_full_menu_redraw();
    }

    void move_v(int dir)
    {
        int c = count();
        const int old_hilited = _hilited;
        const int old_scroll = _scroll;
        const int old_y = item_y(_hilited);
        if (is_adjustable_tab()) {
            // -1 leaves left/right available for tab switching.
            _hilited += dir;
            _hilited = max(-1, min(_hilited, c-1));
        } else {
            _hilited += dir;
            _hilited = max(0,min(_hilited,c-1));
        }
        if (_hilited != old_hilited) {
            if (live_ntsc_menu_fallback() && !modal_popup_open()) {
                scrollbar();
                if (_scroll == old_scroll) {
                    mark_live_dirty_row(old_y);
                    mark_live_dirty_row(item_y(_hilited));
                } else {
                    mark_live_dirty_item_area();
                    mark_live_dirty_scrollbar();
                }
            } else {
                _dirty = true;
            }
        }
    }

    void tick_repeat()
    {
        if (!_visible || _rep_dir == 0 || modal_popup_open())
            return;
        if (_rep_delay > 0) {
            _rep_delay--;
            return;
        }
        if (_rep_rate > 0) {
            _rep_rate--;
            return;
        }
        // time to repeat a step
        if (_rep_key == 82 || _rep_key == 81) {
            // up/down repeat navigation
            move_v(_rep_dir);
        } else if ((_rep_key == 80 || _rep_key == 79) && _hilited >= 0) {
            if (_tab == TAB_MIDI) {
                if (midi_selected_adjustable())
                    midi_adjust(_rep_dir);
            } else if (_tab == TAB_SYSTEM) {
                if (system_selected_adjustable())
                    system_adjust(_rep_dir);
            }
        }
        // slower repeat after the first
        _rep_rate = 8; // repeat every ~8 frames (~130 ms @ 60 Hz)
    }

    void tick_ui_frame()
    {
        const uint32_t frame = (uint32_t)_frame_counter;
        if (_ui_tick_frame == frame)
            return;
        _ui_tick_frame = frame;
        tick_reboot_popup_delay();
        tick_visualizer_popup_delay();
        tick_repeat();
        update_marquee_state();
    }

    int count()
    {
        switch (_tab) {
            case TAB_GAMES:
                return (int)_files.size();
            case TAB_MIDI:
                return midi_visible_count();
            case TAB_SYSTEM:
                return system_visible_count();
        }
        return 0;
    }

    bool insert(const string& path, int flags)
    {
        begin_loading_screen();

        close_info_popup();
        midi_visualizer_deactivate();
        _emu->set_visualizer_active(false);
        end_visualizer_pages();
        const int target_output_mode = selected_output_mode();
        _music_game_boot_grace = (target_output_mode == OUTPUT_MODE_MUSIC) ? MUSIC_GAME_BOOT_GRACE_FRAMES : 0;
        apu_midi_disable_all();
        loading_progress_set(2);
        if (!build_insert_path(path)) {
            end_loading_screen();
            return false;
        }
        loading_progress_set(4);
        try {
            set_pref("recent",path);
        } catch (...) {
            printf("GUI::recent pref failed for %s\n", path.c_str());
        }
        loading_progress_set(6);
        int result = -1;
        try {
            result = _emu->insert(_insert_path,flags);
        } catch (...) {
            printf("GUI::insert failed for %s\n", _insert_path.c_str());
            end_loading_screen();
            msg("LOAD FAILED");
            return false;
        }
        end_loading_screen();
        if (result < 0) {
            msg("LOAD FAILED");
            return false;
        }
        _game_loaded = true;
        if (_pending_output_mode < 0)
            apply_output_mode_audio_state();
        return true;
    }

    void enter(int mods)
    {
        if (_tab != TAB_GAMES)
            return;

        int flags = 1;
        if (mods & 2)
            flags |= 2; // shift key
        int media_index = media_index_for_entry(_hilited);
        if (media_index < 0)
            return;
        if (media_entry_is_folder(_files[media_index])) {
            open_folder(_files[media_index]);
            return;
        }
        if (!insert(_files[media_index],flags))
            return;
        apply_pending_output_mode(false);
        close_midi_confirm();
        close_info_popup();
        _visible = false;
        _dirty = true;
    }

    int find_file(const string& file)
    {
        for (auto i = 0; i < _files.size(); i++) {
            if (_files[i] == file)
                return i;
        }
        return -1;
    }

    // Only rebuild a bundled default ROM that was read and has a bad header;
    // a failed open (busy or flaky card) must not delete it.
    bool default_rom_needs_rebuild(const char* leaf)
    {
        if (Emu::head(media_full_path(leaf), nullptr, 0) < 0)
            return false;
        if (_emu && _emu->flavor == EMU_NES)
            return !valid_nes_rom_leaf(leaf);
        return !valid_gb_rom_leaf(leaf);
    }

    bool valid_nes_rom_leaf(const string& leaf)
    {
        string full = media_full_path(leaf);
        uint8_t h[16];
        int len = Emu::head(full, h, sizeof(h));
        bool valid = len >= (int)sizeof(h) &&
                     h[0] == 'N' && h[1] == 'E' && h[2] == 'S' && h[3] == 0x1A;
        if (!valid) {
            printf("GUI::invalid NES media %s len=%d header=%02X %02X %02X %02X\n",
                   full.c_str(), len, h[0], h[1], h[2], h[3]);
        }
        return valid;
    }

    bool valid_gb_rom_leaf(const string& leaf)
    {
        string full = media_full_path(leaf);
        uint8_t h[0x150] = {};
        int len = Emu::head(full, h, sizeof(h));
        static const uint8_t logo[48] = {
            0xCE,0xED,0x66,0x66,0xCC,0x0D,0x00,0x0B,
            0x03,0x73,0x00,0x83,0x00,0x0C,0x00,0x0D,
            0x00,0x08,0x11,0x1F,0x88,0x89,0x00,0x0E,
            0xDC,0xCC,0x6E,0xE6,0xDD,0xDD,0xD9,0x99,
            0xBB,0xBB,0x67,0x63,0x6E,0x0E,0xEC,0xCC,
            0xDD,0xDC,0x99,0x9F,0xBB,0xB9,0x33,0x3E,
        };
        bool valid = len >= (int)sizeof(h) &&
                     memcmp(h + 0x104, logo, sizeof(logo)) == 0;
        if (valid) {
            uint8_t x = 0;
            for (int i = 0x134; i <= 0x14C; ++i)
                x = (uint8_t)(x - h[i] - 1);
            valid = x == h[0x14D];
        }
        if (!valid) {
            printf("GUI::invalid GB media %s len=%d header=%02X %02X %02X %02X\n",
                   full.c_str(), len, h[0x104], h[0x105], h[0x106], h[0x107]);
        }
        return valid;
    }

    bool can_close_menu() const
    {
        if (!_emu)
            return true;
        if (_emu->flavor != EMU_NES && _emu->flavor != EMU_GB)
            return true;
        return _game_loaded || selected_output_mode() == OUTPUT_MODE_MUSIC;
    }

    void reset_midi_defaults()
    {
        uint8_t def_notes[8] = {60,62,64,65,67,69,71,72};
        midi_set_enabled(true);
        midi_set_channel(15);
        midi_set_fx_channel(-1);
        midi_set_note_map(def_notes);
        midi_set_auto_mode(0);
        // Reset sound mappings before applying the system defaults.
        nes_apu_midi_set_channel_map(0,1,2,9);
        nes_apu_midi_set_env_cc(-1,-1,-1,-1);
        nes_apu_midi_set_duty_cc(-1,-1);
        nes_apu_midi_set_noise_mode_cc(-1);
        nes_apu_midi_set_env_cc(78,75,64,72);
        nes_apu_midi_set_duty_cc(74,71);
        nes_apu_midi_set_pb_default_range(2);
        gb_apu_midi_set_channel_map(0,1,2,9);
        gb_apu_midi_set_env_cc(-1,-1,-1,-1);
        gb_apu_midi_set_duty_cc(-1,-1);
        gb_apu_midi_set_noise_mode_cc(-1);
        gb_apu_midi_set_waveform_cc(-1);
        gb_apu_midi_set_env_cc(78,75,64,72);
        gb_apu_midi_set_duty_cc(74,71);
        gb_apu_midi_set_waveform_cc(73);
        gb_apu_midi_set_pb_default_range(2);
        midi_set_cc_numbers(1,2);
        midi_set_visualizer_sprite_cc(76);
        midi_save_prefs();
        apu_midi_save_prefs();
        close_midi_confirm();
        _dirty = true;
    }

    // raw keycode
    bool key(int keycode, int pressed, int mods)
    {

        if (pressed && keycode == 58) { // F1 - GUI key
            const bool opening = !_visible;
            if (!opening && !can_close_menu()) {
                return true;
            }
            if (!opening && _visualizer_popup) {
                _music_visualizer_declined = !_visualizer_popup_yes;
                close_visualizer_popup();
            }
            _visible = opening;       // toggle GUI
            if (_visible)
            {
                video_palette_glitch_set_game_active(0);
                force_full_menu_redraw();
                if (!gui_has_menu_backdrop(_emu))
                    _overlay->frame();      // draw the frame when it first appears
            }
            else {
                if (_tab == TAB_MIDI) {
                    midi_save_prefs();      // persist MIDI settings when closing the menu
                    apu_midi_save_prefs();
                }
                apply_pending_output_mode(true);
                video_palette_glitch_set_game_active(_output_mode == OUTPUT_MODE_GAMEPLAY &&
                                                     !midi_visualizer_is_active());
            }
            if (!_visible) {
                close_midi_confirm();
                close_info_popup();
                close_reboot_popup();
                close_visualizer_popup();
                _overlay->erase_msg();
            }
            return true;
        }
        if (!_visible && midi_visualizer_is_active()) {
            if (!pressed && (keycode == 40 || keycode == 41 ||
                             keycode == GUI_KEY_FACE_X ||
                             keycode == GUI_KEY_FACE_Y ||
                             keycode == GUI_KEY_LETTER_Y))
                return true;
            if (pressed && keycode == 40) {
                midi_visualizer_cycle_palette();
                gui_render_visualizer_frame();
                _overlay->set_lines(gui_display_video_buffer());
                return true;
            }
            if (pressed && keycode == 41) {
                cycle_visualizer_mode(1);
                return true;
            }
            if (pressed && keycode == GUI_KEY_FACE_X) {
                // X bypass is handled by the shared glitch controls.
                return true;
            }
            if (pressed && (keycode == GUI_KEY_FACE_Y ||
                            keycode == GUI_KEY_LETTER_Y)) {
                midi_visualizer_cycle_sprite();
                gui_render_visualizer_frame();
                _overlay->set_lines(gui_display_video_buffer());
                return true;
            }
        }
        if (!_visible)
            return false;

        if (_reboot_popup) {
            if (pressed) {
                if (keycode == 40) {
                    if (_reboot_popup_yes)
                        gui_restart_device();
                    close_reboot_popup();
                    _dirty = true;
                    return true;
                }
                if (keycode == 41 || keycode == 224 ||
                    keycode == GUI_KEY_FACE_X || keycode == GUI_KEY_FACE_Y ||
                    keycode == GUI_KEY_LETTER_X || keycode == GUI_KEY_LETTER_Y) {
                    close_reboot_popup();
                    _dirty = true;
                    return true;
                }
                if (keycode == 79 || keycode == 80 || keycode == 81 || keycode == 82) {
                    _reboot_popup_yes = !_reboot_popup_yes;
                    _dirty = true;
                    clear_repeat();
                    return true;
                }
            }
            if (!pressed && keycode == _rep_key)
                clear_repeat();
            return true;
        }

        if (_visualizer_popup) {
            if (pressed) {
                if (keycode == 40) {
                    const bool start_visualizer = _visualizer_popup_yes;
                    _music_visualizer_declined = !start_visualizer;
                    close_visualizer_popup();
                    if (start_visualizer)
                        apply_pending_output_mode(true);
                    _dirty = true;
                    return true;
                }
                if (keycode == 41 || keycode == 224 ||
                    keycode == GUI_KEY_FACE_X || keycode == GUI_KEY_FACE_Y ||
                    keycode == GUI_KEY_LETTER_X || keycode == GUI_KEY_LETTER_Y) {
                    _music_visualizer_declined = true;
                    close_visualizer_popup();
                    _dirty = true;
                    return true;
                }
                if (keycode == 79 || keycode == 80 ||
                    keycode == 81 || keycode == 82) {
                    _visualizer_popup_yes = !_visualizer_popup_yes;
                    _dirty = true;
                    clear_repeat();
                    return true;
                }
            }
            if (!pressed && keycode == _rep_key)
                clear_repeat();
            return true;
        }

        if (_info_popup) {
            if (pressed && (keycode == 40 || keycode == 41 ||
                            keycode == GUI_KEY_FACE_X || keycode == GUI_KEY_FACE_Y ||
                            keycode == GUI_KEY_LETTER_X || keycode == GUI_KEY_LETTER_Y)) {
                close_info_popup();
                _dirty = true;
            }
            if (!pressed && keycode == _rep_key)
                clear_repeat();
            return true;
        }

        if (_midi_confirm) {
            if (pressed) {
                if (keycode == 40) {
                    if (_midi_confirm_yes)
                        reset_midi_defaults();
                    else {
                        close_midi_confirm();
                        _dirty = true;
                    }
                    return true;
                }
                if (keycode == 41 || keycode == 224 ||
                    keycode == GUI_KEY_FACE_X || keycode == GUI_KEY_FACE_Y ||
                    keycode == GUI_KEY_LETTER_X || keycode == GUI_KEY_LETTER_Y) {
                    close_midi_confirm();
                    _dirty = true;
                    return true;
                }
                if (keycode == 79 || keycode == 80 || keycode == 81 || keycode == 82) {
                    _midi_confirm_yes = !_midi_confirm_yes;
                    _dirty = true;
                    clear_repeat();
                    return true;
                }
            }
            if (!pressed && keycode == _rep_key)
                clear_repeat();
            return true;
        }

        if (pressed) {
            switch (keycode) {
                case GUI_KEY_INFO:
                    if (_tab == TAB_GAMES && !is_gb_gui()) {
                        open_info_popup();
                        return true;
                    }
                    break;
                case GUI_KEY_LETTER_Y:
                case GUI_KEY_FACE_Y:
                    break;
                case 40:
                    if (_tab == TAB_MIDI) {
                        MidiVisibleRow row = {};
                        if (!midi_row_at(_hilited, &row))
                            return true;
                        if (row.kind == MIDI_ROW_SECTION) {
                            toggle_midi_section(row.section);
                            return true;
                        }
                        if (row.kind == MIDI_ROW_RESET) {
                            open_midi_confirm();
                            return true;
                        }
                        return true;
                    } else if (_tab == TAB_SYSTEM) {
                        SystemVisibleRow row = {};
                        if (system_visible_row_at(_hilited, &row) &&
                            row.kind == SYSTEM_VISIBLE_SECTION) {
                            toggle_system_section(row.section);
                        }
                        return true;
                    } else {
                        enter(mods);    // return
                    }
                    break;
                case 41:
                    if (_tab == TAB_MIDI && midi_back())
                        return true;
                    if (_tab == TAB_SYSTEM && system_back())
                        return true;
                    if (_tab == TAB_GAMES && folder_up())
                        return true;
                    break;
                case 82:    // up
                    move_v(-1);
                    _rep_key = 82; _rep_dir = -1; _rep_delay = 30; _rep_rate = 0; // ~0.5s initial delay
                    break;
                case 81:    // down
                    move_v(1);
                    _rep_key = 81; _rep_dir = 1; _rep_delay = 30; _rep_rate = 0; // ~0.5s initial delay
                    break;
                case 80:    // left
                    if (_tab == TAB_MIDI && _hilited >= 0 && !(mods & 2) && midi_selected_adjustable()) {
                        // adjust selected value; hold to repeat
                        midi_adjust(-1);
                        _rep_key = 80; _rep_dir = -1; _rep_delay = 60; _rep_rate = 0; // longer delay before repeat
                    } else if (_tab == TAB_SYSTEM && _hilited >= 0 && !(mods & 2) && system_selected_adjustable()) {
                        system_adjust(-1);
                        if (!_reboot_popup) {
                            _rep_key = 80; _rep_dir = -1; _rep_delay = 60; _rep_rate = 0; // longer delay before repeat
                        }
                    } else {
                        // no item selected (or shift held): switch tab
                        move_tab(-1, true);
                    }
                    break;
                case 79:    // right
                    if (_tab == TAB_MIDI && _hilited >= 0 && !(mods & 2) && midi_selected_adjustable()) {
                        // adjust selected value; hold to repeat
                        midi_adjust(1);
                        _rep_key = 79; _rep_dir = 1; _rep_delay = 60; _rep_rate = 0; // longer delay before repeat
                    } else if (_tab == TAB_SYSTEM && _hilited >= 0 && !(mods & 2) && system_selected_adjustable()) {
                        system_adjust(1);
                        if (!_reboot_popup) {
                            _rep_key = 79; _rep_dir = 1; _rep_delay = 60; _rep_rate = 0; // longer delay before repeat
                        }
                    } else {
                        // no item selected (or shift held): switch tab
                        move_tab(1, true);
                    }
                    break;
                case 224:   // left control key
                    break;  // FIRE
            }
        } else {
            // key release: stop repeating if it was the held key
            if (keycode == _rep_key) {
                clear_repeat();
            }
        }
        return true;
    }

    void midi_adjust(int delta)
    {
        MidiVisibleRow row = {};
        if (!midi_row_at(_hilited, &row) || row.kind != MIDI_ROW_VALUE)
            return;
        const int idx = row.id;

        // Fetch current config
        int8_t ch = midi_get_channel();
        uint8_t cc1, cc2; midi_get_cc_numbers(&cc1, &cc2);
        int8_t sprite_cc = midi_get_visualizer_sprite_cc();
        uint8_t notes[8]; midi_get_note_map(notes);
        uint8_t auto_mode = midi_get_auto_mode();
        int8_t p1, p2, tri, noi; apu_midi_get_channel_map(&p1, &p2, &tri, &noi);
        int8_t wave_cc = -1;
        int8_t env_mode_cc = -1, env_decay_cc = -1, env_loop_cc = -1, length_cc = -1;
        gb_midi_get_waveform_cc(&wave_cc);
        apu_midi_get_env_cc(&env_mode_cc, &env_decay_cc, &env_loop_cc, &length_cc);

        auto adjust_range = [delta](int value, int lo, int hi) -> int {
            int v = value;
            if (v < lo)
                v = lo;
            if (v > hi)
                v = hi;
            return wrap_range(v + delta, lo, hi);
        };

        auto adjust_global_cc = [delta](
            int value, int avoid_a, int avoid_b) -> uint8_t {
            int candidate = value;
            for (int attempts = 0; attempts < 128; ++attempts) {
                candidate = wrap_range(candidate + delta, 0, 127);
                if (candidate == avoid_a || candidate == avoid_b)
                    continue;
                if (!midi_cc_is_sound_control((uint8_t)candidate))
                    return (uint8_t)candidate;
            }
            return (uint8_t)value;
        };

        auto adjust_optional_global_cc = [delta, cc1, cc2](
            int8_t value) -> int8_t {
            int candidate = value;
            for (int attempts = 0; attempts < 129; ++attempts) {
                candidate = wrap_range(candidate + delta, -1, 127);
                if (candidate < 0)
                    return -1;
                if (candidate == cc1 || candidate == cc2)
                    continue;
                if (!midi_cc_is_sound_control((uint8_t)candidate))
                    return (int8_t)candidate;
            }
            return value;
        };

        auto adjust_sound_cc = [this, delta, cc1, cc2, sprite_cc](
            int8_t value) -> int8_t {
            int candidate = value;
            for (int attempts = 0; attempts < 129; ++attempts) {
                candidate = wrap_range(candidate + delta, -1, 127);
                if (candidate < 0)
                    return -1;
                if (candidate == cc1 || candidate == cc2 ||
                    candidate == sprite_cc) {
                    continue;
                }
                if (!apu_midi_cc_is_sound_control((uint8_t)candidate))
                    return (int8_t)candidate;
            }
            return value;
        };

        if (idx == MIDI_VALUE_BUTTONS_CHANNEL) {
            midi_set_channel((int8_t)adjust_range((int)ch, -1, 15));
        } else if (idx == MIDI_VALUE_FX_CHANNEL) {
            midi_set_fx_channel((int8_t)adjust_range((int)midi_get_fx_channel(), -1, 15));
        } else if (idx == MIDI_VALUE_AUTO_MODE) {
            midi_set_auto_mode((uint8_t)adjust_range((int)auto_mode, 0, 2));
        } else if (idx == MIDI_VALUE_CC_GLITCH_A) {
            midi_set_cc_numbers(
                adjust_global_cc((int)cc1, cc2, sprite_cc), cc2);
        } else if (idx == MIDI_VALUE_CC_GLITCH_B) {
            midi_set_cc_numbers(
                cc1, adjust_global_cc((int)cc2, cc1, sprite_cc));
        } else if (idx == MIDI_VALUE_VISUALIZER_SPRITE_CC) {
            midi_set_visualizer_sprite_cc(
                adjust_optional_global_cc(sprite_cc));
        }
        else if (idx == MIDI_VALUE_PULSE1_CHAN) {
            p1 = (int8_t)adjust_range((int)p1, -1, 15); apu_midi_set_channel_map(p1,p2,tri,noi);
        } else if (idx == MIDI_VALUE_PULSE2_CHAN) {
            p2 = (int8_t)adjust_range((int)p2, -1, 15); apu_midi_set_channel_map(p1,p2,tri,noi);
        } else if (idx == MIDI_VALUE_TRIANGLE_CHAN) {
            tri = (int8_t)adjust_range((int)tri, -1, 15); apu_midi_set_channel_map(p1,p2,tri,noi);
        } else if (idx == MIDI_VALUE_NOISE_CHAN) {
            noi = (int8_t)adjust_range((int)noi, -1, 15); apu_midi_set_channel_map(p1,p2,tri,noi);
        } else if (idx == MIDI_VALUE_PULSE1_DUTY_CC) {
            int8_t d1, d2; apu_midi_get_duty_cc(&d1, &d2);
            d1 = adjust_sound_cc(d1); apu_midi_set_duty_cc(d1,d2);
        } else if (idx == MIDI_VALUE_PULSE2_DUTY_CC) {
            int8_t d1, d2; apu_midi_get_duty_cc(&d1, &d2);
            d2 = adjust_sound_cc(d2); apu_midi_set_duty_cc(d1,d2);
        } else if (idx == MIDI_VALUE_PITCH_BEND_RANGE) {
            apu_midi_set_pb_default_range((uint8_t)adjust_range((int)apu_midi_get_pb_default_range(), 0, 24));
        } else if (idx == MIDI_VALUE_NOISE_SHORT_MODE_CC) {
            int8_t ccnoi; apu_midi_get_noise_mode_cc(&ccnoi);
            apu_midi_set_noise_mode_cc(adjust_sound_cc(ccnoi));
        } else if (idx == MIDI_VALUE_WAVEFORM_CC) {
            gb_midi_set_waveform_cc(adjust_sound_cc(wave_cc));
        } else if (idx == MIDI_VALUE_ENV_MODE_CC) {
            env_mode_cc = adjust_sound_cc(env_mode_cc);
            apu_midi_set_env_cc(env_mode_cc, env_decay_cc, env_loop_cc, length_cc);
        } else if (idx == MIDI_VALUE_ENV_DECAY_CC) {
            env_decay_cc = adjust_sound_cc(env_decay_cc);
            apu_midi_set_env_cc(env_mode_cc, env_decay_cc, env_loop_cc, length_cc);
        } else if (idx == MIDI_VALUE_ENV_LOOP_CC) {
            env_loop_cc = adjust_sound_cc(env_loop_cc);
            apu_midi_set_env_cc(env_mode_cc, env_decay_cc, env_loop_cc, length_cc);
        } else if (idx == MIDI_VALUE_LENGTH_CC) {
            length_cc = adjust_sound_cc(length_cc);
            apu_midi_set_env_cc(env_mode_cc, env_decay_cc, env_loop_cc, length_cc);
        }
        else {
            int ni = idx - MIDI_VALUE_NOTE_UP; if (ni >= 0 && ni < 8) {
                notes[ni] = (uint8_t)adjust_range((int)notes[ni], 0, 127); midi_set_note_map(notes);
            }
        }
        mark_current_adjust_dirty();
    }

    void system_adjust(int delta)
    {
        const int system_row = system_hilited_row();

        if (system_row == SYSTEM_ROW_MODE) {
            const int next = wrap_range(output_mode_option_index() + delta, 0, 1);
            const int next_mode = next == 0 ? OUTPUT_MODE_GAMEPLAY : OUTPUT_MODE_MUSIC;
            _pending_output_mode = (next_mode == _output_mode) ? -1 : next_mode;
            if (_pending_output_mode == OUTPUT_MODE_MUSIC) {
                schedule_visualizer_popup();
            } else {
                close_visualizer_popup();
                _music_visualizer_declined = false;
            }
            mark_current_adjust_dirty();
            clear_repeat();
            return;
        }

        if (system_row == SYSTEM_ROW_BT) {
            const int index = wrap_range((_system_bt_transport == BT_TRANSPORT_BLE_ONLY ? 1 : 0) + delta, 0, 1);
            _system_bt_transport = index == 0 ? BT_TRANSPORT_CLASSIC_ONLY : BT_TRANSPORT_BLE_ONLY;
            bt_transport_save_pref(_system_bt_transport);
            mark_current_adjust_dirty();
            update_reboot_popup_for_system_selection();
            return;
        }

        if (system_row == SYSTEM_ROW_EMULATOR) {
            const int index = wrap_range((_system_emulator == EMU_GB ? 1 : 0) + delta, 0, 1);
            _system_emulator = index == 0 ? EMU_NES : EMU_GB;
            sys_set_pref("bootemu", _system_emulator == EMU_GB ? "gb" : "nes");
            mark_current_adjust_dirty();
            update_reboot_popup_for_system_selection();
            return;
        }

        if (system_row == SYSTEM_ROW_STANDARD) {
            const int index = wrap_range((_system_standard == NTSC ? 1 : 0) + delta, 0, 1);
            _system_standard = index == 0 ? PAL : NTSC;
            sys_set_pref("bootstd", _system_standard == NTSC ? "ntsc" : "pal");
            mark_current_adjust_dirty();
            update_reboot_popup_for_system_selection();
            return;
        }

        if (system_row == SYSTEM_ROW_CV_A || system_row == SYSTEM_ROW_CV_B) {
            const uint8_t cv = system_row == SYSTEM_ROW_CV_A ? GLITCH_CV_A : GLITCH_CV_B;
            const int index = wrap_range(glitch_controls_get_cv_assignment(cv) + delta,
                                         0, GLITCH_CV_ASSIGN_COUNT - 1);
            glitch_controls_set_cv_assignment(cv, (uint8_t)index);
            char pref[8];
            snprintf(pref, sizeof(pref), "%d", index);
            sys_set_pref(cv == GLITCH_CV_A ? "cvaasg" : "cvbasg", pref);
            mark_current_adjust_dirty();
            clear_repeat();
            return;
        }

        if (system_row == SYSTEM_ROW_AUDIO_IN) {
            const int index = wrap_range(glitch_controls_get_audio_assignment() + delta,
                                         GLITCH_AUDIO_ASSIGN_PPU,
                                         GLITCH_AUDIO_ASSIGN_CORRUPTION);
            glitch_controls_set_audio_assignment((uint8_t)index);
            sys_set_pref("audin", index == GLITCH_AUDIO_ASSIGN_CORRUPTION ? "g2" : "g1");
            mark_current_adjust_dirty();
            clear_repeat();
            return;
        }

        if (system_row == SYSTEM_ROW_MENU_TRIGGER) {
            const int index = wrap_range((_menu_start_select_trigger ? 1 : 0) + delta, 0, 1);
            _menu_start_select_trigger = index != 0;
            sys_set_pref("menukey", _menu_start_select_trigger ? "combo" : "hold");
            mark_current_adjust_dirty();
            clear_repeat();
            return;
        }

        if (system_row == SYSTEM_ROW_X_BUTTON) {
            const int index = wrap_range((glitch_controls_get_x_button_momentary() ? 1 : 0) + delta, 0, 1);
            const bool momentary = index != 0;
            glitch_controls_set_x_button_momentary(momentary);
            sys_set_pref("xbtn", momentary ? "mom" : "latch");
            mark_current_adjust_dirty();
            clear_repeat();
            return;
        }

        if (system_row == SYSTEM_ROW_Y_BUTTON) {
            const int index = wrap_range((glitch_controls_get_y_palette_mode() ? 1 : 0) + delta, 0, 1);
            const bool palette_mode = index != 0;
            glitch_controls_set_y_palette_mode(palette_mode);
            sys_set_pref("ybtn", palette_mode ? "pal" : "glitch");
            if (palette_mode) {
                video_palette_glitch_set_enabled(true);
                sys_set_pref("palgl", "1");
            }
            mark_current_adjust_dirty();
            clear_repeat();
            return;
        }

        if (system_row == SYSTEM_ROW_PALETTE_GLITCH) {
            const int index = wrap_range((video_palette_glitch_get_enabled() ? 1 : 0) + delta, 0, 1);
            const bool enabled = index != 0;
            video_palette_glitch_set_enabled(enabled);
            sys_set_pref("palgl", enabled ? "1" : "0");
            mark_current_adjust_dirty();
            clear_repeat();
            return;
        }

        if (system_row == SYSTEM_ROW_APU_GLITCH) {
            const bool next_enabled = !glitch_settings_is_apu_enabled();
            glitch_settings_set_apu_enabled(next_enabled);
            set_pref("apuglt", next_enabled ? "1" : "0");
            mark_current_adjust_dirty();
            clear_repeat();
            return;
        }

        if (system_row == SYSTEM_ROW_GB_DISPLAY && is_gb_gui()) {
            const int index = wrap_range((_system_gb_display_fullscreen ? 1 : 0) + delta, 0, 1);
            _system_gb_display_fullscreen = index != 0;
            video_set_gb_display_fullscreen(_system_gb_display_fullscreen);
            sys_set_pref("gbdisp", _system_gb_display_fullscreen ? "full" : "orig");
            mark_current_adjust_dirty();
            clear_repeat();
        }
    }

    bool is_gb_gui() const
    {
        return _emu && _emu->flavor == EMU_GB;
    }

    bool menu_backbuffer_supported() const
    {
        return _emu &&
               (_emu->flavor == EMU_NES || _emu->flavor == EMU_GB) &&
               _emu->standard == NTSC &&
               !midi_visualizer_is_active();
    }

    bool visualizer_backbuffer_supported() const
    {
        return _emu &&
               _emu->flavor == EMU_NES &&
               midi_visualizer_is_active();
    }

    bool visualizer_backbuffer_ready() const
    {
        return visualizer_backbuffer_supported() &&
               !_menu_alloc_failed &&
               (_menu_pixels[0] || _menu_pixels_chunked[0]) &&
               _menu_lines[0];
    }

    bool visualizer_menu_backbuffer_supported() const
    {
        return _visible && visualizer_backbuffer_supported();
    }

    bool page_backbuffer_storage_supported() const
    {
        return menu_backbuffer_supported() || visualizer_backbuffer_supported();
    }

    bool use_menu_backbuffer() const
    {
        return _visible &&
               (menu_backbuffer_supported() ||
                visualizer_menu_backbuffer_supported());
    }


    void release_menu_buffers()
    {
        const int height = _menu_buffer_height;
        for (int i = 0; i < 2; ++i) {
            if (_menu_pixels_chunked[i] && _menu_lines[i]) {
                for (int y = 0; y < height; ++y)
                    free(_menu_lines[i][y]);
            } else {
                free(_menu_pixels[i]);
            }
            if (_menu_lines_owned[i])
                free(_menu_lines[i]);
            _menu_pixels[i] = 0;
            _menu_lines[i] = 0;
            _menu_lines_owned[i] = false;
            _menu_pixels_chunked[i] = false;
        }
        _menu_buffer_width = 0;
        _menu_buffer_height = 0;
        _menu_front_buffer = 0;
        _menu_front_valid = false;
        _menu_alloc_failed = false;
        _visualizer_commit_pending = false;
    }

    bool ensure_menu_buffers()
    {
        if (!page_backbuffer_storage_supported())
            return false;
        if (_menu_alloc_failed)
            return false;
        if (!_emu || !_emu->video_buffer())
            return false;
        if (_emu->height > GUI_MENU_BACKBUFFER_MAX_HEIGHT) {
            printf("GUI::page backbuffer height unsupported height=%d max=%d\n",
                   _emu->height, GUI_MENU_BACKBUFFER_MAX_HEIGHT);
            _menu_alloc_failed = true;
            return false;
        }

        if (_menu_buffer_width != _emu->width || _menu_buffer_height != _emu->height)
            release_menu_buffers();

        if ((_menu_pixels[0] || _menu_pixels_chunked[0]) && _menu_lines[0] &&
            (_menu_pixels[1] || _menu_pixels_chunked[1]) && _menu_lines[1])
            return true;

        release_menu_buffers();
        _menu_buffer_width = _emu->width;
        _menu_buffer_height = _emu->height;
        const size_t pixel_bytes = (size_t)_emu->width * (size_t)_emu->height;
#ifdef ESP_PLATFORM
        const size_t total_bytes = pixel_bytes * 2u;
        const size_t free8 = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        const size_t largest8 = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (free8 < total_bytes + GUI_MENU_BACKBUFFER_HEAP_RESERVE ||
            largest8 < pixel_bytes) {
            printf("GUI::page backbuffer skipped bytes=%u total=%u free=%u largest=%u reserve=%u\n",
                   (unsigned)pixel_bytes,
                   (unsigned)total_bytes,
                   (unsigned)free8,
                   (unsigned)largest8,
                   (unsigned)GUI_MENU_BACKBUFFER_HEAP_RESERVE);
            release_menu_buffers();
            _menu_alloc_failed = true;
            return false;
        }
#endif
        for (int slot = 0; slot < 2; ++slot) {
            _menu_lines[slot] = _menu_line_storage[slot];
            memset(_menu_lines[slot], 0, (size_t)_emu->height * sizeof(uint8_t*));
            _menu_lines_owned[slot] = false;
#ifdef ESP_PLATFORM
            _menu_pixels[slot] = (uint8_t*)heap_caps_malloc(pixel_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#else
            _menu_pixels[slot] = (uint8_t*)malloc(pixel_bytes);
#endif
            if (_menu_pixels[slot]) {
                memset(_menu_pixels[slot], 0x0D, pixel_bytes);
                for (int y = 0; y < _emu->height; ++y)
                    _menu_lines[slot][y] = _menu_pixels[slot] + ((size_t)y * (size_t)_emu->width);
            } else {
#ifdef ESP_PLATFORM
                printf("GUI::page backbuffer alloc failed slot=%d bytes=%u free=%u largest=%u\n",
                       slot,
                       (unsigned)pixel_bytes,
                       (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                       (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
                release_menu_buffers();
                _menu_alloc_failed = true;
                return false;
#else
                bool rows_ok = true;
                for (int y = 0; y < _emu->height; ++y) {
                    _menu_lines[slot][y] = (uint8_t*)malloc((size_t)_emu->width);
                    if (!_menu_lines[slot][y]) {
                        rows_ok = false;
                        break;
                    }
                    memset(_menu_lines[slot][y], 0x0D, (size_t)_emu->width);
                }
                _menu_pixels_chunked[slot] = true;
                if (!rows_ok) {
                    release_menu_buffers();
                    printf("GUI::page backbuffer alloc failed slot=%d bytes=%u\n",
                           slot, (unsigned)pixel_bytes);
                    _menu_alloc_failed = true;
                    return false;
                }
#endif
            }
        }

        _menu_buffer_width = _emu->width;
        _menu_buffer_height = _emu->height;
        _menu_front_buffer = 0;
        _menu_front_valid = false;
        return true;
    }

    void prepare_menu_backbuffer()
    {
        if (menu_backbuffer_supported())
            ensure_menu_buffers();
    }

    bool ensure_active_menu_backbuffer()
    {
        if (visualizer_menu_backbuffer_supported())
            return ensure_visualizer_buffer();
        return ensure_menu_buffers();
    }

    uint8_t** menu_display_lines()
    {
        if (!use_menu_backbuffer() || !ensure_active_menu_backbuffer() ||
            !_menu_front_valid)
            return _emu ? _emu->video_buffer() : 0;
        if (visualizer_menu_backbuffer_supported())
            return visualizer_display_lines();
        return _menu_lines[_menu_front_buffer];
    }

    uint8_t** menu_render_lines()
    {
        if (!use_menu_backbuffer() || !ensure_active_menu_backbuffer())
            return _emu ? _emu->video_buffer() : 0;
        if (visualizer_menu_backbuffer_supported())
            return visualizer_render_lines();
        return _menu_lines[_menu_front_buffer ^ 1];
    }

    void commit_menu_render()
    {
        if (!use_menu_backbuffer() || !ensure_active_menu_backbuffer())
            return;
        if (visualizer_menu_backbuffer_supported()) {
            commit_visualizer_render();
            return;
        }
        _menu_front_buffer ^= 1;
        _menu_front_valid = true;
    }

    bool use_visualizer_backbuffer()
    {
        return visualizer_backbuffer_supported() && ensure_visualizer_buffer();
    }

    bool ensure_visualizer_buffer()
    {
        if (!visualizer_backbuffer_supported() || !primary_buffer)
            return false;
        if (_menu_alloc_failed)
            return false;
        if (_emu->height > GUI_MENU_BACKBUFFER_MAX_HEIGHT)
            return false;

        if (_menu_buffer_width != _emu->width ||
            _menu_buffer_height != _emu->height)
            release_menu_buffers();

        if ((_menu_pixels[0] || _menu_pixels_chunked[0]) && _menu_lines[0])
            return true;

        release_menu_buffers();
        _menu_buffer_width = _emu->width;
        _menu_buffer_height = _emu->height;
        const size_t pixel_bytes =
            (size_t)_menu_buffer_width * (size_t)_menu_buffer_height;
        _menu_lines[0] = _menu_line_storage[0];
        memset(_menu_lines[0], 0,
               (size_t)_menu_buffer_height * sizeof(uint8_t*));
        _menu_lines_owned[0] = false;

#ifdef ESP_PLATFORM
        const size_t free8 =
            heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        const size_t largest8 =
            heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (free8 < pixel_bytes + GUI_MENU_BACKBUFFER_HEAP_RESERVE ||
            largest8 < pixel_bytes) {
            printf("GUI::visualizer spare page skipped bytes=%u free=%u largest=%u reserve=%u\n",
                   (unsigned)pixel_bytes,
                   (unsigned)free8,
                   (unsigned)largest8,
                   (unsigned)GUI_MENU_BACKBUFFER_HEAP_RESERVE);
            release_menu_buffers();
            _menu_alloc_failed = true;
            return false;
        }
        _menu_pixels[0] = (uint8_t*)heap_caps_malloc(
            pixel_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#else
        _menu_pixels[0] = (uint8_t*)malloc(pixel_bytes);
#endif
        if (!_menu_pixels[0]) {
            release_menu_buffers();
            _menu_alloc_failed = true;
            return false;
        }

        memset(_menu_pixels[0], 0x0D, pixel_bytes);
        for (int y = 0; y < _menu_buffer_height; ++y)
            _menu_lines[0][y] =
                _menu_pixels[0] + ((size_t)y * (size_t)_menu_buffer_width);

        _menu_alloc_failed = false;
        _menu_front_buffer = 0;
        _menu_front_valid = false;
        _visualizer_commit_pending = false;
        return true;
    }

    void begin_visualizer_pages()
    {
        _menu_front_buffer = 0;
        _menu_front_valid = false;
        _menu_alloc_failed = false;
        _visualizer_commit_pending = false;
        (void)ensure_visualizer_buffer();
    }

    void end_visualizer_pages()
    {
        _menu_front_buffer = 0;
        _menu_front_valid = false;
        _visualizer_commit_pending = false;
    }

    uint8_t** visualizer_display_lines()
    {
        if (!use_visualizer_backbuffer() || !_menu_front_valid)
            return _emu ? _emu->video_buffer() : 0;
        return _menu_front_buffer ? _menu_lines[0] : primary_buffer->line;
    }

    uint8_t** visualizer_render_lines()
    {
        if (!use_visualizer_backbuffer())
            return _emu ? _emu->video_buffer() : 0;
        if (!_menu_front_valid || _menu_front_buffer == 0)
            return _menu_lines[0];
        return primary_buffer->line;
    }

    void commit_visualizer_render()
    {
        if (!use_visualizer_backbuffer())
            return;
        if (!_menu_front_valid)
            _menu_front_buffer = 1;
        else
            _menu_front_buffer ^= 1;
        _menu_front_valid = true;
        _visualizer_commit_pending = true;
    }

    bool visualizer_commit_pending() const
    {
        return _visualizer_commit_pending;
    }

    void finish_visualizer_commit()
    {
        _visualizer_commit_pending = false;
    }

    const char* tab_label(int tab) const
    {
        if (is_gb_gui() && tab == TAB_SYSTEM)
            return "SYS";
        return _menu[tab];
    }

    int tab_center(int tab) const
    {
        const int center_min = is_gb_gui() ? 3 : 5;
        const int center_max = _overlay->OVERLAY_WIDTH - (is_gb_gui() ? 3 : 5);
        int center = _overlay->OVERLAY_WIDTH / 2;
        if (TAB_COUNT > 1)
            center = center_min + (tab * (center_max - center_min) + (TAB_COUNT - 2) / 2) / (TAB_COUNT - 1);
        if (!is_gb_gui() && (tab == TAB_GAMES || tab == TAB_SYSTEM))
            center -= 1;
        return center;
    }

    int tab_label_x(int tab, int label_len) const
    {
        int label_x = tab_center(tab) - ((label_len - 1) / 2);
        if (label_x < 1)
            label_x = 1;
        if (label_x + label_len > _overlay->OVERLAY_WIDTH - 1)
            label_x = max(1, _overlay->OVERLAY_WIDTH - 1 - label_len);
        return label_x;
    }

    void menu()
    {
        _overlay->fill(0, 0, _overlay->OVERLAY_WIDTH, ' ');
        _overlay->fill(1, 0, _overlay->OVERLAY_WIDTH, ' ');
        _overlay->fill(2, 0, _overlay->OVERLAY_WIDTH, ' ');
        _overlay->fill(3, 0, _overlay->OVERLAY_WIDTH, ' ');
        if (use_menu_backbuffer())
            return;

        for (int i = 0; i < TAB_COUNT; ++i) {
            const char* item = tab_label(i);
            const int label_len = (int)strlen(item);
            int label_x = tab_label_x(i, label_len);

            if (i == _tab) {
                const int left = max(0, label_x - 1);
                const int right = min(_overlay->OVERLAY_WIDTH - 1, label_x + label_len);
                _overlay->plot_char(' ', left, 1);
                _overlay->plot_str(item, label_x, 1);
                _overlay->plot_char(' ', right, 1);
            } else {
                _overlay->plot_str(item, label_x, 1);
            }
            _overlay->set_hilite(0);
        }
    }

    void draw_selected_tab_brackets()
    {
        if (!_overlay || !_overlay->_lines)
            return;

        const char* item = tab_label(_tab);
        const int label_len = (int)strlen(item);
        int label_x = tab_label_x(_tab, label_len);

        const int left = max(0, label_x - 1);
        const int right = min(_overlay->OVERLAY_WIDTH - 1, label_x + label_len);
        const int ox = _overlay->OVERLAY_ORIGIN_X * 8;
        const int oy = _overlay->OVERLAY_ORIGIN_Y * 8;
        const int y = oy + 8;
        const uint8_t color = info_border_color();

        draw_mask8(ox + left * 8, y, kSelectedBracketLeft, 0, color);
        draw_mask8(ox + right * 8, y, kSelectedBracketLeft, 0, color, true);
    }

    void draw_transparent_text(const char* text, int x, int y, uint8_t color)
    {
        if (!_overlay || !_overlay->_lines || !text)
            return;
        const int px0 = _overlay->OVERLAY_ORIGIN_X * 8 + x * 8;
        const int py0 = _overlay->OVERLAY_ORIGIN_Y * 8 + y * 8;
        for (int i = 0; text[i]; ++i) {
            const uint8_t glyph = (uint8_t)text[i];
            const int px = px0 + i * 8;
            for (int row = 0; row < 8; ++row) {
                uint8_t bits = gui_font_row(glyph, row);
                uint8_t* dst = _overlay->_lines[py0 + row] + px;
                for (int col = 0; col < 8; ++col) {
                    if (bits & (0x80 >> col))
                        _overlay->setp(dst + col, color);
                }
            }
        }
    }

    void draw_tabs_direct()
    {
        if (!use_menu_backbuffer())
            return;
        const uint8_t color = info_border_color();
        for (int i = 0; i < TAB_COUNT; ++i) {
            const char* item = tab_label(i);
            const int label_len = (int)strlen(item);
            draw_transparent_text(item, tab_label_x(i, label_len), 1, color);
        }
    }

    void draw_ntsc_menu_header()
    {
        if (!use_menu_backbuffer())
            return;
        for (int row = 0; row <= 3; ++row)
            restore_menu_pixel_row(row);
        draw_tabs_direct();
    }

    static const int kHeaderComposeMaxWidth = 256;
    static const int kHeaderComposeMaxLines = 80;

    bool can_compose_live_header() const
    {
#ifdef ESP_PLATFORM
        // Needs ~1 KB of stack; fall back to the direct draw on a tight task.
        if (uxTaskGetStackHighWaterMark(NULL) < 2048)
            return false;
#endif
        return _overlay && _overlay->_lines &&
               _overlay->_width <= kHeaderComposeMaxWidth &&
               _overlay->OVERLAY_ORIGIN_Y * 8 + 32 <= kHeaderComposeMaxLines &&
               _overlay->OVERLAY_ORIGIN_Y * 8 + 32 <= _overlay->_height;
    }

    // Header rows 0-3 (tabs, selection brackets, chain) drawn straight onto
    // the displayed page were erased and redrawn, so the beam sometimes
    // caught them blank. Each pixel line is built in a scratch line by the
    // same draw calls, then copied over the old one in a single pass.
    // Stack only (~0.8 KB): no heap cost.
    void compose_live_header(bool with_backdrop)
    {
        uint32_t scratch_words[kHeaderComposeMaxWidth / 4];
        uint32_t discard_words[kHeaderComposeMaxWidth / 4];
        uint8_t* table[kHeaderComposeMaxLines];
        uint8_t* const scratch = (uint8_t*)scratch_words;
        uint8_t* const discard = (uint8_t*)discard_words;
        uint8_t** const live = _overlay->_lines;
        const int width = _overlay->_width;
        const int oy = _overlay->OVERLAY_ORIGIN_Y * 8;
        // Same per-row steps as the direct path (see the full redraw in
        // update_video): text/tabs, brackets, then the chain's restore + links.
        const bool backbuffer_layout = use_menu_backbuffer();

        for (int i = 0; i < kHeaderComposeMaxLines; ++i)
            table[i] = discard;

        for (int row = 0; row <= 3; ++row) {
            for (int py = 0; py < 8; ++py) {
                const int y = oy + row * 8 + py;
                memcpy(scratch, live[y], (size_t)width);
                if (with_backdrop)
                    memcpy(scratch, kGuiMenuBackdropNes + y * kGuiMenuBackdropNesWidth, (size_t)width);
                table[y] = scratch;
                _overlay->_lines = table;

                if (backbuffer_layout) {
                    restore_menu_pixel_row(row);
                    if (row == 1)
                        draw_tabs_direct();
                } else {
                    _overlay->update_row(row);
                }
                if (row == 1)
                    draw_selected_tab_brackets();
                if (backbuffer_layout ? row == 3 : row != 1)
                    restore_menu_pixel_row(row);
                if (row == 3)
                    draw_tab_chain_links();

                _overlay->_lines = live;
                table[y] = discard;
                memcpy(live[y], scratch, (size_t)width);
            }
        }
    }

    void update_marquee_state()
    {
        const int old_offset = _marquee_offset;
        if (!_visible || !is_gb_gui() || _tab != TAB_GAMES ||
            _hilited < 0 || _hilited >= (int)_files.size() ||
            modal_popup_open()) {
            _marquee_tab = -1;
            _marquee_hilited = -1;
            _marquee_frames = 0;
            _marquee_offset = 0;
            return;
        }

        if (_marquee_tab == _tab && _marquee_hilited == _hilited) {
            if (_marquee_frames < 10000)
                _marquee_frames++;
        } else {
            _marquee_tab = _tab;
            _marquee_hilited = _hilited;
            _marquee_frames = 0;
            _marquee_offset = 0;
            return;
        }

        const int visible_width = _overlay ? max(0, _overlay->OVERLAY_WIDTH - 2) : 0;
        const string display = media_display_name(_files[_hilited]);
        const int hidden = (int)display.size() - visible_width;
        static const int kGbTitleMarqueeDelayFrames = 60; // ~1 second
        static const int kGbTitleMarqueeStepFrames = 8;
        if (hidden <= 0 || _marquee_frames < kGbTitleMarqueeDelayFrames) {
            _marquee_offset = 0;
        } else {
            _marquee_offset = min(hidden,
                                  (_marquee_frames -
                                   kGbTitleMarqueeDelayFrames) /
                                      kGbTitleMarqueeStepFrames);
        }

        if (_marquee_offset != old_offset) {
            if (live_ntsc_menu_fallback() && !modal_popup_open())
                mark_live_dirty_row(item_y(_hilited));
            else
                _dirty = true;
        }
    }

    int selected_game_marquee_offset(int text_len, int visible_width) const
    {
        if (!is_gb_gui() || _tab != TAB_GAMES || _marquee_hilited != _hilited)
            return 0;
        const int hidden = text_len - visible_width;
        if (hidden <= 0)
            return 0;
        return min(hidden, max(0, _marquee_offset));
    }

    int visible_item_rows() const
    {
        const int rows = max(0, (_overlay->OVERLAY_HEIGHT - 6) / 2);
        return max(0, rows - (_scroll == 0 ? 1 : 0));
    }

    int item_y(int i)
    {
        int slot = i - _scroll;
        if (slot < 0)
            return -1;

        if (_tab == TAB_GAMES && slot >= visible_item_rows())
            return -1;

        const int y = (_scroll == 0 ? 5 : 4) + slot * 2;
        if (y <= 1 || y >= (_overlay->OVERLAY_HEIGHT - 2))
            return -1;
        return y;
    }

    void draw_item(int i, const char* str, bool selected)
    {
        int y = item_y(i);
        if (y < 0)
            return;
        _overlay->set_hilite(selected);
        _overlay->fill(y,0,_overlay->OVERLAY_WIDTH-1,' ');
        _overlay->plot_str(str,1,y);
        _overlay->set_hilite(0);
    }

    void draw_adjustable_item(int i, const char* label, const char* value, bool selected, bool can_left, bool can_right)
    {
        int y = item_y(i);
        if (y < 0)
            return;

        const int content_left = 1;
        const int content_right = _overlay->OVERLAY_WIDTH - 2;
        const int content_width = content_right - content_left + 1;

        char value_buf[32];
        int value_len = (int)strlen(value);
        if (value_len > (int)sizeof(value_buf) - 1)
            value_len = (int)sizeof(value_buf) - 1;
        memcpy(value_buf, value, value_len);
        value_buf[value_len] = 0;

        int cluster_len = value_len + 2;
        if (cluster_len > content_width) {
            value_len = max(0, content_width - 2);
            value_buf[value_len] = 0;
            cluster_len = value_len + 2;
        }

        const int cluster_x = content_right - cluster_len + 1;
        const int label_max = max(0, cluster_x - content_left - 1);

        char label_buf[64];
        int label_len = (int)strlen(label);
        if (label_len > label_max)
            label_len = label_max;
        if (label_len > (int)sizeof(label_buf) - 1)
            label_len = (int)sizeof(label_buf) - 1;
        memcpy(label_buf, label, label_len);
        label_buf[label_len] = 0;

        _overlay->set_hilite(selected);
        _overlay->fill(y, 0, _overlay->OVERLAY_WIDTH - 1, ' ');
        if (label_len > 0)
            _overlay->plot_str(label_buf, content_left, y);
        _overlay->plot_char(can_left ? '<' : ' ', cluster_x, y);
        if (value_len > 0)
            _overlay->plot_str(value_buf, cluster_x + 1, y);
        _overlay->plot_char(can_right ? '>' : ' ', cluster_x + 1 + value_len, y);
        _overlay->set_hilite(0);
    }

    bool scrollbar_metrics(int* spider_y_px, int* web_top_px, int* strip_x_px, int* strip_y0_px, int* strip_y1_px)
    {
        if (!_overlay || !_overlay->_lines)
            return false;

        const int cnt = count();
        const int viewport = visible_item_rows();
        if (cnt <= 0 || viewport <= 0)
            return false;

        const int track_top_row = 4;
        // The GB menu has no backdrop frame: rows OVERLAY_HEIGHT-2/-1 hold the
        // footer, so keep the track inside the list rows (4..OVERLAY_HEIGHT-3)
        // and off the chain border above it.
        const bool gb_layout = is_gb_gui();
        const int track_bottom_row = gb_layout ? _overlay->OVERLAY_HEIGHT - 3
                                               : _overlay->OVERLAY_HEIGHT - 1;
        const int track_inset_px = gb_layout ? 2 : 0;
        const int strip_x = (_overlay->OVERLAY_ORIGIN_X + _overlay->OVERLAY_WIDTH - 1) * 8;
        const int strip_top = _overlay->OVERLAY_ORIGIN_Y * 8 + track_top_row * 8 + track_inset_px;
        const int strip_bottom = _overlay->OVERLAY_ORIGIN_Y * 8 + track_bottom_row * 8 + 7 - track_inset_px;
        const int spider_h = 8;
        const int travel = max(0, strip_bottom - strip_top + 1 - spider_h);

        int spider_y = strip_top;
        if (cnt > viewport) {
            const int scroll_max = max(1, cnt - viewport);
            // scrollbar() sizes the viewport for the previous scroll position
            // (row 0 is shorter), so _scroll can exceed scroll_max. Unclamped,
            // the spider was drawn past the bottom of the frame: reading line
            // pointers beyond the array crashed (LoadProhibited at web_x).
            const int scroll = min(max(_scroll, 0), scroll_max);
            spider_y = strip_top + (scroll * travel + scroll_max / 2) / scroll_max;
        }
        spider_y = min(spider_y, strip_top + travel);

        if (spider_y_px) *spider_y_px = spider_y;
        if (web_top_px) *web_top_px = strip_top;
        if (strip_x_px) *strip_x_px = strip_x;
        if (strip_y0_px) *strip_y0_px = strip_top;
        if (strip_y1_px) *strip_y1_px = strip_bottom;
        return true;
    }

    void scrollbar()
    {
        const int viewport = visible_item_rows();
        int cnt = count();
        if (cnt <= 0) {
            _scroll = 0;
        } else {
            _scroll = min(_hilited,_scroll);
            _scroll = max(_hilited - max(0, viewport - 1), _scroll);
            if (_scroll < 0) _scroll = 0;   // never allow negative scroll offset
        }
    }

    void draw_spider_scrollbar()
    {
        int spider_y = 0;
        int web_top = 0;
        int strip_x = 0;
        int strip_y0 = 0;
        int strip_y1 = 0;
        if (!scrollbar_metrics(&spider_y, &web_top, &strip_x, &strip_y0, &strip_y1))
            return;
        // Never touch rows outside the frame, whatever the metrics say.
        const int frame_h = _overlay->_height;
        if (strip_y0 < 0 || strip_y1 >= frame_h || spider_y < 0 || spider_y + 8 > frame_h ||
            web_top < 0 || strip_x < 0 || strip_x + 8 > _overlay->_width)
            return;

        const int spider_w = 8;
        const int spider_h = 8;
        const uint8_t bg = (_emu && _emu->flavor == EMU_NES) ? 0x0D : 0x00;
        const uint8_t web = (_emu && _emu->flavor == EMU_NES) ? 0x20 : 0xFF;
        const uint8_t spider = (_emu && _emu->flavor == EMU_NES) ? 0x20 : 0xFF;
        static const uint8_t kSpider[8] = {
            0b11000011,
            0b01011010,
            0b01111110,
            0b11111111,
            0b10111101,
            0b01111110,
            0b11011011,
            0b10000001
        };

        for (int y = strip_y0; y <= strip_y1; ++y) {
            uint8_t* dst = _overlay->_lines[y] + strip_x;
            if (_emu && gui_has_menu_backdrop(_emu)) {
                memcpy(dst, kGuiMenuBackdropNes + y * kGuiMenuBackdropNesWidth + strip_x, spider_w);
            } else {
                for (int x = 0; x < spider_w; ++x)
                    _overlay->setp(dst + x, bg);
            }
        }

        const int web_x = strip_x + 4;
        for (int y = web_top; y <= spider_y; ++y)
            _overlay->setp(_overlay->_lines[y] + web_x, web);

        for (int row = 0; row < spider_h; ++row) {
            uint8_t bits = kSpider[row];
            for (int col = 0; col < spider_w; ++col) {
                if (bits & (0x80 >> col))
                    _overlay->setp(_overlay->_lines[spider_y + row] + strip_x + col, spider);
            }
        }
    }

    void restore_menu_pixel_row(int row)
    {
        if (!_overlay || !_overlay->_lines)
            return;

        const int x0 = _overlay->OVERLAY_ORIGIN_X * 8;
        const int width = _overlay->OVERLAY_WIDTH * 8;
        const int y0 = _overlay->OVERLAY_ORIGIN_Y * 8 + row * 8;
        const uint8_t bg = (_emu && _emu->flavor == EMU_NES) ? 0x0D : 0x00;

        for (int py = 0; py < 8; ++py) {
            uint8_t* dst = _overlay->_lines[y0 + py] + x0;
            if (_emu && gui_has_menu_backdrop(_emu)) {
                memcpy(dst, kGuiMenuBackdropNes + (y0 + py) * kGuiMenuBackdropNesWidth + x0, width);
            } else {
                for (int x = 0; x < width; ++x)
                    _overlay->setp(dst + x, bg);
            }
        }
    }

    void draw_tab_chain()
    {
        if (!_overlay || !_overlay->_lines)
            return;

        if (use_menu_backbuffer()) {
            restore_menu_pixel_row(3);
        } else {
            restore_menu_pixel_row(0);
            restore_menu_pixel_row(2);
            restore_menu_pixel_row(3);
        }
        draw_tab_chain_links();
    }

    void draw_tab_chain_links()
    {
        if (!_overlay || !_overlay->_lines)
            return;

        const int row = 3;
        const int x0 = _overlay->OVERLAY_ORIGIN_X * 8;
        const int width = _overlay->OVERLAY_WIDTH * 8;
        const int y0 = _overlay->OVERLAY_ORIGIN_Y * 8 + row * 8;
        const uint8_t link_px = (_emu && _emu->flavor == EMU_NES) ? 0x20 : 0xFF;
        static const char* kChainTile[8] = {
            "................",
            "XXXX........XXXX",
            "....XXXXXXXX....",
            "...X........X...",
            "....X.XXX.X.....",
            "X.XX.......XXXXX",
            "................",
            "................"
        };

        auto put = [&](int x, int y, uint8_t c) {
            if (x < x0 || x >= x0 + width || y < y0 || y >= y0 + 8)
                return;
            _overlay->setp(_overlay->_lines[y] + x, c);
        };

        auto draw_tile = [&](int x) {
            for (int row = 0; row < 8; ++row) {
                for (int col = 0; col < 16; ++col) {
                    if (kChainTile[row][col] == 'X')
                        put(x + col, y0 + row, link_px);
                }
            }
        };

        for (int x = x0 - 12; x < x0 + width; x += 12)
            draw_tile(x);
    }

    void draw_files()
    {
        const int first_visible = max(0, _scroll);
        const int max_visible_rows = visible_item_rows();
        const int last_visible = min((int)_files.size(), first_visible + max_visible_rows);
        const int max_text_width = _overlay->OVERLAY_WIDTH - 2;
        char line_buf[64];

        for (int y = 4; y < _overlay->OVERLAY_HEIGHT - 2; ++y)
            _overlay->fill(y, 0, _overlay->OVERLAY_WIDTH - 1, ' ');

        for (int m = first_visible; m < last_visible; ++m) {
            const string& entry = _files[m];
            const string display = media_display_name(entry);
            int src_offset = 0;
            if (m == _hilited)
                src_offset = selected_game_marquee_offset((int)display.size(), max_text_width);
            int len = max(0, (int)display.size() - src_offset);
            if (len > max_text_width)
                len = max_text_width;
            if (len > (int)sizeof(line_buf) - 1)
                len = (int)sizeof(line_buf) - 1;
            memcpy(line_buf, display.c_str() + src_offset, len);
            line_buf[len] = 0;
            draw_item(m, line_buf, m == _hilited);
        }
        _overlay->fill(_overlay->OVERLAY_HEIGHT - 2, 0, _overlay->OVERLAY_WIDTH, ' ');
        const char* help;
        if (_browse_dir.empty())
            help = is_gb_gui() ? "A:LOAD" : "A:LOAD  SEL:INFO";
        else
            help = is_gb_gui() ? "A:LOAD  B:BACK" : "A:LOAD  B:BACK  SEL:INFO";
        _overlay->plot_str(help, 1, _overlay->OVERLAY_HEIGHT - 2);
    }

    const char* midi_label_for_index(int i)
    {
        static const char* names[8] = {
            "UP","DOWN","LEFT","RIGHT","A","B","START","SELECT"
        };
        if (i >= 0 && i < 8) return names[i];
        return "";
    }

    void format_note_name(uint8_t n, char* out, int outlen)
    {
        static const char* nn[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        if (outlen <= 0) return;
        if (n > 127) { out[0] = 0; return; }
        int pc = n % 12;
        int oct = (n / 12) - 1;   // MIDI 60 -> C4
        sprintf(out, "%s%d", nn[pc], oct);
    }

    void draw_midi()
    {
        char line[64];
        char value[32];
        int8_t ch = midi_get_channel();
        uint8_t cc1, cc2; midi_get_cc_numbers(&cc1, &cc2);
        int8_t sprite_cc = midi_get_visualizer_sprite_cc();
        uint8_t notes[8]; midi_get_note_map(notes);
        uint8_t auto_mode = midi_get_auto_mode();
        int8_t p1 = -1, p2 = -1, tri = -1, noi = -1;
        int8_t duty1 = -1, duty2 = -1;
        uint8_t pb_range = 0;
        int8_t noise_cc = -1;
        int8_t wave_cc = -1;
        int8_t env_mode_cc = -1, env_decay_cc = -1, env_loop_cc = -1, length_cc = -1;
        apu_midi_get_channel_map(&p1, &p2, &tri, &noi);
        apu_midi_get_duty_cc(&duty1, &duty2);
        pb_range = apu_midi_get_pb_default_range();
        apu_midi_get_noise_mode_cc(&noise_cc);
        gb_midi_get_waveform_cc(&wave_cc);
        apu_midi_get_env_cc(&env_mode_cc, &env_decay_cc, &env_loop_cc, &length_cc);

        auto draw_section_header = [&](int row_index, int section) {
            snprintf(line, sizeof(line), "%c %s", _midi_section_open[section] ? 'v' : '>', midi_section_title(section));
            draw_item(row_index, line, row_index == _hilited);
        };

        auto fmt_channel = [](int8_t channel, char* out, int outlen) {
            if (channel < 0)
                snprintf(out, outlen, "Off");
            else
                snprintf(out, outlen, "Ch %d", (int)channel + 1);
        };

        auto fmt_cc = [](int8_t cc, char* out, int outlen) {
            if (cc < 0)
                snprintf(out, outlen, "Off");
            else
                snprintf(out, outlen, "%d", (int)cc);
        };

        for (int y = 4; y < _overlay->OVERLAY_HEIGHT - 2; ++y)
            _overlay->fill(y, 0, _overlay->OVERLAY_WIDTH - 1, ' ');

        int row_count = count();
        for (int row = 0; row < row_count; ++row) {
            MidiVisibleRow visible = {};
            if (!midi_row_at(row, &visible))
                continue;

            if (visible.kind == MIDI_ROW_SECTION) {
                draw_section_header(row, visible.section);
                continue;
            }

            if (visible.kind == MIDI_ROW_RESET) {
                snprintf(line, sizeof(line), "Reset to defaults");
                draw_item(row, line, row == _hilited);
                continue;
            }

            const char* label = "";
            value[0] = 0;
            switch (visible.id) {
                case MIDI_VALUE_BUTTONS_CHANNEL:
                    label = "Buttons:";
                    if (ch < 0)
                        snprintf(value, sizeof(value), "Any");
                    else
                        snprintf(value, sizeof(value), "Ch %d", ch + 1);
                    break;
                case MIDI_VALUE_FX_CHANNEL: {
                    label = "Channel:";
                    const int8_t fx_ch = midi_get_fx_channel();
                    if (fx_ch < 0)
                        snprintf(value, sizeof(value), "Any");
                    else
                        snprintf(value, sizeof(value), "Ch %d", fx_ch + 1);
                    break;
                }
                case MIDI_VALUE_AUTO_MODE: {
                    label = "Auto Mode:";
                    const char* modes[3] = {"OFF","CLOCK","NOTE ON"};
                    const int mode_index = (auto_mode <= 2) ? auto_mode : 0;
                    snprintf(value, sizeof(value), "%s", modes[mode_index]);
                    break;
                }
                case MIDI_VALUE_CC_GLITCH_A:
                    label = "CC Glitch A:";
                    snprintf(value, sizeof(value), "%d", (int)cc1);
                    break;
                case MIDI_VALUE_CC_GLITCH_B:
                    label = "CC Glitch B:";
                    snprintf(value, sizeof(value), "%d", (int)cc2);
                    break;
                case MIDI_VALUE_VISUALIZER_SPRITE_CC:
                    label = "Sprite CC:";
                    fmt_cc(sprite_cc, value, sizeof(value));
                    break;
                case MIDI_VALUE_PULSE1_CHAN: {
                    label = use_gb_apu_midi() ? "Square 1:" : "Pulse 1:";
                    fmt_channel(p1, value, sizeof(value));
                    break;
                }
                case MIDI_VALUE_PULSE2_CHAN: {
                    label = use_gb_apu_midi() ? "Square 2:" : "Pulse 2:";
                    fmt_channel(p2, value, sizeof(value));
                    break;
                }
                case MIDI_VALUE_TRIANGLE_CHAN: {
                    label = use_gb_apu_midi() ? "Wave:" : "Triangle:";
                    fmt_channel(tri, value, sizeof(value));
                    break;
                }
                case MIDI_VALUE_NOISE_CHAN: {
                    label = "Noise:";
                    fmt_channel(noi, value, sizeof(value));
                    break;
                }
                case MIDI_VALUE_PULSE1_DUTY_CC:
                    label = use_gb_apu_midi() ? "Square1 Duty:" : "Pulse1 Duty:";
                    fmt_cc(duty1, value, sizeof(value));
                    break;
                case MIDI_VALUE_PULSE2_DUTY_CC:
                    label = use_gb_apu_midi() ? "Square2 Duty:" : "Pulse2 Duty:";
                    fmt_cc(duty2, value, sizeof(value));
                    break;
                case MIDI_VALUE_PITCH_BEND_RANGE:
                    label = "Pitch Bend:";
                    snprintf(value, sizeof(value), "%u st", (unsigned)pb_range);
                    break;
                case MIDI_VALUE_NOISE_SHORT_MODE_CC:
                    label = "Noise Short:";
                    fmt_cc(noise_cc, value, sizeof(value));
                    break;
                case MIDI_VALUE_WAVEFORM_CC:
                    label = "Waveform:";
                    fmt_cc(wave_cc, value, sizeof(value));
                    break;
                case MIDI_VALUE_ENV_MODE_CC:
                    label = "Env Mode:";
                    fmt_cc(env_mode_cc, value, sizeof(value));
                    break;
                case MIDI_VALUE_ENV_DECAY_CC:
                    label = "Env Decay:";
                    fmt_cc(env_decay_cc, value, sizeof(value));
                    break;
                case MIDI_VALUE_ENV_LOOP_CC:
                    label = "Env Loop:";
                    fmt_cc(env_loop_cc, value, sizeof(value));
                    break;
                case MIDI_VALUE_LENGTH_CC:
                    label = "Length CC:";
                    fmt_cc(length_cc, value, sizeof(value));
                    break;
                default: {
                    const int note_index = visible.id - MIDI_VALUE_NOTE_UP;
                    if (note_index >= 0 && note_index < 8) {
                        char note_name[8];
                        format_note_name(notes[note_index], note_name, sizeof(note_name));
                        snprintf(line, sizeof(line), "Note %s:", midi_label_for_index(note_index));
                        label = line;
                        snprintf(value, sizeof(value), "%s", note_name);
                    } else {
                        label = "";
                        value[0] = 0;
                    }
                    break;
                }
            }
            const bool adjustable = midi_value_can_adjust(visible.id);
            draw_adjustable_item(row, label, value, row == _hilited, adjustable, adjustable);
        }

        clear(row_count);
        _overlay->fill(_overlay->OVERLAY_HEIGHT - 2, 0, _overlay->OVERLAY_WIDTH, ' ');
    }

    void draw_system()
    {
        char line[64];
        char value[32];

        for (int y = 4; y < _overlay->OVERLAY_HEIGHT - 2; ++y)
            _overlay->fill(y, 0, _overlay->OVERLAY_WIDTH - 1, ' ');

        const int row_count = count();
        for (int row = 0; row < row_count; ++row) {
            SystemVisibleRow visible = {};
            if (!system_visible_row_at(row, &visible))
                continue;
            if (visible.kind == SYSTEM_VISIBLE_SECTION) {
                snprintf(line, sizeof(line), "%c %s",
                         _system_section_open[visible.section] ? 'v' : '>',
                         system_section_title(visible.section));
                draw_item(row, line, row == _hilited);
                continue;
            }

            const char* label = "";
            value[0] = 0;
            switch (visible.id) {
                case SYSTEM_ROW_MODE:
                    label = "Mode:";
                    snprintf(value, sizeof(value), "%s", output_mode_option_label(output_mode_option_index()));
                    break;
                case SYSTEM_ROW_EMULATOR:
                    label = "Core:";
                    snprintf(value, sizeof(value), "%s", _system_emulator == EMU_GB ? "GB" : "NES");
                    break;
                case SYSTEM_ROW_BT:
                    label = "BT:";
                    snprintf(value, sizeof(value), "%s", bt_transport_user_label(_system_bt_transport));
                    break;
                case SYSTEM_ROW_STANDARD:
                    label = "Video:";
                    snprintf(value, sizeof(value), "%s", _system_standard == NTSC ? "NTSC" : "PAL");
                    break;
                case SYSTEM_ROW_GB_DISPLAY:
                    label = "Display:";
                    snprintf(value, sizeof(value), "%s", _system_gb_display_fullscreen ? "Full" : "Original");
                    break;
                case SYSTEM_ROW_CV_A:
                    label = "CV A:";
                    snprintf(value, sizeof(value), "%s", cv_assignment_label(glitch_controls_get_cv_assignment(GLITCH_CV_A)));
                    break;
                case SYSTEM_ROW_CV_B:
                    label = "CV B:";
                    snprintf(value, sizeof(value), "%s", cv_assignment_label(glitch_controls_get_cv_assignment(GLITCH_CV_B)));
                    break;
                case SYSTEM_ROW_AUDIO_IN:
                    label = "Audio:";
                    snprintf(value, sizeof(value), "%s", audio_assignment_label(glitch_controls_get_audio_assignment()));
                    break;
                case SYSTEM_ROW_MENU_TRIGGER:
                    label = "Menu:";
                    snprintf(value, sizeof(value), "%s", _menu_start_select_trigger ? "Start+Select" : "Hold Select");
                    break;
                case SYSTEM_ROW_X_BUTTON:
                    label = "X Button:";
                    snprintf(value, sizeof(value), "%s", glitch_controls_get_x_button_momentary() ? "Momentary" : "Latch");
                    break;
                case SYSTEM_ROW_Y_BUTTON:
                    label = "Y Button:";
                    snprintf(value, sizeof(value), "%s", glitch_controls_get_y_palette_mode() ? "Palette" : "Glitch");
                    break;
                case SYSTEM_ROW_PALETTE_GLITCH:
                    label = "Palette:";
                    snprintf(value, sizeof(value), "%s", video_palette_glitch_get_enabled() ? "On" : "Off");
                    break;
                case SYSTEM_ROW_APU_GLITCH:
                    label = is_gb_gui() ? "Aud Glitch:" : "Audio Glitch:";
                    snprintf(value, sizeof(value), "%s", glitch_settings_is_apu_enabled() ? "On" : "Off");
                    break;
            }
            const bool adjustable = system_row_adjustable(visible.id);
            draw_adjustable_item(row, label, value, row == _hilited, adjustable, adjustable);
        }

        clear(row_count);

        _overlay->fill(_overlay->OVERLAY_HEIGHT - 2, 0, _overlay->OVERLAY_WIDTH, ' ');
    }

    void clear(int i)
    {
        if (use_menu_backbuffer())
            return;
        while (i < _overlay->OVERLAY_HEIGHT - 2)
            draw_item(i++," ",false);
    }

    void load_info_for_index(int index)
    {
        _info.clear();
        int media_index = media_index_for_entry(index);
        if (media_index >= 0 && media_index < (int)_files.size() &&
            !media_entry_is_folder(_files[media_index]))
            _emu->info(media_full_path(_files[media_index]),_info);
    }

    void open_info_popup()
    {
        if (_tab != TAB_GAMES || is_gb_gui())
            return;
        if (_hilited < 0 || _hilited >= count())
            return;
        load_info_for_index(_hilited);
        if (_info.empty()) {
            msg("NO INFO");
            _dirty = true;
            return;
        }
        _info_popup = true;
        _dirty = true;
        clear_repeat();
    }

    const char* reboot_popup_title() const
    {
        return is_gb_gui() ? "reboot?" : "REBOOT TO APPLY?";
    }

    int reboot_popup_width() const
    {
        return is_gb_gui() ? GB_REBOOT_POPUP_WIDTH : REBOOT_POPUP_WIDTH;
    }

    int reboot_popup_height() const
    {
        return is_gb_gui() ? GB_REBOOT_POPUP_HEIGHT : REBOOT_POPUP_HEIGHT;
    }

    const char* visualizer_popup_title() const
    {
        return is_gb_gui() ? "OPEN VIZ?" : "START VISUALIZER?";
    }

    int visualizer_popup_width() const
    {
        return is_gb_gui() ? GB_VISUALIZER_POPUP_WIDTH : REBOOT_POPUP_WIDTH;
    }

    int visualizer_popup_height() const
    {
        return is_gb_gui() ? GB_VISUALIZER_POPUP_HEIGHT : REBOOT_POPUP_HEIGHT;
    }

    void reboot_popup_rect(int& left, int& right, int& top, int& bottom,
                           int target_width = REBOOT_POPUP_WIDTH,
                           int target_height = REBOOT_POPUP_HEIGHT) const
    {
        const int width = _overlay ? _overlay->OVERLAY_WIDTH : 0;
        const int height = _overlay ? _overlay->OVERLAY_HEIGHT : 0;
        if (width <= target_width) {
            left = 0;
            right = max(0, width - 1);
        } else {
            const int popup_width = min(width - 2, target_width);
            left = max(1, (width - popup_width) / 2);
            right = min(width - 1, left + popup_width - 1);
        }

        const int popup_height = min(target_height, max(5, height - 4));
        top = max(2, (height - popup_height) / 2);
        bottom = min(height - 1, top + popup_height - 1);
    }

    void yes_no_popup_option_metrics(int left, int right, int top, int bottom,
                                     const char* subtitle,
                                     int& option_y,
                                     int& yes_text_x,
                                     int& no_text_x,
                                     int& yes_marker_left_x,
                                     int& yes_marker_right_x,
                                     int& no_marker_left_x,
                                     int& no_marker_right_x) const
    {
        const int inner_left = left + 1;
        const int inner_right = right - 1;
        const int inner_width = inner_right - inner_left + 1;
        const int yes_len = 3;
        const int no_len = 2;
        const int gap = 2;
        const int group_width = 1 + yes_len + 1 + gap + 1 + no_len + 1;
        const int group_x = inner_left + max(0, (inner_width - group_width) / 2);

        option_y = min(bottom - 2, top + (subtitle && subtitle[0] ? 5 : 4));
        yes_marker_left_x = group_x;
        yes_text_x = yes_marker_left_x + 1;
        yes_marker_right_x = yes_text_x + yes_len;
        no_marker_left_x = yes_marker_right_x + 1 + gap;
        no_text_x = no_marker_left_x + 1;
        no_marker_right_x = no_text_x + no_len;
    }

    void draw_yes_no_popup(const char* title, const char* subtitle,
                           int target_width = REBOOT_POPUP_WIDTH,
                           int target_height = REBOOT_POPUP_HEIGHT)
    {
        if (!_overlay || !title)
            return;

        int left, right, top, bottom;
        reboot_popup_rect(left, right, top, bottom, target_width, target_height);
        if (right <= left || bottom <= top)
            return;

        const int inner_left = left + 1;
        const int inner_right = right - 1;
        const int inner_width = inner_right - inner_left + 1;

        _overlay->set_hilite(0);
        for (int y = top; y <= bottom; ++y)
            _overlay->fill(y, left, right + 1, ' ');

        const int title_len = (int)strlen(title);
        _overlay->plot_str(title, inner_left + max(0, (inner_width - title_len) / 2), top + 1);

        if (subtitle && subtitle[0]) {
            const int subtitle_len = (int)strlen(subtitle);
            _overlay->plot_str(subtitle, inner_left + max(0, (inner_width - subtitle_len) / 2), top + 3);
        }

        int option_y, yes_x, no_x, yes_left, yes_right, no_left, no_right;
        yes_no_popup_option_metrics(left, right, top, bottom, subtitle,
                                    option_y, yes_x, no_x,
                                    yes_left, yes_right, no_left, no_right);

        _overlay->plot_str("YES", yes_x, option_y);
        _overlay->plot_str("NO", no_x, option_y);
    }

    void draw_reboot_popup()
    {
        if (!_reboot_popup)
            return;
        draw_yes_no_popup(reboot_popup_title(), 0,
                          reboot_popup_width(), reboot_popup_height());
    }

    void draw_visualizer_popup()
    {
        if (!_visualizer_popup)
            return;
        draw_yes_no_popup(visualizer_popup_title(), 0,
                          visualizer_popup_width(),
                          visualizer_popup_height());
    }

    void draw_midi_confirm_popup()
    {
        if (!_midi_confirm)
            return;
        draw_yes_no_popup("RESET MIDI?", "DEFAULTS?");
    }

    void draw_yes_no_popup_selection_markers(const char* subtitle, bool yes_selected,
                                             int target_width = REBOOT_POPUP_WIDTH,
                                             int target_height = REBOOT_POPUP_HEIGHT)
    {
        if (!_overlay || !_overlay->_lines)
            return;

        int left, right, top, bottom;
        reboot_popup_rect(left, right, top, bottom, target_width, target_height);
        if (right <= left || bottom <= top)
            return;

        int option_y, yes_x, no_x, yes_left, yes_right, no_left, no_right;
        yes_no_popup_option_metrics(left, right, top, bottom, subtitle,
                                    option_y, yes_x, no_x,
                                    yes_left, yes_right, no_left, no_right);

        const int marker_left = yes_selected ? yes_left : no_left;
        const int marker_right = yes_selected ? yes_right : no_right;
        const int ox = _overlay->OVERLAY_ORIGIN_X * 8;
        const int oy = _overlay->OVERLAY_ORIGIN_Y * 8;
        const int y = oy + option_y * 8;
        const uint8_t color = info_border_color();

        draw_mask8(ox + marker_left * 8, y, kSelectedBracketLeft, 0, color);
        draw_mask8(ox + marker_right * 8, y, kSelectedBracketLeft, 0, color, true);
    }

    void draw_reboot_popup_selection_markers()
    {
        if (!_reboot_popup)
            return;
        draw_yes_no_popup_selection_markers(0, _reboot_popup_yes,
                                            reboot_popup_width(), reboot_popup_height());
    }

    void draw_visualizer_popup_selection_markers()
    {
        if (!_visualizer_popup)
            return;
        draw_yes_no_popup_selection_markers(0,
                                            _visualizer_popup_yes,
                                            visualizer_popup_width(),
                                            visualizer_popup_height());
    }

    void draw_midi_confirm_popup_selection_markers()
    {
        if (!_midi_confirm)
            return;
        draw_yes_no_popup_selection_markers("DEFAULTS?", _midi_confirm_yes);
    }

    void draw_info_popup()
    {
        if (!_info_popup)
            return;

        const int left = 2;
        const int right = _overlay->OVERLAY_WIDTH - 3;
        const int top = 4;
        const int bottom = _overlay->OVERLAY_HEIGHT - 3;
        const int inner_left = left + 1;
        const int inner_right = right - 1;
        const int inner_width = inner_right - inner_left + 1;

        for (int y = top; y <= bottom; ++y)
            _overlay->fill(y, left, right + 1, ' ');

        _overlay->set_hilite(1);
        _overlay->fill(top + 1, inner_left, right, ' ');
        _overlay->fill(bottom - 1, inner_left, right, ' ');
        _overlay->plot_str("INFO", inner_left + max(0, (inner_width - 4) / 2), top + 1);
        _overlay->plot_str("X/Y/A/B CLOSE", inner_left + max(0, (inner_width - 13) / 2), bottom - 1);
        _overlay->set_hilite(0);

        int y = top + 3;
        const int max_lines = max(0, bottom - top - 4);
        char line_buf[64];
        for (int i = 0; i < (int)_info.size() && i < max_lines; ++i, ++y) {
            const string& line = _info[i];
            int len = (int)line.size();
            if (len > inner_width)
                len = inner_width;
            if (len > (int)sizeof(line_buf) - 1)
                len = (int)sizeof(line_buf) - 1;
            memcpy(line_buf, line.c_str(), len);
            line_buf[len] = 0;
            _overlay->plot_str(line_buf, inner_left, y);
        }
    }

    uint8_t info_border_color() const
    {
        switch (_emu ? _emu->flavor : EMU_NES) {
            case EMU_NES: return 0x23;
            case EMU_GB:
            default: return 0xFF;
        }
    }

    static bool mask8_bit(const uint8_t* mask, int x, int y, int rotation, bool flip_h = false)
    {
        if (flip_h)
            x = 7 - x;

        int sx = x;
        int sy = y;
        switch (rotation & 3) {
            case 1:
                sx = y;
                sy = 7 - x;
                break;
            case 2:
                sx = 7 - x;
                sy = 7 - y;
                break;
            case 3:
                sx = 7 - y;
                sy = x;
                break;
            default:
                break;
        }
        return (mask[sy] & (0x80 >> sx)) != 0;
    }

    void draw_mask8(int px, int py, const uint8_t* mask, int rotation, uint8_t color, bool flip_h = false)
    {
        if (!_overlay || !_overlay->_lines)
            return;

        for (int y = 0; y < 8; ++y) {
            uint8_t* dst = _overlay->_lines[py + y] + px;
            for (int x = 0; x < 8; ++x) {
                if (mask8_bit(mask, x, y, rotation, flip_h))
                    _overlay->setp(dst + x, color);
            }
        }
    }

    void draw_popup_border_rect(int left, int right, int top, int bottom)
    {
        if (!_overlay || !_overlay->_lines)
            return;

        if (right <= left || bottom <= top)
            return;

        static const uint8_t kInfoBorderLineVertical[8] = {
            0b00011000,
            0b00011000,
            0b00011000,
            0b00011000,
            0b00011000,
            0b00011000,
            0b00011000,
            0b00011000
        };
        static const uint8_t kInfoBorderCornerTopRight[8] = {
            0b11100000,
            0b10100010,
            0b10111100,
            0b10100100,
            0b10100100,
            0b10111111,
            0b10000001,
            0b11111111
        };

        const uint8_t color = info_border_color();
        const int ox = _overlay->OVERLAY_ORIGIN_X * 8;
        const int oy = _overlay->OVERLAY_ORIGIN_Y * 8;
        const int left_px = ox + left * 8;
        const int right_px = ox + right * 8;
        const int top_px = oy + top * 8;
        const int bottom_px = oy + bottom * 8;

        for (int x = left + 1; x < right; ++x) {
            const int px = ox + x * 8;
            draw_mask8(px, top_px, kInfoBorderLineVertical, 1, color);
            draw_mask8(px, bottom_px, kInfoBorderLineVertical, 1, color);
        }

        for (int y = top + 1; y < bottom; ++y) {
            const int py = oy + y * 8;
            draw_mask8(left_px, py, kInfoBorderLineVertical, 0, color);
            draw_mask8(right_px, py, kInfoBorderLineVertical, 0, color);
        }

        draw_mask8(right_px, top_px, kInfoBorderCornerTopRight, 0, color);
        draw_mask8(right_px, bottom_px, kInfoBorderCornerTopRight, 1, color);
        draw_mask8(left_px, bottom_px, kInfoBorderCornerTopRight, 2, color);
        draw_mask8(left_px, top_px, kInfoBorderCornerTopRight, 3, color);
    }

    void draw_info_popup_border()
    {
        if (!_info_popup || !_overlay)
            return;

        draw_popup_border_rect(2, _overlay->OVERLAY_WIDTH - 3, 4, _overlay->OVERLAY_HEIGHT - 3);
    }

    void draw_reboot_popup_border()
    {
        if (!_reboot_popup || !_overlay)
            return;

        int left, right, top, bottom;
        reboot_popup_rect(left, right, top, bottom, reboot_popup_width(), reboot_popup_height());
        draw_popup_border_rect(left, right, top, bottom);
    }

    void draw_visualizer_popup_border()
    {
        if (!_visualizer_popup || !_overlay)
            return;

        int left, right, top, bottom;
        reboot_popup_rect(left, right, top, bottom,
                          visualizer_popup_width(),
                          visualizer_popup_height());
        draw_popup_border_rect(left, right, top, bottom);
    }

    void draw_midi_confirm_popup_border()
    {
        if (!_midi_confirm || !_overlay)
            return;

        int left, right, top, bottom;
        reboot_popup_rect(left, right, top, bottom);
        draw_popup_border_rect(left, right, top, bottom);
    }

    string get_pref(const string& key)
    {
        char buf[256] = {0};
        sys_get_pref((_path[1]+key).c_str(),buf,sizeof(buf)-1); // keys are limited to 15 bytes. great
        return buf;
    }

    void set_pref(const string& key, const string& value)
    {
        sys_set_pref((_path[1]+key).c_str(),value.c_str());     // first char discrminates current emus at least
    }

    bool build_insert_path(const string& leaf)
    {
        const bool absolute = media_entry_is_absolute(leaf);
        const size_t wanted = absolute ? leaf.size() : (_path.size() + 1 + leaf.size());
        try {
            if (_insert_path.capacity() < wanted)
                _insert_path.reserve(wanted + 16);
            if (absolute) {
                _insert_path.assign(leaf);
            } else {
                _insert_path.assign(_path);
                _insert_path.push_back('/');
                _insert_path.append(leaf);
            }
            return true;
        } catch (...) {
            printf("GUI::build path failed len=%u cap=%u path=%s file=%s\n",
                   (unsigned)wanted,
                   (unsigned)_insert_path.capacity(),
                   _path.c_str(),
                   leaf.c_str());
            msg("LOAD FAILED");
            return false;
        }
    }

    void insert_default(const char* path)
    {
        read_directory(path);
        if (_files.empty()) {
            _emu->make_default_media(_path);
            read_directory(path);
        }

        int sliding_blaster = -1;
        if (_emu && _emu->flavor == EMU_NES) {
            const char* legacy_default_roms[] = {
                "chase.nes",
                "sokoban.nes",
                "tokumaru_raycast.nes",
                "sliding_blaster.nes",
            };
            bool removed_legacy_defaults = false;
            for (const char* leaf : legacy_default_roms) {
                if (find_file(leaf) != -1) {
                    string legacy_path = media_full_path(leaf);
                    remove(legacy_path.c_str());
                    removed_legacy_defaults = true;
                }
            }

            if (removed_legacy_defaults)
                read_directory(path);

            sliding_blaster = find_file(NES_DEFAULT_ROM);
            if (sliding_blaster == -1 && _files.empty()) {
                _emu->make_default_media(_path);
                read_directory(path);
                sliding_blaster = find_file(NES_DEFAULT_ROM);
            }
        }
        if (_emu && _emu->flavor == EMU_NES &&
            sliding_blaster != -1 && default_rom_needs_rebuild(NES_DEFAULT_ROM)) {
            string bad = media_full_path(NES_DEFAULT_ROM);
            remove(bad.c_str());
            _emu->make_default_media(_path);
            read_directory(path);
            sliding_blaster = find_file(NES_DEFAULT_ROM);
            if (sliding_blaster != -1 && !valid_nes_rom_leaf(NES_DEFAULT_ROM))
                sliding_blaster = -1;
        }
        int gb_default = -1;
        if (_emu && _emu->flavor == EMU_GB) {
            int old_postie = find_file("postie.gbc");
            if (old_postie != -1) {
                string old_path = media_full_path("postie.gbc");
                remove(old_path.c_str());
                read_directory(path);
            }
            gb_default = find_file(GB_DEFAULT_ROM);
            if (gb_default == -1 && _files.empty()) {
                _emu->make_default_media(_path);
                read_directory(path);
                gb_default = find_file(GB_DEFAULT_ROM);
            }
            if (gb_default != -1 && default_rom_needs_rebuild(GB_DEFAULT_ROM)) {
                string bad = media_full_path(GB_DEFAULT_ROM);
                remove(bad.c_str());
                _emu->make_default_media(_path);
                read_directory(path);
                gb_default = find_file(GB_DEFAULT_ROM);
                if (gb_default != -1 && !valid_gb_rom_leaf(GB_DEFAULT_ROM))
                    gb_default = -1;
            }
        }
        int recent = find_file(get_pref("recent"));

        if (_files.empty()) {
            _visible = true;
            force_full_menu_redraw();
        } else {
            const int default_game_index = (sliding_blaster != -1 ? sliding_blaster :
                                           (gb_default != -1 ? gb_default :
                                           (recent == -1 ? 0 : recent)));
            _tab_hilited[TAB_GAMES] = default_game_index;
            _tab_scroll[TAB_GAMES] = 0;
            _tab_hilited[TAB_SYSTEM] = SYSTEM_ROW_MODE;
            _tab_scroll[TAB_SYSTEM] = 0;
            _tab = TAB_GAMES;
            _hilited = default_game_index;
            _scroll = 0;
            _visible = true;
            force_full_menu_redraw();
        }
        close_info_popup();
    }

    int update_video(bool fill_visualizer_audio_before_render = false)
    {
        int emu_frames = 0;
        bool commit_menu_frame = false;
        // UI delays and repeat rates are measured in displayed video frames,
        // not in emulator-loop iterations.
        tick_ui_frame();
        if (fill_visualizer_audio_before_render && _visible)
            update_audio();
        if (_visible) {
            const bool wants_backbuffer = use_menu_backbuffer();
            const bool has_backbuffer =
                wants_backbuffer && ensure_active_menu_backbuffer();
            const bool live_ntsc_fallback = wants_backbuffer && !has_backbuffer;
            const bool redraw_menu = !live_ntsc_fallback || _dirty;
            if (!redraw_menu) {
                _overlay->set_lines(gui_display_video_buffer());
            } else if (live_ntsc_fallback && _live_partial_dirty) {
                const bool redraw_spider_after_rows =
                    _live_scrollbar_dirty || _live_dirty_rows != 0;
                _overlay->set_lines(gui_display_video_buffer());
                menu();
                scrollbar();
                switch (_tab) {
                    case TAB_GAMES: draw_files(); break;
                    case TAB_MIDI: draw_midi(); break;
                    case TAB_SYSTEM: draw_system(); break;
                }
                draw_info_popup();
                draw_reboot_popup();
                draw_visualizer_popup();
                draw_midi_confirm_popup();
                for (int row = 0; row < _overlay->OVERLAY_HEIGHT && row < 32; ++row) {
                    if (_live_dirty_rows & (1u << row))
                        _overlay->update_row(row);
                }
                if (redraw_spider_after_rows)
                    draw_spider_scrollbar();
                _dirty = false;
                clear_live_partial_dirty();
            } else {
                uint8_t** const menu_target =
                    has_backbuffer ? menu_render_lines()
                                   : gui_display_video_buffer();
                _overlay->set_lines(menu_target);
                // Only a page being displayed while it is drawn needs this.
                const bool compose_header =
                    !has_backbuffer && can_compose_live_header();
                const int header_y0 = _overlay->OVERLAY_ORIGIN_Y * 8;
                if (compose_header)
                    gui_draw_menu_backdrop(_overlay->_lines, _emu, header_y0, header_y0 + 32);
                else
                    gui_draw_menu_backdrop(_overlay->_lines, _emu);
                menu();
                scrollbar();
                switch (_tab) {
                    case TAB_GAMES: draw_files(); break;
                    case TAB_MIDI: draw_midi(); break;
                    case TAB_SYSTEM: draw_system(); break;
                }
                draw_info_popup();
                draw_reboot_popup();
                draw_visualizer_popup();
                draw_midi_confirm_popup();
                if (use_menu_backbuffer() || compose_header)
                    _overlay->update_from_row(4);
                else
                    _overlay->update();
                if (compose_header) {
                    compose_live_header(gui_has_menu_backdrop(_emu));
                } else {
                    draw_ntsc_menu_header();
                    draw_selected_tab_brackets();
                }
                draw_info_popup_border();
                draw_reboot_popup_border();
                draw_visualizer_popup_border();
                draw_midi_confirm_popup_border();
                draw_reboot_popup_selection_markers();
                draw_visualizer_popup_selection_markers();
                draw_midi_confirm_popup_selection_markers();
                if (!compose_header)
                    draw_tab_chain();
                draw_spider_scrollbar();
                // Publish the target selected at the start of this menu frame.
                commit_menu_frame =
                    has_backbuffer && _overlay->_lines == menu_target;
                _dirty = false;
                clear_live_partial_dirty();
            }
        } else {
            emu_frames = _emu->update();
            update_music_game_boot_grace();
            apply_output_mode_audio_state();
            if (midi_visualizer_is_active()) {
                // Refill audio before variable-cost visualizer rendering.
                if (fill_visualizer_audio_before_render)
                    update_audio();
                gui_render_visualizer_frame();
                _overlay->set_lines(gui_display_video_buffer());
            }
        }

        // Status messages overlay both menus and gameplay.
        if (_msg.size()) {
            if (--_msg_ticks == 0) {
                _overlay->erase_msg();
                _msg.clear();
                if (_visible)
                    force_full_menu_redraw();
            } else
                _overlay->draw_msg(_msg);
        }
        if (commit_menu_frame)
            commit_menu_render();
        return emu_frames;
    }

    void update_audio()
    {
        int16_t* abuffer = s_gui_audio_work_buffer;
        const int abuffer_bytes = sizeof(s_gui_audio_work_buffer);
        int format = _emu->audio_format >> 8;
        int sample_count = 0;
        const bool visualizer_active = midi_visualizer_is_active();

        // Gameplay uses the low-overhead audio path.
        if (!_visible && !visualizer_active) {
            sample_count = _emu->audio_buffer(abuffer, abuffer_bytes);
            audio_write_16(abuffer,sample_count,format);
            return;
        }

        // Menu open, or visualizer with the synth off: output silence.
        if (!_visible && apu_midi_current_enabled()) {
            sample_count = _emu->audio_buffer(abuffer, abuffer_bytes);
        } else {
            sample_count = _emu->frame_sample_count();
            format = 1;
            memset(abuffer,0,(size_t)abuffer_bytes);
        }
        audio_write_16(abuffer,sample_count,format);
        if (visualizer_active &&
            (_emu->flavor == EMU_NES || _emu->flavor == EMU_GB) &&
            apu_midi_current_enabled())
            top_up_music_visualizer_audio(abuffer, abuffer_bytes, format);
    }

    void top_up_music_visualizer_audio(int16_t* buffer, int buffer_bytes, int format)
    {
        if (!_emu || !buffer || buffer_bytes <= 0)
            return;
        if (_emu->flavor != EMU_NES && _emu->flavor != EMU_GB)
            return;

        const uint32_t capacity = audio_queue_capacity();
        if (capacity == 0)
            return;

        const uint32_t refill_threshold =
            min<uint32_t>(MUSIC_AUDIO_QUEUE_REFILL_THRESHOLD_SAMPLES,
                          capacity - 1u);
        const uint32_t target =
            min<uint32_t>(MUSIC_AUDIO_QUEUE_TARGET_SAMPLES,
                          capacity - 1u);
        if (audio_queue_level() >= refill_threshold)
            return;

        int chunks = 0;
        while (chunks < 3 && audio_queue_level() < target) {
            const uint32_t queued = audio_queue_level();
            const uint32_t missing = target - queued;
            const int request_samples =
                min<int>((int)missing, buffer_bytes / (int)sizeof(buffer[0]));
            if (request_samples <= 0)
                break;
            const int request_bytes =
                request_samples * (int)sizeof(buffer[0]);
            int n = _emu->audio_buffer(buffer, request_bytes);
            if (n <= 0)
                break;
            audio_write_16(buffer, n, format);
            ++chunks;
        }
    }

};

Overlay _overlay;
GUI _gui;

static uint8_t gui_visualizer_clear_color(int flavor)
{
    return flavor == EMU_NES ? 0x0D : 0x00;
}

static uint8_t** gui_base_video_buffer()
{
    return _gui._emu ? _gui._emu->video_buffer() : 0;
}

static uint8_t** gui_visualizer_display_buffer()
{
    if (_gui.visualizer_backbuffer_supported())
        return _gui.visualizer_display_lines();
    if (primary_buffer)
        return primary_buffer->line;
    return gui_base_video_buffer();
}

static uint8_t** gui_visualizer_render_buffer()
{
    if (_gui.visualizer_backbuffer_supported())
        return _gui.visualizer_render_lines();
    return gui_visualizer_display_buffer();
}

static bool gui_has_menu_backdrop(const Emu* emu)
{
    return emu &&
           emu->flavor == EMU_NES &&
           emu->width == kGuiMenuBackdropNesWidth &&
           emu->height == kGuiMenuBackdropNesHeight;
}

// Lines in [skip_y0, skip_y1) are left for the caller to draw.
static void gui_draw_menu_backdrop(uint8_t** lines, const Emu* emu, int skip_y0, int skip_y1)
{
    if (!lines || !gui_has_menu_backdrop(emu))
        return;
    for (int y = 0; y < kGuiMenuBackdropNesHeight; ++y)
        if (y < skip_y0 || y >= skip_y1)
            memcpy(lines[y], kGuiMenuBackdropNes + (y * kGuiMenuBackdropNesWidth), kGuiMenuBackdropNesWidth);
}

static uint8_t** gui_display_video_buffer()
{
    if (_gui._visible && _gui.use_menu_backbuffer())
        return _gui.menu_display_lines();
    if (midi_visualizer_is_active())
        return gui_visualizer_display_buffer();
    return gui_base_video_buffer();
}

static void gui_reset_visualizer_buffers()
{
    uint8_t** lines = gui_visualizer_render_buffer();
    if (!lines || !_gui._emu)
        return;
    // GB and NES packed-canvas commits repaint the complete target.
    // A live NES scanout page cannot be cleared here.
    if (_gui._emu->flavor == EMU_GB ||
        _gui._emu->flavor == EMU_NES)
        return;
    const uint8_t clear = gui_visualizer_clear_color(_gui._emu->flavor);
    int width = _gui._emu->width;
    int height = _gui._emu->height;
    if (primary_buffer && lines == primary_buffer->line) {
        width = primary_buffer->width;
        height = primary_buffer->height;
    }
    for (int y = 0; y < height; ++y)
        if (lines[y])
            memset(lines[y], clear, (size_t)width);
}

static bool gui_render_visualizer_frame()
{
    const bool nes_visualizer =
        _gui._emu && _gui._emu->flavor == EMU_NES;
    const bool nes_backbuffer =
        nes_visualizer && _gui.use_visualizer_backbuffer();
    uint8_t** target =
        nes_backbuffer ? _gui.visualizer_render_lines()
                       : gui_visualizer_display_buffer();
    if (!target)
        return false;
    int width = _gui._emu ? _gui._emu->width : 0;
    int height = _gui._emu ? _gui._emu->height : 0;
    int pitch = width;
    if (primary_buffer && target == primary_buffer->line) {
        width = primary_buffer->width;
        height = primary_buffer->height;
        pitch = primary_buffer->pitch;
    }
    const bool packed_canvas =
        _gui._emu &&
        (_gui._emu->flavor == EMU_GB ||
         _gui._emu->flavor == EMU_NES);
    const bool rendered =
        midi_visualizer_update(target, width, height, pitch,
                               packed_canvas
                                   ? reinterpret_cast<uint8_t*>(
                                         s_gui_audio_work_buffer)
                                   : nullptr,
                               packed_canvas
                                   ? sizeof(s_gui_audio_work_buffer)
                                   : 0);
    if (rendered)
        _gui.commit_visualizer_render();
    return rendered;
}

void gui_start(Emu* emu, const char* path)
{
    _gui._emu = emu;
    _gui._overlay = &_overlay;
    _gui.load_system_prefs();
    _gui.apu_midi_disable_all();
    midi_visualizer_deactivate();
    emu->set_visualizer_active(false);
    midi_visualizer_init(emu->width, emu->height, emu->flavor);
    _overlay.init(emu->video_buffer(),emu->width,emu->height,emu->flavor);
    _gui.prepare_menu_backbuffer();
    _gui.insert_default(path);
    if (_gui._visible && !gui_has_menu_backdrop(emu))
        _overlay.frame();
}

uint8_t** gui_video_buffer()
{
    uint8_t** loading_lines = _gui.loading_lines();
    if (loading_lines)
        return loading_lines;

    uint8_t** display_lines = gui_display_video_buffer();
    _overlay.set_lines(display_lines);
    _gui.finish_loading_commit();
    _gui.finish_visualizer_commit();
    return _overlay._lines;
}

bool gui_video_needs_host_commit()
{
    if (_gui.loading_lines())
        return true;
    if (_gui.loading_commit_pending())
        return true;
    if (_gui._visible)
        return true;
    if (midi_visualizer_is_active()) {
        if (_gui._emu && _gui._emu->flavor == EMU_NES)
            return _gui.visualizer_backbuffer_ready();
        return true;
    }
    return !_gui._msg.empty();
}

bool gui_video_commit_needs_vblank_sync()
{
    return _gui.use_menu_backbuffer() || _gui.visualizer_commit_pending();
}

static uint16_t _generic_pad1_state = 0;
static bool _generic_pad1_valid = false;
static uint8_t _menu_select_hold = 0;
static uint8_t _menu_select_latched = 0;
static bool _last_panel_glitch_x = false;
static bool _last_panel_glitch_y = false;
static uint16_t _last_cv_gate_buttons = 0;
static int _cv_gate_keycodes[16] = {0};
static void update_generic_menu_hold();
static void update_panel_glitch_menu_keys();
static void update_cv_gate_keys();

int gui_update()
{
    video_palette_glitch_set_game_active(_gui._output_mode == OUTPUT_MODE_GAMEPLAY &&
                                         !_gui._visible &&
                                         !midi_visualizer_is_active());
    if (!_gui.loading_lines()) {
        // Hold the published page until update_video() selects the render target.
        _overlay.set_lines(gui_display_video_buffer());
    }
    uint8_t buf[64];
    // Read controls before running expensive emulator work.
    int n = 0;
    for (int i = 0; i < 8; ++i) {
        n = hid_get(buf,sizeof(buf));
        if (n <= 0)
            break;
        gui_hid(buf,n);
    }

    if (_gui._visible)
        (void)get_hid_midi(buf);

    n = get_hid_all(buf);
    if (n > 0)
        gui_hid(buf,n);

    static uint32_t last_menu_hold_frame = UINT32_MAX;
    const uint32_t menu_hold_frame = (uint32_t)_frame_counter;
    if (last_menu_hold_frame != menu_hold_frame) {
        last_menu_hold_frame = menu_hold_frame;
        update_generic_menu_hold();
    }
    glitch_controls_update();
    update_panel_glitch_menu_keys();
    update_cv_gate_keys();
    nes_glitch_update();
    gb_glitch_sync_state();

    int emu_frames = 0;
    const bool visualizer_active = midi_visualizer_is_active();
    if (visualizer_active) {
        // update_video() advances MIDI/APU state, fills audio, then rasterizes.
        emu_frames = _gui.update_video(true);
    } else if (_gui._emu && _gui._emu->flavor == EMU_GB &&
               !GUI_GB_AUDIO_BEFORE_VIDEO) {
        emu_frames = _gui.update_video();
        _gui.update_audio();
    } else {
        _gui.update_audio();
        emu_frames = _gui.update_video();
    }

    video_set_gui_menu_visible(_gui._visible ? 1 : 0);
    return emu_frames;
}

void gui_music_audio_pre_sync()
{
    /* Drain incoming MIDI before topping up either live synth. */
    if (!_gui._emu ||
        (_gui._emu->flavor != EMU_GB && _gui._emu->flavor != EMU_NES) ||
        !midi_visualizer_is_active() ||
        !_gui.apu_midi_current_enabled())
        return;

    uint8_t midi_buf[64];
    (void)get_hid_midi(midi_buf);

    int format = _gui._emu->audio_format >> 8;
    _gui.top_up_music_visualizer_audio(s_gui_audio_work_buffer,
                                       sizeof(s_gui_audio_work_buffer), format);
}

// Refill live audio during long visualizer renders using the unused tail of
// the shared work buffer. MIDI parsing stays outside rendering.
void gui_music_audio_mid_render_topup(int16_t* buffer, int buffer_bytes)
{
    // One queue check keeps this render-loop path short.
    if (audio_queue_level() >= MUSIC_AUDIO_QUEUE_REFILL_THRESHOLD_SAMPLES)
        return;
    if (!_gui._emu || !buffer ||
        buffer_bytes < (int)(16 * sizeof(int16_t)))
        return;
    if ((_gui._emu->flavor != EMU_GB && _gui._emu->flavor != EMU_NES) ||
        !midi_visualizer_is_active() ||
        !_gui.apu_midi_current_enabled())
        return;

    const int format = _gui._emu->audio_format >> 8;
    _gui.top_up_music_visualizer_audio(buffer, buffer_bytes, format);
}

void gui_key(int keycode, int pressed, int mods)
{
    if (!_gui.key(keycode,pressed,mods))
        _gui._emu->key(keycode,pressed,mods);
}

//==================================================================
//==================================================================
// hid keyboard events
// 01 mods XX k k k k k k
// 03 XXXXXXXXX // Apple/Android modifier bitmap

// http://www.usb.org/developers/devclass_docs/Hut1_12v2.pdf
// Same as SDL scancodes

static int _last_key = 0;
static void keyboard(const uint8_t* d, int len)
{
    int mods = d[1];          // HID modifier bitmap
    int key_code = d[3];      // first pressed key
    if (key_code != _last_key) {
        if (key_code) {
            if (_last_key)
                gui_key(_last_key,0,mods);
            gui_key(key_code,1,mods);
            _last_key = key_code;
        } else {
            gui_key(_last_key,0,mods);
            _last_key = 0;
        }
    }
}

// Handle WII/IR controllers for operating GUI
static int _last_pad = 0;
static void pad_key(int mask, int state, int key)
{
    if ((_last_pad & mask) == (state & mask))
        return;
    gui_key(key,state & mask,0);
}

static void update_generic_menu_hold()
{
    if (!_generic_pad1_valid) {
        _menu_select_hold = 0;
        _menu_select_latched = 0;
        return;
    }

    const bool select_down = (_generic_pad1_state & GENERIC_SELECT) != 0;
    const bool start_down = (_generic_pad1_state & GENERIC_START) != 0;
    if (_gui._menu_start_select_trigger) {
        _menu_select_hold = 0;
        if (start_down && select_down) {
            if (!_menu_select_latched) {
                _menu_select_latched = 1;
                gui_key(58, 1, 0);
            }
        } else {
            _menu_select_latched = 0;
        }
        return;
    }

    const uint8_t select_long_press_frames = 28; // ~450ms at ~60fps
    if (select_down) {
        if (_menu_select_hold < 255)
            ++_menu_select_hold;
        if (_menu_select_hold >= select_long_press_frames && !_menu_select_latched) {
            _menu_select_latched = 1;
            _menu_select_hold = select_long_press_frames;
            gui_key(58, 1, 0);
        }
    } else {
        _menu_select_hold = 0;
        _menu_select_latched = 0;
    }
}

static void update_panel_glitch_menu_keys()
{
    bool panel_x = glitch_controls_get_panel_x_pressed();
    bool panel_y = glitch_controls_get_panel_y_pressed();

#ifdef ESP_PLATFORM
    if (!_gui._visible && midi_visualizer_is_active()) {
        panel_x = !gpio_get_level((gpio_num_t)GLITCH_X_PIN);
        panel_y = !gpio_get_level((gpio_num_t)GLITCH_Y_PIN);
    }
#endif

    if (_gui._visible || midi_visualizer_is_active()) {
        if (panel_x != _last_panel_glitch_x)
            gui_key(GUI_KEY_FACE_X, panel_x ? 1 : 0, 0);
        if (panel_y != _last_panel_glitch_y)
            gui_key(GUI_KEY_FACE_Y, panel_y ? 1 : 0, 0);
    }

    _last_panel_glitch_x = panel_x;
    _last_panel_glitch_y = panel_y;
}

static int cv_gate_button_index(uint16_t mask)
{
    for (int i = 0; i < 16; ++i) {
        if (mask == (uint16_t)(1u << i))
            return i;
    }
    return -1;
}

static int cv_gate_key_for_button(uint16_t mask)
{
    switch (mask) {
        case GENERIC_LEFT:   return 80;
        case GENERIC_RIGHT:  return 79;
        case GENERIC_UP:     return 82;
        case GENERIC_DOWN:   return 81;
        case GENERIC_FIRE_A: return 225;
        case GENERIC_FIRE_B: return 226;
        case GENERIC_START:  return 40;
        case GENERIC_SELECT: return 43;
        default:             return 0;
    }
}

static void update_cv_gate_keys()
{
    const uint16_t buttons = glitch_controls_get_cv_gate_buttons() &
                             (uint16_t)~(GENERIC_FIRE_X | GENERIC_FIRE_Y);
    if (_gui._visible) {
        _last_cv_gate_buttons = buttons;
        memset(_cv_gate_keycodes, 0, sizeof(_cv_gate_keycodes));
        return;
    }

    const uint16_t changed = buttons ^ _last_cv_gate_buttons;
    static const uint16_t masks[] = {
        GENERIC_LEFT,
        GENERIC_RIGHT,
        GENERIC_UP,
        GENERIC_DOWN,
        GENERIC_FIRE_A,
        GENERIC_FIRE_B,
        GENERIC_START,
        GENERIC_SELECT
    };

    for (unsigned i = 0; i < sizeof(masks) / sizeof(masks[0]); ++i) {
        const uint16_t mask = masks[i];
        if ((changed & mask) == 0)
            continue;
        const int index = cv_gate_button_index(mask);
        if (index < 0)
            continue;
        if (buttons & mask) {
            const int key = cv_gate_key_for_button(mask);
            _cv_gate_keycodes[index] = key;
            if (key)
                gui_key(key, 1, 0);
        } else {
            const int key = _cv_gate_keycodes[index];
            _cv_gate_keycodes[index] = 0;
            if (key)
                gui_key(key, 0, 0);
        }
    }

    _last_cv_gate_buttons = buttons;
}

static void wii()
{
    int pad = wii_states[0].common();
    pad_key(wii_right,pad,82);  // up
    pad_key(wii_left,pad,81);   // down
    pad_key(wii_down,pad,79);   // right
    pad_key(wii_up,pad,80);     // left
    pad_key(wii_home,pad,58);   // home/gui
    pad_key(wii_a | wii_one | wii_two,pad,40); // enter (A)
    _last_pad = pad;
}

static void ir(const uint8_t* j, int len)
{
    int pad = j[0] + (j[1] << 8);
    const bool glitch_x_down = (pad & GENERIC_FIRE_X) != 0;
    const bool glitch_y_down = (pad & GENERIC_FIRE_Y) != 0;

    // X controls the latch or momentary kill switch; Y drives the repeating mask control.
    glitch_controls_set_virtual_buttons(glitch_x_down, glitch_y_down);

    pad_key(GENERIC_UP,pad,82);  // up
    pad_key(GENERIC_DOWN,pad,81);   // down
    pad_key(GENERIC_RIGHT,pad,79);   // right
    pad_key(GENERIC_LEFT,pad,80);     // left
    pad_key(GENERIC_MENU | GENERIC_RESET | GENERIC_FIRE_Z,pad,58);   // home/gui
    // A confirms and B cancels.
    pad_key(GENERIC_FIRE_A,pad,40); // A -> Enter
    pad_key(GENERIC_FIRE_B,pad,41); // B -> Esc
    pad_key(GENERIC_FIRE_X,pad,GUI_KEY_FACE_X); // X -> popup close only
    pad_key(GENERIC_FIRE_Y,pad,GUI_KEY_FACE_Y); // Y -> popup close
    pad_key(GENERIC_SELECT,pad,GUI_KEY_INFO);   // Select opens info in Games
    _last_pad = pad;
}

void gui_hid(const uint8_t* hid, int len)  // Parse HID event
{
    if (hid[0] != 0xA1)
        return;
    const bool gui_was_visible = _gui._visible;
    switch (hid[1]) {
        case 0x01: keyboard(hid+1,len-1);   break;   // parse keyboard and maintain 1 key state
        case 0x30: wii();                   break;   // Wiimote core buttons
        case 0x32: wii();                   break;   // parse wii stuff: generic?
        case 0x42:                                   // ir joy / generic pad
            _generic_pad1_state = (len >= 4) ? (uint16_t)(hid[2] | (hid[3] << 8)) : 0;
            _generic_pad1_valid = true;
            ir(hid+2,len);
            break;
    }
    // Menus consume pad input. Do not forward it to the emulator.
    if (!gui_was_visible && !_gui._visible && _gui._emu && !midi_visualizer_is_active())
        _gui._emu->hid(hid+1,len-1);
}

void gui_msg(const char* msg)         // temporarily display a msg
{
    _gui.msg(msg);
}

void sys_msg(const char* msg)          // temporarily display a msg
{
    gui_msg(msg);
}
