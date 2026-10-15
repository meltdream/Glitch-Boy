
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

#include <stdint.h>

#define VIDEO_PIN   25

#ifndef IR_INPUT_ENABLED
#define IR_INPUT_ENABLED 0
#endif

#if IR_INPUT_ENABLED && !defined(IR_PIN)
#define IR_PIN      0   // TSOP4838 or equivalent on any pin if desired
#endif

// MCP4822 is the primary output; PWM remains as a build-time fallback.
#define AUDIO_PIN   18
#define AUDIO_PWM_CHANNEL 0
#define AUDIO_PWM_FREQ_HZ 625000
#define AUDIO_PWM_BITS 7

#ifndef AUDIO_MCP4822_OUTPUT
#define AUDIO_MCP4822_OUTPUT 1
#endif

#ifndef AUDIO_MCP4822_PIN_CS
#define AUDIO_MCP4822_PIN_CS 18
#endif

#ifndef AUDIO_MCP4822_PIN_SCK
#define AUDIO_MCP4822_PIN_SCK 2
#endif

#ifndef AUDIO_MCP4822_PIN_SDI
#define AUDIO_MCP4822_PIN_SDI 12
#endif

#ifndef AUDIO_MCP4822_GAIN_1X
#define AUDIO_MCP4822_GAIN_1X 0
#endif

#ifndef AUDIO_MCP4822_SPI_HZ
#define AUDIO_MCP4822_SPI_HZ 20000000u
#endif

#ifndef AUDIO_MCP4822_SUPPLY_MV
#define AUDIO_MCP4822_SUPPLY_MV 3300u
#endif

#ifndef AUDIO_MCP4822_VREF_MV
#define AUDIO_MCP4822_VREF_MV 2048u
#endif

// Three-wire MCP4822 mode: tie LDAC low and SHDN high in hardware.
// DAC A only; DAC B is unused.
#if AUDIO_MCP4822_OUTPUT && \
    (AUDIO_MCP4822_PIN_CS >= 32 || AUDIO_MCP4822_PIN_SCK >= 32 || AUDIO_MCP4822_PIN_SDI >= 32)
#error AUDIO_MCP4822_OUTPUT direct GPIO path currently supports GPIO 0..31 only.
#endif

#ifndef AUDIO_OUTPUT_GAIN_NUM
#if AUDIO_MCP4822_OUTPUT
#define AUDIO_OUTPUT_GAIN_NUM 1
#else
#define AUDIO_OUTPUT_GAIN_NUM 2
#endif
#endif

#ifndef AUDIO_OUTPUT_GAIN_DEN
#if AUDIO_MCP4822_OUTPUT
#define AUDIO_OUTPUT_GAIN_DEN 1
#else
#define AUDIO_OUTPUT_GAIN_DEN 3
#endif
#endif

#ifndef AUDIO_GB_ADAPTIVE_STRETCH
#define AUDIO_GB_ADAPTIVE_STRETCH 0
#endif

#ifndef AUDIO_STRETCH_TARGET
#define AUDIO_STRETCH_TARGET 768
#endif

#ifndef AUDIO_OUTPUT_QUEUE_THINNING
#define AUDIO_OUTPUT_QUEUE_THINNING 1
#endif

#ifndef AUDIO_OUTPUT_THIN_HEADROOM
#define AUDIO_OUTPUT_THIN_HEADROOM 256u
#endif

#ifndef AUDIO_OUTPUT_THIN_MIN_PERIOD
#define AUDIO_OUTPUT_THIN_MIN_PERIOD 8u
#endif

#ifndef AUDIO_OUTPUT_THIN_MAX_PERIOD
#define AUDIO_OUTPUT_THIN_MAX_PERIOD 512u
#endif

#ifndef AUDIO_BASS_BOOST
#define AUDIO_BASS_BOOST 1
#endif

#ifndef AUDIO_BASS_LOWPASS_SHIFT
#define AUDIO_BASS_LOWPASS_SHIFT 4
#endif

#ifndef AUDIO_BASS_GAIN_Q8
#define AUDIO_BASS_GAIN_Q8 455  // +5 dB voltage gain: 10^(5/20) * 256
#endif

#ifndef AUDIO_OUTPUT_ANTIALIAS
#define AUDIO_OUTPUT_ANTIALIAS 1
#endif

#ifndef AUDIO_OUTPUT_ANTIALIAS_ALPHA_Q8
#define AUDIO_OUTPUT_ANTIALIAS_ALPHA_Q8 224
#endif

#ifndef AUDIO_SOFT_LIMIT
#define AUDIO_SOFT_LIMIT 1
#endif

#ifndef AUDIO_SOFT_LIMIT_KNEE
#define AUDIO_SOFT_LIMIT_KNEE (56 * 256)
#endif

#if AUDIO_MCP4822_OUTPUT
typedef uint16_t audio_output_sample_t;
#define AUDIO_OUTPUT_UNDERRUN_STEP 32u
#else
typedef uint8_t audio_output_sample_t;
#define AUDIO_OUTPUT_SILENCE_SAMPLE 0x40u
#define AUDIO_OUTPUT_UNDERRUN_STEP 1u
#endif

#if AUDIO_MCP4822_OUTPUT
#if AUDIO_MCP4822_GAIN_1X
#define AUDIO_MCP4822_CLEAN_TOP_CODE 4095u
#else
#define AUDIO_MCP4822_CLEAN_TOP_CODE \
  ((((AUDIO_MCP4822_SUPPLY_MV) * 4096u) + (AUDIO_MCP4822_VREF_MV)) / \
   (2u * (AUDIO_MCP4822_VREF_MV)))
#endif
#if AUDIO_MCP4822_CLEAN_TOP_CODE > 4095u
#undef AUDIO_MCP4822_CLEAN_TOP_CODE
#define AUDIO_MCP4822_CLEAN_TOP_CODE 4095u
#endif
#if AUDIO_MCP4822_GAIN_1X
#define AUDIO_OUTPUT_SILENCE_SAMPLE 2048u
#define AUDIO_MCP4822_OUTPUT_SPAN 2047u
#else
#define AUDIO_OUTPUT_SILENCE_SAMPLE ((uint16_t)(AUDIO_MCP4822_CLEAN_TOP_CODE / 2u))
#define AUDIO_MCP4822_OUTPUT_SPAN ((uint16_t)(AUDIO_OUTPUT_SILENCE_SAMPLE))
#endif
#endif

#ifdef ESP_PLATFORM
#include "esp_types.h"
#include "esp_heap_caps.h"
#include "esp_attr.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "esp_intr_alloc.h"
#include "esp_err.h"
#include "soc/gpio_reg.h"
#include "soc/rtc.h"
#include "soc/soc.h"
#include "soc/i2s_struct.h"
#include "soc/i2s_reg.h"
#include "soc/ledc_struct.h"
#if AUDIO_MCP4822_OUTPUT
#include "soc/gpio_sig_map.h"
#include "soc/spi_struct.h"
#endif
#include "soc/timer_group_struct.h"
#include "soc/rtc_io_reg.h"
#include "soc/io_mux_reg.h"
#include "rom/gpio.h"
#include "rom/lldesc.h"
#include "driver/periph_ctrl.h"
#include "driver/dac.h"
#include "driver/gpio.h"
#include "driver/i2s.h"
#include "driver/timer.h"

#ifndef VIDEO_ISR_FLAGS
// Audio may preempt video to avoid scanline-length DAC jitter. Its ISR only
// dequeues one sample and starts a 16-bit SPI transfer.
#define VIDEO_ISR_FLAGS (ESP_INTR_FLAG_LEVEL2 | ESP_INTR_FLAG_IRAM)
#endif

#ifndef AUDIO_TIMER_ISR_FLAGS
#define AUDIO_TIMER_ISR_FLAGS (ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_IRAM)
#endif

int DRAM_ATTR _pal_ = 0;

#if IR_INPUT_ENABLED
#include "ir_input.h"  // ir peripherals
#endif


//====================================================================================================
//====================================================================================================
//
// low level HW setup of DAC/DMA/APLL/PWM
//

#ifndef VIDEO_DMA_LINE_DEPTH
#define VIDEO_DMA_LINE_DEPTH 4
#endif

static_assert(VIDEO_DMA_LINE_DEPTH >= 2,
              "VIDEO_DMA_LINE_DEPTH must provide at least two scanlines");
DRAM_ATTR lldesc_t _dma_desc[VIDEO_DMA_LINE_DEPTH] = {};
intr_handle_t _isr_handle;

extern "C"
void IRAM_ATTR video_isr(volatile void* buf);
extern int _machine;
static void audio_output_timer_start(bool enable, int ntsc);
static void audio_output_timer_stop();
#if AUDIO_MCP4822_OUTPUT
static void audio_mcp4822_init();
#endif

// Minimal ISR latency overrun detector (counts long spans between IRQs)
extern "C" uint32_t xthal_get_ccount(void);

#if AUDIO_TIMER_OUTPUT_ENABLED
static timer_isr_handle_t _audio_timer_handle = 0;
volatile bool DRAM_ATTR _audio_timer_active = false;
#endif

// simple isr
void IRAM_ATTR i2s_intr_handler_video(void *arg)
{

    const uint32_t status = I2S0.int_st.val;
    const bool out_eof = (status & I2S_OUT_EOF_INT_ST) != 0;
    lldesc_t* eof_desc = (lldesc_t*)I2S0.out_eof_des_addr;
    I2S0.int_clr.val = status;

    if (out_eof && eof_desc && eof_desc->buf)
        video_isr(eof_desc->buf); // get the next line of video
}

static esp_err_t start_dma(int line_width,int samples_per_cc, int ch = 1)
{
    periph_module_enable(PERIPH_I2S0_MODULE);

    // setup interrupt
    if (esp_intr_alloc(ETS_I2S0_INTR_SOURCE, VIDEO_ISR_FLAGS,
        i2s_intr_handler_video, 0, &_isr_handle) != ESP_OK)
        return -1;

    // reset conf
    I2S0.conf.val = 1;
    I2S0.conf.val = 0;
    I2S0.conf.tx_right_first = 1;
    I2S0.conf.tx_mono = (ch == 2 ? 0 : 1);

    I2S0.conf2.lcd_en = 1;
    I2S0.fifo_conf.tx_fifo_mod_force_en = 1;
    I2S0.sample_rate_conf.tx_bits_mod = 16;
    I2S0.conf_chan.tx_chan_mod = (ch == 2) ? 0 : 1;

    // NES scanline synthesis has the smallest timing margin. Keep the original
    // two-line footprint for the other machines.
    const int dma_line_depth =
        (_machine == EMU_NES || _machine == EMU_NES6)
            ? VIDEO_DMA_LINE_DEPTH
            : 2;

    // Create TX DMA buffers
    for (int i = 0; i < dma_line_depth; i++) {
        int n = line_width*2*ch;
        if (n >= 4092) {
            printf("DMA chunk too big:%d\n",n);
            return -1;
        }
        _dma_desc[i].buf = (uint8_t*)heap_caps_calloc(1, n,
            MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (!_dma_desc[i].buf)
            return -1;

        _dma_desc[i].owner = 1;
        _dma_desc[i].eof = 1;
        _dma_desc[i].length = n;
        _dma_desc[i].size = n;
        _dma_desc[i].empty =
            (uint32_t)&_dma_desc[(i + 1) % dma_line_depth];
    }
    I2S0.out_link.addr = (uint32_t)_dma_desc;

    //  Setup up the APLL: See ESP-IDF TRM 3.2.7 Audio PLL
    //  APLL fields vary across IDF versions. Recheck these values after upgrades.
    //  These constants target colorburst multiples (~3x/4x NTSC, ~4x PAL).
    //  f_xtal = (int)rtc_clk_xtal_freq_get() * 1000000;
    //  f_out = xtal_freq * (4 + sdm2 + sdm1/256 + sdm0/65536); // 250 < f_out < 500
    //  apll_freq = f_out/((o_div + 2) * 2)
    //  operating range of the f_out is 250 MHz ~ 500 MHz
    //  operating range of the apll_freq is 16 ~ 128 MHz.
    //  select sdm0,sdm1,sdm2 to produce nice multiples of colorburst frequencies

    //  see calc_freq() for math: (4+a)*10/((2 + b)*2) mhz
    if (!_pal_) {
        switch (samples_per_cc) {
            case 3: rtc_clk_apll_enable(1,0x46,0x97,0x4,2);   break;    // ~10.738636 MHz (≈3x NTSC)
            case 4: rtc_clk_apll_enable(1,0x46,0x97,0x4,1);   break;    // ~14.318182 MHz (≈4x NTSC)
        }
    } else {
        rtc_clk_apll_enable(1,0x04,0xA4,0x6,1);     // ~17.734476 MHz (≈4x PAL)
    }

    I2S0.clkm_conf.clkm_div_num = 1;            // I2S clock divider’s integral value.
    I2S0.clkm_conf.clkm_div_b = 0;              // Fractional clock divider’s numerator value.
    I2S0.clkm_conf.clkm_div_a = 1;              // Fractional clock divider’s denominator value
    I2S0.sample_rate_conf.tx_bck_div_num = 1;
    I2S0.clkm_conf.clka_en = 1;                 // Set this bit to enable clk_apll.
    I2S0.fifo_conf.tx_fifo_mod = (ch == 2) ? 0 : 1; // 32-bit dual or 16-bit single channel data

    dac_output_enable(DAC_CHANNEL_1);           // DAC, video on GPIO25
    dac_i2s_enable();                           // start DAC!

    I2S0.conf.tx_start = 1;                     // start DMA!
    I2S0.int_clr.val = 0xFFFFFFFF;
    I2S0.int_ena.out_eof = 1;
    I2S0.out_link.start = 1;
    return esp_intr_enable(_isr_handle);        // start interruprs!
}

void video_init_hw(int line_width, int samples_per_cc)
{
#if AUDIO_MCP4822_OUTPUT
    audio_mcp4822_init();
#endif

    // setup apll 4x NTSC or PAL colorburst rate
    start_dma(line_width,samples_per_cc,1);

    // PWM fallback only. The PCB uses MCP4822 on CS=18, SCK=2, SDI=12.

#if !AUDIO_MCP4822_OUTPUT
    double audio_pwm_actual_hz = ledcSetup(AUDIO_PWM_CHANNEL,AUDIO_PWM_FREQ_HZ,AUDIO_PWM_BITS);
    ledcAttachPin(AUDIO_PIN, AUDIO_PWM_CHANNEL);
    ledcWrite(AUDIO_PWM_CHANNEL,0);
    {
        auto& audio_pwm = LEDC.channel_group[0].channel[AUDIO_PWM_CHANNEL];
        audio_pwm.conf0.sig_out_en = 1;
        audio_pwm.conf0.clk_en = 1;
    }
#endif

    audio_output_timer_start(
#if AUDIO_TIMER_OUTPUT_ENABLED && AUDIO_OUTPUT_2X_SCANLINE
        ((_machine == EMU_GB && (AUDIO_DECOUPLED_TIMER != 0)) ||
         ((_machine == EMU_NES || _machine == EMU_NES6) &&
          (AUDIO_NES_2X_SCANLINE != 0) && (AUDIO_NES_TIMER_OUTPUT != 0))),
#else
        false,
#endif
        !_pal_);

    //  IR input if used
#if IR_INPUT_ENABLED
    pinMode(IR_PIN,INPUT);
#endif
}

#if AUDIO_MCP4822_OUTPUT
#if AUDIO_MCP4822_GAIN_1X
#define AUDIO_MCP4822_GAIN_BIT 0x2000u
#else
#define AUDIO_MCP4822_GAIN_BIT 0x0000u
#endif
#define AUDIO_MCP4822_CMD_DAC_A 0x0000u
#define AUDIO_MCP4822_CMD_ACTIVE 0x1000u
#define AUDIO_MCP4822_CMD_BASE (AUDIO_MCP4822_CMD_DAC_A | AUDIO_MCP4822_GAIN_BIT | AUDIO_MCP4822_CMD_ACTIVE)

static inline uint16_t IRAM_ATTR __attribute__((always_inline)) audio_mcp4822_fifo_word(uint16_t word)
{
    return (uint16_t)((word >> 8) | (word << 8));
}

static inline void IRAM_ATTR __attribute__((always_inline)) audio_mcp4822_write_word(uint16_t word);

static void audio_mcp4822_init()
{
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << (gpio_num_t)AUDIO_MCP4822_PIN_CS) |
                           (1ULL << (gpio_num_t)AUDIO_MCP4822_PIN_SCK) |
                           (1ULL << (gpio_num_t)AUDIO_MCP4822_PIN_SDI);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level((gpio_num_t)AUDIO_MCP4822_PIN_CS, 1);
    gpio_set_level((gpio_num_t)AUDIO_MCP4822_PIN_SCK, 0);
    gpio_set_level((gpio_num_t)AUDIO_MCP4822_PIN_SDI, 0);

    periph_module_enable(PERIPH_VSPI_MODULE);

    SPI3.slave.trans_done = 0;
    SPI3.slave.slave_mode = 0;
    SPI3.pin.val = 0;
    SPI3.pin.cs1_dis = 1;
    SPI3.pin.cs2_dis = 1;
    SPI3.user.val = 0;
    SPI3.user.usr_mosi = 1;
    SPI3.user.cs_setup = 1;
    SPI3.user.cs_hold = 1;
    SPI3.user1.val = 0;
    SPI3.user2.val = 0;
    SPI3.ctrl.val = 0;
    SPI3.ctrl1.val = 0;
    SPI3.ctrl2.val = 0;
    SPI3.mosi_dlen.usr_mosi_dbitlen = 15;
    SPI3.miso_dlen.usr_miso_dbitlen = 0;
    for (int i = 0; i < 16; ++i)
        SPI3.data_buf[i] = 0;

    uint32_t div = APB_CLK_FREQ / (AUDIO_MCP4822_SPI_HZ ? AUDIO_MCP4822_SPI_HZ : 1u);
    if (div < 2)
        div = 2;
    if (div > 64)
        div = 64;
    SPI3.clock.val = 0;
    SPI3.clock.clkdiv_pre = 0;
    SPI3.clock.clkcnt_n = div - 1;
    SPI3.clock.clkcnt_h = (div / 2) - 1;
    SPI3.clock.clkcnt_l = div - 1;

    gpio_matrix_out((uint32_t)AUDIO_MCP4822_PIN_SCK, VSPICLK_OUT_IDX, false, false);
    gpio_matrix_out((uint32_t)AUDIO_MCP4822_PIN_SDI, VSPID_OUT_IDX, false, false);
    gpio_matrix_out((uint32_t)AUDIO_MCP4822_PIN_CS, VSPICS0_OUT_IDX, false, false);

    audio_mcp4822_write_word((uint16_t)(AUDIO_MCP4822_CMD_BASE | AUDIO_OUTPUT_SILENCE_SAMPLE));
}

static inline void IRAM_ATTR __attribute__((always_inline)) audio_mcp4822_write_word(uint16_t word)
{
    // SPI3 is dedicated to the DAC. Finish the preceding sub-microsecond
    // transfer rather than dropping an audio sample.
    while (SPI3.cmd.usr)
        __asm__ __volatile__("nop");
    SPI3.data_buf[0] = audio_mcp4822_fifo_word(word);
    SPI3.cmd.usr = 1;
}
#endif

// Send an audio sample every scanline (~15734 Hz for NTSC, 15600 Hz for PAL).
static inline void IRAM_ATTR __attribute__((always_inline)) audio_sample(audio_output_sample_t s)
{
#if AUDIO_MCP4822_OUTPUT
    audio_mcp4822_write_word((uint16_t)(AUDIO_MCP4822_CMD_BASE | (s & 0x0FFFu)));
#else
    auto& reg = LEDC.channel_group[0].channel[AUDIO_PWM_CHANNEL];
    reg.duty.duty = s << 4; // 25 bit (21.4)
    reg.conf1.duty_start = 1; // When duty_num duty_cycle and duty_scale has been configured. these register won't take effect until set duty_start. this bit is automatically cleared by hardware
#endif
}

extern "C"
void* MALLOC32(int x, const char* label)
{
    printf("MALLOC32 %d free, %d biggest, allocating %s:%d\n",
      heap_caps_get_free_size(MALLOC_CAP_32BIT),heap_caps_get_largest_free_block(MALLOC_CAP_32BIT),label,x);
    void * r = heap_caps_malloc(x,MALLOC_CAP_32BIT);
    if (!r) {
        printf("MALLOC32 FAILED allocation of %s:%d!!!!####################\n",label,x);
        esp_restart();
    }
    else
        printf("MALLOC32 allocation of %s:%d %08X\n",label,x,r);
    return r;
}

#if !IR_INPUT_ENABLED
int get_hid_ir(uint8_t* buf)
{
    (void)buf;
    return 0;
}
#endif

#else

//====================================================================================================
//====================================================================================================
//  Simulator
//

#define IRAM_ATTR
#define DRAM_ATTR

int _pal_ = 0;

void video_init_hw(int line_width, int samples_per_cc);

uint32_t xthal_get_ccount() {
    unsigned int lo,hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return lo;
}

void audio_sample(audio_output_sample_t s);

void ir_sample();

int get_hid_ir(uint8_t* buf)
{
    return 0;
}

#endif

//====================================================================================================
//====================================================================================================


uint32_t cpu_ticks()
{
  return xthal_get_ccount();
}

uint32_t us() {
    return cpu_ticks()/240;
}

// Color clock frequency is 315/88 (3.57954545455)
// DAC_MHZ is 315/11 or 8x color clock
// 455/2 color clocks per line, round up to maintain phase
// HSYNCH period is 44/315*455 or 63.55555..us
// Field period is 262*44/315*455 or 16651.5555us

#define IRE(_x)          ((uint32_t)(((_x)+40)*255/3.3/147.5) << 8)   // 3.3V DAC
#define SYNC_LEVEL       IRE(-40)
#define BLANKING_LEVEL   IRE(0)
#define BLACK_LEVEL      IRE(7.5)
#define GRAY_LEVEL       IRE(50)
#define WHITE_LEVEL      IRE(100)


#define P0 (color >> 16)
#define P1 (color >> 8)
#define P2 (color)
#define P3 (color << 8)

uint8_t** DRAM_ATTR _lines; // filled in by emulator
volatile int DRAM_ATTR _line_counter = 0;
// Chroma phase selection uses parity captured at ISR entry.
volatile uint8_t DRAM_ATTR _line_parity_even = 0;
uint8_t DRAM_ATTR _ntsc_phase_banks_ready = 0;
volatile int DRAM_ATTR _frame_counter = 0;
volatile int _last_sync_frame = -1;
volatile int _last_pal_nes_render_frame = -1;
#ifndef GB_DIRECT_PRESENT_QUEUE_DEPTH
#define GB_DIRECT_PRESENT_QUEUE_DEPTH 1
#endif
#if (GB_DIRECT_PRESENT_QUEUE_DEPTH & (GB_DIRECT_PRESENT_QUEUE_DEPTH - 1))
#error GB_DIRECT_PRESENT_QUEUE_DEPTH must be a power of two.
#endif
static const uint32_t kVideoQueuedLineDepth = GB_DIRECT_PRESENT_QUEUE_DEPTH;
uint8_t** DRAM_ATTR _queued_lines[kVideoQueuedLineDepth] = {0};
volatile uint32_t DRAM_ATTR _queued_lines_r = 0;
volatile uint32_t DRAM_ATTR _queued_lines_w = 0;

int DRAM_ATTR _active_lines;
int DRAM_ATTR _line_count;
int DRAM_ATTR _pal_active_first_line;

int DRAM_ATTR _line_width;
int DRAM_ATTR _samples_per_cc;
int DRAM_ATTR _machine; // 2:1 3:2 4:3 3:4 input pixel to color clock ratio
const uint32_t* DRAM_ATTR _palette;
#ifdef ESP_PLATFORM
uint32_t* DRAM_ATTR _palette_ram = 0;
size_t _palette_ram_len = 0;
size_t DRAM_ATTR _palette_active_len = 0;
static bool _nes_visualizer_palette_active = false;
static const uint8_t _nes_visualizer_palette_indices[4] = {
    0x0F, 0x1F, 0x2F, 0x3F
};
static uint32_t _nes_visualizer_palette_saved_even[4] = {0, 0, 0, 0};
static uint32_t _nes_visualizer_palette_saved_odd[4] = {0, 0, 0, 0};
#endif
static int _palette_glitch_enabled = 0;
static int _palette_glitch_game_active = 1;

#ifdef __cplusplus
extern "C" {
#endif
void ppu_palette_glitch_set_enabled(int enabled);
void ppu_palette_glitch_set_game_active(int active);
void ppu_palette_glitch_cycle(void);
void gb_palette_glitch_set_enabled(int enabled);
void gb_palette_glitch_set_game_active(int active);
void gb_palette_glitch_cycle(void);
#ifdef __cplusplus
}
#endif

#ifdef ESP_PLATFORM
static inline uint8_t video_clamp_sample_u8(int sample)
{
    if (sample < 0)
        return 0;
    if (sample > 255)
        return 255;
    return (uint8_t)sample;
}

static inline uint32_t video_pack_palette_samples(uint8_t s0, uint8_t s1,
                                                  uint8_t s2, uint8_t s3)
{
    return ((uint32_t)s0 << 24) | ((uint32_t)s1 << 16) |
           ((uint32_t)s2 << 8) | (uint32_t)s3;
}

static inline uint32_t video_invert_ntsc_phase(uint32_t color)
{
    return (color << 16) | (color >> 16);
}

static inline uint32_t video_encode_ntsc_rgb888(uint32_t rgb)
{
    const float r = (float)((rgb >> 16) & 0xFF);
    const float g = (float)((rgb >> 8) & 0xFF);
    const float b = (float)(rgb & 0xFF);
    const float y_luma = (0.299f * r) + (0.587f * g) + (0.114f * b);
    const float i = (0.596f * r) - (0.275f * g) - (0.321f * b);
    const float q = (0.212f * r) - (0.523f * g) + (0.311f * b);
    const float phase = atan2f(i, q);
    // Same chroma gain as the PAL encoder (~69% of nominal, which also suits
    // GBC colours made for a washed-out LCD). It was /127 (~43%), so NTSC
    // looked much paler than PAL.
    const float saturation = sqrtf((i * i) + (q * q)) / 80.0f;
    const float base = (y_luma / 255.0f) * (float)(WHITE_LEVEL - BLACK_LEVEL) +
                       (float)BLACK_LEVEL;
    // Phase of the (I,Q) vector relative to this file's burst ([0,+A,0,-A],
    // i.e. sin at sample 0; burst = 180 deg on the B-Y axis): Q leads B-Y by
    // 33 deg, so offset = 180 + 33 = 213 deg. It was 240 deg, which rotated
    // every NTSC GB colour by +27 deg (verified by encode/decode simulation).
    const float offset = 213.0f * (float)M_PI / 180.0f;

    int s0 = (int)(base + 0.5f);
    int s1 = s0;
    int s2 = s0;
    int s3 = s0;
    if (!(((uint8_t)r == (uint8_t)g) && ((uint8_t)g == (uint8_t)b))) {
        const float chroma = ((float)BLANKING_LEVEL * 0.5f) * saturation;
        s0 = (int)(base + sinf(phase + offset + (2.0f * (float)M_PI * 0.0f / 4.0f)) * chroma + 0.5f);
        s1 = (int)(base + sinf(phase + offset + (2.0f * (float)M_PI * 1.0f / 4.0f)) * chroma + 0.5f);
        s2 = (int)(base + sinf(phase + offset + (2.0f * (float)M_PI * 2.0f / 4.0f)) * chroma + 0.5f);
        s3 = (int)(base + sinf(phase + offset + (2.0f * (float)M_PI * 3.0f / 4.0f)) * chroma + 0.5f);
    }

    s0 >>= 8;
    s1 >>= 8;
    s2 >>= 8;
    s3 >>= 8;

    return video_pack_palette_samples(video_clamp_sample_u8(s0),
                                      video_clamp_sample_u8(s1),
                                      video_clamp_sample_u8(s2),
                                      video_clamp_sample_u8(s3));
}

static inline void video_encode_pal_rgb888(uint32_t rgb, uint32_t* even, uint32_t* odd)
{
    const float r = (float)((rgb >> 16) & 0xFF);
    const float g = (float)((rgb >> 8) & 0xFF);
    const float b = (float)(rgb & 0xFF);
    // PAL has no 7.5 IRE setup: black sits at blanking. Using NTSC's
    // BLACK_LEVEL made PAL blacks dark grey.
    const float y = (((0.299f * r) + (0.587f * g) + (0.114f * b)) *
                     (float)(WHITE_LEVEL - BLANKING_LEVEL) / 255.0f +
                     (float)BLANKING_LEVEL) / 256.0f;
    const float u = (-0.147407f * r) - (0.289391f * g) + (0.436798f * b);
    const float v = (0.614777f * r) - (0.514799f * g) - (0.099978f * b);
    const float chroma = (((float)BLANKING_LEVEL * 0.5f) / 256.0f) / 80.0f;

    const int e0 = (int)(y - (chroma * v) + 0.5f);
    const int e1 = (int)(y - (chroma * u) + 0.5f);
    const int e2 = (int)(y + (chroma * v) + 0.5f);
    const int e3 = (int)(y + (chroma * u) + 0.5f);

    const int o0 = (int)(y + (chroma * v) + 0.5f);
    const int o1 = (int)(y - (chroma * u) + 0.5f);
    const int o2 = (int)(y - (chroma * v) + 0.5f);
    const int o3 = (int)(y + (chroma * u) + 0.5f);

    if (even)
        *even = video_pack_palette_samples(video_clamp_sample_u8(e0),
                                           video_clamp_sample_u8(e1),
                                           video_clamp_sample_u8(e2),
                                           video_clamp_sample_u8(e3));
    if (odd)
        *odd = video_pack_palette_samples(video_clamp_sample_u8(o0),
                                          video_clamp_sample_u8(o1),
                                          video_clamp_sample_u8(o2),
                                          video_clamp_sample_u8(o3));
}

static inline void video_encode_gb_palette_rgb888(const uint32_t* rgb_palette,
                                                  uint32_t* encoded,
                                                  size_t encoded_len,
                                                  bool ntsc)
{
    if (!rgb_palette || !encoded)
        return;

    for (int i = 0; i < 256; ++i) {
        const uint32_t rgb = rgb_palette[i];
        if (ntsc) {
            encoded[i] = video_encode_ntsc_rgb888(rgb);
            if (encoded_len >= 512)
                encoded[i + 256] = video_invert_ntsc_phase(encoded[i]);
        } else {
            uint32_t even = 0;
            uint32_t odd = 0;
            video_encode_pal_rgb888(rgb, &even, &odd);
            encoded[i] = even;
            if (encoded_len >= 512)
                encoded[i + 256] = odd;
        }
    }
}

static inline bool video_palette_glitch_uses_ppu_palette(int machine)
{
    return machine == EMU_NES || machine == EMU_NES6;
}

static inline bool video_palette_glitch_uses_gb_palette(int machine)
{
    return machine == EMU_GB;
}

void video_palette_glitch_set_enabled(int enabled)
{
    _palette_glitch_enabled = enabled ? 1 : 0;
    if (video_palette_glitch_uses_ppu_palette(_machine))
        ppu_palette_glitch_set_enabled(_palette_glitch_enabled);
    if (video_palette_glitch_uses_gb_palette(_machine))
        gb_palette_glitch_set_enabled(_palette_glitch_enabled);
}

int video_palette_glitch_get_enabled()
{
    return _palette_glitch_enabled ? 1 : 0;
}

void video_palette_glitch_set_game_active(int active)
{
    const int next = active ? 1 : 0;
    _palette_glitch_game_active = next;
    if (video_palette_glitch_uses_ppu_palette(_machine))
        ppu_palette_glitch_set_game_active(_palette_glitch_game_active);
    if (video_palette_glitch_uses_gb_palette(_machine))
        gb_palette_glitch_set_game_active(_palette_glitch_game_active);
}

void video_palette_glitch_cycle()
{
    if (!_palette_glitch_enabled || !_palette_glitch_game_active)
        return;
    if (video_palette_glitch_uses_ppu_palette(_machine))
        ppu_palette_glitch_cycle();
    else if (video_palette_glitch_uses_gb_palette(_machine))
        gb_palette_glitch_cycle();
}

static inline void video_palette_glitch_sync_machine_state()
{
    if (video_palette_glitch_uses_ppu_palette(_machine)) {
        ppu_palette_glitch_set_enabled(_palette_glitch_enabled);
        ppu_palette_glitch_set_game_active(_palette_glitch_game_active);
    }
    if (video_palette_glitch_uses_gb_palette(_machine)) {
        gb_palette_glitch_set_enabled(_palette_glitch_enabled);
        gb_palette_glitch_set_game_active(_palette_glitch_game_active);
    }
}
#else
void video_palette_glitch_set_enabled(int enabled) { (void)enabled; }
int video_palette_glitch_get_enabled() { return 0; }
void video_palette_glitch_set_game_active(int active) { (void)active; }
void video_palette_glitch_cycle() {}
static inline void video_palette_glitch_sync_machine_state() {}
#endif

float _sample_rate;

int DRAM_ATTR _hsync;
int DRAM_ATTR _hsync_long;
int DRAM_ATTR _hsync_short;
int DRAM_ATTR _burst_start;
int DRAM_ATTR _burst_width;
int DRAM_ATTR _active_start;

int16_t* DRAM_ATTR _burst0 = 0; // pal bursts (allocated in internal DRAM)
int16_t* DRAM_ATTR _burst1 = 0;

static int usec(float us)
{
    uint32_t r = (uint32_t)(us*_sample_rate);
    return ((r + _samples_per_cc)/(_samples_per_cc << 1))*(_samples_per_cc << 1);  // multiple of color clock, word align
}

static int usec_even(float us)
{
    const uint32_t samples = (uint32_t)(us * _sample_rate + 0.5f);
    return (int)((samples + 1u) & ~1u);
}

#define NTSC_FREQUENCY \
    ((double)VIDEO_NTSC_COLOR_CLOCK_NUMERATOR_HZ / \
     (double)VIDEO_NTSC_COLOR_CLOCK_DENOMINATOR)
#define NTSC_LINES VIDEO_NTSC_LINES_PER_FRAME

#define PAL_COLOR_CLOCKS_PER_SCANLINE 284        // really 283.75 ?
#define PAL_FREQUENCY 4433618.75
#define PAL_LINES 312

#ifndef GB_PAL_FULLSCREEN
#define GB_PAL_FULLSCREEN 1
#endif

#ifndef GB_NTSC_FULLSCREEN
#define GB_NTSC_FULLSCREEN 1
#endif

#ifndef NES_PAL_HORIZONTAL_CROP_FILL
#define NES_PAL_HORIZONTAL_CROP_FILL 1
#endif

#ifndef NES_PAL_VERTICAL_6_5
#define NES_PAL_VERTICAL_6_5 1
#endif

static const int kPalSyncStartLine = 304;
static const int kPalDefaultActiveFirstLine = 40;
static const int kGbPalFullscreenActiveFirstLine = 16;
static const int kNesPalFullscreenActiveFirstLine = 16;
static const int kNesPalFullscreenActiveLines = 288;
static const int kNesPalCropPixelsPerSide = 8;
static const int kNesPalVisiblePixels = 256 - (kNesPalCropPixelsPerSide * 2);
/* Base PAL position shared by gameplay and menus. */
static const int kNesPalEarlyStartSamples = 16;
/* Menu-only horizontal offset. */
static const int kNesPalMenuExtraLeftSamples = 4;

static const int GB_SRC_WIDTH = 160;
static const int GB_SRC_HEIGHT = 144;
static const int GB_SCALED_WIDTH = GB_SRC_WIDTH;
static const int GB_SCALED_HEIGHT = GB_SRC_HEIGHT;
static const int GB_BORDER_TOP = (240 - GB_SCALED_HEIGHT) / 2;
static const int GB_BORDER_BOTTOM = GB_BORDER_TOP + GB_SCALED_HEIGHT;
static const int GB_ORIGINAL_SAMPLES_PER_PIXEL = 3;
static const int GB_ORIGINAL_WIDTH_SAMPLES = GB_SRC_WIDTH * GB_ORIGINAL_SAMPLES_PER_PIXEL;
static const int GB_NTSC_ACTIVE_LINES = 240;
static const int GB_NTSC_VISIBLE_WIDTH = GB_SRC_WIDTH;
static const int GB_NTSC_LEFT_OFFSET_SAMPLES = 64;
static const int GB_PAL_CROP_LEFT = 2;
static const int GB_PAL_VISIBLE_WIDTH = 158;
static const int GB_PAL_LEFT_OFFSET_SAMPLES = 0;

static volatile int DRAM_ATTR _gb_display_fullscreen = 1;
static volatile int _gui_menu_visible = 0;
static int _gb_original_left_offset_samples = GB_NTSC_LEFT_OFFSET_SAMPLES;
uint8_t DRAM_ATTR _gb_blank_line[GB_SRC_WIDTH];

void pal_init();
static void video_configure_interlace();

static inline void video_apply_display_mode()
{
    const int image_samples = GB_ORIGINAL_WIDTH_SAMPLES;
    int left = (_line_width - _active_start - image_samples) / 2;
    if (left < 0)
        left = 0;
    // Blitters write dst[i^1] word pairs and palette waveforms are locked to
    // the 4-sample color cycle, so the offset must stay a multiple of 4.
    left &= ~3;
    _gb_original_left_offset_samples = left;

#if NES_PAL_VERTICAL_6_5
    if (_pal_ && (_machine == EMU_NES || _machine == EMU_NES6)) {
        _active_lines = kNesPalFullscreenActiveLines;
        _pal_active_first_line = kNesPalFullscreenActiveFirstLine;
        return;
    }
#endif

    if (_machine != EMU_GB)
        return;

    if (_pal_) {
        _active_lines = 240;
        _pal_active_first_line = kPalDefaultActiveFirstLine;
#if GB_PAL_FULLSCREEN
        if (_gb_display_fullscreen) {
            _active_lines = 288;
            _pal_active_first_line = kGbPalFullscreenActiveFirstLine;
        }
#endif
    } else {
        _active_lines = GB_NTSC_ACTIVE_LINES;
        _pal_active_first_line = 0;
    }
}

void video_set_gb_display_fullscreen(int enabled)
{
    _gb_display_fullscreen = enabled ? 1 : 0;
    video_apply_display_mode();
}

int video_get_gb_display_fullscreen()
{
    return _gb_display_fullscreen ? 1 : 0;
}

void video_set_gui_menu_visible(int visible)
{
    _gui_menu_visible = visible ? 1 : 0;
}

static inline void gb_init_scale_map()
{
    static bool inited = false;
    if (inited)
        return;
    memset(_gb_blank_line, 0, sizeof(_gb_blank_line));
    inited = true;
}

static inline void IRAM_ATTR video_consume_queued_lines()
{
    const uint32_t r = __atomic_load_n(&_queued_lines_r, __ATOMIC_RELAXED);
    const uint32_t w = __atomic_load_n(&_queued_lines_w, __ATOMIC_ACQUIRE);
    if (r == w)
        return;
    _lines = _queued_lines[r & (kVideoQueuedLineDepth - 1u)];
    __atomic_store_n(&_queued_lines_r, r + 1u, __ATOMIC_RELEASE);
}

bool video_queue_lines(uint8_t** lines)
{
    if (!lines)
        return false;

    uint32_t w = __atomic_load_n(&_queued_lines_w, __ATOMIC_RELAXED);
    uint32_t r = __atomic_load_n(&_queued_lines_r, __ATOMIC_ACQUIRE);
    if ((w - r) >= kVideoQueuedLineDepth) {
        r = w - (kVideoQueuedLineDepth - 1u);
        __atomic_store_n(&_queued_lines_r, r, __ATOMIC_RELEASE);
    }

    _queued_lines[w & (kVideoQueuedLineDepth - 1u)] = lines;
    __atomic_store_n(&_queued_lines_w, w + 1u, __ATOMIC_RELEASE);
    return true;
}

void video_clear_queued_lines()
{
    const uint32_t w = __atomic_load_n(&_queued_lines_w, __ATOMIC_ACQUIRE);
    __atomic_store_n(&_queued_lines_r, w, __ATOMIC_RELEASE);
}

void video_init(int samples_per_cc, int machine, const uint32_t* palette, int ntsc)
{
    if (ntsc && ((VIDEO_NTSC_COLOR_CLOCKS_PER_LINE_X2 * samples_per_cc) & 1u)) {
#ifdef ESP_PLATFORM
        esp_restart();
#endif
        return;
    }

    _samples_per_cc = samples_per_cc;
    _machine = machine;
    _ntsc_phase_banks_ready = 0;
    video_clear_queued_lines();
    _last_sync_frame = -1;
    _last_pal_nes_render_frame = -1;
#ifdef ESP_PLATFORM
    _nes_visualizer_palette_active = false;
#endif
    if (machine == EMU_GB)
        gb_init_scale_map();
    #ifdef ESP_PLATFORM
    if (palette) {
        int bank_len = 0;
        switch (machine) {
            case EMU_NES:
            case EMU_NES6:
                bank_len = 64;
                break;
            case EMU_GB:
            default:
                bank_len = 256;
                break;
        }
        const int pal_len = bank_len * 2;
        if (pal_len > 0) {
            _palette_active_len = (size_t)pal_len;
            if (_palette_ram_len < (size_t)pal_len) {
                if (_palette_ram)
                    free(_palette_ram);
                _palette_ram = (uint32_t*)heap_caps_malloc((size_t)pal_len * sizeof(uint32_t),
                                                          MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
                if (!_palette_ram) {
                    printf("video_init: internal palette allocation failed bytes=%u\n",
                           (unsigned)((size_t)pal_len * sizeof(uint32_t)));
                    esp_restart();
                }
                _palette_ram_len = _palette_ram ? (size_t)pal_len : 0;
            }
            if (_palette_ram) {
                uint32_t* target_palette = _palette_ram;
                if (machine == EMU_GB) {
                    video_encode_gb_palette_rgb888(palette, target_palette,
                                                   (size_t)pal_len, ntsc != 0);
                } else if (ntsc) {
                    memcpy(target_palette, palette, (size_t)bank_len * sizeof(uint32_t));
                    for (int i = 0; i < bank_len; ++i) {
                        target_palette[i + bank_len] =
                            video_invert_ntsc_phase(target_palette[i]);
                    }
                } else {
                    memcpy(target_palette, palette, (size_t)pal_len * sizeof(uint32_t));
                }
                _palette = _palette_ram;
                _ntsc_phase_banks_ready = ntsc ? 1 : 0;
            } else {
                _palette = palette;
            }
        } else {
            _palette_active_len = 0;
            _palette = palette;
        }
    } else {
        _palette_active_len = 0;
        _palette = NULL;
    }
    #else
    _palette = palette;
    #endif
    video_palette_glitch_sync_machine_state();

    if (ntsc) {
        _sample_rate = (float)(NTSC_FREQUENCY / 1000000.0) * samples_per_cc;
        _line_width = (int)(VIDEO_NTSC_COLOR_CLOCKS_PER_LINE_X2 * samples_per_cc / 2u);
        _line_count = NTSC_LINES;
        _hsync = usec_even(4.7f);
        _hsync_long = _line_width - _hsync;
        _burst_start = usec_even(5.3f);
        _burst_width = 9 * samples_per_cc;
        _active_start = usec_even(9.5f);
        _pal_ = 0;
        _active_lines = 240;
        _pal_active_first_line = 0;
    } else {
        pal_init();
        _pal_ = 1;
        _active_lines = 240;
        _pal_active_first_line = kPalDefaultActiveFirstLine;
    }

    video_apply_display_mode();
    video_configure_interlace();

    video_init_hw(_line_width,_samples_per_cc);    // init the hardware
}

//===================================================================================================
//===================================================================================================
// PAL

void pal_init()
{
    int cc_width = 4;
    _sample_rate = PAL_FREQUENCY*cc_width/1000000.0;       // DAC rate in mhz
    _line_width = PAL_COLOR_CLOCKS_PER_SCANLINE*cc_width;
    _line_count = PAL_LINES;
    _hsync_short = usec(2);
    _hsync_long = usec(30);
    _hsync = usec(4.7);
    _burst_start = usec(5.6);
    // An integer number of color cycles avoids burst DC bias.
    _burst_width = (int)(10*cc_width) & 0xFFFE; // 10 cycles, even number of samples
    _active_start = usec(10.4);

    // Make colorburst tables for even and odd lines
    // Allocate in internal DRAM as they are read from the ISR every scanline.
#ifdef ESP_PLATFORM
    _burst0 = (int16_t*)heap_caps_malloc(_burst_width * sizeof(int16_t), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    _burst1 = (int16_t*)heap_caps_malloc(_burst_width * sizeof(int16_t), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (!_burst0 || !_burst1) {
        printf("pal_init: internal burst allocation failed bytes=%u\n",
               (unsigned)(_burst_width * sizeof(int16_t)));
        esp_restart();
    }
#else
    _burst0 = new int16_t[_burst_width];
    _burst1 = new int16_t[_burst_width];
#endif
    float phase = 2*M_PI/2;
    // Tunable PAL burst amplitude relative to blanking
    const float pal_burst_gain = 0.62f; // ~0.62 x blanking level (~300mV p-p)
    const float burst_amp = BLANKING_LEVEL * pal_burst_gain;
    for (int i = 0; i < _burst_width; i++)
    {
        _burst0[i] = BLANKING_LEVEL + sin(phase + 3*M_PI/4) * burst_amp;
        _burst1[i] = BLANKING_LEVEL + sin(phase - 3*M_PI/4) * burst_amp;
        phase += 2*M_PI/cc_width;
    }
}

bool video_set_gb_palette_entry(uint8_t index, uint32_t rgb888)
{
#ifdef ESP_PLATFORM
    if (_machine != EMU_GB || !_palette_ram)
        return false;

    uint32_t* target_palette = _palette_ram;
    if (_pal_) {
        uint32_t even = 0;
        uint32_t odd = 0;
        video_encode_pal_rgb888(rgb888, &even, &odd);
        target_palette[index] = even;
        if (_palette_active_len >= 512)
            target_palette[index + 256] = odd;
    } else {
        target_palette[index] = video_encode_ntsc_rgb888(rgb888);
        if (_palette_active_len >= 512) {
            target_palette[index + 256] =
                video_invert_ntsc_phase(target_palette[index]);
        }
    }
    return true;
#else
    (void)index;
    (void)rgb888;
    return false;
#endif
}

bool video_set_nes_visualizer_palette(const uint32_t rgb888[4])
{
#ifdef ESP_PLATFORM
    if ((_machine != EMU_NES && _machine != EMU_NES6) ||
        !_palette_ram || !rgb888 || _palette_active_len < 64)
        return false;

    if (!_nes_visualizer_palette_active) {
        for (int i = 0; i < 4; ++i) {
            const uint8_t index = _nes_visualizer_palette_indices[i];
            _nes_visualizer_palette_saved_even[i] = _palette_ram[index];
            if (_palette_active_len >= 128)
                _nes_visualizer_palette_saved_odd[i] = _palette_ram[index + 64];
        }
        _nes_visualizer_palette_active = true;
    }

    for (int i = 0; i < 4; ++i) {
        const uint8_t index = _nes_visualizer_palette_indices[i];
        if (_pal_) {
            uint32_t even = 0;
            uint32_t odd = 0;
            video_encode_pal_rgb888(rgb888[i], &even, &odd);
            _palette_ram[index] = even;
            if (_palette_active_len >= 128)
                _palette_ram[index + 64] = odd;
        } else {
            _palette_ram[index] = video_encode_ntsc_rgb888(rgb888[i]);
            if (_palette_active_len >= 128) {
                _palette_ram[index + 64] =
                    video_invert_ntsc_phase(_palette_ram[index]);
            }
        }
    }
    _palette = _palette_ram;
    return true;
#else
    (void)rgb888;
    return false;
#endif
}

void video_clear_nes_visualizer_palette()
{
#ifdef ESP_PLATFORM
    if (!_nes_visualizer_palette_active || !_palette_ram)
        return;

    for (int i = 0; i < 4; ++i) {
        const uint8_t index = _nes_visualizer_palette_indices[i];
        _palette_ram[index] = _nes_visualizer_palette_saved_even[i];
        if (_palette_active_len >= 128)
            _palette_ram[index + 64] = _nes_visualizer_palette_saved_odd[i];
    }
    _nes_visualizer_palette_active = false;
    _palette = _palette_ram;
#endif
}

bool video_reset_gb_base_palette(const uint32_t* palette)
{
#ifdef ESP_PLATFORM
    if (_machine != EMU_GB || !_palette_ram || !palette)
        return false;

    video_encode_gb_palette_rgb888(palette, _palette_ram, _palette_active_len,
                                   _pal_ == 0);
    return true;
#else
    (void)palette;
    return false;
#endif
}

static inline uint8_t* IRAM_ATTR gb_scaled_line_for_active_index(uint8_t** lines, int active_line)
{
    if (!lines)
        return _gb_blank_line;
#if GB_PAL_FULLSCREEN
    if (_pal_ && _gb_display_fullscreen) {
        int y = active_line >> 1;
        if (y < 0 || y >= GB_SRC_HEIGHT)
            return _gb_blank_line;
        return lines[y];
    }
#endif
#if GB_NTSC_FULLSCREEN
    if (!_pal_ && _gb_display_fullscreen) {
        if (active_line < 0 || active_line >= GB_NTSC_ACTIVE_LINES)
            return _gb_blank_line;
        const int y = (active_line * GB_SRC_HEIGHT) / GB_NTSC_ACTIVE_LINES;
        if (y < 0 || y >= GB_SRC_HEIGHT)
            return _gb_blank_line;
        return lines[y];
    }
#endif
    if (active_line < GB_BORDER_TOP || active_line >= GB_BORDER_BOTTOM)
        return _gb_blank_line;
    int y = active_line - GB_BORDER_TOP;
    return lines[y];
}

static inline uint8_t* IRAM_ATTR pal_line_for_active_index(uint8_t** lines, int active_line)
{
    if (_machine == EMU_GB)
        return gb_scaled_line_for_active_index(lines, active_line);
#if NES_PAL_VERTICAL_6_5
    if (_machine == EMU_NES || _machine == EMU_NES6) {
        const int y = (active_line * 5) / 6;
        return lines[y];
    }
#endif
    return lines[active_line];
}

static inline void IRAM_ATTR blit_gb_scaled(uint8_t* src, uint16_t* dst)
{
    const uint32_t* p = _palette;
    if (_pal_ && _palette)
        p = (_line_parity_even != 0) ? _palette : (_palette + 256);
    else if (_ntsc_phase_banks_ready && _palette && !_line_parity_even)
        p = _palette + 256;
    uint32_t color;

#if GB_PAL_FULLSCREEN
    if (_pal_ && _gb_display_fullscreen) {
        dst += GB_PAL_LEFT_OFFSET_SAMPLES;
        src += GB_PAL_CROP_LEFT;
        for (int i = 0; i < GB_PAL_VISIBLE_WIDTH; i++) {
            color = p[src[i]];
            if (i & 1) {
                dst[0^1] = P2;
                dst[1^1] = P3;
                dst[2^1] = P0;
                dst[3^1] = P1;
                dst[4^1] = P2;
                dst[5^1] = P3;
            } else {
                dst[0^1] = P0;
                dst[1^1] = P1;
                dst[2^1] = P2;
                dst[3^1] = P3;
                dst[4^1] = P0;
                dst[5^1] = P1;
            }
            dst += 6;
        }
        return;
    }
#endif

#if GB_NTSC_FULLSCREEN
    if (!_pal_ && _gb_display_fullscreen) {
        dst += GB_NTSC_LEFT_OFFSET_SAMPLES;
        for (int i = 0; i < GB_NTSC_VISIBLE_WIDTH; ++i) {
            color = p[src[i]];
            dst[0^1] = P0;
            dst[1^1] = P1;
            dst[2^1] = P2;
            dst[3^1] = P3;
            dst += 4;
        }
        return;
    }
#endif

    uint32_t c;
    dst += _gb_original_left_offset_samples;
    for (int i = 0; i < GB_SRC_WIDTH; i += 4) {
        c = (uint32_t)src[i + 0]
          | ((uint32_t)src[i + 1] << 8)
          | ((uint32_t)src[i + 2] << 16)
          | ((uint32_t)src[i + 3] << 24);
        color = p[c & 0xFF];
        dst[0^1] = P0;
        dst[1^1] = P1;
        dst[2^1] = P2;
        color = p[(c >> 8) & 0xFF];
        dst[3^1] = P3;
        dst[4^1] = P0;
        dst[5^1] = P1;
        color = p[(c >> 16) & 0xFF];
        dst[6^1] = P2;
        dst[7^1] = P3;
        dst[8^1] = P0;
        color = p[(c >> 24) & 0xFF];
        dst[9^1] = P1;
        dst[10^1] = P2;
        dst[11^1] = P3;
        dst += 12;
    }
}

/* no extra left offset in PAL blitter; use original centering */

void IRAM_ATTR blit_pal(uint8_t* src, uint16_t* dst)
{
    uint32_t c,color;
    // Parity captured at ISR entry.
    bool even = (_line_parity_even != 0);
    const uint32_t* p = even ? _palette : _palette + 256;
    int left = 0;
    int right = 256;
    uint8_t mask = 0xFF;
    uint8_t c0,c1,c2,c3,c4;
    uint8_t y1,y2,y3;

    switch (_machine) {
        case EMU_NES:
#if NES_PAL_HORIZONTAL_CROP_FILL
            // Crop normal NES overscan and give each retained pixel one full
            // PAL color cycle. The menu has a separate alignment adjustment.
            mask = 0x3F;
            if (!even)
                p = _palette + 64;
            src += kNesPalCropPixelsPerSide;
            dst -= kNesPalEarlyStartSamples +
                   (_gui_menu_visible ? kNesPalMenuExtraLeftSamples : 0);
            for (int i = 0; i < kNesPalVisiblePixels; ++i) {
                color = p[src[i] & mask];
                dst[0^1] = P0;
                dst[1^1] = P1;
                dst[2^1] = P2;
                dst[3^1] = P3;
                dst += 4;
            }
            return;
#else
            // 192 of 288 color clocks wide: roughly correct aspect ratio
            mask = 0x3F;
            if (!even)
              p = _palette + 64;
            dst += 76;  // center the image
            break;
#endif
    }

    // 4 pixels over 3 color clocks, 12 samples
    // do the blitting
    for (int i = left; i < right; i += 4) {
        c = *((uint32_t*)(src+i));
        color = p[c & mask];
        dst[0^1] = P0;
        dst[1^1] = P1;
        dst[2^1] = P2;
        color = p[(c >> 8) & mask];
        dst[3^1] = P3;
        dst[4^1] = P0;
        dst[5^1] = P1;
        color = p[(c >> 16) & mask];
        dst[6^1] = P2;
        dst[7^1] = P3;
        dst[8^1] = P0;
        color = p[(c >> 24) & mask];
        dst[9^1] = P1;
        dst[10^1] = P2;
        dst[11^1] = P3;
        dst += 12;
    }
}

void IRAM_ATTR burst_pal(uint16_t* line)
{
    line += _burst_start;
    // Parity captured at ISR entry.
    int16_t* b = (_line_parity_even != 0) ? _burst0 : _burst1;
    for (int i = 0; i < _burst_width; i += 2) {
        line[i^1] = b[i];
        line[(i+1)^1] = b[i+1];
    }
}

// NTSC tables
// AA AB BB             // 3 pixels, 2 color clocks - nes
// AAA ABB BBC CCC      // 4 pixels, 3 color clocks

// cc == 3 gives 684 samples per line, 3 samples per cc, 3 pixels for 2 cc
// cc == 4 gives 910 samples per line, 4 samples per cc, 2 pixels per cc

#define BEGIN_TIMING()
#define END_TIMING()
#define ISR_BEGIN()
#define ISR_END()

// draw a line of game in NTSC
void IRAM_ATTR blit(uint8_t* src, uint16_t* dst)
{
    uint32_t* d = (uint32_t*)dst;
    const uint32_t* p = _palette;
    uint32_t color,c;
    uint32_t mask = 0xFF;
    int i;

    BEGIN_TIMING();
    if (_machine == EMU_GB) {
        blit_gb_scaled(src, dst);
        END_TIMING();
        return;
    }
    if (_pal_) {
        blit_pal(src,dst);
        END_TIMING();
        return;
    }

    if (_ntsc_phase_banks_ready && p && !_line_parity_even) {
        p += (_machine == EMU_NES || _machine == EMU_NES6) ? 64 : 256;
    }

    switch (_machine) {
        case EMU_NES:
            mask = 0x3F;
            // AAA ABB BBC CCC
            // 4 pixels, 3 color clocks, 4 samples per cc
            // each pixel gets 3 samples, 192 color clocks wide
            for (i = 0; i < 256; i += 4) {
                c = *((uint32_t*)(src+i));
                color = p[c & mask];
                dst[0^1] = P0;
                dst[1^1] = P1;
                dst[2^1] = P2;
                color = p[(c >> 8) & mask];
                dst[3^1] = P3;
                dst[4^1] = P0;
                dst[5^1] = P1;
                color = p[(c >> 16) & mask];
                dst[6^1] = P2;
                dst[7^1] = P3;
                dst[8^1] = P0;
                color = p[(c >> 24) & mask];
                dst[9^1] = P1;
                dst[10^1] = P2;
                dst[11^1] = P3;
                dst += 12;
            }
            break;

    }
    END_TIMING();
}

void IRAM_ATTR burst(uint16_t* line)
{
    if (_pal_) {
        burst_pal(line);
        return;
    }

    int i,phase;
    switch (_samples_per_cc) {
        case 4:
            // Half-line NTSC timing alternates burst phase by line.
            phase = _line_parity_even ? 1 : -1;
            for (i = _burst_start; i < _burst_start + _burst_width; i += 4) {
                line[(i+0)^1] = BLANKING_LEVEL;
                line[(i+1)^1] = BLANKING_LEVEL + phase * (BLANKING_LEVEL/2);
                line[(i+2)^1] = BLANKING_LEVEL;
                line[(i+3)^1] = BLANKING_LEVEL - phase * (BLANKING_LEVEL/2);
            }
            break;
        case 3:
            // 3 samples per color clock
            phase = 0.866025*BLANKING_LEVEL/2;
            for (i = _hsync; i < _hsync + (3*10); i += 6) {
                line[i+1] = BLANKING_LEVEL;
                line[i+0] = BLANKING_LEVEL + phase;
                line[i+3] = BLANKING_LEVEL - phase;
                line[i+2] = BLANKING_LEVEL;
                line[i+5] = BLANKING_LEVEL + phase;
                line[i+4] = BLANKING_LEVEL - phase;
            }
            break;
    }
}

static inline void IRAM_ATTR
video_fill_level(uint16_t* line, int start, int end, uint16_t level)
{
    if (start >= end)
        return;

    uint16_t* dst16 = line + start;
    int count = end - start;
    if (((uintptr_t)dst16) & 0x2) {
        *dst16++ = level;
        --count;
    }

    const uint32_t pair = ((uint32_t)level << 16) | (uint32_t)level;
    uint32_t* dst32 = (uint32_t*)dst16;
    int pairs = count >> 1;
    while (pairs >= 8) {
        dst32[0] = pair;
        dst32[1] = pair;
        dst32[2] = pair;
        dst32[3] = pair;
        dst32[4] = pair;
        dst32[5] = pair;
        dst32[6] = pair;
        dst32[7] = pair;
        dst32 += 8;
        pairs -= 8;
    }
    while (pairs-- > 0)
        *dst32++ = pair;
    if (count & 1)
        *((uint16_t*)dst32) = level;
}

static inline void IRAM_ATTR sync(uint16_t* line, int syncwidth)
{
    video_fill_level(line, 0, syncwidth, (uint16_t)SYNC_LEVEL);
}

static inline void IRAM_ATTR blanking(uint16_t* line, bool vbl)
{
    int syncwidth = vbl ? _hsync_long : _hsync;
    sync(line,syncwidth);
    video_fill_level(line, syncwidth, _line_width, (uint16_t)BLANKING_LEVEL);
    if (!vbl)
        burst(line);    // no burst during vbl
}

static inline void IRAM_ATTR video_blank_range(uint16_t* line, int start, int end)
{
    if (start < 0)
        start = 0;
    if (end > _line_width)
        end = _line_width;
    video_fill_level(line, start, end, (uint16_t)BLANKING_LEVEL);
}

static inline void IRAM_ATTR video_blank_ntsc_active_edges(uint16_t* line,
                                                           int active_start,
                                                           int active_samples)
{
    const int burst_end = _burst_start + _burst_width;
    video_blank_range(line, burst_end, active_start);
    video_blank_range(line, active_start + active_samples, _line_width);
}

static inline int IRAM_ATTR video_ntsc_active_start(int machine,
                                                    int default_start)
{
    if (!_gui_menu_visible ||
        (machine != EMU_NES && machine != EMU_NES6))
        return default_start;

    const int burst_end = _burst_start + _burst_width;
    const int image_samples = (_samples_per_cc == 3) ? 512 : 768;
    const int free_samples = _line_width - burst_end - image_samples;
    if (free_samples < 0)
        return default_start;

    // Center the NES menu, then retain a whole-color-cycle phase offset.
    int start = burst_end + (free_samples / 2);
    const int phase_remainder = start % _samples_per_cc;
    if (phase_remainder)
        start += _samples_per_cc - phase_remainder;
    return (start + image_samples <= _line_width) ? start : default_start;
}

// Fancy pal non-interlace
// http://martin.hinner.info/vga/pal.html
static inline void IRAM_ATTR pal_sync2(uint16_t* line, int width, int swidth)
{
    swidth = swidth ? _hsync_long : _hsync_short;
    video_fill_level(line, 0, swidth, (uint16_t)SYNC_LEVEL);
    video_fill_level(line, swidth, width, (uint16_t)BLANKING_LEVEL);
}

uint8_t DRAM_ATTR _sync_type[8] = {0,0,0,3,3,2,0,0};
static inline void IRAM_ATTR pal_sync_progressive(uint16_t* line, int i)
{
    uint8_t t = _sync_type[i-304];
    pal_sync2(line,_line_width/2, t & 2);
    pal_sync2(line+_line_width/2,_line_width/2, t & 1);
}

//====================================================================================================
// Interlaced output (576i / 480i)
//
// Composite inputs on video mixers (e.g. Roland V-4EX: "576/50i, 480/59.94i"
// only) expect real interlace, not the 288p/240p a console-style signal uses.
// Each field is still one "frame" for the emulators (same 50/60 Hz pacing,
// same active lines); fields alternate between N and N+1 lines and the second
// field's vertical sync starts half a line later, which is all interlace is.
// Pulses follow the standards: equalizing 2.35/2.3 us, broad pulses with a
// 4.7 us serration, 5/5/5 (PAL) or 6/6/6 (NTSC) half-line sequences.

#ifndef VIDEO_INTERLACED_DEFAULT
#define VIDEO_INTERLACED_DEFAULT 1
#endif

int DRAM_ATTR _interlaced = VIDEO_INTERLACED_DEFAULT;
uint8_t DRAM_ATTR _field_odd = 0;           // second field: one extra line
int DRAM_ATTR _line_count_base = 0;          // even-length field
uint32_t DRAM_ATTR _video_line_seq = 0;      // free-running, for colour parity
int DRAM_ATTR _hsync_eq = 0;
int DRAM_ATTR _hsync_broad = 0;

enum {
    HL_NORMAL = 0,  // line sync, then blanking
    HL_EQ = 1,      // equalizing pulse
    HL_BROAD = 2,   // broad (vertical) pulse
    HL_NONE = 3,    // blanking, no pulse
};
#define HL(a, b) (uint8_t)(((a) << 2) | (b))

// PAL: vsync starts at field line 304 (after the last active line, 303).
static const uint8_t DRAM_ATTR kPalVsyncFieldA[8] = {
    HL(HL_EQ,HL_EQ), HL(HL_EQ,HL_EQ), HL(HL_EQ,HL_BROAD), HL(HL_BROAD,HL_BROAD),
    HL(HL_BROAD,HL_BROAD), HL(HL_EQ,HL_EQ), HL(HL_EQ,HL_EQ), HL(HL_EQ,HL_NONE),
};
static const uint8_t DRAM_ATTR kPalVsyncFieldB[9] = {
    HL(HL_NORMAL,HL_EQ), HL(HL_EQ,HL_EQ), HL(HL_EQ,HL_EQ), HL(HL_BROAD,HL_BROAD),
    HL(HL_BROAD,HL_BROAD), HL(HL_BROAD,HL_EQ), HL(HL_EQ,HL_EQ), HL(HL_EQ,HL_EQ),
    HL(HL_NORMAL,HL_NONE),
};
// NTSC: vsync starts 3 lines after the last active line.
static const int kNtscInterlacedVsyncGap = 3;
static const uint8_t DRAM_ATTR kNtscVsyncFieldA[9] = {
    HL(HL_EQ,HL_EQ), HL(HL_EQ,HL_EQ), HL(HL_EQ,HL_EQ),
    HL(HL_BROAD,HL_BROAD), HL(HL_BROAD,HL_BROAD), HL(HL_BROAD,HL_BROAD),
    HL(HL_EQ,HL_EQ), HL(HL_EQ,HL_EQ), HL(HL_EQ,HL_EQ),
};
static const uint8_t DRAM_ATTR kNtscVsyncFieldB[10] = {
    HL(HL_NORMAL,HL_EQ), HL(HL_EQ,HL_EQ), HL(HL_EQ,HL_EQ), HL(HL_EQ,HL_BROAD),
    HL(HL_BROAD,HL_BROAD), HL(HL_BROAD,HL_BROAD), HL(HL_BROAD,HL_EQ),
    HL(HL_EQ,HL_EQ), HL(HL_EQ,HL_EQ), HL(HL_EQ,HL_NONE),
};

static inline void IRAM_ATTR video_half_line(uint16_t* line, int width, uint8_t type)
{
    int pulse;
    switch (type) {
        case HL_NORMAL: pulse = _hsync; break;
        case HL_EQ:     pulse = _hsync_eq; break;
        case HL_BROAD:  pulse = _hsync_broad; break;
        default:        pulse = 0; break;
    }
    video_fill_level(line, 0, pulse, (uint16_t)SYNC_LEVEL);
    video_fill_level(line, pulse, width, (uint16_t)BLANKING_LEVEL);
}

static inline void IRAM_ATTR video_vsync_line(uint16_t* line, uint8_t halves)
{
    const int half = _line_width / 2;
    video_half_line(line, half, halves >> 2);
    video_half_line(line + half, _line_width - half, halves & 3);
}

// Fills vertical-sync line i of the current PAL field (i >= kPalSyncStartLine).
static inline void IRAM_ATTR pal_sync(uint16_t* line, int i)
{
    if (!_interlaced) {
        pal_sync_progressive(line, i);
        return;
    }
    const int k = i - kPalSyncStartLine;
    if (_field_odd)
        video_vsync_line(line, kPalVsyncFieldB[k]);
    else
        video_vsync_line(line, kPalVsyncFieldA[k]);
}

// NTSC lines after the active area. Returns after filling `line`.
static inline void IRAM_ATTR ntsc_post_active_line(uint16_t* line, int i, int active_lines)
{
    if (!_interlaced) {
        if (i < (active_lines + 5))
            blanking(line,false);           // post render/black
        else if (i < (active_lines + 8))
            blanking(line,true);            // vsync
        else
            blanking(line,false);           // pre render/black
        return;
    }
    const int k = i - (active_lines + kNtscInterlacedVsyncGap);
    const int n = _field_odd ? (int)sizeof(kNtscVsyncFieldB) : (int)sizeof(kNtscVsyncFieldA);
    if (k >= 0 && k < n)
        video_vsync_line(line, _field_odd ? kNtscVsyncFieldB[k] : kNtscVsyncFieldA[k]);
    else
        blanking(line,false);
}

// Called by the ISR at the end of every field.
static inline void IRAM_ATTR video_next_field()
{
    if (!_interlaced)
        return;
    _field_odd ^= 1;
    _line_count = _line_count_base + _field_odd;
}

// Sets up interlace timing after the line geometry is known (video_init).
static void video_configure_interlace()
{
    _field_odd = 0;
    _line_count_base = _line_count;
    if (!_interlaced)
        return;
    if (_pal_) {
        _hsync_eq = usec_even(2.35f);
        _hsync_broad = usec_even(27.3f);
    } else {
        _hsync_eq = usec_even(2.3f);
        _hsync_broad = usec_even(27.1f);
    }
    printf("video: interlaced %s eq=%d broad=%d samples line=%d\n",
           _pal_ ? "576i" : "480i", _hsync_eq, _hsync_broad, _line_width);
}

//  audio is buffered as output-device samples: 12-bit DAC or 7-bit PWM fallback
#ifndef AUDIO_BUFFER_SIZE
#if AUDIO_MCP4822_OUTPUT
#define AUDIO_BUFFER_SIZE 4096
#else
#define AUDIO_BUFFER_SIZE 4096
#endif
#endif
#if (AUDIO_BUFFER_SIZE & (AUDIO_BUFFER_SIZE - 1))
#error AUDIO_BUFFER_SIZE must be a power of two.
#endif
DRAM_ATTR audio_output_sample_t _audio_buffer[AUDIO_BUFFER_SIZE];
volatile uint32_t DRAM_ATTR _audio_r = 0;
volatile uint32_t DRAM_ATTR _audio_w = 0;
volatile audio_output_sample_t DRAM_ATTR _audio_last_sample = AUDIO_OUTPUT_SILENCE_SAMPLE;
uint8_t _audio_stretch_phase = 0;
uint16_t _audio_thin_phase = 0;
#if AUDIO_BASS_BOOST
int32_t _audio_bass_lp = 0;
#endif
#if AUDIO_OUTPUT_ANTIALIAS
int32_t _audio_antialias_z1 = 0;
int32_t _audio_antialias_z2 = 0;
int32_t _audio_antialias_z3 = 0;
int32_t _audio_antialias_lp = 0;
bool _audio_antialias_ready = false;
#endif

uint32_t audio_queue_level()
{
    return _audio_w - _audio_r;
}

void audio_queue_clear()
{
    _audio_r = _audio_w;
    _audio_last_sample = AUDIO_OUTPUT_SILENCE_SAMPLE;
    _audio_stretch_phase = 0;
    _audio_thin_phase = 0;
#if AUDIO_BASS_BOOST
    _audio_bass_lp = 0;
#endif
#if AUDIO_OUTPUT_ANTIALIAS
    _audio_antialias_z1 = 0;
    _audio_antialias_z2 = 0;
    _audio_antialias_z3 = 0;
    _audio_antialias_lp = 0;
    _audio_antialias_ready = false;
#endif
}

uint32_t audio_queue_capacity()
{
    return AUDIO_BUFFER_SIZE;
}

static inline void IRAM_ATTR __attribute__((always_inline)) audio_output_step()
{
    uint32_t queued = _audio_w - _audio_r;
    audio_output_sample_t s;
    if (queued) {
        s = _audio_buffer[_audio_r++ & (AUDIO_BUFFER_SIZE - 1)];
        _audio_last_sample = s;
    } else {
        s = _audio_last_sample;
        if (_audio_last_sample < AUDIO_OUTPUT_SILENCE_SAMPLE) {
            uint32_t next = (uint32_t)_audio_last_sample + AUDIO_OUTPUT_UNDERRUN_STEP;
            _audio_last_sample = (next < AUDIO_OUTPUT_SILENCE_SAMPLE)
                ? (audio_output_sample_t)next
                : (audio_output_sample_t)AUDIO_OUTPUT_SILENCE_SAMPLE;
        } else if (_audio_last_sample > AUDIO_OUTPUT_SILENCE_SAMPLE) {
            const uint32_t last = _audio_last_sample;
            _audio_last_sample = (last > (AUDIO_OUTPUT_SILENCE_SAMPLE + AUDIO_OUTPUT_UNDERRUN_STEP))
                ? (audio_output_sample_t)(last - AUDIO_OUTPUT_UNDERRUN_STEP)
                : (audio_output_sample_t)AUDIO_OUTPUT_SILENCE_SAMPLE;
        }
    }
    audio_sample(s);
}

#if AUDIO_TIMER_OUTPUT_ENABLED
static void IRAM_ATTR audio_timer_isr(void* arg)
{
    (void)arg;
    TIMERG1.int_clr_timers.t1 = 1;
    TIMERG1.hw_timer[1].config.alarm_en = 1;
    audio_output_step();
}

static void audio_output_timer_stop()
{
    if (!_audio_timer_handle) {
        _audio_timer_active = false;
        return;
    }
    timer_pause(TIMER_GROUP_1, TIMER_1);
    timer_disable_intr(TIMER_GROUP_1, TIMER_1);
    esp_intr_free(_audio_timer_handle);
    _audio_timer_handle = 0;
    _audio_timer_active = false;
}

static void audio_output_timer_start(bool enable, int ntsc)
{
    audio_output_timer_stop();
    if (!enable)
        return;

    const uint32_t requested_rate = ntsc ? AUDIO_OUTPUT_RATE_NTSC_HZ : AUDIO_OUTPUT_RATE_PAL_HZ;
    const uint32_t ticks = AUDIO_TIMER_TICKS_FOR_RATE(requested_rate);
    const uint32_t actual_rate = AUDIO_TIMER_CLOCK_HZ / ticks;

    timer_config_t config;
    memset(&config, 0, sizeof(config));
    config.alarm_en = true;
    config.counter_en = false;
    config.intr_type = TIMER_INTR_LEVEL;
    config.counter_dir = TIMER_COUNT_UP;
    config.auto_reload = true;
    config.divider = 2;

    if (timer_init(TIMER_GROUP_1, TIMER_1, &config) != ESP_OK)
        return;
    timer_set_counter_value(TIMER_GROUP_1, TIMER_1, 0);
    timer_set_alarm_value(TIMER_GROUP_1, TIMER_1, ticks);
    timer_set_alarm(TIMER_GROUP_1, TIMER_1, TIMER_ALARM_EN);
    if (timer_isr_register(TIMER_GROUP_1, TIMER_1, audio_timer_isr, 0,
                           AUDIO_TIMER_ISR_FLAGS,
                           &_audio_timer_handle) != ESP_OK) {
        _audio_timer_handle = 0;
        return;
    }
    timer_enable_intr(TIMER_GROUP_1, TIMER_1);
    timer_start(TIMER_GROUP_1, TIMER_1);
    _audio_timer_active = true;
    printf("[audio-timer] active=1 requested=%luHz actual=%luHz ticks=%lu clock=%luHz\n",
        (unsigned long)requested_rate,
        (unsigned long)actual_rate,
        (unsigned long)ticks,
        (unsigned long)AUDIO_TIMER_CLOCK_HZ);
}
#else
static void audio_output_timer_stop() {}
static void audio_output_timer_start(bool enable, int ntsc)
{
    (void)enable;
    (void)ntsc;
}
#endif

static inline int32_t audio_apply_bass_boost(int32_t mixed)
{
#if AUDIO_BASS_BOOST
    _audio_bass_lp += (mixed - _audio_bass_lp) >> AUDIO_BASS_LOWPASS_SHIFT;
    const int32_t low = _audio_bass_lp;
    const int32_t high = mixed - low;
    return high + ((low * AUDIO_BASS_GAIN_Q8) >> 8);
#else
    return mixed;
#endif
}

static inline int32_t audio_apply_antialias(int32_t mixed)
{
#if AUDIO_OUTPUT_ANTIALIAS
    if (!(_machine == EMU_GB || _machine == EMU_NES || _machine == EMU_NES6)) {
        _audio_antialias_ready = false;
        return mixed;
    }

    if (!_audio_antialias_ready) {
        _audio_antialias_z1 = mixed;
        _audio_antialias_z2 = mixed;
        _audio_antialias_z3 = mixed;
        _audio_antialias_lp = mixed;
        _audio_antialias_ready = true;
        return mixed;
    }

    const int32_t fir = (mixed + (3 * _audio_antialias_z1) +
                         (3 * _audio_antialias_z2) + _audio_antialias_z3) >> 3;
    _audio_antialias_z3 = _audio_antialias_z2;
    _audio_antialias_z2 = _audio_antialias_z1;
    _audio_antialias_z1 = mixed;

    const int32_t delta = fir - _audio_antialias_lp;
    _audio_antialias_lp += (delta * AUDIO_OUTPUT_ANTIALIAS_ALPHA_Q8) >> 8;
    return _audio_antialias_lp;
#else
    return mixed;
#endif
}

static inline int32_t audio_soft_limit_for_pwm(int32_t mixed)
{
#if AUDIO_SOFT_LIMIT
    const int32_t knee = AUDIO_SOFT_LIMIT_KNEE;
    const int32_t pos_limit = 63 * 256;
    const int32_t neg_limit = -64 * 256;

    if (mixed > knee) {
        mixed = knee + ((mixed - knee) >> 2);
        if (mixed > pos_limit)
            mixed = pos_limit;
    } else if (mixed < -knee) {
        mixed = -knee + ((mixed + knee) >> 2);
        if (mixed < neg_limit)
            mixed = neg_limit;
    }
#endif
    return mixed;
}

static inline bool audio_output_should_thin_for_headroom(uint32_t queued)
{
#if AUDIO_OUTPUT_QUEUE_THINNING
    const uint32_t headroom = min<uint32_t>(AUDIO_OUTPUT_THIN_HEADROOM,
                                           AUDIO_BUFFER_SIZE - 1u);
    const uint32_t start = AUDIO_BUFFER_SIZE - headroom;
    if (queued < start) {
        _audio_thin_phase = 0;
        return false;
    }

    // Thin sparse producer samples near the high-water mark to preserve queue
    // headroom. Audio filters have already advanced, so waveform state remains
    // continuous.
    uint32_t span = headroom ? headroom : 1u;
    uint32_t surplus = queued - start;
    if (surplus > span)
        surplus = span;

    uint32_t min_period = AUDIO_OUTPUT_THIN_MIN_PERIOD;
    uint32_t max_period = AUDIO_OUTPUT_THIN_MAX_PERIOD;
    if (min_period < 1u)
        min_period = 1u;
    if (max_period < min_period)
        max_period = min_period;

    uint32_t period = max_period;
    if (max_period > min_period) {
        period = max_period -
            (uint32_t)(((uint64_t)surplus *
                        (uint64_t)(max_period - min_period) +
                        (span / 2u)) / span);
        if (period < min_period)
            period = min_period;
    }

    if (++_audio_thin_phase >= period) {
        _audio_thin_phase = 0;
        return true;
    }
#else
    (void)queued;
#endif
    return false;
}

void audio_write_16(const int16_t* s, int len, int channels)
{
    uint8_t stretch_phase = _audio_stretch_phase;
    while (len--) {
        if (_audio_w == (_audio_r + AUDIO_BUFFER_SIZE)) {
            break;
        }
        int32_t mixed;
        if (channels == 2) {
            mixed = ((int32_t)s[0] + (int32_t)s[1]) >> 1;
            s += 2;
        } else {
            mixed = *s++;
        }
        mixed = audio_apply_antialias(mixed);
        mixed = audio_apply_bass_boost(mixed);
#if AUDIO_OUTPUT_GAIN_NUM != AUDIO_OUTPUT_GAIN_DEN
        mixed = (mixed * AUDIO_OUTPUT_GAIN_NUM) / AUDIO_OUTPUT_GAIN_DEN;
#endif
#if AUDIO_MCP4822_OUTPUT
        const int32_t span = (int32_t)AUDIO_MCP4822_OUTPUT_SPAN;
        const int32_t scaled = (mixed >= 0)
            ? (int32_t)((((int64_t)mixed * span) + 16384) >> 15)
            : -(int32_t)((((int64_t)(-mixed) * span) + 16384) >> 15);
        int32_t code = (int32_t)AUDIO_OUTPUT_SILENCE_SAMPLE + scaled;
        if (code < 0) {
            code = 0;
        }
        if (code > (int32_t)AUDIO_MCP4822_CLEAN_TOP_CODE) {
            code = (int32_t)AUDIO_MCP4822_CLEAN_TOP_CODE;
        }
        audio_output_sample_t sample = (audio_output_sample_t)code;
#else
        mixed = audio_soft_limit_for_pwm(mixed);
        int b = (mixed >= 0) ? ((mixed + 128) >> 8) : -(((-mixed) + 128) >> 8);
        if (b < -64) {
            b = -64;
        }
        if (b > 63) {
            b = 63;
        }
        audio_output_sample_t sample = (audio_output_sample_t)(b + 64);
#endif
        if (audio_output_should_thin_for_headroom(_audio_w - _audio_r)) {
            continue;
        }
        _audio_buffer[_audio_w++ & (AUDIO_BUFFER_SIZE - 1)] = sample;

#if AUDIO_GB_ADAPTIVE_STRETCH
        if (_machine == EMU_GB) {
            uint32_t queued = _audio_w - _audio_r;
            if (queued < AUDIO_STRETCH_TARGET && _audio_w != (_audio_r + AUDIO_BUFFER_SIZE)) {
                const uint8_t period = queued < 128 ? 32 : (queued < 384 ? 64 : 128);
                stretch_phase++;
                if (stretch_phase >= period) {
                    stretch_phase = 0;
                    _audio_buffer[_audio_w++ & (AUDIO_BUFFER_SIZE - 1)] = sample;
                }
            } else {
                stretch_phase = 0;
            }
        }
#endif
    }
    _audio_stretch_phase = stretch_phase;
}

// test pattern, must be ram
uint8_t _sin64[64] = {
    0x20,0x22,0x25,0x28,0x2B,0x2E,0x30,0x33,
    0x35,0x37,0x38,0x3A,0x3B,0x3C,0x3D,0x3D,
    0x3D,0x3D,0x3D,0x3C,0x3B,0x3A,0x38,0x37,
    0x35,0x33,0x30,0x2E,0x2B,0x28,0x25,0x22,
    0x20,0x1D,0x1A,0x17,0x14,0x11,0x0F,0x0C,
    0x0A,0x08,0x07,0x05,0x04,0x03,0x02,0x02,
    0x02,0x02,0x02,0x03,0x04,0x05,0x07,0x08,
    0x0A,0x0C,0x0F,0x11,0x14,0x17,0x1A,0x1D,
};
uint8_t _x;

// test the fancy DAC
void IRAM_ATTR test_wave(volatile void* vbuf, int t = 1)
{
    uint16_t* buf = (uint16_t*)vbuf;
    int n = _line_width;
    switch (t) {
        case 0: // f/64 sinewave
            for (int i = 0; i < n; i += 2) {
                buf[0^1] = GRAY_LEVEL + (_sin64[_x++ & 0x3F] << 8);
                buf[1^1] = GRAY_LEVEL + (_sin64[_x++ & 0x3F] << 8);
                buf += 2;
            }
            break;
        case 1: // fast square wave
            for (int i = 0; i < n; i += 2) {
                buf[0^1] = GRAY_LEVEL - (0x10 << 8);
                buf[1^1] = GRAY_LEVEL + (0x10 << 8);
                buf += 2;
            }
            break;
    }
}

// Wait for blanking before drawing to avoid tearing.
#ifdef ESP_PLATFORM
#ifndef NES_NTSC_SINGLE_BUFFER_RENDER_LINE
#define NES_NTSC_SINGLE_BUFFER_RENDER_LINE 96
#endif

#ifndef NES_PAL_SINGLE_BUFFER_RENDER_ACTIVE_LINE
#define NES_PAL_SINGLE_BUFFER_RENDER_ACTIVE_LINE 128
#endif

/* True while the beam is inside the active region (and video is running).
 * Lets callers wait out active scan in short slices with useful work in
 * between, instead of blocking through it in video_sync(). */
bool video_scan_in_active()
{
  if (!_lines)
    return false;
  const int active_start = _pal_ ? _pal_active_first_line : 0;
  const int line = _line_counter;
  return line >= active_start && line < active_start + _active_lines;
}

void video_sync()
{
  if (!_lines)
    return;
  const int active_start = _pal_ ? _pal_active_first_line : 0;
  const int active_end = active_start + _active_lines;
  int line = _line_counter;
  if (line < active_start || line >= active_end)
    return;

  const int remaining = active_end - line;
  const int scanlines_per_second = _pal_ ? 15600 : VIDEO_NTSC_SCANLINE_RATE_HZ;
  const int wait_ms = remaining * 1000 / scanlines_per_second;
  if (wait_ms > 1)
    vTaskDelay(wait_ms - 1);

  while ((line = _line_counter) >= active_start && line < active_end) {
  }
}

void video_sync_ntsc_single_buffer_render_slot()
{
  if (!_lines || _pal_) {
    video_sync();
    return;
  }

  const int safe_line = NES_NTSC_SINGLE_BUFFER_RENDER_LINE;
  for (;;) {
    int line = _line_counter;
    if (line >= safe_line && line < _active_lines)
      return;
    vTaskDelay(1);
  }
}

void video_sync_pal_nes_single_buffer_render_slot()
{
  if (!_lines || !_pal_ || !(_machine == EMU_NES || _machine == EMU_NES6)) {
    video_sync();
    return;
  }

  const int safe_line = _pal_active_first_line + NES_PAL_SINGLE_BUFFER_RENDER_ACTIVE_LINE;
  const int active_end = _pal_active_first_line + _active_lines;
  for (;;) {
    const int frame = _frame_counter;
    const int line = _line_counter;
    if (frame != _last_pal_nes_render_frame && line >= safe_line && line < active_end) {
      _last_pal_nes_render_frame = frame;
      return;
    }
    vTaskDelay(1);
  }
}

// GB draws directly into the scanout buffer. Starting near the end of active
// video lets the renderer stay ahead of the beam through the next field. If
// pacing is stale by a full field, resync without waiting.
void video_sync_gb_single_buffer_render_slot()
{
  if (!_lines) {
    _last_sync_frame = _frame_counter;
    return;
  }

  for (;;) {
    const int active_start = _pal_ ? _pal_active_first_line : 0;
    const int safe_line = active_start + (_active_lines * 5) / 6;
    const int frame = _frame_counter;
    const int line = _line_counter;
    const int frame_delta = frame - _last_sync_frame;
    if (frame_delta < 0 || frame_delta >= 2) {
      _last_sync_frame = frame;
      return;
    }
    if (frame_delta == 1 && (line >= safe_line || line < active_start)) {
      _last_sync_frame = frame;
      return;
    }

    // Sleep off the bulk of the wait, then converge in 1-tick polls. The
    // slot window runs from safe_line to the end of the field, so tick
    // granularity cannot skip past it.
    const int wait_lines = (frame_delta == 1)
        ? (safe_line - line)
        : ((_line_count - line) + safe_line);
    const int scanlines_per_second = _pal_ ? 15600 : VIDEO_NTSC_SCANLINE_RATE_HZ;
    const int wait_ms = wait_lines * 1000 / scanlines_per_second;
    if (wait_ms > 2)
      vTaskDelay(wait_ms - 2);
    else
      vTaskDelay(1);
  }
}

void video_sync_for_frame_commit()
{
  if (!_lines)
    return;

  const int active_start = _pal_ ? _pal_active_first_line : 0;
  const int active_end = active_start + _active_lines;

  for (;;) {
    int line = _line_counter;
    // Publish line-table changes only outside active video.
    if (line < active_start || line >= active_end)
      return;

    const int remaining = active_end - line;
    const int scanlines_per_second = _pal_ ? 15600 : VIDEO_NTSC_SCANLINE_RATE_HZ;
    const int wait_ms = remaining * 1000 / scanlines_per_second;
    if (wait_ms > 1)
      vTaskDelay(wait_ms - 1);
    while ((line = _line_counter) >= active_start && line < active_end) {
    }
  }
}

bool video_sync_if_early()
{
  if (!_lines)
    return true;

  if (_frame_counter == _last_sync_frame) {
      while (_frame_counter == _last_sync_frame)
        vTaskDelay(1);
  }
  _last_sync_frame = _frame_counter;
  return true;
}
#endif

// Audio and video ISR.
extern "C"
void IRAM_ATTR video_isr(volatile void* vbuf)
{
    uint8_t** lines = _lines;
    const bool pal = (_pal_ != 0);
    if (!lines)
    {
        uint16_t* buf = (uint16_t*)vbuf;
        // Free-running parity: identical to the field line parity for the
        // even-length progressive fields, and stays continuous across the
        // odd-length fields of interlaced output.
        _line_parity_even = (uint8_t)(_video_line_seq++ & 1);
        if (pal) {
            if (_line_counter < kPalSyncStartLine)
                blanking(buf,false);
            else
                pal_sync(buf,_line_counter);
        } else {
            const int i = _line_counter;
            if (i < _active_lines)
                blanking(buf,false);
            else
                ntsc_post_active_line(buf, i, _active_lines);
        }

        if (++_line_counter == _line_count) {
            _line_counter = 0;
            video_next_field();
            _frame_counter++;
        }
        return;
    }

    ISR_BEGIN();

    const int machine = _machine;
    if (
#if AUDIO_TIMER_OUTPUT_ENABLED
        !_audio_timer_active
#else
        true
#endif
    ) {
        audio_output_step();
#if AUDIO_OUTPUT_2X_SCANLINE && AUDIO_NES_2X_SCANLINE
        if (machine == EMU_NES || machine == EMU_NES6)
            audio_output_step();
#endif
    }
#if IR_INPUT_ENABLED
    ir_sample();
#endif

    int i = _line_counter++;
    _line_parity_even = (uint8_t)(_video_line_seq++ & 1);
    uint16_t* buf = (uint16_t*)vbuf;
    const int active_lines = _active_lines;
    const int active_start = _active_start;
    if (pal) {
        const int pal_active_first_line = _pal_active_first_line;
        // pal
        if (i < pal_active_first_line) {
            blanking(buf,false);                // pre render/black
        } else if (i < active_lines + pal_active_first_line) {
            sync(buf,_hsync);
            burst(buf);
            const int active_index = i - pal_active_first_line;
            uint8_t* src = pal_line_for_active_index(lines, active_index);
            blit(src,buf + active_start);
        } else if (i < kPalSyncStartLine) {     // post render/black
            blanking(buf,false);
        } else {
            pal_sync(buf,i);                    // 8 lines of sync 304-312
        }
    } else {
        // ntsc
        if (i < active_lines) {                 // active video
            const int ntsc_active_start =
                video_ntsc_active_start(machine, active_start);
            sync(buf,_hsync);
            burst(buf);
            if (machine == EMU_NES || machine == EMU_NES6)
                video_blank_ntsc_active_edges(buf, ntsc_active_start,
                                              (_samples_per_cc == 3) ? 512 : 768);
            uint8_t* src = (machine == EMU_GB) ? gb_scaled_line_for_active_index(lines, i) : lines[i];
            blit(src,buf + ntsc_active_start);

        } else {                                // blanking + vsync
            ntsc_post_active_line(buf, i, active_lines);
        }
    }

    if (_line_counter == _line_count) {
        _line_counter = 0;                      // frame (field) is done
        video_next_field();
        if (machine == EMU_GB)
            video_consume_queued_lines();
        _frame_counter++;
    }

    ISR_END();
}
