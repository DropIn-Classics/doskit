/* platmousetest.c - platform.h's mouse as a device (plat_mouse_motion,
 * plat_mouse_grab) on plat_null.c, scripted by selftest.py with
 *
 *   DK_MOUSEMOVE="2:5,-3,1 2:1,1,1 4:0,0,0 6:-7,2,6 8:9,9,0 9:4,0,0"
 *
 * Nothing before the first event; the movement summed up between two
 * askings and given once; the buttons held until an event lets go; a
 * grab starting the movement anew, a second one changing nothing; the
 * pointer (plat_mouse) untouched by all of it.  Says "platmouse ok".
 */
#include <stdio.h>
#include <string.h>
#include "platform.h"

static uint8_t pixels[320 * 200];
static uint32_t palette[256];
static int shown;

/* pictures until `n` were shown */
static void show(int n)
{
    for (; shown < n; shown++)
        plat_present(pixels, 320, 200, palette);
}

static int fail(const char *what, int dx, int dy, int buttons, int seen)
{
    printf("platmouse FAILED: %s: %d,%d buttons %d seen %d\n", what, dx, dy, buttons, seen);
    return 1;
}

/* 0 when the mouse says this after `n` pictures */
static int expect(const char *what, int n, int seen, int dx, int dy, int buttons)
{
    int x = -99, y = -99, b = -99, s;

    show(n);
    s = plat_mouse_motion(&x, &y, &b);
    if (s != seen || x != dx || y != dy || b != buttons)
        return fail(what, x, y, b, s);
    return 0;
}

int main(void)
{
    int x, y, clicks;

    if (!plat_init("platmouse"))
        return 1;
    if (expect("before the first event", 1, 0, 0, 0, 0)
        || expect("two events of one picture", 2, 1, 6, -2, 1)
        || expect("asked again", 2, 1, 0, 0, 1)
        || expect("a picture without an event", 3, 1, 0, 0, 1)
        || expect("the button let go", 4, 1, 0, 0, 0)
        || expect("two buttons and a movement", 6, 1, -7, 2, 6))
        return 1;
    show(8);
    plat_mouse_grab(1);
    if (expect("after the grab", 8, 1, 0, 0, 0))
        return 1;
    show(9);
    plat_mouse_grab(1);
    if (expect("grabbed a second time", 9, 1, 4, 0, 0))
        return 1;
    plat_mouse_grab(0);
    if (expect("let go", 9, 1, 0, 0, 0))
        return 1;
    if (plat_mouse(&x, &y, &clicks) || clicks)
        return fail("the pointer", x, y, clicks, 1);
    plat_shutdown();
    printf("platmouse ok\n");
    return 0;
}
