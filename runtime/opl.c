/* opl.c - an OPL2's FM synthesis (see opl.h).
 *
 * Written for the kit from the chip's documented behaviour (register map,
 * the rates' times, the level steps); the numbers below that are not from
 * there are marked as choices.  What it does per sample and operator:
 *   phase    the channel's F-number and block give the frequency,
 *            fnum * 2^block * 49716 Hz / 2^20, times the operator's
 *            multiple (1/2, 1..10, 12, 15); vibrato bends it by 7 or 14
 *            cents at 6.1 Hz,
 *   level    the envelope (attack, decay to the sustain level, sustain or
 *            not, release) plus the total level (0.75 dB steps), the key
 *            scaling of the level (0, 1.5, 3 or 6 dB an octave), tremolo
 *            (1 or 4.8 dB at 3.7 Hz),
 *   wave     sine, half sine, rectified sine, quarter pulses (with
 *            register 01h bit 5), of the phase plus the modulation.
 * A channel either modulates its second operator by the first or adds the
 * two; the first can modulate itself (feedback 0, pi/16 .. 4 pi).
 * The envelope's times are the chip's table as the data sheet gives it for
 * rate 1 (attack 2826.24 ms, decay 39280.64 ms from 0 to 96 dB), halved for
 * every rate above, the rate's lowest two bits (from key scaling) in
 * between; the attack's curve (exponential in dB) is a choice.  The
 * rhythm mode's drums are an approximation: noise and square waves from
 * the operators' phases, not the chip's circuit. */
#include "opl.h"
#include <math.h>
#include <string.h>

#define CHIP_HZ (3579545.0 / 72.0)
#define SILENT 96.0                     /* dB: the envelope's bottom */
#define PI2 6.283185307179586

/* twice the multiple, by register 20h's bits 0-3 */
static const int mult2[16] = { 1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 20, 24, 24, 30, 30 };
/* dB an octave by register 40h's bits 6-7 */
static const double ksl_db[4] = { 0.0, 3.0, 1.5, 6.0 };

/* the register offset of channel c's operator k (0 modulator, 1 carrier) */
static int op_reg(int c, int k){ return (c / 3) * 8 + c % 3 + 3 * k; }

void opl_init(OPL *o, long rate){
    int i;
    memset(o, 0, sizeof(*o));
    o->rate = rate > 0 ? rate : 49716;
    for(i = 0; i < 18; i++){ o->op[i].env = SILENT; o->op[i].stage = OPL_OFF; }
    o->noise = 1;
}

/* the key scaling number: block and the F-number's top bit (or the one
 * below with register 08h bit 6) */
static int key_scale(const OPL *o, int c){
    int fnum = o->reg[0xA0 + c] | ((o->reg[0xB0 + c] & 3) << 8);
    int block = (o->reg[0xB0 + c] >> 2) & 7;
    int bit = (o->reg[0x08] & 0x40) ? (fnum >> 8) & 1 : (fnum >> 9) & 1;
    return block * 2 + bit;
}

/* the effective rate (0-63) of a 4-bit rate R for channel c's operator */
static int eff_rate(const OPL *o, int c, int r20, int R){
    int ksn = key_scale(o, c), e;
    if(R == 0) return 0;
    e = 4 * R + ((r20 & 0x10) ? ksn : ksn >> 2);
    return e > 63 ? 63 : e;
}

/* the time (s) of a rate: `t1` at rate 1 (4..7), halved each rate above */
static double rate_time(double t1, int e){
    return t1 * 4.0 / (4 + (e & 3)) / (double)(1u << ((e >> 2) - 1));
}

static void op_key(OPL *o, int i, int on){
    OPLOp *p = &o->op[i];
    if(on && !p->keyed){
        p->stage = OPL_ATTACK;
        p->phase = 0.0;                         /* the chip starts it anew */
    } else if(!on && p->keyed && p->stage != OPL_OFF)
        p->stage = OPL_RELEASE;
    p->keyed = on;
}

/* the keys of all operators from B0h-B8h and, in rhythm mode, BDh */
static void keys(OPL *o){
    int c, rhythm = o->reg[0xBD] & 0x20;
    for(c = 0; c < 9; c++){
        int k = (o->reg[0xB0 + c] >> 5) & 1;
        int k0 = k, k1 = k;
        if(rhythm && c == 6) k0 = k1 = k | ((o->reg[0xBD] >> 4) & 1);          /* bass drum */
        if(rhythm && c == 7){ k0 = k | (o->reg[0xBD] & 1);                      /* hi-hat */
                              k1 = k | ((o->reg[0xBD] >> 3) & 1); }             /* snare */
        if(rhythm && c == 8){ k0 = k | ((o->reg[0xBD] >> 2) & 1);               /* tom */
                              k1 = k | ((o->reg[0xBD] >> 1) & 1); }             /* cymbal */
        op_key(o, 2 * c, k0);
        op_key(o, 2 * c + 1, k1);
    }
}

void opl_write(OPL *o, int reg, uint8_t v){
    reg &= 0xFF;
    o->reg[reg] = v;
    if((reg >= 0xB0 && reg <= 0xB8) || reg == 0xBD) keys(o);
}

/* the envelope of channel c's operator k, one sample on */
static void envelope(OPL *o, int c, int k){
    OPLOp *p = &o->op[2 * c + k];
    int r = op_reg(c, k);
    int r20 = o->reg[0x20 + r], r60 = o->reg[0x60 + r], r80 = o->reg[0x80 + r];
    double sl = ((r80 >> 4) == 15) ? 93.0 : (r80 >> 4) * 3.0;
    int e;
    switch(p->stage){
    case OPL_ATTACK:
        e = eff_rate(o, c, r20, r60 >> 4);
        if(e >= 60) p->env = 0.0;
        else if(e > 0){
            /* from 96 dB to 0.1 dB in the rate's time: ln(960) time constants */
            p->env *= exp(-6.8669 / (rate_time(2.82624, e) * o->rate));
            if(p->env < 0.1) p->env = 0.0;
        }
        if(p->env <= 0.0){ p->env = 0.0; p->stage = OPL_DECAY; }
        break;
    case OPL_DECAY:
        e = eff_rate(o, c, r20, r60 & 15);
        if(e > 0) p->env += SILENT / (rate_time(39.28064, e) * o->rate);
        if(p->env >= sl){
            p->env = sl;
            p->stage = OPL_SUSTAIN;
        }
        break;
    case OPL_SUSTAIN:
        /* a sustaining sound (20h bit 5) stays; any other falls on at the
         * release rate while the key is down */
        if(r20 & 0x20) break;
        /* fall through */
    case OPL_RELEASE:
        e = eff_rate(o, c, r20, r80 & 15);
        if(e > 0) p->env += SILENT / (rate_time(39.28064, e) * o->rate);
        if(p->env >= SILENT){
            p->env = SILENT;
            if(p->stage == OPL_RELEASE) p->stage = OPL_OFF;
        }
        break;
    }
}

/* the operator's level (dB below full) without the envelope */
static double level(const OPL *o, int c, int k, double am){
    int r = op_reg(c, k), r40 = o->reg[0x40 + r];
    int fnum = o->reg[0xA0 + c] | ((o->reg[0xB0 + c] & 3) << 8);
    int block = (o->reg[0xB0 + c] >> 2) & 7;
    /* key scaling: none below the lowest notes, up to 7 octaves' worth at
     * the top (a smooth curve; the chip's table has steps) */
    double oct = block + log2(((fnum >> 6) + 1) / 16.0);
    double db = (r40 & 63) * 0.75 + (oct > 0 ? oct * ksl_db[r40 >> 6] : 0.0);
    if(o->reg[0x20 + r] & 0x80) db += am;
    return db;
}

static double wave(int ws, double ph){
    double s;
    ph -= floor(ph);
    s = sin(PI2 * ph);
    switch(ws){
    case 1: return ph < 0.5 ? s : 0.0;
    case 2: return fabs(s);
    case 3: return fmod(ph, 0.5) < 0.25 ? fabs(s) : 0.0;
    }
    return s;
}

/* channel c's operator k's output (-1..1) with the phase moved by pm
 * cycles; its phase goes on by a sample */
static double op_out(OPL *o, int c, int k, double pm, double am, double vib){
    OPLOp *p = &o->op[2 * c + k];
    int r = op_reg(c, k), r20 = o->reg[0x20 + r];
    int fnum = o->reg[0xA0 + c] | ((o->reg[0xB0 + c] & 3) << 8);
    int block = (o->reg[0xB0 + c] >> 2) & 7;
    int ws = (o->reg[0x01] & 0x20) ? o->reg[0xE0 + r] & 3 : 0;
    double f = fnum * (double)(1 << block) * CHIP_HZ / 1048576.0 * mult2[r20 & 15] / 2.0;
    double db, v = 0.0;
    if(r20 & 0x40) f *= vib;
    envelope(o, c, k);
    db = p->env + level(o, c, k, am);
    if(p->stage != OPL_OFF && db < SILENT)
        v = wave(ws, p->phase + pm) * pow(10.0, -db / 20.0);
    p->phase += f / o->rate;
    p->phase -= floor(p->phase);
    p->prev = p->out;
    p->out = v;
    return v;
}

/* a two-operator channel's output */
static double channel(OPL *o, int c, double am, double vib){
    int c0 = o->reg[0xC0 + c], fb = (c0 >> 1) & 7;
    OPLOp *m = &o->op[2 * c];
    /* feedback: the mean of the last two outputs; its peak moves the phase
     * by pi/16 (fb 1) .. 4 pi (fb 7), as the data sheet's table */
    double pm = fb ? (m->out + m->prev) / 2.0 * (double)(1 << (fb - 1)) / 32.0 : 0.0;
    double mv = op_out(o, c, 0, pm, am, vib);
    /* the modulator's peak moves the carrier's phase by 4 cycles (8 pi):
     * a choice, not checked against a chip */
    if(c0 & 1) return mv + op_out(o, c, 1, 0.0, am, vib);
    return op_out(o, c, 1, mv * 4.0, am, vib);
}

/* the rhythm mode's channels 6-8: bass drum as a channel, the other four
 * one operator each; noise and squares for the hi-hat, snare and cymbal */
static double drums(OPL *o, double am, double vib){
    double s = 2.0 * channel(o, 6, am, vib);
    double hh, sd, tom, cy;
    int bit = o->noise & 1;
    o->noise = (o->noise >> 1) ^ (bit ? 0x400181u : 0u);   /* a 23-bit shift register */
    /* each op's output taken for its level, the wave replaced */
    hh = fabs(op_out(o, 7, 0, 0.0, am, vib));
    sd = fabs(op_out(o, 7, 1, 0.0, am, vib));
    tom = op_out(o, 8, 0, 0.0, am, vib);
    cy = fabs(op_out(o, 8, 1, 0.0, am, vib));
    hh *= ((o->op[14].phase < 0.5) ^ bit) ? 1.0 : -1.0;
    sd *= (o->op[15].phase < 0.5) ? (bit ? 1.0 : 0.5) : (bit ? -0.5 : -1.0);
    cy *= (o->op[17].phase < 0.5) ? 1.0 : -1.0;
    return s + 2.0 * (hh + sd + tom + cy);
}

void opl_render(OPL *o, int16_t *out, int n){
    int i, c;
    for(i = 0; i < n; i++){
        double tri, am, vib, sum = 0.0;
        long v;
        /* the LFOs: triangles, tremolo 3.7 Hz (0..depth dB), vibrato 6.1 Hz */
        o->lfo_am += 3.7 / o->rate;   o->lfo_am -= floor(o->lfo_am);
        o->lfo_vib += 6.1 / o->rate;  o->lfo_vib -= floor(o->lfo_vib);
        tri = o->lfo_am < 0.5 ? 2.0 * o->lfo_am : 2.0 - 2.0 * o->lfo_am;
        am = tri * ((o->reg[0xBD] & 0x80) ? 4.8 : 1.0);
        tri = o->lfo_vib < 0.5 ? 4.0 * o->lfo_vib - 1.0 : 3.0 - 4.0 * o->lfo_vib;
        vib = pow(2.0, tri * ((o->reg[0xBD] & 0x40) ? 14.0 : 7.0) / 1200.0);
        for(c = 0; c < ((o->reg[0xBD] & 0x20) ? 6 : 9); c++) sum += channel(o, c, am, vib);
        if(o->reg[0xBD] & 0x20) sum += drums(o, am, vib);
        /* an operator's full scale is 4096, as the chip's outputs */
        v = lround(sum * 4096.0);
        out[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    }
}
