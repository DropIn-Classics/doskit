/* opltest.c - runtime/opl.c by what it plays: a melodic note (a sine of
 * the frequency and level the registers give, with the rhythm mode off
 * and on), and the rhythm mode's drums as the chip's application manual
 * describes them: the tom-tom a sine, the top cymbal a mixture of high
 * frequencies whatever low frequency its channel has (not a tone of that
 * frequency) at the level its operator's envelope gives, not swinging
 * with the operator's wave; the hi-hat noise, the snare a tone with
 * noise; each silent after its key.  Says "opl ok".
 *
 *     opltest
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "opl.h"

#define RATE 49716
#define SEC RATE

static OPL chip;
static int16_t buf[SEC];

static void w(int reg, int v)
{
    opl_write(&chip, reg, (uint8_t)v);
}

/* an operator at register offset `r`: multiple 1 and held, full level,
 * the fastest attack and release, no decay */
static void loud(int r)
{
    w(0x20 + r, 0x21);
    w(0x40 + r, 0x00);
    w(0x60 + r, 0xF0);
    w(0x80 + r, 0x0F);
}

/* channel c's F-number and block for `hz`, its own key as given */
static void pitch(int c, double hz, int block, int key)
{
    int fnum = (int)floor(hz * 1048576.0 / RATE / (1 << block) + 0.5);

    w(0xA0 + c, fnum & 0xFF);
    w(0xB0 + c, (key ? 0x20 : 0) | (block << 2) | (fnum >> 8));
}

static int peak(int from, int to)
{
    int i, p = 0;

    for (i = from; i < to; i++)
        if (abs(buf[i]) > p)
            p = abs(buf[i]);
    return p;
}

/* the changes of sign a second between two samples */
static double crossings(int from, int to)
{
    int i, n = 0;

    for (i = from + 1; i < to; i++)
        if ((buf[i - 1] < 0) != (buf[i] < 0))
            n++;
    return n / ((to - from) / (double)RATE);
}

/* the lowest peak of the stretches of `len` samples between two samples */
static int lowest_peak(int from, int to, int len)
{
    int i, low = 32767;

    for (i = from; i + len <= to; i += len)
        if (peak(i, i + len) < low)
            low = peak(i, i + len);
    return low;
}

static int fail(const char *what, double got)
{
    printf("opl FAILED: %s (%g)\n", what, got);
    return 1;
}

/* a new chip with the waveforms allowed; the rhythm mode as given */
static void start(int rhythm)
{
    opl_init(&chip, RATE);
    w(0x01, 0x20);
    w(0xBD, rhythm ? 0x20 : 0);
}

/* channel 0's second operator alone at 440 Hz (the first never attacks) */
static int melody(int rhythm)
{
    double hz;

    start(rhythm);
    loud(0x03);
    pitch(0, 440.0, 4, 1);
    opl_render(&chip, buf, SEC);
    hz = crossings(SEC / 10, SEC) / 2;
    if (hz < 438 || hz > 442)
        return fail(rhythm ? "a note's frequency in rhythm mode" : "a note's frequency", hz);
    if (peak(SEC / 10, SEC) < 4000 || peak(0, SEC) > 4096)
        return fail(rhythm ? "a note's level in rhythm mode" : "a note's level", peak(0, SEC));
    return 0;
}

/* a drum by its operator's register offset and its bit of BDh, with
 * channel 7 at 50 Hz and channel 8 at 40 Hz; a second of it in buf, then
 * its key let go and the next 100 ms' peak returned */
static int drum(int r, int bit)
{
    static int16_t after[SEC / 10];
    int i, p = 0;

    start(1);
    loud(r);
    pitch(7, 50.0, 1, 0);
    pitch(8, 40.0, 0, 0);
    w(0xBD, 0x20 | bit);
    opl_render(&chip, buf, SEC);
    w(0xBD, 0x20);
    opl_render(&chip, after, SEC / 10);
    for (i = SEC / 100; i < SEC / 10; i++)
        if (abs(after[i]) > p)
            p = abs(after[i]);
    return p;
}

int main(void)
{
    /* a signal of full swing at twice an operator's level, 1/sqrt 2 of it */
    const int full = 5793;
    double x;
    int tail;

    if (melody(0) || melody(1))
        return 1;

    /* the tom-tom: channel 8's first operator, a sine of its frequency at
     * twice a melodic operator's level */
    tail = drum(0x12, 0x04);
    x = crossings(SEC / 10, SEC) / 2;
    if (x < 39 || x > 41)
        return fail("the tom-tom's frequency", x);
    if (peak(SEC / 10, SEC) < 8000 || peak(0, SEC) > 8192)
        return fail("the tom-tom's level", peak(0, SEC));
    if (tail)
        return fail("the tom-tom after its key", tail);

    /* the top cymbal: channel 8's second operator.  A mixture of high
     * frequencies: with the channels at 40 and 50 Hz the sign changes
     * some thousand times a second (a 40 Hz tone's changes 80 times). */
    tail = drum(0x15, 0x02);
    x = crossings(SEC / 10, SEC);
    if (x < 1000)
        return fail("the cymbal's changes of sign a second", x);
    if (peak(0, SEC) < full - 2 || peak(0, SEC) > full + 2)
        return fail("the cymbal's level", peak(0, SEC));
    if (lowest_peak(SEC / 10, SEC, RATE / 500) < full - 2)
        return fail("the cymbal's level over 2 ms", lowest_peak(SEC / 10, SEC, RATE / 500));
    if (tail)
        return fail("the cymbal after its key", tail);

    /* the hi-hat: channel 7's first operator, noise */
    tail = drum(0x11, 0x01);
    x = crossings(SEC / 10, SEC);
    if (x < 10000)
        return fail("the hi-hat's changes of sign a second", x);
    if (peak(SEC / 10, SEC) < 8000 || peak(0, SEC) > 8192)
        return fail("the hi-hat's level", peak(0, SEC));
    if (tail)
        return fail("the hi-hat after its key", tail);

    /* the snare drum: channel 7's second operator, its tone with noise */
    tail = drum(0x14, 0x08);
    x = crossings(SEC / 10, SEC) / 2;
    if (x < 49 || x > 51)
        return fail("the snare's tone", x);
    if (peak(SEC / 10, SEC) < 8000 || peak(0, SEC) > 8192)
        return fail("the snare's level", peak(0, SEC));
    if (tail)
        return fail("the snare after its key", tail);

    printf("opl ok\n");
    return 0;
}
