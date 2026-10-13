/* hint.c - see hint.h */
#include <stddef.h>
#include <string.h>
#include "frame.h"
#include "hint.h"
#include "textmode.h"

/* the setup screen's colours (the CGA's), with the layer's alpha */
#define C_WINDOW 0xFF0000AAu            /* blue */
#define C_FRAME  0xFFAAAAAAu            /* light grey */
#define C_TITLE  0xFFFFFF55u            /* yellow */
#define C_TEXT   0xFFFFFFFFu            /* white */
#define C_KEYS   0xFF55FFFFu            /* light cyan */
#define C_MARK   0xFFFFFF55u            /* yellow */
#define C_EDGE   0xFF000000u            /* black, around the mark */
#define C_SHADOW 0x90000000u
#define C_DIM    0x70000000u            /* over the picture while held */

#define MAX_LINES 24
#define MAX_WIDE 56                     /* a card's text, in characters */

static const Hint *shown, *held;

static uint32_t *pix;
static int pw, ph;                      /* the layer's size */
static int cw, ch, unit;                /* a character's cell, a font pixel */

static void fill(int x, int y, int w, int h, uint32_t colour)
{
    int i, j, x1 = x + w, y1 = y + h;

    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (x1 > pw)
        x1 = pw;
    if (y1 > ph)
        y1 = ph;
    for (j = y; j < y1; j++)
        for (i = x; i < x1; i++)
            pix[j * pw + i] = colour;
}

static void put(int x, int y, uint8_t c, uint32_t colour)
{
    const uint8_t *g = tm_glyph(c);
    int r, b;

    for (r = 0; r < 16; r++)
        for (b = 0; b < 8; b++)
            if (g[r] & (0x80 >> b))
                fill(x + b * unit, y + r * unit, unit, unit, colour);
}

static void put_text(int x, int y, const uint8_t *s, int n, uint32_t colour)
{
    int i;

    for (i = 0; i < n; i++)
        put(x + i * cw, y, s[i], colour);
}

/* the next character of a UTF-8 text in the font's code page 437, '?'
 * for one the font has not; 0 at the end */
static uint8_t next_char(const char **p)
{
    static const struct { uint8_t second, code; } german[] = {
        { 0xA4, TM_A_UMLAUT }, { 0xB6, TM_O_UMLAUT }, { 0xBC, TM_U_UMLAUT },
        { 0x84, TM_CAP_A_UMLAUT }, { 0x96, TM_CAP_O_UMLAUT }, { 0x9C, TM_CAP_U_UMLAUT },
        { 0x9F, TM_SHARP_S },
    };
    const unsigned char *s = (const unsigned char *)*p;
    size_t i;
    uint8_t c = *s;

    if (!c)
        return 0;
    s++;
    if (c >= 0x80) {
        uint8_t lead = c;
        c = '?';
        if (lead == 0xC3)
            for (i = 0; i < sizeof german / sizeof german[0]; i++)
                if (*s == german[i].second)
                    c = german[i].code;
        while ((*s & 0xC0) == 0x80)     /* the rest of the character */
            s++;
    }
    *p = (const char *)s;
    return c;
}

/* a text of one line in the font's codes; its length, at most `size` */
static int one_line(const char *text, uint8_t *out, int size)
{
    int n = 0;
    uint8_t c;

    while (text && n < size && (c = next_char(&text)) != 0)
        out[n++] = c == '\n' ? ' ' : c;
    return n;
}

static uint8_t lines[MAX_LINES][MAX_WIDE];
static int lens[MAX_LINES], nlines;

/* the text into lines of at most `wide` characters, broken at spaces (a
 * longer word is cut); what does not fit into MAX_LINES is left out */
static void wrap(const char *text, int wide)
{
    uint8_t word[MAX_WIDE];
    int n = 0, len = 0;                 /* the word and the line so far */
    uint8_t c;

    nlines = 0;
    if (!text)
        return;
    do {
        c = next_char(&text);
        if (c && c != ' ' && c != '\n' && n < wide) {
            word[n++] = c;
            continue;
        }
        if (n) {                        /* a word ends here */
            if (len && len + 1 + n > wide) {
                lens[nlines++] = len;
                len = 0;
                if (nlines == MAX_LINES)
                    return;
            }
            if (len)
                lines[nlines][len++] = ' ';
            memcpy(lines[nlines] + len, word, (size_t)n);
            len += n;
            n = 0;
        }
        if (c && c != ' ' && c != '\n')
            word[n++] = c;              /* the cut word goes on */
        if (c == '\n' || (!c && len)) {
            lens[nlines++] = len;
            len = 0;
            if (nlines == MAX_LINES)
                return;
        }
    } while (c);
}

/* the frame around the place: yellow, a black line on both its sides */
static void mark(int x, int y, int w, int h)
{
    int t = 2 * unit, e = unit, k;

    for (k = 0; k < 2; k++) {
        /* a black frame from the place outwards, the yellow one inside it */
        int o = k ? t + e : t + 2 * e, d = k ? t : t + 2 * e;
        uint32_t c = k ? C_MARK : C_EDGE;
        int x0 = x - o, y0 = y - o, x1 = x + w + o, y1 = y + h + o;

        fill(x0, y0, x1 - x0, d, c);
        fill(x0, y1 - d, x1 - x0, d, c);
        fill(x0, y0, d, y1 - y0, c);
        fill(x1 - d, y0, d, y1 - y0, c);
    }
}

int hint_layer(uint32_t *pixels, int width, int height, void *user)
{
    const Hint *h = held ? held : shown;
    uint8_t title[MAX_WIDE], keys[MAX_WIDE];
    int ntitle, nkeys, wide, inner, cols, rows, x, y, i, row;
    int placed = 0, mx = 0, my = 0, mw = 0, mh = 0;

    (void)user;
    if (!h || !pixels || width < 64 || height < 64)
        return 0;
    pix = pixels;
    pw = width;
    ph = height;
    /* a character 16 pixels high in a window of 480 lines, 32 from 600 */
    unit = (height + 120) / 360;
    if (unit < 1)
        unit = 1;
    while (unit > 1 && width / (8 * unit) < 40)
        unit--;
    cw = 8 * unit;
    ch = 16 * unit;

    wide = width / cw - 12;
    if (wide > MAX_WIDE)
        wide = MAX_WIDE;
    if (wide < 8)
        wide = 8;
    wrap(h->text, wide);
    ntitle = one_line(h->title, title, wide - 2);
    nkeys = one_line(h->keys, keys, wide);
    inner = ntitle + 2 > nkeys ? ntitle + 2 : nkeys;
    for (i = 0; i < nlines; i++)
        if (lens[i] > inner)
            inner = lens[i];
    cols = inner + 4;                   /* the frame and a space each side */
    rows = 2 + (nlines ? nlines + 2 : 0) + (nkeys ? 1 : 0);
    if (!nlines && !nkeys)
        rows = 3;

    if (h->w > 0 && h->h > 0 && h->across > 0 && h->down > 0) {
        placed = 1;
        mx = (int)((long)h->x * width / h->across);
        my = (int)((long)h->y * height / h->down);
        mw = (int)((long)(h->x + h->w) * width / h->across) - mx;
        mh = (int)((long)(h->y + h->h) * height / h->down) - my;
    }
    if (held) {
        /* the picture dimmed but for the place */
        if (!placed)
            fill(0, 0, width, height, C_DIM);
        else {
            fill(0, 0, width, my, C_DIM);
            fill(0, my + mh, width, height - my - mh, C_DIM);
            fill(0, my, mx, mh, C_DIM);
            fill(mx + mw, my, width - mx - mw, mh, C_DIM);
        }
    }
    if (placed)
        mark(mx, my, mw, mh);

    /* the card: away from the place; without one in the middle while
     * the program is held and at the bottom over the running game */
    x = (width - cols * cw) / 2;
    if (placed ? my + mh / 2 < height / 2 : !held)
        y = height - (rows + 1) * ch;
    else if (placed)
        y = ch;
    else
        y = (height - rows * ch) / 2;
    if (y < 0)
        y = 0;
    fill(x + 2 * cw, y + ch, cols * cw, rows * ch, C_SHADOW);
    fill(x, y, cols * cw, rows * ch, C_WINDOW);
    put(x, y, TM_DD_TOP_LEFT, C_FRAME);
    put(x + (cols - 1) * cw, y, TM_DD_TOP_RIGHT, C_FRAME);
    put(x, y + (rows - 1) * ch, TM_DD_BOTTOM_LEFT, C_FRAME);
    put(x + (cols - 1) * cw, y + (rows - 1) * ch, TM_DD_BOTTOM_RIGHT, C_FRAME);
    for (i = 1; i < cols - 1; i++) {
        int at = (cols - ntitle) / 2;
        if (!ntitle || i < at - 1 || i > at + ntitle)
            put(x + i * cw, y, TM_HH, C_FRAME);
        put(x + i * cw, y + (rows - 1) * ch, TM_HH, C_FRAME);
    }
    for (i = 1; i < rows - 1; i++) {
        put(x, y + i * ch, TM_VV, C_FRAME);
        put(x + (cols - 1) * cw, y + i * ch, TM_VV, C_FRAME);
    }
    put_text(x + (cols - ntitle) / 2 * cw, y, title, ntitle, C_TITLE);
    row = nlines ? 2 : 1;
    for (i = 0; i < nlines; i++)
        put_text(x + 2 * cw, y + (row + i) * ch, lines[i], lens[i], C_TEXT);
    if (nlines)
        row += nlines + 1;
    if (nkeys)
        put_text(x + (cols - nkeys) / 2 * cw, y + row * ch, keys, nkeys, C_KEYS);
    return 1;
}

void hint_show(const Hint *hint)
{
    shown = hint;
}

/* frame_hold's input: what a key or a button answers */
static int answer(int code, int clicks, void *user)
{
    (void)user;
    if (clicks & 1)
        return HINT_NEXT;
    switch (code) {
    case 0x1C: case 0x9C: case 0x39:    /* Enter, the keypad's, Space */
        return HINT_NEXT;
    case 0x0E: case 0xCB:               /* Backspace, Left */
        return HINT_BACK;
    case 0x01:                          /* Esc */
        return HINT_END;
    }
    return 0;
}

int hint_run(const Hint *hint)
{
    int r;

    if (!hint)
        return HINT_NEXT;
    held = hint;
    r = frame_hold(answer, NULL);
    held = NULL;
    return r;
}
