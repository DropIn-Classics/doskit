/* inno.c - see inno.h.  The format as tools/inno.py reads it (its
 * docstring says where it is described); the two are kept alike. */

/* the LZMA SDK's decoders, compiled in here (third_party/lzma/README.md);
 * first, as its Precomp.h asks */
#include "third_party/lzma/LzmaDec.c"
#include "third_party/lzma/Lzma2Dec.c"

#ifndef _WIN32
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <sys/types.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cdimage.h"
#include "inno.h"
#include "sha256.h"
#include "sys.h"

#ifdef _WIN32
#define SEEK64(f, o) _fseeki64(f, (__int64)(o), SEEK_SET)
#else
#define SEEK64(f, o) fseeko(f, (off_t)(o), SEEK_SET)
#endif

/* versions as numbers: 5.6.2 is V(5, 6, 2); 6.4.0.1 V4(6, 4, 0, 1) */
#define V4(a, b, c, d) ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(c) << 8 | (uint32_t)(d))
#define V(a, b, c) V4(a, b, c, 0)

enum { STORED, ZLIB, BZIP2, LZMA1, LZMA2 };
enum { HASH_MD5 = 16, HASH_SHA1 = 20, HASH_SHA256 = 32 };

#define SEARCHED (16L << 20)    /* the offset table is in the .exe's resources, near its start */
#define PIECE 65536
#define IN_MEMORY (1u << 30)   /* the most held in memory at once: a part, an .exe, a zlib chunk */
#define MAX_ARGS 4
#define ARG_LEN 512

static const uint8_t LOADER_MAGIC[2][12] = {
    {'r', 'D', 'l', 'P', 't', 'S', 0xcd, 0xe6, 0xd7, '{', 0x0b, '*'},
    {'n', 'S', '5', 'W', '7', 'd', 'T', 0x83, 0xaa, 0x1b, 0x0f, 'j'},
};

/* ---- little helpers: numbers, CRC32, MD5, SHA-1 */

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint64_t le64(const uint8_t *p)
{
    return (uint64_t)le32(p) | (uint64_t)le32(p + 4) << 32;
}

static uint32_t crc32_of(const uint8_t *p, size_t n)
{
    static uint32_t table[256];
    uint32_t c = 0xFFFFFFFFu;
    size_t i;

    if (!table[1]) {
        uint32_t k, j;
        for (k = 0; k < 256; k++) {
            uint32_t v = k;
            for (j = 0; j < 8; j++)
                v = v & 1 ? 0xEDB88320u ^ (v >> 1) : v >> 1;
            table[k] = v;
        }
    }
    for (i = 0; i < n; i++)
        c = table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static uint32_t rol(uint32_t x, int k)
{
    return x << k | x >> (32 - k);
}

typedef struct {
    uint32_t h[5];
    uint64_t len;
    uint8_t buf[64];
    int n;
} Block64;                      /* MD5's and SHA-1's running state */

static void md5_block(uint32_t *h, const uint8_t *p)
{
    static const uint32_t K[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
    static const int R[16] = {7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21};
    uint32_t m[16], a = h[0], b = h[1], c = h[2], d = h[3];
    int i;

    for (i = 0; i < 16; i++)
        m[i] = le32(p + 4 * i);
    for (i = 0; i < 64; i++) {
        uint32_t f, t;
        int g;
        if (i < 16)
            f = (b & c) | (~b & d), g = i;
        else if (i < 32)
            f = (d & b) | (~d & c), g = (5 * i + 1) & 15;
        else if (i < 48)
            f = b ^ c ^ d, g = (3 * i + 5) & 15;
        else
            f = c ^ (b | ~d), g = (7 * i) & 15;
        t = d;
        d = c;
        c = b;
        b = b + rol(a + f + K[i] + m[g], R[(i / 16) * 4 + (i & 3)]);
        a = t;
    }
    h[0] += a, h[1] += b, h[2] += c, h[3] += d;
}

static uint32_t be32(const uint8_t *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | (uint32_t)p[3];
}

static void sha1_block(uint32_t *h, const uint8_t *p)
{
    uint32_t w[80], a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    int i;

    for (i = 0; i < 16; i++)
        w[i] = be32(p + 4 * i);
    for (; i < 80; i++)
        w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    for (i = 0; i < 80; i++) {
        uint32_t f, k, t;
        if (i < 20)
            f = (b & c) | (~b & d), k = 0x5A827999;
        else if (i < 40)
            f = b ^ c ^ d, k = 0x6ED9EBA1;
        else if (i < 60)
            f = (b & c) | (b & d) | (c & d), k = 0x8F1BBCDC;
        else
            f = b ^ c ^ d, k = 0xCA62C1D6;
        t = rol(a, 5) + f + e + k + w[i];
        e = d, d = c, c = rol(b, 30), b = a, a = t;
    }
    h[0] += a, h[1] += b, h[2] += c, h[3] += d, h[4] += e;
}

typedef struct {
    int kind;                   /* HASH_*: the digest's length */
    Block64 b;
    Sha256 s256;
} Hash;

static void hash_init(Hash *h, int kind)
{
    static const uint32_t start[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    memset(h, 0, sizeof *h);
    h->kind = kind;
    memcpy(h->b.h, start, sizeof start);
    if (kind == HASH_SHA256)
        sha256_init(&h->s256);
}

static void hash_update(Hash *h, const uint8_t *p, size_t n)
{
    if (h->kind == HASH_SHA256) {
        sha256_update(&h->s256, p, n);
        return;
    }
    h->b.len += n;
    while (!h->b.n && n >= 64) {               /* whole blocks straight from p */
        if (h->kind == HASH_MD5)
            md5_block(h->b.h, p);
        else
            sha1_block(h->b.h, p);
        p += 64, n -= 64;
    }
    while (n) {
        size_t k = 64 - (size_t)h->b.n < n ? 64 - (size_t)h->b.n : n;
        memcpy(h->b.buf + h->b.n, p, k);
        h->b.n += (int)k, p += k, n -= k;
        if (h->b.n == 64) {
            if (h->kind == HASH_MD5)
                md5_block(h->b.h, h->b.buf);
            else
                sha1_block(h->b.h, h->b.buf);
            h->b.n = 0;
        }
    }
}

static void hash_final(Hash *h, uint8_t *out)
{
    uint8_t pad[72];
    uint64_t bits = h->b.len * 8;
    size_t k = (size_t)((h->b.n < 56 ? 56 : 120) - h->b.n);
    int i;

    if (h->kind == HASH_SHA256) {
        sha256_final(&h->s256, out);
        return;
    }
    memset(pad, 0, sizeof pad);
    pad[0] = 0x80;
    for (i = 0; i < 8; i++)            /* the length: MD5 little-, SHA-1 big-endian */
        pad[k + (size_t)i] = (uint8_t)(bits >> (h->kind == HASH_MD5 ? 8 * i : 56 - 8 * i));
    hash_update(h, pad, k + 8);
    for (i = 0; i < h->kind / 4; i++) {
        uint32_t v = h->b.h[i];
        if (h->kind == HASH_MD5)
            out[4 * i] = (uint8_t)v, out[4 * i + 1] = (uint8_t)(v >> 8),
            out[4 * i + 2] = (uint8_t)(v >> 16), out[4 * i + 3] = (uint8_t)(v >> 24);
        else
            out[4 * i] = (uint8_t)(v >> 24), out[4 * i + 1] = (uint8_t)(v >> 16),
            out[4 * i + 2] = (uint8_t)(v >> 8), out[4 * i + 3] = (uint8_t)v;
    }
}

/* ---- inflate (RFC 1950/1951), all at once: GOG Galaxy's parts and zlib
 * chunks, whose inflated size the setup says */

typedef struct {
    const uint8_t *in;
    size_t n, at;
    uint32_t bits;
    int nbits;
    uint8_t *out;
    size_t outn, outat;
} Inflate;

#define FAST_BITS 10

typedef struct {
    short count[16];            /* codes of each length */
    short symbol[320];          /* the symbols, by code */
    /* the codes of up to FAST_BITS bits by their next bits (as they come,
     * lowest first): length << 9 | symbol, 0 for a longer code */
    uint16_t fast[1 << FAST_BITS];
} Huffman;

static int bits_of(Inflate *z, int need)
{
    uint32_t v;
    while (z->nbits < need) {
        if (z->at >= z->n)
            return -1;
        z->bits |= (uint32_t)z->in[z->at++] << z->nbits;
        z->nbits += 8;
    }
    v = z->bits & ((1u << need) - 1);
    z->bits >>= need;
    z->nbits -= need;
    return (int)v;
}

/* a canonical code from each symbol's length; 0, or -1 if lengths
 * over-subscribe it */
static int huff_build(Huffman *h, const uint8_t *len, int n)
{
    short offs[16];
    int i, left = 1;

    memset(h->count, 0, sizeof h->count);
    for (i = 0; i < n; i++)
        h->count[len[i]]++;
    h->count[0] = 0;
    for (i = 1; i < 16; i++) {
        left = left * 2 - h->count[i];
        if (left < 0)
            return -1;
    }
    offs[1] = 0;
    for (i = 1; i < 15; i++)
        offs[i + 1] = (short)(offs[i] + h->count[i]);
    for (i = 0; i < n; i++)
        if (len[i])
            h->symbol[offs[len[i]]++] = (short)i;
    memset(h->fast, 0, sizeof h->fast);
    {
        int code = 0, index = 0, l, k;
        for (l = 1; l <= FAST_BITS; l++) {
            for (k = 0; k < h->count[l]; k++) {
                int rev = 0, b, j;
                for (b = 0; b < l; b++)             /* codes are sent from their top bit */
                    rev |= ((code + k) >> b & 1) << (l - 1 - b);
                for (j = rev; j < 1 << FAST_BITS; j += 1 << l)
                    h->fast[j] = (uint16_t)(l << 9 | h->symbol[index + k]);
            }
            index += h->count[l];
            code = (code + h->count[l]) << 1;
        }
    }
    return 0;
}

/* a symbol through `h`, bit by bit (codes are sent from their top bit) */
static int huff_decode(Inflate *z, const Huffman *h)
{
    int code = 0, first = 0, index = 0, len, bit, e;

    while (z->nbits < FAST_BITS && z->at < z->n) {
        z->bits |= (uint32_t)z->in[z->at++] << z->nbits;
        z->nbits += 8;
    }
    e = h->fast[z->bits & ((1u << FAST_BITS) - 1)];
    if (e && e >> 9 <= z->nbits) {             /* a short code, in one step */
        z->bits >>= e >> 9;
        z->nbits -= e >> 9;
        return e & 511;
    }
    for (len = 1; len < 16; len++) {
        if ((bit = bits_of(z, 1)) < 0)
            return -1;
        code |= bit;
        if (code - h->count[len] < first)
            return h->symbol[index + (code - first)];
        index += h->count[len];
        first = (first + h->count[len]) << 1;
        code <<= 1;
    }
    return -1;
}

static int inflate_codes(Inflate *z, const Huffman *lit, const Huffman *dist)
{
    static const short LBASE[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const short LEXTRA[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
                                     3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const short DBASE[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
                                    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
                                    8193, 12289, 16385, 24577};
    static const short DEXTRA[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
                                     7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    for (;;) {
        int sym = huff_decode(z, lit), e;
        size_t len, d;
        if (sym < 0)
            return -1;
        if (sym < 256) {
            if (z->outat >= z->outn)
                return -1;
            z->out[z->outat++] = (uint8_t)sym;
            continue;
        }
        if (sym == 256)
            return 0;
        sym -= 257;
        if (sym >= 29 || (e = bits_of(z, LEXTRA[sym])) < 0)
            return -1;
        len = (size_t)(LBASE[sym] + e);
        sym = huff_decode(z, dist);
        if (sym < 0 || sym >= 30 || (e = bits_of(z, DEXTRA[sym])) < 0)
            return -1;
        d = (size_t)(DBASE[sym] + e);
        if (d > z->outat || len > z->outn - z->outat)
            return -1;
        while (len--) {
            z->out[z->outat] = z->out[z->outat - d];
            z->outat++;
        }
    }
}

static int inflate_dynamic(Inflate *z, Huffman *lit, Huffman *dist)
{
    static const uint8_t ORDER[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    uint8_t len[320];
    Huffman lens;
    int nlen, ndist, ncode, i, sym;

    if ((nlen = bits_of(z, 5)) < 0 || (ndist = bits_of(z, 5)) < 0 || (ncode = bits_of(z, 4)) < 0)
        return -1;
    nlen += 257, ndist += 1, ncode += 4;
    memset(len, 0, sizeof len);
    for (i = 0; i < ncode; i++) {
        int v = bits_of(z, 3);
        if (v < 0)
            return -1;
        len[ORDER[i]] = (uint8_t)v;
    }
    if (huff_build(&lens, len, 19) != 0)
        return -1;
    for (i = 0; i < nlen + ndist;) {
        int rep, v = 0;
        if ((sym = huff_decode(z, &lens)) < 0)
            return -1;
        if (sym < 16) {
            len[i++] = (uint8_t)sym;
            continue;
        }
        if (sym == 16) {
            if (!i || (rep = bits_of(z, 2)) < 0)
                return -1;
            v = len[i - 1], rep += 3;
        } else if (sym == 17) {
            if ((rep = bits_of(z, 3)) < 0)
                return -1;
            rep += 3;
        } else {
            if ((rep = bits_of(z, 7)) < 0)
                return -1;
            rep += 11;
        }
        if (i + rep > nlen + ndist)
            return -1;
        while (rep--)
            len[i++] = (uint8_t)v;
    }
    if (huff_build(lit, len, nlen) != 0 || huff_build(dist, len + nlen, ndist) != 0)
        return -1;
    return inflate_codes(z, lit, dist);
}

/* the zlib stream `in` inflated into `out`, exactly `outn` bytes; 0 or -1 */
static int inflate_zlib(const uint8_t *in, size_t n, uint8_t *out, size_t outn)
{
    Inflate z;
    Huffman lit, dist;
    uint8_t len[320];
    int last, type, i;

    if (n < 2 || (in[0] & 0x0F) != 8 || ((in[0] << 8) | in[1]) % 31 != 0 || (in[1] & 0x20))
        return -1;
    memset(&z, 0, sizeof z);
    z.in = in, z.n = n, z.at = 2, z.out = out, z.outn = outn;
    do {
        if ((last = bits_of(&z, 1)) < 0 || (type = bits_of(&z, 2)) < 0)
            return -1;
        if (type == 0) {                /* stored */
            size_t k;
            z.bits = 0, z.nbits = 0;
            if (z.at + 4 > z.n)
                return -1;
            k = (size_t)(z.in[z.at] | z.in[z.at + 1] << 8);
            if ((k ^ 0xFFFF) != (size_t)(z.in[z.at + 2] | z.in[z.at + 3] << 8))
                return -1;
            z.at += 4;
            if (k > z.n - z.at || k > z.outn - z.outat)
                return -1;
            memcpy(z.out + z.outat, z.in + z.at, k);
            z.at += k, z.outat += k;
        } else if (type == 1) {         /* the fixed codes */
            for (i = 0; i < 288; i++)
                len[i] = (uint8_t)(i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8);
            huff_build(&lit, len, 288);
            for (i = 0; i < 30; i++)
                len[i] = 5;
            huff_build(&dist, len, 30);
            if (inflate_codes(&z, &lit, &dist) != 0)
                return -1;
        } else if (type == 2) {
            if (inflate_dynamic(&z, &lit, &dist) != 0)
                return -1;
        } else
            return -1;
    } while (!last);
    return z.outat == outn ? 0 : -1;
}

/* ---- the header: fields read from a decompressed block */

typedef struct {
    const uint8_t *p;
    size_t n, at;
    int bad, unicode;
} Rd;

static const uint8_t *take(Rd *r, size_t k)
{
    const uint8_t *q;
    if (r->bad || k > r->n - r->at) {
        r->bad = 1;
        return NULL;
    }
    q = r->p + r->at;
    r->at += k;
    return q;
}

static uint32_t rd8(Rd *r)
{
    const uint8_t *q = take(r, 1);
    return q ? q[0] : 0;
}

static uint32_t rd32(Rd *r)
{
    const uint8_t *q = take(r, 4);
    return q ? le32(q) : 0;
}

static uint64_t rd64(Rd *r)
{
    const uint8_t *q = take(r, 8);
    return q ? le64(q) : 0;
}

static void skip_strings(Rd *r, int k)
{
    while (k-- > 0)
        take(r, rd32(r));
}

/* a string as UTF-8 into out (UTF-16 in a Unicode setup, else Windows'
 * Latin-1 as far as it goes) */
static void rd_text(Rd *r, char *out, size_t n)
{
    uint32_t len = rd32(r);
    const uint8_t *q = take(r, len);
    size_t o = 0, i;

    out[0] = 0;
    if (!q)
        return;
    for (i = 0; i < len;) {
        uint32_t c;
        if (r->unicode) {
            if (i + 1 >= len)
                break;
            c = (uint32_t)(q[i] | q[i + 1] << 8), i += 2;
            if (c >= 0xD800 && c < 0xDC00 && i + 1 < len) {
                uint32_t lo = (uint32_t)(q[i] | q[i + 1] << 8);
                if (lo >= 0xDC00 && lo < 0xE000)
                    c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00), i += 2;
            }
        } else
            c = q[i++];
        if (o + 5 > n)
            break;
        if (c < 0x80)
            out[o++] = (char)c;
        else if (c < 0x800)
            out[o++] = (char)(0xC0 | c >> 6), out[o++] = (char)(0x80 | (c & 0x3F));
        else if (c < 0x10000)
            out[o++] = (char)(0xE0 | c >> 12), out[o++] = (char)(0x80 | ((c >> 6) & 0x3F)),
            out[o++] = (char)(0x80 | (c & 0x3F));
        else
            out[o++] = (char)(0xF0 | c >> 18), out[o++] = (char)(0x80 | ((c >> 12) & 0x3F)),
            out[o++] = (char)(0x80 | ((c >> 6) & 0x3F)), out[o++] = (char)(0x80 | (c & 0x3F));
    }
    out[o] = 0;
}

/* the bytes of a set of n flags: one per 8, three made four (Delphi's sets) */
static size_t flag_bytes(int n)
{
    size_t b = (size_t)(n + 7) / 8;
    return b == 3 ? 4 : b;
}

static int header_flag_count(uint32_t v, int u)
{
    int new64 = v >= V4(6, 4, 0, 1);
    return 1 + (v < V(5, 3, 10)) + 1 + 2 * (v < V(5, 3, 3)) + 3 + 4 * !new64 + 1 + 1
           + 2 + (v < V(5, 6, 1)) + (v < V(5, 3, 8)) + 1 + !new64 + 1 + 1 + 1 + 6 + 2
           + 1 + 2 + 1 + 1 + 1 + 1 + 1 + 2 + 1 + (v >= V(5, 0, 4) && v < V(5, 6, 1)) + !u
           + 1 + 1 + (v >= V(5, 3, 8)) + (v >= V(5, 3, 9)) + 3 * (v >= V(5, 5, 0))
           + (v >= V(5, 5, 7)) + 3 * (v >= V(6, 0, 0)) + (v >= V(6, 3, 0));
}

typedef struct {
    uint32_t first_slice, chunk_offset;
    uint64_t offset, size, chunk_size;
    uint8_t digest[32];
    int hash, exe_filter, encrypted, compression, flip;
    int uses;                   /* files unpacking it */
} Data;

typedef struct {
    char *path;                 /* in the game's folder, with / */
    char md5[33];               /* a Galaxy file's MD5, else "" */
    char *langs;                /* check_if_install's languages, "en-US#de-DE#", or NULL */
    int *parts;                 /* data entries */
    uint64_t *part_size;        /* each part inflated (Galaxy), else the data's size */
    int nparts, wanted, done;
    uint64_t size;
    Hash whole;                 /* a Galaxy file's MD5 as its parts come */
} File;

typedef struct {
    char path[SYS_PATH];
    char name[256], app_id[256];
    uint32_t version;
    int unicode, compression, per_disk;
    uint64_t data_offset;
    Data *data;
    int ndata;
    File *files;
    int nfiles;
    char *err;
    size_t n;
} Setup;

static int fail(Setup *s, const char *what)
{
    snprintf(s->err, s->n, "%s", what);
    return -1;
}

/* the loader's offset table in the first bytes of the .exe: the header's
 * and the data's offsets; 0, or -1 if there is none */
static int find_offsets(FILE *f, uint64_t *header, uint64_t *data)
{
    uint8_t *buf = malloc((size_t)SEARCHED);
    size_t n, i;
    int m, r = -1;

    if (!buf)
        return -1;
    n = fread(buf, 1, (size_t)SEARCHED, f);
    for (i = 0; r != 0 && i + 44 <= n; i++)
        for (m = 0; m < 2; m++)
            if (!memcmp(buf + i, LOADER_MAGIC[m], 12) && crc32_of(buf + i, 40) == le32(buf + i + 40)) {
                *header = le32(buf + i + 32);
                *data = le32(buf + i + 36);
                r = 0;
                break;
            }
    if (r != 0 && n >= 22 && !memcmp(buf, "Inno Setup Setup Data ", 22)) {
        *header = *data = 0;            /* a setup-0.bin: the header alone */
        r = 0;
    }
    free(buf);
    return r;
}

/* "Inno Setup Setup Data (5.6.2) (u)": the version, 0 if not one read here */
static uint32_t parse_version(const char *t, int *unicode)
{
    int a, b, c, d = 0;
    const char *p;
    uint32_t v;

    if (strncmp(t, "Inno Setup Setup Data (", 23) != 0 || sscanf(t + 23, "%d.%d.%d", &a, &b, &c) != 3)
        return 0;
    p = strchr(t + 23, ')');
    if (!p)
        return 0;
    if (p - (t + 23) > 0 && sscanf(t + 23, "%*d.%*d.%*d.%d", &d) != 1)
        d = 0;
    v = V4(a, b, c, d);
    *unicode = strstr(p, "(u)") || strstr(p, "(U)") || v >= V(6, 3, 0);
    return v >= V(5, 2, 0) && v < V(7, 0, 0) ? v : 0;
}

static void *sz_alloc(ISzAllocPtr p, size_t size)
{
    (void)p;
    return malloc(size);
}

static void sz_free(ISzAllocPtr p, void *a)
{
    (void)p;
    free(a);
}

static const ISzAlloc ALLOC = {sz_alloc, sz_free};

/* the header block at f's position, decompressed into *out; 0 or -1 */
static int read_block(Setup *s, FILE *f, uint8_t **out, size_t *outn)
{
    uint8_t head[9], *raw, *plain = NULL;
    uint32_t size;
    size_t n = 0, at, cap = 0;
    int r = -1;

    if (fread(head, 1, 9, f) != 9 || crc32_of(head + 4, 5) != le32(head))
        return fail(s, "The installer's header is damaged.");
    size = le32(head + 4);
    raw = malloc(size ? size : 1);
    if (!raw || fread(raw, 1, size, f) != size) {
        free(raw);
        return fail(s, "The installer's header cannot be read.");
    }
    for (at = 0; at < size;) {                  /* 4096-byte chunks, each behind its CRC32 */
        size_t k = size - at - 4 < 4096 ? size - at - 4 : 4096;
        if (size - at < 5 || crc32_of(raw + at + 4, k) != le32(raw + at)) {
            free(raw);
            return fail(s, "The installer's header is damaged.");
        }
        memmove(raw + n, raw + at + 4, k);
        n += k, at += 4 + k;
    }
    if (!head[8]) {
        *out = raw, *outn = n;
        return 0;
    }
    if (n >= 5) {                               /* LZMA: 5 bytes of properties, the stream */
        CLzmaDec dec;
        size_t in_at = 5;
        LzmaDec_Construct(&dec);
        if (LzmaDec_Allocate(&dec, raw, 5, &ALLOC) == SZ_OK) {
            LzmaDec_Init(&dec);
            *outn = 0;
            for (;;) {
                SizeT dl, sl = n - in_at;
                ELzmaStatus st;
                if (*outn == cap) {
                    uint8_t *p = realloc(plain, cap = cap ? cap * 2 : 1 << 20);
                    if (!p)
                        break;
                    plain = p;
                }
                dl = cap - *outn;
                if (LzmaDec_DecodeToBuf(&dec, plain + *outn, &dl, raw + in_at, &sl, LZMA_FINISH_ANY,
                                        &st) != SZ_OK)
                    break;
                *outn += dl, in_at += sl;
                if (st == LZMA_STATUS_FINISHED_WITH_MARK || (in_at == n && !dl)) {
                    r = 0;
                    break;
                }
                if (!dl && !sl && *outn < cap)
                    break;
            }
            LzmaDec_Free(&dec, &ALLOC);
        }
    }
    free(raw);
    if (r != 0) {
        free(plain);
        return fail(s, "The installer's header cannot be decompressed.");
    }
    *out = plain;
    return 0;
}

/* the setup's settings up to its entries; the entry counts into counts */
static void skip_header(Rd *r, uint32_t v, char *name, char *app_id, size_t n, uint32_t *counts,
                        int *compression, int *per_disk)
{
    int i, u = r->unicode;

    rd_text(r, name, n);
    skip_strings(r, 1);                         /* versioned name */
    rd_text(r, app_id, n);                      /* GOG's product ID in GOG's setups */
    skip_strings(r, 9 + 1);                     /* copyright .. default group, base filename */
    if (v < V(5, 2, 5))
        skip_strings(r, 3);                     /* licence, info before, after */
    skip_strings(r, 7);                         /* uninstall files dir .. default serial */
    if (v < V(5, 2, 5))
        skip_strings(r, 1);                     /* compiled code */
    skip_strings(r, 4 + (v >= V(5, 3, 8)) + (v >= V(5, 3, 10)) + (v >= V(5, 5, 0))
                 + (v >= V(5, 5, 6)) + 2 * (v >= V(5, 6, 1)) + 2 * (v >= V(6, 3, 0)));
    if (v >= V(5, 2, 5))
        skip_strings(r, 3);
    if (v >= V(5, 2, 1) && v < V(5, 3, 10))
        skip_strings(r, 1);                     /* uninstaller signature */
    if (v >= V(5, 2, 5))
        skip_strings(r, 1);                     /* compiled code */
    if (!u)
        take(r, 32);                            /* lead bytes */
    for (i = 0; i < 16; i++)
        if ((counts[i] = rd32(r)) > r->n)
            r->bad = 1;                         /* more entries than bytes: damaged */
    take(r, 20);                                /* Windows versions */
    take(r, (size_t)(8 * (v < V4(6, 4, 0, 1)) + 4 * (v < V(5, 5, 7))));   /* colours */
    if (v >= V(6, 0, 0))
        take(r, 9);                             /* wizard style, resize percentages */
    if (v >= V(5, 5, 7))
        take(r, 1);                             /* image alpha format */
    take(r, v >= V(6, 4, 0) ? 4 : v >= V(5, 3, 9) ? 20 : 16);   /* password hash */
    take(r, v >= V(6, 4, 0) ? 44 : 8);          /* salt */
    take(r, 8);                                 /* extra disk space */
    *per_disk = (int)rd32(r);
    take(r, 3);                                 /* uninstall log mode, dir exists warning, privileges */
    if (v >= V(5, 7, 0))
        take(r, 1);
    take(r, 2);                                 /* language dialog, detection */
    *compression = (int)rd8(r);
    if (v < V(6, 3, 0))
        take(r, 2);                             /* architectures */
    if (v >= V(5, 2, 1) && v < V(5, 3, 10))
        take(r, 8);
    if (v >= V(5, 3, 3))
        take(r, 2);
    take(r, v >= V(5, 5, 0) ? 8 : v >= V(5, 3, 6) ? 4 : 0);    /* uninstall display size */
    take(r, flag_bytes(header_flag_count(v, u)));
}

/* languages .. directories, up to the files */
static void skip_entries(Rd *r, uint32_t v, const uint32_t *counts)
{
    uint32_t i;
    for (i = 0; !r->bad && i < counts[0]; i++) {           /* languages */
        skip_strings(r, 10);
        take(r, 4 + 4 * (!r->unicode || v < V(5, 3, 0)) + 16 + (v >= V(5, 2, 3)));
    }
    for (i = 0; !r->bad && i < counts[1]; i++)             /* messages */
        skip_strings(r, 2), take(r, 4);
    for (i = 0; !r->bad && i < counts[2]; i++)             /* permissions */
        skip_strings(r, 1);
    for (i = 0; !r->bad && i < counts[3]; i++)             /* types */
        skip_strings(r, 4), take(r, 20 + 1 + 1 + 8);
    for (i = 0; !r->bad && i < counts[4]; i++)             /* components */
        skip_strings(r, 5), take(r, 8 + 4 + 1 + 20 + 1 + 8);
    for (i = 0; !r->bad && i < counts[5]; i++)             /* tasks */
        skip_strings(r, 6), take(r, 4 + 1 + 20 + 1);
    for (i = 0; !r->bad && i < counts[6]; i++)             /* directories */
        skip_strings(r, 7), take(r, 4 + 20 + 2 + 1);
}

static void read_data(Rd *r, uint32_t v, int compression, Data *d)
{
    uint32_t flags;
    int nflags = 9 + 2 * (v >= V(5, 5, 7) && v < V(6, 3, 0));
    size_t k;
    const uint8_t *q;

    memset(d, 0, sizeof *d);
    d->first_slice = rd32(r);
    rd32(r);
    d->chunk_offset = rd32(r);
    d->offset = rd64(r), d->size = rd64(r), d->chunk_size = rd64(r);
    d->hash = v >= V(6, 4, 0) ? HASH_SHA256 : v >= V(5, 3, 9) ? HASH_SHA1 : HASH_MD5;
    if ((q = take(r, (size_t)d->hash)) != NULL)
        memcpy(d->digest, q, (size_t)d->hash);
    take(r, 16);                                /* time, version */
    k = flag_bytes(nflags);
    q = take(r, k);
    flags = q ? (k > 1 ? (uint32_t)(q[0] | q[1] << 8) : q[0]) : 0;
    if (v >= V(6, 3, 0))
        take(r, 1);                             /* sign mode */
    d->exe_filter = (flags >> 4) & 1;
    d->encrypted = (flags >> 6) & 1;
    d->compression = (flags >> 7) & 1 ? compression : STORED;
    d->flip = v >= V(5, 3, 9);
}

/* ---- GOG's Galaxy installers: the game's files under {tmp}, named by
 * their MD5, each in deflated parts, the real name in Pascal calls */

/* the arguments of `name('a', 12, 'b''c')` in `code` into args; their
 * count, -1 if code is not that call */
static int call_args(const char *code, const char *name, char args[MAX_ARGS][ARG_LEN])
{
    size_t len = strlen(name);
    int n = 0;

    while (*code == ' ')
        code++;
    if (strncmp(code, name, len) != 0)
        return -1;
    code += len;
    while (*code == ' ')
        code++;
    if (*code++ != '(')
        return -1;
    for (;;) {
        size_t o = 0;
        while (*code == ' ')
            code++;
        if (*code == ')' && n == 0)
            return 0;
        if (n == MAX_ARGS)
            return n;
        if (*code == '\'') {
            for (code++; *code; code++) {
                if (*code == '\'') {
                    if (code[1] != '\'')
                        break;
                    code++;
                }
                if (o + 1 < ARG_LEN)
                    args[n][o++] = *code;
            }
            if (*code++ != '\'')
                return -1;
        } else
            while (*code && *code != ',' && *code != ')' && *code != ' ')
                if (o + 1 < ARG_LEN)
                    args[n][o++] = *code++;
                else
                    code++;
        args[n++][o] = 0;
        while (*code == ' ')
            code++;
        if (*code == ',')
            code++;
        else
            return *code == ')' ? n : -1;
    }
}

static char *dup(const char *t)
{
    char *p = malloc(strlen(t) + 1);
    if (p)
        strcpy(p, t);
    return p;
}

static File *add_file(Setup *s, const char *path)
{
    File *f, *files = realloc(s->files, sizeof *files * (size_t)(s->nfiles + 1));
    char *p;

    if (!files)
        return NULL;
    s->files = files;
    f = &files[s->nfiles];
    memset(f, 0, sizeof *f);
    if (!(f->path = dup(path)))
        return NULL;
    for (p = f->path; *p; p++)
        if (*p == '\\')
            *p = '/';
    s->nfiles++;
    return f;
}

static int add_part(File *f, int data, uint64_t size)
{
    int *p = realloc(f->parts, sizeof *p * (size_t)(f->nparts + 1));
    uint64_t *q;
    if (!p)
        return -1;
    f->parts = p;
    q = realloc(f->part_size, sizeof *q * (size_t)(f->nparts + 1));
    if (!q)
        return -1;
    f->part_size = q;
    f->parts[f->nparts] = data;
    f->part_size[f->nparts++] = size;
    f->size += size;
    return 0;
}

/* 1 if dest is in {app}, the game's folder ("{app}\\...", any case) */
static int is_app(const char *dest)
{
    static const char app[] = "{app}\\";
    int i;
    for (i = 0; i < 6; i++)
        if (!dest[i] || (dest[i] | 0x20) != (app[i] | 0x20))
            return 0;
    return dest[6] != 0;
}

/* the Galaxy file whose parts are being read: its index in s->files (-1:
 * a dependency, not kept), the parts still to come */
typedef struct {
    int file, left;
} Galaxy;

/* one [Files] entry: a file into {app} as it is, or a Galaxy part */
static int read_entry(Setup *s, Rd *r, Galaxy *g)
{
    static char dest[SYS_PATH], check[ARG_LEN * 2], after[ARG_LEN * 2], before[ARG_LEN * 2];
    char args[MAX_ARGS][ARG_LEN];
    uint32_t v = s->version, loc;
    int n;

    skip_strings(r, 1);                         /* source */
    rd_text(r, dest, sizeof dest);
    skip_strings(r, 1 + (v >= V(5, 2, 5)) + 3); /* font, assembly; components, tasks, languages */
    rd_text(r, check, sizeof check);
    rd_text(r, after, sizeof after);
    rd_text(r, before, sizeof before);
    take(r, 20);
    loc = rd32(r);
    take(r, 4 + 8 + 2 + flag_bytes(31 + (v >= V(5, 2, 5))) + 1);
    if (r->bad)
        return -1;

    if (is_app(dest) && (int)loc < s->ndata) {
        File *f = add_file(s, dest + 6);
        if (!f || add_part(f, (int)loc, s->data[loc].size) != 0)
            return -1;
    }
    if ((n = call_args(before, "before_install", args)) >= 2 ||
        (n = call_args(before, "before_install_dependency", args)) >= 2) {
        char cargs[MAX_ARGS][ARG_LEN];
        if (g->left)
            return fail(s, "The installer's file list is not complete.");
        g->left = n > 2 && atoi(args[2]) > 0 ? atoi(args[2]) : 1;
        g->file = -1;                           /* GOG's own dependencies: counted, not kept */
        if (strncmp(before + strspn(before, " "), "before_install_dependency", 25) != 0) {
            File *f = add_file(s, args[1]);
            if (!f)
                return -1;
            snprintf(f->md5, sizeof f->md5, "%.32s", args[0]);
            if (call_args(check, "check_if_install", cargs) >= 1 && cargs[0][0])
                f->langs = dup(cargs[0]);
            g->file = s->nfiles - 1;            /* an index: s->files moves as it grows */
        }
    }
    if (g->left && ((n = call_args(after, "after_install", args)) >= 3 ||
                    (n = call_args(after, "after_install_dependency", args)) >= 3)) {
        if (g->file >= 0 && (int)loc < s->ndata &&
            add_part(&s->files[g->file], (int)loc, (uint64_t)strtoull(args[2], NULL, 10)) != 0)
            return -1;
        g->left--;
    }
    return 0;
}

/* ---- languages: GOG's Galaxy installers carry a file per language */

/* 1 if `lang` is in the list "en-US#de-DE#" (negated: "!en-US#") */
static int lang_in(const char *list, const char *lang, int negated)
{
    size_t len = strlen(lang);
    const char *p = list;
    while (p && *p) {
        const char *e = strchr(p, '#'), *q = p;
        size_t k;
        while (*q == ' ')
            q++;
        k = (size_t)((e ? e : q + strlen(q)) - q);
        if ((*q == '!') == negated && k == len + (size_t)negated && !strncmp(q + negated, lang, len))
            return 1;
        p = e ? e + 1 : NULL;
    }
    return 0;
}

static int wanted(const File *f, const char *lang)
{
    int any_yes = 0;
    const char *p;
    if (!f->langs)
        return 1;
    for (p = f->langs; *p; p++)
        if ((p == f->langs || p[-1] == '#') && *p != '!' && *p != '#')
            any_yes = 1;
    return !lang_in(f->langs, lang, 1) && (!any_yes || lang_in(f->langs, lang, 0));
}

/* en-US if the setup has it, else the first there is (in the order of
 * its file list) */
static void pick_language(Setup *s)
{
    char lang[32] = "";
    int i;

    for (i = 0; i < s->nfiles && strcmp(lang, "en-US"); i++) {
        const char *p = s->files[i].langs;
        if (p && lang_in(p, "en-US", 0))
            strcpy(lang, "en-US");
        else if (p && !lang[0] && *p != '!') {
            size_t k = strcspn(p, "#");
            snprintf(lang, sizeof lang, "%.*s", (int)(k < 31 ? k : 31), p);
        }
    }
    for (i = 0; i < s->nfiles; i++)
        s->files[i].wanted = wanted(&s->files[i], lang);
}

/* ---- the setup read */

static void free_setup(Setup *s)
{
    int i;
    for (i = 0; i < s->nfiles; i++) {
        free(s->files[i].path);
        free(s->files[i].langs);
        free(s->files[i].parts);
        free(s->files[i].part_size);
    }
    free(s->files);
    free(s->data);
    s->files = NULL, s->data = NULL, s->nfiles = s->ndata = 0;
}

static int load_setup(Setup *s, const char *path)
{
    FILE *f = fopen(path, "rb");
    uint64_t header, data;
    char version[65];
    uint8_t *main_b = NULL, *second = NULL;
    size_t main_n, second_n;
    uint32_t counts[16], i;
    Rd r;
    Galaxy galaxy = {-1, 0};
    int ok = -1;

    snprintf(s->path, sizeof s->path, "%s", path);
    if (!f)
        return fail(s, "The installer cannot be opened.");
    if (find_offsets(f, &header, &data) != 0) {
        fclose(f);
        return fail(s, "This is not an Inno Setup installer (GOG's setup_*.exe).");
    }
    memset(version, 0, sizeof version);
    if (SEEK64(f, header) != 0 || fread(version, 1, 64, f) != 64 ||
        !(s->version = parse_version(version, &s->unicode))) {
        fclose(f);
        return fail(s, "The installer's version of Inno Setup is not one read here.");
    }
    s->data_offset = data;
    if (read_block(s, f, &main_b, &main_n) != 0 || read_block(s, f, &second, &second_n) != 0)
        goto done;
    memset(&r, 0, sizeof r);
    r.p = main_b, r.n = main_n, r.unicode = s->unicode;
    skip_header(&r, s->version, s->name, s->app_id, sizeof s->name, counts, &s->compression,
                &s->per_disk);
    skip_entries(&r, s->version, counts);
    if (r.bad || counts[8] > 1000000 || counts[7] > 1000000) {
        fail(s, "The installer's header is not as expected.");
        goto done;
    }
    {                                           /* the data entries first: files refer to them */
        Rd r2;
        memset(&r2, 0, sizeof r2);
        r2.p = second, r2.n = second_n, r2.unicode = s->unicode;
        s->data = calloc(counts[8] ? counts[8] : 1, sizeof *s->data);
        if (!s->data)
            goto done;
        s->ndata = (int)counts[8];
        for (i = 0; i < counts[8]; i++)
            read_data(&r2, s->version, s->compression, &s->data[i]);
        if (r2.bad) {
            fail(s, "The installer's header is not as expected.");
            goto done;
        }
    }
    s->err[0] = 0;
    for (i = 0; i < counts[7]; i++)
        if (read_entry(s, &r, &galaxy) != 0) {
            if (!s->err[0])
                fail(s, "The installer's file list cannot be read.");
            goto done;
        }
    if (galaxy.left) {
        fail(s, "The installer's file list is not complete.");
        goto done;
    }
    if (s->per_disk < 1)
        s->per_disk = 1;
    pick_language(s);
    ok = 0;
done:
    free(main_b);
    free(second);
    fclose(f);
    if (ok != 0)
        free_setup(s);
    return ok;
}

int inno_is_setup(const char *path)
{
    FILE *f = fopen(path, "rb");
    uint64_t header, data;
    char version[65];
    int u, r = 0;

    if (!f)
        return 0;
    memset(version, 0, sizeof version);
    if (find_offsets(f, &header, &data) == 0 && SEEK64(f, header) == 0 &&
        fread(version, 1, 64, f) == 64)
        r = parse_version(version, &u) != 0;
    fclose(f);
    return r;
}

/* ---- the data: slices, chunks */

typedef struct {
    Setup *s;
    FILE *f;
    uint32_t slice;
    uint64_t pos, end;          /* in the slice's file */
    uint64_t left;              /* compressed bytes of the chunk not yet read */
    int method, started;
    CLzmaDec lz;
    CLzma2Dec lz2;
    uint8_t in[PIECE];
    size_t in_at, in_n;
    uint8_t *whole;             /* zlib: the chunk inflated at once */
    size_t whole_n, whole_at;
    uint64_t out_pos;           /* decompressed bytes given so far */
} Chunk;

typedef struct {
    Setup *s;
    const char *want;
    char found[SYS_PATH];
} BinFind;

/* slice `k`'s file: the .exe itself (its data at data_offset), or
 * SETUP-1.bin, SETUP-2.bin ... (SETUP-1a.bin with several per disk) */
static int open_slice(Chunk *c, uint32_t k)
{
    Setup *s = c->s;
    char folder[SYS_PATH], stem[SYS_PATH], name[SYS_PATH + 32], path[SYS_PATH];
    const char *base;
    uint8_t head[12];
    size_t len;

    if (c->f)
        fclose(c->f);
    c->f = NULL;
    c->slice = k;
    if (s->data_offset) {
        if (k != 0)
            return fail(s, "The installer's data ends early.");
        if (!(c->f = fopen(s->path, "rb")) || fseek(c->f, 0, SEEK_END) != 0)
            return fail(s, "The installer cannot be read.");
#ifdef _WIN32
        c->end = (uint64_t)_ftelli64(c->f);
#else
        c->end = (uint64_t)ftello(c->f);
#endif
        return 0;
    }
    if (!sys_parent(s->path, folder, sizeof folder))
        folder[0] = 0;
    base = s->path + strlen(folder);
    base += (*base == '/' || *base == '\\');
    snprintf(stem, sizeof stem, "%s", base);
    len = strlen(stem);
    if (len > 4 && !sys_stricmp(stem + len - 4, ".exe"))
        stem[len - 4] = 0;
    else if (len > 6 && !sys_stricmp(stem + len - 6, "-0.bin"))
        stem[len - 6] = 0;
    if (s->per_disk == 1)
        snprintf(name, sizeof name, "%s-%u.bin", stem, (unsigned)k + 1);
    else
        snprintf(name, sizeof name, "%s-%u%c.bin", stem, (unsigned)(k / (uint32_t)s->per_disk) + 1,
                 (char)('a' + k % (uint32_t)s->per_disk));
    if (!sys_find(folder[0] ? folder : ".", name, path, sizeof path)) {
        snprintf(s->err, s->n, "%s is missing beside the installer.", name);
        return -1;
    }
    if (!(c->f = fopen(path, "rb")) || fread(head, 1, 12, c->f) != 12 ||
        (memcmp(head, "idska32\x1a", 8) != 0 && memcmp(head, "idska16\x1a", 8) != 0)) {
        snprintf(s->err, s->n, "%s is not one of the installer's files.", name);
        return -1;
    }
    c->end = le32(head + 8);
    return 0;
}

/* up to n bytes of the chunk's compressed data, across slices */
static size_t raw_read(Chunk *c, uint8_t *buf, size_t n)
{
    size_t got = 0;
    if (n > c->left)
        n = (size_t)c->left;
    while (got < n) {
        size_t k;
        if (c->pos >= c->end) {
            if (open_slice(c, c->slice + 1) != 0)
                return got;
            c->pos = 12;
        }
        k = n - got < c->end - c->pos ? n - got : (size_t)(c->end - c->pos);
        if (SEEK64(c->f, c->pos) != 0 || fread(buf + got, 1, k, c->f) != k) {
            fail(c->s, "The installer's data cannot be read.");
            return got;
        }
        got += k, c->pos += k, c->left -= k;
    }
    return got;
}

static int refill(Chunk *c)
{
    if (c->in_at < c->in_n)
        return 0;
    c->in_at = 0;
    c->in_n = raw_read(c, c->in, sizeof c->in);
    return c->in_n ? 0 : -1;
}

/* the chunk of data entry d opened: its "zlb\x1a", its decompressor;
 * `need` its inflated bytes used (for zlib, inflated at once) */
static int chunk_open(Chunk *c, Setup *s, const Data *d, uint64_t need)
{
    uint8_t magic[4];

    memset(c, 0, sizeof *c);
    c->s = s;
    c->method = d->compression;
    if (d->encrypted)
        return fail(s, "The installer is encrypted; it cannot be unpacked here.");
    if (c->method == BZIP2)
        return fail(s, "The installer uses bzip2; it cannot be unpacked here.");
    if (open_slice(c, d->first_slice) != 0)
        return -1;
    c->pos = (s->data_offset ? s->data_offset : 0) + d->chunk_offset;
    c->left = 4 + d->chunk_size;
    if (raw_read(c, magic, 4) != 4 || memcmp(magic, "zlb\x1a", 4) != 0)
        return fail(s, "The installer's data is not where its header says.");
    if (c->method == LZMA1 || c->method == LZMA2) {
        uint8_t props[5];
        size_t k = c->method == LZMA1 ? 5 : 1;
        if (raw_read(c, props, k) != k)
            return fail(s, "The installer's data ends early.");
        if (c->method == LZMA1) {
            LzmaDec_Construct(&c->lz);
            if (LzmaDec_Allocate(&c->lz, props, 5, &ALLOC) != SZ_OK)
                return fail(s, "Not enough memory to unpack the installer.");
            LzmaDec_Init(&c->lz);
        } else {
            Lzma2Dec_CONSTRUCT(&c->lz2);
            if (Lzma2Dec_Allocate(&c->lz2, props[0], &ALLOC) != SZ_OK)
                return fail(s, "Not enough memory to unpack the installer.");
            Lzma2Dec_Init(&c->lz2);
        }
        c->started = 1;
    } else if (c->method == ZLIB) {
        size_t n = (size_t)d->chunk_size;
        uint8_t *packed;
        if (d->chunk_size > IN_MEMORY || need > IN_MEMORY)
            return fail(s, "The installer's data is too large to unpack here.");
        packed = malloc(n ? n : 1);
        c->whole = malloc(need ? (size_t)need : 1);
        if (!packed || !c->whole) {
            free(packed);
            return fail(s, "Not enough memory to unpack the installer.");
        }
        if (raw_read(c, packed, n) != n || inflate_zlib(packed, n, c->whole, (size_t)need) != 0) {
            free(packed);
            return fail(s, "The installer's data is damaged.");
        }
        free(packed);
        c->whole_n = (size_t)need;
    }
    return 0;
}

static void chunk_close(Chunk *c)
{
    if (c->started && c->method == LZMA1)
        LzmaDec_Free(&c->lz, &ALLOC);
    if (c->started && c->method == LZMA2)
        Lzma2Dec_Free(&c->lz2, &ALLOC);
    free(c->whole);
    if (c->f)
        fclose(c->f);
    memset(c, 0, sizeof *c);
}

/* exactly n inflated bytes of the chunk into buf; 0 or -1 */
static int chunk_read(Chunk *c, uint8_t *buf, size_t n)
{
    size_t got = 0;
    if (c->method == STORED) {
        if (raw_read(c, buf, n) != n)
            return fail(c->s, "The installer's data ends early.");
    } else if (c->method == ZLIB) {
        if (n > c->whole_n - c->whole_at)
            return fail(c->s, "The installer's data ends early.");
        memcpy(buf, c->whole + c->whole_at, n);
        c->whole_at += n;
    } else
        while (got < n) {
            SizeT dl = n - got, sl;
            ELzmaStatus st;
            SRes r;
            if (refill(c) != 0)
                return fail(c->s, "The installer's data ends early.");
            sl = c->in_n - c->in_at;
            if (c->method == LZMA1)
                r = LzmaDec_DecodeToBuf(&c->lz, buf + got, &dl, c->in + c->in_at, &sl,
                                        LZMA_FINISH_ANY, &st);
            else
                r = Lzma2Dec_DecodeToBuf(&c->lz2, buf + got, &dl, c->in + c->in_at, &sl,
                                         LZMA_FINISH_ANY, &st);
            if (r != SZ_OK || (!dl && !sl))
                return fail(c->s, "The installer's data is damaged.");
            got += dl, c->in_at += sl;
        }
    c->out_pos += n;
    return 0;
}

static int chunk_skip(Chunk *c, uint64_t n)
{
    uint8_t buf[PIECE];
    while (n) {
        size_t k = n < sizeof buf ? (size_t)n : sizeof buf;
        if (chunk_read(c, buf, k) != 0)
            return -1;
        n -= k;
    }
    return 0;
}

/* undo Inno's x86 CALL/JMP filter (5.2.0 and later): the 24-bit targets
 * after E8/E9 were made absolute; 64 KB blocks, an instruction across a
 * block's end left alone */
static void call_filter(uint8_t *b, size_t n, int flip)
{
    size_t i = 0;
    while (i < n) {
        uint8_t c = b[i++];
        if ((c != 0xE8 && c != 0xE9) || 0x10000 - ((i - 1) % 0x10000) < 5)
            continue;
        if (i + 4 > n)
            break;
        if (b[i + 3] == 0 || b[i + 3] == 0xFF) {
            uint32_t addr = (uint32_t)(i + 4) & 0xFFFFFF;
            uint32_t rel = ((uint32_t)b[i] | (uint32_t)b[i + 1] << 8 | (uint32_t)b[i + 2] << 16) - addr;
            b[i] = (uint8_t)rel, b[i + 1] = (uint8_t)(rel >> 8), b[i + 2] = (uint8_t)(rel >> 16);
            if (flip && (rel & 0x800000))
                b[i + 3] = (uint8_t)~b[i + 3];
        }
        i += 4;
    }
}

/* ---- writing */

typedef struct {
    Setup *s;
    const char *dir;            /* where the files go (`dir`.part) */
    uint64_t total, done;
    int (*progress)(void *ctx, const char *file, long done, long total);
    void *ctx;
} Out;

/* a path in the setup safe to write below a folder: no .., no drive,
 * not absolute */
static int safe_path(const char *p)
{
    const char *q = p;
    if (!*p || *p == '/' || strchr(p, ':'))
        return 0;
    while (*q) {
        size_t k = strcspn(q, "/");
        if (!k || (k == 2 && !strncmp(q, "..", 2)) || (k == 1 && *q == '.'))
            return 0;
        q += k + (q[k] == '/');
    }
    return 1;
}

/* the file's path below o->dir, its folders made */
static int out_path(Out *o, const char *rel, char *path, size_t n)
{
    char part[SYS_PATH];
    const char *q = rel;

    snprintf(path, n, "%s", o->dir);
    while (*q) {
        size_t k = strcspn(q, "/");
        snprintf(part, sizeof part, "%.*s", (int)k, q);
        sys_join(path, n, path, part);
        q += k;
        if (*q == '/') {
            q++;
            if (!sys_is_dir(path) && sys_mkdir(path) != 0)
                return fail(o->s, "A folder for the game's files cannot be made.");
        }
    }
    return 0;
}

/* bytes as progress's long; the total may be anything a damaged setup says */
static long as_long(uint64_t v)
{
    return v > 0x7FFFFFFF ? 0x7FFFFFFFL : (long)v;
}

static int step(Out *o, const File *f, uint64_t bytes)
{
    o->done += bytes;
    if (o->progress && o->progress(o->ctx, f->path, as_long(o->done), as_long(o->total)))
        return fail(o->s, "Stopped.");
    return 0;
}

/* data entry d's bytes, all of them in memory, given to file f as its
 * part `part` (a Galaxy part inflated, its MD5 checked at the end) */
static int deliver(Out *o, File *f, int part, const uint8_t *b, uint64_t n)
{
    char path[SYS_PATH];
    uint8_t *plain = NULL;
    const uint8_t *w = b;
    uint64_t wn = n;
    FILE *out;

    if (part != f->done)
        return fail(o->s, "The installer's parts are not in order.");
    if (out_path(o, f->path, path, sizeof path) != 0)
        return -1;
    if (f->md5[0]) {
        wn = f->part_size[part];
        if (wn > IN_MEMORY)
            return fail(o->s, "The installer's header is not as expected.");
        plain = malloc(wn ? (size_t)wn : 1);
        if (!plain)
            return fail(o->s, "Not enough memory to unpack the installer.");
        if (inflate_zlib(b, (size_t)n, plain, (size_t)wn) != 0) {
            free(plain);
            return fail(o->s, "A file in the installer is damaged.");
        }
        if (part == 0)
            hash_init(&f->whole, HASH_MD5);
        hash_update(&f->whole, plain, (size_t)wn);
        w = plain;
    }
    out = fopen(path, part ? "ab" : "wb");
    if (!out || fwrite(w, 1, (size_t)wn, out) != wn) {
        if (out)
            fclose(out);
        free(plain);
        return fail(o->s, "A file could not be written.");
    }
    free(plain);
    if (fclose(out) != 0)
        return fail(o->s, "A file could not be written.");
    f->done++;
    if (f->md5[0] && f->done == f->nparts) {
        uint8_t dg[16];
        char hex[33];
        int i;
        hash_final(&f->whole, dg);
        for (i = 0; i < 16; i++)
            snprintf(hex + 2 * i, 3, "%02x", dg[i]);
        if (sys_stricmp(hex, f->md5) != 0)
            return fail(o->s, "A file in the installer is damaged (its MD5 is wrong).");
    }
    return step(o, f, wn);
}

typedef struct {
    int data, file, part;
} Job;

static Setup *sort_setup;

static int job_order(const void *a, const void *b)
{
    const Job *x = a, *y = b;
    const Data *p = &sort_setup->data[x->data], *q = &sort_setup->data[y->data];
    if (p->first_slice != q->first_slice)
        return p->first_slice < q->first_slice ? -1 : 1;
    if (p->chunk_offset != q->chunk_offset)
        return p->chunk_offset < q->chunk_offset ? -1 : 1;
    if (p->offset != q->offset)
        return p->offset < q->offset ? -1 : 1;
    if (x->data != y->data)
        return x->data < y->data ? -1 : 1;
    if (x->file != y->file)
        return x->file < y->file ? -1 : 1;
    return x->part - y->part;
}

/* one data entry's bytes (at the chunk's position) to the jobs using it:
 * streamed into a file when one plain file takes it, else in memory */
static int unpack_data(Out *o, Chunk *c, Job *jobs, int njobs)
{
    Setup *s = o->s;
    Data *d = &s->data[jobs[0].data];
    File *f = &s->files[jobs[0].file];
    Hash h;
    uint8_t dg[32];
    int i;

    hash_init(&h, d->hash);
    if (njobs == 1 && !d->exe_filter && !f->md5[0]) {
        char path[SYS_PATH];
        uint8_t buf[PIECE];
        uint64_t left = d->size;
        FILE *out;
        if (out_path(o, f->path, path, sizeof path) != 0)
            return -1;
        if (!(out = fopen(path, "wb")))
            return fail(s, "A file could not be written.");
        while (left) {
            size_t k = left < sizeof buf ? (size_t)left : sizeof buf;
            if (chunk_read(c, buf, k) != 0 || fwrite(buf, 1, k, out) != k) {
                fclose(out);
                return s->err[0] ? -1 : fail(s, "A file could not be written.");
            }
            hash_update(&h, buf, k);
            left -= k;
        }
        if (fclose(out) != 0)
            return fail(s, "A file could not be written.");
        hash_final(&h, dg);
        if (memcmp(dg, d->digest, (size_t)d->hash) != 0)
            return fail(s, "A file in the installer is damaged (its checksum is wrong).");
        f->done++;
        return step(o, f, d->size);
    } else {
        uint8_t *b = d->size <= IN_MEMORY ? malloc(d->size ? (size_t)d->size : 1) : NULL;
        int r = 0;
        if (!b)
            return fail(s, "Not enough memory to unpack the installer.");
        if (chunk_read(c, b, (size_t)d->size) != 0) {
            free(b);
            return -1;
        }
        if (d->exe_filter)
            call_filter(b, (size_t)d->size, d->flip);
        hash_update(&h, b, (size_t)d->size);
        hash_final(&h, dg);
        if (memcmp(dg, d->digest, (size_t)d->hash) != 0)
            r = fail(s, "A file in the installer is damaged (its checksum is wrong).");
        for (i = 0; r == 0 && i < njobs; i++)
            r = deliver(o, &s->files[jobs[i].file], jobs[i].part, b, d->size);
        free(b);
        return r;
    }
}

/* the wanted files' data, chunk by chunk, each read once */
static int unpack_files(Out *o)
{
    Setup *s = o->s;
    Job *jobs;
    int njobs = 0, i, j, r = 0;

    for (i = 0; i < s->nfiles; i++)
        if (s->files[i].wanted)
            njobs += s->files[i].nparts;
    jobs = malloc(sizeof *jobs * (size_t)(njobs ? njobs : 1));
    if (!jobs)
        return fail(s, "Not enough memory to unpack the installer.");
    njobs = 0;
    for (i = 0; i < s->nfiles; i++)
        for (j = 0; s->files[i].wanted && j < s->files[i].nparts; j++) {
            jobs[njobs].data = s->files[i].parts[j];
            jobs[njobs].file = i;
            jobs[njobs++].part = j;
        }
    sort_setup = s;
    qsort(jobs, (size_t)njobs, sizeof *jobs, job_order);
    for (i = 0; r == 0 && i < njobs;) {
        Chunk c;
        const Data *d0 = &s->data[jobs[i].data];
        uint64_t need = 0;
        int end = i;
        while (end < njobs && s->data[jobs[end].data].first_slice == d0->first_slice &&
               s->data[jobs[end].data].chunk_offset == d0->chunk_offset) {
            const Data *d = &s->data[jobs[end].data];
            if (d->offset + d->size > need)
                need = d->offset + d->size;
            end++;
        }
        if ((r = chunk_open(&c, s, d0, need)) == 0)
            while (r == 0 && i < end) {
                int k = i + 1;
                const Data *d = &s->data[jobs[i].data];
                while (k < end && jobs[k].data == jobs[i].data)
                    k++;
                if (d->offset < c.out_pos)
                    r = fail(s, "The installer's data overlaps.");
                else if ((r = chunk_skip(&c, d->offset - c.out_pos)) == 0)
                    r = unpack_data(o, &c, jobs + i, k - i);
                i = k;
            }
        chunk_close(&c);
        i = end;
    }
    free(jobs);
    for (i = 0; r == 0 && i < s->nfiles; i++)
        if (s->files[i].wanted && s->files[i].done != s->files[i].nparts)
            r = fail(s, "The installer's files are not all there.");
    return r;
}

/* ---- inno_unpack */

static void remove_tree(const char *dir);

static void remove_entry(void *ctx, const char *name, int is_dir)
{
    char path[SYS_PATH];
    sys_join(path, sizeof path, (const char *)ctx, name);
    if (is_dir)
        remove_tree(path);
    else
        remove(path);
}

/* the folder and everything in it (only ever our own `dir`.part) */
static void remove_tree(const char *dir)
{
    sys_list_dir(dir, remove_entry, (void *)dir);
    sys_rmdir(dir);
}

static int path_char(char c)
{
    return c == '\\' ? '/' : c >= 'A' && c <= 'Z' ? c + 32 : c;
}

/* 1 if a wanted file is `must_have` or below it (case ignored) */
static int has_path(const Setup *s, const char *must_have)
{
    size_t k = strlen(must_have), j;
    int i;
    for (i = 0; i < s->nfiles; i++) {
        const char *p = s->files[i].path;
        if (!s->files[i].wanted || strlen(p) < k)
            continue;
        for (j = 0; j < k && path_char(p[j]) == path_char(must_have[j]); j++)
            ;
        if (j == k && (p[k] == 0 || p[k] == '/'))
            return 1;
    }
    return 0;
}

/* the largest wanted file named as a CD image (GOG's game.gog, *.ins,
 * *.iso ...), -1 if none */
static int image_file(const Setup *s)
{
    static const char *const ext[] = {".gog", ".iso", ".bin", ".img", ".ins", ".inst", ".dat"};
    int i, best = -1;
    size_t e;
    for (i = 0; i < s->nfiles; i++) {
        const File *f = &s->files[i];
        size_t len = strlen(f->path);
        if (!f->wanted)
            continue;
        for (e = 0; e < sizeof ext / sizeof *ext; e++) {
            size_t k = strlen(ext[e]);
            if (len > k && !sys_stricmp(f->path + len - k, ext[e]) &&
                (best < 0 || f->size > s->files[best].size))
                best = i;
        }
    }
    return best;
}

/* 1 if the file holds an ISO 9660 file system (2048-byte sectors, or raw
 * 2352-byte ones in Mode 1 or Mode 2 Form 1, as cdimage.c reads them) */
static int is_cd_image(const char *path)
{
    static const long at[3] = {16L * 2048 + 1, 16L * 2352 + 16 + 1, 16L * 2352 + 24 + 1};
    uint8_t id[5];
    FILE *f = fopen(path, "rb");
    int i, r = 0;

    for (i = 0; f && !r && i < 3; i++)
        r = fseek(f, at[i], SEEK_SET) == 0 && fread(id, 1, 5, f) == 5 && !memcmp(id, "CD001", 5);
    if (f)
        fclose(f);
    return r;
}

/* progress passed on, a stop remembered: an unpacking of the image the
 * player stopped does not go on with the setup's files */
typedef struct {
    int (*fn)(void *ctx, const char *file, long done, long total);
    void *ctx;
    int stopped;
} Pass;

static int pass_on(void *ctx, const char *file, long done, long total)
{
    Pass *p = ctx;
    if (p->fn && p->fn(p->ctx, file, done, total)) {
        p->stopped = 1;
        return 1;
    }
    return 0;
}

/* the wanted files (only file `only`, if >= 0) into the folder `into`,
 * made anew */
static int unpack_into(Setup *s, const char *into, int only, Pass *pass)
{
    Out o;
    int i, r, *keep = malloc(sizeof *keep * (size_t)(s->nfiles ? s->nfiles : 1));

    if (!keep)
        return fail(s, "Not enough memory to unpack the installer.");
    memset(&o, 0, sizeof o);
    o.s = s, o.dir = into, o.progress = pass_on, o.ctx = pass;
    for (i = 0; i < s->nfiles; i++) {
        keep[i] = s->files[i].wanted;
        s->files[i].wanted = only >= 0 ? i == only : keep[i];
        s->files[i].done = 0;
        if (s->files[i].wanted)
            o.total += s->files[i].size;
    }
    if (sys_is_dir(into))
        remove_tree(into);                      /* left by an unpacking that was stopped */
    if (sys_mkdir(into) != 0)
        r = fail(s, "The folder for the game's files cannot be made.");
    else
        r = unpack_files(&o);
    for (i = 0; i < s->nfiles; i++)
        s->files[i].wanted = keep[i];
    free(keep);
    return r;
}

/* the setup's CD image taken out into `dir`.setup and unpacked into dir
 * (cd_unpack, which checks must_have on the CD) */
static int from_image(Setup *s, const char *dir, const char *must_have, int image, Pass *pass)
{
    char tmp[SYS_PATH], path[SYS_PATH];
    Out o;
    int r;

    snprintf(tmp, sizeof tmp, "%s.setup", dir);
    memset(&o, 0, sizeof o);
    o.s = s, o.dir = tmp;
    r = unpack_into(s, tmp, image, pass);
    if (r == 0 && out_path(&o, s->files[image].path, path, sizeof path) != 0)
        r = -1;
    if (r == 0 && !is_cd_image(path))
        r = fail(s, "The installer holds no CD image.");
    if (r == 0)
        r = cd_unpack(path, dir, must_have, pass_on, pass, s->err, s->n);
    remove_tree(tmp);
    return r;
}

int inno_unpack(const char *setup, const char *dir, const char *must_have,
                int (*progress)(void *ctx, const char *file, long done, long total), void *ctx,
                char *err, size_t n)
{
    Setup s;
    Pass pass;
    char part[SYS_PATH];
    int r, i, image;

    memset(&s, 0, sizeof s);
    s.err = err, s.n = n;
    err[0] = 0;
    pass.fn = progress, pass.ctx = ctx, pass.stopped = 0;
    if (sys_is_dir(dir) || sys_is_file(dir))
        return fail(&s, "The folder for the game's files is there already.");
    if (load_setup(&s, setup) != 0)
        return -1;
    for (i = 0; i < s.nfiles; i++)
        if (s.files[i].wanted && !safe_path(s.files[i].path)) {
            free_setup(&s);
            return fail(&s, "The installer names a file outside the game's folder.");
        }
    /* a CD image in it first, as gog_find prefers the image of an
     * installed release: GOG installs some CD games partly, the image
     * beside, and the marker may be among both */
    if ((image = image_file(&s)) >= 0) {
        r = from_image(&s, dir, must_have, image, &pass);
        if (r == 0 || pass.stopped) {
            free_setup(&s);
            return r;
        }
        err[0] = 0;
    }
    if (must_have && *must_have && !has_path(&s, must_have)) {
        free_setup(&s);
        return fail(&s, "The installer is not the game's (a file it must have is missing).");
    }
    snprintf(part, sizeof part, "%s.part", dir);
    r = unpack_into(&s, part, -1, &pass);
    if (r == 0 && sys_rename(part, dir) != 0)
        r = fail(&s, "The unpacked files could not be moved to their folder.");
    if (r != 0)
        remove_tree(part);
    free_setup(&s);
    return r;
}

/* ---- a setup lying about */

typedef struct {
    const GogRelease *rel;
    const char *dir;
    char *out;
    size_t n;
    int found;
} Look;

/* 1 if the setup at path is the release's: its product ID, or the marker
 * among its files, or the game's name and a CD image */
static int is_release(const char *path, const GogRelease *rel)
{
    Setup s;
    char err[256];
    int r;

    memset(&s, 0, sizeof s);
    s.err = err, s.n = sizeof err;
    if (load_setup(&s, path) != 0)
        return 0;
    r = (rel->gog_id && *rel->gog_id && !strcmp(s.app_id, rel->gog_id)) ||
        (rel->must_have && *rel->must_have && has_path(&s, rel->must_have)) ||
        (rel->folder && !sys_stricmp(s.name, rel->folder) && image_file(&s) >= 0);
    free_setup(&s);
    return r;
}

static void look_entry(void *ctx, const char *name, int is_dir)
{
    Look *l = ctx;
    size_t len = strlen(name);
    char path[SYS_PATH];

    if (l->found || is_dir || len < 10 || sys_stricmp(name + len - 4, ".exe") != 0)
        return;
    {
        char head[7];
        memcpy(head, name, 6);
        head[6] = 0;
        if (sys_stricmp(head, "setup_") != 0)
            return;
    }
    sys_join(path, sizeof path, l->dir, name);
    if (is_release(path, l->rel)) {
        snprintf(l->out, l->n, "%s", path);
        l->found = 1;
    }
}

int inno_find(const GogRelease *rel, char *out, size_t n)
{
    char dirs[8][SYS_PATH], home[SYS_PATH];
    static const char *const below_home[] = {"Downloads", "Desktop", "Documents"};
    int k = 0, i;
    Look l;

    sys_exe_dir(dirs[k++], SYS_PATH);
    snprintf(dirs[k++], SYS_PATH, ".");
    sys_data_dir(dirs[k++], SYS_PATH);
    sys_home_dir(home, sizeof home);
    if (home[0]) {
        snprintf(dirs[k++], SYS_PATH, "%s", home);
        for (i = 0; i < 3; i++)
            sys_join(dirs[k++], SYS_PATH, home, below_home[i]);
    }
    memset(&l, 0, sizeof l);
    l.rel = rel, l.out = out, l.n = n;
    for (i = 0; i < k && !l.found; i++) {
        l.dir = dirs[i];
        sys_list_dir(dirs[i], look_entry, &l);
    }
    return l.found;
}
