/* launcher.h - a port's setup screen before the game, in the style of a
 * DOS setup program (textmode.h): pages of settings the player steps
 * through with the arrow keys or a controller, the choices kept in a
 * settings file of `name = value` lines.
 *
 * The port describes its pages; the launcher draws them, changes the
 * values the items point to and returns when the player picks an action
 * (start the game, start a table, quit) or closes the window.
 *
 * Keys: Up/Down an item, Left/Right a choice's value (or Enter, which
 * steps it on), Tab / Page Up / Page Down the pages (a controller's
 * shoulder buttons), Enter on a key item waits for the key to give it
 * (Esc keeps the old one, Backspace none), Esc quits (LAUNCHER_QUIT).
 * The controller: pad_menu_keys and the shoulders for the pages. */
#ifndef DK_LAUNCHER_H
#define DK_LAUNCHER_H

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

/* The screen until an action is picked: its `action`, or LAUNCHER_QUIT.
 * `title` heads the screen, `footer` (may be NULL) is the line below it.
 * `changed` (may be NULL) is called after a value changed (for a setting
 * that shows at once: full screen, the volume). */
int launcher_run(const char *title, const char *footer, LauncherPage *pages, int npages,
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

#endif
