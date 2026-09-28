/* pad.h - game controllers: their buttons become keys.
 *
 * The platform reports the buttons of every controller it knows (plugged
 * in before the start or while the game runs) as pad_button calls; each
 * becomes the scan codes of keys, pushed into the same queue as the
 * keyboard's, so the program reads them as keys (and a recording of the
 * keys has them like typed ones).  What a button stands for is a table
 * the game sets and changes with the situation (pad_set_keys): the arrow
 * keys and Enter/Esc in its menus, its own controls in play, the player's
 * choices from a settings file.  Make codes are scan code set 1, the E0
 * keys + 80h (up = C8h). */
#ifndef DK_PAD_H
#define DK_PAD_H

/* the buttons, named by where they sit (the Xbox names) */
enum {
    PAD_A, PAD_B, PAD_X, PAD_Y, PAD_BACK, PAD_START,
    PAD_LSTICK, PAD_RSTICK, PAD_LB, PAD_RB, PAD_LT, PAD_RT,
    PAD_UP, PAD_DOWN, PAD_LEFT, PAD_RIGHT,
    PAD_BUTTONS
};

/* up to two keys (make codes; 0: none) per button */
typedef unsigned char PadKeys[PAD_BUTTONS][2];

/* for menus and text screens: the D-pad (and the left stick) the arrow
 * keys, A and Start Enter, B and Back Esc */
extern const PadKeys pad_menu_keys;

/* the table the buttons follow from now on (the caller keeps it; NULL:
 * the buttons do nothing); the one before */
const PadKeys *pad_set_keys(const PadKeys *keys);

/* A button pressed (down 1) or let go on some controller; `key` is called
 * with a make code and up 0 or 1 for each key that goes down or up by it.
 * A key two buttons hold goes up with the last; a button lets go of the
 * keys it put down even when the table changed meanwhile. */
void pad_button(int button, int down, void (*key)(int code, int up));

/* 1 while some controller holds the button */
int pad_held(int button);

/* the button's name for a settings file: a lower case word ("a",
 * "leftshoulder", "up"), as SDL's game controller API names it */
const char *pad_button_name(int button);
/* the button for such a name; -1 if none */
int pad_button_by_name(const char *name);

#endif
