#!/usr/bin/env python3
"""Unpack a CD image's ISO 9660 file system into a folder.

    isox.py IMAGE [OUT]

IMAGE is the data track: a plain ISO (2048-byte sectors) or a raw image
of 2352-byte sectors (Mode 1: 16 bytes of sync and header before the
2048 bytes of data; Mode 2 Form 1: 24 bytes of sync, header and
subheader), as GOG ships its CD games (game.gog, with a cue sheet
beside it; the audio tracks are separate files).  The sector size is
found from the image.  File names lose their ';1' version.  OUT defaults
to the game folder of the project (kit.py).
"""
import os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from kit import game_dir

SYNC = b'\x00' + b'\xff' * 10 + b'\x00'


class Image:
    def __init__(self, path):
        self.f = open(path, 'rb')
        self.f.seek(0)
        head = self.f.read(16)
        self.raw = 2352 if head[:12] == SYNC else 2048

    def sector(self, n):
        self.f.seek(n * self.raw)
        data = self.f.read(self.raw)
        if self.raw == 2048:
            return data
        mode = data[15]
        if mode == 1:
            return data[16:16 + 2048]
        if mode == 2:
            return data[24:24 + 2048]
        raise ValueError(f'sector {n}: mode {mode}')

    def read(self, lba, size):
        data = b''.join(self.sector(lba + i) for i in range((size + 2047) // 2048))
        return data[:size]


def walk(img, lba, size, path=''):
    """-> (path, lba, size, is_dir) for every entry below a directory"""
    data = img.read(lba, size)
    i = 0
    while i < len(data):
        n = data[i]
        if n == 0:                          # records never cross a sector
            i = (i // 2048 + 1) * 2048
            continue
        rec = data[i:i + n]
        i += n
        name = rec[33:33 + rec[32]]
        if name in (b'\0', b'\1'):
            continue
        name = name.decode('ascii').split(';')[0]
        elba, esize = struct.unpack_from('<I', rec, 2)[0], struct.unpack_from('<I', rec, 10)[0]
        p = f'{path}/{name}' if path else name
        isdir = bool(rec[25] & 2)
        yield p, elba, esize, isdir
        if isdir:
            yield from walk(img, elba, esize, p)


def unpack(image, out):
    img = Image(image)
    pvd = img.sector(16)
    if pvd[1:6] != b'CD001':
        raise ValueError('no ISO 9660 volume')
    root = pvd[156:156 + 34]
    n = 0
    for p, lba, size, isdir in walk(img, struct.unpack_from('<I', root, 2)[0],
                                    struct.unpack_from('<I', root, 10)[0]):
        dst = os.path.join(out, *p.split('/'))
        if isdir:
            os.makedirs(dst, exist_ok=True)
        else:
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            with open(dst, 'wb') as f:
                f.write(img.read(lba, size))
            n += 1
    return n


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    out = sys.argv[2] if len(sys.argv) > 2 else game_dir()
    print(f'{unpack(sys.argv[1], out)} files -> {os.path.normpath(out)}')


if __name__ == '__main__':
    main()
