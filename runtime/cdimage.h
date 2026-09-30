/* cdimage.h - a DOS game's files from the player's CD image.
 *
 * GOG ships a CD game's data track as an image (game.gog, with a cue sheet
 * beside it): raw 2352-byte sectors (Mode 1, or Mode 2 Form 1: 2048 bytes
 * of data each) with an ISO 9660 file system.  A plain ISO (2048-byte
 * sectors) is read too.  The files are unpacked into a folder as
 * tools/isox.py does (as they are, names without ";1"); the CD audio
 * tracks beside the image are not touched. */
#ifndef DK_CDIMAGE_H
#define DK_CDIMAGE_H

#include <stddef.h>

/* where a GOG release is installed */
typedef struct {
    const char *folder;         /* GOG's folder name: "My Game" */
    const char *image;          /* the image's path in the folder; NULL or "": "game.gog" */
    /* on a Mac, the image's path inside /Applications (the release is an
     * app, often with a DOSBox bundle inside); NULL or "": no Mac release */
    const char *mac_bundle;
    /* GOG's product ID, the number in the goggame-ID.info in the installed
     * folder: on Windows the registry key GOG.com\Games\ID says where the
     * game is; NULL or "": the registry is not looked at */
    const char *gog_id;
    /* a path on the CD, as cd_unpack's must_have: an image without it is
     * not taken (GOG ships many DOS games' CDs as game.gog); NULL: any */
    const char *must_have;
} GogRelease;

/* The installed release's image: beside the program, in the current
 * directory, where GOG's installers put it (on Windows also where GOG's
 * registry entry for the game says it is), on a Mac inside the app.
 * 1 if found. */
int gog_find(const GogRelease *rel, char *out, size_t n);

/* The files of `image` unpacked into `dir`, which must not exist: into
 * `dir`.part first, renamed when complete.  With `must_have` (a path on
 * the CD, "GAME/GAME.EXE", case ignored; NULL or "": none) an image without that file is
 * refused before anything is written.  `progress` (may be NULL) is
 * called after each file with the bytes written so far and in all; a
 * nonzero return stops the unpacking.  0 on success, else -1 with the
 * reason in err. */
int cd_unpack(const char *image, const char *dir, const char *must_have,
              int (*progress)(void *ctx, const char *file, long done, long total), void *ctx,
              char *err, size_t n);

#endif
