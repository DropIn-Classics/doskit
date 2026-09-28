/* pad.c - see pad.h */
#include <string.h>
#include "pad.h"

const PadKeys pad_menu_keys = {
    [PAD_UP] = {0xC8}, [PAD_DOWN] = {0xD0}, [PAD_LEFT] = {0xCB}, [PAD_RIGHT] = {0xCD},
    [PAD_A] = {0x1C}, [PAD_START] = {0x1C}, [PAD_B] = {0x01}, [PAD_BACK] = {0x01},
};

static const PadKeys *table = &pad_menu_keys;
static int presses[PAD_BUTTONS];        /* controllers holding the button */
static int pressed[PAD_BUTTONS][2];     /* the keys it put down, 0: none */
static int key_holds[256];              /* buttons holding the key */

static const char *const names[PAD_BUTTONS] = {
    "a", "b", "x", "y", "back", "start", "leftstick", "rightstick",
    "leftshoulder", "rightshoulder", "lefttrigger", "righttrigger",
    "up", "down", "left", "right",
};

const PadKeys *pad_set_keys(const PadKeys *keys)
{
    const PadKeys *was = table;
    table = keys;
    return was;
}

int pad_held(int button)
{
    return button >= 0 && button < PAD_BUTTONS && presses[button] > 0;
}

void pad_button(int button, int down, void (*key)(int code, int up))
{
    int i;

    if (button < 0 || button >= PAD_BUTTONS)
        return;
    if (down) {
        if (presses[button]++)
            return;                     /* another controller holds it already */
        for (i = 0; i < 2; i++) {
            int c = table ? (*table)[button][i] : 0;
            pressed[button][i] = c;
            if (c && key_holds[c]++ == 0)
                key(c, 0);
        }
        return;
    }
    if (presses[button] == 0 || --presses[button])
        return;
    for (i = 0; i < 2; i++) {
        int c = pressed[button][i];
        if (c && --key_holds[c] == 0)
            key(c, 1);
        pressed[button][i] = 0;
    }
}

const char *pad_button_name(int button)
{
    return button >= 0 && button < PAD_BUTTONS ? names[button] : "";
}

int pad_button_by_name(const char *name)
{
    int b;
    for (b = 0; b < PAD_BUTTONS; b++)
        if (!strcmp(names[b], name))
            return b;
    return -1;
}
