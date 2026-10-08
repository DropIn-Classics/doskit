/* oplq.c - an OPL2's register writes with their samples (see oplq.h). */
#include "oplq.h"
#include <string.h>

void oplq_init(OPLQ *q, OPL *chip)
{
    memset(q, 0, sizeof *q);
    q->chip = chip;
}

/* the oldest write to the chip */
static void pop(OPLQ *q)
{
    const OPLQWrite *w = &q->w[q->head];

    opl_write(q->chip, w->reg, w->v);
    q->head = (q->head + 1) % OPLQ_SIZE;
    q->count--;
}

void oplq_write(OPLQ *q, uint64_t at, int reg, uint8_t v)
{
    OPLQWrite *w;

    if (at < q->played)
        q->late++;
    if (at < q->last)
        at = q->last;                   /* the order of the writes is the chip's state */
    if (q->count == OPLQ_SIZE) {
        pop(q);
        q->early++;
    }
    w = &q->w[(q->head + q->count) % OPLQ_SIZE];
    w->at = at;
    w->reg = (uint8_t)reg;
    w->v = v;
    q->count++;
    q->last = at;
}

void oplq_render(OPLQ *q, int16_t *out, int n)
{
    while (n > 0) {
        int run = n;

        while (q->count && q->w[q->head].at <= q->played)
            pop(q);
        if (q->count && q->w[q->head].at - q->played < (uint64_t)run)
            run = (int)(q->w[q->head].at - q->played);
        opl_render(q->chip, out, run);
        out += run;
        n -= run;
        q->played += (uint64_t)run;
    }
}

uint64_t oplq_played(const OPLQ *q)
{
    return q->played;
}

int oplq_pending(const OPLQ *q)
{
    return q->count;
}
