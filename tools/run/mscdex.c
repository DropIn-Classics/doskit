/* MSCDEX: INT 2Fh AH=15h, the CD-ROM extensions, with drive D: as the CD.
 *
 * The CD's files are the guest's tree (dos.c: every drive letter names
 * it), so a program finds its files on D: through DOS as it would on the
 * CD.  What goes to the drive itself, the device requests of AX=1510h, is
 * answered from a table of tracks: by default a disc with one data track
 * and no audio; with -cue the tracks of a cue sheet, their lengths from
 * the files it names (mscdex_cue).  The tree is not an image, so there are
 * no sectors to read raw: READ LONG says "sector not found".  Audio play
 * requests are taken and kept track of on the emulated clock, so a
 * program that asks for the audio status sees the play run and end;
 * nothing is heard (-cd prints them, with the track).
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
#include <ctype.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dirent.h>
#endif

#define AX REG16(R_EAX)
#define BX REG16(R_EBX)
#define CX REG16(R_ECX)
#define AL REG8(0)
#define AH REG8(4)

int cd_log = 0;

#define CD_DRIVE   3                   /* D: */
#define DEV_SEG    0xF000              /* the device driver's header */
#define DEV_OFF    0x0F00

#define ST_ERROR   0x8000
#define ST_BUSY    0x0200
#define ST_DONE    0x0100
#define E_UNKNOWN_UNIT 0x01
#define E_UNKNOWN_CMD  0x03
#define E_NOT_FOUND    0x08

/* The disc: track i+1 starts at frame trk_start[i]; the lead-out at
 * leadout.  Track 1 begins at 00:02:00 (frame 150), as on a pressed CD. */
#define TRACKS_MAX 99
static int ntracks;
static uint32_t trk_start[TRACKS_MAX], leadout;
static uint8_t trk_data[TRACKS_MAX];

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

/* the track (0-based) holding frame f, the last one before the lead-out
 * for a frame beyond */
static int track_of(uint32_t f){
    int i = 0;
    while(i + 1 < ntracks && f >= trk_start[i+1]) i++;
    return i;
}

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
    case 0x08: st32u(&ram[cb+1], leadout); return 0;
    case 0x09: ram[cb+1] = 1; return 0;         /* media not changed */
    case 0x0A:                                  /* audio disk info */
        ram[cb+1] = 1; ram[cb+2] = (uint8_t)ntracks;
        st32u(&ram[cb+3], redbook(leadout));
        return 0;
    case 0x0B: {                                /* audio track info */
        int t = ram[cb+1];
        if(t < 1 || t > ntracks) return ST_ERROR | E_NOT_FOUND;
        st32u(&ram[cb+2], redbook(trk_start[t-1]));
        ram[cb+6] = trk_data[t-1] ? 0x40 : 0x00;
        return 0; }
    case 0x0C: {                                /* Q channel: where the play is */
        uint32_t at = play_now();
        int t = track_of(at);
        uint32_t rel = at > trk_start[t] ? at - trk_start[t] : 0;
        ram[cb+1] = (uint8_t)((trk_data[t] ? 0x40 : 0x00) | 1);
        ram[cb+2] = bcd((uint32_t)t + 1); ram[cb+3] = bcd(1);
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
    switch(cmd){
    case 0x03:
        if(cd_log) printf("cd: ioctl input %02X t=%.6f\n", ram[cb], emu_now());
        return ioctl_in(cb);
    case 0x0C:                                  /* eject, lock, reset, channels, close: taken */
        if(cd_log) printf("cd: ioctl output %02X t=%.6f\n", ram[cb], emu_now());
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
        if(cd_log){
            int t = track_of(start);
            printf("cd: play frames %u..%u (track %d + %u) t=%.6f\n", (unsigned)start,
                   (unsigned)(start+n), t + 1, (unsigned)(start - trk_start[t]), emu_now());
        }
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
        if(cd_log) printf("cd: request %02X refused t=%.6f\n", cmd, emu_now());
        return ST_ERROR | E_UNKNOWN_CMD;
    }
}

/* INT 2Fh: MSCDEX's functions; the rest of the multiplex (AH other than
 * 15h) is left as it was, with nothing installed */
static void mux_int2f(void){
    if(AH != 0x15) return;
    if(cd_log && AL != 0x10) printf("cd: INT 2Fh AX=%04X BX=%04X CX=%04X t=%.6f\n",
                                    AX, BX, CX, emu_now());
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

/* ------------------------------------------------------------ cue sheet */

/* `name` (backslashes or slashes) under `dir`, each part matched without
 * regard to case, as a sheet written on Windows names its files */
static int find_nocase(const char *dir, const char *name, char *out, size_t n){
    char part[260];
    const char *p = name;
    snprintf(out, n, "%s", dir);
    while(*p){
        size_t len = 0;
        int found = 0;
        while(*p == '\\' || *p == '/') p++;
        while(p[len] && p[len] != '\\' && p[len] != '/' && len < sizeof(part) - 1){ part[len] = p[len]; len++; }
        part[len] = 0;
        p += len;
        if(!len) break;
#ifdef _WIN32
        {   char pat[1024]; WIN32_FIND_DATAA fd; HANDLE h;
            snprintf(pat, sizeof(pat), "%s\\%s", out, part);
            h = FindFirstFileA(pat, &fd);
            if(h != INVALID_HANDLE_VALUE){ FindClose(h); found = 1; snprintf(part, sizeof(part), "%s", fd.cFileName); } }
#else
        {   DIR *d = opendir(out);
            struct dirent *e;
            if(d){
                while((e = readdir(d)) != NULL)
                    if(!_stricmp(e->d_name, part)){ snprintf(part, sizeof(part), "%s", e->d_name); found = 1; break; }
                closedir(d);
            } }
#endif
        if(!found) return 0;
        { size_t l = strlen(out);
          if(l + 1 + strlen(part) + 1 > n) return 0;
          snprintf(out + l, n - l, "/%s", part); }
    }
    return 1;
}

/* a file's length in frames: raw sectors (BINARY) by its size; an Ogg
 * Vorbis stream by its last page's granule position (samples) and the
 * rate in its identification header; a WAVE (44.1 kHz, 16-bit stereo, as
 * a CD's audio is) by its data size.  0 when the length cannot be told. */
static uint32_t file_frames(const char *path, int sector){
    FILE *f = fopen(path, "rb");
    long size;
    uint8_t head[64];
    size_t got;
    if(!f) return 0;
    fseek(f, 0, SEEK_END); size = ftell(f); fseek(f, 0, SEEK_SET);
    got = fread(head, 1, sizeof(head), f);
    if(got >= 4 && !memcmp(head, "OggS", 4)){
        static uint8_t tail[65536 + 4];
        uint32_t rate = 0;
        uint64_t granule = 0;
        long from = size > 65536 ? size - 65536 : 0;
        size_t i, k;
        for(i = 0; i + 16 <= got; i++)
            if(!memcmp(&head[i], "\001vorbis", 7)){ rate = ld32u(&head[i+12]); break; }
        fseek(f, from, SEEK_SET);
        k = fread(tail, 1, 65536, f);
        for(i = k >= 14 ? k - 14 : 0; ; i--){
            if(!memcmp(&tail[i], "OggS", 4)){ memcpy(&granule, &tail[i+6], 8); break; }
            if(i == 0) break;
        }
        fclose(f);
        if(!rate || !granule) return 0;
        return (uint32_t)((granule * 75 + rate - 1) / rate);
    }
    fclose(f);
    if(got >= 12 && !memcmp(head, "RIFF", 4) && !memcmp(&head[8], "WAVE", 4))
        return (uint32_t)((size - 44 + 2351) / 2352);
    return (uint32_t)((size + sector - 1) / sector);
}

/* The tracks of a cue sheet: each FILE's tracks at their INDEX 01 inside
 * it, the files one after the other, PREGAP adding frames before a
 * track.  0, or -1 with the reason in err. */
int mscdex_cue(const char *cue, char *err, size_t n){
    FILE *f = fopen(cue, "r");
    char line[1024], dir[1024], cur[1024] = "";
    uint32_t file_base = 150, file_len = 0, gap = 0;
    int sector = 2352;
    char *slash;
    if(!f){ snprintf(err, n, "cannot open %s", cue); return -1; }
    snprintf(dir, sizeof(dir), "%s", cue);
    slash = strrchr(dir, '/');
#ifdef _WIN32
    { char *b = strrchr(dir, '\\'); if(b && (!slash || b > slash)) slash = b; }
#endif
    if(slash) *slash = 0; else snprintf(dir, sizeof(dir), ".");
    ntracks = 0;
    while(fgets(line, sizeof(line), f)){
        char *p = line, word[16];
        int k = 0;
        while(isspace((unsigned char)*p)) p++;
        while(*p && !isspace((unsigned char)*p) && k < 15) word[k++] = (char)toupper((unsigned char)*p++);
        word[k] = 0;
        while(isspace((unsigned char)*p)) p++;
        if(!strcmp(word, "FILE")){
            char name[512], *e;
            if(*p == '"'){ p++; e = strchr(p, '"'); } else { e = p; while(*e && !isspace((unsigned char)*e)) e++; }
            if(!e){ snprintf(err, n, "bad FILE line in %s", cue); fclose(f); return -1; }
            snprintf(name, sizeof(name), "%.*s", (int)(e - p), p);
            file_base += file_len;
            if(!find_nocase(dir, name, cur, sizeof(cur))){
                snprintf(err, n, "%s: no file %s", cue, name); fclose(f); return -1;
            }
            file_len = 0xFFFFFFFFu;             /* measured at its first track's mode */
        } else if(!strcmp(word, "TRACK")){
            int num = atoi(p);
            const char *mode = p;
            while(*mode && !isspace((unsigned char)*mode)) mode++;
            while(isspace((unsigned char)*mode)) mode++;
            if(num != ntracks + 1 || ntracks >= TRACKS_MAX){
                snprintf(err, n, "%s: track %d out of order", cue, num); fclose(f); return -1;
            }
            trk_data[ntracks] = (uint8_t)(_strnicmp(mode, "AUDIO", 5) != 0);
            sector = !_strnicmp(mode, "MODE1/2048", 10) ? 2048 : 2352;
            if(file_len == 0xFFFFFFFFu){
                file_len = file_frames(cur, sector);
                if(!file_len){ snprintf(err, n, "%s: cannot tell the length of %s", cue, cur); fclose(f); return -1; }
            }
            trk_start[ntracks] = 0xFFFFFFFFu;
            ntracks++;
        } else if(!strcmp(word, "PREGAP") && ntracks){
            int m = 0, s = 0, fr = 0;
            if(sscanf(p, "%d:%d:%d", &m, &s, &fr) == 3){ gap = (uint32_t)(m*4500 + s*75 + fr); file_base += gap; }
        } else if(!strcmp(word, "INDEX") && ntracks){
            int idx = 0, m = 0, s = 0, fr = 0;
            if(sscanf(p, "%d %d:%d:%d", &idx, &m, &s, &fr) == 4 && idx == 1)
                trk_start[ntracks-1] = file_base + (uint32_t)(m*4500 + s*75 + fr);
        }
    }
    fclose(f);
    if(!ntracks){ snprintf(err, n, "%s: no tracks", cue); return -1; }
    { int i;
      for(i = 0; i < ntracks; i++)
          if(trk_start[i] == 0xFFFFFFFFu){ snprintf(err, n, "%s: track %d has no INDEX 01", cue, i+1); return -1; } }
    leadout = file_base + file_len;
    return 0;
}

void mscdex_report(void){
    int i;
    printf("cd: %d tracks, lead-out %02u:%02u:%02u\n", ntracks,
           (unsigned)(leadout/4500), (unsigned)(leadout/75%60), (unsigned)(leadout%75));
    for(i = 0; i < ntracks; i++){
        uint32_t s = trk_start[i], e = i + 1 < ntracks ? trk_start[i+1] : leadout;
        printf("cd: track %2d %s %02u:%02u:%02u, %u frames\n", i + 1, trk_data[i] ? "data " : "audio",
               (unsigned)(s/4500), (unsigned)(s/75%60), (unsigned)(s%75), (unsigned)(e - s));
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
    /* the default disc: one data track of 300,000 sectors (about 585 MB),
     * a size that does not depend on the files, so runs stay alike */
    ntracks = 1; trk_start[0] = 150; trk_data[0] = 1; leadout = 150 + 300000;
    playing = paused = 0; play_from = play_to = play_pos = 0; play_t = 0;
    cb_table[0x2F] = mux_int2f;
}
