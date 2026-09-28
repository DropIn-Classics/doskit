/* fli.h - Autodesk FLI animations (320x200, 256 colours): a decoder, and
 * a player that shows them in the window.
 *
 * The frame count in the header does not include the ring frame (the
 * one back to the first picture, for looping), so fli_next stops before
 * it.  The header's speed is in 1/70 s for FLI; a game's own player may
 * keep another clock (measure it in dosrun, or read it from the player's
 * code) and the port passes that as frame_seconds. */
#ifndef DK_FLI_H
#define DK_FLI_H

#include <stddef.h>
#include <stdint.h>

#define FLI_W 320
#define FLI_H 200

typedef struct {
    const uint8_t *data;
    size_t size, pos;                   /* pos: the next frame, 0 at the end */
    unsigned frames, speed, shown;      /* from the header; frames decoded */
    uint8_t pixels[FLI_W * FLI_H];
    uint8_t dac[768];                   /* six bits a component */
} Fli;

/* the animation in data (kept by the caller); 0, or -1 if it is not a
 * 320x200 FLI */
int fli_open(Fli *f, const uint8_t *data, size_t size);
/* the next frame into pixels and dac; 0 when there is none */
int fli_next(Fli *f);
/* a DAC's colours times level/32 (32: as they are) as 0x00RRGGBB */
void fli_palette(const uint8_t dac[768], int level, uint32_t out[256]);

enum { FLI_CLOSED, FLI_KEY, FLI_END };

/* the picture shown until `due` (plat_micros); with `keys`, a key (a make
 * code that would reach the BIOS's buffer: not a shift, Ctrl, Alt or lock
 * key) ends it: FLI_CLOSED, FLI_KEY or FLI_END */
int fli_show_until(const uint8_t *pixels, const uint32_t palette[256], uint64_t due, int keys);

/* the file at `path` played, `frame_seconds` apart (0: the header's
 * speed in 1/70 s); with `keys` a key ends it, and the keys typed are
 * dropped at the end.  The last frame (pixels and palette) into *last
 * when not NULL.  A file that cannot be read or is not an FLI plays as
 * nothing (FLI_END). */
int fli_play_file(const char *path, double frame_seconds, int keys, Fli *last);

/* the last picture faded to black in `steps` steps `step_seconds` apart;
 * 0 when the window was closed */
int fli_fade_out(const Fli *last, int steps, double step_seconds);

#endif
