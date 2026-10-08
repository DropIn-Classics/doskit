/* plat_null.c - platform.h without a window or sound: for tests and
 * scripted runs.  The clock is virtual (plat_sleep_ms moves it on), so a
 * run takes no real time and repeats exactly.  Environment:
 *
 *   DK_FRAMES=n     plat_pump() reports the window closed after n pictures
 *   DK_DUMP=file    the last picture is written there as a PPM file
 *   DK_KEYS=...     scan codes by picture number: "120:3B 125:BB" presses
 *                   and releases F1 at pictures 120 and 125; E0 keys as
 *                   "E0-50"
 *   DK_MOUSE=...    the mouse by picture number: "300:320,240,1" moves it
 *                   to 320,240 of a 640x480 picture (in 1/640 and 1/480
 *                   of the picture whatever its size) and clicks the left
 *                   button (2 the right, 0 none)
 *   DK_MOUSEMOVE=...  the mouse as a device by picture number:
 *                   "300:5,-3,1 310:0,0,0" moves it 5 right and 3 up at
 *                   picture 300 with the left button held from there on
 *                   (1 left, 2 right, 4 middle), and lets go at 310
 *   DK_SHOTS=...    screenshots by picture number: "120:a.png 300:b.png"
 *                   writes pictures 120 and 300 as PNG files (shot.h;
 *                   no spaces in the names)
 *   DK_AUDIO=1      a virtual audio device: plat_audio_start says 1, and
 *                   the callback is asked for the frames of the virtual
 *                   clock's time as plat_sleep_ms moves it on (the frames
 *                   due by then less those given; a fraction is carried),
 *                   on the caller's thread.  Without it there is no
 *                   device, as before.
 *   DK_WAV=file     the same, and the frames are written there as a WAVE
 *                   file (stereo, 16 bits), finished by plat_shutdown
 *
 * The audio's clock is the virtual clock and nothing else: time passes
 * only in plat_sleep_ms, so whatever the program does between two sleeps
 * takes none and falls on one frame.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"
#include "vga.h"
#include "shot.h"

static long frames_left = -1, picture;
static const char *dump_path, *keys, *mouse, *moves, *shots;
static int mouse_seen, mouse_x, mouse_y, mouse_clicks;
static int move_seen, move_dx, move_dy, move_held, move_grabbed;
static uint64_t now_us;
static uint8_t pending[64];
static int npending, pos_pending;

static uint8_t last[VGA_MAX_W * VGA_MAX_H];
static uint32_t last_pal[256];
static int last_w, last_h;

int plat_init(const char *title)
{
    const char *n = getenv("DK_FRAMES");
    (void)title;
    if (n)
        frames_left = atol(n);
    dump_path = getenv("DK_DUMP");
    keys = getenv("DK_KEYS");
    mouse = getenv("DK_MOUSE");
    moves = getenv("DK_MOUSEMOVE");
    shots = getenv("DK_SHOTS");
    return 1;
}

static void dump(void)
{
    FILE *f;
    int i;
    if (!dump_path || !last_w)
        return;
    f = fopen(dump_path, "wb");
    if (!f)
        return;
    fprintf(f, "P6\n%d %d\n255\n", last_w, last_h);
    for (i = 0; i < last_w * last_h; i++) {
        uint32_t c = last_pal[last[i]];
        fputc((int)(c >> 16) & 0xFF, f);
        fputc((int)(c >> 8) & 0xFF, f);
        fputc((int)c & 0xFF, f);
    }
    fclose(f);
}

/* ---- the virtual audio device (DK_AUDIO, DK_WAV) ---- */

static PlatAudioFill audio_fill;
static void *audio_user;
static int audio_rate;
static uint64_t audio_from_us;          /* the clock at plat_audio_start */
static uint64_t audio_given;            /* frames the callback was asked for */
static FILE *wav;

static void put_le(FILE *f, uint32_t v, int bytes)
{
    while (bytes-- > 0) {
        fputc((int)(v & 0xFF), f);
        v >>= 8;
    }
}

/* the WAVE file's header for `frames` frames, at the file's start */
static void wav_header(uint64_t frames)
{
    uint32_t bytes = (uint32_t)(frames * 4);

    fseek(wav, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, wav);
    put_le(wav, 36 + bytes, 4);
    fwrite("WAVEfmt ", 1, 8, wav);
    put_le(wav, 16, 4);
    put_le(wav, 1, 2);                  /* PCM */
    put_le(wav, 2, 2);
    put_le(wav, (uint32_t)audio_rate, 4);
    put_le(wav, (uint32_t)audio_rate * 4, 4);
    put_le(wav, 4, 2);
    put_le(wav, 16, 2);
    fwrite("data", 1, 4, wav);
    put_le(wav, bytes, 4);
}

/* asks the callback for the frames due at the clock's time */
static void audio_run(void)
{
    static int16_t buf[1024 * 2];
    uint64_t due;

    if (!audio_fill)
        return;
    due = (now_us - audio_from_us) * (uint64_t)audio_rate / 1000000;
    while (audio_given < due) {
        int i, n = due - audio_given > 1024 ? 1024 : (int)(due - audio_given);

        memset(buf, 0, sizeof buf);
        audio_fill(buf, n, audio_user);
        audio_given += (uint64_t)n;
        for (i = 0; wav && i < n * 2; i++)
            put_le(wav, (uint16_t)buf[i], 2);
    }
}

void plat_shutdown(void)
{
    dump();
    if (wav) {
        wav_header(audio_given);
        fclose(wav);
        wav = NULL;
    }
    audio_fill = NULL;
}

void plat_message(const char *text)
{
    fprintf(stderr, "%s\n", text);
}

/* the scan codes DK_KEYS gives for picture `picture` */
static void keys_for_picture(void)
{
    const char *p = keys;
    npending = pos_pending = 0;
    while (p && *p) {
        char *end;
        long at = strtol(p, &end, 10);
        if (end == p || *end != ':')
            break;
        p = end + 1;
        if (at == picture && npending < (int)sizeof pending - 1) {
            if (!strncmp(p, "E0-", 3)) {
                pending[npending++] = 0xE0;
                p += 3;
            }
            pending[npending++] = (uint8_t)strtol(p, NULL, 16);
        }
        while (*p && *p != ' ')
            p++;
        while (*p == ' ')
            p++;
    }
}

/* what DK_MOUSE gives for picture `picture` */
static void mouse_for_picture(void)
{
    const char *p = mouse;
    while (p && *p) {
        char *end;
        long at = strtol(p, &end, 10), x, y, c;
        if (end == p || *end != ':')
            break;
        x = strtol(end + 1, &end, 10);
        y = *end == ',' ? strtol(end + 1, &end, 10) : 0;
        c = *end == ',' ? strtol(end + 1, &end, 10) : 0;
        if (at == picture) {
            mouse_seen = 1;
            mouse_x = (int)(x * 65536 / 640);
            mouse_y = (int)(y * 65536 / 480);
            mouse_clicks |= (int)c;
        }
        p = end;
        while (*p == ' ')
            p++;
    }
}

/* what DK_MOUSEMOVE gives for picture `picture` */
static void moves_for_picture(void)
{
    const char *p = moves;
    while (p && *p) {
        char *end;
        long at = strtol(p, &end, 10), x, y, b;
        if (end == p || *end != ':')
            break;
        x = strtol(end + 1, &end, 10);
        y = *end == ',' ? strtol(end + 1, &end, 10) : 0;
        b = *end == ',' ? strtol(end + 1, &end, 10) : 0;
        if (at == picture) {
            move_seen = 1;
            move_dx += (int)x;
            move_dy += (int)y;
            move_held = (int)b & 7;
        }
        p = end;
        while (*p == ' ')
            p++;
    }
}

/* the screenshots DK_SHOTS asks for at picture `picture` */
static void shots_for_picture(void)
{
    const char *p = shots;
    while (p && *p) {
        char *end, path[512];
        long at = strtol(p, &end, 10);
        size_t n;
        if (end == p || *end != ':')
            break;
        p = end + 1;
        for (n = 0; p[n] && p[n] != ' '; n++)
            ;
        if (at == picture && n < sizeof path) {
            memcpy(path, p, n);
            path[n] = 0;
            if (!shot_save(path, NULL, 0))
                fprintf(stderr, "cannot write %s\n", path);
        }
        p += n;
        while (*p == ' ')
            p++;
    }
}

int plat_mouse(int *x, int *y, int *clicks)
{
    *clicks = mouse_clicks;
    mouse_clicks = 0;
    *x = mouse_x;
    *y = mouse_y;
    return mouse_seen;
}

/* nothing to keep it to here; a grab starts the movement anew, as the
 * windows' do */
void plat_mouse_grab(int on)
{
    if (!on == !move_grabbed)
        return;
    move_grabbed = on != 0;
    move_dx = move_dy = 0;
}

int plat_mouse_motion(int *dx, int *dy, int *buttons)
{
    *dx = move_dx;
    *dy = move_dy;
    *buttons = move_held;
    move_dx = move_dy = 0;
    return move_seen;
}

int plat_pump(void)
{
    if (frames_left == 0)
        return 0;
    return 1;
}

int plat_has_window(void)
{
    return 0;
}

void plat_set_fullscreen(int on)
{
    (void)on;
}

void plat_set_vsync(int on)
{
    (void)on;
}

int plat_fullscreen(void)
{
    return 0;
}

void plat_present(const uint8_t *pixels, int width, int height, const uint32_t palette[256])
{
    if (width * height <= (int)sizeof last) {
        memcpy(last, pixels, (size_t)(width * height));
        memcpy(last_pal, palette, sizeof last_pal);
        last_w = width;
        last_h = height;
    }
    shot_keep(pixels, width, height, palette);
    shots_for_picture();
    picture++;
    if (frames_left > 0)
        frames_left--;
    keys_for_picture();
    mouse_for_picture();
    moves_for_picture();
}

int plat_read_scancode(void)
{
    return pos_pending < npending ? pending[pos_pending++] : -1;
}

int plat_read_control(void)
{
    return -1;
}

uint64_t plat_micros(void)
{
    return now_us;
}

void plat_sleep_ms(int ms)
{
    now_us += (uint64_t)(ms > 0 ? ms : 1) * 1000;      /* 0 (a yield) too */
    audio_run();
}

int plat_audio_start(int rate, PlatAudioFill fill, void *user)
{
    const char *path = getenv("DK_WAV"), *on = getenv("DK_AUDIO");

    if (!(path && *path) && !(on && *on && strcmp(on, "0")))
        return 0;               /* no device: sound.c moves the module */
    if (rate <= 0 || !fill || audio_fill)
        return 0;
    audio_rate = rate;
    audio_user = user;
    audio_from_us = now_us;
    audio_given = 0;
    if (path && *path) {
        wav = fopen(path, "wb");
        if (!wav) {
            fprintf(stderr, "cannot write %s\n", path);
            return 0;
        }
        wav_header(0);
    }
    audio_fill = fill;
    return 1;
}

void plat_audio_lock(void)
{
}

void plat_audio_unlock(void)
{
}
