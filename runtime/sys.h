/* sys.h - files and directories, on Windows and POSIX (macOS, Linux) */
#ifndef DK_SYS_H
#define DK_SYS_H

#include <stddef.h>
#include <stdint.h>

#define SYS_PATH 1024

/* the game's name for the folder of sys_data_dir: `name` on a Mac ("My
 * Game"), `unix_name` on Linux ("my-game"); "doskit" until set */
void sys_set_app(const char *name, const char *unix_name);

/* the directory the program runs from, without a trailing separator */
void sys_exe_dir(char *out, size_t n);

/* where settings, saves and imported game files go, made if missing:
 * $DK_DATA_DIR if set, else the user's data folder, named by sys_set_app:
 * %LOCALAPPDATA%\NAME on Windows, ~/Library/Application Support/NAME on a
 * Mac, $XDG_DATA_HOME/UNIX_NAME or ~/.local/share/UNIX_NAME elsewhere.
 * Never the program's folder, so that a newer release can take its place. */
void sys_data_dir(char *out, size_t n);

/* What an earlier version wrote beside the program moved into
 * sys_data_dir: each of `names` (files or folders; NULL ends the list)
 * that is beside the program and not yet in the data folder.  Renamed
 * where that works, else copied (another drive; the old one left).  The
 * number moved. */
int sys_data_migrate(const char *const *names);

/* dir + separator + name; name alone if dir is empty; out may be dir */
void sys_join(char *out, size_t n, const char *dir, const char *name);

/* the folder above path (out may be path); 0 if there is none.  On
 * Windows the drive's root has the empty path above it, the drive list. */
int sys_parent(const char *path, char *out, size_t n);

/* calls fn for each entry of dir but . and .. (is_dir for folders); on
 * Windows the empty path lists the drives, "C:\" and so on.  0, or -1 if
 * dir cannot be read. */
int sys_list_dir(const char *dir, void (*fn)(void *ctx, const char *name, int is_dir), void *ctx);

/* the user's home folder (on Windows the profile folder) */
void sys_home_dir(char *out, size_t n);

int sys_is_dir(const char *path);
int sys_is_file(const char *path);
int sys_mkdir(const char *path);        /* 0 on success */
int sys_rmdir(const char *path);
int sys_rename(const char *from, const char *to);

/* The path of `name` in `dir`, ignoring case (a DOS game's file names are
 * upper case on the disc, often mixed case in its own strings).  1 if
 * found. */
int sys_find(const char *dir, const char *name, char *out, size_t n);

/* a whole file in a malloc'ed buffer, NULL if it cannot be read */
uint8_t *sys_load(const char *path, size_t *size);

int sys_stricmp(const char *a, const char *b);

/* 1 if the folder `dir` holds `marker`, a folder or file below it
 * ("GAME", "GAME/GAME.EXE"; either separator), case ignored */
int sys_has_marker(const char *dir, const char *marker);

/* The game's unpacked files: `given` (a -game option; NULL: none), else
 * $`env` (NULL: none), else the first folder `game` holding the folder or
 * file `marker` (as on the CD: "GAME", "GAME/GAME.EXE" also works) beside
 * the program, in the current directory or in sys_data_dir.  1 if found
 * (given and $env need only be folders). */
int sys_find_game(const char *given, const char *env, const char *marker, char *out, size_t n);

/* 1 on a Steam Deck (plat_sdl.c starts fullscreen there): Steam says so in
 * SteamDeck=1, else the firmware names Valve's Jupiter (LCD) or Galileo
 * (OLED), under SteamOS as under Windows */
int sys_steam_deck(void);

#endif
