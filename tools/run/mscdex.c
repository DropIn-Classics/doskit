/* MSCDEX: INT 2Fh AH=15h, the CD-ROM extensions, with drive D: as the CD.
 *
 * The CD's files are the guest's tree (dos.c: every drive letter names
 * it), so a program finds its files on D: through DOS as it would on the
 * CD.  What goes to the drive itself, the device requests of AX=1510h, is
 * answered for a disc with one data track and no audio tracks (the tree
 * is not an image, so there are no sectors to read raw: READ LONG says
 * "sector not found").  Audio play requests are taken and kept track of
 * on the emulated clock, so a program that asks for the audio status sees
 * the play run and end; nothing is heard (-cd prints them).
 *
 * The request header (ES:BX): [0] length, [1] subunit, [2] command,
 * [3] status word (bit 15 error with the code in the low byte, bit 9
 * busy, bit 8 done); from [0Dh] the command's fields.  IOCTL input and
 * output (commands 3, 0Ch) take a control block at the far pointer
 * [0Eh], its first byte the sub-function.  Addresses are frames, 75 a
 * second: HSG (a number) or Red Book (frame, second, minute, 0 in a
 * dword).
 */
#include "dosrun.h"

#define AX REG16(R_EAX)
#define BX REG16(R_EBX)
#define CX REG16(R_ECX)
#define AL REG8(0)
#define AH REG8(4)

int cd_log = 0;

#define CD_DRIVE   3                   /* D: */
#define DEV_SEG    0xF000              /* the device driver's header */
#define DEV_OFF    0x0F00
/* the disc: track 1, data, from 00:02:00 (frame 150); the lead-out after
 * 300,000 sectors (about 585 MB), a size that does not depend on the
 * files, so runs stay alike */
#define TRACK1     150u
#define LEADOUT    (TRACK1 + 300000u)

#define ST_ERROR   0x8000
#define ST_BUSY    0x0200
#define ST_DONE    0x0100
#define E_UNKNOWN_UNIT 0x01
#define E_UNKNOWN_CMD  0x03
#define E_NOT_FOUND    0x08
#define E_READ_FAULT   0x0B

/* audio play state: frames [play_from, play_to) begun at play_t */
static int playing = 0, paused = 0;
static uint32_t play_from, play_to, play_pos;
static double play_t;

static uint32_t redbook(uint32_t f){
    return ((f / 4500) << 16) | (((f / 75) % 60) << 8) | (f % 75);
}
static uint32_t from_redbook(uint32_t r){
    return ((r >> 16) & 0xFF) * 4500 + ((r >> 8) & 0xFF) * 75 + (r & 0xFF);
}
static uint8_t bcd(uint32_t v){ return (uint8_t)(((v / 10) % 10) << 4 | (v % 10)); }

/* the play position now; a play that has run out ends */
static uint32_t play_now(void){
    if(playing && !paused){
        double f = (emu_now() - play_t) * 75.0;
        uint32_t at = play_pos + (uint32_t)f;
        if(at >= play_to){ playing = 0; play_pos = play_to; return play_to; }
        return at;
    }
    return play_pos;
}

static uint16_t ioctl_in(uint32_t cb){
    switch(ram[cb]){
    case 0x00: st16u(&ram[cb+1], DEV_OFF); st16u(&ram[cb+3], DEV_SEG); return 0;
    case 0x01: {                                /* location of the head */
        uint32_t at = play_now();
        st32u(&ram[cb+2], ram[cb+1] ? redbook(at) : at);
        return 0; }
    case 0x04: {                                /* audio channels: 0..3 at full volume */
        int i;
        for(i = 0; i < 4; i++){ ram[cb+1+2*i] = (uint8_t)i; ram[cb+2+2*i] = 0xFF; }
        return 0; }
    case 0x06:                                  /* device status: door closed, */
        st32u(&ram[cb+1], 0x0216);              /* unlocked, cooked and raw, audio, */
        return 0;                               /* HSG and Red Book addressing */
    case 0x07: ram[cb+1] = 0; st16u(&ram[cb+2], 2048); return 0;
    case 0x08: st32u(&ram[cb+1], LEADOUT); return 0;
    case 0x09: ram[cb+1] = 1; return 0;         /* media not changed */
    case 0x0A:                                  /* audio disk info */
        ram[cb+1] = 1; ram[cb+2] = 1;
        st32u(&ram[cb+3], redbook(LEADOUT));
        return 0;
    case 0x0B:                                  /* audio track info */
        if(ram[cb+1] != 1) return ST_ERROR | E_NOT_FOUND;
        st32u(&ram[cb+2], redbook(TRACK1));
        ram[cb+6] = 0x40;                       /* data track */
        return 0;
    case 0x0C: {                                /* Q channel: where the play is */
        uint32_t at = play_now(), rel = at > TRACK1 ? at - TRACK1 : 0;
        ram[cb+1] = 0x41; ram[cb+2] = bcd(1); ram[cb+3] = bcd(1);
        ram[cb+4] = (uint8_t)(rel / 4500); ram[cb+5] = (uint8_t)(rel / 75 % 60); ram[cb+6] = (uint8_t)(rel % 75);
        ram[cb+7] = 0;
        ram[cb+8] = (uint8_t)(at / 4500); ram[cb+9] = (uint8_t)(at / 75 % 60); ram[cb+10] = (uint8_t)(at % 75);
        return 0; }
    case 0x0F:                                  /* audio status */
        play_now();
        st16u(&ram[cb+1], (uint16_t)(paused ? 1 : 0));
        st32u(&ram[cb+3], redbook(play_from));
        st32u(&ram[cb+7], redbook(play_to));
        return 0;
    default:
        return ST_ERROR | E_UNKNOWN_CMD;
    }
}

static uint16_t request(uint32_t rh){
    uint8_t cmd = ram[rh+2];
    uint32_t cb = (uint32_t)ld16u(&ram[rh+0x10]) * 16 + ld16u(&ram[rh+0x0E]);
    if(cd_log) printf("cd: request %02X t=%.6f\n", cmd, emu_now());
    switch(cmd){
    case 0x03:
        if(cd_log) printf("cd: ioctl input %02X\n", ram[cb]);
        return ioctl_in(cb);
    case 0x0C:                                  /* eject, lock, reset, channels, close: taken */
        if(cd_log) printf("cd: ioctl output %02X\n", ram[cb]);
        return 0;
    case 0x0D: case 0x0E: case 0x82: case 0x83:  /* open, close, prefetch, seek */
        return 0;
    case 0x80:                                  /* read long: the tree is not an image */
        return ST_ERROR | E_NOT_FOUND;
    case 0x84: {                                /* play audio */
        uint32_t start = ld32u(&ram[rh+0x0E]), n = ld32u(&ram[rh+0x12]);
        if(ram[rh+0x0D]) start = from_redbook(start);
        play_now();
        playing = n != 0; paused = 0;
        play_from = play_pos = start; play_to = start + n; play_t = emu_now();
        if(cd_log) printf("cd: play frames %u..%u t=%.6f\n", (unsigned)start, (unsigned)(start+n), emu_now());
        return 0;
    }
    case 0x85:                                  /* stop: pause a play, else forget it */
        play_now();
        if(playing && !paused) paused = 1;
        else { playing = 0; paused = 0; play_from = play_to = play_pos = 0; }
        if(cd_log) printf("cd: stop t=%.6f\n", emu_now());
        return 0;
    case 0x88:                                  /* resume */
        if(playing && paused){ paused = 0; play_t = emu_now(); }
        if(cd_log) printf("cd: resume t=%.6f\n", emu_now());
        return 0;
    default:
        return ST_ERROR | E_UNKNOWN_CMD;
    }
}

/* INT 2Fh: MSCDEX's functions; the rest of the multiplex (AH other than
 * 15h) is left as it was, with nothing installed */
static void mux_int2f(void){
    if(AH != 0x15) return;
    if(cd_log) printf("cd: INT 2Fh AX=%04X BX=%04X CX=%04X ES=%04X t=%.6f\n",
                      AX, BX, CX, cpu.sreg[S_ES], emu_now());
    switch(AL){
    case 0x00: BX = 1; CX = CD_DRIVE; break;      /* installed: one drive, the first D: */
    case 0x01: {                                  /* drive device list */
        uint32_t a = cpu.sbase[S_ES] + BX;
        mem_w8(a, 0); mem_w16(a+1, DEV_OFF); mem_w16(a+3, DEV_SEG);
        break; }
    case 0x0B: AX = (uint16_t)(CX == CD_DRIVE ? 0x5AD8 : 0); BX = 0xADAD; break;
    case 0x0C: BX = 0x0215; break;                /* version 2.21 */
    case 0x0D: mem_w8(cpu.sbase[S_ES] + BX, CD_DRIVE); break;
    case 0x10: {
        uint32_t rh = cpu.sbase[S_ES] + BX;
        uint16_t st = (CX == CD_DRIVE) ? request(rh) : (ST_ERROR | E_UNKNOWN_UNIT);
        if(playing && !paused && play_now() < play_to) st |= ST_BUSY;
        mem_w16(rh+3, (uint16_t)(st | ST_DONE));
        break; }
    default:
        if(cd_log) printf("cd: unimplemented AL=%02X\n", AL);
        AX = 1; bios_set_cf(1);
        break;
    }
}

void mscdex_init(void){
    /* the device driver's header: no chain, character device with IOCTL,
     * no strategy or interrupt routine to call (a program that calls
     * them finds a far RET), the name, then MSCDEX's fields */
    uint32_t h = (uint32_t)DEV_SEG * 16 + DEV_OFF;
    memset(&ram[h], 0, 0x20);
    st32u(&ram[h], 0xFFFFFFFFu);
    st16u(&ram[h+4], 0xC800);
    st16u(&ram[h+6], 0x1E); st16u(&ram[h+8], 0x1E);
    memcpy(&ram[h+0x0A], "DOSRUNCD", 8);
    ram[h+0x14] = CD_DRIVE + 1;
    ram[h+0x15] = 1;
    ram[h+0x1E] = 0xCB;
    playing = paused = 0; play_from = play_to = play_pos = 0; play_t = 0;
    cb_table[0x2F] = mux_int2f;
}
