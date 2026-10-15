#include <stdint.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include "nes_apu_midi.h"

/* Provided by the host (hci_server.cpp). */
int sys_get_pref(const char* key, char* value, int max_len);
void sys_set_pref(const char* key, const char* value);

static int parse_pref_int(const char* text, int* out)
{
    if (!text || !out)
        return 0;
    errno = 0;
    char* end = 0;
    long value = strtol(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' ||
        value < INT_MIN || value > INT_MAX)
        return 0;
    *out = (int)value;
    return 1;
}

/* Read an integer pref clamped to [lo, hi]; returns 0 if absent or invalid. */
static int load_pref(const char* key, int lo, int hi, int* out)
{
    char b[16] = {0};
    int v;
    if (!sys_get_pref(key, b, sizeof(b) - 1) || !parse_pref_int(b, &v))
        return 0;
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    *out = v;
    return 1;
}

static int8_t load_pref_s8(const char* key, int lo, int hi, int8_t fallback)
{
    int v;
    return load_pref(key, lo, hi, &v) ? (int8_t)v : fallback;
}

static void save_pref(const char* key, int value)
{
    char b[8];
    sprintf(b, "%d", value);
    sys_set_pref(key, b);
}

void nes_apu_midi_load_prefs(void)
{
    int v;

    nes_apu_midi_set_channel_map(load_pref_s8("nesmid_p1", -1, 15, 0),
                                 load_pref_s8("nesmid_p2", -1, 15, 1),
                                 load_pref_s8("nesmid_tri", -1, 15, 2),
                                 load_pref_s8("nesmid_noi", -1, 15, 9));

    nes_apu_midi_set_duty_cc(load_pref_s8("nesmid_ccp1", -1, 127, 74),
                             load_pref_s8("nesmid_ccp2", -1, 127, 71));

    if (load_pref("nesmid_pbr", 0, 24, &v))
        nes_apu_midi_set_pb_default_range((uint8_t)v);

    if (load_pref("nesmid_ccnoi", -1, 127, &v))
        nes_apu_midi_set_noise_mode_cc((int8_t)v);

    nes_apu_midi_set_env_cc(load_pref_s8("nesmid_em", -1, 127, 78),
                            load_pref_s8("nesmid_ed", -1, 127, 75),
                            load_pref_s8("nesmid_el", -1, 127, 64),
                            load_pref_s8("nesmid_len", -1, 127, 72));
}

void nes_apu_midi_save_prefs(void)
{
    int8_t p1, p2, tri, noi;
    int8_t d1, d2;
    int8_t ccnoi;
    int8_t em, ed, el, len;

    nes_apu_midi_get_channel_map(&p1, &p2, &tri, &noi);
    save_pref("nesmid_p1", p1);
    save_pref("nesmid_p2", p2);
    save_pref("nesmid_tri", tri);
    save_pref("nesmid_noi", noi);

    nes_apu_midi_get_duty_cc(&d1, &d2);
    save_pref("nesmid_ccp1", d1);
    save_pref("nesmid_ccp2", d2);

    save_pref("nesmid_pbr", nes_apu_midi_get_pb_default_range());

    nes_apu_midi_get_noise_mode_cc(&ccnoi);
    save_pref("nesmid_ccnoi", ccnoi);

    nes_apu_midi_get_env_cc(&em, &ed, &el, &len);
    save_pref("nesmid_em", em);
    save_pref("nesmid_ed", ed);
    save_pref("nesmid_el", el);
    save_pref("nesmid_len", len);
}
