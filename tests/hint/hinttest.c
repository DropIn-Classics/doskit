/* hinttest.c - hint.c on plat_null.c: the card's place, frame, title and
 * text in the layer; the frame around the place and the dimmed picture
 * while the program is held; the German letters; and frame.c's
 * frame_hold: no tick while held, the answering key kept from the
 * program with its release, the release of a key pressed before handed
 * on.  Run with DK_LAYER=640x480 and
 * DK_KEYS="1:1E 4:9E 5:1C 7:9C 12:01 13:81".  Prints "hint ok" or what
 * failed. */
#include <stdio.h>
#include <string.h>
#include "frame.h"
#include "hint.h"
#include "platform.h"
#include "textmode.h"

#define W 640
#define H 480

static uint32_t layer[W * H];
static int fails, ticks, nkeys;
static unsigned char keys[16];

static void expect(const char *what, int got, int want)
{
    if (got != want) {
        printf("%s: %d, not %d\n", what, got, want);
        fails++;
    }
}

static void expect_pixel(const char *what, int x, int y, uint32_t want)
{
    if (layer[y * W + x] != want) {
        printf("%s at %d,%d: %08lX, not %08lX\n", what, x, y,
               (unsigned long)layer[y * W + x], (unsigned long)want);
        fails++;
    }
}

/* the character drawn in the cell at (x, y), a font pixel being one of
 * the layer's: as the font has `c`, in `colour` */
static void expect_char(const char *what, int x, int y, int c, uint32_t colour)
{
    const uint8_t *g = tm_glyph((uint8_t)c);
    int r, b;

    for (r = 0; r < 16; r++)
        for (b = 0; b < 8; b++)
            if ((layer[(y + r) * W + x + b] == colour) != ((g[r] & (0x80 >> b)) != 0)) {
                printf("%s: not the character %02Xh at %d,%d\n", what, c, x, y);
                fails++;
                return;
            }
}

static void tick(void)
{
    ticks++;
}

static void key(unsigned char code)
{
    if (nkeys < (int)sizeof keys)
        keys[nkeys++] = code;
}

static int draw(const Hint *h)
{
    int r;

    memset(layer, 0, sizeof layer);
    hint_show(h);
    r = hint_layer(layer, W, H, NULL);
    hint_show(NULL);
    return r;
}

int main(void)
{
    /* at 640x480 a character is 8x16.  "Ab" in the frame, a line of 3
     * and one of 2 characters, keys of 5: 5 wide inside, 9 with the
     * frame, 7 rows; over the running game at the bottom, a row free
     * below: x (640 - 72) / 2 = 284, y 480 - 8 * 16 = 352 */
    static const Hint card = { "Ab", "one\n\xC3\xA4\xC3\x9F", "Enter", 0, 0, 0, 0, 0, 0 };
    /* a place in the picture's upper left: (32,20) 64x40 of 320x200 is
     * (64,48) 128x96 here; the card goes to the bottom */
    static const Hint place = { NULL, "x", NULL, 32, 20, 64, 40, 320, 200 };
    static const Hint low = { NULL, "x", NULL, 32, 150, 64, 40, 320, 200 };
    static const Hint wide = { NULL, "aaaa bbbb cccc dddd eeee ffff gggg hhhh iiii jjjj kkkk llll mmmm",
                               NULL, 0, 0, 0, 0, 0, 0 };
    const uint8_t *g;
    int i, r;

    expect("no card: the layer", draw(NULL), 0);

    expect("a card: the layer", draw(&card), 1);
    expect_pixel("left of the card", 283, 352, 0);
    expect_pixel("the card", 284, 352, 0xFF0000AAu);
    expect_pixel("above the card", 284, 351, 0);
    expect_pixel("the card's last row", 355, 352 + 7 * 16 - 1, 0xFF0000AAu);
    expect_pixel("the shadow below", 355, 352 + 7 * 16, 0x90000000u);
    expect_pixel("the shadow at the right", 356, 352 + 16, 0x90000000u);
    expect_pixel("right of the card's first row", 356, 352, 0);
    expect_char("the frame's corner", 284, 352, TM_DD_TOP_LEFT, 0xFFAAAAAAu);
    expect_char("the frame's top", 292, 352, TM_HH, 0xFFAAAAAAu);
    expect_char("the title's A", 284 + 3 * 8, 352, 'A', 0xFFFFFF55u);
    expect_char("the title's b", 284 + 4 * 8, 352, 'b', 0xFFFFFF55u);
    expect_char("the gap before the title", 284 + 2 * 8, 352, ' ', 0xFFAAAAAAu);
    expect_char("the frame after the title", 284 + 6 * 8, 352, TM_HH, 0xFFAAAAAAu);
    expect_char("the frame's side", 284, 352 + 16, TM_VV, 0xFFAAAAAAu);
    expect_char("the text's o", 284 + 2 * 8, 352 + 2 * 16, 'o', 0xFFFFFFFFu);
    expect_char("the second line's umlaut", 284 + 2 * 8, 352 + 3 * 16, TM_A_UMLAUT, 0xFFFFFFFFu);
    expect_char("the sharp s", 284 + 3 * 8, 352 + 3 * 16, TM_SHARP_S, 0xFFFFFFFFu);
    expect_char("the keys' E", 284 + 2 * 8, 352 + 5 * 16, 'E', 0xFF55FFFFu);
    expect_char("the frame's foot", 284 + 8, 352 + 6 * 16, TM_HH, 0xFFAAAAAAu);

    /* the umlaut is the letter with two dots over it */
    g = tm_glyph(TM_A_UMLAUT);
    expect("the a's dots", g[2], 0x66);
    expect("the a below them", memcmp(g + 4, tm_glyph('a') + 4, 12), 0);
    expect("the capital's dots", tm_glyph(TM_CAP_O_UMLAUT)[0], 0x66);

    /* the place's frame: black 4 pixels out, yellow from 3 to 2, black */
    expect("a place: the layer", draw(&place), 1);
    expect_pixel("outside the frame", 59, 100, 0);
    expect_pixel("the frame's outer edge", 60, 100, 0xFF000000u);
    expect_pixel("the frame", 61, 100, 0xFFFFFF55u);
    expect_pixel("the frame", 62, 100, 0xFFFFFF55u);
    expect_pixel("the frame's inner edge", 63, 100, 0xFF000000u);
    expect_pixel("the place itself", 64, 100, 0);
    expect_pixel("the frame at the right", 192 + 1, 100, 0xFFFFFF55u);
    expect_pixel("the frame below", 100, 144 + 2, 0xFFFFFF55u);
    expect_pixel("not dimmed over the running game", 400, 100, 0);
    expect_pixel("the card at the bottom", 320, 480 - 2 * 16 - 1, 0xFF0000AAu);
    draw(&low);
    expect_pixel("a place below: the card at the top", 320, 16, 0xFF0000AAu);
    expect_pixel("and none at the bottom", 320, 480 - 2 * 16 - 1, 0);

    /* 64 characters wrap at 56: 54 and 9, the card 58 wide, 6 rows */
    draw(&wide);
    expect_pixel("a wide card's left edge", (640 - 58 * 8) / 2, 480 - 2 * 16, 0xFF0000AAu);
    expect_pixel("left of it", (640 - 58 * 8) / 2 - 1, 480 - 2 * 16, 0);
    expect_pixel("two lines: its top", 320, 480 - 7 * 16, 0xFF0000AAu);
    expect_pixel("above it", 320, 480 - 7 * 16 - 1, 0);

    /* held: A goes down at picture 1 (the program's).  The card from
     * picture 3 on: A's release at 4 is handed on, Enter at 5 answers
     * and is kept with its release at 7; no tick meanwhile. */
    plat_init("hinttest");
    plat_set_layer(hint_layer, NULL);
    frame_set_tick(tick);
    frame_set_keyboard(key);
    for (i = 0; i < 3; i++)
        frame_wait();
    expect("the ticks before", ticks, 3);
    r = hint_run(&place);
    expect("Enter's answer", r, HINT_NEXT);
    expect("no tick while held", ticks, 3);
    expect("the keys the program got", nkeys, 2);
    expect("A pressed", keys[0], 0x1E);
    expect("A let go while held", keys[1], 0x9E);
    for (i = 0; i < 4; i++)
        frame_wait();
    expect("the keys afterwards", nkeys, 2);
    expect("Esc's answer", hint_run(&card), HINT_END);
    for (i = 0; i < 3; i++)
        frame_wait();
    expect("the keys at the end", nkeys, 2);
    /* the last picture with the card over the running game, for
     * DK_LAYERDUMP */
    hint_show(&card);
    frame_wait();
    plat_shutdown();

    if (!fails)
        printf("hint ok\n");
    return fails != 0;
}
