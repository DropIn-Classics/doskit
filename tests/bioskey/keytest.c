/* keytest.c - bioskey.c: a letter, the same with Shift, a key without a
 * character, an extended key's E0 left out, break codes kept out, the
 * order kept, fifteen keys of room, the shift states.  Prints
 * "bioskey ok" or what failed. */
#include <stdio.h>
#include "bioskey.h"

static int fails;

static void expect(const char *what, int got, int wanted)
{
    if (got != wanted) {
        printf("%s: %04X, not %04X\n", what, (unsigned)got, (unsigned)wanted);
        fails++;
    }
}

int main(void)
{
    int i;

    expect("no key, peek", bioskey_peek(), -1);
    expect("no key, read", bioskey_read(), -1);

    bioskey_put(0x20);                  /* D */
    bioskey_put(0xA0);
    expect("d, peek", bioskey_peek(), 0x2064);
    expect("d, peek again", bioskey_peek(), 0x2064);
    expect("d, read", bioskey_read(), 0x2064);
    expect("after d", bioskey_read(), -1);

    bioskey_put(0x2A);                  /* left Shift held */
    expect("Shift alone", bioskey_peek(), -1);
    expect("Shift's state", bioskey_shifts(), 2);
    bioskey_put(0x20);
    bioskey_put(0x02);                  /* 1 */
    bioskey_put(0xAA);
    bioskey_put(0x20);
    expect("D", bioskey_read(), 0x2044);
    expect("!", bioskey_read(), 0x0221);
    expect("d after Shift", bioskey_read(), 0x2064);
    expect("the states after Shift", bioskey_shifts(), 0);

    bioskey_put(0x3B);                  /* F1 */
    bioskey_put(0xE0);                  /* the grey arrow down */
    bioskey_put(0x50);
    bioskey_put(0xE0);
    bioskey_put(0xD0);
    bioskey_put(0x1C);                  /* Enter */
    expect("F1", bioskey_read(), 0x3B00);
    expect("arrow down", bioskey_read(), 0x5000);
    expect("Enter", bioskey_read(), 0x1C0D);
    expect("after Enter", bioskey_read(), -1);

    bioskey_put(0x1D);                  /* Ctrl and the right Shift */
    bioskey_put(0x36);
    expect("Ctrl and right Shift", bioskey_shifts(), 5);
    bioskey_put(0x9D);
    bioskey_put(0xB6);
    bioskey_put(0x38);                  /* Alt */
    expect("Alt", bioskey_shifts(), 8);
    bioskey_put(0xB8);

    for (i = 0; i < 20; i++)            /* 1, 2, 3...: fifteen are kept */
        bioskey_put((unsigned char)(0x02 + i % 9));
    for (i = 0; i < 15; i++)
        expect("a kept key", bioskey_read(), (0x02 + i % 9) << 8 | ('1' + i % 9));
    expect("the sixteenth", bioskey_read(), -1);

    bioskey_put(0x2A);
    bioskey_put(0x1E);
    bioskey_clear();
    expect("cleared", bioskey_read(), -1);
    expect("cleared states", bioskey_shifts(), 0);
    bioskey_put(0x1E);
    expect("a after clearing", bioskey_read(), 0x1E61);

    if (!fails)
        printf("bioskey ok\n");
    return fails != 0;
}
