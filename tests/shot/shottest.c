/* shottest.c - shot_png on pictures that give deflate work: noise, long
 * runs, rows repeated from far back, a pattern with short repeats, one
 * pixel.  Each goes to DIR/NAME.png, its indexes to DIR/NAME.bin and its
 * palette to DIR/NAME.pal (768 bytes RGB); selftest.py reads the PNGs
 * back and compares.
 *
 *     shottest DIR
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shot.h"

static uint32_t seed = 12345;

static unsigned rnd(void)
{
    seed = seed * 1103515245u + 12345u;
    return seed >> 16;
}

static int write_one(const char *dir, const char *name, const uint8_t *pix, int w, int h,
                     const uint32_t pal[256])
{
    char path[512];
    uint8_t rgb[768];
    FILE *f;
    int i;

    snprintf(path, sizeof path, "%s/%s.png", dir, name);
    if (!shot_png(path, pix, w, h, pal)) {
        fprintf(stderr, "cannot write %s\n", path);
        return 0;
    }
    snprintf(path, sizeof path, "%s/%s.bin", dir, name);
    f = fopen(path, "wb");
    if (!f)
        return 0;
    fwrite(pix, 1, (size_t)w * (size_t)h, f);
    fclose(f);
    for (i = 0; i < 256; i++) {
        rgb[3 * i] = (uint8_t)(pal[i] >> 16);
        rgb[3 * i + 1] = (uint8_t)(pal[i] >> 8);
        rgb[3 * i + 2] = (uint8_t)pal[i];
    }
    snprintf(path, sizeof path, "%s/%s.pal", dir, name);
    f = fopen(path, "wb");
    if (!f)
        return 0;
    fwrite(rgb, 1, sizeof rgb, f);
    fclose(f);
    return 1;
}

int main(int argc, char **argv)
{
    static uint8_t pix[640 * 480];
    uint32_t pal[256];
    int i, x, y, ok = 1;

    if (argc != 2) {
        fprintf(stderr, "usage: shottest DIR\n");
        return 2;
    }
    for (i = 0; i < 256; i++)
        pal[i] = (rnd() << 16 ^ rnd()) & 0xFFFFFF;

    for (i = 0; i < 320 * 200; i++)
        pix[i] = (uint8_t)rnd();
    ok &= write_one(argv[1], "noise", pix, 320, 200, pal);

    memset(pix, 7, sizeof pix);         /* runs longer than 258 */
    for (i = 0; i < 640 * 480; i += 1000 + (int)(rnd() % 3000))
        pix[i] = (uint8_t)rnd();
    ok &= write_one(argv[1], "runs", pix, 640, 480, pal);

    for (y = 0; y < 480; y++)           /* rows again 40 rows (25640 bytes) on */
        for (x = 0; x < 640; x++)
            pix[y * 640 + x] = y < 40 ? (uint8_t)rnd() : pix[(y - 40) * 640 + x];
    ok &= write_one(argv[1], "far", pix, 640, 480, pal);

    for (i = 0; i < 360 * 240; i++)     /* short repeats, all lengths */
        pix[i] = (uint8_t)(i % 5 == 0 ? rnd() % 4 : (unsigned)(i / 7) % 11);
    ok &= write_one(argv[1], "pattern", pix, 360, 240, pal);

    pix[0] = 200;
    ok &= write_one(argv[1], "one", pix, 1, 1, pal);
    return ok ? 0 : 1;
}
