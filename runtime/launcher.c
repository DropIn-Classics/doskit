/* launcher.c - see launcher.h */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "launcher.h"
#include "pad.h"
#include "platform.h"
#include "textmode.h"

#define BG TM_BLUE
#define A_TEXT TM_ATTR(TM_LIGHTGREY, BG)
#define A_HEAD TM_ATTR(TM_YELLOW, BG)
#define A_SEL TM_ATTR(TM_BLACK, TM_LIGHTGREY)
#define A_VALUE TM_ATTR(TM_WHITE, BG)
#define A_TAB TM_ATTR(TM_LIGHTGREY, TM_BLACK)
#define A_TAB_ON TM_ATTR(TM_BLACK, TM_CYAN)
#define A_BAR TM_ATTR(TM_BLACK, TM_CYAN)

#define WIN_X 4
#define WIN_Y 5
#define WIN_W 72
#define WIN_H 15
#define ROWS (WIN_H - 2)
#define VALUE_X 44

/* ---- key names (scan code set 1; E0 keys + 80h) */

static const char *const names[128] = {
    [0x01] = "Esc", [0x02] = "1", [0x03] = "2", [0x04] = "3", [0x05] = "4", [0x06] = "5",
    [0x07] = "6", [0x08] = "7", [0x09] = "8", [0x0A] = "9", [0x0B] = "0", [0x0C] = "-",
    [0x0D] = "=", [0x0E] = "Backspace", [0x0F] = "Tab", [0x10] = "Q", [0x11] = "W",
    [0x12] = "E", [0x13] = "R", [0x14] = "T", [0x15] = "Y", [0x16] = "U", [0x17] = "I",
    [0x18] = "O", [0x19] = "P", [0x1A] = "[", [0x1B] = "]", [0x1C] = "Enter",
    [0x1D] = "Left Ctrl", [0x1E] = "A", [0x1F] = "S", [0x20] = "D", [0x21] = "F",
    [0x22] = "G", [0x23] = "H", [0x24] = "J", [0x25] = "K", [0x26] = "L", [0x27] = ";",
    [0x28] = "'", [0x29] = "`", [0x2A] = "Left Shift", [0x2B] = "\\", [0x2C] = "Z",
    [0x2D] = "X", [0x2E] = "C", [0x2F] = "V", [0x30] = "B", [0x31] = "N", [0x32] = "M",
    [0x33] = ",", [0x34] = ".", [0x35] = "/", [0x36] = "Right Shift", [0x37] = "Keypad *",
    [0x38] = "Left Alt", [0x39] = "Space", [0x3A] = "Caps Lock", [0x3B] = "F1",
    [0x3C] = "F2", [0x3D] = "F3", [0x3E] = "F4", [0x3F] = "F5", [0x40] = "F6",
    [0x41] = "F7", [0x42] = "F8", [0x43] = "F9", [0x44] = "F10", [0x45] = "Num Lock",
    [0x46] = "Scroll Lock", [0x47] = "Keypad 7", [0x48] = "Keypad 8", [0x49] = "Keypad 9",
    [0x4A] = "Keypad -", [0x4B] = "Keypad 4", [0x4C] = "Keypad 5", [0x4D] = "Keypad 6",
    [0x4E] = "Keypad +", [0x4F] = "Keypad 1", [0x50] = "Keypad 2", [0x51] = "Keypad 3",
    [0x52] = "Keypad 0", [0x53] = "Keypad .", [0x56] = "<", [0x57] = "F11", [0x58] = "F12",
};

static const char *const e0_names[128] = {
    [0x1C] = "Keypad Enter", [0x1D] = "Right Ctrl", [0x35] = "Keypad /", [0x38] = "Right Alt",
    [0x47] = "Home", [0x48] = "Up", [0x49] = "Page Up", [0x4B] = "Left", [0x4D] = "Right",
    [0x4F] = "End", [0x50] = "Down", [0x51] = "Page Down", [0x52] = "Insert",
    [0x53] = "Delete", [0x5B] = "Left Win", [0x5C] = "Right Win", [0x5D] = "Menu",
};

const char *launcher_key_name(int code)
{
    static char buf[16];
    const char *s;

    if (code <= 0 || code > 0xFF)
        return "none";
    s = code & 0x80 ? e0_names[code & 0x7F] : names[code];
    if (s)
        return s;
    snprintf(buf, sizeof buf, "key %02Xh", code);
    return buf;
}

/* ---- the screen */

static uint8_t pixels[TM_WIDTH * TM_HEIGHT];
static uint32_t palette[256];

static int selectable(const LauncherItem *it)
{
    return it->kind != LI_HEAD;
}

static int count_values(const LauncherItem *it)
{
    int n = 0;

    while (it->values && it->values[n])
        n++;
    return n;
}

static void centre(int y, const char *s, uint8_t attr)
{
    tm_text((TM_COLS - (int)strlen(s)) / 2, y, s, attr);
}

static void draw(const char *title, const char *footer, LauncherPage *pages, int npages,
                 int page, int sel, int top, int waiting)
{
    const LauncherPage *pg = &pages[page];
    int i, x = 2;

    tm_clear(' ', A_TEXT);
    tm_fill(0, 0, TM_COLS, 1, ' ', A_BAR);
    centre(0, title, A_BAR);
    for (i = 0; i < npages; i++) {
        char t[40];

        snprintf(t, sizeof t, " %s ", pages[i].title);
        x = tm_text(x, 3, t, i == page ? A_TAB_ON : A_TAB) + 1;
    }
    tm_fill(WIN_X, WIN_Y, WIN_W, WIN_H, ' ', A_TEXT);
    tm_frame(WIN_X, WIN_Y, WIN_W, WIN_H, A_TEXT);
    tm_shadow(WIN_X, WIN_Y, WIN_W, WIN_H);
    for (i = 0; i < ROWS && top + i < pg->count; i++) {
        const LauncherItem *it = &pg->items[top + i];
        int y = WIN_Y + 1 + i, on = top + i == sel;
        uint8_t a = on ? A_SEL : it->kind == LI_HEAD ? A_HEAD : A_TEXT;

        if (on)
            tm_fill(WIN_X + 2, y, WIN_W - 4, 1, ' ', a);
        tm_text(WIN_X + 3, y, it->label, a);
        if (it->kind == LI_CHOICE && it->value) {
            int n = count_values(it), v = *it->value;

            tm_put(VALUE_X - 2, y, TM_LEFT_TRIANGLE, on ? a : A_VALUE);
            tm_text(VALUE_X, y, v >= 0 && v < n ? it->values[v] : "?", on ? a : A_VALUE);
            tm_put(WIN_X + WIN_W - 4, y, TM_RIGHT_TRIANGLE, on ? a : A_VALUE);
        } else if (it->kind == LI_KEY && it->value) {
            tm_text(VALUE_X, y, on && waiting ? "press a key..." : launcher_key_name(*it->value),
                    on ? a : A_VALUE);
        }
    }
    if (top > 0)
        tm_put(WIN_X + WIN_W - 2, WIN_Y + 1, TM_UP_ARROW, A_TEXT);
    if (top + ROWS < pg->count)
        tm_put(WIN_X + WIN_W - 2, WIN_Y + WIN_H - 2, TM_DOWN_ARROW, A_TEXT);
    if (sel >= 0 && sel < pg->count && pg->items[sel].help)
        tm_text(WIN_X, WIN_Y + WIN_H + 1, pg->items[sel].help, A_VALUE);
    tm_fill(0, TM_ROWS - 1, TM_COLS, 1, ' ', A_BAR);
    centre(TM_ROWS - 1, waiting ? "Press the key  -  Esc: keep  -  Backspace: none"
                                : "Up/Down: item   Left/Right: change   Tab: page   Enter: choose   Esc: quit",
           A_BAR);
    if (footer)
        centre(TM_ROWS - 2, footer, TM_ATTR(TM_DARKGREY, BG));
    tm_render(pixels, palette);
    plat_present(pixels, TM_WIDTH, TM_HEIGHT, palette);
}

/* the next key pressed (make code, E0 keys + 80h); 0 when the window was
 * closed */
static int next_key(void)
{
    static int e0;

    for (;;) {
        int b;

        while ((b = plat_read_scancode()) >= 0) {
            if (b == 0xE0) {
                e0 = 1;
                continue;
            }
            if (b & 0x80) {
                e0 = 0;
                continue;
            }
            b |= e0 ? 0x80 : 0;
            e0 = 0;
            if (b == 0xAA)          /* the fake shift of E0 sequences */
                continue;
            return b;
        }
        return -1;
    }
}

static int first_selectable(const LauncherPage *pg, int from, int step)
{
    int i;

    for (i = from; i >= 0 && i < pg->count; i += step)
        if (selectable(&pg->items[i]))
            return i;
    return -1;
}

int launcher_run(const char *title, const char *footer, LauncherPage *pages, int npages,
                 void (*changed)(const LauncherItem *item))
{
    static PadKeys keys;
    const PadKeys *was;
    int page = 0, sel, top = 0, waiting = 0, result = LAUNCHER_QUIT;

    memcpy(keys, pad_menu_keys, sizeof keys);
    keys[PAD_LB][0] = 0xC9;                 /* Page Up */
    keys[PAD_RB][0] = 0xD1;                 /* Page Down */
    was = pad_set_keys(&keys);
    sel = first_selectable(&pages[0], 0, 1);
    for (;;) {
        LauncherPage *pg = &pages[page];
        LauncherItem *it = sel >= 0 ? &pg->items[sel] : NULL;
        int k, turn = 0;

        if (sel >= 0 && sel < top)
            top = sel;
        if (sel >= top + ROWS)
            top = sel - ROWS + 1;
        if (sel == first_selectable(pg, 0, 1))
            top = 0;
        draw(title, footer, pages, npages, page, sel, top, waiting);
        if (!plat_pump())
            break;
        k = next_key();
        if (k < 0) {
            plat_sleep_ms(10);
            continue;
        }
        if (waiting) {
            waiting = 0;
            if (k == 0x01)
                continue;
            *it->value = k == 0x0E ? 0 : k;
            if (changed)
                changed(it);
            continue;
        }
        switch (k) {
        case 0x01:
            goto done;
        case 0xC8: {                        /* Up */
            int s = sel > 0 ? first_selectable(pg, sel - 1, -1) : -1;
            if (s >= 0)
                sel = s;
            else if (sel >= 0 && top > 0)
                top--;
            break;
        }
        case 0xD0: {                        /* Down */
            int s = first_selectable(pg, sel + 1, 1);
            if (s >= 0)
                sel = s;
            break;
        }
        case 0x0F: case 0xD1:               /* Tab, Page Down */
        case 0xC9:                          /* Page Up */
            page = (page + (k == 0xC9 ? npages - 1 : 1)) % npages;
            sel = first_selectable(&pages[page], 0, 1);
            top = 0;
            break;
        case 0xCB:                          /* Left */
            turn = -1;
            break;
        case 0xCD:                          /* Right */
            turn = 1;
            break;
        case 0x1C: case 0x9C:               /* Enter */
            if (!it)
                break;
            if (it->kind == LI_ACTION) {
                result = it->action;
                goto done;
            }
            if (it->kind == LI_KEY && it->value)
                waiting = 1;
            else
                turn = 1;
            break;
        }
        if (turn && it && it->kind == LI_CHOICE && it->value) {
            int n = count_values(it);

            if (n > 0) {
                *it->value = ((*it->value + turn) % n + n) % n;
                if (changed)
                    changed(it);
            }
        }
    }
done:
    pad_set_keys(was);
    return result;
}

/* ---- the settings file */

static LauncherItem *find_item(LauncherPage *pages, int npages, const char *name)
{
    int p, i;

    for (p = 0; p < npages; p++)
        for (i = 0; i < pages[p].count; i++) {
            LauncherItem *it = &pages[p].items[i];
            if (it->name && it->value && !strcmp(it->name, name))
                return it;
        }
    return NULL;
}

static char *trim(char *s)
{
    char *e;

    while (*s == ' ' || *s == '\t')
        s++;
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n'))
        *--e = 0;
    return s;
}

int launcher_load(const char *path, LauncherPage *pages, int npages)
{
    FILE *f = fopen(path, "r");
    char line[256];

    if (!f)
        return -1;
    while (fgets(line, sizeof line, f)) {
        char *eq = strchr(line, '='), *name, *val, *end;
        LauncherItem *it;
        long v;

        if (line[0] == '#' || !eq)
            continue;
        *eq = 0;
        name = trim(line);
        val = trim(eq + 1);
        it = find_item(pages, npages, name);
        if (!it)
            continue;
        if (it->kind == LI_KEY) {
            v = strtol(val, &end, 16);
            if (end != val && !*end && v >= 0 && v <= 0xFF)
                *it->value = (int)v;
        } else if (it->kind == LI_CHOICE) {
            int n = count_values(it), i;

            v = strtol(val, &end, 10);
            if (end != val && !*end && v >= 0 && v < n) {
                *it->value = (int)v;
                continue;
            }
            for (i = 0; i < n; i++)
                if (!strcmp(it->values[i], val))
                    *it->value = i;
        }
    }
    fclose(f);
    return 0;
}

int launcher_save(const char *path, const char *comment, LauncherPage *pages, int npages)
{
    FILE *f = fopen(path, "w");
    int p, i, ok;

    if (!f)
        return -1;
    if (comment)
        fprintf(f, "# %s\n", comment);
    for (p = 0; p < npages; p++)
        for (i = 0; i < pages[p].count; i++) {
            const LauncherItem *it = &pages[p].items[i];

            if (!it->name || !it->value)
                continue;
            if (it->kind == LI_KEY)
                fprintf(f, "%s = %02X\n", it->name, (unsigned)*it->value);
            else if (it->kind == LI_CHOICE)
                fprintf(f, "%s = %d\n", it->name, *it->value);
        }
    ok = !ferror(f);
    if (fclose(f) != 0)
        ok = 0;
    return ok ? 0 : -1;
}
