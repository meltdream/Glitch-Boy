// SPDX-License-Identifier: GPL-3.0-or-later
//
// VidPix visualizer port derived from VID PIX PAL v1.2.2.
// Upstream copyright 2021 Synthrotek Sound Study Modular.
// See THIRD_PARTY_NOTICES.md and licenses/GPL-3.0.txt.

#include "midi_visualizer.h"

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "glitch/glitch_controls.h"
#include "emu.h"
#include "gb_apu_midi.h"
#include "midi_input.h"

#include "vidpix_vector_font.h"

#include "nofrendo/nes_apu_midi.h"

namespace {

// Framebuffer port of VidPix; the Propeller TV, ADC and cog drivers are omitted.
static const int kVidPixWidth = 128;
static const int kVidPixHeight = 96;
static const int kVidPixNormalXTiles = 8;
static const int kVidPixNormalYTiles = 6;
static const int kVidPixTilePixels = 16;
static const int kVidPixGlitchMinScalar = 4;
static const int kVidPixGlitchMaxScalar = 12;
static const int kVidPixPaletteCount = 10;
static const uint32_t kVidPixNesRenderDivisor = 2;
static const size_t kVidPixCanvasBytes =
    ((size_t)kVidPixWidth * kVidPixHeight + 3u) / 4u;
static int s_visualizer_palette = 0;
static int s_nes_output_palette = -1;

#include "vidpix_data.h"

static bool asset_range_ok(size_t offset, size_t bytes, size_t total)
{
    return offset <= total && bytes <= total - offset;
}

// GB visualizer colors are native RGB332 values.
static const uint8_t kVisualizerPalettes[kVidPixPaletteCount][4] = {
    {0x00, 0xAC, 0x18, 0xAB}, // 8-bit Original
    {0x00, 0xE1, 0x1C, 0xE3}, // RGB Blast
    {0x00, 0xDC, 0xE3, 0x1B}, // Toxic Candy
    {0x00, 0xEC, 0xE2, 0x1B}, // Aqua Flame
    {0x00, 0xE1, 0x1B, 0xE9}, // Cyber Pop
    {0x00, 0xE4, 0x12, 0x1B}, // Arcade Punch
    {0x00, 0xC3, 0x17, 0x1C}, // Electric Grape
    {0x00, 0xE2, 0xEA, 0x17}, // Bubblegum
    {0x00, 0x1C, 0xE3, 0xE1}, // Laser Lime
    {0x00, 0xE8, 0x23, 0x1B}, // Pixel Sunset
};

// The NES visualizer reserves normally-black NES indices for its four colors.
// Their output values are replaced with the exact GB RGB332 colors while the
// visualizer is active, then restored before gameplay resumes.
static const uint8_t kVisualizerNesIndices[4] = {
    0x0F, 0x1F, 0x2F, 0x3F
};

static uint32_t visualizer_rgb332_to_rgb888(uint8_t value)
{
    static const uint8_t kLevel8[8] = {
        0x00, 0x24, 0x48, 0x6C, 0x90, 0xB4, 0xD8, 0xFF
    };
    static const uint8_t kLevel4[4] = {
        0x00, 0x48, 0xB4, 0xFF
    };
    const uint8_t r = kLevel8[(value >> 5) & 0x07];
    const uint8_t g = kLevel8[(value >> 2) & 0x07];
    const uint8_t b = kLevel4[value & 0x03];
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static const uint16_t kVidPixSinQuarter1024[65] = {
    0, 25, 50, 75, 100, 125, 150, 175, 200, 224, 249, 273,
    297, 321, 345, 369, 392, 415, 438, 460, 483, 505, 526, 548,
    569, 590, 610, 630, 650, 669, 688, 706, 724, 742, 759, 775,
    792, 807, 822, 837, 851, 865, 878, 891, 903, 915, 926, 936,
    946, 955, 964, 972, 980, 987, 993, 999, 1004, 1009, 1013,
    1016, 1019, 1021, 1023, 1024, 1024,
};

static inline int clampi(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline int vidpix_palette_variant()
{
    int v = s_visualizer_palette % kVidPixPaletteCount;
    return v < 0 ? v + kVidPixPaletteCount : v;
}

struct VidPixInputFrame {
    int scalar_l;
    int scalar_r;
    int scalar_bigd;
    int totalavg;
};

class VidPixGraphics {
public:
    VidPixGraphics()
        : _lines(0), _physical_width(0), _physical_height(0), _flavor(EMU_NES),
          _target_width(0), _target_height(0), _target_pitch(0),
          _map_num(1), _map_den(1), _xoff(0), _yoff(0),
          _canvas(0),
          _draw_color(1), _pixel_width(16),
          _text_x_scale(1), _text_y_scale(1),
          _text_spacing(6), _text_justification(0),
          _glitch1_level(0),
          _glitch_y_tiles(kVidPixNormalYTiles),
          _audio_service(0),
          _audio_service_bytes(0)
    {
        memset(_palette, 0, sizeof(_palette));
    }

    void init(int width, int height, int flavor)
    {
        _physical_width = width;
        _physical_height = height;
        _flavor = flavor;

        // Map logical cell boundaries to keep fractional scaling contiguous.
        if ((int64_t)width * kVidPixHeight >=
            (int64_t)height * kVidPixWidth) {
            _map_num = width;
            _map_den = kVidPixWidth;
        } else {
            _map_num = height;
            _map_den = kVidPixHeight;
        }

        const int mapped_width =
            (int)(((int64_t)kVidPixWidth * _map_num) / _map_den);
        const int mapped_height =
            (int)(((int64_t)kVidPixHeight * _map_num) / _map_den);
        _xoff = (width - mapped_width) / 2;
        _yoff = (height - mapped_height) / 2;
        _target_width = _physical_width;
        _target_height = _physical_height;
        _target_pitch = _physical_width;
        select_palette(vidpix_palette_variant());
    }

    int flavor() const
    {
        return _flavor;
    }

    void attach_canvas(uint8_t* scratch, size_t scratch_bytes)
    {
        _canvas = ((_flavor == EMU_GB || _flavor == EMU_NES) && scratch &&
                   scratch_bytes >= kVidPixCanvasBytes)
            ? scratch
            : 0;
        // Canvas uses the head; audio top-ups use the remaining aligned space.
        _audio_service = 0;
        _audio_service_bytes = 0;
        if (_canvas) {
            const size_t off = (kVidPixCanvasBytes + 1u) & ~(size_t)1u;
            if (scratch_bytes >= off + 64u) {
                _audio_service = (int16_t*)(scratch + off);
                _audio_service_bytes = (int)(scratch_bytes - off);
            }
        }
    }

    /* Mid-render audio refill for GB/NES music mode. */
    void service_music_audio()
    {
        if (_audio_service)
            gui_music_audio_mid_render_topup(_audio_service,
                                             _audio_service_bytes);
    }

    bool uses_canvas() const
    {
        return _canvas != 0;
    }

    void attach(uint8_t** lines, int width, int height, int pitch)
    {
        _lines = lines;
        if (width <= 0)
            width = _physical_width;
        if (height <= 0)
            height = _physical_height;
        if (pitch <= 0)
            pitch = width;
        if (width != _physical_width || height != _physical_height)
            init(width, height, _flavor);
        _target_width = width;
        _target_height = height;
        _target_pitch = pitch;
        if (_target_width > _target_pitch)
            _target_width = _target_pitch;
    }

    void select_palette(int palette_index)
    {
        palette_index %= kVidPixPaletteCount;
        if (palette_index < 0)
            palette_index += kVidPixPaletteCount;
        const uint8_t* source = kVisualizerPalettes[palette_index];
        if (_flavor == EMU_NES) {
            for (int i = 0; i < 4; ++i) {
                _palette[i] = kVisualizerNesIndices[i];
            }
            if (s_nes_output_palette != palette_index) {
                uint32_t rgb888[4];
                for (int i = 0; i < 4; ++i)
                    rgb888[i] = visualizer_rgb332_to_rgb888(source[i]);
                if (video_set_nes_visualizer_palette(rgb888))
                    s_nes_output_palette = palette_index;
            }
        } else {
            for (int i = 0; i < 4; ++i)
                _palette[i] = map_color(source[i]);
        }
    }

    void set_glitch_effect(int glitch1_level, int y_tiles)
    {
        _glitch1_level = clampi(glitch1_level, 0, 15);
        _glitch_y_tiles = clampi(y_tiles, kVidPixNormalYTiles, 17);
    }

    void clear()
    {
        if (_canvas) {
            memset(_canvas, 0, kVidPixCanvasBytes);
            return;
        }
        if (!_lines)
            return;
        const uint8_t bg = _palette[0];
        for (int y = 0; y < _target_height; ++y)
            if (_lines[y])
                memset(_lines[y], bg, (size_t)_target_width);
    }

    void color(int c)
    {
        _draw_color = (uint8_t)(c & 3);
    }

    void width(int w)
    {
        _pixel_width = (uint8_t)clampi(w, 0, 31);
    }

    void pix(int x, int y, int pixrot, VidPixSpriteId sprite_id)
    {
        if (sprite_id < 0 || sprite_id >= SPR_COUNT)
            return;
        const VidPixSprite& sprite = kVidPixSprites[sprite_id];
        const uint8_t* pixels = kVidPixSpritePixels;
        const size_t pixel_bytes = sizeof(kVidPixSpritePixels);
        const size_t sprite_bytes = (size_t)sprite.height * sprite.row_bytes;
        if (!asset_range_ok(sprite.offset, sprite_bytes, pixel_bytes))
            return;

        const uint8_t* data = pixels + sprite.offset;
        const int mirror = (pixrot & 4) != 0;
        const int rot = pixrot & 3;
        const int px_size = pixel_size();

        for (int sy = 0; sy < sprite.height; ++sy) {
            const uint8_t* row = data + (size_t)sy * sprite.row_bytes;
            int sx = 0;
            while (sx < sprite.width) {
                uint8_t c = packed_sprite_color(row, sx);
                if (!c) {
                    ++sx;
                    continue;
                }

                const int run_start = sx++;
                while (sx < sprite.width &&
                       packed_sprite_color(row, sx) == c) {
                    ++sx;
                }

                draw_sprite_run(x, y, sprite, run_start, sx - 1, sy,
                                mirror, rot, px_size, c);
            }
        }

        // Large multi-sprite scenes can outlast a field; keep audio fed.
        service_music_audio();
    }

    void text(int x, int y, const char* str)
    {
        if (!str || !*str)
            return;

        const int len = (int)strlen(str);
        const int extent =
            (len - 1) * _text_x_scale * _text_spacing +
            _text_x_scale * 5 - 1;
        const int hjust = (_text_justification >> 2) & 3;
        const int vjust = _text_justification & 3;
        int left = x;
        int baseline = y;

        if (hjust == 1)
            left -= extent / 2;
        else if (hjust == 2)
            left -= extent;
        if (vjust == 1)
            baseline -= _text_y_scale * 8;
        else if (vjust == 2)
            baseline -= _text_y_scale * 16;

        for (int i = 0; i < len; ++i) {
            draw_char(left + i * _text_spacing * _text_x_scale,
                      baseline, str[i]);
            service_music_audio();
        }
    }

    void textarc(int x, int y, int xr, int yr, int angle, const char* str)
    {
        text(x + ((xr * cos1024(angle)) / 1024),
             y + ((yr * sin1024(angle)) / 1024),
             str);
    }

    void textmode(int x_scale, int y_scale, int spacing, int justification)
    {
        _text_x_scale = clampi(x_scale, 1, 31);
        _text_y_scale = clampi(y_scale, 1, 31);
        _text_spacing = clampi(spacing, 1, 31);
        _text_justification = (uint8_t)(justification & 0x0F);
    }

    void commit_canvas()
    {
        if (!_canvas || !_lines)
            return;

        for (int sy = 0; sy < kVidPixHeight; ++sy) {
            int y0 = map_y_boundary(sy);
            int y1 = map_y_boundary(sy + 1);
            if (y0 < 0)
                y0 = 0;
            if (y1 > _target_height)
                y1 = _target_height;
            if (y0 >= y1 || !_lines[y0])
                continue;

            uint8_t* dst = _lines[y0];
            memset(dst, _palette[0], (size_t)_target_width);
            int sx = 0;
            while (sx < kVidPixWidth) {
                const uint8_t color_index = canvas_color(sx, sy);
                const int run_start = sx++;
                while (sx < kVidPixWidth &&
                       canvas_color(sx, sy) == color_index) {
                    ++sx;
                }

                int x0 = map_x_boundary(run_start);
                int x1 = map_x_boundary(sx);
                if (x0 < 0)
                    x0 = 0;
                if (x1 > _target_width)
                    x1 = _target_width;
                if (x0 < x1)
                    memset(dst + x0, _palette[color_index & 3],
                           (size_t)(x1 - x0));
            }

            for (int y = y0 + 1; y < y1; ++y) {
                if (_lines[y] && _lines[y] != dst)
                    memcpy(_lines[y], dst, (size_t)_target_width);
            }

            if ((sy & 15) == 15)
                service_music_audio();
        }
    }

private:
    uint8_t** _lines;
    int _physical_width;
    int _physical_height;
    int _flavor;
    int _target_width;
    int _target_height;
    int _target_pitch;
    int _map_num;
    int _map_den;
    int _xoff;
    int _yoff;
    uint8_t* _canvas;
    uint8_t _palette[4];
    uint8_t _draw_color;
    uint8_t _pixel_width;
    int _text_x_scale;
    int _text_y_scale;
    int _text_spacing;
    uint8_t _text_justification;
    int _glitch1_level;
    int _glitch_y_tiles;
    int16_t* _audio_service;
    int _audio_service_bytes;

    uint8_t map_color(uint8_t raw) const
    {
        if (_flavor == EMU_NES) {
            if (raw <= 0x03)
                return 0x0D;
            uint8_t c = (uint8_t)(raw & 0x3F);
            return c == 0x0D ? 0x0E : c;
        }
        if (_flavor == EMU_GB) {
            if (raw <= 0x03)
                return 0x00;
            return raw;
        }
        return raw;
    }

    int pixel_size() const
    {
        const int width_nibble = _pixel_width & 0x0F;
        // VidPix uses width_nibble + 1; capped at 6x to keep the dense
        // Diamond routines within the frame budget.
        return clampi(1 + ((width_nibble * 5 + 7) / 15), 1, 6);
    }

    int map_x_boundary(int logical_x) const
    {
        return _xoff +
               (int)(((int64_t)logical_x * _map_num) / _map_den);
    }

    int map_y_boundary(int logical_y) const
    {
        return _yoff +
               (int)(((int64_t)logical_y * _map_num) / _map_den);
    }

    uint8_t canvas_color(int x, int y) const
    {
        const size_t index =
            (size_t)y * (kVidPixWidth / 4) + (size_t)(x >> 2);
        const int shift = 6 - ((x & 3) << 1);
        return (uint8_t)((_canvas[index] >> shift) & 3u);
    }

    void set_canvas_color(int x, int y, uint8_t color_index)
    {
        const size_t index =
            (size_t)y * (kVidPixWidth / 4) + (size_t)(x >> 2);
        const int shift = 6 - ((x & 3) << 1);
        const uint8_t mask = (uint8_t)(3u << shift);
        _canvas[index] = (uint8_t)((_canvas[index] & ~mask) |
                                  ((color_index & 3u) << shift));
    }

    void fill_canvas_rect(int x0, int y0, int x1, int y1,
                          uint8_t color_index)
    {
        const uint8_t packed = (uint8_t)((color_index & 3u) * 0x55u);
        for (int y = y0; y < y1; ++y) {
            int x = x0;
            while (x < x1 && (x & 3)) {
                set_canvas_color(x++, y, color_index);
            }
            if (x + 4 <= x1) {
                uint8_t* dst = _canvas +
                    (size_t)y * (kVidPixWidth / 4) + (size_t)(x >> 2);
                const int bytes = (x1 - x) >> 2;
                memset(dst, packed, (size_t)bytes);
                x += bytes << 2;
            }
            while (x < x1)
                set_canvas_color(x++, y, color_index);
        }
    }

    static uint8_t packed_sprite_color(const uint8_t* row, int sx)
    {
        const uint8_t b = row[sx >> 2];
        return (uint8_t)((b >> (6 - ((sx & 3) << 1))) & 3);
    }

    static void transform_sprite_point(const VidPixSprite& sprite,
                                       int sx,
                                       int sy,
                                       int mirror,
                                       int rot,
                                       int& tx,
                                       int& ty)
    {
        int lx = sx - sprite.xorigin;
        const int ly = sprite.yorigin - sy;
        if (mirror)
            lx = -lx;

        tx = lx;
        ty = ly;
        switch (rot) {
            case 1: tx = ly; ty = -lx; break;
            case 2: tx = -lx; ty = -ly; break;
            case 3: tx = -ly; ty = lx; break;
        }
    }

    void draw_sprite_run(int x,
                         int y,
                         const VidPixSprite& sprite,
                         int sx0,
                         int sx1,
                         int sy,
                         int mirror,
                         int rot,
                         int px_size,
                         uint8_t color_index)
    {
        int tx0, ty0, tx1, ty1;
        transform_sprite_point(sprite, sx0, sy, mirror, rot, tx0, ty0);
        transform_sprite_point(sprite, sx1, sy, mirror, rot, tx1, ty1);

        const int start = -(px_size >> 1);
        const int cell_x0 = (kVidPixWidth / 2) + x + tx0 * px_size + start;
        const int cell_y0 = (kVidPixHeight / 2) - (y + ty0 * px_size) + start;
        const int cell_x1 = (kVidPixWidth / 2) + x + tx1 * px_size + start;
        const int cell_y1 = (kVidPixHeight / 2) - (y + ty1 * px_size) + start;
        const int min_x = cell_x0 < cell_x1 ? cell_x0 : cell_x1;
        const int min_y = cell_y0 < cell_y1 ? cell_y0 : cell_y1;
        const int max_x = cell_x0 > cell_x1 ? cell_x0 : cell_x1;
        const int max_y = cell_y0 > cell_y1 ? cell_y0 : cell_y1;

        draw_logical_rect_cells(min_x, min_y,
                                (max_x - min_x) + px_size,
                                (max_y - min_y) + px_size,
                                color_index);
    }

    static int sin1024(int angle)
    {
        angle &= 0x1FFF;
        const int quadrant = angle >> 11;
        int t = angle & 0x7FF;
        if (quadrant & 1)
            t = 0x800 - t;
        const int index = t >> 5;
        const int frac = t & 31;
        int s = kVidPixSinQuarter1024[index];
        if (index < 64)
            s += ((int)(kVidPixSinQuarter1024[index + 1] - s) * frac) >> 5;
        return (quadrant >= 2) ? -s : s;
    }

    static int cos1024(int angle)
    {
        return sin1024(angle + 0x800);
    }

    void logical_source_bounds(int& min_x, int& min_y,
                               int& max_x, int& max_y) const
    {
        min_x = -(kVidPixWidth / 2);
        max_x = (kVidPixWidth / 2) - 1;
        min_y = 1 - (kVidPixHeight / 2);
        max_y = (_glitch_y_tiles * kVidPixTilePixels) -
                (kVidPixHeight / 2);
    }

    bool logical_box_visible(int box_min_x, int box_min_y,
                             int box_max_x, int box_max_y,
                             int margin = 0) const
    {
        // Cull only fully invisible work.
        int min_x, min_y, max_x, max_y;
        logical_source_bounds(min_x, min_y, max_x, max_y);
        margin = clampi(margin, 0, 6);
        min_x -= margin;
        min_y -= margin;
        max_x += margin;
        max_y += margin;
        return box_max_x >= min_x && box_min_x <= max_x &&
               box_max_y >= min_y && box_min_y <= max_y;
    }

    void draw_logical_cell(int vx, int vy, uint8_t color_index)
    {
        if (!_lines || vx < 0)
            return;

        if (_glitch_y_tiles != kVidPixNormalYTiles) {
            const int virtual_height = _glitch_y_tiles * kVidPixTilePixels;
            const int px = vx;
            const int py = vy + virtual_height - kVidPixHeight;
            if (px < 0 || py < 0 ||
                (px >> 4) >= kVidPixNormalXTiles || py >= virtual_height)
                return;

            // Map Glitch 2's variable tile stride onto the fixed 8x6 display.
            const int source_tile =
                (px >> 4) * _glitch_y_tiles + (py >> 4);
            const int visible_tile =
                source_tile % (kVidPixNormalXTiles * kVidPixNormalYTiles);
            vx = (visible_tile / kVidPixNormalYTiles) * kVidPixTilePixels +
                 (px & (kVidPixTilePixels - 1));
            vy = (visible_tile % kVidPixNormalYTiles) * kVidPixTilePixels +
                 (py & (kVidPixTilePixels - 1));
        } else if (vy < 0 || vx >= kVidPixWidth || vy >= kVidPixHeight) {
            return;
        }

        if (_glitch1_level > 0) {
            // Glitch 1 displaces complete 16x16 tiles instead of creating stripes.
            const int local_x = vx & (kVidPixTilePixels - 1);
            const int local_y = vy & (kVidPixTilePixels - 1);
            int tile_x = vx / kVidPixTilePixels;
            int tile_y = vy / kVidPixTilePixels;
            const uint32_t hash =
                ((uint32_t)(tile_x + 1) * 0x9E37u) ^
                ((uint32_t)(tile_y + 1) * 0x85EBu);

            if ((int)(hash & 15u) < _glitch1_level) {
                const int x_reach = 1 + (_glitch1_level * 3) / 15;
                const int x_span = x_reach * 2 + 1;
                int dx = (int)((hash >> 4) % (uint32_t)x_span) - x_reach;
                if (dx == 0)
                    dx = (hash & 0x1000u) ? 1 : -1;

                int dy = 0;
                if (_glitch1_level >= 7 && (hash & 0x0600u) == 0) {
                    const int y_reach = 1 + (_glitch1_level - 7) / 5;
                    dy = (hash & 0x0800u) ? y_reach : -y_reach;
                }

                tile_x = (tile_x + dx) % kVidPixNormalXTiles;
                tile_y = (tile_y + dy) % kVidPixNormalYTiles;
                if (tile_x < 0)
                    tile_x += kVidPixNormalXTiles;
                if (tile_y < 0)
                    tile_y += kVidPixNormalYTiles;
                vx = tile_x * kVidPixTilePixels + local_x;
                vy = tile_y * kVidPixTilePixels + local_y;
            }
        }

        if (_canvas) {
            if (vx >= 0 && vx < kVidPixWidth &&
                vy >= 0 && vy < kVidPixHeight) {
                set_canvas_color(vx, vy, color_index);
            }
            return;
        }

        const uint8_t native_color = _palette[color_index & 3];
        int nx = map_x_boundary(vx);
        int ny = map_y_boundary(vy);
        int next_x = map_x_boundary(vx + 1);
        int next_y = map_y_boundary(vy + 1);
        int run = next_x - nx;
        int rows = next_y - ny;
        if (run <= 0 || rows <= 0)
            return;

        if (nx < 0) {
            run += nx;
            nx = 0;
        }
        if (ny < 0) {
            rows += ny;
            ny = 0;
        }
        if (nx + run > _target_width)
            run = _target_width - nx;
        if (ny + rows > _target_height)
            rows = _target_height - ny;
        if (run <= 0 || rows <= 0)
            return;

        for (int row = 0; row < rows; ++row)
            if (_lines[ny + row])
                memset(_lines[ny + row] + nx, native_color, (size_t)run);
    }

    void draw_logical_rect_cells(int cell_x,
                                 int cell_y,
                                 int cell_w,
                                 int cell_h,
                                 uint8_t color_index)
    {
        if (!_lines || cell_w <= 0 || cell_h <= 0)
            return;

        int x0 = cell_x;
        int y0 = cell_y;
        int x1 = cell_x + cell_w;
        int y1 = cell_y + cell_h;
        const int min_y = (_glitch_y_tiles == kVidPixNormalYTiles)
            ? 0
            : kVidPixHeight - (_glitch_y_tiles * kVidPixTilePixels);

        if (x0 < 0)
            x0 = 0;
        if (x1 > kVidPixWidth)
            x1 = kVidPixWidth;
        if (y0 < min_y)
            y0 = min_y;
        if (y1 > kVidPixHeight)
            y1 = kVidPixHeight;
        if (x0 >= x1 || y0 >= y1)
            return;

        if (_canvas && _glitch1_level == 0 &&
            _glitch_y_tiles == kVidPixNormalYTiles) {
            fill_canvas_rect(x0, y0, x1, y1, color_index);
            return;
        }

        if (_glitch1_level == 0 &&
            _glitch_y_tiles == kVidPixNormalYTiles) {
            const uint8_t native_color = _palette[color_index & 3];
            int nx = map_x_boundary(x0);
            int ny = map_y_boundary(y0);
            int run = map_x_boundary(x1) - nx;
            int rows = map_y_boundary(y1) - ny;

            if (nx < 0) {
                run += nx;
                nx = 0;
            }
            if (ny < 0) {
                rows += ny;
                ny = 0;
            }
            if (nx + run > _target_width)
                run = _target_width - nx;
            if (ny + rows > _target_height)
                rows = _target_height - ny;
            if (run <= 0 || rows <= 0)
                return;

            for (int row = 0; row < rows; ++row) {
                if (_lines[ny + row])
                    memset(_lines[ny + row] + nx, native_color,
                           (size_t)run);
            }
            return;
        }

        for (int yy = y0; yy < y1; ++yy)
            for (int xx = x0; xx < x1; ++xx)
                draw_logical_cell(xx, yy, color_index);
    }

    void draw_logical_pixel(int x, int y, uint8_t color_index, int size)
    {
        int start = -(size >> 1);
        const int end = start + size - 1;
        if (!logical_box_visible(x + start, y - end,
                                 x + end, y - start))
            return;

        int vx = (kVidPixWidth / 2) + x;
        int vy = (kVidPixHeight / 2) - y;
        draw_logical_rect_cells(vx + start, vy + start, size, size,
                                color_index);
    }

    void draw_line(int x0, int y0, int x1, int y1,
                   uint8_t color_index, int size)
    {
        const int min_x = x0 < x1 ? x0 : x1;
        const int max_x = x0 > x1 ? x0 : x1;
        const int min_y = y0 < y1 ? y0 : y1;
        const int max_y = y0 > y1 ? y0 : y1;
        if (!logical_box_visible(min_x, min_y, max_x, max_y, size))
            return;

        int dx = x1 - x0;
        int dy = y1 - y0;
        int sx = dx >= 0 ? 1 : -1;
        int sy = dy >= 0 ? 1 : -1;
        dx = dx >= 0 ? dx : -dx;
        dy = dy >= 0 ? dy : -dy;

        if (dx >= dy) {
            int err = dx >> 1;
            for (;;) {
                draw_logical_pixel(x0, y0, color_index, size);
                if (x0 == x1)
                    break;
                x0 += sx;
                err -= dy;
                if (err < 0) {
                    y0 += sy;
                    err += dx;
                }
            }
        } else {
            int err = dy >> 1;
            for (;;) {
                draw_logical_pixel(x0, y0, color_index, size);
                if (y0 == y1)
                    break;
                y0 += sy;
                err -= dx;
                if (err < 0) {
                    x0 += sx;
                    err += dy;
                }
            }
        }
    }

    void draw_char(int x, int y, char ch)
    {
        const uint8_t glyph = (uint8_t)ch;
        if (glyph < 32 || glyph > 126 || glyph == ' ')
            return;

        const int glyph_min_x = x - 2 * _text_x_scale;
        const int glyph_max_x = x + 9 * _text_x_scale;
        const int glyph_min_y = y - 4 * _text_y_scale;
        const int glyph_max_y = y + 19 * _text_y_scale;
        if (!logical_box_visible(glyph_min_x, glyph_min_y,
                                 glyph_max_x, glyph_max_y))
            return;

        const uint8_t color_index = _draw_color ? _draw_color : 1;
        const uint16_t start = kVidPixVectorFontOffsets[glyph - 32];
        const uint16_t end = kVidPixVectorFontOffsets[glyph - 31];
        for (uint16_t i = start; i < end; ++i) {
            const uint16_t command = kVidPixVectorFontWords[i];
            const int ax = command & 7;
            const int ay = (command >> 3) & 15;
            if ((command & 0x4000u) == 0) {
                const int bx = (command >> 7) & 7;
                const int by = (command >> 10) & 15;
                draw_line(x + ax * _text_x_scale,
                          y + ay * _text_y_scale,
                          x + bx * _text_x_scale,
                          y + by * _text_y_scale,
                          color_index, 1);
                continue;
            }

            const int rx = (((command >> 7) & 1) + 1) * _text_x_scale;
            const int ry = (((command >> 8) & 3) + 1) * _text_y_scale;
            const int cx = x + ax * _text_x_scale;
            const int cy = y + ay * _text_y_scale;
            if (!logical_box_visible(cx - rx, cy - ry, cx + rx, cy + ry))
                continue;
            const int arc = (command >> 10) & 15;
            const int angle = (arc & 3) * 0x800;
            const int segments = 64 - ((arc >> 2) & 3) * 16;
            int px = x + ax * _text_x_scale +
                     (rx * cos1024(angle)) / 1024;
            int py = y + ay * _text_y_scale +
                     (ry * sin1024(angle)) / 1024;
            draw_logical_pixel(px, py, color_index, 1);
            for (int segment = 1; segment <= segments; ++segment) {
                const int a = angle + segment * 0x80;
                const int nx = x + ax * _text_x_scale +
                               (rx * cos1024(a)) / 1024;
                const int ny = y + ay * _text_y_scale +
                               (ry * sin1024(a)) / 1024;
                if (nx != px || ny != py)
                    draw_line(px, py, nx, ny, color_index, 1);
                px = nx;
                py = ny;
            }
        }
    }
};

struct VidPixModeInfo {
    uint8_t base_pattern;
    uint8_t pattern_count;
};

static const VidPixModeInfo kVidPixModes[] = {
    {16, 8},  // 8-bit Nostalgia: arcade sprites
    {0, 8},   // Digital Diamonds
    {24, 8},  // Character Assassination
};
static const int kVidPixModeCount =
    (int)(sizeof(kVidPixModes) / sizeof(kVidPixModes[0]));

struct VisualizerManager {
    VidPixGraphics gfx;
    int active_mode;
    uint32_t frame;
    uint8_t pattern[kVidPixModeCount];
    uint8_t j;
    uint8_t h;
    uint8_t character[3];
    uint32_t character_rng;
    uint32_t last_character_step;
    uint32_t last_character_ms;
    bool anim_step_changed;
    bool midi_clock_anim_active;
    uint32_t last_midi_anim_step;
    bool force_render;

    VisualizerManager()
        : active_mode(-1), frame(0), j(1), h(0),
          character_rng(0x51A7C3E9u), last_character_step(0),
          last_character_ms(0),
          anim_step_changed(true),
          midi_clock_anim_active(false), last_midi_anim_step(0),
          force_render(true)
    {
        memset(pattern, 0, sizeof(pattern));
        character[0] = 33;
        character[1] = 48;
        character[2] = 65;
    }
};

static VisualizerManager s_visualizer;

static bool vidpix_glitch_enabled()
{
    return glitch_runtime_is_enabled() && glitch_runtime_is_latch_active();
}

static int vidpix_glitch_knob_level(uint16_t raw)
{
    uint16_t turned_up = (uint16_t)(4095u - raw);
    if (turned_up <= GLITCH_KNOB_DEADZONE)
        return 0;
    const uint32_t active_range = 4095u - GLITCH_KNOB_DEADZONE;
    const uint32_t adjusted = (uint32_t)turned_up - GLITCH_KNOB_DEADZONE;
    return clampi((int)((adjusted * 15u + (active_range / 2u)) / active_range),
                  0, 15);
}

static int vidpix_glitch_scalar_from_level(int level)
{
    level = clampi(level, 0, 15);
    return kVidPixGlitchMinScalar +
           ((level * (kVidPixGlitchMaxScalar - kVidPixGlitchMinScalar) + 7) /
            15);
}

// Knob A sets tile displacement, knob B the vertical tile stride.
static void vidpix_glitch_from_knobs(int* level, int* y_tiles)
{
    *level = vidpix_glitch_knob_level(glitch_controls_get_effective_ppu_adc_raw());
    const int ly = vidpix_glitch_knob_level(glitch_controls_get_effective_corruption_adc_raw());
    *y_tiles = 2 + vidpix_glitch_scalar_from_level(ly);
}

static void resolve_voice_channels(int flavor, int8_t channels[4])
{
    if (flavor == EMU_GB) {
        gb_apu_midi_get_channel_map(&channels[0], &channels[1],
                                    &channels[2], &channels[3]);
        return;
    }
    nes_apu_midi_get_channel_map(&channels[0], &channels[1],
                                  &channels[2], &channels[3]);
}

static int voice_energy(const midi_channel_visual_state_t& st,
                        uint32_t release_hold_ms)
{
    if (st.active)
        return st.velocity ? st.velocity : 64;
    if (release_hold_ms != 0 && st.velocity != 0 &&
        (uint32_t)(millis() - st.last_event_ms) < release_hold_ms) {
        return st.velocity;
    }
    return 0;
}

static int energy_to_vidpix_scalar(int energy)
{
    energy = clampi(energy, 0, 127);
    return clampi((energy * 15 + 63) / 127, 0, 15);
}

static VidPixInputFrame make_input_frame()
{
    VidPixInputFrame in = {};

    int8_t mapped[4];
    midi_channel_visual_state_t voices[4];
    resolve_voice_channels(s_visualizer.gfx.flavor(), mapped);
    for (int i = 0; i < 4; ++i) {
        memset(&voices[i], 0, sizeof(voices[i]));
        if (mapped[i] >= 0 && mapped[i] < 16)
            midi_get_channel_visual_state((uint8_t)mapped[i], &voices[i]);
    }

    // Hold NES voice energy across two 30 Hz raster opportunities.
    const uint32_t release_hold_ms =
        s_visualizer.gfx.flavor() == EMU_NES ? 70u : 0u;
    int e0 = voice_energy(voices[0], release_hold_ms);
    int e1 = voice_energy(voices[1], release_hold_ms);
    int e2 = voice_energy(voices[2], release_hold_ms);
    int e3 = voice_energy(voices[3], release_hold_ms);
    in.scalar_l = energy_to_vidpix_scalar(e0 + e2);
    in.scalar_r = energy_to_vidpix_scalar(e1 + e3);
    in.totalavg = (in.scalar_l + in.scalar_r) >> 1;
    in.scalar_bigd = ((in.scalar_r * 2) / 5) + 16;
    return in;
}

static void advance_vidpix_counters()
{
    s_visualizer.anim_step_changed = true;

    if (s_visualizer.j == 32)
        s_visualizer.j = 1;
    else
        s_visualizer.j++;

    if (s_visualizer.h == 8)
        s_visualizer.h = 0;
    else if (((s_visualizer.j + 1) % 4) == 0)
        s_visualizer.h++;
}

static bool sync_vidpix_counters_to_midi_clock()
{
    if (!midi_clock_is_active()) {
        s_visualizer.midi_clock_anim_active = false;
        s_visualizer.anim_step_changed = (s_visualizer.frame & 3u) == 0;
        return false;
    }

    // 24 PPQN: two beats (48 clocks) map to the 32-step VidPix cycle.
    uint32_t step = (midi_get_clock_pulse_count() * 2u) / 3u;
    s_visualizer.anim_step_changed =
        !s_visualizer.midi_clock_anim_active ||
        step != s_visualizer.last_midi_anim_step;
    s_visualizer.midi_clock_anim_active = true;
    s_visualizer.last_midi_anim_step = step;

    s_visualizer.j = (uint8_t)((step & 31u) + 1u);
    s_visualizer.h = (uint8_t)((step >> 2) & 7u);
    return true;
}

static inline void cycle_vidpix_palette()
{
    s_visualizer_palette =
        (vidpix_palette_variant() + 1) % kVidPixPaletteCount;
}

static uint8_t character_assassination_random(uint8_t range)
{
    uint32_t x = s_visualizer.character_rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_visualizer.character_rng = x;
    return range ? (uint8_t)((x % range) + 1u) : 0;
}

static void advance_character_assassination()
{
    if (midi_clock_is_active()) {
        // Change twice per beat at 24 PPQN, independently of render rate.
        const uint32_t step = midi_get_clock_pulse_count() / 12u;
        if (step == s_visualizer.last_character_step)
            return;
        s_visualizer.last_character_step = step;
    } else {
        // The loop can run far above video rate; keep glyphs legible.
        const uint32_t now = millis();
        if ((uint32_t)(now - s_visualizer.last_character_ms) < 250u)
            return;
        s_visualizer.last_character_ms = now;
    }

    uint8_t& primary = s_visualizer.character[0];
    primary = primary >= 126 ? 33 : (uint8_t)(primary + 1);
    if (primary < 63) {
        s_visualizer.character[1] =
            (uint8_t)(primary + character_assassination_random(32));
        s_visualizer.character[2] =
            (uint8_t)(primary + character_assassination_random(32));
    } else {
        s_visualizer.character[1] =
            (uint8_t)(primary - character_assassination_random(32));
        s_visualizer.character[2] =
            (uint8_t)(primary - character_assassination_random(32));
    }
}

static void character_assassination_textmode(VidPixGraphics& gr,
                                             int scalar_l,
                                             int scalar_r,
                                             bool large)
{
    int xs = large ? (scalar_l * 2) + 1 : (scalar_l / 2) + 1;
    int ys = large ? (scalar_r * 2) + 1 : (scalar_r / 2) + 1;
    if (xs > ys * 2)
        xs = ys * 2;
    if (ys > xs * 2)
        ys = xs * 2;
    gr.textmode(xs, ys, 6, 0x05);
}

static int digital_diamond_main_width(int scalar)
{
    // Scale the central form within the 128x96 canvas.
    scalar = clampi(scalar, 0, 15);
    return 3 + ((scalar * 4 + 7) / 15);
}

static int digital_diamond_accent_width(int scalar)
{
    // Satellites stay smaller than the central form and remain visible at idle.
    scalar = clampi(scalar, 0, 15);
    return (scalar * 4 + 7) / 15;
}

static void render_vidpix_pattern(VidPixGraphics& gr,
                                  const VidPixInputFrame& input,
                                  int displayroutine)
{
    const int scalar_l = input.scalar_l;
    const int scalar_r = input.scalar_r;
    const int scalar_bigd = input.scalar_bigd;
    const int totalavg = input.totalavg;
    const int j = s_visualizer.j;
    const int h = s_visualizer.h;
    const char str1[2] = {(char)s_visualizer.character[0], 0};
    const char str2[2] = {(char)s_visualizer.character[1], 0};
    const char str3[2] = {(char)s_visualizer.character[2], 0};
    const int diamond_main_l = digital_diamond_main_width(scalar_l);
    const int diamond_main_r = digital_diamond_main_width(scalar_r);
    const int diamond_accent = digital_diamond_accent_width(totalavg);
    const int diamond_phase = (j >> 2) & 1;
    const int diamond_x = 42;
    const int diamond_y = 30;
    const int diamond_diag_x = 38;
    const int diamond_diag_y = 27;

    switch (displayroutine) {
        case 0:
            gr.width(diamond_accent);
            gr.pix(-diamond_x, 0, 0, SPR_PIXDEFTRICLEAR3);
            gr.pix(diamond_x, 0, 0, SPR_PIXDEFTRICLEAR3);
            gr.width(diamond_main_r);
            gr.pix(0, 0, 0, diamond_phase ?
                   SPR_PIXDEFTRICLEAR2B : SPR_PIXDEFTRICLEAR2A);
            gr.width(diamond_main_l);
            gr.pix(0, 0, 0, SPR_PIXDEFSMALL1);
            break;

        case 1:
            gr.width(diamond_main_r);
            gr.pix(0, 0, 0, diamond_phase ?
                   SPR_PIXDEFTRICLEAR2B : SPR_PIXDEFTRICLEAR2A);
            gr.width(diamond_main_l);
            gr.pix(0, 0, 0, SPR_PIXDEFSMALL1);
            gr.width(diamond_accent);
            gr.pix(0, 0, diamond_phase ? 1 : 3, SPR_PIXDEFSMALL2);
            break;

        case 2:
            gr.width(diamond_accent);
            gr.pix(-diamond_x, 0, 1, SPR_PIXDEFSMALL1);
            gr.pix(diamond_x, 0, 3, SPR_PIXDEFSMALL1);
            gr.pix(0, diamond_y, 0, SPR_PIXDEFSMALL1);
            gr.pix(0, -diamond_y, 2, SPR_PIXDEFSMALL1);
            gr.width(diamond_main_r);
            gr.pix(0, 0, 0, diamond_phase ?
                   SPR_PIXDEFTRICLEAR2B : SPR_PIXDEFTRICLEAR2A);
            gr.width(diamond_main_l);
            gr.pix(0, 0, 0, SPR_PIXDEFSMALL1);
            break;

        case 3:
            gr.width(diamond_main_l);
            gr.pix(0, 0, 0, SPR_PIXDEFTRICLEAR1);
            gr.width(diamond_main_r);
            gr.pix(0, 0, diamond_phase ? 1 : 3,
                   diamond_phase ?
                   SPR_PIXDEFTRICLEAR2A : SPR_PIXDEFTRICLEAR2B);
            break;

        case 4:
            gr.width(diamond_accent);
            gr.pix(-diamond_x, 0, 0, SPR_PIXDEFTRICLEAR3);
            gr.pix(diamond_x, 0, 0, SPR_PIXDEFTRICLEAR3);
            gr.pix(0, diamond_y, 0, SPR_PIXDEFTRICLEAR3);
            gr.pix(0, -diamond_y, 0, SPR_PIXDEFTRICLEAR3);
            gr.width(diamond_main_r);
            gr.pix(0, 0, 0, SPR_PIXDEFTRICLEAR2);
            gr.width(diamond_main_l);
            gr.pix(0, 0, 0, SPR_PIXDEFTRICLEAR1);
            break;

        case 5:
            gr.width(diamond_accent);
            gr.pix(-diamond_diag_x, -diamond_diag_y, 0,
                   diamond_phase ? SPR_PIXDEFTRICLEAR2 :
                                   SPR_PIXDEFTRICLEAR3);
            gr.pix(diamond_diag_x, diamond_diag_y, 0,
                   diamond_phase ? SPR_PIXDEFTRICLEAR2 :
                                   SPR_PIXDEFTRICLEAR3);
            gr.pix(-diamond_diag_x, diamond_diag_y, 0,
                   diamond_phase ? SPR_PIXDEFTRICLEAR3 :
                                   SPR_PIXDEFTRICLEAR2);
            gr.pix(diamond_diag_x, -diamond_diag_y, 0,
                   diamond_phase ? SPR_PIXDEFTRICLEAR3 :
                                   SPR_PIXDEFTRICLEAR2);
            gr.width(diamond_main_r);
            gr.pix(0, 0, 0, SPR_PIXDEFTRICLEAR2);
            gr.width(diamond_main_l);
            gr.pix(0, 0, 0, SPR_PIXDEFTRICLEAR1);
            break;

        case 6:
            gr.width(diamond_accent);
            gr.pix(0, diamond_y, 0, SPR_PIXDEFTRICLEAR3);
            gr.pix(0, -diamond_y, 0, SPR_PIXDEFTRICLEAR3);
            gr.width(diamond_main_r);
            gr.pix(0, 0, 0, SPR_PIXDEFTRICLEAR2);
            gr.width(diamond_main_l);
            gr.pix(0, 0, 0, SPR_PIXDEFTRICLEAR1);
            break;

        case 7:
            gr.width(diamond_accent);
            gr.pix(-diamond_x, 0, 0, SPR_PIXDEFTRICLEAR2);
            gr.pix(diamond_x, 0, 0, SPR_PIXDEFTRICLEAR2);
            gr.pix(0, diamond_y, 0, SPR_PIXDEFTRICLEAR3);
            gr.pix(0, -diamond_y, 0, SPR_PIXDEFTRICLEAR3);
            gr.width(diamond_main_l);
            gr.pix(0, 0, 0, SPR_PIXDEFTRICLEAR1);
            gr.width(diamond_main_r);
            gr.pix(0, 0, diamond_phase ? 1 : 0, SPR_PIXDEFTRICLEAR2);
            break;

        case 16:
            if (j < 4 || (j > 7 && j < 12) ||
                (j > 15 && j < 20) || (j > 23 && j < 28)) {
                gr.width((totalavg / 2) + 16);
                gr.pix(0, 45, 4, SPR_LINK2);
                gr.pix(-45, 0, 4, SPR_LINK2);
                gr.pix(45, 0, 4, SPR_LINK2);
                gr.pix(0, -45, 4, SPR_LINK2);
                gr.pix(-45, 45, 0, SPR_LINK1);
                gr.pix(45, 45, 0, SPR_LINK1);
                gr.pix(-45, -45, 0, SPR_LINK1);
                gr.pix(45, -45, 0, SPR_LINK1);
                gr.width(scalar_l + 16);
                gr.pix(0, 0, 0, SPR_LINK1);
            } else {
                gr.width((totalavg / 2) + 16);
                gr.pix(0, 45, 4, SPR_LINK1);
                gr.pix(-45, 0, 4, SPR_LINK1);
                gr.pix(45, 0, 4, SPR_LINK1);
                gr.pix(0, -45, 4, SPR_LINK1);
                gr.pix(-45, 45, 0, SPR_LINK2);
                gr.pix(45, 45, 0, SPR_LINK2);
                gr.pix(-45, -45, 0, SPR_LINK2);
                gr.pix(45, -45, 0, SPR_LINK2);
                gr.width(scalar_l + 16);
                gr.pix(0, 0, 0, SPR_LINK2);
            }
            break;

        case 17:
            gr.width((scalar_r / 3) + 16);
            gr.pix(-45, -45, 0, SPR_MUSHROOM);
            gr.pix(-45, 0, 0, SPR_MUSHROOM);
            gr.pix(45, 45, 0, SPR_MUSHROOM);
            gr.pix(45, 0, 0, SPR_MUSHROOM);
            gr.pix(-45, 45, 0, SPR_MUSHROOM);
            gr.pix(0, 45, 0, SPR_MUSHROOM);
            gr.pix(45, -45, 0, SPR_MUSHROOM);
            gr.pix(0, -45, 0, SPR_MUSHROOM);
            gr.width(scalar_l + 16);
            gr.pix(0, 0, 0,
                   ((j % 4) == 0 || ((j - 1) % 4) == 0) ?
                   SPR_MARIO : SPR_MARIOJUMP);
            break;

        case 18:
            if (h == 0 || h == 4) {
                gr.width(scalar_l + 16);
                gr.pix(0, 0, 0, SPR_PACMAN3);
                gr.pix(-45, 0, 0, SPR_PACMANFOOD);
                gr.pix((scalar_l * 8) + 25, 0, 0,
                       scalar_l < 1 ? SPR_PACMANGHOST1B :
                       SPR_PACMANDEAD1B);
                gr.width(scalar_r + 16);
                gr.pix(0, 30, 4, SPR_PACMAN3);
                gr.pix(0, -30, 4, SPR_PACMAN3);
                gr.pix(45, 30, 4, SPR_PACMANFOOD);
                gr.pix(45, -30, 4, SPR_PACMANFOOD);
                gr.pix(-((scalar_l * 8) + 25), -30, 4,
                       scalar_l < 1 ? SPR_PACMANGHOST1B :
                       SPR_PACMANDEAD1B);
                gr.pix(-((scalar_l * 8) + 25), 30, 4,
                       scalar_l < 1 ? SPR_PACMANGHOST1B :
                       SPR_PACMANDEAD1B);
            } else if (h == 2 || h == 6) {
                gr.width(scalar_l + 16);
                gr.pix(0, 0, 0, SPR_PACMAN1);
                gr.pix((scalar_l * 8) + 25, 0, 0,
                       scalar_l < 1 ? SPR_PACMANGHOST1B :
                       SPR_PACMANDEAD1B);
                gr.width(scalar_r + 16);
                gr.pix(0, 30, 4, SPR_PACMAN1);
                gr.pix(0, -30, 4, SPR_PACMAN1);
                gr.pix(-((scalar_l * 8) + 25), 30, 4,
                       scalar_l < 1 ? SPR_PACMANGHOST1B :
                       SPR_PACMANDEAD1B);
                gr.pix(-((scalar_l * 8) + 25), -30, 4,
                       scalar_l < 1 ? SPR_PACMANGHOST1B :
                       SPR_PACMANDEAD1B);
            } else {
                gr.width(scalar_l + 16);
                gr.pix(0, 0, 0, SPR_PACMAN2);
                gr.pix(-30, 0, 0, SPR_PACMANFOOD);
                gr.pix((scalar_l * 8) + 25, 0, 0,
                       scalar_l < 1 ? SPR_PACMANGHOST1A :
                       SPR_PACMANDEAD1A);
                gr.width(scalar_r + 16);
                gr.pix(0, 30, 4, SPR_PACMAN2);
                gr.pix(0, -30, 4, SPR_PACMAN2);
                gr.pix(30, 30, 4, SPR_PACMANFOOD);
                gr.pix(30, -30, 4, SPR_PACMANFOOD);
                gr.pix(-((scalar_l * 8) + 25), 30, 4,
                       scalar_l < 1 ? SPR_PACMANGHOST1A :
                       SPR_PACMANDEAD1A);
                gr.pix(-((scalar_l * 8) + 25), -30, 4,
                       scalar_l < 1 ? SPR_PACMANGHOST1A :
                       SPR_PACMANDEAD1A);
            }
            break;

        case 19:
            if ((j % 4) == 0 || ((j - 1) % 4) == 0) {
                gr.width(scalar_bigd);
                gr.pix(-40, 0, 0, SPR_SPACE_INVADERS4);
                gr.pix(40, 0, 0, SPR_SPACE_INVADERS4);
                gr.pix(0, 40, 0, SPR_SPACE_INVADERS4);
                gr.pix(0, -40, 0, SPR_SPACE_INVADERS4);
                gr.width(scalar_r + 16);
                gr.pix(-40, -40, 0, SPR_SPACE_INVADERS1);
                gr.pix(40, 40, 0, SPR_SPACE_INVADERS1);
                gr.pix(-40, 40, 0, SPR_SPACE_INVADERS1);
                gr.pix(40, -40, 0, SPR_SPACE_INVADERS1);
                gr.pix(0, 0, 0, SPR_SPACE_INVADERS1);
            } else {
                gr.width(scalar_bigd);
                gr.pix(-40, 0, 0, SPR_SPACE_INVADERS3);
                gr.pix(40, 0, 0, SPR_SPACE_INVADERS3);
                gr.pix(0, 40, 0, SPR_SPACE_INVADERS3);
                gr.pix(0, -40, 0, SPR_SPACE_INVADERS3);
                gr.width(scalar_r + 16);
                gr.pix(-40, -40, 0, SPR_SPACE_INVADERS2);
                gr.pix(40, 40, 0, SPR_SPACE_INVADERS2);
                gr.pix(-40, 40, 0, SPR_SPACE_INVADERS2);
                gr.pix(40, -40, 0, SPR_SPACE_INVADERS2);
                gr.pix(0, 0, 0, SPR_SPACE_INVADERS2);
            }
            break;

        case 20:
            gr.width(totalavg + 16);
            if ((j % 4) == 0 || ((j - 1) % 4) == 0) {
                gr.pix(-40, 25, 0, SPR_GALAGAMOTH1);
                gr.pix(0, 25, 0, SPR_GALAGAMOTH2);
                gr.pix(40, 25, 0, SPR_GALAGAMOTH1);
            } else {
                gr.pix(-40, 25, 0, SPR_GALAGAMOTH2);
                gr.pix(0, 25, 0, SPR_GALAGAMOTH1);
                gr.pix(40, 25, 0, SPR_GALAGAMOTH2);
            }
            gr.width(scalar_r + 16);
            gr.pix(j < 16 ? (j * 4 - 32) : (-j * 4 + 96),
                   -10, 0, SPR_GALAGA);
            break;

        case 21:
            gr.width(scalar_l);
            if ((j % 4) == 0 || ((j - 1) % 4) == 0) {
                gr.pix(40, 40, 4, SPR_DIGDUG1);
                gr.pix(40, -40, 4, SPR_DIGDUG1);
                gr.pix(-40, 40, 4, SPR_DIGDUG1);
                gr.pix(-40, -40, 4, SPR_DIGDUG1);
            } else {
                gr.pix(40, 40, 4, SPR_DIGDUG2);
                gr.pix(40, -40, 4, SPR_DIGDUG2);
                gr.pix(-40, 40, 4, SPR_DIGDUG2);
                gr.pix(-40, -40, 4, SPR_DIGDUG2);
            }
            gr.width(scalar_r + 16);
            if (j < 16) {
                gr.pix(-15, 0, 4,
                       ((j % 4) == 0 || ((j - 1) % 4) == 0) ?
                       SPR_DIGDUG3 : SPR_DIGDUG4);
                if (j < 4) {
                    gr.pix(0, -2, 2, SPR_DIGDUGHARPOON);
                    gr.pix(15, 0, 0, SPR_FYGAR3);
                } else if (j < 8) {
                    gr.pix(0, -2, 2, SPR_DIGDUGHARPOON);
                    gr.pix(15, 0, 0, SPR_FYGAR4);
                } else if (j < 12) {
                    gr.pix(0, -2, 2, SPR_DIGDUGHARPOON);
                    gr.pix(15, 0, 0, SPR_FYGAR5);
                } else {
                    gr.pix(15, 0, 0, SPR_FYGAR6);
                }
            } else if (j > 31) {
                gr.pix(-10, -2, 4, SPR_DIGDUGHARPOON);
                gr.pix(j * 2 - 79, 0, 4, SPR_DIGDUG5);
                gr.pix(15, 0, 0, SPR_FYGAR1);
            } else if ((j % 4) == 0 || ((j - 1) % 4) == 0) {
                gr.pix(j * 2 - 79, 0, 4, SPR_DIGDUG1);
                gr.pix(-j * 2 + 79, 0, 0, SPR_FYGAR1);
            } else {
                gr.pix(j * 2 - 79, 0, 4, SPR_DIGDUG2);
                gr.pix(-j * 2 + 79, 0, 0, SPR_FYGAR2);
            }
            break;

        case 22:
            if ((j % 8) == 0 || ((j - 1) % 8) == 0) {
                gr.width(scalar_r + 16);
                gr.pix(40, 40, 0, SPR_BUBBLE4);
                gr.pix(40, -40, 0, SPR_BUBBLE4);
                gr.pix(-40, 40, 0, SPR_BUBBLE4);
                gr.pix(-40, -40, 0, SPR_BUBBLE4);
                gr.width(totalavg + 16);
                gr.pix(0, 0, 0, SPR_BUBBLEBOBBLE1);
            } else if (((j - 2) % 8) == 0 || ((j - 3) % 8) == 0) {
                gr.width(scalar_r + 16);
                gr.pix(10, 10, 0, SPR_BUBBLE1);
                gr.pix(10, -10, 0, SPR_BUBBLE1);
                gr.pix(-10, 10, 0, SPR_BUBBLE1);
                gr.pix(-10, -10, 0, SPR_BUBBLE1);
                gr.width(totalavg + 16);
                gr.pix(0, 0, 0, SPR_BUBBLEBOBBLE2);
            } else if (((j - 4) % 8) == 0 || ((j - 5) % 8) == 0) {
                gr.width(scalar_r + 16);
                gr.pix(20, 20, 0, SPR_BUBBLE2);
                gr.pix(20, -20, 0, SPR_BUBBLE2);
                gr.pix(-20, 20, 0, SPR_BUBBLE2);
                gr.pix(-20, -20, 0, SPR_BUBBLE2);
                gr.width(totalavg + 16);
                gr.pix(0, 0, 0, SPR_BUBBLEBOBBLE1);
            } else {
                gr.width(scalar_r + 16);
                gr.pix(30, 30, 0, SPR_BUBBLE3);
                gr.pix(30, -30, 0, SPR_BUBBLE3);
                gr.pix(-30, 30, 0, SPR_BUBBLE3);
                gr.pix(-30, -30, 0, SPR_BUBBLE3);
                gr.width(totalavg + 16);
                gr.pix(0, 0, 0, SPR_BUBBLEBOBBLE3);
            }
            break;

        case 23:
            if ((j % 8) == 0 || ((j - 1) % 8) == 0) {
                gr.width((totalavg / 2) + 16);
                gr.pix(45, 0, 0, SPR_FINALFANTASY3);
                gr.pix(-45, 0, 0, SPR_FINALFANTASY3);
                gr.pix(0, 45, 0, SPR_FINALFANTASY3);
                gr.pix(0, -45, 0, SPR_FINALFANTASY3);
                gr.width(scalar_l + 16);
                gr.pix(0, 0, 0, SPR_FINALFANTASY1);
            } else if (((j - 2) % 8) == 0 || ((j - 3) % 8) == 0) {
                gr.width((totalavg / 2) + 16);
                gr.pix(45, 0, 0, SPR_FINALFANTASY4);
                gr.pix(-45, 0, 0, SPR_FINALFANTASY4);
                gr.pix(0, 45, 0, SPR_FINALFANTASY4);
                gr.pix(0, -45, 0, SPR_FINALFANTASY4);
                gr.width(scalar_l + 16);
                gr.pix(0, 0, 0, SPR_FINALFANTASY2);
            } else if (((j - 4) % 8) == 0 || ((j - 5) % 8) == 0) {
                gr.width((totalavg / 2) + 16);
                gr.pix(45, 0, 0, SPR_FINALFANTASY1);
                gr.pix(-45, 0, 0, SPR_FINALFANTASY1);
                gr.pix(0, 45, 0, SPR_FINALFANTASY1);
                gr.pix(0, -45, 0, SPR_FINALFANTASY1);
                gr.width(scalar_l + 16);
                gr.pix(0, 0, 0, SPR_FINALFANTASY3);
            } else {
                gr.width((totalavg / 2) + 16);
                gr.pix(45, 0, 0, SPR_FINALFANTASY2);
                gr.pix(-45, 0, 0, SPR_FINALFANTASY2);
                gr.pix(0, 45, 0, SPR_FINALFANTASY2);
                gr.pix(0, -45, 0, SPR_FINALFANTASY2);
                gr.width(scalar_l + 16);
                gr.pix(0, 0, 0, SPR_FINALFANTASY4);
            }
            break;

        case 24:
            gr.width(0);
            for (int q = -6; q <= 6; ++q) {
                character_assassination_textmode(gr, scalar_l, scalar_r, false);
                gr.color(1);
                gr.text(q * 15, 0, str1);
                gr.text(q * 20, 10 * scalar_l, str2);
                gr.text(q * 25, -10 * scalar_l, str3);
                gr.color(2);
                gr.text(q * 15, 30, str1);
                gr.text(q * 20, 30 + 10 * scalar_l, str2);
                gr.text(q * 25, 30 - 10 * scalar_l, str3);
                gr.color(3);
                gr.text(q * 15, -30, str1);
                gr.text(q * 20, -30 + 10 * scalar_l, str2);
                gr.text(q * 25, -30 - 10 * scalar_l, str3);
            }
            break;

        case 25:
            gr.width(0);
            for (int q = -6; q <= 6; ++q) {
                character_assassination_textmode(gr, scalar_l, scalar_r, false);
                gr.color(1);
                gr.text(0, q * 15, str1);
                gr.text(5 * scalar_l, q * 20, str2);
                gr.text(-5 * scalar_l, q * 25, str3);
                gr.color(2);
                gr.text(q * 15, 0, str1);
                gr.text(q * 20, 5 * scalar_l, str2);
                gr.text(q * 25, -5 * scalar_l, str3);
            }
            break;

        case 26:
            gr.width(0);
            for (int q = -6; q <= 6; ++q) {
                character_assassination_textmode(gr, scalar_l, scalar_r, false);
                gr.color(1);
                gr.text(0, q * 15, str1);
                gr.text(5 * scalar_l, q * 20, str2);
                gr.text(-5 * scalar_l, q * 25, str3);
                gr.color(2);
                gr.text(30, q * 15, str1);
                gr.text(30 + 5 * scalar_l, q * 20, str2);
                gr.text(30 - 5 * scalar_l, q * 25, str3);
                gr.color(3);
                gr.text(-30, q * 15, str1);
                gr.text(-30 + 5 * scalar_l, q * 20, str2);
                gr.text(-30 - 5 * scalar_l, q * 25, str3);
            }
            break;

        case 27:
            gr.width(0);
            for (int q = 0; q <= 12; ++q) {
                character_assassination_textmode(gr, scalar_l, scalar_r, false);
                gr.color(1);
                gr.textarc(0, 0, 30, 30, q * 683, str2);
                gr.color(2);
                gr.textarc(0, 0, 15 + 5 * scalar_l, 15 + 5 * scalar_l,
                           q * 683, str3);
            }
            break;

        case 28:
            gr.width(0);
            for (int q = 0; q <= 12; ++q) {
                character_assassination_textmode(gr, scalar_l, scalar_r, false);
                gr.color(1);
                gr.textarc(0, 0, 15 + 5 * scalar_l, 15 + 5 * scalar_l,
                           q * 683, str2);
                gr.color(2);
                gr.textarc(-45, 0, 15, 15, q * 683, str3);
                gr.textarc(45, 0, 15, 15, q * 683, str3);
            }
            break;

        case 29:
            gr.width(0);
            character_assassination_textmode(gr, scalar_l, scalar_r, true);
            gr.color(1);
            gr.text(0, 0, str2);
            break;

        case 30:
            gr.width(0);
            for (int q = -1; q <= 1; ++q) {
                character_assassination_textmode(gr, scalar_l, scalar_r, false);
                gr.color(1);
                gr.text(q * 15, 0, str1);
                gr.color(2);
                gr.text(q * 15 - 5 * scalar_l, 5 * scalar_l, str2);
                gr.text(q * 15 + 5 * scalar_l, -5 * scalar_l, str3);
                gr.color(3);
                gr.text(q * 15 + 5 * scalar_l, 5 * scalar_l, str2);
                gr.text(q * 15 - 5 * scalar_l, -5 * scalar_l, str3);
            }
            break;

        case 31:
            gr.width(0);
            character_assassination_textmode(gr, scalar_l, scalar_r, true);
            gr.color(1);
            gr.text(0, 0, str2);
            gr.color(2);
            gr.text(0, 30, str2);
            gr.text(0, -30, str2);
            gr.text(-30, 0, str2);
            gr.text(30, 0, str2);
            gr.color(3);
            gr.text(30, 30, str3);
            gr.text(30, -30, str3);
            gr.text(-30, 30, str3);
            gr.text(-30, -30, str3);
            break;
    }
}

static void render_vidpix_frame(uint8_t** target_lines,
                                int target_width,
                                int target_height,
                                int target_pitch)
{
    if (s_visualizer.active_mode < 0 ||
        s_visualizer.active_mode >= kVidPixModeCount)
        return;

    s_visualizer.gfx.attach(target_lines, target_width, target_height,
                            target_pitch);
    s_visualizer.gfx.select_palette(vidpix_palette_variant());
    int glitch_y_tiles = kVidPixNormalYTiles;
    int glitch1_level = 0;
    if (vidpix_glitch_enabled())
        vidpix_glitch_from_knobs(&glitch1_level, &glitch_y_tiles);
    s_visualizer.gfx.set_glitch_effect(glitch1_level, glitch_y_tiles);

    VidPixInputFrame input = make_input_frame();
    bool midi_clock_anim = sync_vidpix_counters_to_midi_clock();

    const VidPixModeInfo& mode = kVidPixModes[s_visualizer.active_mode];
    int displayroutine =
        mode.base_pattern + s_visualizer.pattern[s_visualizer.active_mode];
    if (displayroutine >= 24 && displayroutine <= 31)
        advance_character_assassination();

    s_visualizer.gfx.clear();
    render_vidpix_pattern(s_visualizer.gfx, input, displayroutine);
    if (s_visualizer.gfx.uses_canvas()) {
        // GB commits during blanking; NES reaches this path inside its render slot.
        if (s_visualizer.gfx.flavor() == EMU_GB)
            video_sync();
        s_visualizer.gfx.commit_canvas();
    }

    if (!midi_clock_anim && s_visualizer.anim_step_changed)
        advance_vidpix_counters();
}

} // namespace

void midi_visualizer_init(int width, int height, int flavor)
{
    s_visualizer.gfx.init(width, height, flavor);
}

int midi_visualizer_mode_count()
{
    return kVidPixModeCount;
}

void midi_visualizer_activate(int index)
{
    if (index < 0 || index >= kVidPixModeCount)
        return;
    s_visualizer.active_mode = index;
    s_visualizer.frame = 0;
    s_visualizer.j = 1;
    s_visualizer.h = 0;
    s_visualizer.character[0] = 33;
    s_visualizer.character[1] = 48;
    s_visualizer.character[2] = 65;
    s_visualizer.character_rng = 0x51A7C3E9u;
    s_visualizer.last_character_step = 0;
    s_visualizer.last_character_ms = millis();
    s_visualizer.anim_step_changed = true;
    s_visualizer.midi_clock_anim_active = false;
    s_visualizer.last_midi_anim_step = 0;
    s_visualizer.force_render = true;
    memset(s_visualizer.pattern, 0, sizeof(s_visualizer.pattern));
}

void midi_visualizer_deactivate()
{
    video_clear_nes_visualizer_palette();
    s_nes_output_palette = -1;
    s_visualizer.active_mode = -1;
    s_visualizer.frame = 0;
}

bool midi_visualizer_is_active()
{
    return s_visualizer.active_mode >= 0 &&
           s_visualizer.active_mode < kVidPixModeCount;
}

void midi_visualizer_cycle_palette()
{
    cycle_vidpix_palette();
    s_visualizer.force_render = true;
}

void midi_visualizer_cycle_sprite()
{
    if (!midi_visualizer_is_active())
        return;
    const uint8_t count = kVidPixModes[s_visualizer.active_mode].pattern_count;
    if (count == 0)
        return;
    uint8_t& pattern = s_visualizer.pattern[s_visualizer.active_mode];
    pattern = (uint8_t)((pattern + 1u) % count);
    s_visualizer.force_render = true;
}

void midi_visualizer_set_sprite_control(uint8_t value)
{
    if (!midi_visualizer_is_active())
        return;
    const uint8_t count = kVidPixModes[s_visualizer.active_mode].pattern_count;
    if (count == 0)
        return;
    uint8_t pattern = (uint8_t)(((uint16_t)value * count) >> 7);
    if (pattern >= count)
        pattern = (uint8_t)(count - 1);
    s_visualizer.pattern[s_visualizer.active_mode] = pattern;
    s_visualizer.force_render = true;
}

void midi_visualizer_auto_advance_scene()
{
    if (!midi_visualizer_is_active())
        return;

    cycle_vidpix_palette();
    const uint8_t count =
        kVidPixModes[s_visualizer.active_mode].pattern_count;
    if (count > 0) {
        uint8_t& pattern = s_visualizer.pattern[s_visualizer.active_mode];
        pattern = (uint8_t)((pattern + 1u) % count);
    }
    s_visualizer.force_render = true;
}

bool midi_visualizer_update(uint8_t** target_lines,
                            int target_width,
                            int target_height,
                            int target_pitch,
                            uint8_t* scratch,
                            size_t scratch_bytes)
{
    if (!midi_visualizer_is_active() || !target_lines)
        return false;

    s_visualizer.gfx.attach_canvas(scratch, scratch_bytes);

    // Cap NES visual work at 30 Hz; explicit scene changes bypass the cap.
    if (s_visualizer.gfx.flavor() == EMU_NES &&
        !s_visualizer.force_render &&
        (s_visualizer.frame % kVidPixNesRenderDivisor) != 0) {
        ++s_visualizer.frame;
        return false;
    }

    render_vidpix_frame(target_lines, target_width, target_height,
                        target_pitch);
    s_visualizer.force_render = false;
    ++s_visualizer.frame;
    return true;
}
