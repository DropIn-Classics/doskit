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
 * directory, in the data folder (sys_data_dir), where GOG's installers
 * put it (on Windows also where GOG's registry entry for the game says
 * it is; on Linux also where the Linux installer's menu entry or
 * Heroic's list of installed games says it is, and in Wine prefixes),
 * in the install folder or its folder data, on a Mac inside the app
 * (mac_bundle); the image's name in any case.  1 if found. */
int gog_find(const GogRelease *rel, char *out, size_t n);

/* A release installed as a folder (from floppies, a DOSBox folder: no CD
 * image): the folder, looked for where gog_find looks (on Windows GOG's
 * registry entry and default folders, on a Mac the application
 * /Applications/FOLDER.app), holding `must_have` (a path in the folder;
 * without one nothing is found).  1 if found. */
int gog_find_folder(const GogRelease *rel, char *out, size_t n);

/* The installed `folder` copied into `dir` as it is, as cd_unpack does:
 * into `dir`.part, renamed when complete, `must_have` checked first,
 * `progress` after each file.  0 on success, else -1 with the reason in
 * err. */
int gog_copy(const char *folder, const char *dir, const char *must_have,
             int (*progress)(void *ctx, const char *file, long done, long total), void *ctx,
             char *err, size_t n);

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

/* The disc a cue sheet describes copied into `dir`, which must not exist:
 * the sheet under its own name and every file its FILE lines name (the
 * data track's image and the audio tracks, "MUSIC\Track02.ogg" into the
 * folder MUSIC; the names found whatever their case), so that cda_open
 * reads the copy as it reads the original.  Into `dir`.part first,
 * renamed when complete; `progress` as for gog_copy.  0 on success, else
 * -1 with the reason in err. */
int cd_copy_disc(const char *cue, const char *dir,
                 int (*progress)(void *ctx, const char *file, long done, long total), void *ctx,
                 char *err, size_t n);

#endif
