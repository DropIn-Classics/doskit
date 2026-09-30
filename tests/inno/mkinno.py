"""A small Inno Setup installer (the 5.6.2 Unicode format, as GOG's), made
for the selftest: tools/inno.py reads it back.  Nothing of any game.

    mkinno.write(path, slices=0, extra=None)
                                   -> {language: {path: bytes}} expected

What it holds: README.TXT and GAME/PROG.EXE in one LZMA chunk (PROG.EXE
through Inno's CALL/JMP filter), and, as GOG's Galaxy installers put
them, DATA/BIG.DAT in two deflated parts under {tmp}, LANG.TXT once in
English and once in German, and a dependency (DOSBOX/DEP.EXE) that is
not the game's.  With slices=2 the data is in PATH-1.bin and PATH-2.bin
(a chunk across the two) instead of the .exe.  extra ({path: bytes}) are
more files into {app}, in the LZMA chunk (a CD image, say).
"""
import hashlib, lzma, os, struct, zlib

VERSION = b'Inno Setup Setup Data (5.6.2) (u)'
MAGIC = b'rDlPtS\xcd\xe6\xd7{\x0b*'
LZMA_FILTER = {'id': lzma.FILTER_LZMA1, 'dict_size': 1 << 16, 'lc': 3, 'lp': 0, 'pb': 2}
LZMA_PROPS = bytes([(2 * 5 + 0) * 9 + 3]) + struct.pack('<I', 1 << 16)


def s(text):
    b = text.encode('utf-16-le')
    return struct.pack('<I', len(b)) + b


def lzma1(data):
    return LZMA_PROPS + lzma.compress(data, format=lzma.FORMAT_RAW, filters=[LZMA_FILTER])


def block(data):
    """a header block: CRC32, size, compressed, 4096-byte chunks behind CRC32s"""
    packed = lzma1(data)
    body = b''.join(struct.pack('<I', zlib.crc32(packed[i:i + 4096])) + packed[i:i + 4096]
                    for i in range(0, len(packed), 4096))
    head = struct.pack('<IB', len(body), 1)
    return struct.pack('<I', zlib.crc32(head)) + head + body


def call_encode(b):
    """Inno's CALL/JMP filter (5.3.9 and later), the way the compiler applies it"""
    b = bytearray(b)
    i = 0
    while i < len(b):
        c = b[i]
        i += 1
        if c not in (0xe8, 0xe9) or 0x10000 - ((i - 1) % 0x10000) < 5 or i + 4 > len(b):
            continue
        if b[i + 3] in (0, 0xff):
            rel = b[i] | b[i + 1] << 8 | b[i + 2] << 16
            addr = (i + 4) & 0xffffff
            enc = (rel + addr) & 0xffffff
            b[i], b[i + 1], b[i + 2] = enc & 0xff, enc >> 8 & 0xff, enc >> 16 & 0xff
            if rel & 0x800000:
                b[i + 3] ^= 0xff
        i += 4
    return bytes(b)


def header(files, n_data):
    h = s('Test Setup') + s('Test Setup 1.0') + s('1234567890')
    h += b''.join(s(x) for x in ('(c) nobody', 'nobody', 'http://example.invalid/', '',
                                 '', '', '1.0', '{autopf}\\Test', 'Test'))
    h += s('')                                                  # base filename
    h += b''.join(s('') for _ in range(7 + 4 + 6 + 3))
    h += s('')                                                  # compiled code
    counts = [1, 0, 0, 0, 0, 0, 0, len(files), n_data] + [0] * 7
    h += struct.pack('<16I', *counts)
    h += bytes(20) + bytes(8) + b'\0' + bytes(20) + bytes(8)    # versions .. salt
    h += struct.pack('<qI', 0, 1)                               # extra space, slices per disk
    h += bytes(3) + bytes(2) + b'\x03' + bytes(2) + bytes(2)    # .. compression LZMA1 ..
    h += struct.pack('<Q', 0)
    h += bytes(6)                                               # header flags (44: 6 bytes)
    # one language
    h += b''.join(s(x) for x in ('english', 'English', 'Tahoma', 'Tahoma', 'Tahoma', 'Tahoma',
                                 '', '', '', ''))
    h += struct.pack('<I', 0x409) + bytes(16) + b'\0'
    for dest, loc, check, after, before in files:
        h += s('') + s(dest) + s('') + s('') + s('') + s('') + s('')
        h += s(check) + s(after) + s(before)
        h += bytes(20) + struct.pack('<IIQh', loc, 0, 0, -1) + bytes(4) + b'\0'
    return h


def data_entry(slice_no, chunk_offset, offset, body, stored_size, chunk_size, flags):
    e = struct.pack('<III', slice_no, slice_no, chunk_offset)
    e += struct.pack('<QQQ', offset, stored_size, chunk_size)
    e += hashlib.sha1(body).digest() + bytes(16)
    return e + struct.pack('<H', flags)


def write(path, slices=0, extra=None):
    readme = b'a readme\r\n' * 300
    prog = bytes(range(256)) * 8 + b'\xe8\x10\x00\x00\x00' + b'\x90' * 7 + b'\xe9\xf0\xff\xff\xff'
    prog += b'\xe8\x00\x00\x80\x00' + bytes(300)            # bit 23 set: the high byte flipped
    big = bytes((i * 7) & 0xff for i in range(50000))
    lang = {'en-US': b'hello\r\n', 'de-DE': b'hallo\r\n'}
    dep = b'not the game\r\n'

    chunks, data, files = [], [], []

    def chunk(payload, compressed):
        chunks.append(b'zlb\x1a' + (lzma1(payload) if compressed else payload))
        return len(chunks) - 1

    # README.TXT, GAME/PROG.EXE: one solid LZMA chunk
    extra = extra or {}
    c = chunk(readme + call_encode(prog) + b''.join(extra.values()), True)
    data.append((c, 0, readme, len(readme), 1 << 7))
    data.append((c, len(readme), prog, len(prog), 1 << 7 | 1 << 4))
    files.append(('{app}\\README.TXT', 0, '', '', ''))
    files.append(('{app}\\GAME\\PROG.EXE', 1, '', '', ''))
    at = len(readme) + len(prog)
    for name, body in extra.items():
        data.append((c, at, body, len(body), 1 << 7))
        files.append(('{app}\\' + name.replace('/', '\\'), len(data) - 1, '', '', ''))
        at += len(body)
    # GOG Galaxy parts, each deflated, in chunks of their own
    md5 = hashlib.md5(big).hexdigest()
    for i, part in enumerate((big[:30000], big[30000:])):
        if i:                               # a plain file between the parts
            files.append(('{app}\\BETWEEN.TXT', 0, '', '', ''))
        z = zlib.compress(part)
        data.append((chunk(z, False), 0, z, len(z), 0))
        files.append((f'{{tmp}}\\{i}', len(data) - 1, "check_if_install('en-US#de-DE#','32#64#','')",
                      f"after_install('{hashlib.md5(z).hexdigest()}', {len(z)}, {len(part)})",
                      f"before_install('{md5}', 'DATA\\BIG.DAT', 2)" if i == 0 else ''))
    for code, text in lang.items():
        z = zlib.compress(text)
        data.append((chunk(z, False), 0, z, len(z), 0))
        files.append((f'{{tmp}}\\{code}', len(data) - 1, f"check_if_install('{code}#','32#64#','')",
                      f"after_install('x', {len(z)}, {len(text)})",
                      f"before_install('{hashlib.md5(text).hexdigest()}', 'LANG.TXT', 1)"))
    z = zlib.compress(dep)
    data.append((chunk(z, False), 0, z, len(z), 0))
    files.append(('{tmp}\\dep', len(data) - 1, "check_if_install_dependency('d', False)",
                  f"after_install_dependency('x', {len(z)}, {len(dep)})",
                  f"before_install_dependency('{hashlib.md5(dep).hexdigest()}', 'DOSBOX\\DEP.EXE', 1)"))
    files.append(('{commonappdata}\\elsewhere.dll', 0, '', '', ''))

    # where the chunks go: after the .exe's table, or in the .bin slices,
    # each counting from its own start (its 12-byte head included)
    blob = b''.join(chunks)
    cut = len(blob) // 2 if slices else len(blob)
    where, pos = [], 0
    for ch in chunks:
        where.append((0, pos + 12) if not slices or pos < cut else (1, pos - cut + 12))
        if not slices:
            where[-1] = (0, pos)
        pos += len(ch)
    entries = b''
    for c, off, body, size, flags in data:
        entries += data_entry(*where[c], off, body, size, len(chunks[c]) - 4, flags)
    head = VERSION.ljust(64, b'\0') + block(header(files, len(data))) + block(entries)

    exe = bytearray(b'MZ' + bytes(0x3fe))
    data_offset = 0 if slices else len(exe) + 64
    if not slices:
        exe += bytes(64) + blob
    header_offset = len(exe)
    exe += head
    table = MAGIC + struct.pack('<7I', 1, len(exe), 0, 0, 0, header_offset, data_offset)
    exe[0x200:0x200 + 44] = table + struct.pack('<I', zlib.crc32(table))
    with open(path, 'wb') as f:
        f.write(exe)
    if slices:
        stem = path[:-4]                    # a chunk across the two slices
        for n, piece in enumerate((blob[:cut], blob[cut:]), 1):
            with open(f'{stem}-{n}.bin', 'wb') as f:
                f.write(b'idska32\x1a' + struct.pack('<I', 12 + len(piece)) + piece)
    common = dict({'README.TXT': readme, 'BETWEEN.TXT': readme, 'GAME/PROG.EXE': prog,
                   'DATA/BIG.DAT': big}, **extra)
    return {code: dict(common, **{'LANG.TXT': text}) for code, text in lang.items()}
