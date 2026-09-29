/* shot.c - see shot.h */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shot.h"
#include "sys.h"

/* ---- deflate with the fixed codes (RFC 1951 3.2.6) ---- */

typedef struct {
    uint8_t *out;
    size_t n, cap;
    uint32_t bits;
    int nbits, failed;
} Bits;

static void put_byte(Bits *b, uint8_t v)
{
    if (b->n == b->cap) {
        size_t cap = b->cap ? b->cap * 2 : 65536;
        uint8_t *p = (uint8_t *)realloc(b->out, cap);
        if (!p) {
            b->failed = 1;
            return;
        }
        b->out = p;
        b->cap = cap;
    }
    b->out[b->n++] = v;
}

/* n bits of v, the lowest first */
static void put_bits(Bits *b, uint32_t v, int n)
{
    b->bits |= v << b->nbits;
    b->nbits += n;
    while (b->nbits >= 8) {
        put_byte(b, (uint8_t)b->bits);
        b->bits >>= 8;
        b->nbits -= 8;
    }
}

/* a Huffman code goes out with its highest bit first */
static void put_code(Bits *b, uint32_t code, int len)
{
    uint32_t r = 0;
    int i;
    for (i = 0; i < len; i++)
        r |= ((code >> i) & 1) << (len - 1 - i);
    put_bits(b, r, len);
}

/* literal/length symbol 0-287 */
static void put_symbol(Bits *b, int v)
{
    if (v < 144)
        put_code(b, 0x30 + v, 8);
    else if (v < 256)
        put_code(b, 0x190 + v - 144, 9);
    else if (v < 280)
        put_code(b, v - 256, 7);
    else
        put_code(b, 0xC0 + v - 280, 8);
}

static const uint16_t len_base[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const uint8_t len_extra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const uint16_t dist_base[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769,
    1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const uint8_t dist_extra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

static void put_match(Bits *b, int len, int dist)
{
    int i = 28, j = 29;
    while (len_base[i] > len)
        i--;
    put_symbol(b, 257 + i);
    put_bits(b, (uint32_t)(len - len_base[i]), len_extra[i]);
    while (dist_base[j] > dist)
        j--;
    put_code(b, (uint32_t)j, 5);
    put_bits(b, (uint32_t)(dist - dist_base[j]), dist_extra[j]);
}

#define WINDOW 32768
#define HASH_BITS 15
#define CHAIN 64                        /* earlier places looked at, at most */

static unsigned hash3(const uint8_t *p)
{
    return ((unsigned)p[0] << 10 ^ (unsigned)p[1] << 5 ^ p[2]) & ((1u << HASH_BITS) - 1);
}

/* the zlib stream (RFC 1950) of data[0..n) in one fixed-code block */
static int zlib_fixed(const uint8_t *data, size_t n, Bits *b)
{
    int32_t *head = (int32_t *)malloc(sizeof(int32_t) << HASH_BITS);
    int32_t *prev = (int32_t *)malloc(sizeof(int32_t) * WINDOW);
    uint32_t s1 = 1, s2 = 0;
    size_t i, k;

    if (!head || !prev) {
        free(head);
        free(prev);
        return 0;
    }
    for (i = 0; i < (size_t)1 << HASH_BITS; i++)
        head[i] = -1;
    put_byte(b, 0x78);                  /* deflate, 32K window */
    put_byte(b, 0x01);
    put_bits(b, 1, 1);                  /* the last block */
    put_bits(b, 1, 2);                  /* fixed codes */
    i = 0;
    while (i < n) {
        int best = 0, dist = 0, len;
        if (i + 3 <= n) {
            int32_t j = head[hash3(data + i)];
            int chain = CHAIN;
            size_t max = n - i < 258 ? n - i : 258;
            while (j >= 0 && i - (size_t)j <= WINDOW && chain-- > 0) {
                size_t m = 0;
                while (m < max && data[j + m] == data[i + m])
                    m++;
                if ((int)m > best) {
                    best = (int)m;
                    dist = (int)(i - (size_t)j);
                    if (m == max)
                        break;
                }
                j = prev[j % WINDOW];
            }
        }
        len = best >= 3 ? best : 1;
        if (best >= 3)
            put_match(b, best, dist);
        else
            put_symbol(b, data[i]);
        for (k = i; k < i + (size_t)len; k++)
            if (k + 3 <= n) {
                unsigned h = hash3(data + k);
                prev[k % WINDOW] = head[h];
                head[h] = (int32_t)k;
            }
        i += (size_t)len;
    }
    put_symbol(b, 256);                 /* the end of the block */
    if (b->nbits)
        put_bits(b, 0, 8 - b->nbits);
    free(head);
    free(prev);
    for (i = 0; i < n; i++) {
        s1 = (s1 + data[i]) % 65521;
        s2 = (s2 + s1) % 65521;
    }
    put_bits(b, s2 >> 8 & 0xFF, 8);     /* Adler-32, the highest byte first */
    put_bits(b, s2 & 0xFF, 8);
    put_bits(b, s1 >> 8 & 0xFF, 8);
    put_bits(b, s1 & 0xFF, 8);
    return !b->failed;
}

/* ---- PNG ---- */

static uint32_t crc32(uint32_t crc, const uint8_t *p, size_t n)
{
    static uint32_t table[256];
    size_t i;
    if (!table[1]) {
        uint32_t c;
        int k, j;
        for (j = 0; j < 256; j++) {
            for (c = (uint32_t)j, k = 0; k < 8; k++)
                c = c & 1 ? 0xEDB88320u ^ c >> 1 : c >> 1;
            table[j] = c;
        }
    }
    crc = ~crc;
    for (i = 0; i < n; i++)
        crc = table[(crc ^ p[i]) & 0xFF] ^ crc >> 8;
    return ~crc;
}

static void be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static void chunk(FILE *f, const char *type, const uint8_t *data, size_t n)
{
    uint8_t w[4];
    be32(w, (uint32_t)n);
    fwrite(w, 1, 4, f);
    fwrite(type, 1, 4, f);
    if (n)
        fwrite(data, 1, n, f);
    be32(w, crc32(crc32(0, (const uint8_t *)type, 4), data, n));
    fwrite(w, 1, 4, f);
}

int shot_png(const char *path, const uint8_t *pixels, int width, int height,
             const uint32_t palette[256])
{
    static const uint8_t signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    uint8_t ihdr[13], plte[768];
    size_t row = (size_t)width + 1, n = row * (size_t)height;
    uint8_t *raw;
    Bits b;
    FILE *f;
    int y, i, ok;

    if (width <= 0 || height <= 0)
        return 0;
    raw = (uint8_t *)malloc(n);
    if (!raw)
        return 0;
    for (y = 0; y < height; y++) {
        raw[row * (size_t)y] = 0;       /* filter: none */
        memcpy(raw + row * (size_t)y + 1, pixels + (size_t)width * (size_t)y, (size_t)width);
    }
    memset(&b, 0, sizeof b);
    ok = zlib_fixed(raw, n, &b);
    free(raw);
    f = ok ? fopen(path, "wb") : NULL;
    if (!f) {
        free(b.out);
        return 0;
    }
    be32(ihdr, (uint32_t)width);
    be32(ihdr + 4, (uint32_t)height);
    ihdr[8] = 8;                        /* 8 bits a pixel, */
    ihdr[9] = 3;                        /* palette indexes */
    ihdr[10] = ihdr[11] = ihdr[12] = 0;
    for (i = 0; i < 256; i++) {
        plte[3 * i] = (uint8_t)(palette[i] >> 16);
        plte[3 * i + 1] = (uint8_t)(palette[i] >> 8);
        plte[3 * i + 2] = (uint8_t)palette[i];
    }
    fwrite(signature, 1, 8, f);
    chunk(f, "IHDR", ihdr, 13);
    chunk(f, "PLTE", plte, 768);
    chunk(f, "IDAT", b.out, b.n);
    chunk(f, "IEND", NULL, 0);
    free(b.out);
    ok = !ferror(f);
    return fclose(f) == 0 && ok;
}

/* ---- the picture last shown ---- */

static uint8_t *kept;
static size_t kept_size;
static int kept_w, kept_h;
static uint32_t kept_palette[256];
static char shot_folder[512], shot_name[64] = "screenshot";
static unsigned next_number;

void shot_keep(const uint8_t *pixels, int width, int height, const uint32_t palette[256])
{
    size_t n = (size_t)width * (size_t)height;
    if (width <= 0 || height <= 0)
        return;
    if (n > kept_size) {
        uint8_t *p = (uint8_t *)realloc(kept, n);
        if (!p)
            return;
        kept = p;
        kept_size = n;
    }
    memcpy(kept, pixels, n);
    memcpy(kept_palette, palette, sizeof kept_palette);
    kept_w = width;
    kept_h = height;
}

void shot_set_name(const char *folder, const char *name)
{
    snprintf(shot_folder, sizeof shot_folder, "%s", folder ? folder : "");
    snprintf(shot_name, sizeof shot_name, "%s", name ? name : "screenshot");
    next_number = 0;
}

int shot_save(const char *path, char *written, int size)
{
    char file[600], name[96];

    if (!kept_w)
        return 0;
    if (path)
        snprintf(file, sizeof file, "%s", path);
    else
        for (;; next_number++) {
            if (next_number > 9999)
                return 0;
            snprintf(name, sizeof name, "%s_%04u.png", shot_name, next_number);
            if (shot_folder[0])
                sys_join(file, sizeof file, shot_folder, name);
            else
                snprintf(file, sizeof file, "%s", name);
            if (!sys_is_file(file))
                break;
        }
    if (!shot_png(file, kept, kept_w, kept_h, kept_palette))
        return 0;
    if (!path)
        next_number++;
    if (written && size > 0)
        snprintf(written, (size_t)size, "%s", file);
    return 1;
}
