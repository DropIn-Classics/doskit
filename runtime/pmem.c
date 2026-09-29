/* pmem.c - see pmem.h */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pmem.h"
#include "sha256.h"
#include "sys.h"

uint8_t pmem[PM_SIZE];
uint32_t pm_ds;

static uint32_t sd(const uint8_t *p) { return p[0] | p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

int pm_load_image(const uint8_t *f, size_t size, size_t want_size, const char *want_sha256,
                  uint32_t base, const uint16_t *sels, int nsels, PmImage *img,
                  char *err, size_t n)
{
    char hex[65];
    uint8_t digest[32];
    size_t i, start, rel;
    uint32_t nrel;

    if (want_sha256) {
        sha256(f, size, digest);
        for (i = 0; i < 32; i++)
            snprintf(hex + 2 * i, 3, "%02x", digest[i]);
        if (size != want_size || strcmp(hex, want_sha256) != 0) {
            snprintf(err, n, "the program is not the one this port was made for (SHA-256 %s).", hex);
            return -1;
        }
    }
    /* the header (tools/disasm.py, PmaxProgram): a word, the allocation,
     * the format, the number of descriptors, the image's size, the entry,
     * the number of relocations; the descriptors; the image; the
     * relocations (image offset, descriptor number) */
    if (size < 20) {
        snprintf(err, n, "not a pMAX image (%u bytes).", (unsigned)size);
        return -1;
    }
    memset(img, 0, sizeof *img);
    img->base = base;
    img->alloc = sd(f + 4);
    img->ndesc = f[9];
    img->image = sd(f + 10);
    img->entry = sd(f + 14);
    nrel = (uint32_t)(f[18] | f[19] << 8);
    start = 20 + 8 * (size_t)img->ndesc;
    rel = start + img->image;
    if (img->ndesc > PM_MAX_DESC || rel > size || rel + 5 * (size_t)nrel != size
        || img->alloc < img->image || (uint64_t)base + img->alloc > PM_SIZE) {
        snprintf(err, n, "not a pMAX image this loader takes (%u bytes).", (unsigned)size);
        return -1;
    }
    for (i = 0; i < (size_t)img->ndesc; i++) {
        img->desc[i].base = base + sd(f + 20 + 8 * i);
        img->desc[i].size = sd(f + 24 + 8 * i);
        img->desc[i].sel = (int)i < nsels ? sels[i] : 0;
    }

    memset(pmem, 0, sizeof pmem);
    memcpy(pmem + base, f + start, img->image);
    for (i = 0; i < nrel; i++) {
        const uint8_t *r = f + rel + 5 * i;
        uint32_t off = sd(r);
        if (r[4] >= nsels || r[4] >= img->ndesc || off + 2 > img->image) {
            snprintf(err, n, "relocation %u (at %X, descriptor %u) has no selector.",
                     (unsigned)i, (unsigned)off, r[4]);
            return -1;
        }
        lww(base + off, sels[r[4]]);
    }
    return 0;
}

int pm_load(const char *path, size_t want_size, const char *want_sha256,
            uint32_t base, const uint16_t *sels, int nsels, PmImage *img,
            char *err, size_t n)
{
    size_t size;
    uint8_t *f = sys_load(path, &size);
    int r;

    if (!f) {
        snprintf(err, n, "%s cannot be read.", path);
        return -1;
    }
    r = pm_load_image(f, size, want_size, want_sha256, base, sels, nsels, img, err, n);
    free(f);
    return r;
}

int pm_write(const char *path)
{
    FILE *f = fopen(path, "wb");
    int ok = f && fwrite(pmem, 1, PM_SIZE, f) == PM_SIZE;
    if (f && fclose(f) != 0)
        ok = 0;
    return ok ? 0 : -1;
}
