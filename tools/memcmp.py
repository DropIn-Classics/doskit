#!/usr/bin/env python3
"""Compare the memory of a C port with the original's in the runner
(tools/run), both stopped at the same point, by the names of the hints.

    memcmp.py HINTS RAM_A RAM_B [--vram VRAM_A VRAM_B [--region LO HI NAME]...]
              [--skip SEG]... [--load SEG | --base LINEAR] [--max N]

RAM_A/RAM_B: memory 0-A0000h (dosrun -ram FILE, and the port's own dump
of rmem.h's memory).  The program's segments are compared (CODE's frame
at --load, default 0077h: where dosrun loads the first program, PSP 0067h,
and where a port that mirrors it loads the image); each run of differing
bytes is printed with the name at or before it ("hit_rects+4").
VRAM_A/VRAM_B: the 256 KB of video memory (byte 4 * offset + plane),
compared by region (offsets in a plane, hex; the whole plane without
--region).
"""
import argparse, bisect, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from disasm import Hints, load_program


def runs(a, b, base, length):
    """(start, end) of the runs of differing bytes in a[base:base+length]"""
    out, start = [], None
    for i in range(length):
        d = a[base + i] != b[base + i]
        if d and start is None:
            start = i
        elif not d and start is not None:
            out.append((start, i))
            start = None
    if start is not None:
        out.append((start, length))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('hints')
    ap.add_argument('ram_a')
    ap.add_argument('ram_b')
    ap.add_argument('--vram', nargs=2)
    ap.add_argument('--region', nargs=3, action='append', metavar=('LO', 'HI', 'NAME'))
    ap.add_argument('--skip', action='append', default=[], metavar='SEG',
                    help='a segment not compared (the stack, say)')
    ap.add_argument('--load', default='0077')
    ap.add_argument('--base', help='a pMAX image\'s linear address (hex)')
    ap.add_argument('--max', type=int, default=60)
    args = ap.parse_args()
    h = Hints(args.hints)
    a, b = open(args.ram_a, 'rb').read(), open(args.ram_b, 'rb').read()
    load = int(args.load, 16)
    segs = h.segs
    descs = None
    if h.kind in ('pmax', 'bin'):
        if args.base is None:
            raise SystemExit('memcmp.py: a 32-bit image wants --base')
        descs = load_program(h).descs
    total = 0
    for k, s in enumerate(segs):
        if s.name in args.skip:
            continue
        if descs is not None:
            dbase, size = descs[s.frame]
        elif s.size is not None:
            size = s.size
        elif k + 1 < len(segs):
            size = (segs[k + 1].frame - s.frame) * 16
        else:
            size = 0x10000
        names = sorted((off, n) for (sg, off), n in h.names.items() if sg == s.name)
        offs = [o for o, _ in names]
        base = int(args.base, 16) + dbase if descs is not None else (load + s.frame) * 16
        width = 4 if size <= 0x10000 else len('%X' % (size - 1))
        rs = runs(a, b, base, min(size, len(a) - base))
        n = sum(e - st for st, e in rs)
        total += n
        print(f'{s.name:6} {n:6} bytes differ in {len(rs)} runs')
        for st, e in rs[:args.max]:
            i = bisect.bisect_right(offs, st) - 1
            where = f'{names[i][1]}+{st - offs[i]:X}' if i >= 0 else '-'
            sa = a[base + st:base + min(e, st + 8)].hex(' ')
            sb = b[base + st:base + min(e, st + 8)].hex(' ')
            print(f'   {s.name}:{st:0{width}X}..{e - 1:0{width}X} {where:28} {sa:24} | {sb}')
        if len(rs) > args.max:
            print(f'   ... {len(rs) - args.max} more')
    if args.vram:
        va, vb = (open(p, 'rb').read() for p in args.vram)
        regions = [(int(lo, 16), int(hi, 16), what) for lo, hi, what in args.region or []]
        for lo, hi, what in regions or [(0, 0x10000, 'video memory')]:
            n = sum(1 for o in range(lo * 4, hi * 4) if va[o] != vb[o])
            total += n
            print(f'vram {lo:04X}..{hi - 1:04X} {what:30} {n:6} bytes differ')
    sys.exit(1 if total else 0)


if __name__ == '__main__':
    main()
