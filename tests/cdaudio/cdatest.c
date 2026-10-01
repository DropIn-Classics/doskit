/* cdatest.c - the runtime's cdaudio.c (selftest step 4) on the selftest's
 * cue sheet (make_cue: a data track, a WAVE of a known pattern, then
 * tests/cdplay/TONE.OGG after a PREGAP): the table of contents, the WAVE's
 * samples mixed as they are, the Ogg's tones (left 441 Hz at half scale,
 * right at a quarter) by their loudness, the channels swapped and halved,
 * the play's position and end on the clock (plat_null's, moved by
 * plat_sleep_ms).  With COPY (a folder not there) the disc is first
 * copied there by cdimage.c's cd_copy_disc and the copy's sheet read
 * instead.  Prints "cdaudio ok", exit status 0, or what went wrong.
 *
 *     cdatest CUE [COPY] */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "cdaudio.h"
#include "cdimage.h"
#include "sys.h"
#include "platform.h"

static int16_t out[2 * 44100];

/* the pattern selftest.py's wave_sample writes, sample i of the WAVE */
static void wave_sample(long i, int *l, int *r)
{
    *l = (int)(i * 37 % 20001 - 10000);
    *r = (int)(i * 91 % 16001 - 8000);
}

static double rms(int ch, int n)
{
    double s = 0;
    int i;

    for (i = 0; i < n; i++)
        s += (double)out[2 * i + ch] * out[2 * i + ch];
    return sqrt(s / n);
}

int main(int argc, char **argv)
{
    static const uint8_t swap_in[4] = { 1, 0, 2, 3 }, swap_vol[4] = { 255, 128, 0, 0 };
    static const uint8_t std_in[4] = { 0, 1, 2, 3 }, std_vol[4] = { 255, 255, 0, 0 };
    char err[256];
    double l, r;
    int i;

    char copy[SYS_PATH];
    const char *cue = argc >= 2 ? argv[1] : NULL;

    if (argc == 3) {
        const char *base = strrchr(cue, '/');

        if (cd_copy_disc(cue, argv[2], NULL, NULL, err, sizeof err) != 0) {
            printf("copy: %s\n", err);
            return 1;
        }
        sys_join(copy, sizeof copy, argv[2], base ? base + 1 : cue);
        cue = copy;
    }
    if (argc < 2 || argc > 3 || cda_open(cue, err, sizeof err) != 0) {
        printf("cue: %s\n", cue ? err : "no sheet given");
        return 1;
    }
    /* the table the runner prints for the same sheet (selftest's CUE_TABLE) */
    if (cda_tracks() != 3 || cda_track_start(0) != 150 || cda_track_start(1) != 450 ||
        cda_track_start(2) != 750 || cda_leadout() != 825 || !cda_track_data(0) ||
        cda_track_data(1) || cda_track_data(2)) {
        printf("table: %d tracks at %u %u %u, lead-out %u\n", cda_tracks(), cda_track_start(0),
               cda_track_start(1), cda_track_start(2), cda_leadout());
        return 1;
    }

    /* the WAVE from its 10th frame, as it is */
    cda_play(450 + 10, 75);
    memset(out, 0, sizeof out);
    cda_mix(out, 4410, 44100);
    for (i = 0; i < 4410; i++) {
        int wl, wr;

        wave_sample(10L * 588 + i, &wl, &wr);
        if (out[2 * i] != wl || out[2 * i + 1] != wr) {
            printf("wave: sample %d is %d %d, not %d %d\n", i, out[2 * i], out[2 * i + 1], wl, wr);
            return 1;
        }
    }

    /* the Ogg: half and quarter scale sines, RMS 0.354 and 0.177 of full */
    cda_play(750, 75);
    memset(out, 0, sizeof out);
    cda_mix(out, 44100, 44100);
    l = rms(0, 44100) / 32768;
    r = rms(1, 44100) / 32768;
    if (fabs(l - 0.3536) > 0.02 || fabs(r - 0.1768) > 0.02) {
        printf("ogg: RMS %.4f %.4f, not 0.3536 0.1768\n", l, r);
        return 1;
    }
    /* swapped, the left at full, the right at half */
    cda_channels(swap_in, swap_vol);
    cda_play(750, 75);
    memset(out, 0, sizeof out);
    cda_mix(out, 44100, 44100);
    l = rms(0, 44100) / 32768;
    r = rms(1, 44100) / 32768;
    cda_channels(std_in, std_vol);
    if (fabs(l - 0.1768) > 0.02 || fabs(r - 0.1768) > 0.02) {
        printf("channels: RMS %.4f %.4f, not 0.1768 0.1768\n", l, r);
        return 1;
    }

    /* the clock: one second on, 75 frames; stopped and resumed; the end */
    cda_play(450, 150);
    plat_sleep_ms(1000);
    if (!cda_playing() || cda_position() != 525) {
        printf("clock: at %u after 1 s, not 525\n", cda_position());
        return 1;
    }
    cda_stop();
    plat_sleep_ms(1000);
    cda_resume();
    if (cda_position() != 525) {
        printf("clock: at %u after a stop, not 525\n", cda_position());
        return 1;
    }
    plat_sleep_ms(1000);
    if (cda_playing() || cda_position() != 600 || cda_play_end() != 600) {
        printf("clock: at %u at the end, not 600 and stopped\n", cda_position());
        return 1;
    }
    printf("cdaudio ok\n");
    return 0;
}
