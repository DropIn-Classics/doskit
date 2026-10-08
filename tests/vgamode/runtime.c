/* runtime.c - the runtime's vga.c in the 16-colour 200-line modes 0Dh and
 * 0Eh (selftest step 4): the picture's size, a planar pixel where it
 * belongs with the attribute controller's colour, the refresh rate.
 * The start address latched at the retrace (vga_set_start_latch).
 * The VESA modes 101h and 103h: size and rate, then planar with a
 * narrowed line as Pinball Illusions sets them (the picture in the
 * middle of the mode's width).
 * The state before any mode set (text mode 3's timing and retrace).
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

/* VESA mode `mode`: w x h at `hz`; then unchained (sequencer 4 06h, CR 14h
 * bit 6 off, CR 17h bit 6 on), CR 1 29h (42 character clocks, 336
 * pixels; CR 11h's protection off first), CR 13h 2Ah: the picture still w wide, the 336 in its middle */
static int check_vesa(int mode, int w, int h, double hz)
{
    int left = (w - 336) / 2;

    if (!vga_set_mode_vesa(mode, 1)) {
        printf("VESA %Xh: refused\n", mode);
        return 1;
    }
    vga_render(&pic);
    if (pic.width != w || pic.height != h) {
        printf("VESA %Xh: %dx%d, not %dx%d\n", mode, pic.width, pic.height, w, h);
        return 1;
    }
    if (fabs(vga_refresh_hz() - hz) > 0.2) {
        printf("VESA %Xh: %.2f Hz, not %.2f\n", mode, vga_refresh_hz(), hz);
        return 1;
    }
    vga_outw(0x3D4, (uint16_t)((vga_crtc_reg(0x11) & 0x7F) << 8 | 0x11));  /* CR 0-7 writable */
    vga_outw(0x3C4, 0x0604);
    vga_outw(0x3D4, 0x0014);
    vga_outw(0x3D4, 0xE317);
    vga_outw(0x3D4, 0x2901);
    vga_outw(0x3D4, 0x2A13);
    /* pixel 5 of line 1 (plane 1, byte 84 + 1) in colour 33h */
    vga_outw(0x3C4, 0x0202);
    vga_write(85, 0x33);
    vga_render(&pic);
    if (pic.width != w || pic.pixels[w + left + 5] != 0x33 || pic.pixels[w + left + 4] != 0) {
        printf("VESA %Xh planar: %d wide, pixel %02X\n", mode, pic.width, pic.pixels[w + left + 5]);
        return 1;
    }
    return 0;
}

/* before any mode set: text mode 3's timing, and a wait for the
 * retrace's start (3DAh bit 3) ends within one frame's reads */
static int check_start(void)
{
    int n = 0;

    if (fabs(vga_refresh_hz() - 70.08) > 0.05) {
        printf("start: %.2f Hz, not 70.08\n", vga_refresh_hz());
        return 1;
    }
    while (!(vga_inb(0x3DA) & 8))
        if (++n > 1000) {
            printf("start: no retrace in 1000 reads\n");
            return 1;
        }
    return 0;
}

int main(void)
{
    if (check_start() || check(0x0D, 320) || check(0x0E, 640) || check_latch()
        || check_vesa(0x101, 640, 480, 59.94) || check_vesa(0x103, 800, 600, 60.32)
        || vga_set_mode_vesa(0x105, 1) || check(0x0D, 320))
        return 1;
    printf("vga modes ok\n");
    return 0;
}
