/* pmem.h - a protected-mode DOS program's memory, as it was under its
 * DOS extender.
 *
 * rmem.h's counterpart for a 32-bit program (a pMAX flat image, see
 * tools/disasm.py): the C keeps the program's memory as it was, the 16 MB
 * of linear memory dosrun has, with the image loaded from the player's own
 * file where the extender loads it and its selector relocations set to the
 * selectors the extender gives.  The translated routines read and write it
 * through the accessors below by the names of the hints (symmap.py), with
 * 32-bit offsets, so records that point at each other keep working and the
 * memory can be compared byte for byte with the original's in dosrun
 * (`-mem`, tools/memcmp.py --base).
 *
 * The extender itself (its own tables below the image, its heap's block
 * headers) is not modelled; memory other than the image starts cleared.
 * A port includes rmem.h or pmem.h, not both (the accessors share names).
 *
 * This is a stepping stone like vga.c: once the code is C throughout, the
 * variables can move into C structures one by one.
 */
#ifndef DK_PMEM_H
#define DK_PMEM_H

#include <stddef.h>
#include <stdint.h>

#define PM_SIZE 0x1000000u
#define PM_MAX_DESC 8

extern uint8_t pmem[PM_SIZE];

/* the linear base of the segment DS holds nearly always (rb, rw ...); the
 * port sets it, usually to a descriptor's base after pm_load */
extern uint32_t pm_ds;

/* A loaded image: the entry point (an offset in the image), the size the
 * header asks to allocate, and each descriptor's linear base, size and the
 * selector its relocations were given. */
typedef struct {
    uint32_t base, alloc, image, entry;
    int ndesc;
    struct { uint32_t base, size; uint16_t sel; } desc[PM_MAX_DESC];
} PmImage;

/* Loads the pMAX image in `data` (the whole file, `len` bytes) at linear
 * `base` after clearing memory: the image, zeros up to the header's
 * allocation, each selector relocation set to sels[descriptor] (nsels of
 * them; a relocation naming another descriptor is an error).  With
 * want_size/want_sha256 (hex, lower case; NULL: not checked) the file must
 * be the one the hints describe (symmap.py's KEY_SIZE, KEY_SHA256).
 * 0, or -1 with a message in err. */
int pm_load_image(const uint8_t *data, size_t len, size_t want_size, const char *want_sha256,
                  uint32_t base, const uint16_t *sels, int nsels, PmImage *img,
                  char *err, size_t n);
/* the same from the file at `path` */
int pm_load(const char *path, size_t want_size, const char *want_sha256,
            uint32_t base, const uint16_t *sels, int nsels, PmImage *img,
            char *err, size_t n);

/* all of memory into the file `path`, as dosrun's -mem writes it (for
 * tools/memcmp.py --base); 0, or -1 if it cannot be written */
int pm_write(const char *path);

/* ---- access by linear address (wraps at PM_SIZE) */

static inline uint8_t lrb(uint32_t a) { return pmem[a & (PM_SIZE - 1)]; }
static inline void lwb(uint32_t a, uint8_t v) { pmem[a & (PM_SIZE - 1)] = v; }
static inline uint16_t lrw(uint32_t a) { return (uint16_t)(lrb(a) | lrb(a + 1) << 8); }
static inline void lww(uint32_t a, uint16_t v)
{
    lwb(a, (uint8_t)v);
    lwb(a + 1, (uint8_t)(v >> 8));
}
static inline uint32_t lrd(uint32_t a) { return lrw(a) | (uint32_t)lrw(a + 2) << 16; }
static inline void lwd(uint32_t a, uint32_t v)
{
    lww(a, (uint16_t)v);
    lww(a + 2, (uint16_t)(v >> 16));
}

/* ---- at pm_ds, 32-bit offsets */

static inline uint8_t rb(uint32_t off) { return lrb(pm_ds + off); }
static inline void wb(uint32_t off, uint8_t v) { lwb(pm_ds + off, v); }
static inline uint16_t rw(uint32_t off) { return lrw(pm_ds + off); }
static inline void ww(uint32_t off, uint16_t v) { lww(pm_ds + off, v); }
static inline uint32_t rd(uint32_t off) { return lrd(pm_ds + off); }
static inline void wd(uint32_t off, uint32_t v) { lwd(pm_ds + off, v); }
static inline int8_t rsb(uint32_t off) { return (int8_t)rb(off); }
static inline int16_t rsw(uint32_t off) { return (int16_t)rw(off); }
static inline int32_t rsd(uint32_t off) { return (int32_t)rd(off); }

#endif
