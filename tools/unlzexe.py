#!/usr/bin/env python3
"""Unpack a program packed with LZEXE 0.91 into a plain MZ program.

    unlzexe.py PACKED.EXE [OUT.EXE]

PACKED.EXE is relative to the game's files (kit.py) unless it is a path
that exists; OUT defaults to build/unpacked/NAME.EXE of the project, which
a hints file names as `exe build/unpacked/NAME.EXE`.  The runner runs a
packed program as it is; this is for the analysis, which needs the image
and its relocations.

What a packed file holds (found at 1Ch: "LZ91"):
  * the packed image from the end of the header to the unpacker's segment
    (the header's CS), read forwards: a stream of bits, 16 at a time in a
    little-endian word that is fetched as soon as the last bit of the one
    before is used, mixed with whole bytes.  1: a byte as it is.  00 and
    two bits n: n+2 bytes copied from a byte's distance (the byte minus
    256).  01: two bytes, 13 bits of distance (minus 8192) and 3 bits of
    length (+2); length bits 0 mean a third byte: 0 ends the image, 1 is
    a mark between segments with nothing to do, else that byte + 1 bytes;
  * at the unpacker's segment seven words: the program's IP, CS, SP and
    SS, the packed image's paragraphs, the paragraphs the image grows by,
    and the unpacker's length in bytes;
  * at 158h of that segment the relocations, each the distance from the
    one before: a byte, or 0 and a word (word 0: 0FFFh paragraphs further
    with no relocation; word 1: the end).

What the packer did not keep is written the way Microsoft LINK lays a
header out (as build.py does: word 1 at 1Ch, the relocations from 1Eh in
the order the packed file has them, 512-byte blocks): the header's
length, its checksum (0), and the memory the program asks for beyond its
image, which is worked out from what the packed program asks for.  That
the image and relocations are the ones the packed program makes of
itself can be checked in the runner: stop at the program's own entry
(`-break` with the linear address, `-ram`) and compare.
"""
import os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from kit import build_dir, game_dir

RELOCS_AT = 0x158


class Bits:
    """the packed image's bits and bytes"""
    def __init__(self, data, pos):
        self.data, self.pos = data, pos
        self.fetch()

    def fetch(self):
        self.word = struct.unpack_from('<H', self.data, self.pos)[0]
        self.pos += 2
        self.left = 16

    def bit(self):
        b = self.word & 1
        self.word >>= 1
        self.left -= 1
        if not self.left:
            self.fetch()
        return b

    def byte(self):
        self.pos += 1
        return self.data[self.pos - 1]


def unpack_image(data, pos):
    """the image, and where the packed image ended"""
    bits = Bits(data, pos)
    out = bytearray()
    while True:
        if bits.bit():
            out.append(bits.byte())
            continue
        if not bits.bit():
            n = bits.bit() << 1
            n = (n | bits.bit()) + 2
            dist = 256 - bits.byte()
        else:
            lo, hi = bits.byte(), bits.byte()
            dist = 8192 - (lo | (hi >> 3) << 8)
            n = (hi & 7) + 2
            if n == 2:
                n = bits.byte()
                if n == 0:
                    return bytes(out), bits.pos
                if n == 1:
                    continue
                n += 1
        if dist > len(out):
            raise ValueError(f'a copy from before the image at {len(out):X}')
        for _ in range(n):
            out.append(out[-dist])


def unpack_relocs(data, pos, end):
    """the relocations as (offset, segment), in the file's order"""
    rel, seg, off = [], 0, 0
    while True:
        if pos >= end:
            raise ValueError('the relocations have no end')
        span = data[pos]
        pos += 1
        if span == 0:
            span = struct.unpack_from('<H', data, pos)[0]
            pos += 2
            if span == 0:
                seg += 0x0FFF
                continue
            if span == 1:
                return rel
        off += span
        seg += off >> 4
        off &= 15
        rel.append((off, seg))


def unpack(data):
    """the plain program's bytes, from a packed program's"""
    if data[:2] != b'MZ' or data[0x1C:0x20] != b'LZ91':
        raise ValueError('not packed with LZEXE 0.91 (no "LZ91" at 1Ch)')
    hdr, minalloc, maxalloc = struct.unpack_from('<3H', data, 8)
    cs = struct.unpack_from('<H', data, 0x16)[0]
    start = hdr * 16
    stub = start + cs * 16
    ip, cs, sp, ss, packed, grows, stub_len = struct.unpack_from('<7H', data, stub)
    img, end = unpack_image(data, start)
    if end > stub or stub + stub_len > len(data):
        raise ValueError('the packed image and the unpacker overlap')
    rel = unpack_relocs(data, stub + RELOCS_AT, stub + stub_len)
    for off, seg in rel:
        if seg * 16 + off + 2 > len(img):
            raise ValueError(f'a relocation at {seg:04X}:{off:04X}, outside the image')
    # the packed program asks for the room to unpack in and for the
    # unpacker behind it; without those it is what the program asked for
    less = grows + (stub_len + 15) // 16 + 9
    minalloc = max(0, minalloc - less)
    if maxalloc != 0xFFFF:
        maxalloc = max(minalloc, maxalloc - less)
    hdr_len = (0x1E + 4 * len(rel) + 511) // 512 * 512
    size = hdr_len + len(img)
    h = bytearray(hdr_len)
    struct.pack_into('<2s13H', h, 0, b'MZ', size % 512, (size + 511) // 512, len(rel),
                     hdr_len // 16, minalloc, maxalloc, ss, sp, 0, ip, cs, 0x1E, 0)
    struct.pack_into('<H', h, 0x1C, 1)
    for i, (off, seg) in enumerate(rel):
        struct.pack_into('<HH', h, 0x1E + 4 * i, off, seg)
    return bytes(h) + img


def main():
    args = sys.argv[1:]
    if not 1 <= len(args) <= 2:
        sys.exit(__doc__)
    src = args[0] if os.path.exists(args[0]) else os.path.join(game_dir(), args[0])
    with open(src, 'rb') as f:
        data = f.read()
    try:
        plain = unpack(data)
    except (ValueError, struct.error, IndexError) as e:
        sys.exit(f'unlzexe: {src}: {e}')
    out = args[1] if len(args) == 2 else build_dir('unpacked', os.path.basename(src))
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out, 'wb') as f:
        f.write(plain)
    nrel, hdr = struct.unpack_from('<HH', plain, 6)
    ss, sp, _, ip, cs = struct.unpack_from('<5H', plain, 0x0E)
    print(f'{out}: {len(plain)} bytes from {len(data)}, image {len(plain) - hdr * 16}, '
          f'{nrel} relocations, entry {cs:04X}:{ip:04X}, stack {ss:04X}:{sp:04X}')


if __name__ == '__main__':
    main()
