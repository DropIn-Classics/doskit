/* shot.h - screenshots as PNG files.
 *
 * Each platform's plat_present hands the picture to shot_keep, so the
 * picture last shown can be written at any time, whoever showed it
 * (frame.c, fli.c, a port's own screen).  The Print Screen key writes
 * it into the next free NAME_NNNN.png (plat_sdl.c, plat_win32.c); the
 * headless platform writes it at the pictures DK_SHOTS names
 * (plat_null.c).  The PNG is indexed (8 bits, the picture's palette) and
 * compressed with deflate's fixed codes; no library is needed. */
#ifndef DK_SHOT_H
#define DK_SHOT_H

#include <stdint.h>

/* writes width x height palette indexes with a 0x00RRGGBB palette to
 * `path` as a PNG; 0 when it could not be written */
int shot_png(const char *path, const uint8_t *pixels, int width, int height,
             const uint32_t palette[256]);

/* a copy of the picture shown (called by plat_present) */
void shot_keep(const uint8_t *pixels, int width, int height, const uint32_t palette[256]);

/* where the Print Screen key writes: `folder` (NULL or "": the current
 * one) and file names `name`_0000.png on (NULL: "screenshot") */
void shot_set_name(const char *folder, const char *name);

/* writes the picture last shown to `path`, or with path NULL into the
 * next NAME_NNNN.png not there yet; the file written goes to `written`
 * when that is not NULL (at most `size` bytes).  0 when there was no
 * picture yet or it could not be written. */
int shot_save(const char *path, char *written, int size);

#endif
