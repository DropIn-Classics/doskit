/* frame.h - one picture after the other.
 *
 * A DOS game waits for its next frame in its own way: polls the VGA's
 * retrace, or a flag its timer interrupt (or a sound driver's tick
 * callback) sets.  frame_wait() stands for that wait: it keeps the
 * refresh rate of the video mode (vga_refresh_hz), hands the keys to the
 * program's keyboard handler (its INT 9, byte by byte as port 60h gave
 * them), runs the tick callback and shows the picture vga.c scans out.
 * A hud (the port's own volume display, say) draws over the picture and
 * gets the control keys (platform.h, plat_read_control). */
#ifndef DK_FRAME_H
#define DK_FRAME_H

#include "vga.h"

typedef void (*FrameCallback)(void);
typedef void (*KeyHandler)(unsigned char scancode);

/* the routine run once a picture, before it is drawn (the program's timer
 * interrupt or sound driver callback); NULL for none */
void frame_set_tick(FrameCallback tick);

/* the program's INT 9 handler, which gets each byte of port 60h */
void frame_set_keyboard(KeyHandler handler);

/* drawn into each picture after vga_render, before it is shown (what
 * video memory does not hold: a mouse driver's pointer); NULL for none */
void frame_set_overlay(void (*draw)(VgaFrame *picture));

/* the port's own display over each picture (after the overlay) and the
 * handler of the control keys; with a handler the keypad's + - * / are
 * kept from the program.  NULL, NULL: none */
void frame_set_hud(void (*draw)(VgaFrame *picture), void (*control)(int control));

/* 1: each picture is scanned out at the end of its frame (at the next
 * wait, before the tick), from the start address the card took at the
 * frame's retrace (vga_set_start_latch).  For a program that draws into
 * the page on show after the retrace, ahead of the beam, and writes the
 * next start address during the picture.  0 (the default): scanned out
 * at the tick, with the registers as they are then. */
void frame_set_scanout_end(int on);

/* waits for the next picture; 0 once the window was closed */
int frame_wait(void);
/* For a loop that waits on the keyboard alone (a wait for keys let go):
 * the next picture's keys handed over, and when there were any, back to
 * the program before the tick, which the next wait runs without waiting
 * (under DOS the key's interrupt came before the tick, and such a loop
 * went on at once).  0 once the window was closed. */
int frame_wait_keys(void);

/* each keyboard byte handed to the program is written to the file `path`
 * as PICTURE:HEX (the picture count when it was handed over) */
void frame_record(const char *path);

/* pictures shown so far */
unsigned long frame_count(void);

#endif
