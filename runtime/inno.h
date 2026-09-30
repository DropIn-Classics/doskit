/* inno.h - a DOS game's files from GOG's Windows installer.
 *
 * A game GOG sells for Windows only comes as an Inno Setup installer,
 * setup_NAME_VERSION_(ID).exe, with setup_..-1.bin, -2.bin beside it when
 * the game is large; on a Mac or Linux it is all the player has.  Its
 * files are unpacked without running it, as tools/inno.py does: what the
 * setup installs into the game's folder ({app}), GOG Galaxy's parts put
 * together (each part's and each file's checksum checked), one language
 * where there are several (English, else the first).  Inno Setup 5.2 to
 * 6.4, stored, LZMA, LZMA2 or zlib; encrypted and bzip2 setups are
 * refused. */
#ifndef DK_INNO_H
#define DK_INNO_H

#include <stddef.h>
#include "cdimage.h"

/* 1 if `path` is an Inno Setup installer read here */
int inno_is_setup(const char *path);

/* GOG's installer of the release lying about: a setup_*.exe beside the
 * program, in the current directory, in the data folder (sys_data_dir),
 * in the home folder or its Downloads, Desktop or Documents, which is the
 * release's (rel->gog_id its product ID, or rel->must_have among its
 * files, or rel->folder its name and a CD image in it).  Its path into
 * out; 1 if found. */
int inno_find(const GogRelease *rel, char *out, size_t n);

/* The files the installer `setup` puts into the game's folder unpacked
 * into `dir`, which must not exist, as cd_unpack does: into `dir`.part
 * first, renamed when complete.  With `must_have` (a file or folder in the
 * game's folder, "GAME/GAME.EXE", case ignored; NULL or "": none) a setup
 * without it is refused before anything is written, unless it holds a
 * CD image (GOG's game.gog, *.ins, *.iso ...): then that image is taken
 * out and unpacked by cd_unpack, `must_have` a path on the CD.
 * `progress` (may be NULL) is called after each file with the bytes
 * written so far and in all; a nonzero return stops the unpacking.  0 on
 * success, else -1 with the reason in err. */
int inno_unpack(const char *setup, const char *dir, const char *must_have,
                int (*progress)(void *ctx, const char *file, long done, long total), void *ctx,
                char *err, size_t n);

#endif
