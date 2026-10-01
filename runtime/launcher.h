/* launcher.h - a port's setup screen before the game, in the style of a
 * DOS setup program (textmode.h): pages of settings the player steps
 * through with the arrow keys or a controller, the choices kept in a
 * settings file of `name = value` lines.
 *
 * docs/LAUNCHER.md is its design contract.  The kit owns the complete
 * presentation and common dialogs; a port supplies LauncherApp,
 * LauncherPage and LauncherItem content through this interface.  It
 * does not copy the implementation or draw, theme or rearrange the
 * launcher itself.
 *
 * The port describes its pages; the launcher draws them, changes the
 * values the items point to and returns when the player picks an action
 * (start the game, start a table, quit) or closes the window.
 *
 * Keys: Up/Down an item, Left/Right a choice's value (or Enter, which
 * steps it on), Tab / Page Up / Page Down the pages (a controller's
 * shoulder buttons), Enter on a key item waits for the key to give it
 * (Esc keeps the old one, Backspace none), Esc quits (LAUNCHER_QUIT).
 * The controller: pad_menu_keys and the shoulders for the pages.
 *
 * The dialog about the game's files (launcher_offer_copy and the others
 * below) is the same in every port: the setup screen's backdrop, a window
 * "The game's files" saying where the player's GOG release is and where
 * its files would be copied, the choices below (Up/Down, Enter; Esc the
 * last one), and a bar while copying.  A port gives its names and says
 * what is copied; the texts are the kit's. */
#ifndef DK_LAUNCHER_H
#define DK_LAUNCHER_H

#include <stdint.h>

enum {
    LI_HEAD,        /* a heading line, not selectable */
    LI_CHOICE,      /* *value an index into values */
    LI_KEY,         /* *value a make code (E0 keys + 80h, as pad.h's), 0 none */
    LI_ACTION       /* Enter returns action */
};

typedef struct {
    int kind;
    const char *label;
    const char *name;               /* in the settings file; NULL: not kept */
    const char *const *values;      /* LI_CHOICE: the values' texts, NULL-ended */
    int *value;                     /* LI_CHOICE, LI_KEY */
    int action;                     /* LI_ACTION */
    const char *help;               /* the line at the bottom; NULL: none */
} LauncherItem;

typedef struct {
    const char *title;
    LauncherItem *items;
    int count;
} LauncherPage;

#define LAUNCHER_QUIT (-1)          /* Esc, or the window closed */

/* the names in the title bar, of the setup screen and of the dialog about
 * the game's files alike: "My Game" at the left, "mygame 1.2" at
 * the right */
typedef struct {
    const char *game;               /* the game's name: "My Game" */
    const char *port;               /* the program's name: "mygame" */
    const char *version;            /* the release's version; NULL or "": none */
} LauncherApp;

/* The screen until an action is picked: its `action`, or LAUNCHER_QUIT.
 * `app` names the title bar, `footer` (may be NULL) is the line above the
 * bottom bar.  `changed` (may be NULL) is called after a value changed
 * (for a setting that shows at once: full screen, the volume). */
int launcher_run(const LauncherApp *app, const char *footer, LauncherPage *pages, int npages,
                 void (*changed)(const LauncherItem *item));

/* The values of the items with a name from the file `path` (a choice by
 * its text or its index, a key by its make code in hex); lines that are
 * not understood, or out of range, are passed over.  0, or -1 when the
 * file cannot be read (the values stay). */
int launcher_load(const char *path, LauncherPage *pages, int npages);

/* the named items' values into `path` (choices by index, keys in hex),
 * after `comment` lines starting "# " (may be NULL); 0, or -1 */
int launcher_save(const char *path, const char *comment, LauncherPage *pages, int npages);

/* a key's name ("Left Shift", "Keypad Enter") for a make code as above;
 * "none" for 0, "key XXh" for one without a name */
const char *launcher_key_name(int code);

/* ---- the dialog about the game's files */

/* what is copied from the GOG release */
enum {
    LAUNCHER_GAME,                  /* the game's files */
    LAUNCHER_GAME_AND_CD,           /* and the CD's image and music */
    LAUNCHER_CD                     /* the CD's image and music only (the game's files are there) */
};

/* The copy of `what` from the release at `from` into `to` offered: "Copy
 * the files" or "Quit" (LAUNCHER_CD, without which the game runs: "Not
 * now").  1 to copy, 0 not (or the window was closed). */
int launcher_offer_copy(const LauncherApp *app, int what, const char *from, const char *to);

/* Neither the game's files nor a GOG release were found: said, until
 * "Quit".  `how` ends the sentence "Install the game from GOG and start
 * mygame again, or start it with " (the port's arguments; NULL: -gog and
 * -game as the template has them). */
void launcher_no_game(const LauncherApp *app, const char *how);

/* the copy from `from` failed because of `why` (the copying function's
 * err): said, until "Quit" */
void launcher_copy_failed(const LauncherApp *app, const char *from, const char *why);

/* the bar while copying: launcher_copy_progress is the `progress` of
 * cd_unpack, gog_copy, cd_copy_disc and inno_unpack, its ctx a
 * LauncherCopy with app and what set and the rest 0.  `closed` is 1
 * afterwards when the player closed the window (the copy was stopped). */
typedef struct {
    const LauncherApp *app;
    int what;                       /* LAUNCHER_GAME or LAUNCHER_CD */
    uint64_t drawn;
    int closed;
} LauncherCopy;

int launcher_copy_progress(void *ctx, const char *file, long done, long total);

#endif
