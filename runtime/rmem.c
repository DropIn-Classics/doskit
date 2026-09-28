/* rmem.c - see rmem.h */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rmem.h"
#include "sha256.h"
#include "sys.h"

uint8_t mem[MEM_SIZE];
uint16_t rm_ds, rm_psp, rm_arena;

static uint16_t sw(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

int rm_load_exe(const char *path, size_t want_size, const char *want_sha256,
                uint16_t psp, uint16_t paras, char *err, size_t n)
{
    char hex[65];
    uint8_t digest[32];
    size_t size, hdr, image, i;
    uint8_t *f;
    uint16_t nrel, relofs, code = (uint16_t)(psp + 0x10);

    f = sys_load(path, &size);
    if (!f) {
        snprintf(err, n, "%s cannot be read.", path);
        return -1;
    }
    if (want_sha256) {
        sha256(f, size, digest);
        for (i = 0; i < 32; i++)
            snprintf(hex + 2 * i, 3, "%02x", digest[i]);
        if (size != want_size || strcmp(hex, want_sha256) != 0) {
            snprintf(err, n, "%s is not the file this port was made for (SHA-256 %s).", path, hex);
            free(f);
            return -1;
        }
    }
    if (size < 0x1C || f[0] != 'M' || f[1] != 'Z') {
        snprintf(err, n, "%s is not an MZ program.", path);
        free(f);
        return -1;
    }

    memset(mem, 0, sizeof mem);
    /* the image: from the header's end to the size in the header */
    hdr = (size_t)sw(f + 8) * 16;
    image = (size_t)(sw(f + 4) - 1) * 512 + (sw(f + 2) ? sw(f + 2) : 512) - hdr;
    if (hdr + image > size || (size_t)code * 16 + image > MEM_SIZE) {
        snprintf(err, n, "%s: the header's sizes do not fit.", path);
        free(f);
        return -1;
    }
    memcpy(mem + (size_t)code * 16, f + hdr, image);
    nrel = sw(f + 6);
    relofs = sw(f + 0x18);
    for (i = 0; i < nrel && relofs + 4 * i + 4 <= size; i++) {
        const uint8_t *r = f + relofs + 4 * i;
        uint16_t off = sw(r), seg = (uint16_t)(sw(r + 2) + code);
        fww(seg, off, (uint16_t)(frw(seg, off) + code));
    }
    free(f);

    /* the arena: one free block from the program's end to RM_TOP */
    rm_arena = (uint16_t)(psp + paras);
    fwb(rm_arena, 0, 'Z');
    fww(rm_arena, 1, 0);
    fww(rm_arena, 3, (uint16_t)(RM_TOP - rm_arena - 1));
    rm_psp = psp;
    return 0;
}

int rm_write(const char *path)
{
    FILE *f = fopen(path, "wb");
    int ok = f && fwrite(mem, 1, 0xA0000, f) == 0xA0000;
    if (f && fclose(f) != 0)
        ok = 0;
    return ok ? 0 : -1;
}

/* The MCB chain as DOS keeps it: 'M' or 'Z' (the last), the owner (0 =
 * free), the size in paragraphs.  First fit, as DOS's default strategy. */
uint16_t dos_alloc(uint16_t paras)
{
    uint16_t mcb = rm_arena;
    for (;;) {
        uint8_t kind = frb(mcb, 0);
        uint16_t owner = frw(mcb, 1), size = frw(mcb, 3);
        if (owner == 0 && size >= paras) {
            if (size > paras) {                 /* the rest stays free */
                uint16_t rest = (uint16_t)(mcb + 1 + paras);
                fwb(rest, 0, kind);
                fww(rest, 1, 0);
                fww(rest, 3, (uint16_t)(size - paras - 1));
                fwb(mcb, 0, 'M');
                fww(mcb, 3, paras);
            }
            fww(mcb, 1, rm_psp);
            return (uint16_t)(mcb + 1);
        }
        if (kind == 'Z')
            return 0;
        mcb = (uint16_t)(mcb + 1 + size);
    }
}

int dos_free(uint16_t seg)
{
    uint16_t mcb = rm_arena;
    for (;;) {
        uint8_t kind = frb(mcb, 0);
        uint16_t size = frw(mcb, 3);
        if ((uint16_t)(mcb + 1) == seg) {
            fww(mcb, 1, 0);
            break;
        }
        if (kind == 'Z')
            return -1;
        mcb = (uint16_t)(mcb + 1 + size);
    }
    /* join free neighbours, as DOS does at the next allocation */
    for (mcb = rm_arena; frb(mcb, 0) == 'M';) {
        uint16_t next = (uint16_t)(mcb + 1 + frw(mcb, 3));
        if (frw(mcb, 1) == 0 && frw(next, 1) == 0) {
            fwb(mcb, 0, frb(next, 0));
            fww(mcb, 3, (uint16_t)(frw(mcb, 3) + 1 + frw(next, 3)));
        } else {
            mcb = next;
        }
    }
    return 0;
}
