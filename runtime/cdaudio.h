/* cdaudio.h - a CD's audio tracks from a cue sheet, for a port.
 *
 * GOG ships a CD game's audio as files beside the data track's image and
 * names them in a cue sheet (FILE "MUSIC\Track02.ogg" MP3, TRACK 02
 * AUDIO, ...).  cda_open reads the sheet as tools/run's -cue does: each
 * FILE's tracks at their INDEX 01 inside it, the files one after the
 * other from 00:02:00, PREGAP adding frames; a file's length from its
 * contents (Ogg Vorbis, WAVE, raw sectors).  The table of contents and
 * the plays a program asks for (MSCDEX's play, stop, resume, audio
 * status, channel control) are answered from it; cda_mix adds what the
 * drive plays to the port's audio.  Addresses are frames, 75 a second.
 *
 * The play's position runs on plat_micros (as a drive does, whether or
 * not anything is heard); the samples mixed follow their own count from
 * the play's start.  Not thread-safe: the audio thread calls cda_mix, so
 * the caller holds plat_audio_lock() around every call. */
#ifndef DK_CDAUDIO_H
#define DK_CDAUDIO_H

#include <stddef.h>
#include <stdint.h>

#define CDA_TRACKS_MAX 99

/* 0, or -1 with the reason in err; on failure no disc */
int cda_open(const char *cue, char *err, size_t n);

/* the disc read: its tracks (0 with none), track i's (0-based) first
 * frame, 1 for a data track, and the lead-out's frame */
int cda_tracks(void);
uint32_t cda_track_start(int i);
int cda_track_data(int i);
uint32_t cda_leadout(void);

/* plays `n` frames from frame `start` (0: stops); stop keeps the
 * position for resume */
void cda_play(uint32_t start, uint32_t n);
void cda_stop(void);
void cda_resume(void);

/* 1 while a play runs (not stopped, not at its end); the position now;
 * the play's end */
int cda_playing(void);
uint32_t cda_position(void);
uint32_t cda_play_end(void);

/* MSCDEX's audio channel control: output i (0 left, 1 right) plays input
 * in[i] at volume vol[i] (0..255); inputs 2 and 3 are silent */
void cda_channels(const uint8_t in[4], const uint8_t vol[4]);

/* adds `frames` stereo frames at `rate` Hz of what plays to out
 * (saturated) */
void cda_mix(int16_t *out, int frames, int rate);

#endif
