/* bioskey.h - the BIOS's keyboard for a program that read its keys
 * through INT 16h.
 *
 * The BIOS's INT 9 turned each byte of port 60h into a word, the scan
 * code above the character, and kept up to fifteen of them until the
 * program asked.  bioskey_put() is that handler (hand it to
 * frame_set_keyboard), bioskey_peek() and bioskey_read() are INT 16h's
 * AH=1 and AH=0.  The characters are those of the US layout, as the
 * runner's BIOS gives them (tools/run/bios.c), so a key script means
 * the same in both: a key with Shift held gives its upper character,
 * Ctrl and Alt are kept as shift states but change no character, a key
 * without a character (the arrows, the function keys) gives 0 below its
 * scan code, and the E0 byte before an extended key is left out. */
#ifndef DK_BIOSKEY_H
#define DK_BIOSKEY_H

/* a byte of port 60h: a make code is translated and kept, when there is
 * room; break codes and the shift keys change the shift states only */
void bioskey_put(unsigned char scancode);

/* the oldest word kept, scan code * 256 + character, or -1 for none;
 * bioskey_read() takes it away (INT 16h AH=0 waited for a key instead:
 * the program's loop does that here, around frame_wait) */
int bioskey_peek(void);
int bioskey_read(void);

/* the shift states as INT 16h AH=2 gave them: 1 right Shift, 2 left
 * Shift, 4 Ctrl, 8 Alt */
int bioskey_shifts(void);

/* every key kept thrown away, the shift states cleared */
void bioskey_clear(void);

#endif
