/* Sound: the two 8237 DMA controllers, a Sound Blaster 16 (DSP and the
 * mixer's configuration registers; see dosrun.h) and an OPL2.  The samples the card
 * would play go to a WAV file (-wav) or nowhere.
 */
#include "dosrun.h"
#include "../../runtime/opl.h"

int sound_debug = 0;

/* 1: interrupt on the DMA controller's wrap instead of the DSP's own
 * transfer count (off). */
int sb_dmairq = 0;

/* ------------------------------------------------------------ WAV sink */
static FILE *wav_fp;
static unsigned long wav_n;
static int wav_hz = 0;

static void wav_put32(uint8_t *p, uint32_t v){
    p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
}
static void wav_header(void){
    uint8_t h[44];
    memset(h, 0, sizeof(h));
    memcpy(h, "RIFF", 4); memcpy(h+8, "WAVEfmt ", 8);
    h[16] = 16; h[20] = 1; h[22] = 1;                  /* PCM, mono */
    wav_put32(h+24, (uint32_t)wav_hz);
    wav_put32(h+28, (uint32_t)wav_hz*2);
    h[32] = 2; h[34] = 16;
    memcpy(h+36, "data", 4);
    wav_put32(h+4, 36 + (uint32_t)wav_n*2);
    wav_put32(h+40, (uint32_t)wav_n*2);
    fseek(wav_fp, 0, SEEK_SET);
    fwrite(h, 1, 44, wav_fp);
    fseek(wav_fp, 0, SEEK_END);
}
void sound_wav_open(const char *path){
    wav_fp = fopen(path, "wb");
    if(!wav_fp){ fprintf(stderr, "cannot write %s\n", path); return; }
    wav_n = 0;
    wav_header();
}
/* The rate is the first one the driver programs; a driver that changes it
 * later is not expected (one DSP time constant per program run). */
static void wav_rate(int hz){
    if(wav_fp && !wav_hz){ wav_hz = hz; wav_header(); }
}
static void wav_push(const int16_t *s, int n){
    if(!wav_fp) return;
    fwrite(s, 2, (size_t)n, wav_fp);
    wav_n += (unsigned long)n;
}
static void sb_flush(void);
void sound_wav_close(void){
    if(!wav_fp) return;
    sb_flush();                     /* the last samples, less than a chunk */
    wav_header();
    fclose(wav_fp); wav_fp = NULL;
}

/* ------------------------------------------------------------------ 8237 */
/* Two controllers as in an AT: channels 0-3 move bytes (ports 00h-0Fh),
 * channels 4-7 move words (ports C0h-DFh, one register every other port;
 * address and count in words, the page's bit 0 unused). */
typedef struct {
    uint16_t addr, count, base_addr, base_count;
    uint8_t  page, mode, masked;
} DMACH;
static DMACH dma[8];
static int dma_ff[2];                  /* low/high byte flip-flop, per controller */

static void dma_reset(int k){
    int i;
    memset(&dma[4*k], 0, 4*sizeof(dma[0]));
    for(i=0;i<4;i++) dma[4*k+i].masked = 1;
    dma_ff[k] = 0;
}

/* The page register's port for each channel (none for 4: the cascade). */
static const uint16_t dma_page_port[8] = { 0x87, 0x83, 0x81, 0x82, 0x8F, 0x8B, 0x89, 0x8A };

int dma_is_port(uint16_t p){
    int c;
    if(p < 0x10 || (p >= 0xC0 && p <= 0xDF)) return 1;
    for(c=0;c<8;c++) if(p == dma_page_port[c]) return 1;
    return 0;
}

/* The controller (0, 1) and its register number (0-15) of a port, or -1. */
static int dma_reg(uint16_t p, int *k){
    if(p < 0x10){ *k = 0; return p; }
    if(p >= 0xC0 && p <= 0xDF && !(p & 1)){ *k = 1; return (p - 0xC0) >> 1; }
    return -1;
}

void dma_write(uint16_t p, uint8_t v){
    int k, r = dma_reg(p, &k), c;
    if(r < 0){
        for(c=0;c<8;c++) if(p == dma_page_port[c]){ dma[c].page = v; return; }
        return;
    }
    if(r < 8){
        DMACH *d = &dma[4*k + (r >> 1)];
        uint16_t *base = (r & 1) ? &d->base_count : &d->base_addr;
        if(!dma_ff[k]) *base = (uint16_t)((*base & 0xFF00) | v);
        else           *base = (uint16_t)((*base & 0x00FF) | (v << 8));
        if(r & 1) d->count = d->base_count; else d->addr = d->base_addr;
        dma_ff[k] ^= 1;
        return;
    }
    switch(r){
    case 0x0A: dma[4*k + (v & 3)].masked = (v & 4) ? 1 : 0; return;   /* single mask  */
    case 0x0B: dma[4*k + (v & 3)].mode = v; return;                   /* mode         */
    case 0x0C: dma_ff[k] = 0; return;                                 /* clear ff     */
    case 0x0D: dma_reset(k); return;                                  /* master clear */
    case 0x0E: for(c=0;c<4;c++) dma[4*k+c].masked = 0; return;        /* clear mask   */
    case 0x0F: for(c=0;c<4;c++) dma[4*k+c].masked = (v >> c) & 1; return;
    }
}

uint8_t dma_read(uint16_t p){
    int k, r = dma_reg(p, &k), c;
    if(r < 0){
        for(c=0;c<8;c++) if(p == dma_page_port[c]) return dma[c].page;
        return 0xFF;
    }
    if(r < 8){
        DMACH *d = &dma[4*k + (r >> 1)];
        uint16_t w = (r & 1) ? d->count : d->addr;
        uint8_t b = dma_ff[k] ? (uint8_t)(w >> 8) : (uint8_t)w;
        dma_ff[k] ^= 1;
        return b;
    }
    return 0xFF;
}

/* Pull one byte (channels 0-3) or word (4-7) across the channel.  Returns
 * -1 when the channel is masked (a block that ended and is not auto-init
 * masks it): the card gets nothing and waits, as on hardware, until it is
 * unmasked. */
static int dma_fetch(int c, int *end_of_block){
    uint32_t phys;
    int b;
    *end_of_block = 0;
    if(dma[c].masked) return -1;
    if(c < 4){
        phys = ((uint32_t)dma[c].page << 16) | dma[c].addr;
        b = mem_r8(phys);
    } else {
        phys = ((uint32_t)(dma[c].page & 0xFE) << 16) | ((uint32_t)dma[c].addr << 1);
        b = mem_r8(phys) | (mem_r8(phys + 1) << 8);
    }
    dma[c].addr++;
    if(dma[c].count == 0){
        *end_of_block = 1;
        if(dma[c].mode & 0x10){                 /* auto-init: reload and go on */
            dma[c].addr  = dma[c].base_addr;
            dma[c].count = dma[c].base_count;
        } else {
            dma[c].masked = 1;
        }
    } else {
        dma[c].count--;
    }
    return b;
}

/* ------------------------------------------------------- Sound Blaster DSP */
/* A Sound Blaster 16 at 220h, IRQ 7, 8-bit DMA 1, 16-bit DMA 5, as its
 * mixer's registers 80h and 81h report them (an SB16 driver reads its
 * configuration there).  The DSP knows the 8-bit commands of the older
 * cards and the SB16's 41h/42h (rate) and Bxh/Cxh (8- and 16-bit
 * transfers, mono or stereo, signed or not).  It still says it is a DSP
 * 1.05 (E1h): a driver that picks its way by the version keeps the
 * single-cycle 8-bit path it had before the SB16's commands were there. */
#define SB_BASE 0x220
int sb_irq = 7;
#define SB_DMA8  1
#define SB_DMA16 5

static struct {
    int  reset_stage;
    uint8_t outbuf[8]; int outlen, outpos;
    uint8_t cmd; int need_args; uint8_t arg[3]; int nargs;
    int  block_size;          /* units (bytes, or words for 16-bit) in one DSP transfer */
    int  dsp_left;            /* units still owed on it */
    int  playing, auto_init;
    int  bits16, stereo, is_signed;   /* the running transfer's format */
    int  speaker;
    int  irq8, irq16;         /* the interrupts the card asserts (mixer 82h) */
    double rate;              /* sample frames per second */
    double frac;              /* fractional frame carried between ticks */
    double last_t;
    uint8_t mix_idx, mix[256];
} sb;

static void sb_out(uint8_t v){
    if(sb.outlen < (int)sizeof(sb.outbuf)) sb.outbuf[sb.outlen++] = v;
}

static void sb_irq_update(void){
    if(sb.irq8 || sb.irq16) pic_raise(sb_irq); else pic_lower(sb_irq);
}

void sb_reset_dev(void){
    memset(&sb, 0, sizeof(sb));
    sb.rate = 22050.0;
    sb.last_t = -1.0;
}

static void sb_set_rate(double hz){
    if(hz < 4000.0) hz = 4000.0;
    if(hz > 48000.0) hz = 48000.0;
    sb.rate = hz;
}

/* The DMA channel the running transfer reads. */
static int sb_channel(void){ return sb.bits16 ? SB_DMA16 : SB_DMA8; }

static void sb_start(int auto_init, int len){
    int c;
    sb.playing = 1;
    sb.auto_init = auto_init;
    if(len > 0) sb.block_size = len;
    sb.dsp_left = sb.block_size;
    sb.last_t = emu_now();
    sb.frac = 0.0;
    wav_rate((int)(sb.rate + 0.5));
    c = sb_channel();
    if(sound_debug){
        /* Both lengths and the gap between re-arms, because the interesting
         * question is how the DSP's transfer length relates to the DMA
         * block the controller is actually looping - see the header. */
        static double prev_start = -1.0;
        fprintf(stderr,
                "[sb] start %s %d-bit %s%s, %d units, %.0f Hz | t=%.3f dt=%.3f | "
                "dma%d mode=%02X (%s) page=%02X addr=%04X count=%u\n",
                auto_init ? "auto-init" : "single", sb.bits16 ? 16 : 8,
                sb.stereo ? "stereo" : "mono", sb.is_signed ? " signed" : "",
                len, sb.rate, sb.last_t, prev_start < 0.0 ? 0.0 : sb.last_t - prev_start,
                c, dma[c].mode, (dma[c].mode & 0x10) ? "auto-init" : "single",
                dma[c].page, dma[c].base_addr, (unsigned)dma[c].base_count + 1u);
        prev_start = sb.last_t;
    }
}

/* The commands of the older cards: 8-bit unsigned mono. */
static void sb_start8(int auto_init, int len){
    sb.bits16 = 0; sb.stereo = 0; sb.is_signed = 0;
    sb_start(auto_init, len);
}

static void sb_command(uint8_t c){
    if(c >= 0xB0 && c <= 0xCF){                  /* SB16 transfer: mode, length */
        sb.cmd = c; sb.need_args = 3; sb.nargs = 0; return;
    }
    switch(c){
    case 0x10: sb.cmd = c; sb.need_args = 1; sb.nargs = 0; return;   /* direct DAC */
    case 0x14: case 0x24:
    case 0x1C: case 0x2C:
        sb.cmd = c; sb.need_args = (c == 0x14 || c == 0x24) ? 2 : 0;
        sb.nargs = 0;
        if(!sb.need_args) sb_start8(1, sb.block_size);
        return;
    case 0x40: sb.cmd = c; sb.need_args = 1; sb.nargs = 0; return;   /* time const */
    case 0x41: case 0x42:                                            /* rate (SB16) */
    case 0x48: sb.cmd = c; sb.need_args = 2; sb.nargs = 0; return;   /* block size */
    case 0xD0: case 0xD5: sb.playing = 0; return;                    /* halt DMA   */
    case 0xD4: case 0xD6:                                            /* continue   */
        if(sb.block_size) sb.playing = 1;
        sb.last_t = emu_now(); return;
    case 0xD9: case 0xDA: sb.auto_init = 0; return;  /* end auto-init after this block */
    case 0xD1: sb.speaker = 1; return;
    case 0xD3: sb.speaker = 0; return;
    case 0xD8: sb_out((uint8_t)(sb.speaker ? 0xFF : 0x00)); return;
    case 0xE1: sb_out(0x01); sb_out(0x05); return;   /* DSP 1.05, see above */
    case 0xE0: sb.cmd = c; sb.need_args = 1; sb.nargs = 0; return;   /* identify   */
    case 0xF2: sb.irq8 = 1; sb_irq_update(); return;                 /* force IRQ  */
    default:
        if(sound_debug) fprintf(stderr, "[sb] unhandled DSP command %02X\n", c);
        return;
    }
}

static void sb_command_arg(uint8_t v){
    sb.arg[sb.nargs++] = v;
    if(sb.nargs < sb.need_args) return;
    switch(sb.cmd){
    case 0x10: break;                                   /* direct DAC: ignored */
    case 0x40: sb_set_rate(1000000.0 / (256.0 - (double)v)); break;
    case 0x41: case 0x42: sb_set_rate((double)((sb.arg[0] << 8) | sb.arg[1])); break;
    case 0x48: sb.block_size = (sb.arg[0] | (sb.arg[1] << 8)) + 1; break;
    case 0x14: case 0x24:
        sb_start8(0, (sb.arg[0] | (sb.arg[1] << 8)) + 1); break;
    case 0xE0: sb_out((uint8_t)~v); break;
    default:
        if(sb.cmd >= 0xB0 && sb.cmd <= 0xCF){
            /* Bxh 16-bit, Cxh 8-bit; bit 3 records (not here), bit 2
             * auto-init; the mode byte's bit 5 stereo, bit 4 signed; the
             * length in units (bytes or words, both channels counted). */
            if(sb.cmd & 0x08){
                if(sound_debug) fprintf(stderr, "[sb] recording (%02X) not emulated\n", sb.cmd);
                break;
            }
            sb.bits16 = sb.cmd < 0xC0;
            sb.stereo = (sb.arg[0] >> 5) & 1;
            sb.is_signed = (sb.arg[0] >> 4) & 1;
            sb_start((sb.cmd >> 2) & 1, (sb.arg[1] | (sb.arg[2] << 8)) + 1);
        }
        break;
    }
    sb.need_args = 0; sb.nargs = 0;
}

/* The mixer's registers the emulation answers itself: the configuration
 * (80h interrupt, 81h DMA) and the interrupt status (82h).  The others
 * (volumes) keep what was written. */
static uint8_t sb_mixer_read(uint8_t r){
    switch(r){
    case 0x80:
        switch(sb_irq){ case 2: case 9: return 0x01; case 5: return 0x02;
                        case 7: return 0x04; case 10: return 0x08; }
        return 0x00;
    case 0x81: return (uint8_t)((1 << SB_DMA8) | (1 << SB_DMA16));
    case 0x82: return (uint8_t)((sb.irq8 ? 0x01 : 0) | (sb.irq16 ? 0x02 : 0));
    }
    return sb.mix[r];
}

void sb_write(uint16_t p, uint8_t v){
    switch(p - SB_BASE){
    case 0x04: sb.mix_idx = v; return;           /* mixer index */
    case 0x05:                                   /* mixer data (80h-82h stay) */
        if(sb.mix_idx < 0x80 || sb.mix_idx > 0x82) sb.mix[sb.mix_idx] = v;
        return;
    case 0x06:                                   /* DSP reset */
        if(v & 1) sb.reset_stage = 1;
        else if(sb.reset_stage){
            sb.reset_stage = 0;
            sb.playing = 0; sb.outlen = sb.outpos = 0;
            sb.need_args = 0; sb.nargs = 0;
            /* A reset also drops any interrupt the card was still asserting.
             * Without this, a second driver instance (each game program loads
             * its own) unmasks the line and immediately takes an interrupt
             * left over from the previous one, part-way through its own
             * initialisation. */
            sb.irq8 = sb.irq16 = 0;
            sb_irq_update();
            sb_out(0xAA);                        /* the "I am here" reply */
            if(sound_debug) fprintf(stderr, "[sb] DSP reset\n");
        }
        return;
    case 0x0C:                                   /* command / data */
        if(sb.need_args) sb_command_arg(v);
        else sb_command(v);
        return;
    }
}

uint8_t sb_read(uint16_t p){
    switch(p - SB_BASE){
    case 0x04: return sb.mix_idx;
    case 0x05: return sb_mixer_read(sb.mix_idx);
    case 0x0A:                                   /* read data */
        if(sb.outpos < sb.outlen){
            uint8_t r = sb.outbuf[sb.outpos++];
            if(sb.outpos >= sb.outlen) sb.outpos = sb.outlen = 0;
            return r;
        }
        return 0x00;
    case 0x0C: return 0x7F;                      /* write buffer always ready */
    case 0x0E:                                   /* read-buffer status + 8-bit IRQ ack */
        sb.irq8 = 0;
        sb_irq_update();
        return (uint8_t)((sb.outpos < sb.outlen) ? 0xFF : 0x7F);
    case 0x0F:                                   /* 16-bit IRQ ack */
        sb.irq16 = 0;
        sb_irq_update();
        return 0xFF;
    }
    return 0xFF;
}

/* One unit of the running transfer as a signed 16-bit sample, or -99999
 * while the channel is masked.  A 16-bit transfer on an 8-bit channel would
 * take two bytes; the channels here are fixed, so it does not happen. */
#define SB_WAITING (-99999)
static int sb_unit(int *eob){
    int b = dma_fetch(sb_channel(), eob);
    if(b < 0) return SB_WAITING;
    if(sb.bits16) return sb.is_signed ? (int)(int16_t)b : b - 32768;
    return (sb.is_signed ? (int)(int8_t)b : b - 128) * 256;
}

/* Called often from the main loop: move emulated time forward, pull the bytes
 * the card would have consumed in that interval, and hand them to the host. */
#define SB_CHUNK 512
static int16_t pcm[SB_CHUNK];
static int pcm_n;

static void sb_flush(void){ wav_push(pcm, pcm_n); pcm_n = 0; }

void sb_tick(void){
    double now, dt;
    int due;
    if(!sb.playing || sb.last_t < 0.0) return;
    now = emu_now();
    dt = now - sb.last_t;
    if(dt <= 0.0) return;
    if(dt > 0.25) dt = 0.25;                     /* never try to catch up far */
    sb.last_t = now;
    sb.frac += dt * sb.rate;
    due = (int)sb.frac;
    if(due <= 0) return;
    sb.frac -= due;
    if(due > 4096) due = 4096;
    while(due-- > 0 && sb.playing){
        /* one frame: one unit, or two for stereo (mixed to the WAV's mono);
         * scaled to 3/4 so that a full-scale sample leaves headroom */
        int ch, n = sb.stereo ? 2 : 1, sum = 0;
        for(ch = 0; ch < n && sb.playing; ch++){
            int eob, s = sb_unit(&eob);
            /* a masked channel (a driver's pause masks it) holds the
             * transfer where it is: no samples, the frames due are
             * dropped as for a halted DSP, and it goes on when unmasked */
            if(s == SB_WAITING){ due = 0; break; }
            sum += s;
            /* What ends a transfer, and so interrupts: the DSP's own unit
             * count, which is the card's job on hardware.  The controller's
             * wrap (eob) is invisible to the DSP - it just reloads and keeps
             * going.  With no length ever given (a 1Ch with no preceding
             * 48h) there is no count to run down, so fall back to the wrap
             * rather than fire on every single sample. */
            { int fire;
              if(sb_dmairq || sb.block_size <= 0) fire = eob;
              else fire = (--sb.dsp_left <= 0);
              if(fire){
                  sb.dsp_left = sb.block_size;
                  if(sb.bits16) sb.irq16 = 1; else sb.irq8 = 1;
                  sb_irq_update();
                  if(!sb.auto_init) sb.playing = 0;
              } }
        }
        if(ch == 0) break;
        pcm[pcm_n++] = (int16_t)(sum / ch * 3 / 4);
        if(pcm_n == SB_CHUNK) sb_flush();
    }
}

/* ------------------------------------------------------------------ OPL */
/* An OPL2 (AdLib) at 388h/389h: the two timers, so that a driver's probe
 * finds the chip, and with -oplwav its sound (runtime/opl.c) into a WAV
 * file.  Timer 1 counts up from register 2 in steps of 80 us, timer 2 from
 * register 3 in steps of 320 us; at the overflow each sets its flag in the
 * status (bit 6, bit 5, and bit 7 with either) unless register 4 masks it
 * (bit 6, bit 5), and reloads.  Register 4 with bit 7 clears the flags and
 * nothing else; otherwise bits 0 and 1 run timer 1 and 2, a timer that is
 * started loads its register then (a write that leaves it running does not
 * restart it: how the chip takes that is not known here).  The flags are
 * worked out from the clock when the status is read.  The status's low
 * bits read 06h, as an OPL2's are said to. */
static uint8_t opl_regs[256];
static struct {
    int run, mask;                   /* per timer: bit 0 timer 1, bit 1 timer 2 */
    double start[2];                 /* when each was started or reloaded last */
    uint8_t flags;                   /* bits 7, 6, 5 as the status reads them */
} opl_t;

static double opl_period(int k){
    return (256 - opl_regs[2 + k]) * (k ? 320e-6 : 80e-6);
}
static void opl_timers(void){
    int k;
    double now = emu_now();
    for(k = 0; k < 2; k++){
        double per;
        if(!(opl_t.run & (1 << k))) continue;
        per = opl_period(k);
        if(now - opl_t.start[k] < per) continue;
        /* the overflows since the start; the timer goes on from the last */
        opl_t.start[k] += per * (double)(uint64_t)((now - opl_t.start[k]) / per);
        if(!(opl_t.mask & (1 << k))) opl_t.flags |= (uint8_t)(0x80 | (k ? 0x20 : 0x40));
    }
}

/* -oplwav: the chip's own rate, mono, sample k the moment k/OPL_HZ s from
 * t=0 (as -cdwav); a register write takes effect at the sample of its
 * moment */
#define OPL_HZ 49716
static OPL opl_synth;
static FILE *oplwav_fp;
static uint64_t oplwav_n;

static void oplwav_header(void){
    uint8_t h[44];
    uint32_t bytes = (uint32_t)(oplwav_n * 2);
    memset(h, 0, sizeof(h));
    memcpy(h, "RIFF", 4); wav_put32(h+4, 36 + bytes); memcpy(h+8, "WAVEfmt ", 8);
    h[16] = 16; h[20] = 1; h[22] = 1;                  /* PCM, mono */
    wav_put32(h+24, OPL_HZ); wav_put32(h+28, OPL_HZ*2);
    h[32] = 2; h[34] = 16;
    memcpy(h+36, "data", 4); wav_put32(h+40, bytes);
    fseek(oplwav_fp, 0, SEEK_SET);
    fwrite(h, 1, 44, oplwav_fp);
    fseek(oplwav_fp, 0, SEEK_END);
}
void opl_wav_open(const char *path){
    oplwav_fp = fopen(path, "wb");
    if(!oplwav_fp){ fprintf(stderr, "cannot write %s\n", path); return; }
    opl_init(&opl_synth, OPL_HZ);
    oplwav_n = 0;
    oplwav_header();
}
void opl_wav_tick(void){
    int16_t buf[512];
    uint64_t upto;
    if(!oplwav_fp) return;
    upto = (uint64_t)(emu_now() * OPL_HZ);
    while(oplwav_n < upto){
        int n = upto - oplwav_n > 512 ? 512 : (int)(upto - oplwav_n);
        opl_render(&opl_synth, buf, n);
        fwrite(buf, 2, (size_t)n, oplwav_fp);
        oplwav_n += (uint64_t)n;
    }
}
void opl_wav_close(void){
    if(!oplwav_fp) return;
    opl_wav_tick();
    oplwav_header();
    fclose(oplwav_fp); oplwav_fp = NULL;
}

void opl_io_write(int reg, uint8_t v){
    reg &= 255;
    if(reg == 4){
        int k, run = v & 3;
        opl_timers();
        if(v & 0x80){ opl_t.flags = 0; return; }
        opl_t.mask = ((v >> 6) & 1) | ((v >> 4) & 2);
        for(k = 0; k < 2; k++)
            if((run & (1 << k)) && !(opl_t.run & (1 << k))) opl_t.start[k] = emu_now();
        opl_t.run = run;
    }
    opl_regs[reg] = v;
    if(oplwav_fp){ opl_wav_tick(); opl_write(&opl_synth, reg, v); }
}
uint8_t opl_io_status(void){
    opl_timers();
    return (uint8_t)(opl_t.flags | 0x06);
}
void spk_update(int on, uint16_t div){ (void)on; (void)div; }


void sound_init(void){ dma_reset(0); dma_reset(1); sb_reset_dev(); }
