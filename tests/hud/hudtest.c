/* hudtest.c - hud.c: the box, a letter, the bar's full and empty steps,
 * the size in a large picture, the count of pictures shown.  Prints
 * "hud ok" or what failed. */
#include <stdio.h>
#include <string.h>
#include "hud.h"

static VgaFrame f;
static int fails;

static void expect(const char *what, int x, int y, int colour)
{
    int got = f.pixels[y * f.width + x];
    if (got != colour) {
        printf("%s at %d,%d: colour %d, not %d\n", what, x, y, got, colour);
        fails++;
    }
}

static void picture(int w, int h)
{
    int i;

    f.width = w;
    f.height = h;
    memset(f.pixels, 5, sizeof f.pixels);
    for (i = 0; i < 256; i++)
        f.palette[i] = 0x808080;
    f.palette[1] = 0x000000;
    f.palette[9] = 0xFFFFFF;
}

int main(void)
{
    /* "VOLUME" and 10 steps, 3 full: 35 + 5 + 39 = 79 pixels wide, from
     * x (320 - 79) / 2 = 120, the box from y 4 */
    picture(320, 200);
    hud_show("VOLUME", 3, 10, 2);
    hud_draw(&f);
    expect("the box's corner", 116, 4, 1);
    expect("V's top left", 120, 6, 9);
    expect("V's top middle", 122, 6, 1);
    expect("the first step", 160, 6, 9);
    expect("the third step's foot", 168, 12, 9);
    expect("the fourth step's top", 172, 6, 1);
    expect("the fourth step's foot", 172, 12, 9);
    expect("outside the box", 115, 4, 5);
    hud_draw(&f);
    picture(320, 200);
    hud_draw(&f);
    expect("after the pictures shown", 120, 6, 5);

    /* "MUTE", no bar, at 800x600: twice the size; 23 pixels, x 377 */
    picture(800, 600);
    hud_show("Mute", 0, 0, 1);
    hud_draw(&f);
    expect("M's top left", 377, 12, 9);
    expect("M's top left, the next picture pixel", 378, 13, 9);
    expect("the box's corner", 369, 8, 1);
    expect("above the box", 369, 7, 5);

    if (!fails)
        printf("hud ok\n");
    return fails != 0;
}
