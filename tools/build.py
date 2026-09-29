#!/usr/bin/env python3
"""Make a program's source from the hints, assemble and link it, and
compare the result with the shipped program.

    build.py HINTS [--rounds N]

Writes build/NAME.ASM and build/NAME.EXE (in the project, see kit.py;
for a pMAX image NAME with the program's own extension).
Every line of the source knows the address it came from, so the
comparison works line by line: an instruction the assembler encodes
differently (tasm.py and the original's assembler do not always agree)
is reported and written as DB in the next round; a data line that
differs is a bug of disasm.py.  Ends with the byte comparison of the
whole file.  The EXE header is laid out as Microsoft LINK lays it out
(write_mz); a program linked otherwise needs its own layout there.  A
pMAX image is written by write_pmax.  A line tasm.py cannot assemble is
written as DB in the next round too.
"""
import argparse, os, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import disasm, tasm, tlink
from kit import build_dir


def write_mz(out, prog, reloc_segs):
    """The EXE the way Microsoft LINK lays it out: word 1 at 1Ch, the
    relocations from 1Eh, the header padded to 512 bytes."""
    img = out.img
    end = len(img)
    while end > 0 and not out.init[end - 1]:
        end -= 1
    stored = bytes(img[:end])
    rel = []
    for frame in reloc_segs:
        rel += sorted(a for a, fr, _ in out.relocs if fr == frame)
    hdr_len = (0x1E + 4 * len(rel) + 511) // 512 * 512
    size = hdr_len + len(stored)
    minalloc = (len(img) - len(stored) + 15) // 16
    h = bytearray(hdr_len)
    struct.pack_into('<2s13H', h, 0, b'MZ', size % 512, (size + 511) // 512, len(rel),
                     hdr_len // 16, minalloc, 0xFFFF, out.ss, out.sp, 0, out.ip, out.cs, 0x1E, 0)
    struct.pack_into('<H', h, 0x1C, 1)
    fr_of = {a: fr for a, fr, _ in out.relocs}
    for i, a in enumerate(rel):
        fr = fr_of[a]
        struct.pack_into('<HH', h, 0x1E + 4 * i, a - fr * 16, fr)
    return bytes(h) + stored


def write_pmax(out, segs):
    """A pMAX flat image from the linked program: the hints' segments are
    the descriptors, in their order; a selector word (SEG fixup) becomes a
    relocation to the descriptor of its segment, the word itself 0.  Not
    known and written as the one image examined has them: the header's
    first word (0), the format number (1), the size to allocate (the
    image's), the relocations in address order, uninitialised bytes stored
    as zeros, the entry point as an image offset."""
    img = bytearray(out.img)
    desc = {}
    for i, s in enumerate(segs):
        desc.setdefault(out.byname[s.name]['frame'], i)
    rel = []
    for a, fr, _ in out.relocs:
        rel.append((a, desc[struct.unpack_from('<H', img, a)[0]]))
        img[a:a + 2] = b'\0\0'
    rel.sort()
    h = struct.pack(disasm.PMAX_HDR, 0, len(img), 1, len(segs), len(img), out.cs * 16 + out.ip, len(rel))
    for s in segs:
        sg = out.byname[s.name]
        h += struct.pack('<II', sg['start'], sg['end'] - sg['start'])
    return h + bytes(img) + b''.join(struct.pack('<IB', a, d) for a, d in rel)


def assemble(asm_path, an, em):
    """tasm.py on the source; the lines it refuses are returned as the
    addresses they came from, to be written as DB"""
    a = tasm.Assembler(asm_path)
    for k, v in an.h.asm.items():         # the hints' `asm` switches
        setattr(a, k, v)
    a.collect = []
    a.assemble()
    refused = {}
    for passno, (where, lineno), err, text in a.collect:
        loc = em.map[lineno - 1] if lineno <= len(em.map) else None
        if loc is None or (loc[0], loc[1]) not in an.insns:
            raise tasm.AsmError(f'{where}:{lineno}: {err}\n    {text}')
        refused[(loc[0], loc[1])] = (loc[0], loc[1], 'asm', f'{text}  {err}')
    return a, list(refused.values())


def build_once(hints, raw):
    an, em = disasm.generate(hints, raw)
    name = os.path.splitext(os.path.basename(hints))[0]
    asm_path = build_dir(name + '.ASM')
    open(asm_path, 'w', newline='\r\n').write('\n'.join(em.lines) + '\n')
    a, refused = assemble(asm_path, an, em)
    if refused:
        return an, em, a, None, None, refused, name
    m = tlink.module_from_asm(a, name)
    out = tlink.link([m])
    # line by line, independent of where earlier lines put it: a line is
    # wrong if it has another length, or other bytes outside the fields
    # that hold addresses (those follow from the lines before)
    img = an.p.img
    bad = []
    for sname, entries in a.linemap.items():
        S = an.byname[sname]
        base = out.byname[sname]['pieces'][0].base
        segend = a.segments[sname].size
        for i, (pc, where, lineno, text) in enumerate(entries):
            loc = em.map[lineno - 1] if lineno - 1 < len(em.map) else None
            if loc is None:
                continue
            s, off, n = loc
            nxt = next((e[0] for e in entries[i + 1:] if e[0] != pc), segend)
            length = nxt - pc
            if length != n:
                bad.append((s, off, 'size', f'{text.strip()}  {length} bytes, want {n}'))
                continue
            if S.base + off >= len(img):
                continue
            got = bytes(out.img[base + pc:base + pc + n])
            want = bytes(img[S.base + off:S.base + off + n])
            if got == want:
                continue
            mask = set()
            ins = an.insns.get((s, off))
            if ins is not None:
                ci = ins.ci
                if ci.disp_size:
                    mask |= set(range(ci.disp_offset, ci.disp_offset + ci.disp_size))
                if ci.imm_size:
                    mask |= set(range(ci.imm_offset, ci.imm_offset + ci.imm_size))
                if ci.mnemonic in disasm.JUMPS or ci.mnemonic == 'call':
                    mask |= set(range(1, n))
            elif 'DW ' in text or 'DD ' in text or 'DF ' in text:
                continue
            if any(got[k] != want[k] for k in range(n) if k not in mask):
                bad.append((s, off, 'bytes', f'{text.strip()}  got {got.hex()} want {want.hex()}'))
    if an.p.kind == 'pmax':
        exe = write_pmax(out, an.segs)
    elif an.p.kind == 'bin':
        if out.relocs:
            raise SystemExit('a raw image has no relocations: a segment reference in the source')
        exe = bytes(out.img)
    else:
        exe = write_mz(out, an.p, [an.byname[s].frame for s in an.h.relocorder])
    if an.h.keeptail:
        exe += an.p.tail
    return an, em, a, out, exe, bad, name


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('hints')
    ap.add_argument('--rounds', type=int, default=6)
    args = ap.parse_args()
    raw = set()
    t0 = time.time()
    for rnd in range(args.rounds):
        an, em, a, out, exe, bad, name = build_once(args.hints, raw)
        new = set((s, off) for s, off, kind, text in bad if (s, off) in an.insns)
        if not bad:
            break
        print(f'round {rnd}: {len(bad)} lines differ, {len(new)} instructions to DB')
        for s, off, kind, text in sorted(bad, key=lambda b: (b[0], b[1]))[:8]:
            print(f'   {s}:{off:04X} {kind}: {text}')
        if not new:
            break
        raw |= new
    if exe is None:
        sys.exit(f'lines tasm.py refuses are left after {args.rounds} rounds')
    ext = os.path.splitext(an.h.exe)[1] if an.p.kind in ('pmax', 'bin') else '.EXE'
    path = build_dir(name + ext)
    open(path, 'wb').write(exe)
    ref = an.p.file
    same = exe == ref
    print(f'{len(an.insns)} instructions, {len(an.labels)} labels, {len(an.h.raw)} as DB; '
          f'{path}: {"IDENTICAL" if same else "differs"} ({time.time() - t0:.1f} s)')
    if raw:
        print('written as DB (add a raw hint or teach disasm.py):')
        for s, off in sorted(raw):
            ins = an.insns[(s, off)]
            print(f'   raw {s}:{off:04X}   ; {ins.ci.mnemonic} {ins.ci.op_str}  [{ins.ci.bytes.hex()}]')
    if not same:
        n = min(len(exe), len(ref))
        diff = next((i for i in range(n) if exe[i] != ref[i]), n)
        print(f'first difference at file offset {diff:05X} (sizes {len(exe)} / {len(ref)})')
        sys.exit(1)


if __name__ == '__main__':
    main()
