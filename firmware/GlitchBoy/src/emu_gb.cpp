#if defined(__GNUC__)
#pragma GCC optimize("O3")
#endif

#include "emu.h"
#include "gb_diag_config.h"
#include "gb_apu_midi.h"
#include "midi_input.h"
#include "media.h"

#include <Arduino.h>
#include <algorithm>
#include <cstring>
#include <sys/stat.h>
#include <time.h>
#ifdef ESP_PLATFORM
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_heap_caps.h>
#include <rom/ets_sys.h>
#endif

#ifndef WALNUTGB_USE_DUALFETCH
#define WALNUTGB_USE_DUALFETCH 1
#endif

#ifndef WALNUTGB_USE_THREADED_CPU
#define WALNUTGB_USE_THREADED_CPU 1
#endif

#ifndef WALNUTGB_THREADED_LAZY_POST
#define WALNUTGB_THREADED_LAZY_POST 1
#endif

#ifndef WALNUTGB_USE_HYBRID_ROM
#define WALNUTGB_USE_HYBRID_ROM 1
#endif

#ifndef WALNUTGB_ENABLE_16_BIT_OPS
#define WALNUTGB_ENABLE_16_BIT_OPS 0
#endif

#ifndef WALNUTGB_ENABLE_GB_GLITCH
#define WALNUTGB_ENABLE_GB_GLITCH 1
#endif

#ifndef WALNUTGB_DOUBLE_BUFFER
#define WALNUTGB_DOUBLE_BUFFER 0
#endif

extern "C" {
#define ENABLE_SOUND 1
#define ENABLE_LCD 1
#define WALNUT_GB_12_COLOUR 1
#define WALNUT_FULL_GBC_SUPPORT 1
#define WALNUT_GB_HIGH_LCD_ACCURACY 0
#define WALNUT_GB_FAST_STAT_POLL WALNUTGB_FAST_STAT_POLL
#define WALNUT_GB_FAST_STAT_POLL_DIRECT_READS WALNUTGB_FAST_STAT_POLL_DIRECT_READS
#define WALNUT_GB_FAST_POLL_WAIT WALNUTGB_FAST_POLL_WAIT
#define WALNUT_GB_DIRTY_FRAME_SKIP WALNUTGB_DIRTY_FRAME_SKIP
#define WALNUT_GB_16_BIT_OPS WALNUTGB_ENABLE_16_BIT_OPS
#define WALNUT_GB_ENABLE_GLITCH WALNUTGB_ENABLE_GB_GLITCH
#define WALNUT_GB_THREADED_CPU WALNUTGB_USE_THREADED_CPU
#define WALNUT_GB_THREADED_LAZY_POST WALNUTGB_THREADED_LAZY_POST
#define MINIGB_APU_AUDIO_FORMAT_S16SYS
uint8_t audio_read(uint16_t addr);
void audio_write(uint16_t addr, uint8_t val);
#include "walnut/minigb_apu.h"
#include "walnut/walnut_cgb.h"
void walnutgb_audio_bind(struct minigb_apu_ctx *ctx);
}

using namespace std;

extern bool video_set_gb_palette_entry(uint8_t index, uint32_t rgb888);
extern bool video_reset_gb_base_palette(const uint32_t* palette);

class EmuGameBoy;
static EmuGameBoy* g_active_gb_emu = nullptr;

#ifndef WALNUTGB_AUDIO_QUEUE_FEEDBACK
#define WALNUTGB_AUDIO_QUEUE_FEEDBACK 1
#endif

#ifndef WALNUTGB_AUDIO_QUEUE_TARGET
#define WALNUTGB_AUDIO_QUEUE_TARGET 3840u
#endif

#ifndef WALNUTGB_AUDIO_QUEUE_TARGET_NTSC
#define WALNUTGB_AUDIO_QUEUE_TARGET_NTSC 3840u
#endif

#ifndef WALNUTGB_AUDIO_QUEUE_TARGET_MIDI
#define WALNUTGB_AUDIO_QUEUE_TARGET_MIDI MUSIC_AUDIO_QUEUE_TARGET_SAMPLES
#endif

#ifndef WALNUTGB_AUDIO_QUEUE_DEADBAND
#define WALNUTGB_AUDIO_QUEUE_DEADBAND 128u
#endif

#ifndef WALNUTGB_AUDIO_QUEUE_DEADBAND_NTSC
#define WALNUTGB_AUDIO_QUEUE_DEADBAND_NTSC 128u
#endif

#ifndef WALNUTGB_AUDIO_QUEUE_DEADBAND_MIDI
#define WALNUTGB_AUDIO_QUEUE_DEADBAND_MIDI MUSIC_AUDIO_QUEUE_DEADBAND_SAMPLES
#endif

#ifndef WALNUTGB_AUDIO_QUEUE_MAX_CORRECTION
#define WALNUTGB_AUDIO_QUEUE_MAX_CORRECTION 1536
#endif

#ifndef WALNUTGB_AUDIO_QUEUE_MAX_CORRECTION_NTSC
#define WALNUTGB_AUDIO_QUEUE_MAX_CORRECTION_NTSC 1536
#endif

#ifndef WALNUTGB_AUDIO_QUEUE_FILL_DIV
#define WALNUTGB_AUDIO_QUEUE_FILL_DIV 2u
#endif

#ifndef WALNUTGB_AUDIO_QUEUE_DRAIN_DIV
#define WALNUTGB_AUDIO_QUEUE_DRAIN_DIV 16u
#endif

#ifndef WALNUTGB_AUDIO_RING_SIZE
#define WALNUTGB_AUDIO_RING_SIZE 4096u
#endif

#ifndef WALNUTGB_AUDIO_RING_TARGET
#define WALNUTGB_AUDIO_RING_TARGET 3072u
#endif

#ifndef WALNUTGB_AUDIO_RING_DEADBAND
#define WALNUTGB_AUDIO_RING_DEADBAND 384u
#endif

#ifndef WALNUTGB_AUDIO_RING_MAX_STEP_CORRECTION_Q16
#define WALNUTGB_AUDIO_RING_MAX_STEP_CORRECTION_Q16 768u
#endif

#ifndef WALNUTGB_AUDIO_RING_MAX_DRAIN_STEP_CORRECTION_Q16
#define WALNUTGB_AUDIO_RING_MAX_DRAIN_STEP_CORRECTION_Q16 1536u
#endif

#ifndef WALNUTGB_AUDIO_RING_DRAIN_EMERGENCY_GAIN
#define WALNUTGB_AUDIO_RING_DRAIN_EMERGENCY_GAIN 2u
#endif

#ifndef WALNUTGB_AUDIO_RING_MAX_SLOW_STEP_CORRECTION_Q16
#define WALNUTGB_AUDIO_RING_MAX_SLOW_STEP_CORRECTION_Q16 24576u
#endif

#ifndef WALNUTGB_AUDIO_RING_MAX_REFILL_STEP_CORRECTION_Q16
#define WALNUTGB_AUDIO_RING_MAX_REFILL_STEP_CORRECTION_Q16 8192u
#endif

#ifndef WALNUTGB_AUDIO_RING_SLOW_RESPONSE_SAMPLES
#define WALNUTGB_AUDIO_RING_SLOW_RESPONSE_SAMPLES 1024u
#endif

#ifndef WALNUTGB_AUDIO_SPEED_MIN_Q16
#define WALNUTGB_AUDIO_SPEED_MIN_Q16 49152u
#endif

#ifndef WALNUTGB_AUDIO_SPEED_SLOWDOWN_SHIFT
#define WALNUTGB_AUDIO_SPEED_SLOWDOWN_SHIFT 5u
#endif

#ifndef WALNUTGB_AUDIO_SPEED_RECOVERY_SHIFT
#define WALNUTGB_AUDIO_SPEED_RECOVERY_SHIFT 6u
#endif

#ifndef WALNUTGB_AUDIO_SOURCE_UNDERRUN_FADE_STEP
#define WALNUTGB_AUDIO_SOURCE_UNDERRUN_FADE_STEP 512
#endif

#ifndef WALNUTGB_NTSC_SOURCE_ANTIALIAS
#define WALNUTGB_NTSC_SOURCE_ANTIALIAS 1
#endif

#ifndef WALNUTGB_AUDIO_PITCH_COMPENSATION
#define WALNUTGB_AUDIO_PITCH_COMPENSATION 1
#endif

#ifndef WALNUTGB_AUDIO_PITCH_COMP_MIN_Q16
#define WALNUTGB_AUDIO_PITCH_COMP_MIN_Q16 49152u
#endif

#ifndef WALNUTGB_AUDIO_PITCH_COMP_MAX_Q16
#define WALNUTGB_AUDIO_PITCH_COMP_MAX_Q16 98304u
#endif

#ifndef WALNUTGB_AUDIO_PITCH_COMP_STEP_DEADBAND_Q16
#define WALNUTGB_AUDIO_PITCH_COMP_STEP_DEADBAND_Q16 0
#endif

#ifndef WALNUTGB_AUDIO_PITCH_TRANSITION_TRIM
#define WALNUTGB_AUDIO_PITCH_TRANSITION_TRIM 0
#endif

#ifndef WALNUTGB_AUDIO_PITCH_TRANSITION_THRESHOLD_Q16
#define WALNUTGB_AUDIO_PITCH_TRANSITION_THRESHOLD_Q16 2048u
#endif

#ifndef WALNUTGB_AUDIO_PITCH_TRANSITION_RING_TARGET
#define WALNUTGB_AUDIO_PITCH_TRANSITION_RING_TARGET 4096u
#endif

#ifndef WALNUTGB_AUDIO_PITCH_COMP_ATTACK_SHIFT
#define WALNUTGB_AUDIO_PITCH_COMP_ATTACK_SHIFT 0u
#endif

#ifndef WALNUTGB_AUDIO_PITCH_COMP_RELEASE_SHIFT
#define WALNUTGB_AUDIO_PITCH_COMP_RELEASE_SHIFT 0u
#endif

#ifndef WALNUTGB_AUDIO_RING_CORRECTION_ATTACK_SHIFT
#define WALNUTGB_AUDIO_RING_CORRECTION_ATTACK_SHIFT 4u
#endif

#ifndef WALNUTGB_AUDIO_RING_CORRECTION_RELEASE_SHIFT
#define WALNUTGB_AUDIO_RING_CORRECTION_RELEASE_SHIFT 6u
#endif

#ifndef WALNUTGB_AUDIO_RING_CORRECTION_DRAIN_SHIFT
#define WALNUTGB_AUDIO_RING_CORRECTION_DRAIN_SHIFT 4u
#endif

#ifndef WALNUTGB_AUDIO_RING_STEP_DIV
#define WALNUTGB_AUDIO_RING_STEP_DIV 2u
#endif

#ifndef WALNUTGB_AUDIO_MAX_FRAME_DELTA
#define WALNUTGB_AUDIO_MAX_FRAME_DELTA 8u
#endif

#ifndef WALNUTGB_AUDIO_PREFILL_FRAMES
#define WALNUTGB_AUDIO_PREFILL_FRAMES 2u
#endif

#ifndef WALNUTGB_AUDIO_PENDING_FRAME_CAP
#define WALNUTGB_AUDIO_PENDING_FRAME_CAP 32u
#endif

#ifndef WALNUTGB_AUDIO_RECOVERY_SKIP
#define WALNUTGB_AUDIO_RECOVERY_SKIP 0
#endif

#ifndef WALNUTGB_AUDIO_RECOVERY_COOLDOWN_FRAMES
#define WALNUTGB_AUDIO_RECOVERY_COOLDOWN_FRAMES 2u
#endif

#ifndef WALNUTGB_AUDIO_RECOVERY_RING_LOW_SAMPLES
#define WALNUTGB_AUDIO_RECOVERY_RING_LOW_SAMPLES (AUDIO_SAMPLES * 3u)
#endif

#ifndef WALNUTGB_HOST_LOCK_TIMING
#define WALNUTGB_HOST_LOCK_TIMING 0
#endif

#ifndef WALNUTGB_HOST_LOCK_MAX_FRAME_RATE
#define WALNUTGB_HOST_LOCK_MAX_FRAME_RATE 60.0
#endif

#ifndef WALNUTGB_DRAW_PERIOD_MIN
#define WALNUTGB_DRAW_PERIOD_MIN 1u
#endif

#ifndef WALNUTGB_INITIAL_DRAW_PERIOD
#define WALNUTGB_INITIAL_DRAW_PERIOD 1u
#endif

#ifndef WALNUTGB_FULL_DRAW_UP_STREAK
#define WALNUTGB_FULL_DRAW_UP_STREAK 5u
#endif

#ifndef WALNUTGB_FULL_DRAW_OVER_BUDGET_PERCENT
#define WALNUTGB_FULL_DRAW_OVER_BUDGET_PERCENT 98u
#endif

#ifndef WALNUTGB_FULL_DRAW_SEVERE_PERCENT
#define WALNUTGB_FULL_DRAW_SEVERE_PERCENT 105u
#endif

#ifndef WALNUTGB_DRAW_PERIOD_SEVERE_UP_STREAK
#define WALNUTGB_DRAW_PERIOD_SEVERE_UP_STREAK 8u
#endif

#ifndef WALNUTGB_DRAW_PERIOD_3_UP_STREAK
#define WALNUTGB_DRAW_PERIOD_3_UP_STREAK 8u
#endif

#ifndef WALNUTGB_DRAW_PERIOD_3_ENTER_FPS_Q100
#define WALNUTGB_DRAW_PERIOD_3_ENTER_FPS_Q100 2500u
#endif

#ifndef WALNUTGB_DRAW_PERIOD_3_EXIT_PERCENT
#define WALNUTGB_DRAW_PERIOD_3_EXIT_PERCENT 100u
#endif

#ifndef WALNUTGB_DRAW_PERIOD_3_DOWN_STREAK
#define WALNUTGB_DRAW_PERIOD_3_DOWN_STREAK 8u
#endif

#ifndef WALNUTGB_DRAW_PERIOD_DOWN_STREAK
#define WALNUTGB_DRAW_PERIOD_DOWN_STREAK 18u
#endif

#ifndef WALNUTGB_FULL_DRAW_DOWN_STREAK
#define WALNUTGB_FULL_DRAW_DOWN_STREAK 60u
#endif

#ifndef WALNUTGB_OVER_BUDGET_PERCENT
#define WALNUTGB_OVER_BUDGET_PERCENT 105u
#endif

#ifndef WALNUTGB_SEVERE_OVER_BUDGET_PERCENT
#define WALNUTGB_SEVERE_OVER_BUDGET_PERCENT 115u
#endif

#ifndef WALNUTGB_UNDER_BUDGET_PERCENT
#define WALNUTGB_UNDER_BUDGET_PERCENT 92u
#endif

namespace {

#if defined(__GNUC__)
#define WALNUTGB_ALWAYS_INLINE inline __attribute__((always_inline))
#define WALNUTGB_NOINLINE __attribute__((noinline))
#else
#define WALNUTGB_ALWAYS_INLINE inline
#define WALNUTGB_NOINLINE
#endif

constexpr double kGbFrameRate = 4194304.0 / 70224.0;
constexpr double kHostPalFrameRate = 4433618.75 / (284.0 * 312.0);
constexpr double kHostNtscFrameRate = VIDEO_NTSC_FRAME_RATE_HZ;
constexpr int kHostPalAudioRate = (int)((kHostPalFrameRate * 312.0) + 0.5);
constexpr int kGbWidth = LCD_WIDTH;
constexpr int kGbHeight = LCD_HEIGHT;
constexpr uint8_t kGbCgbPaletteBase = 0x80;
constexpr size_t kGbFixedRomBankSize = 0x4000;
constexpr size_t kGbRomCachePageSize = 0x1000;
constexpr int kGbMinRomCacheSlots = 2;
constexpr int kGbMaxRomCacheSlots = 4;
constexpr size_t kGbMappedRomReserve = 0x10000;
constexpr size_t kGbSaveRamWindowSize = 0x1000;
constexpr size_t kGbSaveRamMinWindowSize = 0x0200;
static_assert((kGbSaveRamWindowSize & (kGbSaveRamWindowSize - 1u)) == 0,
              "GB save RAM window must be a power of two");
static_assert((kGbSaveRamMinWindowSize &
               (kGbSaveRamMinWindowSize - 1u)) == 0,
              "GB minimum save RAM window must be a power of two");
constexpr size_t kGbSaveRamFullLoadMax = 0x8000;
constexpr size_t kGbSaveRamPriorityFullLoadMax = 0x2000;
constexpr int kGbSaveRamCacheMaxSlots = 4;
constexpr uint32_t kQ16One = 1u << 16;
constexpr size_t kAudioRingSize = WALNUTGB_AUDIO_RING_SIZE;
static_assert((kAudioRingSize & (kAudioRingSize - 1u)) == 0,
              "WALNUTGB_AUDIO_RING_SIZE must be a power of two");
constexpr uint32_t kSaveFlushMs = 15000;
constexpr uint32_t kSaveErrorRetryMs = 30000;

static uint32_t q16_from_double(double value)
{
    if (value <= 0.0)
        return 0;
    return (uint32_t)((value * (double)kQ16One) + 0.5);
}

static int q16_samples_per_frame(uint32_t sample_rate, double frame_rate)
{
    if (sample_rate == 0 || frame_rate <= 0.0)
        return 0;
    return (int)(((double)sample_rate * (double)kQ16One / frame_rate) + 0.5);
}

static uint32_t frame_period_us_for_rate(double frame_rate)
{
    if (frame_rate <= 0.0)
        return 20000u;
    return (uint32_t)((1000000.0 / frame_rate) + 0.5);
}

struct RtcSnapshot {
    uint64_t unix_time;
    uint8_t rtc[5];
};

uint32_t g_rgb332_palette[256];
bool g_rgb332_palette_init = false;
uint8_t* g_gb_frame_pixels[2] = {0, 0};
uint8_t* g_gb_frame_lines[2][kGbHeight];
bool g_gb_framebuffers_initialized = false;

const char* kGbExt[] = {"gb", "gbc", 0};
const char* kGbHelp[] = {
    "Keyboard:",
    "  Arrow Keys - D-Pad",
    "  Left Shift - Button A",
    "  Option     - Button B",
    "  Return     - Start",
    "  Tab        - Select",
    "  R / T      - Reset",
    "",
    "Controller:",
    "  D-Pad      - D-Pad",
    "  A,1        - Button A",
    "  B,2        - Button B",
    "  +          - Start",
    "  -          - Select",
    0
};

static uint8_t* gb_alloc_frame_pixels(size_t bytes)
{
#ifdef ESP_PLATFORM
    uint8_t* p = (uint8_t*)heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!p) {
        printf("gb frame buffer alloc failed: %u bytes heap=%u max=%u\n",
               (unsigned)bytes,
               (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
               (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
    return p;
#else
    return (uint8_t*)malloc(bytes);
#endif
}

static uint32_t rgb565_to_corrected_rgb888(uint16_t rgb)
{
    // CGB palettes use neutral RGB565 expansion; composite encoding adds color.
    const uint8_t r = (uint8_t)(((((int)rgb >> 11) & 0x1F) * 255 + 15) / 31);
    const uint8_t g = (uint8_t)(((((int)rgb >> 5) & 0x3F) * 255 + 31) / 63);
    const uint8_t b = (uint8_t)((((int)rgb & 0x1F) * 255 + 15) / 31);
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static const char* gb_init_error_name(enum gb_init_error_e error)
{
    switch (error) {
        case GB_INIT_NO_ERROR: return "none";
        case GB_INIT_CARTRIDGE_UNSUPPORTED: return "cartridge unsupported";
        case GB_INIT_INVALID_CHECKSUM: return "invalid header checksum";
        default: return "unknown";
    }
}

static const char* gb_cart_type_name(uint8_t type)
{
    switch (type) {
        case 0x00: return "ROM ONLY";
        case 0x01: return "MBC1";
        case 0x02: return "MBC1+RAM";
        case 0x03: return "MBC1+RAM+BATTERY";
        case 0x05: return "MBC2";
        case 0x06: return "MBC2+BATTERY";
        case 0x08: return "ROM+RAM";
        case 0x09: return "ROM+RAM+BATTERY";
        case 0x0B: return "MMM01";
        case 0x0C: return "MMM01+RAM";
        case 0x0D: return "MMM01+RAM+BATTERY";
        case 0x0F: return "MBC3+TIMER+BATTERY";
        case 0x10: return "MBC3+TIMER+RAM+BATTERY";
        case 0x11: return "MBC3";
        case 0x12: return "MBC3+RAM";
        case 0x13: return "MBC3+RAM+BATTERY";
        case 0x19: return "MBC5";
        case 0x1A: return "MBC5+RAM";
        case 0x1B: return "MBC5+RAM+BATTERY";
        case 0x1C: return "MBC5+RUMBLE";
        case 0x1D: return "MBC5+RUMBLE+RAM";
        case 0x1E: return "MBC5+RUMBLE+RAM+BATTERY";
        case 0x20: return "MBC6";
        case 0x22: return "MBC7+SENSOR+RUMBLE+RAM+BATTERY";
        case 0xFC: return "POCKET CAMERA";
        case 0xFD: return "BANDAI TAMA5";
        case 0xFE: return "HuC3";
        case 0xFF: return "HuC1+RAM+BATTERY";
        default: return "unknown";
    }
}

static void init_rgb332_palette()
{
    if (g_rgb332_palette_init)
        return;

    static const uint8_t kLevel8[8] = {0x00, 0x24, 0x48, 0x6C, 0x90, 0xB4, 0xD8, 0xFF};
    static const uint8_t kLevel4[4] = {0x00, 0x48, 0xB4, 0xFF};
    for (int i = 0; i < 256; ++i) {
        const uint8_t r = kLevel8[(i >> 5) & 0x07];
        const uint8_t g = kLevel8[(i >> 2) & 0x07];
        const uint8_t b = kLevel4[i & 0x03];
        const uint32_t rgb = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        g_rgb332_palette[i] = rgb;
    }
    g_rgb332_palette_init = true;
}

static string replace_ext(const string& path, const char* ext)
{
    size_t dot = path.find_last_of('.');
    if (dot == string::npos || dot <= path.find_last_of("/\\"))
        return path + ext;
    return path.substr(0, dot) + ext;
}

static string dec_string(int value)
{
    char buf[32];
    sprintf(buf, "%d", value);
    return buf;
}

static bool read_binary_file(const string& path, uint8_t* dst, size_t len)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (!f)
        return false;
    size_t got = fread(dst, 1, len, f);
    fclose(f);
    return got == len;
}

static bool read_binary_file_range(const string& path, size_t offset, uint8_t* dst, size_t len)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (!f)
        return false;
    bool ok = fseek(f, (long)offset, SEEK_SET) == 0 &&
              fread(dst, 1, len, f) == len;
    fclose(f);
    return ok;
}

static bool ensure_file_size(FILE* f, size_t len, uint8_t fill)
{
    if (!f)
        return false;
    if (fseek(f, 0, SEEK_END) != 0)
        return false;
    long pos = ftell(f);
    if (pos < 0)
        return false;
    size_t cur = (size_t)pos;
    if (cur >= len)
        return true;

    uint8_t buf[256];
    memset(buf, fill, sizeof(buf));
    while (cur < len) {
        size_t n = len - cur;
        if (n > sizeof(buf))
            n = sizeof(buf);
        if (fwrite(buf, 1, n, f) != n)
            return false;
        cur += n;
    }
    return true;
}

static bool write_file_bytes(FILE* f, const uint8_t* src, size_t len)
{
    size_t done = 0;
    while (done < len) {
        size_t chunk = len - done;
        if (chunk > 512)
            chunk = 512;
        if (fwrite(src + done, 1, chunk, f) != chunk)
            return false;
        done += chunk;
    }
    return true;
}

// Saves are rewritten in place: no truncation, so a power cut mid-write can't
// leave an empty .sav and same-size rewrites don't touch the FAT chain. A file
// that exists but can't be opened right now is left alone (caller retries)
// instead of being truncated by "w+b".
static FILE* open_save_for_update(const string& path)
{
    FILE* f = fopen(path.c_str(), "r+b");
    if (f)
        return f;
    struct stat st;
    if (stat(path.c_str(), &st) == 0)
        return 0;
    return fopen(path.c_str(), "w+b");
}

static bool write_binary_file_range(const string& path, size_t offset, const uint8_t* src, size_t len, size_t total_len, uint8_t fill)
{
    FILE* f = open_save_for_update(path);
    if (!f)
        return false;
    bool ok = ensure_file_size(f, total_len, fill) &&
              fseek(f, (long)offset, SEEK_SET) == 0 &&
              write_file_bytes(f, src, len) &&
              fflush(f) == 0;
    fclose(f);
    return ok;
}

static bool write_binary_file(const string& path, const void* src, size_t len)
{
    FILE* f = open_save_for_update(path);
    if (!f)
        return false;
    bool ok = fseek(f, 0, SEEK_SET) == 0 &&
              write_file_bytes(f, (const uint8_t*)src, len) &&
              fflush(f) == 0;
    fclose(f);
    return ok;
}

static size_t read_stream_fully(FILE* f, void* dst, size_t len)
{
    uint8_t* out = (uint8_t*)dst;
    size_t total = 0;
    while (total < len) {
        size_t got = fread(out + total, 1, len - total, f);
        if (got == 0)
            break;
        total += got;
    }
    return total;
}

static void fill_local_time(struct tm* out_tm)
{
    if (!out_tm)
        return;
#ifdef ESP_PLATFORM
    const uint64_t seconds = (uint64_t)(millis() / 1000u);
    memset(out_tm, 0, sizeof(*out_tm));
    out_tm->tm_sec = (int)(seconds % 60ull);
    out_tm->tm_min = (int)((seconds / 60ull) % 60ull);
    out_tm->tm_hour = (int)((seconds / 3600ull) % 24ull);
    out_tm->tm_yday = (int)((seconds / 86400ull) & 0x1FF);
#else
    time_t now = time(0);
#ifdef _WIN32
    localtime_s(out_tm, &now);
#else
    localtime_r(&now, out_tm);
#endif
#endif
}

static uint64_t unix_time_now()
{
#ifdef ESP_PLATFORM
    return (uint64_t)(millis() / 1000u);
#else
    return (uint64_t)time(0);
#endif
}

static size_t gb_mapped_rom_capacity()
{
#ifdef ESP_PLATFORM
    static bool inited = false;
    static size_t capacity = 0;
    if (inited)
        return capacity;
    inited = true;

    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* preferred = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, "app1");
    const esp_partition_t* part = 0;
    if (preferred && (!running || preferred->address != running->address))
        part = preferred;
    if (!part) {
        esp_partition_iterator_t it = esp_partition_find(
            ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
        while (it) {
            const esp_partition_t* candidate = esp_partition_get(it);
            if (!running || candidate->address != running->address) {
                part = candidate;
                break;
            }
            it = esp_partition_next(it);
        }
        if (it)
            esp_partition_iterator_release(it);
    }

    if (part && part->size > kGbMappedRomReserve)
        capacity = part->size - kGbMappedRomReserve;
    return capacity;
#else
    return (size_t)-1;
#endif
}

static void rtc_advance(union cart_rtc* rtc, uint64_t seconds)
{
    if (!rtc || (rtc->reg.high & 0x40))
        return;

    uint64_t days = (uint64_t)rtc->reg.yday | (uint64_t)((rtc->reg.high & 1) << 8);
    uint64_t total = (days * 24ull * 60ull * 60ull) +
                     ((uint64_t)rtc->reg.hour * 60ull * 60ull) +
                     ((uint64_t)rtc->reg.min * 60ull) +
                     (uint64_t)rtc->reg.sec +
                     seconds;

    uint8_t halt_bit = rtc->reg.high & 0x40;
    uint8_t overflow_bit = rtc->reg.high & 0x80;
    days = total / 86400ull;
    if (days > 0x1FF) {
        overflow_bit = 0x80;
        days &= 0x1FF;
    }

    total %= 86400ull;
    rtc->reg.hour = (uint8_t)(total / 3600ull);
    total %= 3600ull;
    rtc->reg.min = (uint8_t)(total / 60ull);
    rtc->reg.sec = (uint8_t)(total % 60ull);
    rtc->reg.yday = (uint8_t)(days & 0xFF);
    rtc->reg.high = (uint8_t)(halt_bit | overflow_bit | ((days >> 8) & 1));
}

// DMG shades are sent to the video encoder as exact colours in palette slots
// 0x80-0x83. Going through the RGB332 table (truncated, 4 blue levels) turned
// the lightest shade into pure white and the darkest into pure black. In DMG
// mode those slots are free (CGB uses 0x80-0xBF only in colour games) and the
// menu (0x00/0xFF) and visualizer palettes never use them.
static const uint8_t kDmgShadeIndexBase = 0x80;
static const uint32_t kDmgShadeRgb[4] = {
    0xE0F8D0, 0x88C070, 0x346856, 0x081820,
};
static const uint8_t kDefaultDmgPalette[12] = {
    kDmgShadeIndexBase + 0, kDmgShadeIndexBase + 1,
    kDmgShadeIndexBase + 2, kDmgShadeIndexBase + 3,
    kDmgShadeIndexBase + 0, kDmgShadeIndexBase + 1,
    kDmgShadeIndexBase + 2, kDmgShadeIndexBase + 3,
    kDmgShadeIndexBase + 0, kDmgShadeIndexBase + 1,
    kDmgShadeIndexBase + 2, kDmgShadeIndexBase + 3,
};

static uint16_t cgb_raw_palette_entry_to_rgb565(const uint8_t* palette, int entry)
{
    const int offset = entry << 1;
    const uint16_t raw = (uint16_t)(((uint16_t)palette[offset + 1] << 8) |
                                    palette[offset]);
#if WALNUT_GB_RGB565_BIGENDIAN
    return bgr555_to_rgb565BE_accurate(raw);
#else
    return bgr555_to_rgb565_accurate(raw);
#endif
}

static uint16_t cgb_bgr555_to_rgb565(uint16_t raw)
{
#if WALNUT_GB_RGB565_BIGENDIAN
    return bgr555_to_rgb565BE_accurate(raw);
#else
    return bgr555_to_rgb565_accurate(raw);
#endif
}

static uint16_t cgb_raw_palette_entry_to_bgr555(const uint8_t* palette, int entry)
{
    const int offset = entry << 1;
    return (uint16_t)((((uint16_t)palette[offset + 1] << 8) | palette[offset]) & 0x7FFF);
}

static uint16_t gb_palette_glitch_shift_bgr555(uint16_t raw, uint8_t phase)
{
    uint8_t r = (uint8_t)(raw & 0x1F);
    uint8_t g = (uint8_t)((raw >> 5) & 0x1F);
    uint8_t b = (uint8_t)((raw >> 10) & 0x1F);
    uint8_t nr = r;
    uint8_t ng = g;
    uint8_t nb = b;

    switch (phase % 6) {
        case 1:
            nr = g; ng = b; nb = r;
            break;
        case 2:
            nr = b; ng = r; nb = g;
            break;
        case 3:
            nr = (uint8_t)(31 - r); ng = (uint8_t)(31 - g); nb = (uint8_t)(31 - b);
            break;
        case 4:
            nr = (uint8_t)(31 - g); ng = (uint8_t)(31 - b); nb = (uint8_t)(31 - r);
            break;
        case 5:
            nr = (uint8_t)(31 - b); ng = (uint8_t)(31 - r); nb = (uint8_t)(31 - g);
            break;
        default:
            break;
    }

    return (uint16_t)(nr | (ng << 5) | (nb << 10));
}

static uint8_t gb_palette_glitch_shift_dmg_shade(uint8_t shade, uint8_t phase)
{
    shade &= 3;
    switch (phase % 6) {
        case 1:
            return (uint8_t)((shade + 1) & 3);
        case 2:
            return (uint8_t)((shade + 2) & 3);
        case 3:
            return (uint8_t)((shade + 3) & 3);
        case 4:
            return (uint8_t)(3 - shade);
        case 5:
            return (uint8_t)((4 - shade) & 3);
        default:
            return shade;
    }
}

static void rebuild_cgb_fix_palette(gb_s* gb)
{
    if (!gb)
        return;
    for (int i = 0; i < 0x20; ++i) {
        gb->cgb.fixPalette[i] =
            cgb_raw_palette_entry_to_rgb565(gb->cgb.BGPalette, i);
        gb->cgb.fixPalette[0x20 + i] =
            cgb_raw_palette_entry_to_rgb565(gb->cgb.OAMPalette, i);
    }
}

//====================================================================================================
// Idle-buffer pool
//
// The emulator is chosen once per boot, so in a GB session the NES core's
// static buffers are dead internal RAM, and the CrapFS directory buffer is
// idle between map_file() calls. ROM cache pages and save RAM come from here
// first; the heap is only used for extras while kGbHeapReserve stays free
// (a GB session with a 2 MB ROM used to leave ~600 bytes, and the next
// lazily created C-library lock aborted the firmware).
//
// Entries may overlap (the NES sprite table is offered whole, for an 8 KB
// resident save, or as two halves, for save windows); a borrow is refused if
// it overlaps anything already lent out.
constexpr size_t kGbHeapReserve = 6 * 1024;

struct GbIdleBuffer {
    uint8_t* data;
    size_t len;
    bool used;
};
static GbIdleBuffer g_gb_idle[6];
static int g_gb_idle_count = 0;

static void gb_idle_add(uint8_t* data, size_t len)
{
    if (!data || !len || g_gb_idle_count >= (int)(sizeof(g_gb_idle) / sizeof(g_gb_idle[0])))
        return;
    g_gb_idle[g_gb_idle_count++] = { data, len, false };
}

static void gb_idle_pool_init()
{
    if (g_gb_idle_count)
        return;
    size_t len = 0;
    uint8_t* p = map_file_scratch(&len);    // idle after the ROM is mapped
    gb_idle_add(p, len);
    p = nes6502_idle_scratch(&len);
    gb_idle_add(p, len);
    p = nes_wram_idle_scratch(&len);
    gb_idle_add(p, len);
    p = nes_ppu_idle_scratch(&len);
    if (p && len) {
        gb_idle_add(p, len);                        // whole (resident save)
        gb_idle_add(p, len / 2);                    // halves (save windows)
        gb_idle_add(p + len / 2, len - len / 2);
    }
}

static bool gb_idle_overlaps_used(const GbIdleBuffer& b)
{
    for (int i = 0; i < g_gb_idle_count; ++i) {
        const GbIdleBuffer& u = g_gb_idle[i];
        if (!u.used)
            continue;
        if (b.data < u.data + u.len && u.data < b.data + b.len)
            return true;
    }
    return false;
}

// Smallest free buffer with min_len <= len <= max_len, or null.
static uint8_t* gb_idle_borrow(size_t min_len, size_t max_len)
{
    gb_idle_pool_init();
    int best = -1;
    for (int i = 0; i < g_gb_idle_count; ++i) {
        const GbIdleBuffer& b = g_gb_idle[i];
        if (b.used || b.len < min_len || b.len > max_len || gb_idle_overlaps_used(b))
            continue;
        if (best < 0 || b.len < g_gb_idle[best].len)
            best = i;
    }
    if (best < 0)
        return 0;
    g_gb_idle[best].used = true;
    return g_gb_idle[best].data;
}

// Returns true if p belonged to the pool (and is now free again).
static bool gb_idle_release(uint8_t* p)
{
    if (!p)
        return false;
    for (int i = 0; i < g_gb_idle_count; ++i) {
        if (g_gb_idle[i].used && g_gb_idle[i].data == p) {
            g_gb_idle[i].used = false;
            return true;
        }
    }
    return false;
}

static void gb_buffer_free(uint8_t* p)
{
    if (!gb_idle_release(p))
        free(p);
}

static bool gb_heap_has_room_for(size_t bytes)
{
#ifdef ESP_PLATFORM
    return heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) >= bytes + kGbHeapReserve;
#else
    (void)bytes;
    return true;
#endif
}

}

class EmuGameBoy : public Emu {
    enum RomBackend {
        ROM_BACKEND_NONE = 0,
        ROM_BACKEND_MAPPED,
        ROM_BACKEND_SD_BANKED,
        ROM_BACKEND_HYBRID
    };

    struct RomCacheSlot {
        uint8_t* data;
        uint32_t page;
        uint32_t age;
        bool valid;
    };

    struct CartRamCacheSlot {
        uint8_t* data;
        uint32_t page;
        uint32_t age;
        bool valid;
        bool dirty;
    };

    gb_s _gb;
    minigb_apu_ctx _apu;
    uint8_t** _lines;
    uint8_t** _frame_lines[2];
    uint8_t* _frame_pixels[2];
    uint8_t _front_buffer;
    RomBackend _rom_backend;
    uint8_t* _rom;
    size_t _rom_len;
    size_t _rom_mapped_len;
    FILE* _rom_file;
    RomCacheSlot _rom_cache[kGbMaxRomCacheSlots];
    int _rom_cache_slots;
    bool _rom_cache_pins_page0;
    uint32_t _rom_cache_age;
    uint8_t* _cart_ram;
    size_t _cart_ram_len;
    size_t _cart_ram_window_len;
    CartRamCacheSlot _cart_ram_cache[kGbSaveRamCacheMaxSlots];
    int _cart_ram_cache_slots;
    uint32_t _cart_ram_cache_age;
    bool _cart_ram_dirty;
    bool _cart_ram_full;
    string _rom_path;
    string _save_path;
    string _rtc_path;
    bool _save_dirty;
    bool _save_error_logged;
    bool _fatal_error;
    uint16_t _cgb_palette565[0x40];
    uint32_t _cgb_palette_rgb[0x40];
    uint64_t _cgb_palette_dirty_mask;
    uint8_t _dmg_palette[12];
    uint8_t _gb_palette_glitch_enabled;
    uint8_t _gb_palette_glitch_game_active;
    uint8_t _gb_palette_glitch_phase;
    int16_t _audio_ring[kAudioRingSize];
    uint32_t _audio_r;
    uint32_t _audio_w;
    bool _resample_ready;
    int16_t _resample_prev;
    int16_t _resample_next;
    uint32_t _resample_phase_q16;
    uint32_t _resample_step_q16;
    int32_t _resample_step_correction_q16;
    int16_t _source_filter_prev;
    bool _source_filter_ready;
    uint32_t _apu_pitch_q16;
    uint32_t _emu_speed_q16;
    uint32_t _host_frame_step_q16;
    uint32_t _frame_accum_q16;
    uint32_t _ntsc_audio_fraction_q16;
    uint32_t _host_frame_delta;
    uint32_t _audio_frame_delta;
    uint32_t _core_frame_seq;
    uint32_t _timing_ema_us;
    uint32_t _timing_over_budget_streak;
    uint32_t _timing_under_budget_streak;
    uint32_t _host_frame_period_us;
    uint32_t _presented_fps_last_ms;
    uint32_t _presented_fps_frames;
    uint32_t _presented_fps_q100;
    uint8_t _presented_fps_draw_period;
    uint8_t _draw_period;
    uint8_t _line_effect_skip_streak;
    uint32_t _lcd_skip_streak;
    uint8_t _audio_recovery_cooldown;
    bool _midi_audio_active;
    uint32_t _save_deadline_ms;

    WALNUTGB_ALWAYS_INLINE static EmuGameBoy* self(gb_s* gb)
    {
        return static_cast<EmuGameBoy*>(gb->direct.priv);
    }


    WALNUTGB_ALWAYS_INLINE uint8_t draw_buffer_index() const
    {
        return (uint8_t)(_front_buffer ^ 1u);
    }

    WALNUTGB_ALWAYS_INLINE uint8_t* draw_pixels()
    {
        return _frame_pixels[draw_buffer_index()];
    }

    WALNUTGB_ALWAYS_INLINE static void copy_cgb_line(uint8_t* dst,
                                                     const uint8_t* src)
    {
        typedef uint32_t __attribute__((may_alias)) AliasU32;
        if ((((uintptr_t)dst | (uintptr_t)src) & 3u) == 0) {
            AliasU32* dst32 = reinterpret_cast<AliasU32*>(dst);
            const AliasU32* src32 = reinterpret_cast<const AliasU32*>(src);
            for (int i = 0; i < kGbWidth / 4; ++i)
                dst32[i] = src32[i] | 0x80808080u;
            return;
        }

        for (int x = 0; x < kGbWidth; ++x)
            dst[x] = (uint8_t)(src[x] | 0x80u);
    }

    bool init_screen()
    {
        if (!g_gb_framebuffers_initialized) {
            if (!g_gb_frame_pixels[0])
                g_gb_frame_pixels[0] = gb_alloc_frame_pixels(kGbWidth * kGbHeight);
            if (!g_gb_frame_pixels[0])
                return false;
#if WALNUTGB_DOUBLE_BUFFER
            if (!g_gb_frame_pixels[1])
                g_gb_frame_pixels[1] = gb_alloc_frame_pixels(kGbWidth * kGbHeight);
            if (!g_gb_frame_pixels[1])
                g_gb_frame_pixels[1] = g_gb_frame_pixels[0];
#else
            g_gb_frame_pixels[1] = g_gb_frame_pixels[0];
#endif
            for (int i = 0; i < 2; ++i) {
                for (int y = 0; y < kGbHeight; ++y)
                    g_gb_frame_lines[i][y] = g_gb_frame_pixels[i] + (y * kGbWidth);
            }
            memset(g_gb_frame_pixels[0], 0, kGbWidth * kGbHeight);
            if (g_gb_frame_pixels[1] != g_gb_frame_pixels[0])
                memset(g_gb_frame_pixels[1], 0, kGbWidth * kGbHeight);
            g_gb_framebuffers_initialized = true;
        }
        _frame_pixels[0] = g_gb_frame_pixels[0];
        _frame_pixels[1] = g_gb_frame_pixels[1];
        _frame_lines[0] = g_gb_frame_lines[0];
        _frame_lines[1] = g_gb_frame_lines[1];
        _front_buffer = 0;
        _lines = _frame_lines[_front_buffer];
        return true;
    }

    void clear_all_screens()
    {
        if (_frame_pixels[0])
            memset(_frame_pixels[0], 0, kGbWidth * kGbHeight);
        if (_frame_pixels[1] && _frame_pixels[1] != _frame_pixels[0])
            memset(_frame_pixels[1], 0, kGbWidth * kGbHeight);
    }

    void present_frame()
    {
        ++_presented_fps_frames;
        _front_buffer ^= 1u;
        _lines = _frame_lines[_front_buffer];
        if (!gui_video_needs_host_commit())
            video_queue_lines(_lines);
    }

    bool has_rom() const
    {
        switch (_rom_backend) {
            case ROM_BACKEND_MAPPED:
                return _rom != 0;
            case ROM_BACKEND_SD_BANKED:
                return _rom_file != 0;
            case ROM_BACKEND_HYBRID:
                return _rom != 0 && _rom_file != 0 && _rom_mapped_len > 0;
            case ROM_BACKEND_NONE:
            default:
                return false;
        }
    }

    void free_rom_cache_page(uint8_t* data)
    {
        gb_buffer_free(data);   // pool page or heap page
    }

    void free_rom_cache_storage()
    {
        for (int i = 0; i < kGbMaxRomCacheSlots; ++i) {
            free_rom_cache_page(_rom_cache[i].data);
            _rom_cache[i].data = 0;
            _rom_cache[i].page = 0;
            _rom_cache[i].age = 0;
            _rom_cache[i].valid = false;
        }
        _rom_cache_slots = 0;
        _rom_cache_pins_page0 = false;
        _rom_cache_age = 0;
    }

    int required_rom_cache_slots() const
    {
        switch (_rom_backend) {
            case ROM_BACKEND_SD_BANKED:
                return kGbMinRomCacheSlots;
            case ROM_BACKEND_HYBRID:
                return 1;
            case ROM_BACKEND_MAPPED:
            case ROM_BACKEND_NONE:
            default:
                return 0;
        }
    }

    bool release_one_extra_rom_cache_page_for_save()
    {
        const int keep_slots = required_rom_cache_slots();
        if (_rom_cache_slots <= keep_slots)
            return false;

        const int old_slots = _rom_cache_slots;
        const int i = old_slots - 1;
        free_rom_cache_page(_rom_cache[i].data);
        _rom_cache[i].data = 0;
        _rom_cache[i].page = 0;
        _rom_cache[i].age = 0;
        _rom_cache[i].valid = false;
        _rom_cache_slots = i;
        printf("walnutgb rom cache shrunk for small save ram: pages=%d->%d\n",
               old_slots,
               _rom_cache_slots);
        return true;
    }

    bool alloc_rom_cache(int min_slots, bool pin_page0)
    {
        free_rom_cache_storage();
        int pool_pages = 0;
        while (_rom_cache_slots < kGbMaxRomCacheSlots) {
            // Idle static buffers first (exactly page-sized ones, so the NES
            // sprite table stays free for an 8 KB resident save).
            uint8_t* data = gb_idle_borrow(kGbRomCachePageSize, kGbRomCachePageSize);
            if (data) {
                ++pool_pages;
            } else if (_rom_cache_slots < min_slots ||
                       gb_heap_has_room_for(kGbRomCachePageSize)) {
                // Heap only for the required minimum, or while the reserve
                // for BT / file I/O / C-library locks stays intact.
#ifdef ESP_PLATFORM
                data = (uint8_t*)heap_caps_malloc(kGbRomCachePageSize,
                                                  MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
                if (!data)
                    data = (uint8_t*)malloc(kGbRomCachePageSize);
#else
                data = (uint8_t*)malloc(kGbRomCachePageSize);
#endif
            }
            if (!data) {
#ifdef ESP_PLATFORM
                printf("walnutgb rom cache alloc stopped pages=%d min=%d heap=%u max=%u\n",
                       _rom_cache_slots,
                       min_slots,
                       (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                       (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
#else
                printf("walnutgb rom cache alloc stopped pages=%d min=%d\n",
                       _rom_cache_slots,
                       min_slots);
#endif
                break;
            }
            memset(data, 0xFF, kGbRomCachePageSize);
            _rom_cache[_rom_cache_slots].data = data;
            ++_rom_cache_slots;
        }
        if (pool_pages)
            printf("walnutgb rom cache pages=%d (idle buffers=%d)\n", _rom_cache_slots, pool_pages);
        if (_rom_cache_slots < min_slots) {
            printf("walnutgb rom cache alloc failed pages=%d min=%d\n",
                   _rom_cache_slots,
                   min_slots);
            free_rom_cache_storage();
            return false;
        }
        _rom_cache_pins_page0 = pin_page0;
        return true;
    }

    WALNUTGB_ALWAYS_INLINE RomCacheSlot* find_rom_cache_slot(uint32_t page)
    {
        if (_rom_cache_pins_page0 &&
            _rom_cache_slots > 0 &&
            _rom_cache[0].valid &&
            _rom_cache[0].page == page)
            return &_rom_cache[0];

        const int start = _rom_cache_pins_page0 ? 1 : 0;
        for (int i = start; i < _rom_cache_slots; ++i) {
            if (_rom_cache[i].valid && _rom_cache[i].page == page)
                return &_rom_cache[i];
        }
        return 0;
    }

    WALNUTGB_ALWAYS_INLINE RomCacheSlot* choose_rom_cache_slot(uint32_t page)
    {
        if (_rom_cache_slots <= 0)
            return 0;
        if (_rom_cache_pins_page0 && page == 0)
            return &_rom_cache[0];

        const int start = _rom_cache_pins_page0 ? 1 : 0;
        RomCacheSlot* oldest = 0;
        for (int i = start; i < _rom_cache_slots; ++i) {
            RomCacheSlot* slot = &_rom_cache[i];
            if (!slot->data)
                continue;
            if (!slot->valid)
                return slot;
            if (!oldest || slot->age < oldest->age)
                oldest = slot;
        }
        return oldest;
    }

    WALNUTGB_NOINLINE bool load_rom_page(RomCacheSlot* slot, uint32_t page)
    {
        if (!slot || !_rom_file)
            return false;

        const size_t offset = (size_t)page * kGbRomCachePageSize;
        if (offset >= _rom_len)
            return false;

        memset(slot->data, 0xFF, kGbRomCachePageSize);
        if (fseek(_rom_file, (long)offset, SEEK_SET) != 0) {
            printf("walnutgb rom page %u seek failed offset=%u\n",
                   (unsigned)page, (unsigned)offset);
            return false;
        }

        const size_t to_read = min(kGbRomCachePageSize, _rom_len - offset);
        if (read_stream_fully(_rom_file, slot->data, to_read) != to_read) {
            printf("walnutgb rom page %u read failed offset=%u len=%u\n",
                   (unsigned)page, (unsigned)offset, (unsigned)to_read);
            return false;
        }

        slot->page = page;
        slot->age = ++_rom_cache_age;
        slot->valid = true;
        return true;
    }

    WALNUTGB_ALWAYS_INLINE RomCacheSlot* ensure_rom_page(uint32_t page)
    {
        RomCacheSlot* slot = find_rom_cache_slot(page);
        if (slot) {
            slot->age = ++_rom_cache_age;
            return slot;
        }

        slot = choose_rom_cache_slot(page);
        if (!slot)
            return 0;

        if (!load_rom_page(slot, page)) {
            _fatal_error = true;
            return 0;
        }
        return slot;
    }

    WALNUTGB_ALWAYS_INLINE static uint16_t read_u16_fast(const uint8_t* src)
    {
        if ((uintptr_t)src & 1u)
            return (uint16_t)src[0] | ((uint16_t)src[1] << 8);
        return *(const uint16_t*)src;
    }

    WALNUTGB_ALWAYS_INLINE static uint32_t read_u32_fast(const uint8_t* src)
    {
        if ((uintptr_t)src & 3u) {
            return (uint32_t)src[0] |
                   ((uint32_t)src[1] << 8) |
                   ((uint32_t)src[2] << 16) |
                   ((uint32_t)src[3] << 24);
        }
        return *(const uint32_t*)src;
    }

    WALNUTGB_ALWAYS_INLINE uint8_t mapped_rom_read_byte_fast(uint32_t addr) const
    {
        if (addr >= _rom_len)
            return 0xFF;
        return _rom[addr];
    }

    WALNUTGB_ALWAYS_INLINE uint16_t mapped_rom_read_u16_fast(uint32_t addr) const
    {
        if ((addr + 1) >= _rom_len)
            return 0xFFFF;
        return read_u16_fast(_rom + addr);
    }

    WALNUTGB_ALWAYS_INLINE uint32_t mapped_rom_read_u32_fast(uint32_t addr) const
    {
        if ((addr + 3) >= _rom_len)
            return 0xFFFFFFFFu;
        return read_u32_fast(_rom + addr);
    }

    WALNUTGB_ALWAYS_INLINE uint8_t banked_rom_read_byte_fast(uint32_t addr)
    {
        if (addr >= _rom_len)
            return 0xFF;
        const uint32_t page = addr / kGbRomCachePageSize;
        const uint32_t page_offset = addr & (kGbRomCachePageSize - 1);
        RomCacheSlot* slot = ensure_rom_page(page);
        return slot ? slot->data[page_offset] : 0xFF;
    }

    WALNUTGB_ALWAYS_INLINE uint16_t banked_rom_read_u16_fast(uint32_t addr)
    {
        if ((addr + 1) >= _rom_len)
            return 0xFFFF;

        const uint32_t page = addr / kGbRomCachePageSize;
        const uint32_t page_offset = addr & (kGbRomCachePageSize - 1);
        if (page_offset <= (kGbRomCachePageSize - 2)) {
            RomCacheSlot* slot = ensure_rom_page(page);
            if (!slot)
                return 0xFFFF;
            return read_u16_fast(slot->data + page_offset);
        }

        return (uint16_t)banked_rom_read_byte_fast(addr)
             | ((uint16_t)banked_rom_read_byte_fast(addr + 1) << 8);
    }

    WALNUTGB_ALWAYS_INLINE uint32_t banked_rom_read_u32_fast(uint32_t addr)
    {
        if ((addr + 3) >= _rom_len)
            return 0xFFFFFFFFu;

        const uint32_t page = addr / kGbRomCachePageSize;
        const uint32_t page_offset = addr & (kGbRomCachePageSize - 1);
        if (page_offset <= (kGbRomCachePageSize - 4)) {
            RomCacheSlot* slot = ensure_rom_page(page);
            if (!slot)
                return 0xFFFFFFFFu;
            return read_u32_fast(slot->data + page_offset);
        }

        return (uint32_t)banked_rom_read_byte_fast(addr)
             | ((uint32_t)banked_rom_read_byte_fast(addr + 1) << 8)
             | ((uint32_t)banked_rom_read_byte_fast(addr + 2) << 16)
             | ((uint32_t)banked_rom_read_byte_fast(addr + 3) << 24);
    }

    WALNUTGB_ALWAYS_INLINE uint8_t hybrid_rom_read_byte_fast(uint32_t addr)
    {
        if ((size_t)addr < _rom_mapped_len)
            return _rom[addr];
        return banked_rom_read_byte_fast(addr);
    }

    WALNUTGB_ALWAYS_INLINE uint16_t hybrid_rom_read_u16_fast(uint32_t addr)
    {
        if ((addr + 1) >= _rom_len)
            return 0xFFFF;
        if ((size_t)(addr + 1) < _rom_mapped_len)
            return read_u16_fast(_rom + addr);
        return (uint16_t)hybrid_rom_read_byte_fast(addr)
             | ((uint16_t)hybrid_rom_read_byte_fast(addr + 1) << 8);
    }

    WALNUTGB_ALWAYS_INLINE uint32_t hybrid_rom_read_u32_fast(uint32_t addr)
    {
        if ((addr + 3) >= _rom_len)
            return 0xFFFFFFFFu;
        if ((size_t)(addr + 3) < _rom_mapped_len)
            return read_u32_fast(_rom + addr);
        return (uint32_t)hybrid_rom_read_byte_fast(addr)
             | ((uint32_t)hybrid_rom_read_byte_fast(addr + 1) << 8)
             | ((uint32_t)hybrid_rom_read_byte_fast(addr + 2) << 16)
             | ((uint32_t)hybrid_rom_read_byte_fast(addr + 3) << 24);
    }

    bool open_mapped_rom(const string& path, size_t len)
    {
        _rom = map_file(path.c_str(), (int)len);
        if (!_rom) {
            printf("walnutgb rom backend mapped failed len=%u for %s\n",
                   (unsigned)len,
                   path.c_str());
            return false;
        }

        _rom_backend = ROM_BACKEND_MAPPED;
        _rom_len = len;
        _rom_mapped_len = len;
        printf("walnutgb rom backend mapped len=%u\n", (unsigned)_rom_len);
        return true;
    }

    bool open_banked_rom(const string& path, size_t len)
    {
        if (!alloc_rom_cache(kGbMinRomCacheSlots, true)) {
            printf("walnutgb rom backend banked cache failed len=%u for %s\n",
                   (unsigned)len,
                   path.c_str());
            return false;
        }

        _rom_file = fopen(path.c_str(), "rb");
        if (!_rom_file) {
            printf("walnutgb rom backend banked open failed for %s\n",
                   path.c_str());
            free_rom_cache_storage();
            return false;
        }

        _rom_backend = ROM_BACKEND_SD_BANKED;
        _rom = 0;
        _rom_len = len;
        _rom_mapped_len = 0;
        if (!load_rom_page(&_rom_cache[0], 0)) {
            printf("walnutgb rom backend banked first page load failed for %s\n",
                   path.c_str());
            fclose(_rom_file);
            _rom_file = 0;
            _rom_backend = ROM_BACKEND_NONE;
            free_rom_cache_storage();
            return false;
        }

        for (int i = 1; i < _rom_cache_slots; ++i) {
            const uint32_t page = (uint32_t)i;
            if (((size_t)page * kGbRomCachePageSize) >= _rom_len)
                break;
            load_rom_page(&_rom_cache[i], page);
        }

        printf("walnutgb rom backend banked len=%u pages=%d page=%u\n",
               (unsigned)_rom_len,
               _rom_cache_slots,
               (unsigned)kGbRomCachePageSize);
        return true;
    }

    bool open_hybrid_rom(const string& path, size_t len, size_t mapped_len)
    {
        const size_t aligned_mapped_len =
            min(len, (mapped_len / kGbFixedRomBankSize) * kGbFixedRomBankSize);
        if (aligned_mapped_len < kGbFixedRomBankSize || aligned_mapped_len >= len)
            return false;

        uint8_t* mapped_rom = map_file_strict(path.c_str(), (int)aligned_mapped_len);
        if (!mapped_rom) {
            printf("walnutgb rom backend hybrid map failed mapped=%u len=%u for %s\n",
                   (unsigned)aligned_mapped_len,
                   (unsigned)len,
                   path.c_str());
            return false;
        }

        _rom_file = fopen(path.c_str(), "rb");
        if (!_rom_file) {
            printf("walnutgb rom backend hybrid open failed for %s\n",
                   path.c_str());
            unmap_file(mapped_rom);
            return false;
        }

        if (!alloc_rom_cache(1, false)) {
            printf("walnutgb rom backend hybrid cache failed mapped=%u len=%u for %s\n",
                   (unsigned)aligned_mapped_len,
                   (unsigned)len,
                   path.c_str());
            fclose(_rom_file);
            _rom_file = 0;
            unmap_file(mapped_rom);
            return false;
        }

        _rom = mapped_rom;
        _rom_backend = ROM_BACKEND_HYBRID;
        _rom_len = len;
        _rom_mapped_len = aligned_mapped_len;

        const uint32_t first_upper_page = (uint32_t)(_rom_mapped_len / kGbRomCachePageSize);
        for (int i = 0; i < _rom_cache_slots; ++i) {
            const uint32_t page = first_upper_page + (uint32_t)i;
            if (((size_t)page * kGbRomCachePageSize) >= _rom_len)
                break;
            if (!load_rom_page(&_rom_cache[i], page)) {
                if (i == 0) {
                    printf("walnutgb rom backend hybrid first banked page load failed for %s\n",
                           path.c_str());
                    fclose(_rom_file);
                    _rom_file = 0;
                    unmap_file(_rom);
                    _rom = 0;
                    _rom_backend = ROM_BACKEND_NONE;
                    _rom_len = 0;
                    _rom_mapped_len = 0;
                    free_rom_cache_storage();
                    return false;
                }
                break;
            }
        }

        printf("walnutgb rom backend hybrid len=%u mapped=%u pages=%d page=%u\n",
               (unsigned)_rom_len,
               (unsigned)_rom_mapped_len,
               _rom_cache_slots,
               (unsigned)kGbRomCachePageSize);
        return true;
    }

    void reset_audio_state()
    {
        _audio_r = 0;
        _audio_w = 0;
        _resample_ready = false;
        _resample_prev = 0;
        _resample_next = 0;
        _resample_phase_q16 = 0;
        _source_filter_prev = 0;
        _source_filter_ready = false;
        _resample_step_correction_q16 = 0;
        _apu_pitch_q16 = 1u << 16;
        _emu_speed_q16 = 1u << 16;
        minigb_apu_set_pitch_q16(&_apu, _apu_pitch_q16);
        _audio_frame_delta = 0;
        _audio_recovery_cooldown = 0;
    }

    double host_frame_rate() const
    {
        return standard == NTSC ? kHostNtscFrameRate : kHostPalFrameRate;
    }

    double emulated_frame_rate() const
    {
#if WALNUTGB_HOST_LOCK_TIMING
        double rate = host_frame_rate();
        const double cap = (double)WALNUTGB_HOST_LOCK_MAX_FRAME_RATE;
        if (cap > 0.0 && rate > cap)
            rate = cap;
        return rate;
#else
        return kGbFrameRate;
#endif
    }

    uint32_t host_audio_rate() const
    {
        return (uint32_t)audio_frequency;
    }

    static constexpr uint32_t apu_frame_samples()
    {
        return AUDIO_SAMPLES;
    }

    double effective_apu_rate() const
    {
        return (double)AUDIO_SAMPLES * emulated_frame_rate();
    }

    uint32_t apu_status_reg() const
    {
        return _apu.audio_mem[0xFF26 - AUDIO_ADDR_COMPENSATION];
    }

    uint32_t apu_mix_reg() const
    {
        return _apu.audio_mem[0xFF25 - AUDIO_ADDR_COMPENSATION];
    }

    void configure_timing()
    {
        _host_frame_period_us = frame_period_us_for_rate(host_frame_rate());
        _host_frame_step_q16 = q16_from_double(emulated_frame_rate() / host_frame_rate());
        _resample_step_q16 = q16_from_double(effective_apu_rate() / (double)host_audio_rate());
        _frame_accum_q16 = 0;
        _ntsc_audio_fraction_q16 = 0;
        audio_frame_samples = q16_samples_per_frame(host_audio_rate(), host_frame_rate());
        audio_fraction = 0;
        reset_timing_adaptation();
    }

    int gb_output_frame_sample_count()
    {
        const uint32_t n = (uint32_t)audio_frame_samples + _ntsc_audio_fraction_q16;
        _ntsc_audio_fraction_q16 = n & 0xFFFFu;
        return (int)(n >> 16);
    }

    int audio_output_safe_frame_samples() const
    {
        const int frame_samples = (audio_frame_samples + 0xFFFF) >> 16;
        return frame_samples + 1;
    }

    uint8_t clamp_draw_period(uint32_t period) const
    {
        const uint32_t lo = WALNUTGB_DRAW_PERIOD_MIN ? WALNUTGB_DRAW_PERIOD_MIN : 1u;
        const uint32_t hi = max<uint32_t>(lo, WALNUTGB_DRAW_PERIOD_MAX);
        if (period < lo)
            period = lo;
        if (period > hi)
            period = hi;
        return (uint8_t)period;
    }

    uint32_t clamp_host_frame_delta(uint32_t frames) const
    {
        if (frames < 1)
            frames = 1;
        const uint32_t max_frames = WALNUTGB_HOST_FRAME_DELTA_MAX ?
            WALNUTGB_HOST_FRAME_DELTA_MAX : 1u;
        if (frames > max_frames)
            frames = max_frames;
        return frames;
    }

    void add_audio_pending_frames(uint32_t frames)
    {
        if (frames == 0)
            return;
        const uint32_t cap = WALNUTGB_AUDIO_PENDING_FRAME_CAP ?
            WALNUTGB_AUDIO_PENDING_FRAME_CAP : 1u;
        const uint32_t pending = _audio_frame_delta;
        _audio_frame_delta = (frames > cap || pending > cap - frames) ?
            cap : (pending + frames);
    }

    void clamp_core_backlog()
    {
        const uint32_t cap_frames = WALNUTGB_CORE_BACKLOG_FRAME_CAP;
        if (cap_frames == 0)
            return;
        const uint64_t cap = (uint64_t)cap_frames * kQ16One;
        if ((uint64_t)_frame_accum_q16 > cap)
            _frame_accum_q16 = (uint32_t)cap;
    }

    void reset_timing_adaptation()
    {
        _draw_period = clamp_draw_period(WALNUTGB_INITIAL_DRAW_PERIOD);
        _line_effect_skip_streak =
            (uint8_t)min<uint32_t>(WALNUTGB_LINE_EFFECT_MAX_SKIP_FRAMES, UINT8_MAX);
        _lcd_skip_streak = 0;
        _core_frame_seq = 0;
        _timing_ema_us = 0;
        _timing_over_budget_streak = 0;
        _timing_under_budget_streak = 0;
        _audio_recovery_cooldown = 0;
        _presented_fps_last_ms = 0;
        _presented_fps_frames = 0;
        _presented_fps_q100 = 0;
        _presented_fps_draw_period = 0;
    }

    void update_presented_fps(uint32_t now)
    {
        if (_presented_fps_last_ms == 0) {
            _presented_fps_last_ms = now;
            _presented_fps_frames = 0;
            return;
        }

        const uint32_t elapsed_ms = now - _presented_fps_last_ms;
        if (elapsed_ms < 1000u)
            return;

        _presented_fps_q100 =
            (uint32_t)((((uint64_t)_presented_fps_frames * 100000u) +
                        (elapsed_ms / 2u)) / elapsed_ms);
        _presented_fps_draw_period = _draw_period;
        _presented_fps_last_ms = now;
        _presented_fps_frames = 0;
    }

    bool should_draw_scheduled_frame()
    {
#if WALNUTGB_ADAPTIVE_LCD_SKIP
        ++_core_frame_seq;
        if (_draw_period <= 1)
            return true;
        return (_core_frame_seq % _draw_period) == 0;
#else
        ++_core_frame_seq;
        return true;
#endif
    }

    bool lcd_line_effect_active() const
    {
        if (!(_gb.hram_io[IO_LCDC] & LCDC_ENABLE))
            return false;
        if (!(_gb.hram_io[IO_IE] & LCDC_INTR))
            return false;
        if (!(_gb.hram_io[IO_STAT] & STAT_LYC_INTR))
            return false;
        if (_gb.hram_io[IO_LYC] >= LCD_HEIGHT)
            return false;

        // Ages line-scroll state lives in these HRAM bytes.
        // Behaviours 0..2 need real LCD frames.
        const uint8_t ages_behaviour = _gb.hram_io[0x9B];
        const uint8_t ages_next_behaviour = _gb.hram_io[0x9D];
        return ages_behaviour <= 2 ||
               ages_next_behaviour <= 2;
    }

    void update_timing_adaptation(uint32_t update_us,
                                  uint32_t host_frames,
                                  int core_frames,
                                  bool capped_frames)
    {
#if WALNUTGB_ADAPTIVE_LCD_SKIP
        if (host_frames == 0 || core_frames <= 0 || _host_frame_period_us == 0)
            return;

        const uint32_t per_host_us = (update_us + (host_frames / 2u)) / host_frames;
        if (_timing_ema_us == 0)
            _timing_ema_us = per_host_us;
        else
            _timing_ema_us = ((_timing_ema_us * 7u) + per_host_us + 4u) >> 3;

        const uint32_t over_threshold =
            (_host_frame_period_us * WALNUTGB_OVER_BUDGET_PERCENT) / 100u;
        const uint32_t under_threshold =
            (_host_frame_period_us * WALNUTGB_UNDER_BUDGET_PERCENT) / 100u;
        const uint32_t severe_over_threshold =
            (_host_frame_period_us * WALNUTGB_SEVERE_OVER_BUDGET_PERCENT) / 100u;
        const uint32_t full_draw_over_threshold =
            (_host_frame_period_us * WALNUTGB_FULL_DRAW_OVER_BUDGET_PERCENT) / 100u;
        const uint32_t full_draw_severe_threshold =
            (_host_frame_period_us * WALNUTGB_FULL_DRAW_SEVERE_PERCENT) / 100u;
        const uint32_t draw_period_3_exit_threshold =
            (_host_frame_period_us * WALNUTGB_DRAW_PERIOD_3_EXIT_PERCENT) / 100u;

        const bool full_draw = _draw_period <= 1u;
        const bool current_draw_period_3 = _draw_period >= 3u;
        const bool over_budget = _timing_ema_us >
            (full_draw ? full_draw_over_threshold : over_threshold);
        const bool severe_over_budget =
            capped_frames && _timing_ema_us >
                (full_draw ? full_draw_severe_threshold : severe_over_threshold);
        const bool entering_draw_period_3 = _draw_period == 2u;
        const bool low_presented_fps =
            _presented_fps_q100 != 0u &&
            _presented_fps_draw_period == 2u &&
            _presented_fps_q100 < WALNUTGB_DRAW_PERIOD_3_ENTER_FPS_Q100;
        const bool escalate_draw_period =
            full_draw ? over_budget :
            (entering_draw_period_3 ? low_presented_fps : severe_over_budget);

        if (escalate_draw_period &&
            _draw_period < clamp_draw_period(WALNUTGB_DRAW_PERIOD_MAX)) {
            _timing_under_budget_streak = 0;
            const uint32_t up_streak = full_draw ?
                (severe_over_budget ? 1u : WALNUTGB_FULL_DRAW_UP_STREAK) :
                (entering_draw_period_3 ? WALNUTGB_DRAW_PERIOD_3_UP_STREAK :
                                          WALNUTGB_DRAW_PERIOD_SEVERE_UP_STREAK);
            if (++_timing_over_budget_streak >= max<uint32_t>(up_streak, 1u)) {
                _draw_period = clamp_draw_period((uint32_t)_draw_period + 1u);
                _timing_over_budget_streak = 0;
                _core_frame_seq = 0;
                _presented_fps_last_ms = 0;
                _presented_fps_frames = 0;
                _presented_fps_q100 = 0;
                _presented_fps_draw_period = 0;
            }
        } else if ((!current_draw_period_3 || !capped_frames) &&
                   _timing_ema_us <
                       (current_draw_period_3 ? draw_period_3_exit_threshold :
                                                under_threshold) &&
                   _draw_period > clamp_draw_period(WALNUTGB_DRAW_PERIOD_MIN)) {
            _timing_over_budget_streak = 0;
            const uint32_t next_period = (uint32_t)_draw_period - 1u;
            const uint32_t down_streak =
                current_draw_period_3 ? WALNUTGB_DRAW_PERIOD_3_DOWN_STREAK :
                (next_period <= 1u ? WALNUTGB_FULL_DRAW_DOWN_STREAK :
                                     WALNUTGB_DRAW_PERIOD_DOWN_STREAK);
            if (++_timing_under_budget_streak >= max<uint32_t>(down_streak, 1u)) {
                _draw_period = clamp_draw_period((uint32_t)_draw_period - 1u);
                _timing_under_budget_streak = 0;
                _core_frame_seq = 0;
                _presented_fps_last_ms = 0;
                _presented_fps_frames = 0;
                _presented_fps_q100 = 0;
                _presented_fps_draw_period = 0;
            }
        } else {
            _timing_over_budget_streak = 0;
            _timing_under_budget_streak = 0;
        }
#else
        (void)update_us;
        (void)host_frames;
        (void)core_frames;
        (void)capped_frames;
#endif
    }

    uint32_t audio_queue_target_samples(uint32_t capacity) const
    {
        const uint32_t target = midi_audio_low_latency() ?
            WALNUTGB_AUDIO_QUEUE_TARGET_MIDI :
            ((standard == NTSC) ? WALNUTGB_AUDIO_QUEUE_TARGET_NTSC
                                : WALNUTGB_AUDIO_QUEUE_TARGET);
        return min<uint32_t>(target, capacity - 1u);
    }

    uint32_t audio_queue_deadband_samples() const
    {
        return midi_audio_low_latency() ?
            WALNUTGB_AUDIO_QUEUE_DEADBAND_MIDI :
            ((standard == NTSC) ? WALNUTGB_AUDIO_QUEUE_DEADBAND_NTSC
                                : WALNUTGB_AUDIO_QUEUE_DEADBAND);
    }

    int audio_queue_max_correction_samples() const
    {
        return (standard == NTSC) ? WALNUTGB_AUDIO_QUEUE_MAX_CORRECTION_NTSC
                                  : WALNUTGB_AUDIO_QUEUE_MAX_CORRECTION;
    }

    int audio_queue_feedback_samples(int base_samples) const
    {
#if WALNUTGB_AUDIO_QUEUE_FEEDBACK
        (void)base_samples;
        const bool low_latency = midi_audio_low_latency();

        const uint32_t capacity = audio_queue_capacity();
        if (capacity == 0)
            return 0;

        const uint32_t queued = audio_queue_level();
        const uint32_t target = audio_queue_target_samples(capacity);
        const uint32_t step = audio_queue_deadband_samples();
        if (step == 0)
            return 0;

        const int max_correction = audio_queue_max_correction_samples();
        if (max_correction <= 0)
            return 0;

        int correction = 0;
        if (!low_latency && queued + step < target) {
            const uint32_t div = WALNUTGB_AUDIO_QUEUE_FILL_DIV ?
                WALNUTGB_AUDIO_QUEUE_FILL_DIV : 1u;
            const uint32_t deficit = target - queued - step;
            correction = (int)((deficit + div - 1u) / div);
            if (correction > max_correction)
                correction = max_correction;
        } else if (queued > target + step) {
            const uint32_t configured_div =
                WALNUTGB_AUDIO_QUEUE_DRAIN_DIV ?
                WALNUTGB_AUDIO_QUEUE_DRAIN_DIV : 1u;
            const uint32_t div = low_latency ? 1u : configured_div;
            const uint32_t surplus = queued - target - step;
            correction = -(int)(surplus / div);
            if (correction < -max_correction)
                correction = -max_correction;
        }

        return correction;
#else
        (void)base_samples;
        return 0;
#endif
    }

    uint32_t audio_available() const
    {
        return _audio_w - _audio_r;
    }

    uint32_t audio_ring_emergency_drain_threshold() const
    {
        const uint32_t guard = min<uint32_t>(apu_frame_samples(), kAudioRingSize / 4u);
        return (guard < kAudioRingSize) ? (kAudioRingSize - guard) : (kAudioRingSize - 1u);
    }

    uint32_t audio_ring_backpressure_threshold() const
    {
        const uint32_t guard = min<uint32_t>(apu_frame_samples(), kAudioRingSize / 4u);
        return (guard < kAudioRingSize) ? (kAudioRingSize - guard) : (kAudioRingSize - 1u);
    }

    bool audio_source_backpressure_active() const
    {
        return !midi_audio_low_latency() &&
               audio_available() >= audio_ring_backpressure_threshold();
    }

    bool audio_source_frame_room_available() const
    {
        return midi_audio_low_latency() ||
               audio_available() <= kAudioRingSize - apu_frame_samples();
    }

    static int32_t audio_correction_abs_i32(int32_t value)
    {
        return value < 0 ? -value : value;
    }

    static uint32_t audio_abs_diff_u32(uint32_t a, uint32_t b)
    {
        return a > b ? (a - b) : (b - a);
    }

    static int32_t smooth_audio_correction_q16(int32_t current,
                                               int32_t target,
                                               uint32_t shift)
    {
        if (current == target)
            return current;
        if (shift == 0 || shift >= 31)
            return target;

        const int32_t delta = target - current;
        const uint32_t round = (1u << shift) - 1u;
        if (delta > 0) {
            const uint32_t step = ((uint32_t)delta + round) >> shift;
            return current + (int32_t)step;
        }

        const uint32_t step = ((uint32_t)(-delta) + round) >> shift;
        return current - (int32_t)step;
    }

    static int16_t fade_audio_sample_towards_zero(int16_t sample)
    {
        const int32_t step = WALNUTGB_AUDIO_SOURCE_UNDERRUN_FADE_STEP;
        if (step <= 0)
            return 0;
        if (sample > step)
            return (int16_t)(sample - step);
        if (sample < -step)
            return (int16_t)(sample + step);
        return 0;
    }

    void update_audio_speed_estimate(uint32_t update_us,
                                     int core_frames)
    {
        const uint32_t emu_frame_period_us =
            frame_period_us_for_rate(emulated_frame_rate());
        if (update_us == 0 || emu_frame_period_us == 0)
            return;

        const uint32_t frames = core_frames > 0 ? (uint32_t)core_frames : 0u;
        uint64_t speed =
            ((uint64_t)frames * (uint64_t)emu_frame_period_us *
             (uint64_t)kQ16One + (update_us / 2u)) /
            (uint64_t)update_us;
        if (speed > kQ16One)
            speed = kQ16One;
        if (speed < WALNUTGB_AUDIO_SPEED_MIN_Q16)
            speed = WALNUTGB_AUDIO_SPEED_MIN_Q16;
        const uint32_t target = (uint32_t)speed;

        const uint32_t shift = (target < _emu_speed_q16) ?
            WALNUTGB_AUDIO_SPEED_SLOWDOWN_SHIFT :
            WALNUTGB_AUDIO_SPEED_RECOVERY_SHIFT;
        _emu_speed_q16 = (uint32_t)smooth_audio_correction_q16(
            (int32_t)_emu_speed_q16,
            (int32_t)target,
            shift);
    }

    int32_t audio_speed_step_correction_q16() const
    {
        if (_resample_step_q16 == 0 || _emu_speed_q16 >= kQ16One)
            return 0;

        const int32_t delta = (int32_t)_emu_speed_q16 - (int32_t)kQ16One;
        int32_t correction =
            (int32_t)((((int64_t)_resample_step_q16 * (int64_t)delta) +
                       (delta < 0 ? -(int64_t)(kQ16One / 2u) : (int64_t)(kQ16One / 2u))) >>
                      16);
        if (correction < -(int32_t)WALNUTGB_AUDIO_RING_MAX_SLOW_STEP_CORRECTION_Q16)
            correction = -(int32_t)WALNUTGB_AUDIO_RING_MAX_SLOW_STEP_CORRECTION_Q16;
        return correction;
    }

    int32_t audio_ring_step_correction_target_q16() const
    {
        if (midi_audio_low_latency())
            return 0;

        const uint32_t used = audio_available();
        const uint32_t target = min<uint32_t>(WALNUTGB_AUDIO_RING_TARGET, kAudioRingSize - 2u);
        const uint32_t deadband = min<uint32_t>(WALNUTGB_AUDIO_RING_DEADBAND, target);
        const uint32_t div = WALNUTGB_AUDIO_RING_STEP_DIV ? WALNUTGB_AUDIO_RING_STEP_DIV : 1u;
        int32_t correction = audio_speed_step_correction_q16();

        if (used > target + deadband) {
            int32_t drain = (int32_t)((used - target - deadband) / div);
            if (drain > (int32_t)WALNUTGB_AUDIO_RING_MAX_STEP_CORRECTION_Q16)
                drain = (int32_t)WALNUTGB_AUDIO_RING_MAX_STEP_CORRECTION_Q16;
            const uint32_t emergency = audio_ring_emergency_drain_threshold();
            if (used > emergency) {
                uint32_t emergency_drain =
                    (used - emergency) * WALNUTGB_AUDIO_RING_DRAIN_EMERGENCY_GAIN;
                if (emergency_drain > WALNUTGB_AUDIO_RING_MAX_DRAIN_STEP_CORRECTION_Q16)
                    emergency_drain = WALNUTGB_AUDIO_RING_MAX_DRAIN_STEP_CORRECTION_Q16;
                if (drain < (int32_t)emergency_drain)
                    drain = (int32_t)emergency_drain;
            }
            correction += drain;
        } else if (used + deadband < target) {
            const uint32_t deficit = target - used - deadband;
            const uint32_t span =
                min<uint32_t>(max<uint32_t>(target - deadband, 1u),
                              max<uint32_t>(WALNUTGB_AUDIO_RING_SLOW_RESPONSE_SAMPLES, 1u));
            uint32_t refill =
                (uint32_t)(((uint64_t)deficit *
                            (uint64_t)WALNUTGB_AUDIO_RING_MAX_REFILL_STEP_CORRECTION_Q16 +
                            (span / 2u)) / (uint64_t)span);
            if (refill > WALNUTGB_AUDIO_RING_MAX_REFILL_STEP_CORRECTION_Q16)
                refill = WALNUTGB_AUDIO_RING_MAX_REFILL_STEP_CORRECTION_Q16;
            correction -= (int32_t)refill;
        }

        if (correction < -(int32_t)WALNUTGB_AUDIO_RING_MAX_SLOW_STEP_CORRECTION_Q16)
            correction = -(int32_t)WALNUTGB_AUDIO_RING_MAX_SLOW_STEP_CORRECTION_Q16;
        if (correction > (int32_t)WALNUTGB_AUDIO_RING_MAX_DRAIN_STEP_CORRECTION_Q16)
            correction = (int32_t)WALNUTGB_AUDIO_RING_MAX_DRAIN_STEP_CORRECTION_Q16;
        return correction;
    }

    void update_audio_ring_step_correction()
    {
        const int32_t target = audio_ring_step_correction_target_q16();
        const bool crossing_from_slow_to_drain =
            _resample_step_correction_q16 < 0 && target >= 0;
        const bool emergency_drain =
            target > (int32_t)WALNUTGB_AUDIO_RING_MAX_STEP_CORRECTION_Q16;
        const bool increasing_correction =
            audio_correction_abs_i32(target) >
            audio_correction_abs_i32(_resample_step_correction_q16);
        const uint32_t shift = (crossing_from_slow_to_drain || emergency_drain) ?
            WALNUTGB_AUDIO_RING_CORRECTION_DRAIN_SHIFT :
            increasing_correction ?
            WALNUTGB_AUDIO_RING_CORRECTION_ATTACK_SHIFT :
            WALNUTGB_AUDIO_RING_CORRECTION_RELEASE_SHIFT;
        _resample_step_correction_q16 =
            smooth_audio_correction_q16(_resample_step_correction_q16,
                                        target,
                                        shift);
    }

    uint32_t audio_pitch_compensation_target_q16() const
    {
#if WALNUTGB_AUDIO_PITCH_COMPENSATION
        const int32_t step =
            (int32_t)_resample_step_q16 + _resample_step_correction_q16;
        if (_resample_step_q16 == 0 || step <= 0)
            return 1u << 16;
        if (WALNUTGB_AUDIO_PITCH_COMP_STEP_DEADBAND_Q16 > 0 &&
            audio_correction_abs_i32(_resample_step_correction_q16) <=
                (int32_t)WALNUTGB_AUDIO_PITCH_COMP_STEP_DEADBAND_Q16)
            return 1u << 16;

        // Compensate pitch for the current resampler step.
        uint32_t pitch = (uint32_t)((((uint64_t)_resample_step_q16 << 16) +
                                     ((uint32_t)step / 2u)) /
                                    (uint32_t)step);
        if (pitch < WALNUTGB_AUDIO_PITCH_COMP_MIN_Q16)
            pitch = WALNUTGB_AUDIO_PITCH_COMP_MIN_Q16;
        if (pitch > WALNUTGB_AUDIO_PITCH_COMP_MAX_Q16)
            pitch = WALNUTGB_AUDIO_PITCH_COMP_MAX_Q16;
        return pitch;
#else
        return 1u << 16;
#endif
    }

    void trim_audio_ring_for_pitch_transition(uint32_t target_pitch_q16)
    {
#if WALNUTGB_AUDIO_PITCH_TRANSITION_TRIM
        if (audio_abs_diff_u32(_apu_pitch_q16, target_pitch_q16) <
            WALNUTGB_AUDIO_PITCH_TRANSITION_THRESHOLD_Q16)
            return;

        const uint32_t keep = min<uint32_t>(
            WALNUTGB_AUDIO_PITCH_TRANSITION_RING_TARGET,
            kAudioRingSize - apu_frame_samples());
        const uint32_t used = audio_available();
        if (used <= keep)
            return;

        _audio_r += used - keep;
#else
        (void)target_pitch_q16;
#endif
    }

    void update_apu_pitch_compensation()
    {
        const uint32_t target = audio_pitch_compensation_target_q16();
        trim_audio_ring_for_pitch_transition(target);
        const uint32_t shift = (target > _apu_pitch_q16) ?
            WALNUTGB_AUDIO_PITCH_COMP_ATTACK_SHIFT :
            WALNUTGB_AUDIO_PITCH_COMP_RELEASE_SHIFT;
        _apu_pitch_q16 = (uint32_t)smooth_audio_correction_q16(
            (int32_t)_apu_pitch_q16,
            (int32_t)target,
            shift);
        minigb_apu_set_pitch_q16(&_apu, _apu_pitch_q16);
    }

    bool audio_recovery_skip_needed() const
    {
#if WALNUTGB_AUDIO_RECOVERY_SKIP
        const uint32_t threshold =
            max<uint32_t>(apu_frame_samples(), WALNUTGB_AUDIO_RECOVERY_RING_LOW_SAMPLES);
        return audio_available() < threshold;
#else
        return false;
#endif
    }

    int audio_queue_write_room_limit(int requested) const
    {
        if (requested <= 0)
            return 0;

        const uint32_t capacity = audio_queue_capacity();
        if (capacity == 0)
            return requested;

        const uint32_t queued = audio_queue_level();
        if (queued >= capacity)
            return 0;

        const uint32_t room = capacity - queued;
        if ((uint32_t)requested > room)
            return (int)room;
        return requested;
    }

    bool midi_audio_low_latency() const
    {
        return _midi_audio_active || gb_apu_midi_is_enabled();
    }

    bool pop_audio_sample(int16_t* sample)
    {
        if ((_audio_w - _audio_r) == 0)
            return false;
        *sample = _audio_ring[_audio_r++ & (kAudioRingSize - 1)];
        return true;
    }

    int16_t filter_source_sample(int16_t sample)
    {
#if WALNUTGB_NTSC_SOURCE_ANTIALIAS
        if (standard == NTSC) {
            if (!_source_filter_ready) {
                _source_filter_prev = sample;
                _source_filter_ready = true;
                return sample;
            }
            const int16_t filtered =
                (int16_t)(((int32_t)_source_filter_prev + (int32_t)sample) / 2);
            _source_filter_prev = sample;
            return filtered;
        }
#endif
        return sample;
    }

    void queue_apu_frame()
    {
        audio_sample_t mono[AUDIO_SAMPLES];
        update_apu_pitch_compensation();
        minigb_apu_audio_callback_mono(&_apu, mono);
        queue_audio_samples(mono, apu_frame_samples());
    }

    void queue_audio_samples(const audio_sample_t* mono, uint32_t count)
    {
        if (!mono || count == 0)
            return;

        const uint32_t used = _audio_w - _audio_r;
        if ((used + count) > kAudioRingSize)
            _audio_r += used + count - kAudioRingSize;

#if WALNUTGB_NTSC_SOURCE_ANTIALIAS
        if (standard == NTSC) {
            for (uint32_t i = 0; i < count; ++i)
                _audio_ring[(_audio_w + i) & (kAudioRingSize - 1)] =
                    filter_source_sample(mono[i]);
        } else
#endif
        {
            uint32_t w = _audio_w & (kAudioRingSize - 1);
            const uint32_t first = min<uint32_t>(count, kAudioRingSize - w);
            memcpy(_audio_ring + w, mono, first * sizeof(mono[0]));
            if (count > first)
                memcpy(_audio_ring, mono + first, (count - first) * sizeof(mono[0]));
        }
        _audio_w += count;
    }

    void render_midi_audio_direct(int16_t* out, int count)
    {
        if (!out || count <= 0)
            return;

        // Render only the source samples this host block consumes, plus one
        // interpolation lookahead sample.
        const uint32_t queue_delay_us =
            (uint32_t)(((uint64_t)audio_queue_level() * 1000000ull +
                        (host_audio_rate() / 2u)) /
                       host_audio_rate());
        const uint32_t first_sample_playout_us = micros() + queue_delay_us;
        int produced = 0;
        while (produced < count) {
            int chunk = min(count - produced, 256);
            uint32_t advances = 0;
            int source_count = 0;

            for (;;) {
                const uint64_t end_phase =
                    (uint64_t)_resample_phase_q16 +
                    (uint64_t)(uint32_t)chunk * (uint64_t)_resample_step_q16;
                advances = (uint32_t)(end_phase >> 16);
                source_count = (int)advances + (_resample_ready ? 0 : 2);
                if (source_count <= (int)AUDIO_SAMPLES || chunk <= 1)
                    break;
                chunk >>= 1;
            }

            audio_sample_t source[AUDIO_SAMPLES];
            if (source_count > 0)
                gb_apu_midi_render_samples_timed(
                    source, (uint16_t)source_count,
                    first_sample_playout_us);

            for (int i = 0; i < source_count; ++i)
                source[i] = filter_source_sample(source[i]);

            int source_pos = 0;
            if (!_resample_ready) {
                _resample_prev = source[source_pos++];
                _resample_next = source[source_pos++];
                _resample_phase_q16 = 0;
                _resample_ready = true;
            }

            for (int i = 0; i < chunk; ++i) {
                const int32_t a = _resample_prev;
                const int32_t next = _resample_next;
                out[produced + i] =
                    (int16_t)(a + (int32_t)(
                        ((int64_t)(next - a) *
                         (int64_t)_resample_phase_q16) >> 16));

                _resample_phase_q16 += _resample_step_q16;
                while (_resample_phase_q16 >= kQ16One) {
                    _resample_phase_q16 -= kQ16One;
                    _resample_prev = _resample_next;
                    _resample_next = source[source_pos++];
                }
            }

            produced += chunk;
        }
    }

    void prime_resampler()
    {
        if (_resample_ready)
            return;
        if (audio_available() < 2)
            return;
        pop_audio_sample(&_resample_prev);
        pop_audio_sample(&_resample_next);
        _resample_phase_q16 = 0;
        _resample_ready = true;
    }

    int16_t resample_audio_sample()
    {
        prime_resampler();
        if (!_resample_ready)
            return 0;

        int32_t a = _resample_prev;
        int32_t b = _resample_next;
        int32_t out = a + (int32_t)(
            ((int64_t)(b - a) * (int64_t)_resample_phase_q16) >> 16);

        int32_t step_q16 = (int32_t)_resample_step_q16 + _resample_step_correction_q16;
        if (step_q16 < 1)
            step_q16 = 1;
        _resample_phase_q16 += (uint32_t)step_q16;
        while (_resample_phase_q16 >= kQ16One) {
            _resample_phase_q16 -= kQ16One;
            _resample_prev = _resample_next;
            if (!pop_audio_sample(&_resample_next)) {
                _resample_next = fade_audio_sample_towards_zero(_resample_prev);
                break;
            }
        }
        return (int16_t)out;
    }

    bool gb_palette_glitch_effective() const
    {
        return _gb_palette_glitch_enabled && _gb_palette_glitch_game_active;
    }

    void rebuild_dmg_palette_cache()
    {
        const uint8_t phase = gb_palette_glitch_effective() ? _gb_palette_glitch_phase : 0;

        for (int bank = 0; bank < 3; ++bank) {
            for (int shade = 0; shade < 4; ++shade) {
                const uint8_t mapped = gb_palette_glitch_shift_dmg_shade((uint8_t)shade, phase);
                _dmg_palette[(bank << 2) + shade] = kDefaultDmgPalette[(bank << 2) + mapped];
            }
        }
    }

    uint32_t cgb_effective_palette_rgb(uint8_t index, uint16_t rgb565) const
    {
        if (!gb_palette_glitch_effective())
            return rgb565_to_corrected_rgb888(rgb565);

        uint16_t raw;
        if (index < 0x20) {
            raw = cgb_raw_palette_entry_to_bgr555(_gb.cgb.BGPalette, index);
        } else {
            raw = cgb_raw_palette_entry_to_bgr555(_gb.cgb.OAMPalette, index - 0x20);
        }
        raw = gb_palette_glitch_shift_bgr555(raw, _gb_palette_glitch_phase);
        return rgb565_to_corrected_rgb888(cgb_bgr555_to_rgb565(raw));
    }

    void rebuild_cgb_palette_cache_for_glitch()
    {
        for (int i = 0; i < 0x40; ++i)
            _cgb_palette_rgb[i] = cgb_effective_palette_rgb((uint8_t)i, _cgb_palette565[i]);
        _cgb_palette_dirty_mask = 0xFFFFFFFFFFFFFFFFull;
    }

    void apply_gb_palette_glitch_state()
    {
        rebuild_dmg_palette_cache();
        if (_gb.cgb.cgbMode)
            rebuild_cgb_palette_cache_for_glitch();
    }

    void prime_cgb_palette_cache(const uint16_t* src)
    {
        for (int i = 0; i < 0x40; ++i) {
            _cgb_palette565[i] = src[i];
            _cgb_palette_rgb[i] = cgb_effective_palette_rgb((uint8_t)i, src[i]);
        }
        _cgb_palette_dirty_mask = 0xFFFFFFFFFFFFFFFFull;
    }

    void reset_video_palette_for_boot()
    {
        video_reset_gb_base_palette(g_rgb332_palette);
        if (!_gb.cgb.cgbMode) {
            for (int shade = 0; shade < 4; ++shade)
                video_set_gb_palette_entry((uint8_t)(kDmgShadeIndexBase + shade),
                                           kDmgShadeRgb[shade]);
        }
        rebuild_dmg_palette_cache();
        if (_gb.cgb.cgbMode) {
            rebuild_cgb_fix_palette(&_gb);
            prime_cgb_palette_cache(_gb.cgb.fixPalette);
        } else {
            _cgb_palette_dirty_mask = 0;
        }
    }

    static void cgb_palette_changed(gb_s* gb, uint8_t index, uint16_t rgb565)
    {
        EmuGameBoy* emu = self(gb);
        if (!emu || index >= 0x40)
            return;
        emu->_cgb_palette565[index] = rgb565;
        emu->_cgb_palette_rgb[index] = emu->cgb_effective_palette_rgb(index, rgb565);
        emu->_cgb_palette_dirty_mask |= (1ull << index);
    }

    uint32_t sync_cgb_video_palette()
    {
        if (_cgb_palette_dirty_mask == 0)
            return 0;

        uint64_t remaining = 0;
        uint64_t dirty = _cgb_palette_dirty_mask;
        uint32_t pushed = 0;
        while (dirty) {
            const int idx = __builtin_ctzll(dirty);
            const uint64_t bit = dirty & (~dirty + 1);
            if (!video_set_gb_palette_entry((uint8_t)(kGbCgbPaletteBase + idx),
                                            _cgb_palette_rgb[idx])) {
                remaining |= bit;
            } else {
                ++pushed;
            }
            dirty &= dirty - 1;
        }
        _cgb_palette_dirty_mask = remaining;
        return pushed;
    }

    // Idle static buffers first; heap only when `required` (the minimum to
    // run the game) or while the heap reserve stays intact.
    uint8_t* alloc_cart_ram_storage(size_t len, bool required, bool* from_pool = 0)
    {
        uint8_t* p = gb_idle_borrow(len, (size_t)-1);
        if (from_pool)
            *from_pool = p != 0;
        if (!p && (required || gb_heap_has_room_for(len))) {
#ifdef ESP_PLATFORM
            p = (uint8_t*)heap_caps_malloc(len, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#else
            p = (uint8_t*)malloc(len);
#endif
        }
        if (p)
            memset(p, cart_ram_erased_value(), len);
        return p;
    }

    uint8_t cart_ram_erased_value() const
    {
        return _gb.cart_is_pocket_camera ? 0xFF : 0x00;
    }

    bool cart_ram_page_looks_stale_zero_erase(const uint8_t* data, size_t len) const
    {
        if (!_gb.cart_is_pocket_camera || !data || len == 0)
            return false;
        for (size_t i = 0; i < len; ++i) {
            if (data[i] != 0)
                return false;
        }
        return true;
    }

    bool try_load_save_ram_resident(const char* mode)
    {
        _cart_ram_full = true;
        _cart_ram_window_len = _cart_ram_len;
        // A resident save is optional (banked windows work too), so the heap
        // is only used while its reserve stays intact.
        bool from_pool = false;
        _cart_ram = alloc_cart_ram_storage(_cart_ram_window_len, false, &from_pool);
        if (from_pool)
            mode = "idle-buffer";
        if (!_cart_ram) {
            _cart_ram_full = false;
            return false;
        }

        if (read_binary_file(_save_path, _cart_ram, _cart_ram_len) &&
            cart_ram_page_looks_stale_zero_erase(_cart_ram, _cart_ram_len)) {
            memset(_cart_ram, 0xFF, _cart_ram_len);
            printf("gb pocket camera save looked zero-erased; treating as erased FF\n");
        }
        _cart_ram_dirty = false;
        printf("gb save ram resident: total=%u mode=%s rom_pages=%d heap=%u max=%u\n",
               (unsigned)_cart_ram_len,
               mode,
               _rom_cache_slots,
               (unsigned)ESP.getFreeHeap(),
               (unsigned)ESP.getMaxAllocHeap());
        return true;
    }

    bool alloc_cart_ram_cache_page(int slot_index)
    {
        if (slot_index < 0 || slot_index >= kGbSaveRamCacheMaxSlots)
            return false;
        // Only the first window is required; extra heap windows respect the
        // reserve (idle buffers are always fine).
        uint8_t* data = alloc_cart_ram_storage(_cart_ram_window_len, slot_index == 0);
        if (!data)
            return false;
        _cart_ram_cache[slot_index].data = data;
        _cart_ram_cache[slot_index].page = 0;
        _cart_ram_cache[slot_index].age = 0;
        _cart_ram_cache[slot_index].valid = false;
        _cart_ram_cache[slot_index].dirty = false;
        ++_cart_ram_cache_slots;
        return true;
    }

    void alloc_cart_ram_cache_pages()
    {
        while (_cart_ram_cache_slots < kGbSaveRamCacheMaxSlots) {
            if (!alloc_cart_ram_cache_page(_cart_ram_cache_slots))
                break;
        }
    }

    void load_save_ram()
    {
        if (!_cart_ram_len)
            return;

        if (_cart_ram_len <= kGbSaveRamFullLoadMax) {
            if (try_load_save_ram_resident("direct"))
                return;

            while (_cart_ram_len <= kGbSaveRamPriorityFullLoadMax &&
                   release_one_extra_rom_cache_page_for_save()) {
                if (try_load_save_ram_resident("small-priority"))
                    return;
            }

            printf("gb save ram resident alloc failed: total=%u heap=%u max=%u; falling back to banked\n",
                   (unsigned)_cart_ram_len,
                   (unsigned)ESP.getFreeHeap(),
                   (unsigned)ESP.getMaxAllocHeap());
            _cart_ram_full = false;
        }

        _cart_ram_full = false;
        _cart_ram_window_len = min(_cart_ram_len, kGbSaveRamWindowSize);
        for (;;) {
            alloc_cart_ram_cache_pages();
            if (_cart_ram_cache_slots > 0)
                break;
            if (release_one_extra_rom_cache_page_for_save())
                continue;
            if (_cart_ram_window_len <= kGbSaveRamMinWindowSize)
                break;
            _cart_ram_window_len =
                max(kGbSaveRamMinWindowSize, _cart_ram_window_len >> 1);
        }

        _cart_ram_dirty = false;
        if (_cart_ram_cache_slots == 0) {
            printf("gb save ram cache alloc failed: total=%u page=%u heap=%u max=%u\n",
                   (unsigned)_cart_ram_len,
                   (unsigned)_cart_ram_window_len,
                   (unsigned)ESP.getFreeHeap(),
                   (unsigned)ESP.getMaxAllocHeap());
            return;
        }

        printf("gb save ram cached: total=%u page=%u slots=%d heap=%u max=%u\n",
               (unsigned)_cart_ram_len,
               (unsigned)_cart_ram_window_len,
               _cart_ram_cache_slots,
               (unsigned)ESP.getFreeHeap(),
               (unsigned)ESP.getMaxAllocHeap());
    }

    void seed_pocket_camera_image_if_blank()
    {
        if (!_gb.cart_is_pocket_camera || !_cart_ram_len ||
            !_gb.gb_cart_ram_read || !_gb.gb_cart_ram_write)
            return;

        const size_t image_len =
            (size_t)POCKET_CAMERA_IMAGE_TILE_COLS *
            (size_t)POCKET_CAMERA_IMAGE_TILE_ROWS *
            (size_t)POCKET_CAMERA_TILE_BYTES;
        if (POCKET_CAMERA_IMAGE_OFFSET + image_len > _cart_ram_len)
            return;

        bool all_ff = true;
        bool all_zero = true;
        for (size_t i = 0; i < image_len; ++i) {
            const uint8_t b = _gb.gb_cart_ram_read(
                &_gb, POCKET_CAMERA_IMAGE_OFFSET + (uint_fast32_t)i);
            if (b != 0xFF)
                all_ff = false;
            if (b != 0x00)
                all_zero = false;
            if (!all_ff && !all_zero)
                return;
        }

        __gb_pocket_camera_write_dummy_frame(&_gb);
        printf("gb pocket camera seeded dummy frame into blank save image area (%s)\n",
               all_zero ? "00" : "FF");
    }

    bool flush_cart_ram_cache_slot(CartRamCacheSlot* slot)
    {
        if (!slot || !slot->valid || !slot->dirty)
            return true;

        const size_t offset =
            (size_t)slot->page * _cart_ram_window_len;
        if (offset >= _cart_ram_len)
            return false;
        const size_t len = min(_cart_ram_window_len, _cart_ram_len - offset);
        if (!write_binary_file_range(_save_path, offset, slot->data, len,
                                     _cart_ram_len, cart_ram_erased_value())) {
            log_save_write_failed("page", slot->page, offset, len);
            return false;
        }
        _save_error_logged = false;
        slot->dirty = false;
        return true;
    }

    void log_save_write_failed(const char* mode, uint32_t page, size_t offset, size_t len)
    {
        if (_save_error_logged)
            return;
        _save_error_logged = true;
        printf("gb save ram write failed: mode=%s page=%u offset=%u len=%u retry=%ums\n",
               mode,
               (unsigned)page,
               (unsigned)offset,
               (unsigned)len,
               (unsigned)kSaveErrorRetryMs);
    }

    bool flush_cart_ram_bank(bool force)
    {
        if (!_cart_ram_len)
            return true;
        if (!_cart_ram_dirty && !force)
            return true;

        if (_cart_ram_full) {
            if (!_cart_ram)
                return true;
            if (!write_binary_file(_save_path, _cart_ram, _cart_ram_len)) {
                log_save_write_failed("full", 0, 0, _cart_ram_len);
                return false;
            }
            _save_error_logged = false;
            _cart_ram_dirty = false;
            return true;
        }

        if (_cart_ram_cache_slots <= 0)
            return true;

        if (!force) {
            CartRamCacheSlot* dirty_slot = 0;
            for (int i = 0; i < _cart_ram_cache_slots; ++i) {
                CartRamCacheSlot* slot = &_cart_ram_cache[i];
                if (slot->dirty && (!dirty_slot || slot->age < dirty_slot->age))
                    dirty_slot = slot;
            }
            if (!dirty_slot) {
                _cart_ram_dirty = false;
                return true;
            }
            if (!flush_cart_ram_cache_slot(dirty_slot))
                return false;

            bool still_dirty = false;
            for (int i = 0; i < _cart_ram_cache_slots; ++i) {
                if (_cart_ram_cache[i].dirty) {
                    still_dirty = true;
                    break;
                }
            }
            _cart_ram_dirty = still_dirty;
            return !still_dirty;
        }

        bool ok = true;
        bool still_dirty = false;
        for (int i = 0; i < _cart_ram_cache_slots; ++i) {
            if (!flush_cart_ram_cache_slot(&_cart_ram_cache[i]))
                ok = false;
            if (_cart_ram_cache[i].dirty)
                still_dirty = true;
        }
        _cart_ram_dirty = still_dirty;
        return ok;
    }

    CartRamCacheSlot* load_cart_ram_page(uint32_t page)
    {
        if (_cart_ram_cache_slots <= 0 || !_cart_ram_len)
            return 0;

        for (int i = 0; i < _cart_ram_cache_slots; ++i) {
            CartRamCacheSlot* slot = &_cart_ram_cache[i];
            if (slot->valid && slot->page == page) {
                slot->age = ++_cart_ram_cache_age;
                return slot;
            }
        }

        CartRamCacheSlot* victim = 0;
        for (int i = 0; i < _cart_ram_cache_slots; ++i) {
            CartRamCacheSlot* slot = &_cart_ram_cache[i];
            if (!slot->valid) {
                victim = slot;
                break;
            }
            if (!slot->dirty && (!victim || victim->dirty || slot->age < victim->age))
                victim = slot;
        }
        if (!victim) {
            for (int i = 0; i < _cart_ram_cache_slots; ++i) {
                CartRamCacheSlot* slot = &_cart_ram_cache[i];
                if (!victim || slot->age < victim->age)
                    victim = slot;
            }
        }
        if (!victim)
            return 0;
        if (!flush_cart_ram_cache_slot(victim))
            return 0;

        const size_t offset =
            (size_t)page * _cart_ram_window_len;
        if (offset >= _cart_ram_len)
            return 0;
        const size_t len = min(_cart_ram_window_len, _cart_ram_len - offset);
        memset(victim->data, cart_ram_erased_value(), _cart_ram_window_len);
        if (read_binary_file_range(_save_path, offset, victim->data, len) &&
            cart_ram_page_looks_stale_zero_erase(victim->data, len)) {
            memset(victim->data, 0xFF, _cart_ram_window_len);
            printf("gb pocket camera save page %u looked zero-erased; treating as erased FF\n",
                   (unsigned)page);
        }
        victim->page = page;
        victim->age = ++_cart_ram_cache_age;
        victim->valid = true;
        victim->dirty = false;
        return victim;
    }

    void load_rtc_state()
    {
        if (_rtc_path.empty() || _gb.mbc != 3)
            return;

        struct tm tm_now = {};
        fill_local_time(&tm_now);
        gb_set_rtc(&_gb, &tm_now);

        RtcSnapshot snap = {};
        if (!read_binary_file(_rtc_path, (uint8_t*)&snap, sizeof(snap)))
            return;

        memcpy(_gb.rtc_real.bytes, snap.rtc, sizeof(snap.rtc));
        uint64_t now = unix_time_now();
        if (snap.unix_time && now > snap.unix_time)
            rtc_advance(&_gb.rtc_real, now - snap.unix_time);
    }

    void save_rtc_state()
    {
        if (_rtc_path.empty() || _gb.mbc != 3)
            return;

        RtcSnapshot snap = {};
        snap.unix_time = unix_time_now();
        memcpy(snap.rtc, _gb.rtc_real.bytes, sizeof(snap.rtc));
        write_binary_file(_rtc_path, &snap, sizeof(snap));
    }

    void flush_save(bool force)
    {
        if (!_cart_ram_len) {
            if (force)
                save_rtc_state();
            return;
        }
        if (!_save_dirty && !force)
            return;
        const bool save_ok = flush_cart_ram_bank(force);
        save_rtc_state();
        if (save_ok) {
            _save_dirty = false;
            _save_deadline_ms = millis() + kSaveFlushMs;
        } else if (!force) {
            _save_deadline_ms = millis() + kSaveErrorRetryMs;
        }
    }

    void shutdown_cart()
    {
        flush_save(true);
        walnutgb_audio_bind(0);
        gb_buffer_free(_cart_ram);     // idle-pool buffer or heap
        _cart_ram = 0;
        for (int i = 0; i < kGbSaveRamCacheMaxSlots; ++i) {
            gb_buffer_free(_cart_ram_cache[i].data);
            _cart_ram_cache[i].data = 0;
            _cart_ram_cache[i].page = 0;
            _cart_ram_cache[i].age = 0;
            _cart_ram_cache[i].valid = false;
            _cart_ram_cache[i].dirty = false;
        }
        _cart_ram_len = 0;
        _cart_ram_window_len = 0;
        _cart_ram_cache_slots = 0;
        _cart_ram_cache_age = 0;
        _cart_ram_dirty = false;
        _cart_ram_full = false;
        if (_rom)
            unmap_file(_rom);
        if (_rom_file)
            fclose(_rom_file);
        _rom_file = 0;
        _rom = 0;
        _rom_len = 0;
        _rom_mapped_len = 0;
        _rom_backend = ROM_BACKEND_NONE;
        free_rom_cache_storage();
        _rom_path.clear();
        _save_path.clear();
        _rtc_path.clear();
        _save_dirty = false;
        _save_error_logged = false;
        _fatal_error = false;
    }

    void run_frame_once(bool skip_lcd, bool suppress_lcd_stat_interrupts = false)
    {
        if (!has_rom() || _fatal_error)
            return;
        walnutgb_audio_bind(&_apu);
        const bool prev_frame_skip = _gb.direct.frame_skip;
        const bool prev_frame_skip_count = _gb.display.frame_skip_count;
        const bool prev_suppress_lcd_stat_interrupts =
            _gb.direct.suppress_lcd_stat_interrupts;
        if (skip_lcd) {
            _gb.direct.frame_skip = true;
            _gb.display.frame_skip_count = false;
        }
        if (suppress_lcd_stat_interrupts)
            _gb.direct.suppress_lcd_stat_interrupts = true;
#if WALNUT_GB_THREADED_CPU_ACTIVE
        gb_run_frame_threaded(&_gb);
#elif WALNUTGB_USE_DUALFETCH
        gb_run_frame_dualfetch(&_gb);
#else
        gb_run_frame(&_gb);
#endif
        queue_apu_frame();
        bool present = !skip_lcd;
#if WALNUTGB_DIRTY_FRAME_SKIP
        present = present && _gb.display.frame_dirty_active;
#endif
        if (present)
            present_frame();
        if (skip_lcd) {
            _gb.direct.frame_skip = prev_frame_skip;
            _gb.display.frame_skip_count = prev_frame_skip_count;
        }
        _gb.direct.suppress_lcd_stat_interrupts =
            prev_suppress_lcd_stat_interrupts;
    }

    void prefill()
    {
        reset_audio_state();
        reset_timing_adaptation();
        const uint32_t prefill_frames = max<uint32_t>(WALNUTGB_AUDIO_PREFILL_FRAMES, 2u);
        for (uint32_t i = 0; i < prefill_frames; ++i)
            run_frame_once(false);
        _save_deadline_ms = millis() + kSaveFlushMs;
    }

    void apply_pad_mask(uint8_t mask)
    {
        _gb.direct.joypad = (uint8_t)(0xFFu & ~mask);
    }

    static uint8_t rom_read(gb_s* gb, const uint_fast32_t addr)
    {
        EmuGameBoy* emu = self(gb);
        if (!emu)
            return 0xFF;


        switch (emu->_rom_backend) {
            case ROM_BACKEND_MAPPED:
                return emu->mapped_rom_read_byte_fast((uint32_t)addr);
            case ROM_BACKEND_SD_BANKED:
                return emu->banked_rom_read_byte_fast((uint32_t)addr);
            case ROM_BACKEND_HYBRID:
                return emu->hybrid_rom_read_byte_fast((uint32_t)addr);
            case ROM_BACKEND_NONE:
            default:
                return 0xFF;
        }
    }

    static uint16_t rom_read16(gb_s* gb, const uint_fast32_t addr)
    {
        EmuGameBoy* emu = self(gb);
        if (!emu)
            return 0xFFFF;


        switch (emu->_rom_backend) {
            case ROM_BACKEND_MAPPED:
                return emu->mapped_rom_read_u16_fast((uint32_t)addr);
            case ROM_BACKEND_SD_BANKED:
                return emu->banked_rom_read_u16_fast((uint32_t)addr);
            case ROM_BACKEND_HYBRID:
                return emu->hybrid_rom_read_u16_fast((uint32_t)addr);
            case ROM_BACKEND_NONE:
            default:
                return 0xFFFF;
        }
    }

    static uint32_t rom_read32(gb_s* gb, const uint_fast32_t addr)
    {
        EmuGameBoy* emu = self(gb);
        if (!emu)
            return 0xFFFFFFFFu;


        switch (emu->_rom_backend) {
            case ROM_BACKEND_MAPPED:
                return emu->mapped_rom_read_u32_fast((uint32_t)addr);
            case ROM_BACKEND_SD_BANKED:
                return emu->banked_rom_read_u32_fast((uint32_t)addr);
            case ROM_BACKEND_HYBRID:
                return emu->hybrid_rom_read_u32_fast((uint32_t)addr);
            case ROM_BACKEND_NONE:
            default:
                return 0xFFFFFFFFu;
        }
    }

    static uint8_t cart_ram_read(gb_s* gb, const uint_fast32_t addr)
    {
        EmuGameBoy* emu = self(gb);
        if (!emu || addr >= emu->_cart_ram_len)
            return 0xFF;
        if (emu->_cart_ram_full) {
            if (!emu->_cart_ram)
                return 0xFF;
            return emu->_cart_ram[(size_t)addr];
        }
        if (emu->_cart_ram_window_len == 0)
            return 0xFF;
        const uint32_t page =
            (uint32_t)(addr / emu->_cart_ram_window_len);
        CartRamCacheSlot* slot = emu->load_cart_ram_page(page);
        const size_t page_offset =
            (size_t)addr % emu->_cart_ram_window_len;
        if (!slot || page_offset >= emu->_cart_ram_window_len)
            return 0xFF;
        return slot->data[page_offset];
    }

    static void cart_ram_write(gb_s* gb, const uint_fast32_t addr, const uint8_t val)
    {
        EmuGameBoy* emu = self(gb);
        if (!emu || addr >= emu->_cart_ram_len)
            return;
        if (emu->_cart_ram_full) {
            if (!emu->_cart_ram)
                return;
            if (emu->_cart_ram[(size_t)addr] == val)
                return;
            emu->_cart_ram[(size_t)addr] = val;
            emu->_cart_ram_dirty = true;
            if (!emu->_save_dirty)
                emu->_save_deadline_ms = millis() + kSaveFlushMs;
            emu->_save_dirty = true;
            return;
        }
        if (emu->_cart_ram_window_len == 0)
            return;
        const uint32_t page =
            (uint32_t)(addr / emu->_cart_ram_window_len);
        CartRamCacheSlot* slot = emu->load_cart_ram_page(page);
        const size_t page_offset =
            (size_t)addr % emu->_cart_ram_window_len;
        if (!slot || page_offset >= emu->_cart_ram_window_len)
            return;
        if (slot->data[page_offset] == val)
            return;
        slot->data[page_offset] = val;
        slot->dirty = true;
        emu->_cart_ram_dirty = true;
        if (!emu->_save_dirty)
            emu->_save_deadline_ms = millis() + kSaveFlushMs;
        emu->_save_dirty = true;
    }

    static void error(gb_s* gb, const enum gb_error_e gb_err, const uint16_t addr)
    {
        static const char* kErrors[] = {
            "UNKNOWN",
            "INVALID OPCODE",
            "INVALID READ",
            "INVALID WRITE",
            "HALT FOREVER"
        };

        EmuGameBoy* emu = self(gb);
        printf("walnutgb fatal error %s at 0x%04X\n",
               (gb_err >= 0 && gb_err < GB_INVALID_MAX) ? kErrors[gb_err] : "UNKNOWN",
               addr);
        if (emu)
            emu->_fatal_error = true;
    }

    static void lcd_draw_line(gb_s* gb, const uint8_t* pixels, const uint_fast8_t line)
    {
        EmuGameBoy* emu = self(gb);
        if (!emu || line >= kGbHeight)
            return;

        uint8_t* dst = emu->draw_pixels() + ((size_t)line * kGbWidth);
        if (gb->cgb.cgbMode) {
            emu->sync_cgb_video_palette();
            copy_cgb_line(dst, pixels);
        } else {
            for (int x = 0; x < kGbWidth; ++x) {
                const uint8_t p = pixels[x];
                const uint8_t idx = (uint8_t)(((p & LCD_PALETTE_ALL) >> 2) | (p & LCD_COLOUR));
                dst[x] = emu->_dmg_palette[idx];
            }
        }
    }

public:
    EmuGameBoy(int ntsc)
        : Emu("gameboy", kGbWidth, kGbHeight, ntsc, (16 | (1 << 8)), 4, EMU_GB),
          _lines(0),
          _front_buffer(0),
          _rom_backend(ROM_BACKEND_NONE),
          _rom(0),
          _rom_len(0),
          _rom_mapped_len(0),
          _rom_file(0),
          _rom_cache_slots(0),
          _rom_cache_pins_page0(false),
          _rom_cache_age(0),
          _cart_ram(0),
          _cart_ram_len(0),
          _cart_ram_window_len(0),
          _cart_ram_cache_slots(0),
          _cart_ram_cache_age(0),
          _cart_ram_dirty(false),
          _cart_ram_full(false),
          _save_dirty(false),
          _save_error_logged(false),
          _fatal_error(false),
          _cgb_palette_dirty_mask(0),
          _gb_palette_glitch_enabled(0),
          _gb_palette_glitch_game_active(1),
          _gb_palette_glitch_phase(0),
          _audio_r(0),
          _audio_w(0),
          _resample_ready(false),
          _resample_prev(0),
          _resample_next(0),
          _resample_phase_q16(0),
          _resample_step_q16(0),
          _resample_step_correction_q16(0),
          _source_filter_prev(0),
          _source_filter_ready(false),
          _apu_pitch_q16(1u << 16),
          _emu_speed_q16(1u << 16),
          _host_frame_step_q16(0),
          _frame_accum_q16(0),
          _ntsc_audio_fraction_q16(0),
          _host_frame_delta(1),
          _audio_frame_delta(0),
          _core_frame_seq(0),
          _timing_ema_us(0),
          _timing_over_budget_streak(0),
          _timing_under_budget_streak(0),
          _host_frame_period_us(0),
          _presented_fps_last_ms(0),
          _presented_fps_frames(0),
          _presented_fps_q100(0),
          _presented_fps_draw_period(0),
          _draw_period(1),
          _line_effect_skip_streak(0),
          _lcd_skip_streak(0),
          _audio_recovery_cooldown(0),
          _midi_audio_active(false),
          _save_deadline_ms(0)
    {
        _ext = kGbExt;
        _help = kGbHelp;
#if AUDIO_DECOUPLED_TIMER && AUDIO_OUTPUT_2X_SCANLINE
        audio_frequency = standard ? AUDIO_TIMER_ACTUAL_RATE(AUDIO_OUTPUT_RATE_NTSC_HZ)
                                   : AUDIO_TIMER_ACTUAL_RATE(AUDIO_OUTPUT_RATE_PAL_HZ);
        audio_frame_samples = standard
            ? q16_samples_per_frame((uint32_t)audio_frequency, kHostNtscFrameRate)
            : (audio_frequency << 16)/50;
        audio_fraction = 0;
#else
        if (!standard) {
            audio_frequency = kHostPalAudioRate;
            audio_frame_samples = (audio_frequency << 16)/50;
            audio_fraction = 0;
        }
#endif
        _frame_lines[0] = 0;
        _frame_lines[1] = 0;
        _frame_pixels[0] = 0;
        _frame_pixels[1] = 0;
        memset(&_gb, 0, sizeof(_gb));
        memset(&_apu, 0, sizeof(_apu));
        memset(_rom_cache, 0, sizeof(_rom_cache));
        memset(_cart_ram_cache, 0, sizeof(_cart_ram_cache));
        memset(_cgb_palette565, 0xFF, sizeof(_cgb_palette565));
        memset(_cgb_palette_rgb, 0, sizeof(_cgb_palette_rgb));
        memcpy(_dmg_palette, kDefaultDmgPalette, sizeof(_dmg_palette));
        g_active_gb_emu = this;
        init_rgb332_palette();
        if (!init_screen()) {
            _fatal_error = true;
            return;
        }
        configure_timing();
    }

    virtual ~EmuGameBoy()
    {
        if (g_active_gb_emu == this)
            g_active_gb_emu = nullptr;
        shutdown_cart();
    }

    void palette_glitch_set_enabled(int enabled)
    {
        _gb_palette_glitch_enabled = enabled ? 1 : 0;
        if (!_gb_palette_glitch_enabled)
            _gb_palette_glitch_phase = 0;
        apply_gb_palette_glitch_state();
    }

    void palette_glitch_set_game_active(int active)
    {
        _gb_palette_glitch_game_active = active ? 1 : 0;
        apply_gb_palette_glitch_state();
    }

    void palette_glitch_cycle()
    {
        if (!gb_palette_glitch_effective())
            return;
        _gb_palette_glitch_phase = (uint8_t)((_gb_palette_glitch_phase + 1) % 6);
        apply_gb_palette_glitch_state();
    }

    virtual void gen_palettes() {}

    virtual int info(const string& file, vector<string>& strs)
    {
        uint8_t hdr[0x150] = {};
        int len = Emu::head(file, hdr, sizeof(hdr));
        if (len < 0)
            return -1;

        const string name = file.substr(file.find_last_of("/") + 1);
        strs.push_back(name);
        strs.push_back(dec_string((len + 1023) / 1024) + "k Game Boy ROM");
        strs.push_back("");

        const bool cgb = (hdr[0x143] & 0x80) != 0;
        char buf[64];
        sprintf(buf, "Mode:%s", cgb ? "GB/GBC" : "GB");
        strs.push_back(buf);
        sprintf(buf, "Mapper:%02X", hdr[0x147]);
        strs.push_back(buf);
        sprintf(buf, "ROM:%02X RAM:%02X", hdr[0x148], hdr[0x149]);
        strs.push_back(buf);
        return 0;
    }

    virtual int make_default_media(const string& path)
    {
#ifdef _WIN32
        _mkdir(path.c_str());
#else
        mkdir(path.c_str(), 0755);
#endif
        remove((path + "/postie.gbc").c_str());
        unpack((path + "/Postie.gbc").c_str(),postie_gbc,sizeof(postie_gbc));
        return 0;
    }

    virtual int insert(const string& path, int flags, int disk_index)
    {
        (void)flags;
        (void)disk_index;

        printf("walnutgb inserting %s\n", path.c_str());
        loading_progress_set(8);
        shutdown_cart();
        if (!init_screen()) {
            printf("walnutgb screen init failed for %s\n", path.c_str());
            return -1;
        }
        _front_buffer = 0;
        _lines = _frame_lines[_front_buffer];
        reset_audio_state();
        memset(&_gb, 0, sizeof(_gb));
        memset(&_apu, 0, sizeof(_apu));
        memset(_cgb_palette565, 0xFF, sizeof(_cgb_palette565));
        memset(_cgb_palette_rgb, 0, sizeof(_cgb_palette_rgb));
        _cgb_palette_dirty_mask = 0xFFFFFFFFFFFFFFFFull;
        minigb_apu_audio_init(&_apu);

        uint8_t hdr[0x150] = {};
        int len = head(path, hdr, sizeof(hdr));
        if (len <= 0) {
            printf("walnutgb can't open %s (head=%d)\n",
                   path.c_str(),
                   len);
            return -1;
        }
        loading_progress_set(12);
        printf("walnutgb %s is %d bytes header={%02X %02X %02X %02X} cart=%02X(%s) rom=%02X ram=%02X cgb=%02X\n",
               path.c_str(),
               len,
               hdr[0x100],
               hdr[0x101],
               hdr[0x102],
               hdr[0x103],
               hdr[0x147],
               gb_cart_type_name(hdr[0x147]),
               hdr[0x148],
               hdr[0x149],
               hdr[0x143]);

        _rom_path = path;
        _save_path = replace_ext(path, ".sav");
        _rtc_path = replace_ext(path, ".rtc");

        const size_t rom_len = (size_t)len;
        const size_t mapped_capacity = gb_mapped_rom_capacity();
        const size_t hybrid_mapped_len =
            min(rom_len, (mapped_capacity / kGbFixedRomBankSize) * kGbFixedRomBankSize);
        const bool prefer_mapped = rom_len <= mapped_capacity;
        bool rom_opened = false;

        if (prefer_mapped)
            rom_opened = open_mapped_rom(path, rom_len);
        if (!rom_opened && WALNUTGB_USE_HYBRID_ROM &&
            !prefer_mapped && hybrid_mapped_len >= kGbFixedRomBankSize)
            rom_opened = open_hybrid_rom(path, rom_len, hybrid_mapped_len);
        if (!rom_opened)
            rom_opened = open_banked_rom(path, rom_len);
        if (!rom_opened) {
            printf("walnutgb rom open failed for %s len=%u mapped_capacity=%u\n",
                   path.c_str(),
                   (unsigned)rom_len,
                   (unsigned)mapped_capacity);
            return -1;
        }
        loading_progress_set(82);

        enum gb_init_error_e ret = gb_init(&_gb,
                                           rom_read,
                                           rom_read16,
                                           rom_read32,
                                           cart_ram_read,
                                           cart_ram_write,
                                           error,
                                           this);
        if (ret != GB_INIT_NO_ERROR) {
            printf("walnutgb init failed %d (%s) for %s cart=%02X(%s) rom=%02X ram=%02X cgb=%02X\n",
                   ret,
                   gb_init_error_name(ret),
                   path.c_str(),
                   hdr[0x147],
                   gb_cart_type_name(hdr[0x147]),
                   hdr[0x148],
                   hdr[0x149],
                   hdr[0x143]);
            shutdown_cart();
            return -1;
        }
        loading_progress_set(88);
        const bool is_pocket_camera = hdr[0x147] == 0xFC;
        if (is_pocket_camera) {
            printf("walnutgb pocket camera support active: camera-mbc+ram camera=dummy-frame\n");
        }
        if (_rom && _rom_mapped_len > 0) {
            _gb.direct.rom_data = _rom;
            _gb.direct.rom_data_len = (uint32_t)min<size_t>(_rom_mapped_len, UINT32_MAX);
        } else {
            _gb.direct.rom_data = 0;
            _gb.direct.rom_data_len = 0;
        }

        _gb.direct.palette_changed = cgb_palette_changed;
        size_t probed_cart_ram_len = 0;
        if (gb_get_save_size_s(&_gb, &probed_cart_ram_len) < 0)
            probed_cart_ram_len = 0;
        _cart_ram_len = probed_cart_ram_len;
        load_rtc_state();
        load_save_ram();
        seed_pocket_camera_image_if_blank();
        loading_progress_set(94);
        gb_init_lcd(&_gb, lcd_draw_line);
        _gb.direct.interlace = false;
        _gb.direct.frame_skip = false;
        reset_video_palette_for_boot();
        configure_timing();
        loading_progress_set(97);
        const uint32_t prefill_presented_before = _presented_fps_frames;
        prefill();
        if (is_pocket_camera) {
            printf("walnutgb pocket camera prefill: presented=%u lcdc=%02X ly=%u pc=%04X bank=%02X cam0=%02X save_dirty=%u\n",
                   (unsigned)(_presented_fps_frames - prefill_presented_before),
                   (unsigned)_gb.hram_io[IO_LCDC],
                   (unsigned)_gb.hram_io[IO_LY],
                   (unsigned)_gb.cpu_reg.pc.reg,
                   (unsigned)_gb.cart_ram_bank,
                   (unsigned)_gb.pocket_camera_regs[0],
                   _save_dirty ? 1u : 0u);
        }
        loading_progress_set(100);
        printf("walnutgb inserted %s len=%u backend=%d save=%u cgb=%d\n",
               path.c_str(),
               (unsigned)_rom_len,
               (int)_rom_backend,
               (unsigned)_cart_ram_len,
               _gb.cgb.cgbMode ? 1 : 0);
        return 0;
    }

    virtual void hid(const uint8_t* d, int len)
    {
        (void)len;
        static const uint32_t common_map[16] = {
            0, 0, 0, JOYPAD_START,
            JOYPAD_LEFT, JOYPAD_RIGHT, JOYPAD_UP, JOYPAD_DOWN,
            0, 0, 0, JOYPAD_SELECT,
            JOYPAD_A, JOYPAD_B, JOYPAD_A, JOYPAD_B
        };

        static const uint32_t classic_map[16] = {
            JOYPAD_RIGHT, JOYPAD_DOWN, 0, JOYPAD_SELECT,
            0, JOYPAD_START, 0, 0,
            0, JOYPAD_B, 0, JOYPAD_A,
            0, 0, JOYPAD_LEFT, JOYPAD_UP
        };

        static const uint32_t generic_map_gb[16] = {
            0, 0, 0, 0,
            JOYPAD_A, JOYPAD_B, 0, 0,
            JOYPAD_START, JOYPAD_SELECT, JOYPAD_A, JOYPAD_RIGHT,
            JOYPAD_LEFT, JOYPAD_DOWN, JOYPAD_UP, 0
        };

        if (!has_rom())
            return;

        uint8_t mask = 0;
        if (d[0] == 0x42) {
            const uint16_t raw = (uint16_t)d[1] | ((uint16_t)d[2] << 8);
            mask = (uint8_t)generic_map(raw, generic_map_gb);
        } else if (d[0] == 0x30 || d[0] == 0x32) {
            mask = (uint8_t)wii_map(0, common_map, classic_map);
        } else {
            return;
        }
        apply_pad_mask(mask);
    }

    virtual void key(int keycode, int pressed, int mods)
    {
        (void)mods;
        if (!has_rom())
            return;

        uint8_t mask = (uint8_t)(~_gb.direct.joypad);
        const uint8_t bit = [keycode]() -> uint8_t {
            switch (keycode) {
                case 82: return JOYPAD_UP;
                case 81: return JOYPAD_DOWN;
                case 80: return JOYPAD_LEFT;
                case 79: return JOYPAD_RIGHT;
                case 225: return JOYPAD_A;
                case 226: return JOYPAD_B;
                case 40: return JOYPAD_START;
                case 43: return JOYPAD_SELECT;
                default: return 0;
            }
        }();

        if (keycode == 21 || keycode == 23 || keycode == 61 || keycode == 62) {
            if (pressed) {
                gb_reset(&_gb);
                reset_video_palette_for_boot();
            }
            return;
        }

        if (!bit)
            return;

        if (pressed)
            mask |= bit;
        else
            mask &= (uint8_t)~bit;
        apply_pad_mask(mask);
    }

    virtual int update()
    {
        const uint32_t update_start_us = micros();
        uint32_t host_frames = clamp_host_frame_delta(_host_frame_delta);
        _host_frame_delta = 1;
        add_audio_pending_frames(host_frames);

        const bool midi_active = gb_apu_midi_is_enabled();
        if (midi_active != _midi_audio_active) {
            reset_audio_state();
            audio_queue_clear();
            _midi_audio_active = midi_active;
        }

        if (midi_active) {
            _frame_accum_q16 += _host_frame_step_q16 * host_frames;
            int frames = 0;
            while (_frame_accum_q16 >= kQ16One && frames < 3) {
                _frame_accum_q16 -= kQ16One;
                ++frames;
            }
            return frames;
        }

        if (!has_rom() || _fatal_error) {
            return 0;
        }

        _frame_accum_q16 += _host_frame_step_q16 * host_frames;
        clamp_core_backlog();
        bool source_backpressure = audio_source_backpressure_active();
        if (source_backpressure && _frame_accum_q16 > kQ16One)
            _frame_accum_q16 = kQ16One;
        int frames = 0;
        uint32_t desired_frames =
            (uint32_t)((((uint64_t)_host_frame_step_q16 * host_frames) + kQ16One - 1u) >> 16);
        int max_frames = (int)desired_frames + 1;
        if (max_frames < 3)
            max_frames = 3;
        if (max_frames > WALNUTGB_MAX_CATCHUP_FRAMES)
            max_frames = WALNUTGB_MAX_CATCHUP_FRAMES;
        if (source_backpressure)
            max_frames = 1;
        bool line_effect_drawn_this_update = false;
        bool lcd_drawn_this_update = false;
        bool source_blocked = false;
        while (_frame_accum_q16 >= kQ16One && frames < max_frames) {
            if (!audio_source_frame_room_available()) {
                source_backpressure = true;
                source_blocked = true;
                break;
            }
            _frame_accum_q16 -= kQ16One;
            // Host-locked mode schedules LCD work at a fixed draw period.
#if WALNUTGB_ADAPTIVE_LCD_SKIP
            const bool catchup_frame = _frame_accum_q16 >= kQ16One;
            const bool scheduled_draw = should_draw_scheduled_frame();
            const bool line_effect_active = lcd_line_effect_active();
            bool skip_lcd;
            bool suppress_lcd_stat = false;
            if (line_effect_active) {
                const bool force_line_effect_draw =
                    _line_effect_skip_streak >=
                    min<uint32_t>(WALNUTGB_LINE_EFFECT_MAX_SKIP_FRAMES,
                                  UINT8_MAX);
                skip_lcd = catchup_frame || line_effect_drawn_this_update ||
                    (!scheduled_draw && !force_line_effect_draw);
                suppress_lcd_stat = skip_lcd;
            } else {
                // Draw before catch-up frames to preserve raster alignment.
                skip_lcd = lcd_drawn_this_update || !scheduled_draw;
            }
#else
            const bool capped_after_this = (frames + 1) >= max_frames;
            bool skip_lcd = !capped_after_this && _frame_accum_q16 >= kQ16One;
            bool suppress_lcd_stat = false;
#endif
            bool recovery_skip = false;
            if (!skip_lcd && _audio_recovery_cooldown == 0 &&
                audio_recovery_skip_needed()) {
                skip_lcd = true;
                recovery_skip = true;
            }
            const uint32_t max_consecutive_lcd_skip =
                _draw_period >= 3u ?
                    (uint32_t)WALNUTGB_DRAW_PERIOD_3_MAX_CONSECUTIVE_LCD_SKIP :
                    (uint32_t)WALNUTGB_MAX_CONSECUTIVE_LCD_SKIP;
            if (skip_lcd && max_consecutive_lcd_skip != 0 &&
                _lcd_skip_streak >= max_consecutive_lcd_skip) {
                skip_lcd = false;
                suppress_lcd_stat = false;
                recovery_skip = false;
            }
            run_frame_once(skip_lcd, suppress_lcd_stat && skip_lcd);
            if (skip_lcd) {
                if (_lcd_skip_streak < UINT32_MAX)
                    ++_lcd_skip_streak;
            } else {
                _lcd_skip_streak = 0;
                lcd_drawn_this_update = true;
            }
#if WALNUTGB_ADAPTIVE_LCD_SKIP
            if (line_effect_active) {
                if (skip_lcd) {
                    if (_line_effect_skip_streak < UINT8_MAX)
                        ++_line_effect_skip_streak;
                } else {
                    _line_effect_skip_streak = 0;
                    line_effect_drawn_this_update = true;
                }
            } else {
                _line_effect_skip_streak =
                    (uint8_t)min<uint32_t>(
                        WALNUTGB_LINE_EFFECT_MAX_SKIP_FRAMES, UINT8_MAX);
            }
#endif
            if (recovery_skip) {
                const uint32_t cooldown = WALNUTGB_AUDIO_RECOVERY_COOLDOWN_FRAMES;
                _audio_recovery_cooldown = (uint8_t)min<uint32_t>(cooldown, UINT8_MAX);
            } else if (_audio_recovery_cooldown > 0) {
                --_audio_recovery_cooldown;
            }
            ++frames;
            if (audio_source_backpressure_active()) {
                source_backpressure = true;
                break;
            }
        }
        const bool capped_frames = _frame_accum_q16 >= kQ16One;

        const uint32_t timing_update_us = micros() - update_start_us;
        if (!source_blocked)
            update_audio_speed_estimate(timing_update_us, frames);
        update_timing_adaptation(timing_update_us, host_frames, frames, capped_frames);

        const uint32_t now = millis();
        update_presented_fps(now);
        if (_save_dirty && (int32_t)(now - _save_deadline_ms) >= 0)
            flush_save(false);
        return frames;
    }

    virtual uint8_t** video_buffer()
    {
        if (!_lines)
            init_screen();
        return _lines;
    }

    virtual void set_host_frame_delta(int frames)
    {
        _host_frame_delta = clamp_host_frame_delta((uint32_t)frames);
    }

    virtual int audio_buffer(int16_t* b, int max_len)
    {
        const int max_samples = max_len > 0 ? max_len / (int)sizeof(b[0]) : 0;
        if (max_samples <= 0)
            return 0;

        const bool midi_low_latency = midi_audio_low_latency();
        uint32_t audio_frames = midi_low_latency ? 1u : _audio_frame_delta;
        if (audio_frames < 1)
            audio_frames = 1;
        if (audio_frames > WALNUTGB_AUDIO_MAX_FRAME_DELTA)
            audio_frames = WALNUTGB_AUDIO_MAX_FRAME_DELTA;

        const int safe_frame_samples = audio_output_safe_frame_samples();
        const int max_correction = audio_queue_max_correction_samples();
        while (audio_frames > 1 &&
               ((int)audio_frames * safe_frame_samples +
                max_correction) > max_samples)
            --audio_frames;
        if (midi_low_latency) {
            // Live MIDI renders one host frame; queue refill covers missed callbacks.
            _audio_frame_delta = 0;
        } else if (_audio_frame_delta > audio_frames) {
            _audio_frame_delta -= audio_frames;
        } else {
            _audio_frame_delta = 0;
        }

        int base_n = 0;
        for (uint32_t i = 0; i < audio_frames; ++i)
            base_n += gb_output_frame_sample_count();

        int n = base_n + audio_queue_feedback_samples(base_n);
        if (n < 1)
            n = 1;
        if (n > max_samples)
            n = max_samples;
        n = audio_queue_write_room_limit(n);
        if (n <= 0)
            return 0;
        if (midi_low_latency) {
            _resample_step_correction_q16 = 0;
            render_midi_audio_direct(b, n);
            return n;
        }
        update_audio_ring_step_correction();
        for (int i = 0; i < n; ++i)
            b[i] = resample_audio_sample();
        return n;
    }

    virtual const uint32_t* ntsc_palette() { return g_rgb332_palette; }
    virtual const uint32_t* pal_palette() { return g_rgb332_palette; }
    virtual const uint32_t* rgb_palette() { return g_rgb332_palette; }
};

extern "C" void gb_palette_glitch_set_enabled(int enabled)
{
    if (g_active_gb_emu)
        g_active_gb_emu->palette_glitch_set_enabled(enabled);
}

extern "C" void gb_palette_glitch_set_game_active(int active)
{
    if (g_active_gb_emu)
        g_active_gb_emu->palette_glitch_set_game_active(active);
}

extern "C" void gb_palette_glitch_cycle(void)
{
    if (g_active_gb_emu)
        g_active_gb_emu->palette_glitch_cycle();
}

Emu* NewGameBoy(int ntsc)
{
    return new EmuGameBoy(ntsc);
}
