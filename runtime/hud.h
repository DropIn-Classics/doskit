/* hud.h - a short box at the top of the picture that says what a key
 * just set: a word ("VOLUME", "MUTE") and, if wanted, a bar of steps.
 * For frame.h's hud: the port keeps the values and calls hud_show when
 * one changed, and hands hud_draw to frame_set_hud.
 *
 * The box is drawn into the picture after vga_render, in the palette's
 * entries nearest to white and black; nothing reaches the program's
 * memory.  The letters are a 5x7 font of A-Z, 0-9 and space; others are
 * left out. */
#ifndef DK_HUD_H
#define DK_HUD_H

#include "vga.h"

/* `label` (kept by pointer; a string literal or static) for `pictures`
 * pictures, with a bar of `steps` steps of which `value` are full, or
 * no bar when `steps` is 0 */
void hud_show(const char *label, int value, int steps, int pictures);

/* the box over the picture while it is shown; counts the pictures down */
void hud_draw(VgaFrame *picture);

#endif
