/* unlzexe.h - a program packed with LZEXE 0.91, unpacked.
 *
 * A game's program may be packed on the player's disc; a port that keeps
 * the program's memory (rmem.h) needs the image and the relocations the
 * packed program makes of itself.  This is tools/unlzexe.py in C and
 * gives the same bytes: a plain MZ program with the header laid out as
 * Microsoft LINK does (the relocations from 1Eh in the packed file's
 * order).  The format is described at the top of tools/unlzexe.py.
 * rm_load_exe unpacks a packed program by itself.
 */
#ifndef DK_UNLZEXE_H
#define DK_UNLZEXE_H

#include <stddef.h>
#include <stdint.h>

/* 1 if the bytes are an MZ program with LZEXE 0.91's mark ("LZ91" at 1Ch) */
int lzexe_packed(const uint8_t *data, size_t size);

/* The plain program (malloc'd, *out_size bytes), or NULL when the bytes
 * are not such a program or are damaged (a copy from before the image, a
 * stream or relocations running past the file, a relocation outside the
 * image). */
uint8_t *lzexe_unpack(const uint8_t *data, size_t size, size_t *out_size);

#endif
