/* launcher.c - see launcher.h */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "launcher.h"
#include "pad.h"
#include "platform.h"
#include "textmode.h"

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

/* The attributes of the design in docs/LAUNCHER.md: white on blue, a grey
 * bar at the top and at the bottom (the keys in red), a cyan cursor. */
#define BG TM_BLUE
#define A_TEXT TM_ATTR(TM_WHITE, BG)
#define A_TITLE TM_ATTR(TM_YELLOW, BG)
#define A_HEAD TM_ATTR(TM_LIGHTCYAN, BG)
#define A_LABEL TM_ATTR(TM_LIGHTGREY, BG)
#define A_VALUE TM_ATTR(TM_YELLOW, BG)
#define A_CURSOR TM_ATTR(TM_BLACK, TM_CYAN)
#define A_BAR TM_ATTR(TM_BLACK, TM_LIGHTGREY)
#define A_BAR_KEY TM_ATTR(TM_RED, TM_LIGHTGREY)

#define MAX_PAGES 16
#define MIN_W 40                /* a window's width, at least */
#define MAX_W 76
#define MAX_ROWS 15             /* items shown at once */
#define HELP_ROW (TM_ROWS - 3)  /* the selected item's help */
#define NOTE_ROW (TM_ROWS - 2)  /* the port's footer */

static uint8_t pixels[TM_WIDTH * TM_HEIGHT];
static uint32_t palette[256];

/* what the main page ends with: a gap and Quit */
static LauncherItem quit_items[2] = {
    { LI_HEAD, "", NULL, NULL, NULL, 0, NULL },
    { LI_ACTION, "Quit", NULL, NULL, NULL, LAUNCHER_QUIT, NULL },
};

/* how many items a page shows: the main page (0) has Quit added */
static int item_count(const LauncherPage *pages, int page)
{
    return pages[page].count + (page == 0 ? 2 : 0);
}

static LauncherItem *item_at(LauncherPage *pages, int page, int i)
{
    return i < pages[page].count ? &pages[page].items[i] : &quit_items[i - pages[page].count];
}

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

/* the title bar: the game's name at the left, the port's name
 * and version at the right (left out when both do not fit) */
static void title_bar(const LauncherApp *app)
{
    char left[TM_COLS], right[TM_COLS];

    snprintf(left, sizeof left, "%.60s", app->game);
    if (app->version && *app->version)
        snprintf(right, sizeof right, "%.40s %.20s", app->port, app->version);
    else
        snprintf(right, sizeof right, "%.40s", app->port);
    tm_fill(0, 0, TM_COLS, 1, ' ', A_BAR);
    tm_text(1, 0, left, A_BAR);
    if ((int)(strlen(left) + strlen(right)) + 3 <= TM_COLS)
        tm_text(TM_COLS - 1 - (int)strlen(right), 0, right, A_BAR);
}

/* the bottom bar: pairs of a key (red) and what it does */
static void help_bar(const char *const *parts)
{
    int x = 1;

    tm_fill(0, TM_ROWS - 1, TM_COLS, 1, ' ', A_BAR);
    for (; parts[0]; parts += 2) {
        x = tm_text(x, TM_ROWS - 1, parts[0], A_BAR_KEY);
        x = tm_text(x + 1, TM_ROWS - 1, parts[1], A_BAR) + 3;
    }
}

/* the blue screen with its title bar */
static void backdrop(const LauncherApp *app)
{
    tm_clear(' ', A_TEXT);
    title_bar(app);
}

static void present(void)
{
    tm_render(pixels, palette);
    plat_present(pixels, TM_WIDTH, TM_HEIGHT, palette);
}

/* a page's window: the label column, the value column, the width */
typedef struct {
    int w, x, y, h, rows, value_x;
} Layout;

static void layout(LauncherPage *pages, int page, const char *title, Layout *l)
{
    int i, n = item_count(pages, page), lw = 0, vw = 0, w;

    for (i = 0; i < n; i++) {
        const LauncherItem *it = item_at(pages, page, i);
        int len = (int)strlen(it->label), v = 0, k;

        if (it->kind == LI_CHOICE) {
            for (k = 0; it->values && it->values[k]; k++)
                if ((int)strlen(it->values[k]) > v)
                    v = (int)strlen(it->values[k]);
        } else if (it->kind == LI_KEY) {
            v = 14;                         /* "press a key ..." */
        }
        if (v) {
            if (len > lw)
                lw = len;
            if (v > vw)
                vw = v;
        }
    }
    w = 4 + lw;
    if (vw)
        w += 4 + vw + 4;
    for (i = 0; i < n; i++) {
        const LauncherItem *it = item_at(pages, page, i);

        if (4 + (int)strlen(it->label) + 4 > w)
            w = 4 + (int)strlen(it->label) + 4;
    }
    if ((int)strlen(title) + 8 > w)
        w = (int)strlen(title) + 8;
    l->w = w < MIN_W ? MIN_W : w > MAX_W ? MAX_W : w;
    l->rows = n < MAX_ROWS ? n : MAX_ROWS;
    l->h = l->rows + 4;
    l->x = (TM_COLS - l->w) / 2;
    l->y = 2 + (19 - l->h) / 2;
    l->value_x = l->x + 4 + lw + 4;
}

static void draw(const LauncherApp *app, const char *footer, LauncherPage *pages, int page, int sel,
                 int top, int waiting)
{
    static const char *const help_main[] = { "\x18\x19", "Select", "Enter", "Choose", "Esc", "To Quit", NULL };
    static const char *const help_main_choice[] = {
        "\x18\x19", "Select", "\x1B\x1A", "Change", "Enter", "Choose", "Esc", "To Quit", NULL
    };
    static const char *const help_page[] = {
        "\x18\x19", "Select", "\x1B\x1A", "Change", "Enter", "Choose", "Esc", "Back", NULL
    };
    static const char *const help_key[] = { "Any key", "Set it", "Backspace", "None", "Esc", "Keep the key", NULL };
    const char *title = page == 0 ? "Setup" : pages[page].title;
    char buf[48];
    Layout l;
    int i;

    layout(pages, page, title, &l);
    backdrop(app);
    tm_fill(l.x, l.y, l.w, l.h, ' ', A_TEXT);
    tm_frame(l.x, l.y, l.w, l.h, A_TEXT);
    tm_shadow(l.x, l.y, l.w, l.h);
    snprintf(buf, sizeof buf, " %s ", title);
    tm_text(l.x + (l.w - (int)strlen(buf)) / 2, l.y, buf, A_TITLE);
    for (i = 0; i < l.rows; i++) {
        const LauncherItem *it = item_at(pages, page, top + i);
        int y = l.y + 2 + i, on = top + i == sel;
        uint8_t a = on ? A_CURSOR : it->kind == LI_HEAD ? A_HEAD : A_LABEL;

        if (on)
            tm_fill(l.x + 2, y, l.w - 4, 1, ' ', A_CURSOR);
        tm_text(l.x + 4, y, it->label, a);
        if (it->kind == LI_CHOICE && it->value) {
            int v = *it->value, n = count_values(it);
            const char *s = v >= 0 && v < n ? it->values[v] : "?";

            tm_text(l.value_x, y, s, on ? a : A_VALUE);
            if (on) {
                tm_put(l.value_x - 2, y, TM_LEFT_TRIANGLE, a);
                tm_put(l.value_x + (int)strlen(s) + 1, y, TM_RIGHT_TRIANGLE, a);
            }
        } else if (it->kind == LI_KEY && it->value) {
            tm_text(l.value_x, y, on && waiting ? "press a key ..." : launcher_key_name(*it->value),
                    on ? a : A_VALUE);
        }
    }
    if (top > 0)
        tm_put(l.x + l.w - 3, l.y + 2, TM_UP_ARROW, A_TEXT);
    if (top + l.rows < item_count(pages, page))
        tm_put(l.x + l.w - 3, l.y + 1 + l.rows, TM_DOWN_ARROW, A_TEXT);
    if (sel >= 0 && item_at(pages, page, sel)->help)
        centre(HELP_ROW, item_at(pages, page, sel)->help, A_HEAD);
    if (footer)
        centre(NOTE_ROW, footer, TM_ATTR(TM_DARKGREY, BG));
    help_bar(waiting ? help_key
             : page == 0 ? (sel >= 0 && item_at(pages, 0, sel)->kind == LI_CHOICE ? help_main_choice : help_main)
             : help_page);
    present();
}

/* the next key pressed (make code, E0 keys + 80h); -1 when there is none */
static int next_key(void)
{
    static int e0;
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
        if (b == 0xAA)              /* the fake shift of E0 sequences */
            continue;
        return b;
    }
    return -1;
}

static int first_selectable(LauncherPage *pages, int page, int from, int step)
{
    int i, n = item_count(pages, page);

    for (i = from; i >= 0 && i < n; i += step)
        if (selectable(item_at(pages, page, i)))
            return i;
    return -1;
}

int launcher_run(const LauncherApp *app, const char *footer, LauncherPage *pages, int npages,
                 void (*changed)(const LauncherItem *item))
{
    const PadKeys *was;
    int page = 0, from[MAX_PAGES], sel[MAX_PAGES], top[MAX_PAGES], waiting = 0, result = LAUNCHER_QUIT, i;

    if (npages < 1 || npages > MAX_PAGES)
        return LAUNCHER_QUIT;
    was = pad_set_keys(&pad_menu_keys);
    for (i = 0; i < npages; i++) {
        from[i] = 0;
        sel[i] = first_selectable(pages, i, 0, 1);
        top[i] = 0;
    }
    for (;;) {
        LauncherItem *it = sel[page] >= 0 ? item_at(pages, page, sel[page]) : NULL;
        int k, turn = 0, n = item_count(pages, page), rows = n < MAX_ROWS ? n : MAX_ROWS;

        if (sel[page] >= 0 && sel[page] < top[page])
            top[page] = sel[page];
        if (sel[page] >= top[page] + rows)
            top[page] = sel[page] - rows + 1;
        if (sel[page] == first_selectable(pages, page, 0, 1))
            top[page] = 0;
        draw(app, footer, pages, page, sel[page], top[page], waiting);
        if (!plat_pump())
            break;
        while (plat_read_control() >= 0)
            ;
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
        case 0x01:                          /* Esc: back; in the menu on to "Quit" */
            if (page == 0)
                sel[0] = first_selectable(pages, 0, item_count(pages, 0) - 1, -1);
            else
                page = from[page];
            break;
        case 0xC8: {                        /* Up */
            int s = sel[page] > 0 ? first_selectable(pages, page, sel[page] - 1, -1) : -1;

            if (s >= 0)
                sel[page] = s;
            break;
        }
        case 0xD0: {                        /* Down */
            int s = first_selectable(pages, page, sel[page] + 1, 1);

            if (s >= 0)
                sel[page] = s;
            break;
        }
        case 0xC7: case 0xC9:               /* Home, Page Up: the first */
            sel[page] = first_selectable(pages, page, 0, 1);
            break;
        case 0xCF: case 0xD1:               /* End, Page Down: the last */
            sel[page] = first_selectable(pages, page, n - 1, -1);
            break;
        case 0xCB:                          /* Left */
            turn = -1;
            break;
        case 0xCD:                          /* Right */
            turn = 1;
            break;
        case 0x1C: case 0x9C: case 0x39:    /* Enter, Space */
            if (!it)
                break;
            if (it->kind == LI_ACTION) {
                result = it->action;
                goto done;
            }
            if (it->kind == LI_PAGE) {
                if (it->action > 0 && it->action < npages && it->action != page) {
                    from[it->action] = page;
                    page = it->action;
                }
            } else if (it->kind == LI_KEY && it->value) {
                waiting = 1;
            } else {
                turn = 1;
            }
            break;
        }
        if (turn && it && it->kind == LI_CHOICE && it->value) {
            int m = count_values(it);

            if (m > 0) {
                *it->value = ((*it->value + turn) % m + m) % m;
                if (changed)
                    changed(it);
            }
        }
    }
done:
    pad_set_keys(was);
    return result;
}

/* ---- the dialog about the game's files */

#define D_WINDOW A_TEXT
#define D_TITLE A_TITLE
#define D_LABEL A_LABEL
#define D_VALUE A_VALUE
#define D_CURSOR A_CURSOR

#define D_W 70                  /* the window's width */
#define D_TEXT (D_W - 6)        /* and its text's */
#define D_MAX 14                /* lines of text at most */

/* the window's lines: "" an empty one, one starting with a space a value
 * (a path) */
static char d_lines[D_MAX][D_TEXT + 1];
static int d_n;

static void d_gap(void)
{
    if (d_n < D_MAX)
        d_lines[d_n++][0] = 0;
}

/* a paragraph, broken between words */
static void d_para(const char *s)
{
    while (*s && d_n < D_MAX) {
        int k = (int)strlen(s);

        if (k > D_TEXT) {
            for (k = D_TEXT; k > 0 && s[k] != ' '; k--)
                ;
            if (!k)
                k = D_TEXT;
        }
        snprintf(d_lines[d_n++], sizeof d_lines[0], "%.*s", k, s);
        s += k;
        while (*s == ' ')
            s++;
    }
}

/* a path or a reason on a line of its own; of one too long the start is
 * left out */
static void d_value(const char *s)
{
    int len = (int)strlen(s), w = D_TEXT - 2;

    if (d_n >= D_MAX)
        return;
    if (len <= w)
        snprintf(d_lines[d_n++], sizeof d_lines[0], "  %s", s);
    else
        snprintf(d_lines[d_n++], sizeof d_lines[0], "  ...%s", s + len - (w - 3));
}

/* the window of d_lines and below them the choices, `cursor` highlighted */
static void d_window(const char *const *choices, int nchoices, int cursor)
{
    static const char title[] = " The game's files ";
    int h = d_n + nchoices + 5, x = (TM_COLS - D_W) / 2, y = 2 + (20 - h) / 2, i;

    tm_fill(x, y, D_W, h, ' ', D_WINDOW);
    tm_frame(x, y, D_W, h, D_WINDOW);
    tm_text(x + (D_W - (int)strlen(title)) / 2, y, title, D_TITLE);
    tm_shadow(x, y, D_W, h);
    for (i = 0; i < d_n; i++)
        if (d_lines[i][0])
            tm_text(x + 3, y + 2 + i, d_lines[i], d_lines[i][0] == ' ' ? D_VALUE : D_LABEL);
    for (i = 0; i < nchoices; i++) {
        int row = y + 3 + d_n + i;

        if (i == cursor)
            tm_fill(x + 2, row, D_W - 4, 1, ' ', D_CURSOR);
        tm_text(x + 4, row, choices[i], i == cursor ? D_CURSOR : D_LABEL);
    }
}

/* d_lines and the choices until Enter (Esc, or the window closed: the
 * last choice); the choice */
static int d_ask(const LauncherApp *app, const char *const *choices, int nchoices)
{
    static const char *const help[] = { "\x18\x19", "Select", "Enter", "Choose", NULL };
    const PadKeys *was = pad_set_keys(&pad_menu_keys);
    int cursor = 0, r = -1, k;

    while (r < 0) {
        if (!plat_pump()) {
            r = nchoices - 1;
            break;
        }
        while (plat_read_control() >= 0)
            ;
        while (r < 0 && (k = next_key()) >= 0) {
            if ((k == 0x48 || k == 0xC8) && cursor > 0)
                cursor--;
            else if ((k == 0x50 || k == 0xD0) && cursor < nchoices - 1)
                cursor++;
            else if (k == 0x1C || k == 0x9C || k == 0x39)
                r = cursor;
            else if (k == 0x01)
                r = nchoices - 1;
        }
        backdrop(app);
        d_window(choices, nchoices, cursor);
        help_bar(help);
        present();
        plat_sleep_ms(15);
    }
    pad_set_keys(was);
    return r;
}

int launcher_offer_copy(const LauncherApp *app, int what, const char *from, const char *to)
{
    static const char *const copy_or_quit[] = { "Copy the files", "Quit" };
    static const char *const copy_or_not[] = { "Copy the files", "Not now" };
    char text[400];

    d_n = 0;
    if (what == LAUNCHER_CD)
        snprintf(text, sizeof text, "%s plays the music of %s from a copy of your own CD. It "
                 "is not here yet; your GOG release is:", app->port, app->game);
    else
        snprintf(text, sizeof text, "%s runs %s with the files of your own copy of the game. "
                 "They are not here yet; your GOG release is:", app->port, app->game);
    d_para(text);
    d_value(from);
    d_gap();
    d_para(what == LAUNCHER_GAME ? "Its files can be copied from there into:"
           : what == LAUNCHER_CD ? "The CD's image and music can be copied from there into:"
           : "Its files and the CD's image and music can be copied from there into:");
    d_value(to);
    return d_ask(app, what == LAUNCHER_CD ? copy_or_not : copy_or_quit, 2) == 0;
}

static const char *const d_quit[] = { "Quit" };

void launcher_no_game(const LauncherApp *app, const char *how)
{
    char text[400];

    d_n = 0;
    snprintf(text, sizeof text, "%s runs %s with the files of your own copy of the game, its "
             "GOG release. Neither its files nor an installed GOG release were found.",
             app->port, app->game);
    d_para(text);
    d_gap();
    snprintf(text, sizeof text, "Install the game from GOG and start %s again, or start it "
             "with %s.", app->port,
             how ? how : "-gog FILE (the release's CD image, folder or installer) or -game "
                         "FOLDER (the game's files)");
    d_para(text);
    d_ask(app, d_quit, 1);
}

void launcher_copy_failed(const LauncherApp *app, const char *from, const char *why)
{
    d_n = 0;
    d_para("The files could not be copied:");
    d_value(why);
    d_gap();
    d_para("From:");
    d_value(from);
    d_ask(app, d_quit, 1);
}

int launcher_copy_progress(void *ctx, const char *file, long done, long total)
{
    static const char *const help[] = { "", "Copying ...", NULL };
    LauncherCopy *c = (LauncherCopy *)ctx;
    int width = 50, full = total > 0 ? (int)((double)done / (double)total * width) : 0, i;

    if (c->drawn && plat_micros() - c->drawn < 30000 && done < total)
        return c->closed;
    c->drawn = plat_micros();
    d_n = 0;
    d_para(c->what == LAUNCHER_CD ? "Copying the CD's image and music from your GOG release:"
                                  : "Copying the game's files from your GOG release:");
    d_gap();
    for (i = 0; i < width; i++)
        d_lines[d_n][i] = (char)(i < full ? TM_BLOCK : TM_SHADE_LIGHT);
    d_lines[d_n++][width] = 0;
    snprintf(d_lines[d_n++], sizeof d_lines[0], " %3d %%   %.50s",
             total > 0 ? (int)(100.0 * (double)done / (double)total) : 0, file ? file : "");
    backdrop(c->app);
    d_window(NULL, 0, -1);
    help_bar(help);
    present();
    if (!plat_pump())
        c->closed = 1;
    return c->closed;
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
