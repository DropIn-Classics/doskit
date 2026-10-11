/* unlzexe.c - a program packed with LZEXE 0.91, unpacked (unlzexe.h) */
#include <stdlib.h>
#include <string.h>
#include "unlzexe.h"

#define RELOCS_AT 0x158         /* in the unpacker's segment */
#define IMAGE_MAX 0xA0000u      /* no program is larger than conventional memory */

static uint16_t word_at(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static void put_word(uint8_t *p, unsigned v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }

/* the packed image's bits (16 at a time, a word fetched as soon as the
 * last bit of the one before is used) and whole bytes */
typedef struct {
    const uint8_t *data;
    size_t size, pos;
    unsigned word;
    int left, bad;              /* bad: read past the end */
} Bits;

static void fetch(Bits *b)
{
    if (b->pos + 2 > b->size) {
        b->bad = 1;
        b->word = 0;
    } else
        b->word = word_at(b->data + b->pos);
    b->pos += 2;
    b->left = 16;
}

static int bit(Bits *b)
{
    int v = b->word & 1;
    b->word >>= 1;
    if (--b->left == 0)
        fetch(b);
    return v;
}

static int byte(Bits *b)
{
    if (b->pos >= b->size) {
        b->bad = 1;
        return 0;
    }
    return b->data[b->pos++];
}

/* the image into out (at most IMAGE_MAX bytes); its length, and in *end
 * where the packed image ended; -1 for a damaged stream */
static long unpack_image(const uint8_t *data, size_t size, size_t pos, uint8_t *out, size_t *end)
{
    Bits b = { data, size, pos, 0, 0, 0 };
    size_t n = 0;

    fetch(&b);
    while (!b.bad) {
        size_t count, dist;
        if (bit(&b)) {
            if (n == IMAGE_MAX)
                return -1;
            out[n++] = (uint8_t)byte(&b);
            continue;
        }
        if (!bit(&b)) {
            count = (size_t)bit(&b) << 1;
            count = (count | (size_t)bit(&b)) + 2;
            dist = 256 - (size_t)byte(&b);
        } else {
            int lo = byte(&b), hi = byte(&b);
            dist = 8192 - (size_t)(lo | (hi >> 3) << 8);
            count = (size_t)(hi & 7) + 2;
            if (count == 2) {
                count = (size_t)byte(&b);
                if (b.bad)
                    break;
                if (count == 0) {
                    *end = b.pos;
                    return (long)n;
                }
                if (count == 1)
                    continue;
                count++;
            }
        }
        if (b.bad || dist > n || n + count > IMAGE_MAX)
            return -1;
        while (count--) {
            out[n] = out[n - dist];
            n++;
        }
    }
    return -1;
}

/* the relocations, each the distance from the one before, into rel as
 * offset and segment words; how many, or -1 */
static long unpack_relocs(const uint8_t *data, size_t pos, size_t end, uint8_t *rel, size_t max)
{
    unsigned long seg = 0, off = 0;
    size_t count = 0;

    for (;;) {
        unsigned span;
        if (pos >= end)
            return -1;
        span = data[pos++];
        if (span == 0) {
            if (pos + 2 > end)
                return -1;
            span = word_at(data + pos);
            pos += 2;
            if (span == 0) {
                seg += 0x0FFF;
                continue;
            }
            if (span == 1)
                return (long)count;
        }
        off += span;
        seg += off >> 4;
        off &= 15;
        if (count == max || seg > 0xFFFF)
            return -1;
        put_word(rel + 4 * count, (unsigned)off);
        put_word(rel + 4 * count + 2, (unsigned)seg);
        count++;
    }
}

int lzexe_packed(const uint8_t *data, size_t size)
{
    return size >= 0x20 && data[0] == 'M' && data[1] == 'Z' && !memcmp(data + 0x1C, "LZ91", 4);
}

uint8_t *lzexe_unpack(const uint8_t *data, size_t size, size_t *out_size)
{
    size_t start, stub, stub_len, end = 0, hdr_len, total, i;
    unsigned ip, cs, sp, ss, grows, minalloc, maxalloc, less;
    uint8_t *image, *rel, *out = NULL;
    long image_len, nrel = -1;

    if (!lzexe_packed(data, size))
        return NULL;
    start = (size_t)word_at(data + 8) * 16;
    stub = start + (size_t)word_at(data + 0x16) * 16;
    if (stub + 14 > size)
        return NULL;
    ip = word_at(data + stub);
    cs = word_at(data + stub + 2);
    sp = word_at(data + stub + 4);
    ss = word_at(data + stub + 6);
    grows = word_at(data + stub + 10);
    stub_len = word_at(data + stub + 12);
    if (stub + stub_len > size || stub_len < RELOCS_AT)
        return NULL;

    image = malloc(IMAGE_MAX);
    rel = malloc(4 * 0x10000);
    image_len = image ? unpack_image(data, stub, start, image, &end) : -1;
    if (image_len >= 0 && end <= stub && rel)
        nrel = unpack_relocs(data, stub + RELOCS_AT, stub + stub_len, rel, 0xFFFF);
    for (i = 0; nrel >= 0 && i < (size_t)nrel; i++)
        if ((size_t)word_at(rel + 4 * i + 2) * 16 + word_at(rel + 4 * i) + 2 > (size_t)image_len)
            nrel = -1;
    if (nrel >= 0) {
        /* the packed program asks for the room to unpack in and for the
         * unpacker behind it; without those it is what the program asked for */
        less = grows + (unsigned)((stub_len + 15) / 16) + 9;
        minalloc = word_at(data + 0x0A);
        maxalloc = word_at(data + 0x0C);
        minalloc = minalloc > less ? minalloc - less : 0;
        if (maxalloc != 0xFFFF)
            maxalloc = maxalloc > less && maxalloc - less > minalloc ? maxalloc - less : minalloc;
        hdr_len = (0x1E + 4 * (size_t)nrel + 511) / 512 * 512;
        total = hdr_len + (size_t)image_len;
        out = calloc(1, total ? total : 1);
    }
    if (out) {
        out[0] = 'M';
        out[1] = 'Z';
        put_word(out + 2, (unsigned)(total % 512));
        put_word(out + 4, (unsigned)((total + 511) / 512));
        put_word(out + 6, (unsigned)nrel);
        put_word(out + 8, (unsigned)(hdr_len / 16));
        put_word(out + 0x0A, minalloc);
        put_word(out + 0x0C, maxalloc);
        put_word(out + 0x0E, ss);
        put_word(out + 0x10, sp);
        put_word(out + 0x14, ip);
        put_word(out + 0x16, cs);
        put_word(out + 0x18, 0x1E);
        put_word(out + 0x1C, 1);
        memcpy(out + 0x1E, rel, 4 * (size_t)nrel);
        memcpy(out + hdr_len, image, (size_t)image_len);
        *out_size = total;
    }
    free(image);
    free(rel);
    return out;
}
