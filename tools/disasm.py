#!/usr/bin/env python3
"""Turn a shipped program back into assembly source that tasm.py rebuilds
to the same bytes.

    disasm.py HINTS [-o OUT.ASM]

HINTS names the program (relative to the game's files, see kit.py, or
`build/...` for a file a project's tool unpacked into its build folder)
and says what the analysis cannot find out by itself: the segments, entry points the code reaches
only through pointers, tables, names and comments.  It holds no bytes of
the game, so the source is made from the player's own copy each time.

The analysis:
  * code: recursive descent from the entry points, following jumps and
    calls.  Along each path it tracks what DS and ES hold (a segment of
    the program or an unknown value), so that a direct address can be
    written as a label of the segment it really reads;
  * relocations: every segment value in the program is a relocation site,
    written as the segment's name (MOV AX,DATA; DD far pointers);
  * data: the bytes nothing decoded as code, as DB lines (text as
    strings), cut at every label.

Kinds of program:
  * an MZ program (`exe`), 16-bit real mode;
  * a pMAX flat image (`pmax`), the 32-bit protected-mode program of the
    pMAX DOS extender: its descriptors are the segments, its selector
    relocations name a descriptor (the word itself is 0 in the file).
    The source has USE32 segments; offsets, near pointers and `words`
    tables are 32 bits wide.  With one segment (a flat program) DS holds
    CODE too.
  * a little-endian LE image (`le`), often embedded in a DOS/4GW MZ file;
    objects are segments, with uncompressed pages and internal selector
    fixups.
  * a raw 32-bit image (`bin`): the file is the image, offsets from 0,
    entered at 0, no header and no relocations (a driver a program loads
    into a segment of its own and calls); one segment, number 0, the
    whole file.  Written back as the image alone.  With `noentry` nothing
    is entered at 0 (a module whose host calls it through pointers).
    With an `offrel` list (a module its host relocates by adding the
    base it loaded it at to every listed dword) exactly the listed
    dwords are offsets: an instruction's immediate or displacement
    there, else a DD; every other number stays a number.  build.py
    checks that the rebuilt source puts its offsets at exactly those
    places.

Hints syntax (one per line, ';' starts a comment, numbers are hex):
    exe        GAME/GAME.EXE           an MZ program
    pmax       GAME/GAME.386           a pMAX image
    le         GAME/GAME.EXE           an LE image
    bin        GAME/GAME.DRV [noentry] a raw 32-bit image
    offrel     GAME/GAME.REL [flags=MASK]   the image's offset relocations:
                                       little-endian 32-bit image offsets,
                                       each of a dword holding an offset;
                                       the bits of MASK in such a dword are
                                       flags, not part of the offset
                                       (written DD label+FLAGS)
    segment    NAME FRAME CLASS [stack] [size=N] [start=N] [prefix=P]
                                       in image order; in a pMAX image FRAME
                                       is the descriptor's number, in LE it
                                       is the object number minus one (base
                                       and size come from the image). start=N: the
                                       segment's first byte is at offset N
                                       (below 10h) of its frame, as a BYTE-
                                       or WORD-aligned segment the linker put
                                       after the previous one's end (written
                                       SEGMENT BYTE); size=N counts from the
                                       frame, not from the start.  Labels are made of
                                       the name's first letter and the
                                       offset, code labels of L and the
                                       offset (L, the letter and the offset
                                       outside a CODE-class segment); with
                                       prefix=P they are P+offset and
                                       L+P+offset (a program of many code
                                       segments, whose labels would collide)
    code       SEG:OFF [NAME]          an entry point
    coderange  SEG:OFF-END             code throughout, a routine after every
                                       RET/JMP (handlers reached by pointers)
    words      SEG:OFF COUNT TARGETSEG a table of near pointers into TARGETSEG
                                       (a TARGETSEG of class CODE also seeds
                                       code there); 32-bit
                                       ones in a pMAX or LE image
    words      SEG:OFF COUNT TARGETSEG stride=N
                                       COUNT pointers N bytes apart (a field
                                       of records), the bytes between as data
    rwords     SEG:OFF COUNT           a table of signed 16-bit offsets from
                                       the table's own start (a compiled
                                       switch: LEA reg,[reg+table]; JMP reg),
                                       code in SEG; written DW target-table
    rwords     SEG:OFF COUNT [stride=N] [from=OFF]
                                       the same with one offset every N bytes
                                       (a field of records, the bytes between
                                       as data), counted from SEG:OFF
    name       SEG:OFF NAME            a label's name (each name once: a
                                        name given twice is refused)
    ptr        SEG:OFF TARGETSEG       the immediate of the instruction at
                                       SEG:OFF is an offset in TARGETSEG, code
                                       there if its class is CODE (without
                                       an immediate: its address operand, as for
                                       a LEA of what is read with another DS)
    dptr       SEG:OFF TARGETSEG       the same for an offset of data (no code
                                       is looked for there)
    var        SEG:OFF SEG|num         what a word variable holds (offsets in
                                       SEG, or numbers); the analysis finds
                                       most pointer variables by itself
    num        SEG:OFF                 the instruction's address operand
                                       stays a number
    ds         SEG:OFF-END DSSEG       what DS holds in that code range
    es         SEG:OFF-END ESSEG       the same for ES
    comment    SEG:OFF TEXT            a comment line before the address
    raw        SEG:OFF                 write the instruction as DB (the
                                       assembler would pick other bytes)
    stop       SEG:OFF                 the instruction at SEG:OFF does not
                                       return (an exit through a service the
                                       analysis does not know): nothing after
                                       it is code by falling through
    keeptail                           the bytes after the program image
                                       (debug information) are copied from
                                       the original, not made
    relocorder SEG SEG...              the order of the relocation table:
                                       by the segment holding the site
    relocorder original                the order of the original's table
                                       (a linker that writes them as it
                                       meets them in the object records,
                                       as TLINK does); the set is still
                                       compared
    linker     tlink VER               the header as Borland's TLINK writes
                                       it (relocations from 3Eh, its mark
                                       01 00 FB VER 6A 72 at 1Ch; VER the
                                       original's byte at 1Fh); Microsoft
                                       LINK's otherwise
    linker     tlink VER header=original   the same with the header as long
                                       as the original's (a TLINK that
                                       leaves room after the relocations
                                       beyond the next 512 bytes, as 5.0
                                       does at times: zeros)
    asm        OPTION=VALUE...         how the original's assembler encoded
                                       what has two encodings (tasm.py's
                                       defaults otherwise):
                                         lea_smart=0     LEA reg,[addr] stays
                                                         LEA (not MOV reg,addr)
                                         alu_ax_short=0  ALU AX,imm8 in the
                                                         sign-extended 83h form
                                         test_form=rm_reg  TEST r,r with the
                                                         first register in r/m
                                         imm8_alu=OP,OP...  only these ALU
                                                         operations (add, or,
                                                         adc, sbb, and, sub,
                                                         xor, cmp) take the
                                                         83h byte form
                                         xchg_ax_short=0  XCHG AX,reg as 87h /r
"""
import argparse, os, re, struct, sys
import capstone
from capstone import x86

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from kit import game_dir, build_dir


REG = {}


def hexnum(n):
    if n < 10:
        return str(n)
    h = f'{n:X}H'
    return '0' + h if h[0] in 'ABCDEF' else h


# ---------------------------------------------------------------- the program

class Seg:
    def __init__(self, name, frame, cls, stack=False, size=None, prefix=None, start=0):
        self.name, self.frame, self.cls, self.stack = name, frame, cls, stack
        self.base = frame * 16
        self.start = start        # offset of the first byte (a segment not on a paragraph)
        self.size = size          # set from the next segment when not given
        self.prefix = prefix or name[0]
        self.own_prefix = prefix is not None


def program_path(name):
    """a program as the hints name it: relative to the game's files, or
    build/... in the project's build folder (a file a tool unpacked)"""
    parts = name.replace('\\', '/').split('/')
    if parts[0] == 'build':
        return build_dir(*parts[1:])
    return os.path.join(game_dir(), name)


class Program:
    """an MZ program"""
    kind, bits, descs = 'mz', 16, None
    offsites = None           # an offrel list (a raw image's)

    def __init__(self, path):
        d = open(path, 'rb').read()
        self.file = d
        (sig, cblp, cp, crlc, cparhdr, minalloc, maxalloc, ss, sp, csum, ip, cs,
         lfarlc, ovno) = struct.unpack_from('<2s13H', d)
        assert sig == b'MZ'
        size = (cp - 1) * 512 + (cblp or 512)
        self.hdr = d[:cparhdr * 16]
        self.img = d[cparhdr * 16:size]
        self.minalloc, self.maxalloc = minalloc, maxalloc
        self.ss, self.sp, self.cs, self.ip = ss, sp, cs, ip
        self.relocs = [struct.unpack_from('<HH', d, lfarlc + 4 * i) for i in range(crlc)]
        self.tail = d[size:]                   # after the image (debug information)
        self.relsites = {}                     # image offset -> segment value
        for off, seg in self.relocs:
            a = seg * 16 + off
            self.relsites[a] = struct.unpack_from('<H', self.img, a)[0]

    def entry(self, byframe):
        return byframe[self.cs].name if self.cs in byframe else 'CODE', self.ip


PMAX_HDR = '<IIBBIIH'


class PmaxProgram:
    """A pMAX flat image: a 20-byte header, the descriptors (base and
    size, 32 bits each), the image, the selector relocations (32-bit image
    offset, 8-bit descriptor number) to the end of the file.  The header:
    a word not understood (0 in the image examined), the size to allocate,
    a format number (1), the number of descriptors, the image's size, the
    entry point (an image offset), the number of relocations."""
    kind, bits = 'pmax', 32
    offsites = None

    def __init__(self, path):
        d = open(path, 'rb').read()
        self.file = d
        (self.word0, self.alloc, self.version, n, size, self.ip,
         nrel) = struct.unpack_from(PMAX_HDR, d)
        self.descs = [struct.unpack_from('<II', d, 20 + 8 * i) for i in range(n)]
        start = 20 + 8 * n
        self.img = d[start:start + size]
        rel = start + size
        if rel + 5 * nrel != len(d):
            raise SystemExit(f'{path}: not a pMAX image ({len(d)} bytes, header says {rel + 5 * nrel})')
        self.relocs = [struct.unpack_from('<IB', d, rel + 5 * i) for i in range(nrel)]
        self.relsites = dict(self.relocs)      # image offset -> descriptor number
        self.tail = b''

    def entry(self, byframe):
        for S in byframe.values():
            if S.base <= self.ip < S.base + S.size:
                return S.name, self.ip - S.base
        raise SystemExit(f'entry point {self.ip:X} in no segment')


class LEProgram(PmaxProgram):
    """LE objects mapped into a flat image. This first implementation
    accepts little-endian LE files with legal, zero-filled or invalid pages;
    other page encodings are rejected explicitly."""
    kind, bits = 'le', 32

    @staticmethod
    def valid_header(d, h):
        if h < 0 or h + 0xb0 > len(d) or d[h:h + 2] != b'LE' or d[h + 2:h + 4] != b'\0\0':
            return False
        try:
            pages, page_size = struct.unpack_from('<II', d, h + 0x14)[0], struct.unpack_from('<I', d, h + 0x28)[0]
            object_off, objects = struct.unpack_from('<II', d, h + 0x40)
            page_map = struct.unpack_from('<I', d, h + 0x48)[0]
        except struct.error:
            return False
        return (0 < pages < 1_000_000 and 0 < objects < 256 and
                0 < page_size <= 65536 and not page_size & (page_size - 1) and
                object_off + objects * 24 <= len(d) - h and
                page_map + pages * 4 <= len(d) - h)

    def __init__(self, path):
        d = open(path, 'rb').read()
        self.file = d
        off = None
        if len(d) >= 0x40:
            candidate = struct.unpack_from('<I', d, 0x3c)[0]
            if self.valid_header(d, candidate):
                off = candidate
        if off is None:
            hits = [i for i in range(len(d) - 0xb0 + 1)
                    if d[i:i + 2] == b'LE' and self.valid_header(d, i)]
            if len(hits) != 1:
                raise SystemExit(f'{path}: expected one valid embedded LE header, found {len(hits)}')
            off = hits[0]
        self.leoff = off
        # A bound executable may prepend another DOS program. File-relative
        # LE fields then belong to the MZ image whose header points here.
        origins = []
        for start in range(off):
            if d[start:start + 2] != b'MZ' or start + 0x40 > off:
                continue
            header_size = struct.unpack_from('<H', d, start + 8)[0] * 16
            link = struct.unpack_from('<I', d, start + 0x3c)[0]
            if header_size >= 0x40 and start + header_size <= off and start + link == off:
                origins.append(start)
        if len(origins) > 1:
            raise SystemExit(f'{path}: ambiguous MZ origin for LE header')
        self.file_origin = origins[0] if origins else 0
        h = off
        u32 = lambda x: struct.unpack_from('<I', d, h + x)[0]
        if d[h + 2:h + 4] != b'\0\0':
            raise SystemExit(f'{path}: LE byte/word order is unsupported')
        self.npages = u32(0x14)
        self.entry_object, self.ip = u32(0x18), u32(0x1c)
        self.stack_object, self.esp = u32(0x20), u32(0x24)
        self.page_size, self.last_page = u32(0x28), u32(0x2c)
        object_off, nobjects, page_map = u32(0x40), u32(0x44), u32(0x48)
        self.fixup_page_off, self.fixup_rec_off = u32(0x68), u32(0x6c)
        self.data_off = self.file_origin + u32(0x80)
        if self.data_off >= len(d):
            raise SystemExit(f'{path}: LE data pages start beyond the file')
        self.descs, self.object_flags, self.object_pages = [], [], []
        for i in range(nobjects):
            q = h + object_off + i * 24
            size, base, flags, first, count, reserved = struct.unpack_from('<6I', d, q)
            if reserved:
                raise SystemExit(f'{path}: LE object {i + 1} has a nonzero reserved field')
            self.descs.append((base, size))
            self.object_flags.append(flags)
            self.object_pages.append((first, count))
        total = max((base + size for base, size in self.descs), default=0)
        if total > 0x10000000:
            raise SystemExit(f'{path}: LE objects need an unreasonable {total:X} bytes')
        image = bytearray(total)
        self.pages, self.logical = [], {}
        for oi, ((base, size), (first, count)) in enumerate(zip(self.descs, self.object_pages)):
            if first == 0 and count:
                raise SystemExit(f'{path}: LE object {oi + 1} has a zero page-table index')
            if count > (size + self.page_size - 1) // self.page_size:
                raise SystemExit(f'{path}: LE object {oi + 1} has more pages than its virtual size')
            for j in range(count):
                logical = first - 1 + j
                if logical >= self.npages or logical in self.logical:
                    raise SystemExit(f'{path}: invalid or overlapping LE page index in object {oi + 1}')
                raw = d[h + page_map + logical * 4:h + page_map + logical * 4 + 4]
                if len(raw) != 4:
                    raise SystemExit(f'{path}: truncated LE object page table')
                disk_page, kind = int.from_bytes(raw[:3], 'big'), raw[3]
                self.logical[logical] = (oi, j)
                self.pages.append((logical, oi, j, disk_page, kind))
                start = base + j * self.page_size
                n = min(self.page_size, max(0, size - j * self.page_size))
                if kind == 0:
                    if disk_page == 0:
                        raise SystemExit(f'{path}: LE page {logical + 1} has no file page')
                    file_at = self.data_off + (disk_page - 1) * self.page_size
                    physical_n = self.last_page if disk_page == self.npages and self.last_page else self.page_size
                    take = min(n, physical_n)
                    if file_at + take > len(d):
                        raise SystemExit(f'{path}: truncated LE data page {disk_page}')
                    image[start:start + take] = d[file_at:file_at + take]
                elif kind not in (2, 3):
                    raise SystemExit(f'{path}: LE page kind {kind} is not supported yet')
        self.img = bytes(image)
        self.relocs, self.relsites = [], {}
        self.selector_sites = {}
        self._read_fixups(d, h)
        self.offsites, self.tail = None, b''

    @staticmethod
    def _field(d, p, n):
        if p + n > len(d):
            raise ValueError('truncated field')
        return int.from_bytes(d[p:p + n], 'little'), p + n

    def _read_fixups(self, d, h):
        fixup_size = struct.unpack_from('<I', d, h + 0x30)[0]
        if not self.fixup_page_off and not self.fixup_rec_off and not fixup_size:
            return
        if not self.fixup_page_off or not self.fixup_rec_off:
            raise SystemExit('incomplete LE fixup table offsets')
        page_table = h + self.fixup_page_off
        if page_table + 4 * (self.npages + 1) > len(d):
            raise SystemExit('truncated LE fixup page table')
        starts = [struct.unpack_from('<I', d, page_table + 4 * i)[0]
                  for i in range(self.npages + 1)]
        if starts != sorted(starts) or starts[-1] > fixup_size:
            raise SystemExit('invalid LE fixup page table')
        table = h + self.fixup_rec_off
        if table + starts[-1] > len(d):
            raise SystemExit('truncated LE fixup record table')
        for page, (begin, end) in enumerate(zip(starts, starts[1:])):
            pos, stop = table + begin, table + end
            loc = self.logical.get(page)
            if loc is None:
                if begin != end:
                    raise SystemExit(f'LE fixups for unmapped page {page + 1}')
                continue
            obj, page_in_obj = loc
            while pos < stop:
                src, flags = d[pos], d[pos + 1]
                pos += 2
                stype, listmode = src & 0x0f, bool(src & 0x20)
                source, pos = self._field(d, pos, 1 if listmode else 2)
                target = flags & 3
                obj_width = 2 if flags & 0x40 else 1
                val_width = 4 if flags & 0x10 else 2
                target_obj = None
                if target == 0:
                    target_obj, pos = self._field(d, pos, obj_width)
                    if stype != 2:
                        _, pos = self._field(d, pos, val_width)
                elif target in (1, 2):
                    _, pos = self._field(d, pos, obj_width)
                    ordinal_width = 1 if target == 1 and flags & 0x80 else val_width
                    _, pos = self._field(d, pos, ordinal_width)
                elif target == 3:
                    _, pos = self._field(d, pos, obj_width)
                if flags & 4:
                    _, pos = self._field(d, pos, 4 if flags & 0x20 else 2)
                sources = []
                if listmode:
                    for _ in range(source):
                        v, pos = self._field(d, pos, 2)
                        sources.append(v - 0x10000 if v & 0x8000 else v)
                else:
                    sources.append(source - 0x10000 if source & 0x8000 else source)
                if pos > stop:
                    raise SystemExit(f'LE fixup record runs past page {page + 1}')
                selector_delta = {2: 0, 3: 2, 6: 4}.get(stype)
                if selector_delta is not None:
                    for source in sources:
                        site = self.descs[obj][0] + page_in_obj * self.page_size + source + selector_delta
                        if site < self.descs[obj][0] or site + 2 > self.descs[obj][0] + self.descs[obj][1]:
                            raise SystemExit(f'LE selector fixup is outside object {obj + 1}')
                        if target == 0 and target_obj and target_obj > len(self.descs):
                            raise SystemExit(f'LE fixup names missing object {target_obj}')
                        target_index = target_obj - 1 if target == 0 and target_obj else -1
                        self.selector_sites[site] = target_index
                        self.relsites[site] = target_index
                        if target_index >= 0:
                            self.relocs.append((site, target_index))
            if pos != stop:
                raise SystemExit(f'malformed LE fixups on page {page + 1}')

    def entry(self, byframe):
        ix = self.entry_object - 1
        if ix < 0 or ix not in byframe:
            raise SystemExit(f'LE entry object {self.entry_object} is missing from hints')
        return byframe[ix].name, self.ip


class RawProgram(PmaxProgram):
    """A raw 32-bit image: the file itself, one descriptor (base 0, the
    file's size), entered at 0, no relocations."""
    kind = 'bin'

    def __init__(self, path, noentry=False, offrel=None):
        d = open(path, 'rb').read()
        self.file = self.img = d
        self.word0 = self.alloc = self.version = 0
        self.ip = None if noentry else 0
        self.descs = [(0, len(d))]
        self.relocs, self.relsites = [], {}
        self.tail = b''
        if offrel is not None:
            r = open(offrel, 'rb').read()
            if len(r) % 4:
                raise SystemExit(f'{offrel}: not a list of 32-bit offsets ({len(r)} bytes)')
            self.offsites = set(struct.unpack_from('<I', r, i)[0] for i in range(0, len(r), 4))
            bad = [a for a in self.offsites if a + 4 > len(d)]
            if bad:
                raise SystemExit(f'{offrel}: offset {bad[0]:X} beyond the image')

    def entry(self, byframe):
        return None if self.ip is None else super().entry(byframe)


# ---------------------------------------------------------------- hints

class Hints:
    def __init__(self, path):
        self.path = path
        self.exe = None
        self.kind = 'mz'
        self.segs = []
        self.code = []            # (seg, off, name)
        self.coderanges = []      # (seg, start, end)
        self.words = []           # (seg, off, count, target)
        self.rwords = []          # (seg, off, count, from): offsets from SEG:from
        self.names = {}           # (seg, off) -> name
        self.ptr = {}             # (seg, off) -> target seg
        self.dptr = set()         # ptr hints to data
        self.vars = {}            # (seg, off) -> what a word variable holds
        self.num = set()
        self.ds = []              # (seg, start, end, dsseg)
        self.es = []              # (seg, start, end, esseg)
        self.comments = {}        # (seg, off) -> [text]
        self.raw = set()
        self.stop = set()
        self.relocorder = []
        self.keeptail = False
        self.linker = None        # None: Microsoft LINK's header; ('tlink', VER)
        self.noentry = False
        self.offrel, self.offflags = None, 0
        self.asm = {}             # assembler switches (see the docstring)
        carried = False
        named = {}                  # name -> (seg, off): a name given twice
                                    # (at another address) is refused, so a
                                    # misnamed address cannot pass unnoticed

        def take_name(name, s, o):
            if name in named and named[name] != (s, o):
                was = named[name]
                raise ValueError(f'name {name} given twice ({was[0]}:{was[1]:X} and {s}:{o:X})')
            named[name] = (s, o)

        for n, line in enumerate(open(path, encoding='utf-8'), 1):
            if line.startswith('; ==== carried over'):
                carried = True      # xfer.py's block: its names do not replace the file's own
                # (the address's, or the name's elsewhere)
            line = line.split(';', 1)[0].strip() if not line.lstrip().startswith('comment') else line.strip()
            if not line:
                continue
            f = line.split()
            k = f[0]
            try:
                if k in ('exe', 'pmax', 'bin', 'le'):
                    self.exe, self.kind = f[1], 'mz' if k == 'exe' else k
                    self.noentry = 'noentry' in f[2:]
                elif k == 'offrel':
                    self.offrel = f[1]
                    self.offflags = next((int(x[6:], 16) for x in f[2:] if x.startswith('flags=')), 0)
                elif k == 'segment':
                    opts = f[4:]
                    size = next((int(o[5:], 16) for o in opts if o.startswith('size=')), None)
                    prefix = next((o[7:] for o in opts if o.startswith('prefix=')), None)
                    start = next((int(o[6:], 16) for o in opts if o.startswith('start=')), 0)
                    if not 0 <= start < 0x10:
                        raise ValueError('start= must be below 10h (a larger one is another frame)')
                    self.segs.append(Seg(f[1], int(f[2], 16), f[3], 'stack' in opts, size, prefix, start))
                elif k == 'code':
                    s, o = self.addr(f[1])
                    self.code.append((s, o, f[2] if len(f) > 2 else None))
                    if len(f) > 2:
                        take_name(f[2], s, o)
                        self.names.setdefault((s, o), f[2])
                elif k == 'words':
                    s, o = self.addr(f[1])
                    stride = next((int(x[7:], 16) for x in f[4:] if x.startswith('stride=')), None)
                    if stride:
                        # one pointer every STRIDE bytes (records): tables of one
                        for i in range(int(f[2], 16)):
                            self.words.append((s, o + stride * i, 1, f[3]))
                    else:
                        self.words.append((s, o, int(f[2], 16), f[3]))
                elif k == 'rwords':
                    s, o = self.addr(f[1])
                    stride = next((int(x[7:], 16) for x in f[3:] if x.startswith('stride=')), None)
                    frm = next((int(x[5:], 16) for x in f[3:] if x.startswith('from=')), o)
                    if stride:
                        # one offset every STRIDE bytes (records): tables of one
                        for i in range(int(f[2], 16)):
                            self.rwords.append((s, o + stride * i, 1, frm))
                    else:
                        self.rwords.append((s, o, int(f[2], 16), frm))
                elif k == 'name':
                    if not (carried and (self.addr(f[1]) in self.names or f[2] in named)):
                        s, o = self.addr(f[1])
                        take_name(f[2], s, o)
                        self.names[(s, o)] = f[2]
                elif k == 'ptr':
                    self.ptr[self.addr(f[1])] = f[2]
                elif k == 'dptr':
                    self.ptr[self.addr(f[1])] = f[2]
                    self.dptr.add(self.addr(f[1]))
                elif k == 'var':
                    self.vars[self.addr(f[1])] = f[2]
                elif k == 'num':
                    self.num.add(self.addr(f[1]))
                elif k == 'coderange':
                    sg, rng = f[1].split(':')
                    a_, b_ = rng.split('-')
                    self.coderanges.append((sg, int(a_, 16), int(b_, 16)))
                elif k in ('ds', 'es'):
                    s, rng = f[1].split(':')
                    a, b = rng.split('-')
                    getattr(self, k).append((s, int(a, 16), int(b, 16), f[2]))
                elif k == 'comment':
                    text = line.split(None, 2)[2] if len(f) > 2 else ''
                    self.comments.setdefault(self.addr(f[1]), []).append(text)
                elif k == 'keeptail':
                    self.keeptail = True
                elif k == 'relocorder':
                    self.relocorder = f[1:]
                elif k == 'linker':
                    if f[1] != 'tlink' or len(f) not in (3, 4) or f[3:] not in ([], ['header=original']):
                        raise ValueError('linker tlink VER [header=original] is the only one')
                    self.linker = ('tlink', int(f[2], 16), f[3:] == ['header=original'])
                elif k == 'asm':
                    for o in f[1:]:
                        key, val = o.split('=', 1)
                        if key not in ('lea_smart', 'alu_ax_short', 'test_form', 'imm8_alu', 'xchg_ax_short'):
                            raise ValueError(f'unknown asm option {key}')
                        if key == 'imm8_alu':
                            self.asm[key] = set(val.lower().split(','))
                        else:
                            self.asm[key] = val if key == 'test_form' else val not in ('0', 'no', 'off')
                elif k == 'raw':
                    self.raw.add(self.addr(f[1]))
                elif k == 'stop':
                    self.stop.add(self.addr(f[1]))
                else:
                    raise ValueError(f'unknown hint {k}')
            except (ValueError, IndexError) as e:
                raise SystemExit(f'{path}:{n}: {e}')

    @staticmethod
    def addr(t):
        s, o = t.split(':')
        return s, int(o, 16)


# ---------------------------------------------------------------- analysis

JUMPS = {'jmp', 'je', 'jne', 'jb', 'jae', 'jbe', 'ja', 'jl', 'jge', 'jle', 'jg', 'js', 'jns',
         'jo', 'jno', 'jp', 'jnp', 'jcxz', 'loop', 'loope', 'loopne'}
STOP = {'jmp', 'ret', 'retf', 'iret', 'iretd', 'ljmp'}
JUMPS.add('jecxz')

SEGREGS = {x86.X86_REG_ES: 'ES', x86.X86_REG_CS: 'CS', x86.X86_REG_SS: 'SS', x86.X86_REG_DS: 'DS'}


def regkey(ci, reg):
    """a register's name for the tracking of segment values: EAX as AX"""
    n = ci.reg_name(reg).upper()
    return n[1:] if len(n) == 3 and n[0] == 'E' and n[1] in 'ABCDSI' else n


class Insn:
    __slots__ = ('seg', 'off', 'size', 'ci', 'ds', 'es', 'refs', 'text', 'raw')

    def __init__(self, seg, off, ci, ds, es):
        self.seg, self.off, self.size, self.ci = seg, off, ci.size, ci
        self.ds, self.es = ds, es
        self.refs = {}            # 'disp'/'imm'/'target' -> (segname, offset) or 'SEG:name'
        self.text = None
        self.raw = False


class Analysis:
    def __init__(self, prog, hints):
        self.p, self.h = prog, hints
        self.segs = hints.segs
        for s in self.segs if prog.descs is not None else ():
            s.base, s.size = prog.descs[s.frame]
        for i, s in enumerate(self.segs):
            if s.size is None:
                n = self.segs[i + 1] if i + 1 < len(self.segs) else None
                nxt = n.base + n.start if n else len(prog.img)
                s.size = nxt - s.base
        self.byname = {s.name: s for s in self.segs}
        self.byframe = {s.frame: s for s in self.segs}
        self.bits = prog.bits
        self.w = prog.bits // 8                 # bytes of an offset
        self.mask = (1 << prog.bits) - 1
        self.md = capstone.Cs(capstone.CS_ARCH_X86,
                              capstone.CS_MODE_32 if prog.bits == 32 else capstone.CS_MODE_16)
        self.md.detail = True
        # what DS holds where nothing says otherwise: DATA, or in a
        # program without one (a flat image) CODE
        self.dflt_ds = 'DATA' if 'DATA' in self.byname else 'CODE'
        self.entry = prog.entry(self.byframe)
        self.insns = {}           # (seg, off) -> Insn
        self.labels = {}          # (seg, off) -> name
        self.label_owner = {}     # name in upper case -> (seg, off)
        self.farptrs = {}         # image offset of offset word -> (seg, off)
        self.regdisp = []         # instructions with a register and a displacement
        self.warnings = []
        self.dsmap = [(s, a, b, d) for s, a, b, d in hints.ds]
        # an offrel list: exactly these dwords are offsets (None: found by
        # the analysis); offdata: those in data, image offset -> (target, flags)
        self.offs = prog.offsites
        self.offflags = hints.offflags
        self.offdata = {}
        self.esmap = [(s, a, b, d) for s, a, b, d in hints.es]

    # ---- helpers
    def seg_at(self, a):
        for s in self.segs:
            if s.base + s.start <= a < s.base + s.size:
                return s
        return None

    def byte(self, a):
        return self.p.img[a] if a < len(self.p.img) else 0

    def rword_targets(self, seg, off, cnt, frm):
        base = self.byname[seg].base + off
        return [frm + int.from_bytes(self.p.img[base + 2 * i:base + 2 * i + 2], 'little', signed=True)
                for i in range(cnt)]

    def is_code(self, seg):
        """a segment of class CODE: a ptr or words hint into it seeds code
        (in a program of many code segments, not only the one named CODE)"""
        S = self.byname.get(seg)
        return S is not None and S.cls == 'CODE'

    def label(self, seg, off, kind=None):
        key = (seg, off)
        if key not in self.labels:
            name = self.h.names.get(key)
            if name is None:
                s = self.byname[seg]
                code = kind == 'code'
                name = ('L' if code else s.prefix) + f'{off:04X}'
                if code and (s.cls != 'CODE' or s.own_prefix):
                    name = 'L' + s.prefix + f'{off:04X}'
                other = self.label_owner.get(name.upper())
                if other is not None and other != key:
                    raise SystemExit(f'label {name} for {seg}:{off:04X} and {other[0]}:{other[1]:04X}: '
                                     f'give the segments their own prefix= (hints: segment)')
            self.labels[key] = name
            self.label_owner[name.upper()] = key
        return self.labels[key]

    # ---- code
    def run(self):
        work = []
        dd = self.dflt_ds
        if self.entry is not None:
            es, eo = self.entry
            work.append((es, eo, dd, None))
            self.label(es, eo, 'code')
        for s, o, n in self.h.code:
            work.append((s, o, dd, None))
            self.label(s, o, 'code')
        for s, a_, b_ in self.h.coderanges:
            S = self.byname[s]
            o = a_
            start = True
            while o < b_:
                ci = next(self.md.disasm(bytes(self.p.img[S.base + o:S.base + o + 16]), o), None)
                if ci is None:
                    break
                if start:
                    work.append((s, o, dd, None))
                    self.label(s, o, 'code')
                start = ci.mnemonic in STOP
                o += ci.size
        for s, o, cnt, t in self.h.words:
            base = self.byname[s].base + o
            for i in range(cnt):
                v = int.from_bytes(self.p.img[base + self.w * i:base + self.w * (i + 1)], 'little')
                if self.is_code(t):
                    work.append((t, v, dd, None))
                self.label(t, v, 'code' if self.is_code(t) else None)
        for s, o, cnt, frm in self.h.rwords:
            self.label(s, frm)
            for t in self.rword_targets(s, o, cnt, frm):
                work.append((s, t, dd, None))
                self.label(s, t, 'code')
        while work:
            seg, off, ds, es = work.pop()
            self.trace(seg, off, ds, es, work)
        # pointer variables can lead to more code; repeat until nothing new
        # (with an offrel list every offset is known already)
        self.ptrvars = {}
        for _ in range(8 if self.offs is None else 0):
            for seed in self.pointer_vars():
                work.append(seed)
            if not work:
                break
            while work:
                seg, off, ds, es = work.pop()
                self.trace(seg, off, ds, es, work)
        self.field_offsets()
        self.far_pointers()
        self.offset_data()

    def offset_target(self, v):
        """an offrel dword's value -> (offset, flags), or None beyond the image"""
        t = v & ~self.offflags & 0xFFFFFFFF
        return (t, v - t) if t <= len(self.p.img) else None

    def offset_data(self):
        """the offrel dwords outside instructions: DD label (+ flags)"""
        if self.offs is None:
            return
        covered = {}
        for (s, o), ins in self.insns.items():
            b = self.byname[s].base + o
            for k in range(ins.size):
                covered[b + k] = ins
        for a in sorted(self.offs):
            if any(a + k in covered for k in range(4)):
                continue            # an instruction's (collect), or a warning there
            v = int.from_bytes(self.p.img[a:a + 4], 'little')
            t = self.offset_target(v)
            S = self.seg_at(a)
            if t is None or S is None:
                self.warnings.append(f'offrel {a:X}: {v:X} is no offset in the image')
                continue
            self.offdata[a] = (S.name, t[0], t[1])
            self.label(S.name, t[0], 'code' if (S.name, t[0]) in self.insns else None)

    def field_offsets(self):
        """A displacement with a register that lands in code (at an
        instruction or inside one) is a field offset, not an address: a
        flat program's records have fields beyond 100h, and its one
        segment holds code and data.  The others become labels."""
        covered = set()
        for (s, o), ins in self.insns.items():
            covered.update((s, o + k) for k in range(ins.size))
        for ins in self.regdisp:
            ref = ins.refs['disp']
            if ref in covered:
                del ins.refs['disp']
            else:
                self.label(*ref)

    def pointer_vars(self):
        """Word variables that hold offsets: a variable loaded into a
        register that then addresses memory (or is called) holds offsets
        of that segment, so every constant stored into it is written as an
        offset.  Returns new code seeds: the constants of a variable that
        is only called or jumped through (one that addresses memory can
        point at data in the code segment, all data in a flat program)."""
        order = sorted(self.insns)
        votes = {}
        called, addressed = set(), set()
        for i, k in enumerate(order):
            ins = self.insns[k]
            ci = ins.ci
            ref = ins.refs.get('disp')
            if not ref or not ci.operands:
                continue
            ops = ci.operands
            if ci.mnemonic in ('call', 'jmp') and ops[0].type == x86.X86_OP_MEM \
                    and ops[0].size == self.w and not ops[0].mem.base and not ops[0].mem.index:
                votes.setdefault(ref, set()).add(k[0])
                called.add(ref)
                continue
            if not (ci.mnemonic == 'mov' and len(ops) == 2 and ops[0].type == x86.X86_OP_REG
                    and ops[0].size == self.w and ops[1].type == x86.X86_OP_MEM
                    and not ops[1].mem.base and not ops[1].mem.index):
                continue
            r = ops[0].reg
            for k2 in order[i + 1:i + 16]:
                if k2[0] != k[0]:
                    break
                ins2 = self.insns[k2]
                c2 = ins2.ci
                if c2.mnemonic in ('call', 'jmp') and c2.operands and \
                        c2.operands[0].type == x86.X86_OP_REG and c2.operands[0].reg == r:
                    votes.setdefault(ref, set()).add(k2[0])
                    called.add(ref)
                    break
                used = None
                for op in c2.operands:
                    if op.type == x86.X86_OP_MEM and r in (op.mem.base, op.mem.index):
                        used = self.mem_seg(ins2, op)
                        if used is None:
                            used = '?'
                if used:
                    votes.setdefault(ref, set()).add(used)
                    addressed.add(ref)
                    break
                try:
                    written = c2.regs_access()[1]
                except Exception:
                    written = ()
                if r in written or c2.mnemonic in STOP:
                    break
        types = {}
        for var, segs in votes.items():
            if var in self.h.vars:
                continue
            if len(segs) == 1 and '?' not in segs:
                types[var] = next(iter(segs))
        for var, t in self.h.vars.items():
            if t != 'num':
                types[var] = t
        self.ptrvars = types
        seeds = []
        for k in order:
            ins = self.insns[k]
            ci = ins.ci
            ref = ins.refs.get('disp')
            if ref not in types or 'imm' in ins.refs or ci.mnemonic != 'mov' or ci.imm_size != self.w:
                continue
            if ci.operands[0].type != x86.X86_OP_MEM:
                continue
            t = types[ref]
            v = int.from_bytes(ci.bytes[ci.imm_offset:ci.imm_offset + self.w], 'little')
            T = self.byname[t]
            if v == 0 or v > T.size:
                continue
            ins.refs['imm'] = (t, v)
            # a var hint to CODE says code; found by the analysis, a
            # variable that is called and never addresses memory
            code = ref in self.h.vars or ref in called and ref not in addressed
            if T.cls == 'CODE' and code:
                self.label(t, v, 'code')
                if (t, v) not in self.insns:
                    seeds.append((t, v, self.dflt_ds, None))
            else:
                self.label(t, v)
        return seeds

    def ds_override(self, seg, off, which='dsmap'):
        for s, a, b, d in getattr(self, which):
            if s == seg and a <= off < b:
                return d
        return None

    def trace(self, seg, off, ds, es, work):
        S = self.byname[seg]
        stack = []                 # shadow stack of pushed segment values
        regs = {}                  # 'AX' etc. -> segment name, when known
        while True:
            key = (seg, off)
            if key in self.insns:
                return
            if off >= S.size:
                self.warnings.append(f'{seg}:{off:04X}: code runs off the segment')
                return
            a = S.base + off
            ci = next(self.md.disasm(bytes(self.p.img[a:a + 16]), off), None)
            if ci is None:
                self.warnings.append(f'{seg}:{off:04X}: not an instruction')
                return
            # a segment value the loader relocates can only be an immediate
            rel = [k for k in range(ci.size) if a + k in self.p.relsites]
            if rel and not (ci.imm_size == 2 and rel == [ci.imm_offset]) \
                    and not (ci.mnemonic in ('lcall', 'ljmp') and rel == [ci.size - 2]):
                self.warnings.append(f'{seg}:{off:04X}: decoding ran into a relocated word')
                return
            if self.offs is not None:
                fields = {ci.imm_offset if ci.imm_size == 4 else None,
                          ci.disp_offset if ci.disp_size == 4 else None}
                hit = [k for k in range(-3, ci.size) if a + k in self.offs]
                if any(k not in fields for k in hit):
                    self.warnings.append(f'{seg}:{off:04X}: decoding ran into an offrel dword')
                    return
            dsh = self.ds_override(seg, off)
            esh = self.ds_override(seg, off, 'esmap')
            ins = Insn(seg, off, ci, dsh or ds, esh or es)
            self.insns[key] = ins
            self.collect(ins, work)
            m = ci.mnemonic
            # track segment registers
            ops = ci.operands
            if m == 'mov' and len(ops) == 2:
                d, s_ = ops
                if d.type == x86.X86_OP_REG:
                    dn = regkey(ci, d.reg)
                    val = None
                    if s_.type == x86.X86_OP_IMM:
                        r = self.p.relsites.get(a + ci.imm_offset) if ci.imm_offset else None
                        val = self.byframe[r].name if r is not None and r in self.byframe else None
                    elif s_.type == x86.X86_OP_REG:
                        sn = regkey(ci, s_.reg)
                        val = {'DS': ds, 'ES': es}.get(sn, regs.get(sn))
                    if dn == 'DS':
                        ds = val
                    elif dn == 'ES':
                        es = val
                    else:
                        regs[dn] = val
            elif m == 'push' and ops and ops[0].type == x86.X86_OP_REG:
                rn = regkey(ci, ops[0].reg)
                stack.append({'DS': ds, 'ES': es, 'CS': seg}.get(rn, regs.get(rn)))
            elif m == 'push':
                stack.append(None)         # an immediate or memory: keeps the stack in step
            elif m == 'pop' and ops and ops[0].type == x86.X86_OP_REG:
                rn = regkey(ci, ops[0].reg)
                v = stack.pop() if stack else None
                if rn == 'DS':
                    ds = v
                elif rn == 'ES':
                    es = v
                else:
                    regs[rn] = v
            elif m in ('pushaw', 'popaw', 'pushal', 'popal'):
                regs = {}
            elif m in ('les', 'lds'):
                if m == 'lds':
                    ds = None
                else:
                    es = None
            if m in STOP or key in self.h.stop:
                return
            if m == 'int' and ops[0].imm == 0x20:
                return
            if m == 'int' and ops[0].imm == 0x21 and self.ah_before(seg, off) == 0x4C:
                return
            off += ci.size

    def ah_before(self, seg, off):
        """AH set by the instruction right before (MOV AX,4Cxx / MOV AH,4Ch)"""
        S = self.byname[seg]
        a = S.base + off
        b = self.p.img
        if self.w == 4 and b[a - 5] == 0xB8:
            return b[a - 3]
        if self.w == 2 and b[a - 3] == 0xB8:
            return b[a - 1]
        if b[a - 2] == 0xB4:
            return b[a - 1]
        return None

    def mem_seg(self, ins, op):
        """the segment name a memory operand's address belongs to, or None"""
        ci = ins.ci
        sr = op.mem.segment
        if sr == x86.X86_REG_INVALID or sr == 0:
            sr = x86.X86_REG_SS if op.mem.base == x86.X86_REG_BP else x86.X86_REG_DS
        r = SEGREGS.get(sr)
        if r == 'DS':
            return ins.ds
        if r == 'ES':
            return ins.es
        if r == 'CS':
            return ins.seg
        return None

    def collect(self, ins, work):
        ci = ins.ci
        S = self.byname[ins.seg]
        a = S.base + ins.off
        m = ci.mnemonic
        key = (ins.seg, ins.off)
        if m in JUMPS or m == 'call':
            op = ci.operands[0]
            if op.type == x86.X86_OP_IMM:
                t = op.imm & self.mask
                ins.refs['target'] = (ins.seg, t)
                self.label(ins.seg, t, 'code')
                work.append((ins.seg, t, ins.ds if m != 'call' else self.dflt_ds, ins.es if m != 'call' else None))
                return
        if m in ('lcall', 'ljmp') and ci.operands and ci.operands[0].type == x86.X86_OP_IMM:
            o = int.from_bytes(self.p.img[a + 1:a + 1 + self.w], 'little')
            sa = a + 1 + self.w
            s = self.p.relsites.get(sa, struct.unpack_from('<H', self.p.img, sa)[0])
            T = self.byframe.get(s)
            if T:
                ins.refs['far'] = (T.name, o)
                self.label(T.name, o, 'code')
                work.append((T.name, o, self.dflt_ds, None))
            return
        if self.offs is not None:
            self.collect_offrel(ins, a, work)
            return
        for op in ci.operands:
            if op.type == x86.X86_OP_MEM and ci.disp_size == self.w and key not in self.h.num:
                has_reg = op.mem.base != 0 or op.mem.index != 0
                sn = self.mem_seg(ins, op)
                if key in self.h.ptr and not (ci.imm_offset and ci.imm_size == self.w):
                    sn = self.h.ptr[key]        # LEA of an address used with another DS
                if sn is None or sn not in self.byname:
                    continue
                d = op.mem.disp & self.mask
                T = self.byname[sn]
                # a displacement written in full though it fits a byte is
                # one the assembler did not know: an address (a compiler's
                # array[BX])
                wide = has_reg and (d < 0x80 or d > self.mask - 0x80)
                # a small displacement with a register is most often a
                # field offset, not an address; a ptr hint says otherwise
                if has_reg and (d < 0x100 and key not in self.h.ptr and not wide or d >= T.size):
                    continue
                if not has_reg and d > T.size:
                    continue
                ins.refs['disp'] = (sn, d)
                if has_reg and key not in self.h.ptr and not wide:
                    self.regdisp.append(ins)    # labelled once all code is known
                else:
                    self.label(sn, d)
        ia = a + ci.imm_offset
        if ci.imm_offset and ci.imm_size == 2 and ia in self.p.relsites:
            v = self.p.relsites[ia]
            if v in self.byframe:
                ins.refs['imm'] = 'SEG:' + self.byframe[v].name
            else:
                self.warnings.append(f'{ins.seg}:{ins.off:04X}: segment value {v:04X} is no segment')
        elif ci.imm_offset and ci.imm_size == self.w and key in self.h.ptr:
            t = self.h.ptr[key]
            v = int.from_bytes(self.p.img[ia:ia + self.w], 'little')
            ins.refs['imm'] = (t, v)
            code = self.is_code(t) and key not in self.h.dptr
            self.label(t, v, 'code' if code else None)
            if code:
                work.append((t, v, self.dflt_ds, None))

    def collect_offrel(self, ins, a, work):
        """an instruction's offsets when an offrel list names them all: its
        displacement or immediate where a listed dword is, nothing else"""
        ci = ins.ci
        S = self.byname[ins.seg]
        for what, fo, fs in (('disp', ci.disp_offset, ci.disp_size), ('imm', ci.imm_offset, ci.imm_size)):
            if fs != 4 or a + fo not in self.offs:
                continue
            v = int.from_bytes(self.p.img[a + fo:a + fo + 4], 'little')
            t = self.offset_target(v)
            if t is None or t[1]:
                self.warnings.append(f'{ins.seg}:{ins.off:04X}: offrel {v:X} is no offset in the image')
                continue
            ins.refs[what] = (S.name, t[0])
            key = (ins.seg, ins.off)
            # an immediate a ptr hint calls code is an entry point
            code = what == 'imm' and self.h.ptr.get(key) == 'CODE' and key not in self.h.dptr
            self.label(S.name, t[0], 'code' if code else None)
            if code:
                work.append((S.name, t[0], self.dflt_ds, None))

    def far_pointers(self):
        """relocated segment words with an offset before them: DD label
        (DF in a 32-bit program)"""
        w = self.w
        for a, v in self.p.relsites.items():
            T = self.byframe.get(v)
            if T is None:
                continue
            S = self.seg_at(a)
            if (S.name, a - S.base) in self.insns or any(
                    (S.name, a - S.base - k) in self.insns for k in range(1, 6 if w == 2 else 12)):
                continue
            o = int.from_bytes(self.p.img[a - w:a], 'little')
            if o <= T.size:
                self.farptrs[a - w] = (T.name, o)
                self.label(T.name, o, 'code' if T.cls == 'CODE' and (T.name, o) in self.insns else None)


# ---------------------------------------------------------------- formatting

SIZEPTR = {1: 'BYTE PTR', 2: 'WORD PTR', 4: 'DWORD PTR', 6: 'FWORD PTR', 8: 'QWORD PTR',
           10: 'TBYTE PTR'}
STRINGOPS = {'movsb', 'movsw', 'lodsb', 'lodsw', 'stosb', 'stosw', 'scasb', 'scasw',
             'cmpsb', 'cmpsw', 'insb', 'insw', 'outsb', 'outsw'}
STRINGOPS32 = STRINGOPS | {'movsd', 'lodsd', 'stosd', 'scasd', 'cmpsd', 'insd', 'outsd'}
RENAME = {'pushaw': 'PUSHA', 'popaw': 'POPA', 'xlatb': 'XLAT', 'lcall': 'CALL', 'ljmp': 'JMP',
          'pushfw': 'PUSHF', 'popfw': 'POPF', 'iretw': 'IRET', 'int1': 'INT 1',
          'cwde': 'CBW', 'cdq': 'CWD', 'cbw': 'CBW', 'cwd': 'CWD'}
# in a 32-bit program capstone names what differs by the operand size
RENAME32 = {'pushal': 'PUSHAD', 'popal': 'POPAD', 'xlatb': 'XLAT', 'lcall': 'CALL', 'ljmp': 'JMP',
            'int1': 'INT 1', 'iret': 'IRETW'}
PREFIXES = {0x26: 'ES', 0x2E: 'CS', 0x36: 'SS', 0x3E: 'DS', 0x64: 'FS', 0x65: 'GS',
            0xF2: 'REPNE', 0xF3: 'REP', 0xF0: 'LOCK'}
SEGPFX = (0x26, 0x2E, 0x36, 0x3E, 0x64, 0x65)
SIZEPFX = (0x66, 0x67)          # follow from the operands' registers


class Unformattable(Exception):
    pass


class Formatter:
    def __init__(self, an):
        self.an = an
        self.assumed_ds = an.dflt_ds  # what the source's last ASSUME gives DS
        self.w = an.w

    def sym(self, ref):
        if isinstance(ref, str):
            return ref[4:]
        s, o = ref
        return self.an.labels[(s, o)]

    def prefixes(self, ins):
        b = self.an.p.img[self.an.byname[ins.seg].base + ins.off:]
        out = []
        for x in b[:ins.size]:
            if x in PREFIXES or x in SIZEPFX:
                out.append(x)
            else:
                break
        return out

    def memtext(self, ins, op, need_size=True, force_seg=None):
        ci = ins.ci
        m = op.mem
        parts = []
        if m.base:
            parts.append(ci.reg_name(m.base).upper())
        if m.index:
            parts.append(ci.reg_name(m.index).upper() + (f'*{m.scale}' if m.scale > 1 else ''))
        seg = ci.reg_name(m.segment).upper() if m.segment else None
        pf = self.prefixes(ins)
        segpf = [PREFIXES[x] for x in pf if x in SEGPFX]
        if len(segpf) > 1:
            raise Unformattable('two segment prefixes')
        seg = segpf[0] if segpf else None
        disp = m.disp & (0xFFFF if ci.disp_size < 4 else 0xFFFFFFFF)
        ref = ins.refs.get('disp')
        if ref:
            parts.append(self.sym(ref))
            dtxt = None
        else:
            dtxt = disp
        if dtxt is not None and (dtxt or not parts) and not (ci.disp_size == 0):
            if parts and (ci.disp_size == 1 or ci.disp_size == 4 and m.disp < 0):
                v = m.disp
                parts_s = '+'.join(parts) + (f'+{hexnum(v)}' if v >= 0 else f'-{hexnum(-v)}')
            elif parts:
                parts_s = '+'.join(parts) + f'+{hexnum(dtxt)}'
            else:
                parts_s = hexnum(dtxt)
                if seg is None:
                    seg = 'DS' if m.base != x86.X86_REG_BP else 'SS'
        else:
            parts_s = '+'.join(parts)
        # a symbol alone: a segment override must be written if the prefix is there
        # and differs from what the ASSUMEs give; an extra prefix the assembler would
        # not produce is caught by the byte comparison
        if ref and not isinstance(ref, str) and seg is None:
            want = self.an.mem_seg(ins, op)
            if ref[0] != self.assumed_ds and want == ref[0] and ins.seg != ref[0]:
                raise Unformattable('label outside DS without override')
        if self.w == 4 and 0x67 in pf and not m.base and not m.index:
            parts_s = 'SMALL ' + parts_s    # a 16-bit address, no register (tasm.py's SMALL)
        t = f'[{parts_s}]'
        if seg:
            t = f'{seg}:{t}'
        if need_size:
            t = f'{SIZEPTR[op.size]} {t}'
        return t

    def imm(self, ins, op, size):
        ref = ins.refs.get('imm')
        if ref:
            if isinstance(ref, str):
                return ref[4:]
            return 'OFFSET ' + self.sym(ref)
        v = op.imm & ((1 << (8 * size)) - 1)
        return hexnum(v)

    def fmt(self, ins):
        ci = ins.ci
        m = ci.mnemonic
        pf = self.prefixes(ins)
        rep = [PREFIXES[x] for x in pf if x in (0xF2, 0xF3, 0xF0)]
        segpf = [PREFIXES[x] for x in pf if x in SEGPFX]
        size = [x for x in pf if x in SIZEPFX]
        if len(pf) != len(rep) + len(segpf) + len(size) or len(size) != len(set(size)):
            raise Unformattable('prefix')
        base = m.split()[-1]
        ops = ci.operands
        # string instructions
        if base in (STRINGOPS32 if self.w == 4 else STRINGOPS):
            r = ''
            if rep:
                if len(rep) > 1:
                    raise Unformattable('rep')
                r = {'REP': 'REPE' if base[:4] in ('scas', 'cmps') else 'REP', 'REPNE': 'REPNE',
                     'LOCK': 'LOCK'}[rep[0]] + ' '
            if not segpf:
                return r + base.upper()
            s = segpf[0]
            w = {'b': 'BYTE PTR', 'w': 'WORD PTR', 'd': 'DWORD PTR'}[base[-1]]
            # the index registers as the address size has them
            e = 'E' if self.w == 4 and 0x67 not in pf or self.w == 2 and 0x67 in pf else ''
            si, di = e + 'SI', e + 'DI'
            if base.startswith('lods'):
                return f'{r}LODS {w} {s}:[{si}]'
            if base.startswith('movs'):
                return f'{r}MOVS {w} ES:[{di}],{w} {s}:[{si}]'
            if base.startswith('cmps'):
                return f'{r}CMPS {w} {s}:[{si}],{w} ES:[{di}]'
            if base.startswith('outs'):
                return f'{r}OUTS DX,{w} {s}:[{si}]'
            raise Unformattable('string op override')
        if rep:
            raise Unformattable('rep on non-string')
        if segpf and not any(o.type == x86.X86_OP_MEM for o in ops):
            raise Unformattable('segment prefix without memory operand')
        mn = (RENAME32 if self.w == 4 else RENAME).get(m, m.upper())
        # an indirect far CALL/JMP (FF /3, /5), which capstone does not
        # always name lcall/ljmp
        far = ci.opcode[0] == 0xFF and (ci.modrm >> 3) & 7 in (3, 5)
        if m in JUMPS or m == 'call':
            op = ops[0]
            if op.type == x86.X86_OP_IMM:
                return f'{mn} {self.sym(ins.refs["target"])}'
        if m in ('lcall', 'ljmp') or far:
            mn = 'CALL' if m in ('lcall', 'call') else 'JMP'
            if ops[0].type == x86.X86_OP_IMM:
                if 'far' not in ins.refs:
                    raise Unformattable('far to unknown segment')
                return f'{mn} FAR PTR {self.sym(ins.refs["far"])}'
            op = ops[-1]
            size = 'FWORD PTR' if (self.w == 4) != (0x66 in pf) else 'DWORD PTR'
            return f'{mn} {size} {self.memtext(ins, op, need_size=False)}'
        if m == 'int' and ops[0].imm == 3 and ci.bytes[0] == 0xCC:
            return 'INT 3'
        out = []
        regsize = None
        for op in ops:
            if op.type == x86.X86_OP_REG:
                regsize = op.size
        for op in ops:
            if op.type == x86.X86_OP_REG:
                out.append(ci.reg_name(op.reg).upper())
            elif op.type == x86.X86_OP_IMM:
                size = op.size or regsize or 2
                if m in ('int', 'ret', 'retf', 'enter', 'in', 'out'):
                    size = op.size or 1
                    if m in ('ret', 'retf'):
                        size = 2
                out.append(self.imm(ins, op, size))
            elif op.type == x86.X86_OP_MEM:
                need = m not in ('lea', 'les', 'lds') and (regsize is None or m in ('movzx', 'movsx'))
                if m in ('les', 'lds', 'lea', 'lfs', 'lgs', 'lss'):
                    need = False
                # a segment register stored with 66h: WORD PTR makes the
                # assembler write the prefix
                if m == 'mov' and 0x66 in pf and ops[-1].type == x86.X86_OP_REG \
                        and ci.reg_name(ops[-1].reg).upper() in ('ES', 'CS', 'SS', 'DS', 'FS', 'GS'):
                    need = True
                out.append(self.memtext(ins, op, need_size=need))
        # capstone writes the implicit shift count 1 and 'in al, dx' fine; a few need care
        if m in ('shl', 'shr', 'sar', 'sal', 'rol', 'ror', 'rcl', 'rcr') and ci.opcode[0] in (0xD0, 0xD1):
            out = out[:1] + ['1']
        # XCHG reg,reg: the assembler puts the first operand in the reg field
        if m == 'xchg' and len(ops) == 2 and all(o.type == x86.X86_OP_REG for o in ops) \
                and len(ci.bytes) >= 2 and ci.bytes[-2] in (0x86, 0x87):
            modrm = ci.bytes[-1]
            reg = (modrm >> 3) & 7
            names8 = ['AL', 'CL', 'DL', 'BL', 'AH', 'CH', 'DH', 'BH']
            names16 = ['AX', 'CX', 'DX', 'BX', 'SP', 'BP', 'SI', 'DI']
            names = names8 if ci.bytes[-2] == 0x86 else names16
            if ops[0].size == 4:
                names = ['E' + r for r in names16]
            out = [names[reg], names[modrm & 7]]
        return mn + (' ' + ','.join(out) if out else '')


# ---------------------------------------------------------------- emitting

def is_text(b):
    return 32 <= b < 127 and b not in (0x27,)


def db_lines(data):
    """DB lines for a run of bytes: printable runs as strings"""
    lines = []
    i = 0
    while i < len(data):
        j = i
        while j < len(data) and is_text(data[j]):
            j += 1
        if j - i >= 4:
            lines.append("\tDB '" + data[i:j].decode('ascii') + "'")
            i = j
            continue
        k = i
        chunk = []
        while k < len(data) and len(chunk) < 16:
            j = k
            while j < len(data) and is_text(data[j]):
                j += 1
            if j - k >= 4:
                break
            chunk.append(data[k])
            k += 1
        # runs of one value
        if len(chunk) == 16 and len(set(chunk)) == 1:
            n = 16
            while i + n < len(data) and data[i + n] == chunk[0]:
                n += 1
            lines.append(f'\tDB {hexnum(n)} DUP ({hexnum(chunk[0])})')
            i += n
            continue
        lines.append('\tDB ' + ','.join(hexnum(x) for x in chunk))
        i = k
    return lines


class Emitter:
    def __init__(self, an):
        self.an = an
        self.f = Formatter(an)
        self.lines = []
        self.map = []             # per output line: (seg, off, length) or None

    def out(self, text, where=None):
        self.lines.append(text)
        self.map.append(where)

    def emit(self):
        an = self.an
        self.out('; generated by tools/disasm.py from ' + an.h.exe + ' - do not edit, edit the hints')
        self.out('.386' if an.w == 4 else '.186')
        use = ' USE32' if an.w == 4 else ''
        order = an.segs
        for S in order:
            al = 'BYTE' if S.start else 'PARA'
            if S.cls == 'CODE':
                self.out(f'{S.name} SEGMENT {al} PUBLIC{use} \'{S.cls}\'')
                self.out(f'\tASSUME CS:{S.name},DS:{an.dflt_ds},ES:NOTHING,SS:NOTHING')
            elif S.stack:
                self.out(f'{S.name} SEGMENT PARA STACK{use} \'{S.cls}\'')
            else:
                self.out(f'{S.name} SEGMENT {al} PUBLIC{use} \'{S.cls}\'')
            self.segment(S)
            self.out(f'{S.name} ENDS')
            self.out('')
        self.out(f'\tEND {an.labels[an.entry]}' if an.entry is not None else '\tEND')

    def label_lines(self, S, off):
        for c in self.an.h.comments.get((S.name, off), []):
            self.out(f'; {c}')
        n = self.an.labels.get((S.name, off))
        if n:
            if (S.name, off) in self.an.insns:
                self.out(f'{n}:')
            else:
                self.out(f'{n}\tLABEL BYTE')

    def segment(self, S):
        an = self.an
        img = an.p.img
        stored = max(0, min(S.size, len(img) - S.base))
        labels = sorted(o for (s, o) in an.labels if s == S.name)
        # a table of a words hint is written as DWs, with or without a label
        tables = set(o for s, o, cnt, t in an.h.words if s == S.name)
        tables |= set(o for s, o, cnt, frm in an.h.rwords if s == S.name)
        import bisect
        off = S.start
        w = an.w
        self.f.assumed_ds = an.dflt_ds      # as the ASSUME at the segment's start says
        while off < S.size:
            key = (S.name, off)
            ins = an.insns.get(key)
            if ins and off + ins.size <= S.size:
                self.label_lines(S, off)
                # where DS holds another segment of the program (tracked, or a
                # ds hint) the source says so; unknown DS makes no labels
                if ins.ds in an.byname and ins.ds != self.f.assumed_ds:
                    self.out(f'	ASSUME DS:{ins.ds}')
                    self.f.assumed_ds = ins.ds
                self.inner_labels(S, off, ins.size)
                text = None
                if not ins.raw and key not in an.h.raw:
                    try:
                        text = self.f.fmt(ins)
                    except Unformattable as e:
                        text = None
                        ins.raw = True
                if text is None and self.offset_fields(S, ins):
                    self.db_with_offsets(S, ins)
                    off += ins.size
                    continue
                if text is None:
                    b = img[S.base + off:S.base + off + ins.size]
                    text = 'DB ' + ','.join(hexnum(x) for x in b) + f'\t; {ins.ci.mnemonic} {ins.ci.op_str}'
                self.out('\t' + text, (S.name, off, ins.size))
                off += ins.size
                continue
            self.label_lines(S, off)
            # data up to the next label / instruction / far pointer / reloc
            end = off + 1
            while end < S.size and (S.name, end) not in an.insns and (S.name, end) not in an.labels \
                    and S.base + end not in an.farptrs and S.base + end not in an.p.relsites \
                    and S.base + end not in an.offdata \
                    and end not in tables and not (end == stored):
                end += 1
            a = S.base + off
            if a in an.farptrs:
                t = an.farptrs[a]
                self.inner_labels(S, off, w + 2)
                self.out(f'\t{"DF" if w == 4 else "DD"} {an.labels[t]}', (S.name, off, w + 2))
                off += w + 2
                continue
            if a in an.offdata:
                s, t, fl = an.offdata[a]
                self.inner_labels(S, off, 4)
                self.out(f'\tDD {an.labels[(s, t)]}' + (f'+{hexnum(fl)}' if fl else ''), (S.name, off, 4))
                off += 4
                continue
            if a in an.p.relsites:
                v = an.p.relsites[a]
                T = an.byframe.get(v)
                if T:
                    self.inner_labels(S, off, 2)
                    self.out(f'\tDW {T.name}', (S.name, off, 2))
                    off += 2
                    continue
            if off >= stored:
                self.out(f'\tDB {hexnum(end - off)} DUP (?)', (S.name, off, end - off))
                off = end
                continue
            data = bytes(img[a:S.base + end])
            rw = next(((cnt, frm) for s, o, cnt, frm in an.h.rwords if s == S.name and o == off), None)
            if rw:
                rw, frm = rw
                tab = an.labels[(S.name, frm)]
                for i, t in enumerate(an.rword_targets(S.name, off, rw, frm)):
                    if i:
                        self.label_lines(S, off + 2 * i)
                    self.inner_labels(S, off + 2 * i, 2)
                    self.out(f'\tDW {an.labels[(S.name, t)]}-{tab}', (S.name, off + 2 * i, 2))
                off += 2 * rw
                continue
            ws = self.words_at(S, off)
            if ws:
                t, n = ws
                for i in range(n):
                    v = int.from_bytes(img[a + w * i:a + w * (i + 1)], 'little')
                    if i:
                        self.label_lines(S, off + w * i)
                    self.inner_labels(S, off + w * i, w)
                    self.out(f'\t{"DD" if w == 4 else "DW"} {an.labels[(t, v)]}', (S.name, off + w * i, w))
                off += w * n
                continue
            p = off
            for ln in db_lines(data):
                n = self.db_len(ln)
                self.out(ln, (S.name, p, n))
                p += n
            off = end

    def offset_fields(self, S, ins):
        """an instruction's offrel dwords: (offset in it, label)"""
        an = self.an
        if an.offs is None:
            return []
        a = S.base + ins.off
        out = []
        for what, fo in (('disp', ins.ci.disp_offset), ('imm', ins.ci.imm_offset)):
            ref = ins.refs.get(what)
            if isinstance(ref, tuple) and a + fo in an.offs:
                out.append((fo, self.f.sym(ref)))
        return sorted(out)

    def db_with_offsets(self, S, ins):
        """an instruction written as DB whose offrel dwords stay DD label,
        so that the source keeps every offset of the list"""
        img = self.an.p.img
        a = S.base + ins.off
        p = 0
        note = f'\t; {ins.ci.mnemonic} {ins.ci.op_str}'
        for fo, name in self.offset_fields(S, ins):
            if fo > p:
                self.out('\tDB ' + ','.join(hexnum(x) for x in img[a + p:a + fo]) + note,
                         (S.name, ins.off + p, fo - p))
                note = ''
            self.out(f'\tDD {name}', (S.name, ins.off + fo, 4))
            p = fo + 4
        if p < ins.size:
            self.out('\tDB ' + ','.join(hexnum(x) for x in img[a + p:a + ins.size]), (S.name, ins.off + p, ins.size - p))

    def inner_labels(self, S, off, n):
        """labels inside an item that is written as one line"""
        for k in range(1, n):
            for c in self.an.h.comments.get((S.name, off + k), []):
                self.out(f'; {c}')
            name = self.an.labels.get((S.name, off + k))
            if name:
                self.out(f'{name} = $+{k}')

    def words_at(self, S, off):
        for s, o, cnt, t in self.an.h.words:
            if s == S.name and o == off:
                return t, cnt
        return None

    @staticmethod
    def db_len(ln):
        body = ln.split('DB', 1)[1].strip()
        if body.startswith("'"):
            return len(body) - 2
        m = re.match(r'(\w+) DUP', body)
        if m:
            v = m.group(1)
            return int(v[:-1], 16) if v.endswith('H') else int(v)
        return body.count(',') + 1


def load_program(h):
    """the program the hints describe, read from the player's files"""
    if h.kind == 'bin':
        return RawProgram(program_path(h.exe), h.noentry,
                          program_path(h.offrel) if h.offrel else None)
    if h.offrel or h.noentry:
        raise SystemExit('offrel and noentry are for a raw image (bin)')
    return {'pmax': PmaxProgram, 'le': LEProgram}.get(h.kind, Program)(program_path(h.exe))


def generate(hints_path, raw_extra=()):
    h = Hints(hints_path)
    for r in raw_extra:
        h.raw.add(r)
    prog = load_program(h)
    an = Analysis(prog, h)
    an.run()
    em = Emitter(an)
    em.emit()
    return an, em


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('hints')
    ap.add_argument('-o', '--out')
    a = ap.parse_args()
    an, em = generate(a.hints)
    out = a.out or build_dir(os.path.splitext(os.path.basename(a.hints))[0] + '.ASM')
    os.makedirs(os.path.dirname(out), exist_ok=True)
    open(out, 'w', newline='\r\n').write('\n'.join(em.lines) + '\n')
    for w in an.warnings:
        print('warning:', w)
    print(f'{len(an.insns)} instructions, {len(an.labels)} labels -> {out}')


if __name__ == '__main__':
    main()


def gaps(an, seg='CODE', show=3):
    """ranges of a code segment no instruction covers, with a look at them"""
    S = an.byname[seg]
    cov = bytearray(S.size)
    for (s, o), ins in an.insns.items():
        if s == seg:
            cov[o:o + ins.size] = b'\1' * ins.size
    out = []
    o = S.start
    while o < S.size:
        if cov[o]:
            o += 1
            continue
        e = o
        while e < S.size and not cov[e]:
            e += 1
        out.append((o, e))
        o = e
    return out
