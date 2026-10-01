/* opl.h - an FM synthesizer that plays what a program writes to an OPL2
 * (the AdLib's chip): nine channels of two operators, the four waveforms,
 * tremolo and vibrato, the rhythm mode's five drums.  Written for the kit
 * from the chip's documented behaviour; it sounds like the chip, it is not
 * its exact output (the operators are computed in floating point, not
 * with the chip's tables; the drums are an approximation; see opl.c).
 *
 * The runner feeds it with the program's register writes at their moment
 * (-oplwav); a port calls it with its own writes and plays the samples.
 * No timers here: they are the port's (or the runner's) business. */
#ifndef DK_OPL_H
#define DK_OPL_H

#include <stdint.h>

typedef struct {
    double phase, inc;              /* cycles, cycles per sample */
    double env;                     /* the envelope's attenuation, dB */
    int stage;                      /* OPL_OFF .. OPL_RELEASE */
    int keyed;                      /* its key is down */
    double out, prev;               /* the last two outputs (feedback) */
} OPLOp;

typedef struct {
    long rate;
    uint8_t reg[256];
    OPLOp op[18];
    int key[9];                     /* a channel's key (B0h bit 5) */
    int drum_key;                   /* the rhythm mode's keys (BDh bits 0-4) */
    double lfo_am, lfo_vib;         /* the tremolo's and vibrato's phases, cycles */
    uint32_t noise;
} OPL;

enum { OPL_OFF, OPL_ATTACK, OPL_DECAY, OPL_SUSTAIN, OPL_RELEASE };

/* silence, all registers 0, `rate` samples a second */
void opl_init(OPL *o, long rate);

/* register `reg` (0-FFh) = v, as a program writes 389h after 388h */
void opl_write(OPL *o, int reg, uint8_t v);

/* the next n samples, mono, 16 bits */
void opl_render(OPL *o, int16_t *out, int n);

#endif
