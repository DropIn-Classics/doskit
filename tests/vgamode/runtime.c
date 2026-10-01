/* runtime.c - the runtime's vga.c in the 16-colour 200-line modes 0Dh and
 * 0Eh (selftest step 4): the picture's size, a planar pixel where it
 * belongs with the attribute controller's colour, the refresh rate.
 * The start address latched at the retrace (vga_set_start_latch).
 * Prints "vga modes ok", exit status 0, or what went wrong. */
#include <math.h>
#include <stdio.h>
#include "vga.h"

static VgaFrame pic;

static int check(int mode, int width)
{
    int x;

    vga_set_mode(mode);
    /* pixel 9 (the second byte's bit 6) in colour 0Ah: planes 1 and 3 */
    vga_outw(0x3C4, 0x0A02);
    vga_write(1, 0x40);
    /* the last pixel of line 199 in colour 0Fh */
    vga_outw(0x3C4, 0x0F02);
    vga_write((uint16_t)(199 * (width / 8) + width / 8 - 1), 0x01);
    vga_render(&pic);
    if (pic.width != width || pic.height != 200) {
        printf("mode %02Xh: %dx%d, not %dx200\n", mode, pic.width, pic.height, width);
        return 1;
    }
    for (x = 0; x < width; x++) {
        /* the attribute registers of these modes: 8-15 to DAC 10h-17h */
        int want = x == 9 ? 0x12 : 0;
        if (pic.pixels[x] != want) {
            printf("mode %02Xh: pixel %d is %02X, not %02X\n", mode, x, pic.pixels[x], want);
            return 1;
        }
    }
    if (pic.pixels[199 * width + width - 1] != 0x17) {
        printf("mode %02Xh: the last pixel is %02X, not 17\n", mode, pic.pixels[199 * width + width - 1]);
        return 1;
    }
    if (fabs(vga_refresh_hz() - 70.0) > 0.2) {
        printf("mode %02Xh: %.2f Hz, not 70\n", mode, vga_refresh_hz());
        return 1;
    }
    return 0;
}

/* vga_set_start_latch: a start address written during a picture shows
 * from the next retrace (vga_frame_start) on, not at once */
static int check_latch(void)
{
    vga_set_mode(0x0D);
    vga_outw(0x3C4, 0x0F02);
    vga_write(40, 0xFF);                /* line 1's first 8 pixels, colour 0Fh */
    vga_set_start_latch(1);
    vga_outw(0x3D4, 0x0C);              /* start address 40: line 1 on top */
    vga_outw(0x3D4, 0x280D);
    vga_render(&pic);
    if (pic.pixels[0] != 0) {
        printf("latch: the start address written showed before the retrace\n");
        return 1;
    }
    vga_frame_start();
    vga_render(&pic);
    if (pic.pixels[0] != 0x17) {
        printf("latch: the start address not taken at the retrace (pixel %02X)\n", pic.pixels[0]);
        return 1;
    }
    vga_set_start_latch(0);
    return 0;
}

int main(void)
{
    if (check(0x0D, 320) || check(0x0E, 640) || check_latch())
        return 1;
    printf("vga modes ok\n");
    return 0;
}
