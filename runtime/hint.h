/* hint.h - what a port says to the player beside the game: a card of
 * text over the picture and a frame around the place it talks about,
 * for a tutorial, a note before the game, a first visit's explanation.
 *
 * docs/HINTS.md is its design contract.  The card is the kit's, in the
 * setup screen's style (launcher.h): a window of double lines with its
 * title in the frame, drawn with textmode.h's font into platform.h's
 * layer, so in the window's own pixels and not the video mode's.  A port
 * supplies the words and the place; it does not draw a card of its own.
 * Nothing of it reaches the program's memory or its picture.
 *
 * Two ways to show one: hint_run holds the program until the player
 * answers (frame.h's frame_hold: no tick, no input for the program), the
 * rest of the picture dimmed; hint_show leaves a card and its frame over
 * the running game until it is taken away.  Either needs the layer:
 * plat_set_layer(hint_layer, NULL), once, after plat_init. */
#ifndef DK_HINT_H
#define DK_HINT_H

#include <stdint.h>

typedef struct {
    const char *title;      /* in the card's frame; NULL: none */
    const char *text;       /* wrapped to the card's width, '\n' a new line
                             * and "\n\n" a gap; UTF-8, of which the font
                             * has ASCII and the German letters (others
                             * show as '?'); NULL: none */
    const char *keys;       /* the card's last line, what the player can
                             * do now ("Enter: next   Esc: end the
                             * tutorial"); NULL: none */
    /* the place the card talks about: a frame around the rectangle
     * (x, y) w x h of a picture `across` x `down` (the game's own
     * coordinates: 320 and 200, say); w 0: no place */
    int x, y, w, h;
    int across, down;
} Hint;

/* what the player answered to hint_run */
enum {
    HINT_CLOSED,            /* the window was closed */
    HINT_NEXT,              /* Enter, Space or the left button */
    HINT_BACK,              /* Backspace or the Left key */
    HINT_END                /* Esc */
};

/* The card over the running game from the next picture on, until
 * hint_show(NULL).  The Hint and its strings are the caller's and stay
 * as they are while it is shown. */
void hint_show(const Hint *hint);

/* The card shown with the program held until the player answers: one of
 * the above.  A card shown by hint_show is back afterwards. */
int hint_run(const Hint *hint);

/* platform.h's layer: draws the card shown, 0 when there is none */
int hint_layer(uint32_t *pixels, int width, int height, void *user);

#endif
