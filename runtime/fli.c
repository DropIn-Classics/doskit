/* fli.c - see fli.h.  The format is Autodesk's FLI: a 128-byte header
 * (AF11h), then frames (F1FAh) of chunks: COLOR_64 (the palette, six bits
 * a component), LC (changed lines), BLACK, BRUN (the whole picture, run
 * length coded) and COPY (the whole picture as it is).  The last chunk
 * of a frame can declare a byte more than the file holds (Animator's
 * files do; tools/flifiles.py reports it), so every chunk is read only
 * as far as its frame goes. */
#include <stdlib.h>
#include <string.h>
#include "fli.h"
#include "platform.h"
#include "sys.h"

static unsigned u16(const uint8_t *p) { return (unsigned)(p[0] | p[1] << 8); }
static unsigned long u32(const uint8_t *p) { return u16(p) | (unsigned long)u16(p + 2) << 16; }

static void color_64(Fli *f, const uint8_t *p, const uint8_t *e)
{
    unsigned packets, i, n, c = 0;

    if (e - p < 2)
        return;
    packets = u16(p);
    p += 2;
    while (packets-- && e - p >= 2) {
        c += p[0];
        n = p[1] ? p[1] : 256;
        p += 2;
        for (i = 0; i < n && e - p >= 3; i++, c++, p += 3)
            if (c < 256) {
                f->dac[3 * c] = p[0] & 63;
                f->dac[3 * c + 1] = p[1] & 63;
                f->dac[3 * c + 2] = p[2] & 63;
            }
    }
}

/* n bytes to (x, y), as far as the line and the data go */
static void copy(Fli *f, int x, int y, const uint8_t *p, const uint8_t *e, int n)
{
    if (n > FLI_W - x)
        n = FLI_W - x;
    if (n > e - p)
        n = (int)(e - p);
    if (n > 0 && x >= 0)
        memcpy(f->pixels + y * FLI_W + x, p, (size_t)n);
}

static void fill(Fli *f, int x, int y, uint8_t v, int n)
{
    if (n > FLI_W - x)
        n = FLI_W - x;
    if (n > 0 && x >= 0)
        memset(f->pixels + y * FLI_W + x, v, (size_t)n);
}

/* LC: the first line, the number of lines; per line packets of a skip
 * and a count (positive: bytes, negative: one byte repeated) */
static void lc(Fli *f, const uint8_t *p, const uint8_t *e)
{
    unsigned y, lines, packets;

    if (e - p < 4)
        return;
    y = u16(p);
    lines = u16(p + 2);
    p += 4;
    for (; lines-- && y < FLI_H; y++) {
        int x = 0;
        if (p >= e)
            return;
        for (packets = *p++; packets--; ) {
            int n;
            if (e - p < 2)
                return;
            x += p[0];
            n = (int8_t)p[1];
            p += 2;
            if (n >= 0) {
                copy(f, x, (int)y, p, e, n);
                p += n;
            } else {
                if (p >= e)
                    return;
                fill(f, x, (int)y, *p++, -n);
                n = -n;
            }
            x += n;
        }
    }
}

/* BRUN: each line a packet count (not needed), then counts (positive: one
 * byte repeated, negative: bytes) until the line is full */
static void brun(Fli *f, const uint8_t *p, const uint8_t *e)
{
    int y, x, n;

    for (y = 0; y < FLI_H; y++) {
        if (p >= e)
            return;
        p++;
        for (x = 0; x < FLI_W; x += n) {
            if (p >= e)
                return;
            n = (int8_t)*p++;
            if (n > 0) {
                if (p >= e)
                    return;
                fill(f, x, y, *p++, n);
            } else {
                n = -n;
                if (n == 0)
                    return;
                copy(f, x, y, p, e, n);
                p += n;
            }
        }
    }
}

/* the frame at pos decoded into pixels and dac; the next frame's
 * position, 0 when there is none */
static size_t frame(Fli *f, size_t pos)
{
    const uint8_t *d = f->data;
    size_t size = f->size, fsize, end, c;
    unsigned chunks;

    if (size < pos + 16)
        return 0;
    fsize = u32(d + pos);
    if (fsize < 16)
        return 0;
    end = pos + fsize < size ? pos + fsize : size;
    if (u16(d + pos + 4) == 0xF1FA)
        for (chunks = u16(d + pos + 6), c = pos + 16; chunks-- && c + 6 <= end; ) {
            size_t csize = u32(d + c), cend = c + csize < end ? c + csize : end;
            const uint8_t *p = d + c + 6, *e = d + cend;
            switch (u16(d + c + 4)) {
            case 11: color_64(f, p, e); break;
            case 12: lc(f, p, e); break;
            case 13: memset(f->pixels, 0, sizeof f->pixels); break;
            case 15: brun(f, p, e); break;
            case 16:
                memcpy(f->pixels, p, (size_t)(e - p) < sizeof f->pixels ? (size_t)(e - p) : sizeof f->pixels);
                break;
            }
            if (csize < 6)
                break;
            c += csize;
        }
    return pos + fsize;
}

int fli_open(Fli *f, const uint8_t *data, size_t size)
{
    memset(f, 0, sizeof *f);
    if (size < 128 || u16(data + 4) != 0xAF11 || u16(data + 8) != FLI_W || u16(data + 10) != FLI_H)
        return -1;
    f->data = data;
    f->size = size;
    f->pos = 128;
    f->frames = u16(data + 6);
    f->speed = u16(data + 16);
    return 0;
}

int fli_next(Fli *f)
{
    if (f->shown >= f->frames || !f->pos)
        return 0;
    f->pos = frame(f, f->pos);
    f->shown++;
    return 1;
}

void fli_palette(const uint8_t dac[768], int level, uint32_t out[256])
{
    int i;
    for (i = 0; i < 256; i++) {
        uint32_t c[3];
        int k;
        for (k = 0; k < 3; k++) {
            unsigned v = (unsigned)dac[3 * i + k] * (unsigned)level / 32;
            c[k] = (v << 2) | (v >> 4);
        }
        out[i] = c[0] << 16 | c[1] << 8 | c[2];
    }
}

/* a key as DOS's kbhit sees one: a make code, not of a shift, Ctrl, Alt
 * or a lock key, which put nothing into the BIOS's buffer */
static int is_key(int b)
{
    static int e0;
    int code = b & 0x7F;

    if (b == 0xE0) {
        e0 = 1;
        return 0;
    }
    if (b & 0x80) {
        e0 = 0;
        return 0;
    }
    if (code == 0x1D || code == 0x38 || code == 0x2A || code == 0x36 ||
        (!e0 && (code == 0x3A || code == 0x45 || code == 0x46))) {
        e0 = 0;
        return 0;
    }
    e0 = 0;
    return 1;
}

static void drain_keys(void)
{
    while (plat_read_scancode() >= 0)
        ;
    while (plat_read_control() >= 0)
        ;
}

int fli_show_until(const uint8_t *pixels, const uint32_t palette[256], uint64_t due, int keys)
{
    for (;;) {
        uint64_t now;
        int b;

        if (!plat_pump())
            return FLI_CLOSED;
        while (plat_read_control() >= 0)
            ;
        while ((b = plat_read_scancode()) >= 0)
            if (keys && is_key(b))
                return FLI_KEY;
        plat_present(pixels, FLI_W, FLI_H, palette);
        now = plat_micros();
        if (now >= due)
            return FLI_END;
        plat_sleep_ms(due - now > 10000 ? 10 : (int)((due - now) / 1000));
    }
}

int fli_play_file(const char *path, double frame_seconds, int keys, Fli *last)
{
    static Fli f;
    uint8_t *d;
    size_t size;
    uint32_t palette[256];
    uint64_t start, period;
    int r = FLI_END;

    if ((d = sys_load(path, &size)) == NULL)
        return FLI_END;
    if (fli_open(&f, d, size)) {
        free(d);
        return FLI_END;
    }
    if (frame_seconds <= 0)
        frame_seconds = f.speed / 70.0;
    period = (uint64_t)(frame_seconds * 1e6);
    start = plat_micros();
    while (fli_next(&f)) {
        fli_palette(f.dac, 32, palette);
        r = fli_show_until(f.pixels, palette, start + f.shown * period, keys);
        if (r != FLI_END)
            break;
    }
    if (keys)
        drain_keys();
    free(d);
    f.data = NULL;
    f.size = 0;
    if (last)
        *last = f;
    return r;
}

int fli_fade_out(const Fli *last, int steps, double step_seconds)
{
    uint32_t palette[256];
    uint64_t at = plat_micros();
    int s;

    for (s = steps - 1; s >= 0; s--) {
        fli_palette(last->dac, s * 32 / steps, palette);
        if (fli_show_until(last->pixels, palette, at += (uint64_t)(step_seconds * 1e6), 0) == FLI_CLOSED)
            return 0;
    }
    drain_keys();
    return 1;
}
