/* vim: set tabstop=3 expandtab:
**
** This file is in the public domain.
**
** osd.c
**
** $Id: osd.c,v 1.2 2001/04/27 14:37:11 neil Exp $
**
*/

#include "errno.h"
#include "fcntl.h"
#include "limits.h"
#include "signal.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "sys/time.h"
#include "sys/stat.h"
#include "sys/types.h"
#include "unistd.h"

#include "noftypes.h"
#include "nofconfig.h"
#include "log.h"
#include "osd.h"
#include "nofrendo.h"
#include "event.h"

#include "version.h"
#include "nes.h"

#include "nes_apu_midi.h"

// TODO. this is really ugly. need to resolve with emu_nofrendo

#define  NES_CLOCK_DIVIDER    12
#define  NES_MASTER_CLOCK     (236250000 / 11)
#define  NES_SCANLINE_CYCLES  (1364.0 / NES_CLOCK_DIVIDER)
#define  NES_FIQ_PERIOD       (NES_MASTER_CLOCK / NES_CLOCK_DIVIDER / 60)

char configfilename[]="na";

/* This is os-specific part of main() */
int osd_main(int argc, char *argv[])
{
   config.filename = configfilename;
   return main_loop("rom", system_autodetect);
}

/* File system interface */
void osd_fullname(char *fullname, const char *shortname)
{
   strncpy(fullname, shortname, PATH_MAX);
}

/* This gives filenames for storage of saves */
char *osd_newextension(char *string, char *ext)
{
   char *dot;
   char *sep;

   if (NULL == string || NULL == ext)
      return string;

   sep = strrchr(string, PATH_SEP);
#ifdef ALT_PATH_SEP
   {
      char *alt_sep = strrchr(string, ALT_PATH_SEP);
      if (alt_sep && (!sep || alt_sep > sep))
         sep = alt_sep;
   }
#endif

   dot = strrchr(string, '.');
   if (dot && (!sep || dot > sep))
      *dot = '\0';

   strncat(string, ext, PATH_MAX - strlen(string));
   string[PATH_MAX] = '\0';
   return string;
}

/* This gives filenames for storage of PCX snapshots */
int osd_makesnapname(char *filename, int len)
{
   return -1;
}

//=======


#define  DEFAULT_SAMPLERATE   22100
#define  DEFAULT_FRAGSIZE     128

#define  DEFAULT_WIDTH        256
#define  DEFAULT_HEIGHT       240

static int init(int width, int height) { return 0; };
static void shutdown(void) {};
static int set_mode(int width, int height) { return 0; };

uint32 nes_pal[256];
static void set_palette(rgb_t *pal)
{
    for (int i = 0; i < 256; i++)
        nes_pal[i] = (pal[i].r << 16) | (pal[i].g << 8) | pal[i].b;
};
static void clear(uint8 color) {};


/* acquire the directbuffer for writing */
bitmap_t* myBitmap = 0;
uint8 fb[1];  // dummy
static bitmap_t *lock_write(void)
{
//   SDL_LockSurface(mySurface);
   myBitmap = bmp_createhw((uint8*)fb, DEFAULT_WIDTH, DEFAULT_HEIGHT, 0);
   return myBitmap;
}

/* release the resource */
static void free_write(int num_dirties, rect_t *dirty_rects)
{
   bmp_destroy(&myBitmap);
}

static void custom_blit(bitmap_t *bmp, int num_dirties, rect_t *dirty_rects)
{
};

viddriver_t sdlDriver =
{
   "Simple DirectMedia Layer",         /* name */
   init,          /* init */
   shutdown,      /* shutdown */
   set_mode,      /* set_mode */
   set_palette,   /* set_palette */
   clear,         /* clear */
   lock_write,    /* lock_write */
   free_write,    /* free_write */
   custom_blit,   /* custom_blit */
   false          /* invalidate flag */
};

void osd_getvideoinfo(vidinfo_t *info)
{
   info->default_width = DEFAULT_WIDTH;
   info->default_height = DEFAULT_HEIGHT;
   info->driver = &sdlDriver;
}

/* flip between full screen and windowed */
void osd_togglefullscreen(int code)
{
}

//
int _input_mask;
void input_key(int k, int down)
{
    int b = -1;
    switch (k) {
        case 0x400000E2: b = 0; break; // option->a
        case 0x400000E1: b = 1; break; // shift->b
        case 0x0D:       b = 2; break; // return->start
        case 0x09:       b = 3; break; // tab->select
        case 0x40000052: b = 4; break; // up
        case 0x40000051: b = 5; break; // down
        case 0x40000050: b = 6; break; // left
        case 0x4000004F: b = 7; break; // right

        case 'r': b = 8; break; // soft reset
        case 't': b = 9; break; // hard reset
        default:
            return;
    }
    b = 1 << b;
    if (down)
        _input_mask |= b;
    else
        _input_mask &= ~b;
}

void osd_getinput(void)
{
    const int ev[16]= {
        event_joypad1_a,
        event_joypad1_b,
        event_joypad1_start,
        event_joypad1_select,

        event_joypad1_up,
        event_joypad1_down,
        event_joypad1_left,
        event_joypad1_right,

        event_soft_reset,
        event_hard_reset
    };

    static int oldb=0xffff;
    int b =_input_mask;
    int chg = b^oldb;
    int x;
    oldb=b;
    event_t evh;
    for (x=0; x<16; x++) {
        if (chg&1) {
            evh=event_get(ev[x]);
            if (evh) evh(b & 1);
        }
        chg>>=1;
        b>>=1;
    }
}

void osd_getmouse(int *x, int *y, int *button)
{
}

//Seemingly, this will be called only once. Should call func with a freq of frequency,
int osd_installtimer(int frequency, void *func, int funcsize, void *counter, int countersize)
{
    printf("Timer install, freq=%d\n", frequency);
   return 0;
}

int osd_init()
{
    return 0;
}

nes_t* _nes_p = 0;
static int _nes_osd_inited = 0;
extern bitmap_t *primary_buffer;

static int nes_emulate_runtime_init(int width, int height)
{
    if (!_nes_osd_inited) {
        if (log_init())
        {
            printf("nofrendo init: log_init failed\n");
            return -1;
        }
        if (vid_init(width,height,&sdlDriver))
        {
            printf("nofrendo init: vid_init failed\n");
            return -1;
        }
        event_init();
        _nes_osd_inited = 1;
    }
    event_set_system(system_nes);
    return 0;
}

void nes_emulate_shutdown(void)
{
    if (_nes_p) {
        nes_destroy(&_nes_p);
        _nes_p = 0;
    }
}

uint8_t** nes_emulate_boot_buffer(int width, int height)
{
    if (nes_emulate_runtime_init(width, height) != 0)
        return 0;
    if (vid_setmode(NES_SCREEN_WIDTH, 240))
    {
        printf("nofrendo boot: vid_setmode failed\n");
        return 0;
    }
    if (primary_buffer)
        return primary_buffer->line;
    return 0;
}

int nes_emulate_init(const char* path, int width, int height)
{
    /* Recreate the machine without re-registering process-wide OSD state. */
    nes_emulate_shutdown();
    if (nes_emulate_runtime_init(width, height) != 0)
        return -1;
    _nes_p = nes_create();
    if (NULL == _nes_p) {
        printf("nofrendo init: nes_create failed\n");
        return -1;
    }
    if (vid_setmode(NES_SCREEN_WIDTH, 240))
    {
        printf("nofrendo init: vid_setmode failed\n");
        nes_destroy(&_nes_p);
        return -1;
    }
    if (nes_insertcart(path,_nes_p)) {
        printf("nofrendo init: nes_insertcart failed for %s\n", path);
        _nes_p = 0;
        return -1;
    }
    // Frame-by-frame mode still needs the APU callback.
    if (_nes_p && _nes_p->apu && _nes_p->apu->process) {
        osd_setsound(_nes_p->apu->process);
    }
    /* Restore APU MIDI settings after creating the APU. */
    nes_apu_midi_load_prefs();
    return 0;
}

void nes_renderframe(bool draw_flag);

// emulate a frame, return
uint8** nes_emulate_frame(bool draw_flag)
{
    nes_renderframe(draw_flag);
    if (draw_flag)
        vid_flush();
    osd_getinput();
    if (primary_buffer)
        return primary_buffer->line;
    return NULL;
}
