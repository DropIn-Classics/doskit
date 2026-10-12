/* bioskey.c - the BIOS's keyboard (bioskey.h) */
#include "bioskey.h"

#define ROOM 16                         /* the BIOS's ring of 16 words holds 15 */

enum { RIGHT_SHIFT = 1, LEFT_SHIFT = 2, CTRL = 4, ALT = 8 };

/* the character of each make code, without and with Shift (US layout) */
static const unsigned char plain[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\r', 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0, '5', 0, '+', 0,
    0, 0, 0, 0, 0, 0, '\\', 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
static const unsigned char shifted[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\r', 0, 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0, '5', 0, '+', 0,
    0, 0, 0, 0, 0, 0, '|', 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static unsigned short kept[ROOM];
static int head, tail, shifts;

static void hold(int state, int down)
{
    shifts = down ? (shifts | state) : (shifts & ~state);
}

void bioskey_put(unsigned char scancode)
{
    int code = scancode & 0x7F, down = !(scancode & 0x80), next;

    if (scancode == 0xE0)
        return;
    switch (code) {
    case 0x2A: hold(LEFT_SHIFT, down); return;
    case 0x36: hold(RIGHT_SHIFT, down); return;
    case 0x1D: hold(CTRL, down); return;
    case 0x38: hold(ALT, down); return;
    }
    next = (tail + 1) % ROOM;
    if (!down || next == head)
        return;
    kept[tail] = (unsigned short)(code << 8
                                  | ((shifts & (LEFT_SHIFT | RIGHT_SHIFT)) ? shifted : plain)[code]);
    tail = next;
}

int bioskey_peek(void)
{
    return head == tail ? -1 : kept[head];
}

int bioskey_read(void)
{
    int key = bioskey_peek();

    if (key >= 0)
        head = (head + 1) % ROOM;
    return key;
}

int bioskey_shifts(void)
{
    return shifts;
}

void bioskey_clear(void)
{
    head = tail = shifts = 0;
}
