#!/usr/bin/env python3
"""Carry hints from one program to a sibling built from the same source.

    xfer.py FROM.hints TO.hints [--check]

Two programs of one game often share most of their code (one engine,
two builds) at other addresses.  This aligns the two instruction streams
(the analysed one of FROM, a linear decoding of TO's code; each stream
all the segments of class CODE in image order, each from its start=),
matching instructions by their shape (mnemonic, registers, kinds of
operands).  Matched pairs give the address map for code; the relocated
words inside matched pairs (a far call's segment, MOV AX,SEG) and where
the matched code lies give the map of segment names; the memory operands
and immediates of matched pairs vote for the map of every other segment.
Each hint of FROM is then written for TO with its addresses and segment
names mapped, or commented out with the reason.  The comments above a
hint come with it; those above a hint that is not carried (the file's
header, a block about FROM's own segments) stay behind, as they describe
FROM and not TO.

TO.hints must exist with at least exe and segment lines (and its own
linker, relocorder, asm and keeptail lines: those are not carried); its
other lines are kept and the carried ones appended after a marker
(replacing any earlier carried block).  A name the file's own lines give an
address stays when the carried block names it otherwise (disasm.py): data a
sibling uses otherwise than the program the hints come from is named by hand."""
import argparse, bisect, difflib, os, re, struct, sys
from collections import Counter, defaultdict
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import disasm
import capstone
from capstone import x86

MARK = '; ==== carried over from '


def shape(ci):
    """what must be equal for two instructions to match"""
    parts = [ci.mnemonic]
    branch = ci.group(capstone.CS_GRP_JUMP) or ci.group(capstone.CS_GRP_CALL)
    for op in ci.operands:
        if op.type == x86.X86_OP_REG:
            parts.append(ci.reg_name(op.reg))
        elif op.type == x86.X86_OP_IMM:
            # a jump's or call's target is an address, and so, mostly, is
            # a word immediate (a constant below 100h is mostly a byte one):
            # their values move with the code, only a small constant's counts
            v = op.imm & 0xFFFF
            if branch:
                parts.append('iT')
            else:
                parts.append('i' + (str(v) if v < 0x100 and ci.imm_size != 2 else 'W'))
        elif op.type == x86.X86_OP_MEM:
            m = op.mem
            parts.append('m%d%s%s%s%s' % (op.size, ci.reg_name(m.base) if m.base else '',
                                          ci.reg_name(m.index) if m.index else '',
                                          ci.reg_name(m.segment) if m.segment else '',
                                          str(m.disp) if ci.disp_size == 1 else ('W' if ci.disp_size else '')))
    return ' '.join(parts)


def code_segs(an):
    return [S for S in an.segs if S.cls == 'CODE']


def linear(an):
    """((segment, offset), capstone insn) for all the code segments, each
    decoded in a line from its start; instructions the analysis knows are
    taken from it"""
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_16)
    md.detail = True
    out = []
    img = an.p.img
    for S in code_segs(an):
        o = S.start
        while o < S.size:
            ins = an.insns.get((S.name, o))
            if ins:
                out.append(((S.name, o), ins.ci))
                o += ins.size
                continue
            ci = next(md.disasm(bytes(img[S.base + o:S.base + min(o + 16, S.size)]), o), None)
            if ci is None:
                o += 1
                continue
            out.append(((S.name, o), ci))
            o += ci.size
    return out


def build_maps(a, b, la, lb):
    ta, tb = [shape(c) for _, c in la], [shape(c) for _, c in lb]
    sm = difflib.SequenceMatcher(None, ta, tb, autojunk=False)
    code = {}
    segvotes = defaultdict(Counter)    # segment name in A -> Counter(name in B)
    pairs = []
    for i, j, n in sm.get_matching_blocks():
        if n < 3:
            continue
        for k in range(n):
            (sa, oa), ca = la[i + k]
            (sb, ob), cb = lb[j + k]
            code[(sa, oa)] = (sb, ob)
            segvotes[sa][sb] += 1
            pairs.append(((sa, oa), ca, cb))
            # a relocated word at the same place in both: the frames agree
            pa, pb = a.byname[sa].base + oa, b.byname[sb].base + ob
            for x in range(ca.size):
                fa, fb = a.p.relsites.get(pa + x), b.p.relsites.get(pb + x)
                if fa in a.byframe and fb in b.byframe:
                    segvotes[a.byframe[fa].name][b.byframe[fb].name] += 1
    segmap = {s: c.most_common(1)[0][0] for s, c in segvotes.items()}
    votes = defaultdict(Counter)       # (seg, off) in A -> Counter((seg, off) in B)
    for (sa, oa), x, y in pairs:
        # operands with 16-bit values: displacements and immediates
        for what in ('disp', 'imm'):
            if what == 'disp' and x.disp_size == 2 and y.disp_size == 2:
                va = int.from_bytes(x.bytes[x.disp_offset:x.disp_offset + 2], 'little')
                vb = int.from_bytes(y.bytes[y.disp_offset:y.disp_offset + 2], 'little')
            elif what == 'imm' and x.imm_size == 2 and y.imm_size == 2:
                va = int.from_bytes(x.bytes[x.imm_offset:x.imm_offset + 2], 'little')
                vb = int.from_bytes(y.bytes[y.imm_offset:y.imm_offset + 2], 'little')
            else:
                continue
            insa = a.insns.get((sa, oa))
            if insa is None or what not in insa.refs:
                continue
            ref = insa.refs[what]
            if isinstance(ref, str) or a.byname[ref[0]].cls == 'CODE':
                continue
            votes[ref][(segmap.get(ref[0], ref[0]), (ref[1] + (vb - va)) & 0xFFFF)] += 1
    data = {}
    for ref, c in votes.items():
        (t, v), n = c.most_common(1)[0]
        data[ref] = (t, v)
    return code, data, segmap, len(pairs)


def map_data(ref, data, near=0x40):
    """a data address: exact vote, else shifted like the nearest voted one
    below it (within `near` bytes)"""
    if ref in data:
        return data[ref]
    s, o = ref
    best = None
    for (s2, o2), (t, v) in data.items():
        if s2 == s and 0 <= o - o2 <= near and (best is None or o2 > best[0]):
            best = (o2, t, v)
    if best:
        return best[1], (best[2] + o - best[0]) & 0xFFFF
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('src')
    ap.add_argument('dst')
    ap.add_argument('--check', action='store_true',
                    help="only say whether DST's carried block is up to date")
    args = ap.parse_args()
    a, _ = disasm.generate(args.src)
    # the target, analysed with its own lines only (none carried yet)
    text = open(args.dst, encoding='utf-8').read()
    own = text.split(MARK)[0].rstrip('\n') + '\n'
    tmp = args.dst + '.tmp'
    open(tmp, 'w', encoding='utf-8').write(own)
    try:
        b, _ = disasm.generate(tmp)
    finally:
        os.remove(tmp)
    la, lb = linear(a), linear(b)
    code, data, segmap, matched = build_maps(a, b, la, lb)
    print(f'{matched} of {len(la)} / {len(lb)} instructions matched, {len(data)} data '
          f'addresses and {len(segmap)} segments mapped', file=sys.stderr)
    # a code address carries over only if the instructions from there on
    # look the same; else it goes where that look is found once
    ia = {o: i for i, (o, _) in enumerate(la)}
    ib = {o: i for i, (o, _) in enumerate(lb)}
    sa = [shape(c) for _, c in la]
    sb = [shape(c) for _, c in lb]
    K = 10
    first = defaultdict(list)           # a shape in B -> where it stands
    for j, s in enumerate(sb):
        first[s].append(j)
    codekeys = defaultdict(list)        # a code segment of A -> its mapped offsets
    for s, o in sorted(code):
        codekeys[s].append(o)

    def check_code(o):
        i = ia.get(o)
        if i is None:
            return None
        want = sa[i:i + K]
        t = code.get(o)
        if t is not None and ib.get(t) is not None and sb[ib[t]:ib[t] + K] == want:
            return t
        hits = [lb[j][0] for j in first.get(want[0], ()) if sb[j:j + K] == want]
        return hits[0] if len(hits) == 1 else None

    cache = {}

    def code_at(o):
        if o not in cache:
            cache[o] = check_code(o)
        return cache[o]

    def is_code(s):
        return a.byname[s].cls == 'CODE'

    def conv(tok):
        s, o = tok.split(':')
        if '-' in o:
            x, y = (int(v, 16) for v in o.split('-'))
            t, u = code_at((s, x)), code.get((s, y))
            if is_code(s) and t is not None and u is not None and t[0] == u[0]:
                return f'{t[0]}:{t[1]:04X}-{u[1]:04X}'
            return None
        o = int(o, 16)
        if is_code(s):
            t = code_at((s, o))
            if t is None and (s, o) not in ia:
                # data in a code segment (a switch table) or inside an
                # instruction: shifted like the nearest mapped instruction
                # below it in its segment
                ks = codekeys.get(s, [])
                k = bisect.bisect_right(ks, o) - 1
                if k >= 0 and o - ks[k] <= 0x100:
                    t2 = code[(s, ks[k])]
                    t = (t2[0], (t2[1] + o - ks[k]) & 0xFFFF)
            return f'{t[0]}:{t[1]:04X}' if t is not None else None
        r = map_data((s, o), data)
        return f'{r[0]}:{r[1]:04X}' if r else None

    def words_agree(f, new):
        """a table of code pointers carries only if the target's words at
        the mapped place are the mapped routines (tables in data a program
        has for itself, such as the objects of its tables, do not)"""
        (s, o), (s2, o2) = (x.split(':') for x in (f[1], new[1]))
        A, B = a.byname[s], b.byname[s2]
        stride = next((int(x[7:], 16) for x in f[4:] if x.startswith('stride=')), 2)
        for i in range(int(f[2], 16)):
            va = struct.unpack_from('<H', a.p.img, A.base + int(o, 16) + stride * i)[0]
            p = B.base + int(o2, 16) + stride * i
            vb = struct.unpack_from('<H', b.p.img, p)[0] if p + 2 <= len(b.p.img) else None
            t = code_at((f[3], va))
            if t is None or t != (new[3], vb):
                return False
        return True

    out = [MARK + os.path.basename(args.src) + ' by tools/xfer.py; check, then keep or edit']
    skip = ('exe', 'segment', 'relocorder', 'asm', 'linker', 'keeptail')
    n_ok = n_bad = 0
    lines = []                  # (source line, its words mapped or None)
    for line in open(args.src, encoding='utf-8'):
        line = line.rstrip('\r\n')
        body, semi, note = line.partition(';')
        f = body.split()
        if not f:
            if semi:
                lines.append((line, 'comment'))
            continue
        if f[0] in skip:
            lines.append((line, 'skip'))
            continue
        new = []
        for i, t in enumerate(f):
            if i > 0 and re.fullmatch(r'\w+:[0-9A-F]+(-[0-9A-F]+)?', t) and t.split(':')[0] in a.byname:
                c = conv(t)
            elif i > 0 and t in a.byname:
                c = segmap.get(t)     # a segment by its name
            else:
                c = t
            if c is None:
                new = None
                break
            new.append(c)
        if new and f[0] == 'words' and is_code(f[3]) and not words_agree(f, new):
            new = None
        lines.append((line, new and (new, (' ' + semi + note) if semi else '')))
    # two routines that differ only in an immediate (MOV AX,1201h / 1200h)
    # can both map to the one the target has, and two data addresses to
    # one: then none of the names is sure, and none is carried
    names = {}
    for line in own.split('\n'):
        f = line.split(';', 1)[0].split()
        if len(f) == 3 and f[0] == 'name':
            names.setdefault(f[1], []).append(f[2])
    for line, new in lines:
        if new not in (None, 'comment') and new[0][0] == 'name' and len(new[0]) > 2:
            names.setdefault(new[0][1], []).append(new[0][2])
    pending = []                # comment lines waiting for the hint they belong to
    for line, new in lines:
        if new == 'comment':
            pending.append(line)
            continue
        if new == 'skip':
            pending = []
            continue
        if new is None:
            out.append('; (not mapped) ' + line)
            n_bad += 1
        elif new[0][0] == 'name' and len(new[0]) > 2 and len(names[new[0][1]]) > 1:
            w = new[0][1]
            out.append(f'; (not mapped: {w} would be {" and ".join(names[w])}) ' + line)
            n_bad += 1
        else:
            out.extend(pending)
            out.append(' '.join(new[0]) + new[1])
            n_ok += 1
        pending = []
    result = own + '\n' + '\n'.join(out) + '\n'
    if args.check:
        if result != text:
            print(f'{args.dst}: carried block out of date (run tools/xfer.py '
                  f'{args.src} {args.dst})', file=sys.stderr)
            sys.exit(1)
        return
    # LF line ends on every platform: the hints are stored so, and a file
    # rewritten with the platform's would differ in every line
    open(args.dst, 'w', encoding='utf-8', newline='\n').write(result)
    print(f'{n_ok} hints carried, {n_bad} not mapped -> {args.dst}', file=sys.stderr)


if __name__ == '__main__':
    main()
