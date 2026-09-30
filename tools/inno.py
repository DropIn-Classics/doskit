#!/usr/bin/env python3
"""The files in an Inno Setup installer, as GOG's Windows installers are
(setup_NAME_VERSION_(ID).exe, with setup_..-1.bin, -2.bin beside it when
the game is large): listed and unpacked without running it, on any
system.

    inno.py SETUP.exe                    the files it installs into {app}
    inno.py SETUP.exe -x DIR [NAME...]   unpacked into DIR (only NAMEs,
                                         paths as listed, if given)

Inno Setup 5.2.0 to 6.4 (GOG's installers are 5.2 to 6.x); older ones,
encrypted ones and those with bzip2 are refused.  Only what the setup
puts into {app} (the game's folder) is taken, its paths with / and
{app}\\ left off; each file's SHA-1 (MD5, SHA-256) is checked.

The format as described by innoextract (Daniel Scharrer, zlib licence),
read as documentation; nothing of its code is here.  The setup: the
.exe's offset table (magic rDlPtS...) names where the header and the
data begin; the header is two LZMA blocks (4096-byte chunks each behind
its CRC32), the first holding the setup's settings and its entries
(languages, messages, ..., directories, files, ...), the second the data
entries (where each file's bytes are: a chunk, "zlb\\x1a" then an
LZMA/LZMA2/zlib stream, with several files one after another in it).
"""
import argparse, bz2, hashlib, lzma, os, re, struct, sys, zlib

LOADER_MAGICS = (b'rDlPtS\xcd\xe6\xd7{\x0b*', b'nS5W7dT\x83\xaa\x1b\x0fj')
SLICE_MAGICS = (b'idska16\x1a', b'idska32\x1a')
STORED, ZLIB, BZIP2, LZMA1, LZMA2 = range(5)
SEARCHED = 16 << 20         # the offset table is in the .exe's resources, near its start


class InnoError(Exception):
    pass


class Reader:
    """little-endian fields from a bytes object"""

    def __init__(self, data, unicode):
        self.d, self.p, self.unicode = data, 0, unicode

    def take(self, n):
        if self.p + n > len(self.d):
            raise InnoError('the header ends early (not a format known here)')
        b = self.d[self.p:self.p + n]
        self.p += n
        return b

    def u8(self):
        return self.take(1)[0]

    def u16(self):
        return struct.unpack('<H', self.take(2))[0]

    def u32(self):
        return struct.unpack('<I', self.take(4))[0]

    def u64(self):
        return struct.unpack('<Q', self.take(8))[0]

    def raw(self):
        """a string as stored: its length, then its bytes"""
        return self.take(self.u32())

    def text(self):
        b = self.raw()
        return b.decode('utf-16-le' if self.unicode else 'cp1252', 'replace')

    def skip_strings(self, n):
        for _ in range(n):
            self.raw()


def flag_bytes(n):
    """the bytes a set of n flags takes: one per 8, 3 made 4 (Delphi's sets)"""
    b = (n + 7) // 8
    return 4 if b == 3 else b


def parse_version(text):
    """(a, b, c, d), unicode from 'Inno Setup Setup Data (5.6.2) (u)'"""
    m = re.match(rb'Inno Setup Setup Data \((\d+)\.(\d+)\.(\d+)(?:\.(\d+))?\)( \([uU]\))?', text)
    if not m:
        raise InnoError(f'not an Inno Setup header: {text[:40]!r}')
    v = tuple(int(x or 0) for x in m.group(1, 2, 3, 4))
    unicode = bool(m.group(5)) or v >= (6, 3, 0, 0)
    if v < (5, 2, 0, 0):
        raise InnoError(f'Inno Setup {".".join(map(str, v[:3]))}: older than 5.2, not read here')
    return v, unicode


# -- the file: the offset table, the header blocks, the data's slices

def find_offsets(d):
    """(header, data) offsets from the loader's table, found by its magic
    and its CRC32 (5.1.5 and later keep it in a resource of the .exe)"""
    for magic in LOADER_MAGICS:
        i = d.find(magic)
        while i >= 0:
            t = d[i:i + 12 + 4 * 8]
            if len(t) == 44 and zlib.crc32(t[:40]) == struct.unpack_from('<I', t, 40)[0]:
                _rev, _total, _exe, _exesize, _execrc, header, data = struct.unpack_from('<7I', t, 12)
                return header, data
            i = d.find(magic, i + 1)
    if d[:22] == b'Inno Setup Setup Data ':
        return 0, 0                     # a setup-0.bin: the header alone
    raise InnoError('no Inno Setup offset table (not an Inno Setup installer, or older than 5.1.5)')


def lzma_raw(props, data, lzma2=False):
    if lzma2:
        p = props[0]
        dict_size = 0xffffffff if p == 40 else (2 | (p & 1)) << (p // 2 + 11)
        filt = {'id': lzma.FILTER_LZMA2, 'dict_size': dict_size}
    else:
        lc, rest = props[0] % 9, props[0] // 9
        filt = {'id': lzma.FILTER_LZMA1, 'dict_size': struct.unpack_from('<I', props, 1)[0],
                'lc': lc, 'lp': rest % 5, 'pb': rest // 5}
    return lzma.LZMADecompressor(format=lzma.FORMAT_RAW, filters=[filt]), data


def read_block(f):
    """the header block at f's position: its bytes"""
    head = f.read(9)
    if len(head) < 9 or zlib.crc32(head[4:]) != struct.unpack_from('<I', head)[0]:
        raise InnoError('header block: CRC32 wrong')
    size, compressed = struct.unpack_from('<IB', head, 4)
    d, pos = f.read(size), 0
    end, out = size, bytearray()
    while pos < end:
        n = min(4096, end - pos - 4)
        chunk = d[pos + 4:pos + 4 + n]
        if zlib.crc32(chunk) != struct.unpack_from('<I', d, pos)[0]:
            raise InnoError('header block: a chunk\'s CRC32 wrong')
        out += chunk
        pos += 4 + n
    if compressed:
        dec, rest = lzma_raw(out[:5], bytes(out[5:]))
        out = dec.decompress(rest)
    return bytes(out)


class Slices:
    """the setup's data as one stream per slice: the .exe itself (from the
    data offset) or setup-1.bin, setup-2.bin ... (setup-1a.bin with
    several slices per disk)"""

    def __init__(self, path, data_offset, slices_per_disk):
        self.path, self.base, self.per_disk = path, data_offset, slices_per_disk
        self.files = {}

    def open(self, n):
        if n not in self.files:
            if self.base:
                if n:
                    raise InnoError('a slice beyond the .exe, in a setup without .bin files')
                f = open(self.path, 'rb')
                f.seek(0, 2)
                self.files[n] = (f, self.base, f.tell())
            else:
                self.files[n] = self.open_bin(n)
        return self.files[n]

    def open_bin(self, n):
        stem = re.sub(r'(?i)\.exe$', '', self.path)
        stem = re.sub(r'-0\.bin$', '', stem)
        suffix = f'{n + 1}' if self.per_disk == 1 else f'{n // self.per_disk + 1}{chr(97 + n % self.per_disk)}'
        want = os.path.basename(stem) + f'-{suffix}.bin'
        folder = os.path.dirname(self.path) or '.'
        for name in os.listdir(folder):
            if name.lower() == want.lower():
                f = open(os.path.join(folder, name), 'rb')
                head = f.read(12)
                if head[:8] not in SLICE_MAGICS:
                    raise InnoError(f'{name}: not one of the setup\'s .bin files')
                return f, 0, struct.unpack_from('<I', head, 8)[0]
        raise InnoError(f'{want} missing beside {os.path.basename(self.path)}')

    def pieces(self, slice_no, offset, n, piece=1 << 20):
        """n bytes from offset in slice_no on, a piece at a time, into the
        next slices as needed"""
        f, base, size = self.open(slice_no)
        pos = base + offset
        while n:
            if pos >= size:
                slice_no += 1
                f, base, size = self.open(slice_no)
                pos = base if self.base else 12
            f.seek(pos)
            got = f.read(min(n, size - pos, piece))
            if not got:
                raise InnoError('the setup\'s data ends early')
            n -= len(got)
            pos += len(got)
            yield got


# -- the header's entries, as far as the files

def skip_header(r, v, u):
    """the setup's settings; returns the entry counts (and its name, app ID)"""
    name = r.text()
    r.raw()                                 # versioned name
    app_id = r.text()
    r.skip_strings(9)                       # copyright .. default group name
    r.raw()                                 # base filename
    if v < (5, 2, 5):
        r.skip_strings(3)                   # licence, info before, after
    r.skip_strings(7)                       # uninstall files dir .. default serial
    if v < (5, 2, 5):
        r.raw()                             # compiled code
    r.skip_strings(4)                       # readme, contact, comments, modify path
    r.skip_strings((v >= (5, 3, 8)) + (v >= (5, 3, 10)) + (v >= (5, 5, 0)) + (v >= (5, 5, 6))
                   + 2 * (v >= (5, 6, 1)) + 2 * (v >= (6, 3, 0)))
    if v >= (5, 2, 5):
        r.skip_strings(3)
    if (5, 2, 1) <= v < (5, 3, 10):
        r.raw()                             # uninstaller signature
    if v >= (5, 2, 5):
        r.raw()                             # compiled code
    if not u:
        r.take(32)                          # lead bytes
    counts = [r.u32() for _ in range(16)]
    r.take(20)                              # Windows versions
    new64 = v >= (6, 4, 0, 1)
    r.take(4 * (not new64) * 2 + 4 * (v < (5, 5, 7)))   # colours
    if v >= (6, 0, 0):
        r.take(9)                           # wizard style, resize percentages
    if v >= (5, 5, 7):
        r.u8()                              # image alpha format
    r.take(4 if v >= (6, 4, 0) else 20 if v >= (5, 3, 9) else 16)   # password hash
    r.take(44 if v >= (6, 4, 0) else 8)     # salt
    r.take(12)                              # extra disk space, slices per disk
    slices_per_disk = struct.unpack_from('<I', r.d, r.p - 4)[0]
    r.take(3)                               # uninstall log mode, dir exists warning, privileges
    if v >= (5, 7, 0):
        r.u8()
    r.take(2)                               # language dialog, detection
    compression = r.u8()
    if v < (5, 3, 9) and compression > LZMA1:
        raise InnoError('unknown compression')
    if v < (6, 3, 0):
        r.take(2)                           # architectures
    if (5, 2, 1) <= v < (5, 3, 10):
        r.take(8)
    if v >= (5, 3, 3):
        r.take(2)
    r.take(8 if v >= (5, 5, 0) else 4 if v >= (5, 3, 6) else 0)   # uninstall display size
    r.take(flag_bytes(header_flag_count(v, u)))
    return name, app_id, counts, compression, max(slices_per_disk, 1)


def header_flag_count(v, u):
    new64 = v >= (6, 4, 0, 1)
    return (1 + (v < (5, 3, 10)) + 1 + 2 * (v < (5, 3, 3)) + 3 + 4 * (not new64) + 1 + 1
            + 2 + (v < (5, 6, 1)) + (v < (5, 3, 8)) + 1 + (not new64) + 1 + 1 + 1 + 6 + 2
            + 1 + 2 + 1 + 1 + 1 + 1 + 1 + 2 + 1 + ((5, 0, 4) <= v < (5, 6, 1)) + (not u)
            + 1 + 1 + (v >= (5, 3, 8)) + (v >= (5, 3, 9)) + 3 * (v >= (5, 5, 0))
            + (v >= (5, 5, 7)) + 3 * (v >= (6, 0, 0)) + (v >= (6, 3, 0)))


def skip_language(r, v, u):
    r.skip_strings(10)
    r.u32()                                 # language ID
    if not u or v < (5, 3, 0):
        r.u32()                             # codepage
    r.take(16)                              # font sizes
    if v >= (5, 2, 3):
        r.u8()


def skip_entries(r, v, u, counts):
    """languages .. directories, up to the files"""
    languages, messages, permissions, types, components, tasks, directories = counts[:7]
    for _ in range(languages):
        skip_language(r, v, u)
    for _ in range(messages):
        r.skip_strings(2)
        r.u32()
    for _ in range(permissions):
        r.raw()
    for _ in range(types):
        r.skip_strings(4)
        r.take(20 + 1 + 1 + 8)
    for _ in range(components):
        r.skip_strings(5)
        r.take(8 + 4 + 1 + 20 + 1 + 8)
    for _ in range(tasks):
        r.skip_strings(6)
        r.take(4 + 1 + 20 + 1)
    for _ in range(directories):
        r.skip_strings(7)
        r.take(4 + 20 + 2 + 1)




class Entry:
    """one [Files] entry: its destination, its data entry, and the Pascal
    calls GOG's Galaxy installers put in Check, BeforeInstall, AfterInstall"""

    def __init__(self, r, v):
        r.raw()                             # source
        self.dest = r.text()
        r.raw()                             # font name
        if v >= (5, 2, 5):
            r.raw()                         # assembly name
        r.skip_strings(3)                   # components, tasks, languages
        self.check, self.after, self.before = r.text(), r.text(), r.text()
        r.take(20)
        self.location = r.u32()
        r.take(4 + 8 + 2)                   # attributes, external size, permission
        r.take(flag_bytes(31 + (v >= (5, 2, 5))))
        r.u8()                              # file type


class Data:
    """a data entry: where a file's bytes are and how they are kept"""

    def __init__(self, r, v, compression):
        self.first_slice, _last, self.chunk_offset = r.u32(), r.u32(), r.u32()
        self.offset, self.size, self.chunk_size = r.u64(), r.u64(), r.u64()
        n = 32 if v >= (6, 4, 0) else 20 if v >= (5, 3, 9) else 16
        self.hash = {32: 'sha256', 20: 'sha1', 16: 'md5'}[n]
        self.digest = r.take(n)
        r.take(8 + 8)                       # time, version
        nflags = 9 + 2 * ((5, 5, 7) <= v < (6, 3, 0))
        flags = int.from_bytes(r.take(flag_bytes(nflags)), 'little')
        if v >= (6, 3, 0):
            r.u8()                          # sign mode
        self.exe_filter = bool(flags & 1 << 4)
        self.encrypted = bool(flags & 1 << 6)
        self.compression = compression if flags & 1 << 7 else STORED
        self.flip = v >= (5, 3, 9)

    def chunk_key(self):
        return self.first_slice, self.chunk_offset


class File:
    """a file the setup installs: its path in {app} and its data, in parts
    each deflated with the whole file's MD5 when a GOG Galaxy installer's"""

    def __init__(self, path, md5=None, languages=()):
        self.path, self.md5, self.languages = path, md5, set(languages)
        self.parts, self.size, self.left = [], 0, 0


# -- GOG's Galaxy installers (from 2018): the game's files under {tmp}
# named by their MD5, each in parts, the real name in a Pascal call

def call_args(code, name):
    """the arguments of `name('a', 12, 'b''c')`, None if code is not that call"""
    m = re.fullmatch(r'\s*' + name + r'\s*\((.*)\)\s*;?\s*', code or '', re.S)
    if not m:
        return None
    args, rest = [], m.group(1)
    while rest.strip():
        q = re.match(r"\s*'((?:[^']|'')*)'\s*(,|$)", rest) or re.match(r'\s*([^,]*?)\s*(,|$)', rest)
        args.append(q.group(1).replace("''", "'"))
        rest = rest[q.end():]
    return args


def galaxy_files(entries, data):
    """the game's files from the parts (GOG's own dependencies, DOSBox and
    the like, left out)"""
    files, cur, dependency = [], None, False
    for e in entries:
        start = call_args(e.before, 'before_install')
        dep = call_args(e.before, 'before_install_dependency')
        if start or dep:
            if cur and cur.left:
                raise InnoError(f'{cur.path}: parts missing')
            args, dependency = start or dep, dep is not None
            check = call_args(e.check, 'check_if_install') or ['']
            cur = File(args[1].replace('\\', '/'), args[0].lower(),
                       [x.strip() for x in check[0].split('#') if x.strip()])
            cur.left = max(int(args[2]), 1) if len(args) > 2 else 1
            if not dependency:
                files.append(cur)
        part = call_args(e.after, 'after_install') or call_args(e.after, 'after_install_dependency')
        if part and cur and cur.left and e.location < len(data):
            cur.parts.append(data[e.location])
            cur.size += int(part[2])
            cur.left -= 1
    return files


def pick_language(files, lang):
    """the files of one language (lang, else en-US, else the first there
    is): GOG's Galaxy installers carry several with the same path"""
    known = sorted({x.lstrip('!') for f in files for x in f.languages})
    if not known:
        return files
    lang = lang or ('en-US' if 'en-US' in known else known[0])
    if lang not in known:
        raise InnoError(f'no language {lang} (there are {", ".join(known)})')

    def wanted(f):
        yes = {x for x in f.languages if not x.startswith('!')}
        no = {x[1:] for x in f.languages if x.startswith('!')}
        return lang not in no and (not yes or lang in yes)
    return [f for f in files if wanted(f)]


# -- the setup

class Setup:
    def __init__(self, path, lang=None):
        self.path = path
        with open(path, 'rb') as f:
            header, data = find_offsets(f.read(SEARCHED))
            f.seek(header)
            self.version, u = parse_version(f.read(64))
            main, second = read_block(f), read_block(f)
        r = Reader(main, u)
        self.name, self.app_id, counts, compression, per_disk = skip_header(r, self.version, u)
        skip_entries(r, self.version, u, counts)
        entries = [Entry(r, self.version) for _ in range(counts[7])]
        r2 = Reader(second, u)
        self.data = [Data(r2, self.version, compression) for _ in range(counts[8])]
        self.files = []
        for e in entries:
            m = re.match(r'(?i)\{app\}\\(.+)', e.dest)
            if m and e.location < len(self.data):
                f = File(m.group(1).replace('\\', '/'))
                f.parts, f.size = [self.data[e.location]], self.data[e.location].size
                self.files.append(f)
        self.files += pick_language(galaxy_files(entries, self.data), lang)
        self.slices = Slices(path, data, per_disk)

    def unpack(self, folder, names=None):
        """the files (those named, if names) into folder; returns their count"""
        want = [f for f in self.files if names is None or f.path in names]
        users = {}                          # data entry -> [(file, part number)]
        for f in want:
            for i, p in enumerate(f.parts):
                users.setdefault(id(p), (p, []))[1].append((f, i))
        chunks = {}
        for p, us in users.values():
            chunks.setdefault(p.chunk_key(), []).append((p, us))
        out = Output(folder)
        for key in sorted(chunks):
            self.unpack_chunk(sorted(chunks[key], key=lambda x: x[0].offset), out)
        out.finish(want)
        return len(want)

    def unpack_chunk(self, group, out):
        d0 = group[0][0]
        if d0.encrypted:
            raise InnoError('encrypted, not read here')
        src = self.slices.pieces(d0.first_slice, d0.chunk_offset, 4 + d0.chunk_size)
        stream = Stream(inflate(d0.compression, src))
        for d, users in group:
            stream.skip(d.offset - stream.pos)
            h = hashlib.new(d.hash)
            if d.exe_filter:
                body = call_filter(stream.read(d.size), d.flip)
                h.update(body)
                pieces = [body]
            else:
                pieces = stream.pieces(d.size, h)
            out.part(users, pieces)
            if h.digest() != d.digest:
                raise InnoError(f'{users[0][0].path}: {d.hash.upper()} wrong')


class Stream:
    """a chunk's decompressed bytes, read forward"""

    def __init__(self, source):
        self.source, self.buf, self.pos = source, b'', 0

    def fill(self):
        try:
            self.buf += next(self.source)
        except StopIteration:
            raise InnoError('a chunk ends early')

    def read(self, n):
        while len(self.buf) < n:
            self.fill()
        b, self.buf = self.buf[:n], self.buf[n:]
        self.pos += n
        return b

    def skip(self, n):
        for _ in self.pieces(n, None):
            pass

    def pieces(self, n, h):
        while n:
            if not self.buf:
                self.fill()
            b, self.buf = self.buf[:n], self.buf[n:]
            n -= len(b)
            self.pos += len(b)
            if h:
                h.update(b)
            yield b


def inflate(method, src):
    """a chunk's bytes, decompressed, from the pieces src gives: first its
    magic "zlb\x1a", then the compressed stream"""
    data = b''
    while len(data) < 4:
        data += next(src, b'') or _early()
    if data[:4] != b'zlb':
        raise InnoError('no chunk where the header says')
    data = data[4:]
    if method == STORED:
        yield data
        yield from src
        return
    if method == ZLIB:
        dec = zlib.decompressobj()
    elif method == BZIP2:
        dec = bz2.BZ2Decompressor()
    else:
        n = 5 if method == LZMA1 else 1
        while len(data) < n:
            data += next(src, b'') or _early()
        dec, _ = lzma_raw(data[:n], b'', lzma2=method == LZMA2)
        data = data[n:]
    yield dec.decompress(data)
    for piece in src:
        yield dec.decompress(piece)


def _early():
    raise InnoError('a chunk ends early')


class Output:
    """the files written as their parts come: a GOG Galaxy file's parts
    inflated one after another, its MD5 checked at the end"""

    def __init__(self, folder):
        self.folder, self.open = folder, {}

    def part(self, users, pieces):
        pieces = list(pieces) if len(users) > 1 else pieces
        for f, i in users:
            st = self.open.get(id(f))
            if st is None:
                if i:
                    raise InnoError(f'{f.path}: its parts out of order')
                path = os.path.join(self.folder, *f.path.split('/'))
                os.makedirs(os.path.dirname(path) or '.', exist_ok=True)
                st = self.open[id(f)] = [open(path, 'wb'), hashlib.md5(), 0]
            elif i != st[2]:
                raise InnoError(f'{f.path}: its parts out of order')
            z = zlib.decompressobj() if f.md5 else None
            for b in pieces:
                if z:
                    b = z.decompress(b)
                    st[1].update(b)
                st[0].write(b)
            if z:
                b = z.flush()
                st[1].update(b)
                st[0].write(b)
            st[2] += 1

    def finish(self, files):
        for f in files:
            st = self.open.pop(id(f), None)
            if st is None:
                continue
            st[0].close()
            if st[2] != len(f.parts):
                raise InnoError(f'{f.path}: parts missing')
            if f.md5 and st[1].hexdigest() != f.md5:
                raise InnoError(f'{f.path}: MD5 wrong')


def call_filter(b, flip):
    """undo Inno's x86 CALL/JMP filter (5.2.0 and later): the 24-bit
    targets after E8/E9 were made absolute; 64 KB blocks, an instruction
    across a block's end left alone"""
    b = bytearray(b)
    i, n = 0, len(b)
    while i < n:
        c = b[i]
        i += 1
        if c not in (0xe8, 0xe9) or 0x10000 - ((i - 1) % 0x10000) < 5:
            continue
        if i + 4 > n:
            break
        if b[i + 3] in (0, 0xff):
            addr = (i + 4) & 0xffffff
            rel = (b[i] | b[i + 1] << 8 | b[i + 2] << 16) - addr
            b[i], b[i + 1], b[i + 2] = rel & 0xff, rel >> 8 & 0xff, rel >> 16 & 0xff
            if flip and rel & 0x800000:
                b[i + 3] ^= 0xff
        i += 4
    return bytes(b)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('setup')
    ap.add_argument('-x', metavar='DIR', help='unpack into DIR')
    ap.add_argument('--lang', help='the language, where the setup has several (en-US, de-DE ...)')
    ap.add_argument('names', nargs='*')
    a = ap.parse_args()
    try:
        s = Setup(a.setup, a.lang)
        if a.x:
            n = s.unpack(a.x, set(a.names) or None)
            print(f'{n} files unpacked into {a.x}')
        else:
            print(f'{s.name} (ID {s.app_id}), Inno Setup {".".join(map(str, s.version[:3]))}, '
                  f'{len(s.files)} files')
            for f in s.files:
                print(f'{f.size:>12}  {f.path}')
    except InnoError as e:
        sys.exit(f'{a.setup}: {e}')


if __name__ == '__main__':
    main()
