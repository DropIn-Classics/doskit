/* oplq.h - an OPL2's register writes with the sample each belongs to: a
 * queue in front of opl.h for a port whose own thread makes the timer's
 * interrupts while the audio callback renders.  The port stamps a write
 * with a count of samples (oplq_write); the rendering (oplq_render) hands
 * it to the chip when it gets to that sample, so a write is heard at its
 * sample wherever the callback's buffers begin and end.
 *
 * The samples are counted from oplq_init at the chip's rate; oplq_played
 * says how many were rendered so far, which is the clock a port counts
 * its interrupts by (a stamp is that count plus a lead of the port's
 * choosing: a write stamped behind the count is late).
 *
 * No lock here: where oplq_render runs on the audio thread, the port
 * holds plat_audio_lock around oplq_write and oplq_played. */
#ifndef DK_OPLQ_H
#define DK_OPLQ_H

#include <stdint.h>
#include "opl.h"

#define OPLQ_SIZE 4096                  /* writes waiting at most */

typedef struct {
    uint64_t at;                        /* the sample it is written before */
    uint8_t reg, v;
} OPLQWrite;

typedef struct {
    OPL *chip;
    OPLQWrite w[OPLQ_SIZE];             /* a ring: `count` writes from `head` */
    int head, count;
    uint64_t played;                    /* samples rendered */
    uint64_t last;                      /* the newest write's stamp */
    long late;                          /* writes stamped behind `played` */
    long early;                         /* writes the full queue gave the chip at once */
} OPLQ;

/* an empty queue in front of `chip` (opl_init is the caller's), no sample
 * rendered yet */
void oplq_init(OPLQ *q, OPL *chip);

/* register `reg` = v before sample `at` is rendered.  The writes reach the
 * chip in the order they were made: a stamp below the one before it counts
 * as that one.  A stamp below the samples rendered is written before the
 * next sample (counted in `late`).  Into a full queue the oldest write
 * goes to the chip at once (counted in `early`); none is lost. */
void oplq_write(OPLQ *q, uint64_t at, int reg, uint8_t v);

/* the next n samples, mono, 16 bits, each write made at its sample */
void oplq_render(OPLQ *q, int16_t *out, int n);

/* the samples rendered since oplq_init */
uint64_t oplq_played(const OPLQ *q);

/* the writes still waiting */
int oplq_pending(const OPLQ *q);

#endif
