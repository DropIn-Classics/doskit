/* cdaudio.c - see cdaudio.h */
#include "cdaudio.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"
#include "sys.h"
#include "../third_party/stb_vorbis/stb_vorbis.c"

#define CD_HZ 44100
#define FRAME_SAMPLES 588              /* CD_HZ / 75 */

enum { SRC_RAW, SRC_WAVE, SRC_OGG };

/* the disc: track i+1 starts at frame start[i], its file's first sample
 * is at frame file_base[i] */
static int ntracks;
static uint32_t trk_start[CDA_TRACKS_MAX], trk_file_base[CDA_TRACKS_MAX], leadout;
static uint8_t trk_data[CDA_TRACKS_MAX], trk_kind[CDA_TRACKS_MAX];
static char trk_path[CDA_TRACKS_MAX][1024];

/* the play on the clock: frames [play_pos, play_to), running since
 * play_us unless stopped */
static int play_on;
static uint32_t play_pos, play_to;
static uint64_t play_us;

/* the play as mixed: disc sample aud_d (in 1/65536ths aud_frac) up to
 * aud_to */
static int aud_on;
static uint64_t aud_d, aud_to;
static uint32_t aud_frac;

static uint8_t chan_in[4] = { 0, 1, 2, 3 }, chan_vol[4] = { 255, 255, 255, 255 };

/* ---- the cue sheet */

static uint32_t ld32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* `name` (backslashes or slashes) under `dir`, each part found ignoring
 * case, as a sheet written on Windows names its files */
static int find_path(const char *dir, const char *name, char *out, size_t n)
{
    char at[1024], part[260];
    const char *p = name;

    snprintf(at, sizeof at, "%s", dir);
    while (*p) {
        size_t len = 0;

        while (*p == '\\' || *p == '/')
            p++;
        while (p[len] && p[len] != '\\' && p[len] != '/' && len < sizeof part - 1) {
            part[len] = p[len];
            len++;
        }
        part[len] = 0;
        p += len;
        if (!len)
            break;
        if (!sys_find(at, part, out, n))
            return 0;
        snprintf(at, sizeof at, "%s", out);
    }
    snprintf(out, n, "%s", at);
    return 1;
}

/* a file's length in frames: an Ogg Vorbis stream by its last page's
 * granule position and the rate in its identification header; a WAVE
 * (44.1 kHz, 16-bit stereo) by its data; raw sectors by its size.  0 when
 * it cannot be told. */
static uint32_t file_frames(const char *path, int sector, uint8_t *kind)
{
    static uint8_t tail[65536];
    FILE *f = fopen(path, "rb");
    uint8_t head[64];
    size_t got;
    long size;

    if (!f)
        return 0;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    got = fread(head, 1, sizeof head, f);
    *kind = SRC_RAW;
    if (got >= 4 && !memcmp(head, "OggS", 4)) {
        uint32_t rate = 0;
        uint64_t granule = 0;
        size_t i, k;

        *kind = SRC_OGG;
        for (i = 0; i + 16 <= got; i++)
            if (!memcmp(&head[i], "\001vorbis", 7)) {
                rate = ld32(&head[i + 12]);
                break;
            }
        fseek(f, size > (long)sizeof tail ? size - (long)sizeof tail : 0, SEEK_SET);
        k = fread(tail, 1, sizeof tail, f);
        fclose(f);
        for (i = k >= 14 ? k - 14 : 0;; i--) {
            if (!memcmp(&tail[i], "OggS", 4)) {
                granule = (uint64_t)ld32(&tail[i + 6]) | (uint64_t)ld32(&tail[i + 10]) << 32;
                break;
            }
            if (i == 0)
                break;
        }
        if (!rate || !granule)
            return 0;
        return (uint32_t)((granule * 75 + rate - 1) / rate);
    }
    fclose(f);
    if (got >= 12 && !memcmp(head, "RIFF", 4) && !memcmp(&head[8], "WAVE", 4)) {
        *kind = SRC_WAVE;
        return (uint32_t)((size - 44 + 2351) / 2352);
    }
    return (uint32_t)((size + sector - 1) / sector);
}

static int fail(char *err, size_t n, const char *what, const char *cue)
{
    snprintf(err, n, "%s: %s", cue, what);
    ntracks = 0;
    return -1;
}

int cda_open(const char *cue, char *err, size_t n)
{
    FILE *f = fopen(cue, "r");
    char line[1024], dir[1024], cur[1024] = "", *slash;
    uint32_t file_base = 150, file_len = 0;
    uint8_t kind = SRC_RAW;
    int i;

    ntracks = 0;
    play_on = aud_on = 0;
    if (!f)
        return fail(err, n, "cannot be opened", cue);
    snprintf(dir, sizeof dir, "%s", cue);
    slash = strrchr(dir, '/');
    {
        char *b = strrchr(dir, '\\');

        if (b && (!slash || b > slash))
            slash = b;
    }
    if (slash)
        *slash = 0;
    else
        snprintf(dir, sizeof dir, ".");
    while (fgets(line, sizeof line, f)) {
        char *p = line, word[16];
        int k = 0;

        while (isspace((unsigned char)*p))
            p++;
        while (*p && !isspace((unsigned char)*p) && k < 15)
            word[k++] = (char)toupper((unsigned char)*p++);
        word[k] = 0;
        while (isspace((unsigned char)*p))
            p++;
        if (!strcmp(word, "FILE")) {
            char name[512], *e;

            if (*p == '"') {
                p++;
                e = strchr(p, '"');
            } else {
                e = p;
                while (*e && !isspace((unsigned char)*e))
                    e++;
            }
            if (!e) {
                fclose(f);
                return fail(err, n, "a bad FILE line", cue);
            }
            snprintf(name, sizeof name, "%.*s", (int)(e - p), p);
            file_base += file_len;
            if (!find_path(dir, name, cur, sizeof cur)) {
                fclose(f);
                return fail(err, n, "a file it names is missing", cue);
            }
            file_len = 0xFFFFFFFFu;     /* measured at its first track's mode */
        } else if (!strcmp(word, "TRACK")) {
            const char *mode = p;
            int sector;

            while (*mode && !isspace((unsigned char)*mode))
                mode++;
            while (isspace((unsigned char)*mode))
                mode++;
            if (atoi(p) != ntracks + 1 || ntracks >= CDA_TRACKS_MAX || !cur[0]) {
                fclose(f);
                return fail(err, n, "a track out of order", cue);
            }
            trk_data[ntracks] = (uint8_t)(strncmp(mode, "AUDIO", 5) != 0 && strncmp(mode, "audio", 5) != 0);
            sector = !strncmp(mode, "MODE1/2048", 10) ? 2048 : 2352;
            if (file_len == 0xFFFFFFFFu) {
                file_len = file_frames(cur, sector, &kind);
                if (!file_len) {
                    fclose(f);
                    return fail(err, n, "a file's length cannot be told", cue);
                }
            }
            trk_start[ntracks] = 0xFFFFFFFFu;
            snprintf(trk_path[ntracks], sizeof trk_path[0], "%s", cur);
            trk_kind[ntracks] = kind;
            ntracks++;
        } else if (!strcmp(word, "PREGAP") && ntracks) {
            int m, s, fr;

            if (sscanf(p, "%d:%d:%d", &m, &s, &fr) == 3)
                file_base += (uint32_t)(m * 4500 + s * 75 + fr);
        } else if (!strcmp(word, "INDEX") && ntracks) {
            int idx, m, s, fr;

            if (sscanf(p, "%d %d:%d:%d", &idx, &m, &s, &fr) == 4 && idx == 1) {
                trk_start[ntracks - 1] = file_base + (uint32_t)(m * 4500 + s * 75 + fr);
                trk_file_base[ntracks - 1] = file_base;
            }
        }
    }
    fclose(f);
    if (!ntracks)
        return fail(err, n, "no tracks", cue);
    for (i = 0; i < ntracks; i++)
        if (trk_start[i] == 0xFFFFFFFFu)
            return fail(err, n, "a track without INDEX 01", cue);
    leadout = file_base + file_len;
    return 0;
}

int cda_tracks(void)
{
    return ntracks;
}

uint32_t cda_track_start(int i)
{
    return i >= 0 && i < ntracks ? trk_start[i] : leadout;
}

int cda_track_data(int i)
{
    return i >= 0 && i < ntracks ? trk_data[i] : 1;
}

uint32_t cda_leadout(void)
{
    return leadout;
}

/* ---- the play */

static uint32_t now_pos(void)
{
    if (play_on) {
        uint64_t f = (plat_micros() - play_us) * 75 / 1000000;
        uint64_t at = play_pos + f;

        if (at >= play_to) {
            play_on = 0;
            play_pos = play_to;
        } else
            return (uint32_t)at;
    }
    return play_pos;
}

void cda_play(uint32_t start, uint32_t n)
{
    play_pos = start;
    play_to = start + n;
    play_us = plat_micros();
    play_on = aud_on = n != 0;
    aud_d = (uint64_t)start * FRAME_SAMPLES;
    aud_to = (uint64_t)(start + n) * FRAME_SAMPLES;
    aud_frac = 0;
}

void cda_stop(void)
{
    play_pos = now_pos();
    play_on = 0;
    aud_on = 0;
}

void cda_resume(void)
{
    if (play_on || play_pos >= play_to)
        return;
    play_us = plat_micros();
    play_on = 1;
    aud_on = aud_d < aud_to;
}

int cda_playing(void)
{
    now_pos();
    return play_on;
}

uint32_t cda_position(void)
{
    return now_pos();
}

uint32_t cda_play_end(void)
{
    return play_to;
}

void cda_channels(const uint8_t in[4], const uint8_t vol[4])
{
    memcpy(chan_in, in, 4);
    memcpy(chan_vol, vol, 4);
}

/* ---- the samples: the open track's file, read on from buf_at + buf_n */

#define SRC_BUF 1024
static int src_trk = -1;
static uint32_t src_rate = CD_HZ;
static FILE *src_fp;
static stb_vorbis *src_ov;
static int16_t buf[SRC_BUF * 2];
static uint64_t buf_at;
static int buf_n;

static void src_close(void)
{
    if (src_fp)
        fclose(src_fp);
    if (src_ov)
        stb_vorbis_close(src_ov);
    src_fp = NULL;
    src_ov = NULL;
    src_trk = -1;
}

static int src_open(int t)
{
    src_close();
    src_trk = t;
    buf_at = 0;
    buf_n = 0;
    src_rate = CD_HZ;
    if (trk_kind[t] == SRC_OGG) {
        int e;

        src_ov = stb_vorbis_open_filename(trk_path[t], &e, NULL);
        if (!src_ov)
            return 0;
        src_rate = stb_vorbis_get_info(src_ov).sample_rate;
    } else {
        src_fp = fopen(trk_path[t], "rb");
        if (!src_fp)
            return 0;
        fseek(src_fp, trk_kind[t] == SRC_WAVE ? 44 : 0, SEEK_SET);
    }
    return 1;
}

static int src_fill(void)
{
    if (src_ov)
        buf_n = stb_vorbis_get_samples_short_interleaved(src_ov, 2, buf, SRC_BUF * 2);
    else if (src_fp) {
        uint8_t raw[SRC_BUF * 4];
        int i;

        buf_n = (int)fread(raw, 4, SRC_BUF, src_fp);
        for (i = 0; i < buf_n * 2; i++)
            buf[i] = (int16_t)(raw[2 * i] | raw[2 * i + 1] << 8);
    } else
        buf_n = 0;
    return buf_n > 0;
}

/* the open file's sample j into s[2]; silence past its end */
static void src_sample(uint64_t j, int16_t *s)
{
    s[0] = s[1] = 0;
    if (j < buf_at || j >= buf_at + (uint64_t)buf_n + CD_HZ) {
        if (src_ov) {
            if (!stb_vorbis_seek(src_ov, (unsigned)j)) {
                buf_n = 0;
                return;
            }
        } else if (src_fp) {
            if (fseek(src_fp, (long)(j * 4 + (trk_kind[src_trk] == SRC_WAVE ? 44 : 0)), SEEK_SET)) {
                buf_n = 0;
                return;
            }
        } else
            return;
        buf_at = j;
        buf_n = 0;
    }
    while (j >= buf_at + (uint64_t)buf_n) {
        buf_at += (uint64_t)buf_n;
        if (!src_fill())
            return;
    }
    s[0] = buf[2 * (j - buf_at)];
    s[1] = buf[2 * (j - buf_at) + 1];
}

static int track_of(uint32_t f)
{
    int i = 0;

    while (i + 1 < ntracks && f >= trk_start[i + 1])
        i++;
    return i;
}

/* disc sample d into s[2]: its track's file, silence on a data track */
static void disc_sample(uint64_t d, int16_t *s)
{
    uint32_t f = (uint32_t)(d / FRAME_SAMPLES);
    int t = src_trk;
    uint64_t j;

    s[0] = s[1] = 0;
    if (!ntracks)
        return;
    if (t < 0 || f < trk_start[t] || (t + 1 < ntracks && f >= trk_start[t + 1]))
        t = track_of(f);
    if (trk_data[t] || f < trk_file_base[t])
        return;
    if (t != src_trk && !src_open(t))
        return;
    j = d - (uint64_t)trk_file_base[t] * FRAME_SAMPLES;
    if (src_rate != CD_HZ)
        j = j * src_rate / CD_HZ;
    src_sample(j, s);
}

static int16_t sat(int v)
{
    return (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
}

void cda_mix(int16_t *out, int frames, int rate)
{
    uint32_t step = (uint32_t)(((uint64_t)CD_HZ << 16) / (uint32_t)(rate > 0 ? rate : CD_HZ));
    int i, o;

    for (i = 0; i < frames && aud_on; i++) {
        int16_t s[2];

        if (aud_d >= aud_to) {
            aud_on = 0;
            break;
        }
        disc_sample(aud_d, s);
        for (o = 0; o < 2; o++)
            if (chan_in[o] < 2)
                out[2 * i + o] = sat(out[2 * i + o] + s[chan_in[o]] * chan_vol[o] / 255);
        aud_frac += step;
        aud_d += aud_frac >> 16;
        aud_frac &= 0xFFFF;
    }
}
