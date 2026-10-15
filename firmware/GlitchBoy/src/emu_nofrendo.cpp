
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
#include "media.h"

#include <Arduino.h>
#include <sys/stat.h>

extern "C" {
#include "nofrendo/osd.h"
#include "nofrendo/event.h"
#include "nofrendo/nes_apu.h"
#include "nofrendo/nes_apu_midi.h"
};
#include "math.h"


using namespace std;

#ifndef NES_ADAPTIVE_FRAME_SKIP
#define NES_ADAPTIVE_FRAME_SKIP 0
#endif

#ifndef NES_AUDIO_OUTPUT_GAIN_NUM
#define NES_AUDIO_OUTPUT_GAIN_NUM 3
#endif

#ifndef NES_AUDIO_OUTPUT_GAIN_DEN
#define NES_AUDIO_OUTPUT_GAIN_DEN 2
#endif

#ifndef NES_AUDIO_QUEUE_FEEDBACK
#define NES_AUDIO_QUEUE_FEEDBACK 1
#endif

#ifndef NES_AUDIO_QUEUE_TARGET
#define NES_AUDIO_QUEUE_TARGET 2048u
#endif

#ifndef NES_AUDIO_QUEUE_TARGET_MIDI
#define NES_AUDIO_QUEUE_TARGET_MIDI MUSIC_AUDIO_QUEUE_TARGET_SAMPLES
#endif

#ifndef NES_AUDIO_QUEUE_DEADBAND
#define NES_AUDIO_QUEUE_DEADBAND 160u
#endif

#ifndef NES_AUDIO_QUEUE_FEEDBACK_DIV
#define NES_AUDIO_QUEUE_FEEDBACK_DIV 320
#endif

#ifndef NES_AUDIO_QUEUE_MAX_DRAIN
#define NES_AUDIO_QUEUE_MAX_DRAIN 4
#endif

// https://wiki.nesdev.com/w/index.php/NTSC_video
// NES/SMS have pixel rates of 5.3693175, or 2/3 color clock
// in 3 phase mode each pixel gets 2 DAC values written, 2 color clocks = 3 nes pixels

uint32_t nes_3_phase[64] = {
    0x2C2C2C00,0x241D2400,0x221D2600,0x1F1F2700,0x1D222600,0x1D242400,0x1D262200,0x1F271F00,
    0x22261D00,0x24241D00,0x26221D00,0x271F1F00,0x261D2200,0x14141400,0x14141400,0x14141400,
    0x38383800,0x2C252C00,0x2A252E00,0x27272F00,0x252A2E00,0x252C2C00,0x252E2A00,0x272F2700,
    0x2A2E2500,0x2C2C2500,0x2E2A2500,0x2F272700,0x2E252A00,0x1F1F1F00,0x15151500,0x15151500,
    0x45454500,0x3A323A00,0x37333C00,0x35353C00,0x33373C00,0x323A3A00,0x333C3700,0x353C3500,
    0x373C3300,0x3A3A3200,0x3C373300,0x3C353500,0x3C333700,0x2B2B2B00,0x16161600,0x16161600,
    0x45454500,0x423B4200,0x403B4400,0x3D3D4500,0x3B404400,0x3B424200,0x3B444000,0x3D453D00,
    0x40443B00,0x42423B00,0x44403B00,0x453D3D00,0x443B4000,0x39393900,0x17171700,0x17171700,
};
uint32_t nes_4_phase[64] = {
    0x2C2C2C2C,0x241D1F26,0x221D2227,0x1F1D2426,0x1D1F2624,0x1D222722,0x1D24261F,0x1F26241D,
    0x2227221D,0x24261F1D,0x26241D1F,0x27221D22,0x261F1D24,0x14141414,0x14141414,0x14141414,
    0x38383838,0x2C25272E,0x2A252A2F,0x27252C2E,0x25272E2C,0x252A2F2A,0x252C2E27,0x272E2C25,
    0x2A2F2A25,0x2C2E2725,0x2E2C2527,0x2F2A252A,0x2E27252C,0x1F1F1F1F,0x15151515,0x15151515,
    0x45454545,0x3A33353C,0x3732373C,0x35333A3C,0x33353C3A,0x32373C37,0x333A3C35,0x353C3A33,
    0x373C3732,0x3A3C3533,0x3C3A3335,0x3C373237,0x3C35333A,0x2B2B2B2B,0x16161616,0x16161616,
    0x45454545,0x423B3D44,0x403B4045,0x3D3B4244,0x3B3D4442,0x3B404540,0x3B42443D,0x3D44423B,
    0x4045403B,0x42443D3B,0x44423B3D,0x45403B40,0x443D3B42,0x39393939,0x17171717,0x17171717,
};


// PAL yuyv table, must be in RAM
uint32_t _nes_yuv_4_phase_pal[] = {
    0x31313131,0x2D21202B,0x2720252D,0x21212B2C,0x1D23302A,0x1B263127,0x1C293023,0x202B2D22,
    0x262B2722,0x2C2B2122,0x2F2B1E23,0x31291F27,0x30251F2A,0x18181818,0x19191919,0x19191919,
    0x3D3D3D3D,0x34292833,0x2F282D34,0x29283334,0x252B3732,0x232E392E,0x2431382B,0x28333429,
    0x2D342F28,0x33342928,0x3732252A,0x392E232E,0x382B2431,0x24242424,0x1A1A1A1A,0x1A1A1A1A,
    0x49494949,0x42373540,0x3C373B40,0x36374040,0x3337433F,0x3139433B,0x323D4338,0x35414237,
    0x3B423D35,0x41413736,0x453F3238,0x473C313B,0x4639323F,0x2F2F2F2F,0x1A1A1A1A,0x1A1A1A1A,
    0x49494949,0x48413D45,0x42404345,0x3D3F4644,0x3B3D4543,0x3B3E4542,0x3B42453F,0x3E47463E,
    0x434A453E,0x46483E3D,0x4843393E,0x4A403842,0x4B403944,0x3E3E3E3E,0x1B1B1B1B,0x1B1B1B1B,
    //odd
    0x31313131,0x20212D2B,0x2520272D,0x2B21212C,0x30231D2A,0x31261B27,0x30291C23,0x2D2B2022,
    0x272B2622,0x212B2C22,0x1E2B2F23,0x1F293127,0x1F25302A,0x18181818,0x19191919,0x19191919,
    0x3D3D3D3D,0x28293433,0x2D282F34,0x33282934,0x372B2532,0x392E232E,0x3831242B,0x34332829,
    0x2F342D28,0x29343328,0x2532372A,0x232E392E,0x242B3831,0x24242424,0x1A1A1A1A,0x1A1A1A1A,
    0x49494949,0x35374240,0x3B373C40,0x40373640,0x4337333F,0x4339313B,0x433D3238,0x42413537,
    0x3D423B35,0x37414136,0x323F4538,0x313C473B,0x3239463F,0x2F2F2F2F,0x1A1A1A1A,0x1A1A1A1A,
    0x49494949,0x3D414845,0x43404245,0x463F3D44,0x453D3B43,0x453E3B42,0x45423B3F,0x46473E3E,
    0x454A433E,0x3E48463D,0x3943483E,0x38404A42,0x39404B44,0x3E3E3E3E,0x1B1B1B1B,0x1B1B1B1B,
};

// make_yuv_palette from RGB palette
void make_yuv_palette(const char* name, const uint32_t* rgb, int len)
{
    uint32_t pal[256*2];
    uint32_t* even = pal;
    uint32_t* odd = pal + len;

    float chroma_scale = BLANKING_LEVEL/2/256;
    chroma_scale /= 80;
    for (int i = 0; i < len; i++) {
        uint8_t r = rgb[i] >> 16;
        uint8_t g = rgb[i] >> 8;
        uint8_t b = rgb[i];

        float y = 0.299 * r + 0.587*g + 0.114 * b;
        float u = -0.147407 * r - 0.289391 * g + 0.436798 * b;
        float v =  0.614777 * r - 0.514799 * g - 0.099978 * b;
        y /= 255.0;
        y = (y*(WHITE_LEVEL-BLACK_LEVEL) + BLACK_LEVEL)/256;

        uint32_t e = 0;
        uint32_t o = 0;
        for (int i = 0; i < 4; i++) {
            float p = 2*M_PI*i/4 + M_PI;
            float s = sin(p)*chroma_scale;
            float c = cos(p)*chroma_scale;
            uint8_t e0 = round(y + (s*u) + (c*v));
            uint8_t o0 = round(y + (s*u) - (c*v));
            e = (e << 8) | e0;
            o = (o << 8) | o0;
        }
        *even++ = e;
        *odd++ = o;
    }

    printf("uint32_t %s_4_phase_pal[] = {\n",name);
    for (int i = 0; i < len*2; i++) {  // start with luminance map
        printf("0x%08X,",pal[i]);
        if ((i & 7) == 7)
            printf("\n");
        if (i == (len-1)) {
            printf("//odd\n");
        }
    }
    printf("};\n");

}

extern rgb_t nes_palette[64];
extern "C" void pal_generate();
void make_alt_pal()
{
    pal_generate();
    uint32_t pal[64];
    for (int i = 0; i < 64; i++) {
        auto p = nes_palette[i];
        pal[i] = (p.r << 16) | (p.g << 8) | p.b;
    }
    make_yuv_palette("_nes_yuv",pal,64);
}

static const float _nes_luma[] = {
    0.50,0.29,0.29,0.29,0.29,0.29,0.29,0.29,0.29,0.29,0.29,0.29,0.29,0.00,0.02,0.02,
    0.75,0.45,0.45,0.45,0.45,0.45,0.45,0.45,0.45,0.45,0.45,0.45,0.45,0.24,0.04,0.04,
    1.00,0.73,0.73,0.73,0.73,0.73,0.73,0.73,0.73,0.73,0.73,0.73,0.73,0.47,0.05,0.05,
    1.00,0.90,0.90,0.90,0.90,0.90,0.90,0.90,0.90,0.90,0.90,0.90,0.90,0.77,0.07,0.07,
};

static void make_nes_palette(int phases)
{
    float saturation = 0.5;
    printf("uint32_t nes_%d_phase[64] = {\n",phases);
    for (int i = 0; i < 64; i++) {
        int chroma = (i & 0xF);
        int luma = _nes_luma[i]*(WHITE_LEVEL-BLACK_LEVEL) + BLANKING_LEVEL;

        int p[8] = {0};
        for (int j = 0; j < phases; j++)
            p[j] = luma;  // 0x1D is really black, really BLANKING_LEVEL

        chroma -= 1;
        if (chroma >= 0 && chroma < 12) {
            for (int j = 0; j < phases; j++)
                p[j] += sin(2*M_PI*(5 + chroma + (12/phases)*j)/12) * BLANKING_LEVEL/2*saturation;   // not sure why 5 is the right offset
        }

        uint32_t pi = 0;
        for (int j = 0; j < 4; j++)
            pi = (pi << 8) | p[j] >> 8;
        printf("0x%08X,",pi);
        if ((i & 7) == 7)
            printf("\n");
    }
    printf("};\n");
}

uint8_t* _nofrendo_rom = 0;
extern "C"
char *osd_getromdata()
{
    return (char *)_nofrendo_rom;
}

extern "C"
int nes_emulate_init(const char* path, int width, int height);

extern "C"
uint8_t** nes_emulate_boot_buffer(int width, int height);

extern "C"
void nes_emulate_shutdown(void);

extern "C"
uint8_t** nes_emulate_frame(bool draw_flag);

static void (*nes_sound_cb)(void *buffer, int length) = 0;

extern uint32_t nes_pal[256];
extern uint32_t _frame_time;

const char* _nes_help[] = {
    "Keyboard:",
    "  Arrow Keys - D-Pad",
    "  Left Shift - Button A",
    "  Option     - Button B",
    "  Return     - Start",
    "  Tab        - Select",
    "  F1         - Game Menu",
    "",
    "Controller:",
    "  D-Pad      - D-Pad",
    "  A,1        - Button A",
    "  B,2        - Button B",
    "  +          - Start",
    "  -          - Select",
    "  + & -      - Game Menu",
    0
};

const char* _nes_ext[] = {
    "nes",
    0
};

int _audio_frequency;
extern "C"
void osd_getsoundinfo(sndinfo_t *info)
{
    info->sample_rate = _audio_frequency;
    info->bps = 16;
}

extern "C"
void osd_setsound(void (*playfunc)(void *buffer, int length))
{
    nes_sound_cb = playfunc;
}

std::string to_string(int i);
class EmuNofrendo : public Emu {
    uint8_t** _lines;
    int _skip_warmup;
    bool _visualizer_active;
#if NES_AUDIO_QUEUE_FEEDBACK
    int32_t _audio_queue_feedback_accum;
#endif

    int audio_queue_feedback_samples(int base_samples)
    {
#if NES_AUDIO_QUEUE_FEEDBACK
        if (base_samples <= 0)
            return 0;

        const uint32_t capacity = audio_queue_capacity();
        if (capacity == 0)
            return 0;

        const bool midi_low_latency =
            _visualizer_active && nes_apu_midi_is_enabled();
        uint32_t target = midi_low_latency ?
            NES_AUDIO_QUEUE_TARGET_MIDI : NES_AUDIO_QUEUE_TARGET;
        if (target >= capacity)
            target = capacity / 2u;
        if (target == 0)
            target = 1u;

        const uint32_t deadband = midi_low_latency ?
            MUSIC_AUDIO_QUEUE_DEADBAND_SAMPLES : NES_AUDIO_QUEUE_DEADBAND;
        const uint32_t queued = audio_queue_level();
        const int div = NES_AUDIO_QUEUE_FEEDBACK_DIV > 0 ?
            NES_AUDIO_QUEUE_FEEDBACK_DIV : 1;
        const int max_drain = NES_AUDIO_QUEUE_MAX_DRAIN > 0 ?
            NES_AUDIO_QUEUE_MAX_DRAIN : 1;

        int correction = 0;
        if (queued > target + deadband) {
            uint32_t surplus = queued - target - deadband;
            if (surplus > 2048u)
                surplus = 2048u;
            _audio_queue_feedback_accum += (int32_t)surplus;
            while (_audio_queue_feedback_accum >= div &&
                   correction > -max_drain) {
                _audio_queue_feedback_accum -= div;
                --correction;
            }
        } else if (queued + deadband < target) {
            // The visualizer refills low audio queues.
            if (_audio_queue_feedback_accum > 0)
                _audio_queue_feedback_accum >>= 1;
        } else {
            if (_audio_queue_feedback_accum > 0)
                --_audio_queue_feedback_accum;
            else if (_audio_queue_feedback_accum < 0)
                ++_audio_queue_feedback_accum;
        }

        if (correction < -base_samples)
            correction = -base_samples;
        return correction;
#else
        (void)base_samples;
        return 0;
#endif
    }

    static bool valid_nes_source_header(const uint8_t* h, int len)
    {
        return len >= 16 &&
               h[0] == 'N' && h[1] == 'E' &&
               h[2] == 'S' && h[3] == 0x1A;
    }

    static uint32_t nes_crc32_update(uint32_t crc, const uint8_t* data, size_t len)
    {
        while (len--) {
            crc ^= *data++;
            for (int bit = 0; bit < 8; bit++) {
                if (crc & 1)
                    crc = (crc >> 1) ^ 0xEDB88320UL;
                else
                    crc >>= 1;
            }
        }
        return crc;
    }

    static int nes_mapper_from_header(const uint8_t* h)
    {
        int mapper = h[6] >> 4;
        bool nes20 = (h[7] & 0x0C) == 0x08;

        if (nes20)
            return mapper | (h[7] & 0xF0) | ((h[8] & 0x0F) << 8);

        bool clean = true;
        for (int i = 8; i < 16; i++) {
            if (h[i] != 0) {
                clean = false;
                break;
            }
        }

        if (clean || !(h[7] == 'D' && memcmp(h + 8, "iskDude!", 8) == 0))
            mapper |= (h[7] & 0xF0);

        return mapper;
    }

    static bool nes_file_payload_crc32(const std::string& path, const uint8_t* h, uint32_t* crc_out)
    {
        FILE* f = fopen(path.c_str(), "rb");
        if (!f)
            return false;

        int skip = 16 + ((h[6] & 0x04) ? 512 : 0);
        int remaining = (h[4] * 16 * 1024) + (h[5] * 8 * 1024);
        uint32_t crc = 0xFFFFFFFFUL;
        uint8_t buf[256];

        if (fseek(f, skip, SEEK_SET) != 0) {
            fclose(f);
            return false;
        }

        while (remaining > 0) {
            int want = remaining < (int)sizeof(buf) ? remaining : (int)sizeof(buf);
            size_t got = fread(buf, 1, want, f);
            if (got != (size_t)want) {
                fclose(f);
                return false;
            }
            crc = nes_crc32_update(crc, buf, got);
            remaining -= want;
        }

        fclose(f);
        *crc_out = ~crc;
        return true;
    }

    static int nes_display_mapper(const std::string& path, const uint8_t* h)
    {
        static const uint32_t cv2_us_prg_chr_crc32 = 0xC471E42DUL;
        int mapper = nes_mapper_from_header(h);
        uint32_t payload_crc;

        if (10 == mapper && h[4] == 8 && h[5] == 16 &&
            nes_file_payload_crc32(path, h, &payload_crc) &&
            payload_crc == cv2_us_prg_chr_crc32) {
            mapper = 1;
        }

        return mapper;
    }

    static bool is_builtin_default_rom_path(const std::string& path)
    {
        size_t slash = path.find_last_of("/\\");
        std::string leaf = (slash == std::string::npos) ? path : path.substr(slash + 1);
        return leaf == "Sliding Blaster.nes" ||
               leaf == "sliding_blaster.nes";
    }

    bool rebuild_builtin_default_rom(const std::string& path)
    {
        size_t slash = path.find_last_of("/\\");
        if (slash == std::string::npos)
            return false;

        std::string dir = path.substr(0, slash);
        printf("nofrendo rebuilding bundled default ROM %s\n", path.c_str());
        remove(path.c_str());
        make_default_media(dir);
        return true;
    }

public:
    EmuNofrendo(int ntsc) : Emu("nofrendo",256,240,ntsc,(16 | (1 << 8)),4,EMU_NES)    // audio is 16bit, 3 or 6 cc width
    {
        _lines = 0;
        _skip_warmup = 30;
        _visualizer_active = false;
#if NES_AUDIO_QUEUE_FEEDBACK
        _audio_queue_feedback_accum = 0;
#endif
        _ext = _nes_ext;
        _help = _nes_help;
#if AUDIO_NES_2X_SCANLINE && AUDIO_OUTPUT_2X_SCANLINE
        audio_frequency =
#if AUDIO_NES_TIMER_OUTPUT
            standard ? AUDIO_TIMER_ACTUAL_RATE(AUDIO_OUTPUT_RATE_NTSC_HZ)
                     : AUDIO_TIMER_ACTUAL_RATE(AUDIO_OUTPUT_RATE_PAL_HZ);
#else
            standard ? AUDIO_OUTPUT_RATE_NTSC_HZ
                     : AUDIO_OUTPUT_RATE_PAL_HZ;
#endif
        audio_frame_samples = standard
            ? (int)(((double)audio_frequency * 65536.0 /
                     VIDEO_NTSC_FRAME_RATE_HZ) + 0.5)
            : (audio_frequency << 16)/50;
        audio_fraction = 0;
#endif
        _audio_frequency = audio_frequency;
    }

    virtual void set_visualizer_active(bool active)
    {
        _visualizer_active = active;
    }

    virtual void prepare_music_mode_audio()
    {
        uint32_t enable_reg = 0;
        uint32_t mix_enable = 0;
        uint32_t sample_rate = 0;
        uint32_t refresh_rate = 0;
        uint32_t base_freq_hz = 0;
        apu_getdiag(&enable_reg, &mix_enable, &sample_rate,
                    &refresh_rate, &base_freq_hz);
        const int refresh = standard ? 60 : 50;
        if (sample_rate == 0 || refresh_rate == 0 || base_freq_hz == 0) {
            apu_create(0, audio_frequency, refresh, 16);
            nes_sound_cb = apu_process;
#if NES_AUDIO_QUEUE_FEEDBACK
            _audio_queue_feedback_accum = 0;
#endif
        } else if ((mix_enable & 0x1F) == 0) {
            for (int ch = 0; ch < 5; ++ch)
                apu_setchan(ch, true);
            nes_sound_cb = apu_process;
        }
    }

    virtual void gen_palettes()
    {
        make_nes_palette(3);
        make_nes_palette(4);
        make_alt_pal();
    }

    virtual int info(const string& file, vector<string>& strs)
    {
        string ext = get_ext(file);
        uint8_t hdr[16];
        int len = Emu::head(file,hdr,sizeof(hdr));
        size_t slash = file.find_last_of("/\\");
        string name = (slash == string::npos) ? file : file.substr(slash + 1);
        strs.push_back(name);
        strs.push_back(::to_string(len/1024) + "k NES Cartridge");
        strs.push_back("");
        if (valid_nes_source_header(hdr, sizeof(hdr))) {
            int prg = hdr[4] * 16;
            int chr = hdr[5] * 8;
            int mapper = nes_display_mapper(file, hdr);
            char buf[64];
            sprintf(buf,"MAP:%d",mapper);
            strs.push_back(buf);
            sprintf(buf,"PRG:%dk",prg);
            strs.push_back(buf);
            sprintf(buf,"CHR:%dk",chr);
            strs.push_back(buf);
        }
        return 0;
    }

    void pad(int pressed, int index)
    {
        event_t e = event_get(index);
        e(pressed);
    }

    enum {
        event_joypad1_up_ = 1,
        event_joypad1_down_ = 2,
        event_joypad1_left_ = 4,
        event_joypad1_right_ = 8,
        event_joypad1_start_ = 16,
        event_joypad1_select_ = 32,
        event_joypad1_a_ = 64,
        event_joypad1_b_ = 128,

        event_soft_reset_ = 256,
        event_hard_reset_ = 512
    };

    const int _nes_1[11] = {
        event_joypad1_up,
        event_joypad1_down,
        event_joypad1_left,
        event_joypad1_right,
        event_joypad1_start,
        event_joypad1_select,
        event_joypad1_a,
        event_joypad1_b,

        event_soft_reset,
        event_hard_reset,
        0
    };

    const int _nes_2[9] = {
        event_joypad2_up,
        event_joypad2_down,
        event_joypad2_left,
        event_joypad2_right,
        event_joypad2_start,
        event_joypad2_select,
        event_joypad2_a,
        event_joypad2_b,
        0
    };

    // Rotated 90%
    const uint32_t _common_nes[16] = {
        0,  // msb
        0,
        0,
        event_joypad1_start_,       // PLUS
        event_joypad1_left_,        // UP
        event_joypad1_right_,       // DOWN
        event_joypad1_up_,          // RIGHT
        event_joypad1_down_,        // LEFT

        0, // HOME
        0,
        0,
        event_joypad1_select_,  // MINUS
        event_joypad1_a_,      // A
        event_joypad1_b_,      // B
        event_joypad1_b_,      // ONE
        event_joypad1_a_,      // TWO
    };

    const uint32_t _classic_nes[16] = {
        event_joypad1_right_,    // RIGHT
        event_joypad1_down_,     // DOWN
        0,                       // LEFT_TOP
        event_joypad1_select_,    // MINUS
        0,                        // HOME
        event_joypad1_start_,     // PLUS
        0,                    // RIGHT_TOP
        0,

        0,                  // LOWER_LEFT
        event_joypad1_b_,   // B
        0,                  // Y
        event_joypad1_a_,   // A
        0,                  // X
        0,                  // LOWER_RIGHT
        event_joypad1_left_, // LEFT
        event_joypad1_up_   // UP
    };

    const uint32_t _generic_nes[16] = {
        0,                  // GENERIC_OTHER   0x8000
        0,                  // GENERIC_FIRE_X  0x4000  // RETCON
        0,                  // GENERIC_FIRE_Y  0x2000
        0,                  // GENERIC_FIRE_Z  0x1000

        event_joypad1_a_,      //GENERIC_FIRE_A  0x0800
        event_joypad1_b_,      //GENERIC_FIRE_B  0x0400
        0,                      //GENERIC_FIRE_C  0x0200
        0,                      //GENERIC_RESET   0x0100     // ATARI FLASHBACK

        event_joypad1_start_,   //GENERIC_START   0x0080
        event_joypad1_select_,  //GENERIC_SELECT  0x0040
        event_joypad1_a_,      //GENERIC_FIRE    0x0020
        event_joypad1_right_,  //GENERIC_RIGHT   0x0010

        event_joypad1_left_,   //GENERIC_LEFT    0x0008
        event_joypad1_down_,   //GENERIC_DOWN    0x0004
        event_joypad1_up_,      //GENERIC_UP      0x0002
        0,                      //GENERIC_MENU    0x0001
    };

    // raw HID data. handle WII/IR mappings
    virtual void hid(const uint8_t* d, int len)
    {
        if (d[0] != 0x30 && d[0] != 0x32 && d[0] != 0x42)
            return;
        bool ir = *d++ == 0x42;

        for (int i = 0; i < 2; i++) {
            uint32_t p;
            if (ir) {
                int m = d[0] + (d[1] << 8);
                p = generic_map(m,_generic_nes);
                d += 2;
            } else
                p = wii_map(i,_common_nes,_classic_nes);

            const int* m = i ? _nes_2 : _nes_1;
            for (int e = 0; m[e]; e++)
            {
                pad((p & 1),m[e]);
                p >>= 1;
            }
        }
    }

    /*
     Return - Joypad 1 Start
     Tab - Joypad 1 Select
     */
    virtual void key(int keycode, int pressed, int mods)
    {
        switch (keycode) {
            case 82: pad(pressed,event_joypad1_up); break;
            case 81: pad(pressed,event_joypad1_down); break;
            case 80: pad(pressed,event_joypad1_left); break;
            case 79: pad(pressed,event_joypad1_right); break;

            case 21: pad(pressed,event_soft_reset); break; // soft reset - 'r'
            case 23: pad(pressed,event_hard_reset); break; // hard reset - 't'

            case 61: pad(pressed,event_joypad1_start); break; // F4
            case 62: pad(pressed,((KEY_MOD_LSHIFT|KEY_MOD_RSHIFT) & mods) ? event_hard_reset : event_soft_reset); break; // F5

            case 40: pad(pressed,event_joypad1_start); break; // return
            case 43: pad(pressed,event_joypad1_select); break; // tab

            case 225: pad(pressed,event_joypad1_a); break; // left shift key event_joypad1_a
            case 226: pad(pressed,event_joypad1_b); break; // option key event_joypad1_b

            //case 59: system(pressed,INPUT_PAUSE); break; // F2
            //case 61: system(pressed,INPUT_START); break; // F4
            //case 62: system(pressed,((KEY_MOD_LSHIFT|KEY_MOD_RSHIFT) & mods) ? INPUT_HARD_RESET : INPUT_SOFT_RESET); break; // F5
        }
    }

    virtual int insert(const std::string& path, int flags, int disk_index)
    {
        loading_progress_set(8);
        // Shut down before remapping the single CrapFS ROM cache.
        nes_emulate_shutdown();
#if NES_AUDIO_QUEUE_FEEDBACK
        _audio_queue_feedback_accum = 0;
#endif
        _lines = 0;
        unmap_file(_nofrendo_rom);
        _nofrendo_rom = 0;
        printf("nofrendo inserting %s\n",path.c_str());

        uint8_t h[16] = {0};
        int len = head(path,h,sizeof(h));
        if (len < 0) {
            printf("nofrendo can't open %s\n",path.c_str());
            return -1;
        }
        loading_progress_set(12);

        printf("nofrendo %s is %d bytes\n",path.c_str(),len);
        if (!valid_nes_source_header(h, len) && is_builtin_default_rom_path(path)) {
            if (rebuild_builtin_default_rom(path)) {
                memset(h, 0, sizeof(h));
                len = head(path,h,sizeof(h));
                if (len >= 0)
                    printf("nofrendo retry %s is %d bytes\n",path.c_str(),len);
            }
        }

        if (!valid_nes_source_header(h, len)) {
            printf("nofrendo invalid source header for %s (%02X %02X %02X %02X), len=%d\n",
                path.c_str(),
                h[0], h[1], h[2], h[3], len);
            gui_msg("Invalid NES ROM on SD");
            _lines = nes_emulate_boot_buffer(width, height);
            return -1;
        }
        _nofrendo_rom = map_file(path.c_str(),len);
        if (!_nofrendo_rom) {
            printf("nofrendo can't map %s\n",path.c_str());
            _lines = nes_emulate_boot_buffer(width, height);
            return -1;
        }
        loading_progress_set(82);

        if (nes_emulate_init(path.c_str(),width,height) != 0) {
            printf("nofrendo init failed for %s\n", path.c_str());
            unmap_file(_nofrendo_rom);
            _nofrendo_rom = 0;
            _lines = nes_emulate_boot_buffer(width, height);
            return -1;
        }
        loading_progress_set(92);
        _lines = nes_emulate_frame(true);   // first frame!
        loading_progress_set(100);
        _skip_warmup = 30;
#if NES_AUDIO_QUEUE_FEEDBACK
        _audio_queue_feedback_accum = 0;
#endif
        printf("nofrendo inserted %s\n", path.c_str());
        return 0;
    }

    virtual int update()
    {
        int frames = 0;
        if (_nofrendo_rom) {
            if (_visualizer_active) {
                // The visualizer owns scanout, so the last NES frame stays in place.
            } else {
                bool draw = true;

                // Frame dropping starts after the startup frames.
                if (_skip_warmup > 0) {
                    _skip_warmup--;
                }
#if NES_ADAPTIVE_FRAME_SKIP
                else if (standard == NTSC) {
                    // 2.4M cycles is about 10 ms at 240 MHz.
                    if (_frame_time > 2400000)
                        draw = false;
                }
#endif

                _lines = nes_emulate_frame(draw);
                frames = 1;
            }
        }
        return frames;
    }

    virtual uint8_t** video_buffer()
    {
        if (!_lines)
            _lines = nes_emulate_boot_buffer(width, height);
        return _lines;
    }

    virtual int audio_buffer(int16_t* b, int len)
    {
        const int max_samples = len / (int)sizeof(int16_t);
        int n = frame_sample_count();
        if (n > max_samples)
            n = max_samples;
        if (n < 0)
            n = 0;
        int correction = audio_queue_feedback_samples(n);
        if (correction != 0) {
            n += correction;
            if (n > max_samples)
                n = max_samples;
            if (n < 0)
                n = 0;
        }
        if (nes_sound_cb) {
            nes_sound_cb(b,n);
            for (int i = 0; i < n; i++) {
                int sample = b[i];
                sample = (sample * NES_AUDIO_OUTPUT_GAIN_NUM) / NES_AUDIO_OUTPUT_GAIN_DEN;
                if (sample > 32767)
                    sample = 32767;
                else if (sample < -32768)
                    sample = -32768;
                b[i] = (int16_t)sample;
            }
        }
        else
            memset(b,0,2*n);
        return n;
    }

    virtual const uint32_t* ntsc_palette() { return cc_width == 3 ? nes_3_phase : nes_4_phase; };
    virtual const uint32_t* pal_palette() { return _nes_yuv_4_phase_pal; };
    virtual const uint32_t* rgb_palette() { return nes_pal; };

    virtual int make_default_media(const string& path)
    {
#ifdef _WIN32
        _mkdir(path.c_str());
#else
        mkdir(path.c_str(), 0755);
#endif
        remove((path + "/sokoban.nes").c_str());
        remove((path + "/chase.nes").c_str());
        remove((path + "/tokumaru_raycast.nes").c_str());
        remove((path + "/sliding_blaster.nes").c_str());
        unpack((path + "/Sliding Blaster.nes").c_str(),sliding_blaster_nes,sizeof(sliding_blaster_nes));
        return 0;
    }
};

Emu* NewNofrendo(int ntsc)
{
    return new EmuNofrendo(ntsc);
}
