
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

#ifndef emu_hpp
#define emu_hpp

#include "stdlib.h"
#include "stdio.h"
#include "unistd.h"
#include "string.h"
#include "dirent.h"

#include <string>
#include <vector>
#include <map>

#include "hid_server/hid_server.h"

#define EMU_NES 2
#define EMU_NES6 4
#define EMU_GB 5

#ifndef AUDIO_MCP4822_OUTPUT
#define AUDIO_MCP4822_OUTPUT 1
#endif

#ifndef AUDIO_MCP4822_GB_DECOUPLED_TIMER
#define AUDIO_MCP4822_GB_DECOUPLED_TIMER 1
#endif

#ifndef AUDIO_DECOUPLED_TIMER
#if AUDIO_MCP4822_OUTPUT
#define AUDIO_DECOUPLED_TIMER AUDIO_MCP4822_GB_DECOUPLED_TIMER
#else
#define AUDIO_DECOUPLED_TIMER 1
#endif
#endif

#ifndef AUDIO_OUTPUT_2X_SCANLINE
#define AUDIO_OUTPUT_2X_SCANLINE 1
#endif

#ifndef AUDIO_NES_2X_SCANLINE
#define AUDIO_NES_2X_SCANLINE 1
#endif

#ifndef AUDIO_NES_TIMER_OUTPUT
#define AUDIO_NES_TIMER_OUTPUT 1
#endif

#define AUDIO_TIMER_OUTPUT_ENABLED \
  ((AUDIO_DECOUPLED_TIMER != 0) || ((AUDIO_NES_2X_SCANLINE != 0) && (AUDIO_NES_TIMER_OUTPUT != 0)))

#define AUDIO_TIMER_CLOCK_HZ 40000000u
#define VIDEO_NTSC_COLOR_CLOCK_NUMERATOR_HZ 315000000u
#define VIDEO_NTSC_COLOR_CLOCK_DENOMINATOR 88u
#define VIDEO_NTSC_COLOR_CLOCKS_PER_LINE_X2 455u
#define VIDEO_NTSC_LINES_PER_FRAME 262u
#define VIDEO_NTSC_SCANLINE_RATE_DENOMINATOR \
  (VIDEO_NTSC_COLOR_CLOCK_DENOMINATOR * VIDEO_NTSC_COLOR_CLOCKS_PER_LINE_X2)
#define VIDEO_NTSC_FRAME_RATE_HZ \
  (((double)VIDEO_NTSC_COLOR_CLOCK_NUMERATOR_HZ * 2.0) / \
   ((double)VIDEO_NTSC_SCANLINE_RATE_DENOMINATOR * VIDEO_NTSC_LINES_PER_FRAME))
#define VIDEO_NTSC_SCANLINE_RATE_HZ \
  ((VIDEO_NTSC_COLOR_CLOCK_NUMERATOR_HZ * 2u + \
    VIDEO_NTSC_SCANLINE_RATE_DENOMINATOR / 2u) / \
   VIDEO_NTSC_SCANLINE_RATE_DENOMINATOR)
#define AUDIO_OUTPUT_RATE_PAL_HZ 31200u
#define AUDIO_OUTPUT_RATE_NTSC_HZ \
  ((VIDEO_NTSC_COLOR_CLOCK_NUMERATOR_HZ * 4u + \
    VIDEO_NTSC_SCANLINE_RATE_DENOMINATOR / 2u) / \
   VIDEO_NTSC_SCANLINE_RATE_DENOMINATOR)
// Live music mode stays below 25 ms including the queue deadband. A smaller
// 384-sample queue caused audible jitter and timeline resets on hardware.
#define MUSIC_AUDIO_QUEUE_TARGET_SAMPLES 736u
#define MUSIC_AUDIO_QUEUE_REFILL_THRESHOLD_SAMPLES 704u
#define MUSIC_AUDIO_QUEUE_DEADBAND_SAMPLES 32u
#if MUSIC_AUDIO_QUEUE_REFILL_THRESHOLD_SAMPLES > MUSIC_AUDIO_QUEUE_TARGET_SAMPLES
#error Music audio refill threshold must not exceed its queue target.
#endif
#if ((MUSIC_AUDIO_QUEUE_TARGET_SAMPLES + MUSIC_AUDIO_QUEUE_DEADBAND_SAMPLES) * 1000u) > \
    (AUDIO_OUTPUT_RATE_PAL_HZ * 25u)
#error Music audio queue exceeds the 25 ms live-play latency budget.
#endif
#define AUDIO_TIMER_TICKS_FOR_RATE(_rate) ((AUDIO_TIMER_CLOCK_HZ + ((_rate) / 2u)) / (_rate))
#define AUDIO_TIMER_ACTUAL_RATE(_rate) (AUDIO_TIMER_CLOCK_HZ / AUDIO_TIMER_TICKS_FOR_RATE(_rate))

enum {
  PAL = 0,
  NTSC = 1
};

#define IRE(_x)          ((uint32_t)((_x+40)*255/3.3/147.5) << 8)   // 3.3V DAC
#define SYNC_LEVEL       IRE(-40)
#define BLANKING_LEVEL   IRE(0)
#define BLACK_LEVEL      IRE(7.5)
#define GRAY_LEVEL       IRE((100-7.5)/2)
#define WHITE_LEVEL      IRE(100)

extern "C"
void* MALLOC32(int size, const char* name);

class Emu {
public:

    // hid modifiers
    #define KEY_MOD_LCTRL  0x01
    #define KEY_MOD_LSHIFT 0x02
    #define KEY_MOD_LALT   0x04
    #define KEY_MOD_LGUI   0x08
    #define KEY_MOD_RCTRL  0x10
    #define KEY_MOD_RSHIFT 0x20
    #define KEY_MOD_RALT   0x40
    #define KEY_MOD_RGUI   0x80

    #define KEY_MOD_CTRL   (KEY_MOD_LCTRL|KEY_MOD_RCTRL)
    #define KEY_MOD_SHIFT  (KEY_MOD_LSHIFT|KEY_MOD_RSHIFT)
    #define KEY_MOD_ALT    (KEY_MOD_LALT|KEY_MOD_RALT)
    #define KEY_MOD_GUI    (KEY_MOD_LGUI|KEY_MOD_RGUI)

    // generic joystick, ir for Atari Flashback and RETCON
    #define GENERIC_OTHER   0x8000

    #define GENERIC_FIRE_X  0x4000  // RETCON
    #define GENERIC_FIRE_Y  0x2000
    #define GENERIC_FIRE_Z  0x1000
    #define GENERIC_FIRE_A  0x0800
    #define GENERIC_FIRE_B  0x0400
    #define GENERIC_FIRE_C  0x0200

    #define GENERIC_RESET   0x0100     // ATARI FLASHBACK
    #define GENERIC_START   0x0080
    #define GENERIC_SELECT  0x0040
    #define GENERIC_FIRE    0x0020
    #define GENERIC_RIGHT   0x0010
    #define GENERIC_LEFT    0x0008
    #define GENERIC_DOWN    0x0004
    #define GENERIC_UP      0x0002
    #define GENERIC_MENU    0x0001

    std::string name;
    const char** _ext;
    const char** _help;
    uint8_t** _lines;

    int width;
    int height;
    int standard; // ntsc = 1

    int audio_frequency;
    int audio_frame_samples;
    int audio_fraction;
    int audio_format;

    int cc_width;           // number of samples per color clock
    int flavor;             // color flavor (cleaner?);

    Emu(const char* n, int w, int h, int standard, int aformat, int cc, int flavor);
    virtual ~Emu();

    virtual void gen_palettes() = 0;

    int frame_sample_count();   // # of audio samples for next frame (standard dependent)

    virtual int make_default_media(const std::string& path) = 0;

    virtual int insert(const std::string& path, int flags = 1, int disk_index = 0) = 0;
    static int load(const std::string& path, uint8_t** data, int* len);
    static int head(const std::string& path, uint8_t* data, int len);
    virtual int info(const std::string& file, std::vector<std::string>& strs) { return -1; };

    virtual void hid(const uint8_t* d, int len) {};
    virtual void key(int keycode, int pressed, int mod) {};
    virtual void set_visualizer_active(bool active) { (void)active; };
    virtual void prepare_music_mode_audio() {};
    virtual void set_host_frame_delta(int frames) { (void)frames; };

    virtual int update() = 0;
    virtual uint8_t** video_buffer() = 0;
    virtual int audio_buffer(int16_t* b, int max_len) = 0;

    virtual const uint32_t* ntsc_palette() { return NULL; };
    virtual const uint32_t* pal_palette() { return NULL; };
    virtual const uint32_t* rgb_palette() { return NULL; };
    virtual const uint32_t* composite_palette();
};

void gui_start(Emu* emu, const char* path);
uint8_t** gui_video_buffer();
void gui_hid(const uint8_t* hid, int len);  // Parse HID event
int gui_update();
void gui_music_audio_pre_sync();
void gui_music_audio_mid_render_topup(int16_t* buffer, int buffer_bytes);
void gui_key(int keycode, int pressed, int mod);

extern "C"
void gui_msg(const char* msg);         // temporarily display a msg

// for loading carts
std::string get_ext(const std::string& s);
extern "C" uint8_t* map_file(const char* path, int len);
extern "C" uint8_t* map_file_strict(const char* path, int len);
extern "C" void unmap_file(uint8_t* ptr);
// Idle static scratch owned by map_file*(); invalid after the next map_file*() call.
extern "C" uint8_t* map_file_scratch(size_t* len);
// Idle NES buffers; only valid when the NES core is not the boot emulator.
extern "C" uint8_t* nes_ppu_idle_scratch(size_t* len);
extern "C" uint8_t* nes6502_idle_scratch(size_t* len);
extern "C" uint8_t* nes_wram_idle_scratch(size_t* len);
extern "C" FILE* mkfile(const char* path);
extern "C" int unpack(const char* dst_path, const uint8_t* d, int len);

typedef void (*loading_progress_callback_t)(int percent, void* user);
void loading_progress_set_callback(loading_progress_callback_t callback, void* user);
void loading_progress_clear_callback();
void loading_progress_set(int percent);

void audio_write_16(const int16_t* s, int len, int channels);
void audio_queue_clear();
uint32_t audio_queue_level();
uint32_t audio_queue_capacity();
int get_hid_ir(uint8_t* dst);
int get_hid_ble(uint8_t* dst);
int get_hid_all(uint8_t* dst);
uint32_t generic_map(uint32_t m, const uint32_t* target);
bool gui_video_needs_host_commit();
bool gui_video_commit_needs_vblank_sync();
void video_sync();
bool video_scan_in_active();
void video_sync_for_frame_commit();
void video_sync_ntsc_single_buffer_render_slot();
void video_sync_pal_nes_single_buffer_render_slot();
void video_sync_gb_single_buffer_render_slot();
bool video_queue_lines(uint8_t** lines);
void video_clear_queued_lines();
void video_set_gb_display_fullscreen(int enabled);
int video_get_gb_display_fullscreen();
void video_set_gui_menu_visible(int visible);
void video_palette_glitch_set_enabled(int enabled);
int video_palette_glitch_get_enabled();
void video_palette_glitch_set_game_active(int active);
bool video_set_nes_visualizer_palette(const uint32_t rgb888[4]);
void video_clear_nes_visualizer_palette();
void video_palette_glitch_cycle();

Emu* NewNofrendo(int ntsc = 1);
Emu* NewGameBoy(int ntsc = 1);

#endif /* emu_hpp */
