/* vsynctest.c - plat_set_vsync on the headless platform: pictures go
 * through with the switch off, on and off again, each shot as handed
 * in (the null backend ignores the switch; only a real panel behind a
 * windowed renderer can show tearing).
 *
 *     vsynctest DIR
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "platform.h"
#include "shot.h"

#define W 160
#define H 100

static uint8_t pix_a[W * H], pix_b[W * H];
static uint32_t pal[256];

static int save(const char *dir, const char *name, const uint8_t *pix)
{
    char path[512];

    snprintf(path, sizeof path, "%s/%s.png", dir, name);
    return shot_png(path, pix, W, H, pal);
}

int main(int argc, char **argv)
{
    int x, y;

    if (argc != 2) {
        fprintf(stderr, "usage: vsynctest DIR\n");
        return 2;
    }
    for (x = 0; x < 256; x++)
        pal[x] = ((uint32_t)x * 379u) & 0xFFFFFFu;
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) {
            pix_a[y * W + x] = (uint8_t)(x + y);
            pix_b[y * W + x] = (uint8_t)(255 - x - y);
        }
    if (!save(argv[1], "expA", pix_a) || !save(argv[1], "expB", pix_b))
        return 1;
    if (!plat_init("vsync"))
        return 1;
    plat_present(pix_a, W, H, pal);
    plat_set_vsync(1);
    plat_present(pix_b, W, H, pal);
    plat_set_vsync(0);
    plat_present(pix_a, W, H, pal);
    plat_shutdown();
    printf("vsync ok\n");
    return 0;
}
