/* Glitch Boy firmware. Project lineage is documented in THIRD_PARTY_NOTICES.md. */

#include "esp_system.h"
#include "esp_int_wdt.h"
#include "esp_timer.h"
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <sys/time.h>
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "src/hid_server/hci_server.h"

// SD card SPI pins
#define PIN_NUM_MISO 33
#define PIN_NUM_MOSI 13
#define PIN_NUM_CLK  14
#define PIN_NUM_CS    5
#define MOUNT_POINT "/sd"

#include "src/diag_config.h"

#if BLE_HOGP_ENABLED
#include <NimBLEDevice.h>
#endif

#include "src/emu.h"
#include "src/gb_diag_config.h"
#include "src/bt_mode.h"
#include "src/bt_pairing.h"
#include "src/ble_hogp.h"
#include "src/video_out.h"
#include "src/gpio_input.h"
#include "src/glitch/glitch_controls.h"
#include "src/midi_input.h"
#include "src/midi_visualizer.h"
#include "src/hid_server/hid_server.h"

// Start one Bluetooth stack per boot to conserve internal heap.
// Transport changes take effect after restart.
#ifndef BT_TRANSPORT_DEFAULT_MODE
#define BT_TRANSPORT_DEFAULT_MODE BT_TRANSPORT_CLASSIC_ONLY
#endif

#if (BT_TRANSPORT_DEFAULT_MODE != BT_TRANSPORT_DUAL) && \
    (BT_TRANSPORT_DEFAULT_MODE != BT_TRANSPORT_CLASSIC_ONLY) && \
    (BT_TRANSPORT_DEFAULT_MODE != BT_TRANSPORT_BLE_ONLY)
#error Invalid BT_TRANSPORT_DEFAULT_MODE. Use BT_TRANSPORT_DUAL, BT_TRANSPORT_CLASSIC_ONLY, or BT_TRANSPORT_BLE_ONLY.
#endif

// Choose PAL or NTSC video.
#ifndef VIDEO_STANDARD
#define VIDEO_STANDARD NTSC
#endif
// Default emulator when none has been chosen in the menu: EMU_NES or EMU_GB.
#ifndef EMULATOR
#define EMULATOR EMU_NES
#endif

// Define SINGLE_CORE for single-core targets. Filesystem access may flicker.

// NES and GB are selected at boot (the bootemu preference).
Emu* NewEmulator()
{
  char pref[16];
  int standard = VIDEO_STANDARD;
  if (sys_get_pref("bootstd", pref, sizeof(pref) - 1)) {
    if (!strcmp(pref, "ntsc"))
      standard = NTSC;
    else if (!strcmp(pref, "pal"))
      standard = PAL;
  }

#if (EMULATOR==EMU_NES || EMULATOR==EMU_GB)
  int emulator = EMULATOR;
  if (sys_get_pref("bootemu", pref, sizeof(pref) - 1)) {
    if (!strcmp(pref, "gb"))
      emulator = EMU_GB;
    else if (!strcmp(pref, "nes"))
      emulator = EMU_NES;
  }

  printf("Boot selection: emulator=%s standard=%s\n",
         emulator == EMU_GB ? "gb" : "nes",
         standard == NTSC ? "ntsc" : "pal");
  if (emulator == EMU_GB)
    return NewGameBoy(standard);
  return NewNofrendo(standard);
#endif

  printf("Must choose one of the following emulators: EMU_NES,EMU_GB\n");
  return 0;
}

Emu* _emu = 0;            // emulator running on core 0
uint32_t _frame_time = 0;
uint32_t _drawn = 1;
bool _inited = false;
bt_transport_mode_t _bt_transport_mode = BT_TRANSPORT_CLASSIC_ONLY;
static bool _bt_discovery_started = false;
static uint32_t _bt_discovery_next_retry_ms = 0;

#ifndef BT_CLASSIC_HEAP_GUARD
#define BT_CLASSIC_HEAP_GUARD 1
#endif

#ifndef BT_CLASSIC_INIT_MIN_FREE_I8
#define BT_CLASSIC_INIT_MIN_FREE_I8 112000u
#endif

#ifndef BT_CLASSIC_INIT_MIN_LARGEST_I8
#define BT_CLASSIC_INIT_MIN_LARGEST_I8 76000u
#endif

#ifndef BT_CLASSIC_POST_INIT_MIN_FREE_I8
#define BT_CLASSIC_POST_INIT_MIN_FREE_I8 108000u
#endif

#ifndef BT_CLASSIC_POST_INIT_MIN_LARGEST_I8
#define BT_CLASSIC_POST_INIT_MIN_LARGEST_I8 70000u
#endif

#ifndef BT_CLASSIC_RUNTIME_MIN_FREE_I8
#define BT_CLASSIC_RUNTIME_MIN_FREE_I8 16000u
#endif

#ifndef BT_CLASSIC_RUNTIME_MIN_LARGEST_I8
#define BT_CLASSIC_RUNTIME_MIN_LARGEST_I8 9000u
#endif

#ifndef BT_DISCOVERY_FALLBACK_MS
#define BT_DISCOVERY_FALLBACK_MS 6000u
#endif

static bool bt_transport_wants_classic(bt_transport_mode_t mode)
{
  return mode != BT_TRANSPORT_BLE_ONLY;
}

static bool bt_transport_wants_ble(bt_transport_mode_t mode)
{
  return mode != BT_TRANSPORT_CLASSIC_ONLY;
}

static void bt_set_active_transport(bt_transport_mode_t mode)
{
  _bt_transport_mode = bt_transport_normalize((int)mode, BT_TRANSPORT_CLASSIC_ONLY);
  bt_transport_set_active_mode(_bt_transport_mode);
}

static bt_transport_mode_t bt_resolve_boot_transport(bt_transport_mode_t requested)
{
  requested = bt_transport_normalize((int)requested, BT_TRANSPORT_DUAL);
  return requested == BT_TRANSPORT_DUAL ? BT_TRANSPORT_CLASSIC_ONLY : requested;
}

static uint32_t bt_heap_free_i8()
{
  return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

static uint32_t bt_heap_largest_i8()
{
  return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

static bool bt_classic_heap_guard_ok(uint32_t min_free, uint32_t min_largest)
{
#if BT_CLASSIC_HEAP_GUARD
  return bt_heap_free_i8() >= min_free && bt_heap_largest_i8() >= min_largest;
#else
  return true;
#endif
}

static void bt_disable_classic_for_heap()
{
  if (!bt_transport_wants_classic(_bt_transport_mode))
    return;
  hid_close();
  bt_set_active_transport(BT_TRANSPORT_BLE_ONLY);
}

static void bt_runtime_heap_guard_before_discovery()
{
  if (_bt_transport_mode == BT_TRANSPORT_DUAL && hid_connected_count() == 0 &&
      !bt_classic_heap_guard_ok(BT_CLASSIC_RUNTIME_MIN_FREE_I8, BT_CLASSIC_RUNTIME_MIN_LARGEST_I8))
    bt_disable_classic_for_heap();
}

static void bt_start_discovery_once(const char* phase)
{
  if (_bt_discovery_started)
    return;

  uint32_t now = millis();
  if ((int32_t)(now - _bt_discovery_next_retry_ms) < 0)
    return;

  bt_runtime_heap_guard_before_discovery();

  int bt_pairing_enabled = bt_pairing_set_enabled(1);
#if EVENT_LOGS
  printf("[bt] discovery phase=%s transport=%s pairing=%d free_i8=%lu largest_i8=%lu\n",
         phase ? phase : "unknown", bt_transport_name(_bt_transport_mode),
         bt_pairing_enabled,
         (unsigned long)bt_heap_free_i8(),
         (unsigned long)bt_heap_largest_i8());
#endif
  if (bt_pairing_enabled)
    _bt_discovery_started = true;
  else
    _bt_discovery_next_retry_ms = now + 2000u;
}

void emu_init()
{
    std::string folder = std::string(MOUNT_POINT) + "/" + _emu->name;
    gui_start(_emu,folder.c_str());
    _drawn = _frame_counter;
}

void emu_loop()
{
    const bool host_commit_before = gui_video_needs_host_commit();
    const bool gb_direct_present_before =
        _emu && _emu->flavor == EMU_GB && !host_commit_before;

    // wait for blanking before drawing to avoid tearing
    if (_emu && host_commit_before && midi_visualizer_is_active() &&
        (_emu->flavor == EMU_GB || _emu->flavor == EMU_NES)) {
        // Service MIDI and audio in 1 ms slices while waiting for blanking.
        gui_music_audio_pre_sync();
        while (video_scan_in_active()) {
            vTaskDelay(1);
            gui_music_audio_pre_sync();
        }
    }
    else if (_emu && _emu->flavor == EMU_GB) {
        if (host_commit_before)
            video_sync();
        else
            video_sync_gb_single_buffer_render_slot();
    }
    else if (_emu && _emu->flavor == EMU_NES && !host_commit_before) {
        if (_emu->standard == NTSC)
            video_sync_ntsc_single_buffer_render_slot();
        else
            video_sync_pal_nes_single_buffer_render_slot();
    }
    else
        video_sync();

    static int last_visualizer_frame = -1;
    if (midi_visualizer_is_active()) {
        int current_frame = _frame_counter;
        if (current_frame == last_visualizer_frame) {
            vTaskDelay(1);
            return;
        }
        last_visualizer_frame = current_frame;
    } else {
        last_visualizer_frame = -1;
    }

    static bool host_delta_ready = false;
    static uint32_t host_delta_last_video = 0;
    uint32_t host_delta_video = (uint32_t)_frame_counter;
    uint32_t host_delta = 1;
    if (host_delta_ready) {
      host_delta = host_delta_video - host_delta_last_video;
      if (host_delta < 1)
        host_delta = 1;
      else {
        const uint32_t host_delta_max =
          (_emu && _emu->flavor == EMU_GB) ? (uint32_t)GB_HOST_FRAME_DELTA_MAX : 8u;
        if (host_delta > host_delta_max)
          host_delta = host_delta_max;
      }
    } else {
      host_delta_ready = true;
    }
    host_delta_last_video = host_delta_video;
    if (_emu)
      _emu->set_host_frame_delta((int)host_delta);

    // Draw a frame, update sound, process hid events
    uint32_t t = xthal_get_ccount();
    gui_update();
    _frame_time = xthal_get_ccount() - t;
    const bool gb_direct_present_after =
        _emu && _emu->flavor == EMU_GB && !gui_video_needs_host_commit();
    if (!gb_direct_present_after) {
      if (gb_direct_present_before)
        video_sync();
      if (gui_video_commit_needs_vblank_sync())
        video_sync_for_frame_commit();
      video_clear_queued_lines();
      _lines = gui_video_buffer();
    }
    _drawn++;
}

// Dual-core mode runs the emulator on the communications core.
void emu_task(void* arg)
{
    printf("emu_task %s running on core %d at %dmhz\n",
      _emu->name.c_str(),xPortGetCoreID(),rtc_clk_cpu_freq_value(rtc_clk_cpu_freq_get()));
    emu_init();
    uint32_t idle_slice = 0;
    for (;;) {
      emu_loop();
      // Give IDLE0 time after long frames to service the watchdog and controls.
      if ((++idle_slice & 0x07) == 0 || _frame_time > 4800000u)
        vTaskDelay(1);
    }
}

esp_err_t mount_filesystem()
{
  printf("\n\n\nGlitch Boy\n\nmounting sd card....\n");
  uint32_t t = millis();

  printf("SD card pins - MOSI:%d, MISO:%d, CLK:%d, CS:%d\n",
         PIN_NUM_MOSI, PIN_NUM_MISO, PIN_NUM_CLK, PIN_NUM_CS);

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
  slot_config.gpio_miso = (gpio_num_t)PIN_NUM_MISO;
  slot_config.gpio_mosi = (gpio_num_t)PIN_NUM_MOSI;
  slot_config.gpio_sck  = (gpio_num_t)PIN_NUM_CLK;
  slot_config.gpio_cs   = (gpio_num_t)PIN_NUM_CS;

  esp_vfs_fat_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024
  };
  sdmmc_card_t *card;
  esp_err_t ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);
  printf("SD card mount result: %d (%s)\n", ret, esp_err_to_name(ret));
  if (ret == ESP_OK) {
      printf("... mounted in %d ms\n", millis() - t);
      printf("SD card info: %s, %d MB\n", card->cid.name, (int)(card->csd.capacity / 1024 / 1024));
      return ESP_OK;
  }
  printf("Failed to mount SD card: %d (%s)\n", ret, esp_err_to_name(ret));

  // Fall back to SPIFFS
  printf("mounting spiffs (will take ~15 seconds if formatting for the first time)....\n");
  t = millis();
  esp_vfs_spiffs_conf_t conf = {
      .base_path = MOUNT_POINT,
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
  };
  ret = esp_vfs_spiffs_register(&conf);
  if (ret != ESP_OK)
      printf("Failed to mount or format filesystem: %d. Use 'ESP32 Sketch Data Upload' from 'Tools' menu\n", ret);
  vTaskDelay(1);
  printf("... mounted in %d ms\n", millis() - t);
  return ret;
}

void setup()
{
#if EVENT_LOGS && !SERIAL_DIAG_LOGS
  esp_log_level_set("*", ESP_LOG_NONE);
#endif
#if EVENT_LOGS
  Serial.begin(EVENT_LOG_BAUD);
  delay(200);
  printf("[event-log] enabled baud=%lu periodic=off\n",
         (unsigned long)EVENT_LOG_BAUD);
#endif

  // newlib creates its time lock lazily on first gettimeofday(), which FAT
  // calls when a file is written. If that first call comes when the heap is
  // exhausted (late in a GB session), lock creation abort()s. Create it now.
  {
    struct timeval tv;
    gettimeofday(&tv, NULL);
  }

  bt_transport_mode_t boot_transport_pref =
    bt_transport_load_pref((bt_transport_mode_t)BT_TRANSPORT_DEFAULT_MODE);
  bt_set_active_transport(bt_resolve_boot_transport(boot_transport_pref));
  rtc_clk_cpu_freq_set(RTC_CPU_FREQ_240M);

  // Mount SD before another subsystem claims SPI2.
  printf("=== EARLY SD CARD TEST ===\n");
  printf("Attempting SD mount before any other initialization...\n");
  mount_filesystem();

  _emu = NewEmulator();
#if EVENT_LOGS
  printf("[bt] setup transport=%s ble_available=%d free_i8=%lu largest_i8=%lu\n",
         bt_transport_name(_bt_transport_mode), ble_hogp_is_available(),
         (unsigned long)bt_heap_free_i8(),
         (unsigned long)bt_heap_largest_i8());
#endif
  if (bt_transport_wants_classic(_bt_transport_mode)) {
    bool init_classic = true;
    if (_bt_transport_mode == BT_TRANSPORT_DUAL)
      init_classic = bt_classic_heap_guard_ok(BT_CLASSIC_INIT_MIN_FREE_I8, BT_CLASSIC_INIT_MIN_LARGEST_I8);

    if (init_classic) {
      hid_init("emu32");
      if (_bt_transport_mode == BT_TRANSPORT_DUAL &&
          !bt_classic_heap_guard_ok(BT_CLASSIC_POST_INIT_MIN_FREE_I8, BT_CLASSIC_POST_INIT_MIN_LARGEST_I8))
        bt_disable_classic_for_heap();
    } else {
      bt_set_active_transport(BT_TRANSPORT_BLE_ONLY);
    }
  }

  if (bt_transport_wants_ble(_bt_transport_mode))
    ble_hogp_init("emu32");

  gpio_input_init();
  glitch_controls_init();
  midi_input_init();

  #ifdef SINGLE_CORE
  emu_init();
  video_init(_emu->cc_width,_emu->flavor,_emu->composite_palette(),_emu->standard);
  bt_start_discovery_once("setup:after_video_init_single");
  #else
  const int emu_task_priority = (_emu && (_emu->flavor == EMU_NES || _emu->flavor == EMU_GB)) ? 2 : 1;
  const int emu_task_stack = (_emu && _emu->flavor == EMU_GB) ? 10*1024 : ((_emu && _emu->flavor == EMU_NES) ? 5*1024 : 3*1024);
  xTaskCreatePinnedToCore(emu_task, "emu_task", emu_task_stack, NULL, emu_task_priority, NULL, 0);
  #endif
}

// Runs on application core 1.
void loop()
{
  #ifdef SINGLE_CORE
  emu_loop();
  #else
  // Start video after the emulator is ready.
  if (!_inited) {
    if (_lines) {
      printf("video_init\n");
      video_init(_emu->cc_width,_emu->flavor,_emu->composite_palette(),_emu->standard);
      _inited = true;
      bt_start_discovery_once("loop:after_video_init");
    } else {
      vTaskDelay(1);
      if ((uint32_t)millis() >= BT_DISCOVERY_FALLBACK_MS)
        bt_start_discovery_once("loop:fallback_no_video");
    }
  }
  if (!_bt_discovery_started && (_inited || (uint32_t)millis() >= BT_DISCOVERY_FALLBACK_MS))
    bt_start_discovery_once(_inited ? "loop:discovery_retry" : "loop:fallback_retry");
  #endif

  // Capture UART MIDI continuously; the emulator core parses the ring.
  midi_input_uart_poll();

  if (_bt_transport_mode != BT_TRANSPORT_BLE_ONLY)
    hid_update();
  if (_bt_transport_mode != BT_TRANSPORT_CLASSIC_ONLY)
    ble_hogp_update();

  #ifndef SINGLE_CORE
  // Leave core 1 idle time for Bluetooth and video ISR timing.
  vTaskDelay(1);
  #endif
}
//Original copyright notice from esp_8_bit

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
