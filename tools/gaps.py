#!/usr/bin/env python3
"""List what of the code segment the analysis has not reached.

    gaps.py HINTS [-n N] [--seg SEG|all] [--cover FILE]

Each gap is shown with its first instructions (linear decoding), to decide
whether it is code only reached through a pointer or data.  --seg all goes
through every segment of class CODE.  --cover takes the file of a run
with the runner's -cover (run.py -cover FILE ...): a gap in which
instructions began in that run is marked `RAN` with how many and the
first of them, so code the analysis misses shows apart from code that
did not run (the program's first load in the run is taken).  For an LE
image the runner records where the extender mapped each object, and
those addresses are used instead of the MZ load segment."""
import argparse, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import disasm


def read_cover(path, p, exe):
    """the image offsets instructions began at in a -cover file's run of
    the program named exe (its first load).  An LE image (p.kind 'le') is
    not at the MZ load segment but where the extender mapped its objects,
    which the load line names as OBJ@LINEAR after the segment."""
    base = os.path.basename(exe.replace('\\', '/')).upper()
    load, lins = None, []
    for line in open(path):
        f = line.split()
        if not f or f[0].startswith('#'):
            continue
        if f[0] == 'load':
            if load is None and f[1].upper() == base:
                load = f[2:]
        else:
            lins.append(int(f[0], 16))
    if load is None:
        raise SystemExit(f'{path}: {base} was not loaded in that run')
    if p.kind != 'le':
        start = int(load[0], 16) * 16
        return {a - start for a in lins if a >= start}
    maps = []                   # (linear, size, image offset) per object
    for m in load[1:]:
        obj, lin = m.split('@')
        ib, size = p.descs[int(obj) - 1]
        maps.append((int(lin, 16), size, ib))
    if not maps:
        raise SystemExit(f'{path}: no LE object of {base} found in memory in that run '
                         '(a cover file from an older runner, or the extender had not mapped it)')
    return {ib + a - lin for a in lins for lin, size, ib in maps if lin <= a < lin + size}

ap = argparse.ArgumentParser()
ap.add_argument('hints')
ap.add_argument('-n', type=int, default=4, help='instructions shown per gap')
ap.add_argument('--seg', default='CODE', help='a segment, or all')
ap.add_argument('--cover', help="a run's -cover file")
a = ap.parse_args()
an, em = disasm.generate(a.hints)
md = an.md
segs = [S for S in an.segs if S.cls == 'CODE'] if a.seg == 'all' else [an.byname[a.seg]]
ran = read_cover(a.cover, an.p, an.h.exe) if a.cover else None

imm_at = {}
for (s, o), ins in an.insns.items():
    ci = ins.ci
    if ci.imm_size == an.w:
        v = int.from_bytes(ci.bytes[ci.imm_offset:ci.imm_offset + an.w], 'little')
        imm_at.setdefault(v, []).append(f'{s}:{o:04X} {ci.mnemonic} {ci.op_str}')
img = an.p.img
nran = 0
for S in segs:
    g = disasm.gaps(an, S.name)
    tot = sum(e - s for s, e in g)
    print(f'{S.name}: {len(g)} gaps, {tot} of {S.size} bytes not reached as code')
    for s, e in g:
        b = bytes(img[S.base + s:S.base + e])
        lab = an.labels.get((S.name, s), '')
        zero = ' (zeros)' if not any(b) else ''
        mark = ''
        if ran is not None:
            hit = sorted(o - S.base for o in range(S.base + s, S.base + e) if o in ran)
            if hit:
                mark = f' RAN: {len(hit)} instructions, the first at {S.name}:{hit[0]:04X}'
                nran += 1
        print(f'{s:04X}-{e:04X} {e - s:5d} {lab}{zero}{mark}')
        if zero:
            continue
        for i, ins in enumerate(md.disasm(b, s)):
            if i >= a.n:
                break
            print(f'        {ins.address:04X} {ins.bytes.hex():14s} {ins.mnemonic} {ins.op_str}')

    # where each gap's start appears as a word: immediates of reached
    # instructions, and data
    print(f'\n{S.name}: where gap starts appear as words:')
    for s, e in g:
        b = bytes(img[S.base + s:S.base + e])
        if not any(b):
            continue
        for k in range(s, e):
            if k != s and (S.name, k) not in an.labels:
                continue
            hits = list(imm_at.get(k, []))
            w = k.to_bytes(an.w, 'little')
            p = img.find(w)
            while p >= 0 and len(hits) < 8:
                T = an.seg_at(p)
                if T and T.cls != 'CODE':
                    hits.append(f'{T.name}:{p - T.base:04X} (data)')
                p = img.find(w, p + 1)
            if hits:
                print(f'  {k:04X}: ' + '; '.join(hits[:8]))
    print()
if ran is not None:
    print(f'{nran} gaps ran in that run')
