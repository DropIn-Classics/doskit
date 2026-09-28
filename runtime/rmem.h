/* rmem.h - a DOS program's memory, as it was under DOS.
 *
 * The first step of a port that translates a program routine by routine:
 * the C keeps the program's memory as it was, one flat megabyte of
 * real-mode memory with the program image loaded from the player's own
 * file (relocations applied, the rest cleared) and the DOS blocks the
 * program allocates behind it.  The translated routines read and write
 * it through the accessors below, by the names of the hints (symmap.py
 * writes them into a header), so records that point at each other with
 * 16-bit offsets and tables of routine offsets keep working, and the
 * memory can be compared byte for byte with the original's in dosrun
 * (tools/memcmp.py).  Loaded where dosrun loads it, the addresses are the
 * same too.
 *
 * This is a stepping stone like vga.c: once the code is C throughout, the
 * variables can move into C structures one by one.
 */
#ifndef DK_RMEM_H
#define DK_RMEM_H

#include <stddef.h>
#include <stdint.h>

#define MEM_SIZE 0x100000u

extern uint8_t mem[MEM_SIZE];

/* The PSP dosrun gives the first program it starts (the image follows at
 * PSP + 10h): its MCB at 0060h, the environment's block before the PSP.
 * 0067h for the programs ported so far; `-log` of the entry point in
 * dosrun shows it for another. */
#define RM_LOAD_PSP 0x0067u
/* the end of conventional memory the arena reaches */
#define RM_TOP 0xA000u

/* the segment DS holds nearly always (rb, rw ...); the port sets it */
extern uint16_t rm_ds;
/* the loaded program's PSP, and the first MCB behind the program */
extern uint16_t rm_psp, rm_arena;

/* Loads the MZ program at `path` with its PSP at `psp` (the image at psp +
 * 10h, relocated by that segment) after clearing memory; the program keeps
 * `paras` paragraphs from its PSP on (what it keeps with INT 21h 4Ah), one
 * free block follows up to RM_TOP.  With want_size/want_sha256 (hex, lower
 * case; NULL: not checked) the file must be the one the hints describe
 * (symmap.py's KEY_SIZE, KEY_SHA256).  0, or -1 with a message in err. */
int rm_load_exe(const char *path, size_t want_size, const char *want_sha256,
                uint16_t psp, uint16_t paras, char *err, size_t n);

/* memory 0-A0000h into the file `path`, as dosrun's -ram writes it (for
 * tools/memcmp.py); 0, or -1 if it cannot be written */
int rm_write(const char *path);

/* ---- access, segment:offset (the offset wraps at 64 KB as on the CPU) */

static inline uint32_t lin(uint16_t seg, uint16_t off)
{
    return (((uint32_t)seg << 4) + off) & (MEM_SIZE - 1);
}
static inline uint8_t frb(uint16_t seg, uint16_t off) { return mem[lin(seg, off)]; }
static inline void fwb(uint16_t seg, uint16_t off, uint8_t v) { mem[lin(seg, off)] = v; }
static inline uint16_t frw(uint16_t seg, uint16_t off)
{
    return (uint16_t)(frb(seg, off) | frb(seg, (uint16_t)(off + 1)) << 8);
}
static inline void fww(uint16_t seg, uint16_t off, uint16_t v)
{
    fwb(seg, off, (uint8_t)v);
    fwb(seg, (uint16_t)(off + 1), (uint8_t)(v >> 8));
}

/* at rm_ds */
static inline uint8_t rb(uint16_t off) { return frb(rm_ds, off); }
static inline void wb(uint16_t off, uint8_t v) { fwb(rm_ds, off, v); }
static inline uint16_t rw(uint16_t off) { return frw(rm_ds, off); }
static inline void ww(uint16_t off, uint16_t v) { fww(rm_ds, off, v); }
static inline int16_t rsw(uint16_t off) { return (int16_t)rw(off); }

/* two words as the 32-bit number the code keeps in them (low word first) */
static inline uint32_t rd(uint16_t off) { return rw(off) | (uint32_t)rw((uint16_t)(off + 2)) << 16; }
static inline void wd(uint16_t off, uint32_t v)
{
    ww(off, (uint16_t)v);
    ww((uint16_t)(off + 2), (uint16_t)(v >> 16));
}

/* ---- DOS memory (INT 21h 48h/49h): blocks behind the program */

/* a block of `paras` paragraphs; its segment, or 0 when there is no room */
uint16_t dos_alloc(uint16_t paras);
/* 0 on success */
int dos_free(uint16_t seg);

#endif
