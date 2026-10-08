/* oplqtest.c - runtime/oplq.c (an OPL2's register writes with their
 * samples) on plat_null.c's virtual audio device, run by selftest.py with
 * DK_WAV=file.
 *
 * A short tune as a port would play it: the program's thread sleeps a
 * picture's time at a pass and then makes the timer's interrupts that
 * are due by the count of samples played (a period of 2000 of the PC
 * timer's 1193182 Hz, 73.92 samples at 44100 Hz, the fraction carried),
 * each stamping what it writes with its own sample plus a lead of two
 * pictures; the device's callback renders through the queue.
 *
 * Checked: the device is there and asks for the frames of the clock's
 * time; no write late, early or left; the samples are those of a second
 * chip that got every write exactly at its sample (rendered in other
 * pieces); silence up to the first note's sample and up to the sample of
 * a note behind a rest, sound from there; and the queue by itself: a
 * stamp that goes back, a late one, more writes than it holds.
 * Says "oplq ok" with the numbers selftest.py looks for in the WAVE file.
 *
 *     oplqtest
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"
#include "oplq.h"

#define RATE 44100
#define PIT_HZ 1193182
#define PERIOD 2000                     /* the timer's, in PIT counts */
#define PICTURE_MS 14
#define LEAD (2 * PICTURE_MS * RATE / 1000)
#define TICKS 900                       /* the tune's length: 1.5 s */
#define MAXW 512
#define MAXS (RATE * 3)

static OPL chip, ref_chip;
static OPLQ queue;
static int16_t got[MAXS], ref[MAXS];
static long ngot;
static int stereo_differs;

static struct { uint64_t at; int reg, v; } made[MAXW];
static int nmade;

static int fail(const char *what, long a, long b)
{
    printf("oplq FAILED: %s: %ld, %ld\n", what, a, b);
    return 1;
}

/* the device's callback: the queue's mono samples on both sides */
static void fill(int16_t *out, int frames, void *user)
{
    static int16_t mono[1024];
    int i;

    (void)user;
    if (frames > 1024 || ngot + frames > MAXS) {
        stereo_differs = 1;             /* fails the run */
        return;
    }
    oplq_render(&queue, mono, frames);
    for (i = 0; i < frames; i++) {
        out[2 * i] = out[2 * i + 1] = mono[i];
        got[ngot++] = mono[i];
    }
}

/* the sample tick k is due at */
static uint64_t tick_sample(long k)
{
    return (uint64_t)k * PERIOD * RATE / PIT_HZ;
}

static uint64_t stamp;                  /* the tick's, for what it writes */

static void w(int reg, int v)
{
    if (nmade < MAXW) {
        made[nmade].at = stamp;
        made[nmade].reg = reg;
        made[nmade].v = v;
    }
    nmade++;
    oplq_write(&queue, stamp, reg, (uint8_t)v);
}

/* an operator at register offset r: a held sine, the fastest attack and
 * release */
static void voice(int r, int level)
{
    w(0x20 + r, 0x21);
    w(0x40 + r, level);
    w(0x60 + r, 0xF0);
    w(0x80 + r, 0x0F);
}

/* the tune: a note of channel c from tick `on` to tick `off` */
static const struct { int on, off, c, fnum, block; } notes[] = {
    {  20,  80, 0, 0x157, 4 }, {  50, 110, 1, 0x1CA, 4 },
    { 120, 180, 0, 0x202, 4 }, { 121, 181, 1, 0x241, 4 },
    { 200, 320, 0, 0x287, 4 }, { 260, 321, 1, 0x157, 5 },
    { 700, 780, 0, 0x1CA, 5 },          /* behind a rest */
    { 781, 860, 1, 0x202, 3 },
};
#define NNOTES ((int)(sizeof notes / sizeof notes[0]))
#define FIRST 0
#define RESTED 6

static void tick(long k)
{
    int i;

    stamp = tick_sample(k) + LEAD;
    if (k == 0) {
        w(0x01, 0x20);
        voice(0x00, 0x3F);              /* channel 0: the carrier alone */
        voice(0x03, 0x08);
        w(0xC0, 0x01);
        voice(0x01, 0x3F);              /* channel 1 */
        voice(0x04, 0x10);
        w(0xC1, 0x01);
    }
    for (i = 0; i < NNOTES; i++) {
        if (k == notes[i].off)
            w(0xB0 + notes[i].c, (notes[i].block << 2) | (notes[i].fnum >> 8));
        if (k == notes[i].on) {
            w(0xA0 + notes[i].c, notes[i].fnum & 0xFF);
            w(0xB0 + notes[i].c, 0x20 | (notes[i].block << 2) | (notes[i].fnum >> 8));
        }
    }
}

/* silence in got[] up to sample `at` (from `from`), sound within 32
 * samples from there */
static int onset(const char *what, long from, long at)
{
    long i;

    for (i = from; i < at; i++)
        if (got[i])
            return fail(what, i, at);
    for (i = at; i < at + 32 && !got[i]; i++)
        ;
    if (i == at + 32)
        return fail(what, -1, at);
    return 0;
}

/* the queue without a device */
static int queue_alone(void)
{
    static OPL c;
    static OPLQ q;
    static int16_t buf[64];
    int i;

    opl_init(&c, RATE);
    oplq_init(&q, &c);
    oplq_write(&q, 10, 0x40, 1);
    oplq_write(&q, 4, 0x40, 2);         /* a stamp that goes back: at 10, behind the first */
    oplq_render(&q, buf, 10);
    if (c.reg[0x40] != 0 || oplq_pending(&q) != 2 || oplq_played(&q) != 10)
        return fail("a write before its sample", c.reg[0x40], oplq_pending(&q));
    oplq_render(&q, buf, 1);
    if (c.reg[0x40] != 2 || oplq_pending(&q) || q.late)
        return fail("two writes at one sample", c.reg[0x40], q.late);
    oplq_write(&q, 3, 0x41, 7);         /* late: before the next sample */
    if (q.late != 1 || c.reg[0x41] != 0)
        return fail("a late write", q.late, c.reg[0x41]);
    oplq_render(&q, buf, 1);
    if (c.reg[0x41] != 7 || oplq_played(&q) != 12)
        return fail("a late write made", c.reg[0x41], (long)oplq_played(&q));
    for (i = 0; i < OPLQ_SIZE + 3; i++)
        oplq_write(&q, 1000 + (uint64_t)i, 0x40 + (i & 1), (uint8_t)i);
    if (q.early != 3 || oplq_pending(&q) != OPLQ_SIZE || c.reg[0x40] != 2 || c.reg[0x41] != 1)
        return fail("a full queue", q.early, c.reg[0x40]);
    while (oplq_played(&q) < 1000 + OPLQ_SIZE + 3)
        oplq_render(&q, buf, 64);
    if (oplq_pending(&q) || c.reg[0x40] != ((OPLQ_SIZE + 2) & 0xFF)
        || c.reg[0x41] != ((OPLQ_SIZE + 1) & 0xFF))
        return fail("a full queue emptied", oplq_pending(&q), c.reg[0x40]);
    return 0;
}

int main(void)
{
    long k = 0, i, at = 0, first, rested;
    int n;

    if (queue_alone())
        return 1;
    if (!plat_init("oplqtest"))
        return 1;
    opl_init(&chip, RATE);
    oplq_init(&queue, &chip);
    if (!plat_audio_start(RATE, fill, NULL))
        return fail("no virtual audio device (DK_WAV not set?)", 0, 0);

    /* the program: a pass a picture, the interrupts due by the samples played */
    while (k < TICKS || oplq_pending(&queue)) {
        plat_sleep_ms(PICTURE_MS);
        for (; k < TICKS && tick_sample(k) <= oplq_played(&queue); k++)
            tick(k);
    }
    for (i = 0; i < 10; i++)
        plat_sleep_ms(PICTURE_MS);
    if (stereo_differs || nmade > MAXW)
        return fail("the callback's frames", ngot, nmade);
    if ((uint64_t)ngot != plat_micros() * RATE / 1000000 || oplq_played(&queue) != (uint64_t)ngot)
        return fail("the frames of the clock's time", ngot, (long)oplq_played(&queue));
    if (queue.late || queue.early || oplq_pending(&queue))
        return fail("late or early writes", queue.late, queue.early);

    /* a second chip with each write exactly at its sample */
    opl_init(&ref_chip, RATE);
    for (n = 0; n < nmade; n++) {
        if ((long)made[n].at > ngot || (n && made[n].at < made[n - 1].at))
            return fail("a stamp", n, (long)made[n].at);
        opl_render(&ref_chip, ref + at, (int)((long)made[n].at - at));
        at = (long)made[n].at;
        opl_write(&ref_chip, made[n].reg, (uint8_t)made[n].v);
    }
    opl_render(&ref_chip, ref + at, (int)(ngot - at));
    for (i = 0; i < ngot; i++)
        if (got[i] != ref[i])
            return fail("a sample is not the chip's with the writes at their samples", i, got[i]);

    first = (long)tick_sample(notes[FIRST].on) + LEAD;
    rested = (long)tick_sample(notes[RESTED].on) + LEAD;
    if (onset("the first note", 0, first)
        || onset("the note behind the rest", rested - RATE / 4, rested))
        return 1;
    plat_shutdown();
    printf("oplq ok: %ld frames, %d writes, the first note at %ld, the rested at %ld\n",
           ngot, nmade, first, rested);
    return 0;
}
