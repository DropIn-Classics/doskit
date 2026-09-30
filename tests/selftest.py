#!/usr/bin/env python3
"""The kit's own check: the whole way from a program to its port, on a
program of our own (tests/hello/HELLO.ASM), so no game is needed.

    python3 tests/selftest.py

In build/selftest (a project as a game's would be, see kit.py):

  1. HELLO.ASM assembled and linked with tasm.py and tlink.py into
     game/HELLO/HELLO.EXE (the "shipped program"); tests/flat/FLAT.ASM,
     32-bit, the same way into a pMAX image, build/files/FLAT.386 (as a
     project's tool would unpack it), tests/raw/RAWDRV.ASM into a raw
     32-bit image, build/files/RAWDRV.DRV, tests/relmod/RELMOD.ASM into a
     relocatable one, build/files/RELMOD.MOD with its offrel list
     RELMOD.REL (the linker's OFF32 fixups); tests/pmode/PMODE.ASM into
     game/PMODE/PMODE.EXE, tests/cdrom/CDROM.ASM into game/CDROM/CDROM.EXE,
     tests/vgamode/VGAMODE.ASM into game/VGAMODE/VGAMODE.EXE,
     tests/sb16/SB16.ASM into game/SB16/SB16.EXE, tests/cdplay/CDPLAY.ASM
     into game/CDPLAY/CDPLAY.EXE;
  2. check.py: disasm.py makes their sources from tests/hello/src/HELLO.hints
     and tests/flat/src/FLAT.hints, tests/raw/RAWDRV.ASM's raw image from
     tests/raw/src/RAWDRV.hints, RELMOD's from tests/relmod/src/RELMOD.hints
     (its offsets exactly the offrel list's, the numbers that look like
     offsets left numbers), build.py rebuilds them byte for byte; RELMOD
     with a dword of a words table taken out of its offrel list fails
     build.py's check;
     PROVENANCE.md (the template's) is there; the names headers symmap.py
     wrote (HELLO's, FLAT's) are up to date; FLAT without its raw hint rebuilds too (the
     line tasm.py refuses written as DB by build.py);
  3. run.py: its table of the runner's options has every option the
     runner parses (one it lacks is taken for PROGRAM, and the addresses
     after it go untranslated); the program in the runner, stopped at
     its end (CODE:0026), its console line read, memory and video memory written out; PMODE.EXE
     in the runner, which checks the runner's protected mode itself (into
     it through INT 15h AH=89h, a #GP, IRQ0 through the IDT, ring 3, a
     call gate, V86 mode with the I/O bitmap, back to real mode; then its
     own file opened for writing, with no layer folder yet) and says
     "pmode ok"; CDROM.EXE, which checks the runner's MSCDEX (the drive,
     device requests, an audio play and stop; INT 21h AH=57h) and says
     "cdrom ok", on the default disc and on the tracks of a cue sheet
     (-cue: a data track and two audio tracks, a WAVE made here and
     tests/cdplay/TONE.OGG), whose table the runner prints as expected;
     CDPLAY.EXE, which sets the CD's channels (swapped, one at half
     volume), reads them back and plays from the WAVE's last second
     through the pregap to the Ogg's end, and says "cdplay ok", its
     -cdwav holding the WAVE's samples exactly, the pregap's silence and
     the Ogg's two tones at their pitch and loudness, all on their
     channels; VGAMODE.EXE, which checks the runner's BIOS mode set
     (modes 0Dh and 0Eh planar at A0000h, back to text; VESA 4F00h,
     4F01h, 4F02h with modes 101h and 103h, 4F03h) and says
     "vgamode ok"; SB16.EXE, which checks the runner's
     Sound Blaster 16 (the DSP's reset, the mixer's IRQ and DMA, a 16-bit
     transfer on DMA 5 and an 8-bit one on DMA 1, each ending in IRQ 7,
     and a 16-bit one started with DMA 5 masked, which waits for the
     unmask) and says "sb16 ok", its -wav holding the 144 samples it
     played; the
     same with a -log on its wait loop and with a -shot during the wait
     leaves the same memory (looking does not change a run);
  4. the C runtime: tests/hello/port.c (HELLO in C over rmem.h and vga.h,
     on plat_null.c) built with cc, run on the same program; its memory
     compared with the runner's by memcmp.py (CODE, DATA and video memory;
     the stack is not the same and not compared); its picture as a PNG
     (shot.c, DK_SHOTS) read back and compared with its PPM (DK_DUMP);
     tests/shot/shottest.c's PNGs (noise, long runs, far repeats) read
     back and compared with the pixels it wrote; tests/vgamode/runtime.c
     (the runtime's vga.c in modes 0Dh and 0Eh: size, a planar pixel and
     its colour, 70 Hz) says "vga modes ok"; tests/update/updatetest.c
     (update.c: versions, latest.json's fields, nothing before the
     player's yes, a latest.json made here fetched by curl as file://,
     then the kept one used the same day; sys_data_migrate) says
     "update ok" twice, when curl is there; tests/gogfind/gogfind.c
     (cdimage.c's gog_find, HOME a folder laid out as the Linux
     installer, a menu entry, Heroic, Wine and Lutris leave it; the image
     in a folder data, its name in other case; a folder of another name
     taken only with a must_have) says which image it found;
     tests/flat/port.c (the start of FLAT.386 in C over pmem.h, loaded at a linear address with
     a selector per descriptor) against the memory made here from the
     image, by memcmp.py --base, which also finds a byte changed in it;
     the runner's -mem (the HELLO run of step 3) begins with -ram's bytes;
  5. every runtime module compiled with warnings as errors (plat_sdl.c
     only when runtime/sdl2-flags.sh finds SDL2; plat_win32.c not here);
  6. new_project.py: a project made from template/ in build/selftest-new
     (the kit linked in as doskit/), its port built with its build.sh and
     run headless on HELLO's files; its check.py says all ok.  Then one
     chosen from the installed GOG games (DOSKIT_GOG_DIRS naming a folder
     made here: a game's goggame-ID.info and a raw CD image with HELLO's
     files, cd_image below, named by a cue sheet; and one of GOG's older
     Mac applications, found by its Info.plist, its *.dat the image and
     not Boxer's DummyCD.iso): its name, ID, folder
     name, image path and marker in its port/src/main.c, the image
     unpacked into its game/ as it was; one installed as a folder (no
     image) copied into game/, its program taken for the marker; the
     template's port started with -gog on that folder (copied into its
     data folder), on the image (unpacked there) and on a folder without
     the marker (refused); without -gog, game.gog put into its data folder
     found; the template's program made into an app by macapp.py (its
     Info.plist read back; on a Mac the bundle's signature verified).

Prints `selftest ok` at the end, exit status 0 then.  Needs cc (clang
or gcc); on Windows it is not written for MSVC yet.
"""
import os, re, shutil, struct, subprocess, sys, zlib

HERE = os.path.dirname(os.path.abspath(__file__))
KIT = os.path.normpath(os.path.join(HERE, '..'))
TOOLS = os.path.join(KIT, 'tools')
RUNTIME = os.path.join(KIT, 'runtime')
sys.path.insert(0, TOOLS)
import build, tasm, tlink

PROJ = os.path.join(KIT, 'build', 'selftest')
CC = os.environ.get('CC', 'cc')
CFLAGS = ['-std=c99', '-O1', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter',
          '-D_POSIX_C_SOURCE=200809L']


def step(what):
    print(f'---- {what}', flush=True)


def run(cmd, check=True, **kw):
    r = subprocess.run(cmd, cwd=PROJ, capture_output=True, text=True,
                       env=dict(os.environ, DOSKIT_PROJECT=PROJ), **kw)
    out = (r.stdout + r.stderr).strip()
    if r.returncode and check:
        print(out)
        raise SystemExit(f'selftest FAILED: {" ".join(os.path.basename(c) for c in cmd[:2])}')
    return out


def read_png(path):
    """width, height and the RGB bytes of an 8-bit indexed PNG (shot.c's
    kind: filter 0 on every row)"""
    data = open(path, 'rb').read()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise SystemExit(f'selftest FAILED: {path} is not a PNG')
    pos, idat, plte, w = 8, b'', b'', 0
    while pos < len(data):
        n, kind = struct.unpack('>I4s', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        if zlib.crc32(kind + body) != struct.unpack('>I', data[pos + 8 + n:pos + 12 + n])[0]:
            raise SystemExit(f'selftest FAILED: {path}: the CRC of {kind}')
        if kind == b'IHDR':
            w, h, depth, color = struct.unpack('>IIBB', body[:10])
            if (depth, color) != (8, 3):
                raise SystemExit(f'selftest FAILED: {path} is not 8-bit indexed')
        elif kind == b'PLTE':
            plte = body
        elif kind == b'IDAT':
            idat += body
        pos += 12 + n
    raw = zlib.decompress(idat)
    rgb = bytearray()
    for y in range(h):
        row = raw[y * (w + 1):(y + 1) * (w + 1)]
        if row[0] != 0:
            raise SystemExit(f'selftest FAILED: {path}: filter {row[0]}')
        for i in row[1:]:
            rgb += plte[3 * i:3 * i + 3]
    return w, h, bytes(rgb)


def make_exe(name='HELLO'):
    a = tasm.Assembler(os.path.join(HERE, name.lower(), name + '.ASM'))
    a.assemble()
    out = tlink.link([tlink.module_from_asm(a, name)])
    exe = build.write_mz(out, None, [0])
    os.makedirs(os.path.join(PROJ, 'game', name))
    with open(os.path.join(PROJ, 'game', name, name + '.EXE'), 'wb') as f:
        f.write(exe)
    return len(exe)


# the cue sheet's disc as the runner prints it: the data track 300
# sectors; the WAVE 150 frames (2 s), and the PREGAP of 2 s before track 3
# in its span, as a disc's table of contents has it; the Ogg 1 s (44100
# samples at 44.1 kHz)
CUE_TABLE = ['cd: 3 tracks, lead-out 00:11:00',
             'cd: track  1 data  00:02:00, 300 frames',
             'cd: track  2 audio 00:06:00, 300 frames',
             'cd: track  3 audio 00:10:00, 75 frames']


def wave_sample(i):
    """the WAVE track's stereo sample i, a pattern that does not repeat
    within the track"""
    return (i * 37 % 20001 - 10000, i * 91 % 16001 - 8000)


def make_cue():
    """a cue sheet as GOG writes them (Windows names, other case than
    the files) over a data track, a WAVE made here and TONE.OGG"""
    import struct
    d = os.path.join(PROJ, 'build', 'cue')
    os.makedirs(os.path.join(d, 'music'), exist_ok=True)
    with open(os.path.join(d, 'data.bin'), 'wb') as f:
        f.write(bytes(2352 * 300))
    with open(os.path.join(d, 'music', 'track02.wav'), 'wb') as f:
        n = 2352 * 150
        f.write(b'RIFF' + struct.pack('<I', 36 + n) + b'WAVEfmt ' +
                struct.pack('<IHHIIHH', 16, 1, 2, 44100, 44100 * 4, 4, 16) +
                b'data' + struct.pack('<I', n) +
                b''.join(struct.pack('<hh', *wave_sample(i)) for i in range(n // 4)))
    shutil.copy(os.path.join(HERE, 'cdplay', 'TONE.OGG'), os.path.join(d, 'music', 'track03.ogg'))
    cue = os.path.join(d, 'game.inst')
    with open(cue, 'w') as f:
        f.write('FILE "DATA.BIN" BINARY\n\tTRACK 01 MODE2/2352\n\t INDEX 01 00:00:00\n'
                'FILE "MUSIC\\Track02.wav" WAVE\n\tTRACK 02 AUDIO\n\t INDEX 01 00:00:00\n'
                'FILE "MUSIC\\TRACK03.OGG" MP3\n\tTRACK 03 AUDIO\n\tPREGAP 00:02:00\n'
                '\t INDEX 01 00:00:00\n')
    return cue


def check_cdplay(py, b):
    """CDPLAY.EXE on the cue sheet's disc with -cdwav: the channels set
    and read back, then what the drive played, sample by sample where the
    source is exact (the WAVE, the silence), by pitch and loudness where
    it is decoded (the Ogg)"""
    import struct
    wav = os.path.join(b, 'cdplay.wav')
    out = run([py, os.path.join(TOOLS, 'run.py'), '-until', '8', '-cd', '-cue', make_cue(),
               '-cdwav', wav, 'CDPLAY/CDPLAY.EXE'])
    lines = out.splitlines()
    print('\n'.join(l for l in lines if l.startswith(('cd: channels', 'cd: play', 'con:'))))
    play = [l for l in lines if l.startswith('cd: play frames 525..825 (track 2 + 75) t=')]
    if ('con: cdplay ok' not in out or len(play) != 1 or
            not any(l.startswith('cd: channels 0<-1 FF, 1<-0 80, 2<-2 00, 3<-3 00 ') for l in lines)):
        print(out)
        raise SystemExit('selftest FAILED: CDPLAY.EXE (the runner\'s CD audio)')
    with open(wav, 'rb') as f:
        data = f.read()
    fmt = struct.unpack('<HHIIHH', data[20:36])
    s = struct.unpack('<%dh' % ((len(data) - 44) // 2), data[44:])
    left, right = s[0::2], s[1::2]

    def trunc(x, vol):                  # the runner's x * vol / 255, as C does it
        q = abs(x) * vol // 255
        return q if x >= 0 else -q
    # the WAVE's last second: output 0 is input 1 at FFh, output 1 input 0
    # at 80h; its start is the play's moment (the -cd line), give or take
    # the one sample the printed time rounds
    k0 = int(float(play[0].split('t=')[1]) * 44100)
    src = [wave_sample(44100 + i) for i in range(44100)]
    want_l = [r for l, r in src]
    want_r = [trunc(l, 0x80) for l, r in src]
    k = next((k for k in (k0 - 1, k0, k0 + 1)
              if list(left[k:k + 44100]) == want_l and list(right[k:k + 44100]) == want_r), None)
    bad = []
    if fmt != (1, 2, 44100, 44100 * 4, 4, 16):
        bad.append(f'format {fmt}')
    if k is None:
        bad.append(f'the WAVE\'s samples not at {k0}')
    else:
        if any(left[:k]) or any(right[:k]):
            bad.append('sound before the play')
        if any(left[k + 44100:k + 3 * 44100]) or any(right[k + 44100:k + 3 * 44100]):
            bad.append('sound in the pregap')
        if any(left[k + 4 * 44100:]) or any(right[k + 4 * 44100:]):
            bad.append('sound after the play')
        # the Ogg: left 441 Hz at 1/2, right 1102.5 Hz at 1/4; swapped, the
        # left output the 1102.5 Hz tone at 1/4, the right the 441 Hz at
        # 1/2 x 80h/FFh; sign changes 2 a period, loudness amplitude / sqrt 2
        for name, ch, hz, amp in (('left', left, 1102.5, 0.25), ('right', right, 441, 0.5 * 0x80 / 255)):
            seg = ch[k + 3 * 44100:k + 4 * 44100]
            flips = sum(1 for a, c in zip(seg, seg[1:]) if (a < 0) != (c < 0))
            rms = (sum(x * x for x in seg) / len(seg)) ** 0.5
            want = amp * 32767 / 2 ** 0.5
            print(f'cdplay.wav: the Ogg\'s {name}: {flips} sign changes, loudness {rms:.0f} '
                  f'(expected {2 * hz:.0f}, {want:.0f})')
            if abs(flips - 2 * hz) > 10 or abs(rms - want) > 0.05 * want:
                bad.append(f'the Ogg\'s {name} channel')
    print(f'cdplay.wav: {len(left)} samples; the play at sample {k}')
    if len(left) < (k or 0) + 4 * 44100 + 15000 or (k or 0) < 15000:
        bad.append('no pause before and after the play')
    if bad:
        raise SystemExit('selftest FAILED: CDPLAY.EXE\'s -cdwav: ' + ', '.join(bad))


def make_flat():
    """tests/flat/FLAT.ASM as a pMAX image, its segments the descriptors"""
    a = tasm.Assembler(os.path.join(HERE, 'flat', 'FLAT.ASM'))
    a.assemble()
    out = tlink.link([tlink.module_from_asm(a, 'FLAT')])
    segs = [type('Seg', (), {'name': n}) for n in a.segorder]
    img = build.write_pmax(out, segs)
    os.makedirs(os.path.join(PROJ, 'build', 'files'))
    with open(os.path.join(PROJ, 'build', 'files', 'FLAT.386'), 'wb') as f:
        f.write(img)
    return len(img)


def make_raw():
    """tests/raw/RAWDRV.ASM as a raw 32-bit image: the linked image alone"""
    a = tasm.Assembler(os.path.join(HERE, 'raw', 'RAWDRV.ASM'))
    a.assemble()
    out = tlink.link([tlink.module_from_asm(a, 'RAWDRV')])
    with open(os.path.join(PROJ, 'build', 'files', 'RAWDRV.DRV'), 'wb') as f:
        f.write(bytes(out.img))
    return len(out.img)


def make_relmod():
    """tests/relmod/RELMOD.ASM as a relocatable raw image and its offrel
    list: the image offsets of the linker's OFF32 fixups, in reverse order
    (the order is not the analysis's business)"""
    a = tasm.Assembler(os.path.join(HERE, 'relmod', 'RELMOD.ASM'))
    a.assemble()
    out = tlink.link([tlink.module_from_asm(a, 'RELMOD')])
    files = os.path.join(PROJ, 'build', 'files')
    with open(os.path.join(files, 'RELMOD.MOD'), 'wb') as f:
        f.write(bytes(out.img))
    with open(os.path.join(files, 'RELMOD.REL'), 'wb') as f:
        f.write(b''.join(struct.pack('<I', x) for x in sorted(out.offs32, reverse=True)))
    return len(out.img), len(out.offs32)


def check_update(b):
    """tests/update/updatetest.c in a data folder of its own: a first start
    fetching a latest.json made here, then one the same day using the kept
    file (a fetch would fail: the address is nowhere)"""
    if not shutil.which('curl'):
        print('curl not found: update.c\'s fetch not checked')
        return
    d = os.path.join(b, 'update')
    if os.path.isdir(d):
        shutil.rmtree(d)
    os.makedirs(os.path.join(d, 'data'))
    exe = os.path.join(d, 'updatetest')
    run([CC] + CFLAGS + ['-I', RUNTIME, '-o', exe, os.path.join(HERE, 'update', 'updatetest.c'),
                         os.path.join(RUNTIME, 'update.c'), os.path.join(RUNTIME, 'sys.c')])
    with open(os.path.join(d, 'latest.json'), 'w') as f:
        f.write('{"version": "v1.3", "page": "https://github.com/o/r/releases/tag/v1.3",\n'
                ' "notes": "Faster.\\nFixed.", "packages": {}}\n')
    env = dict(os.environ, DK_DATA_DIR=os.path.join(d, 'data'))
    url = 'file://' + os.path.join(d, 'latest.json').replace(os.sep, '/')
    for mode, where in (('fetch', url), ('kept', 'file:///nowhere/latest.json')):
        r = subprocess.run([exe, mode, where], capture_output=True, text=True, env=env)
        if r.returncode or 'update ok' not in r.stdout:
            print(r.stdout + r.stderr)
            raise SystemExit(f'selftest FAILED: update.c ({mode})')
        print(r.stdout.strip())


def check_gogfind(b):
    """tests/gogfind/gogfind.c (cdimage.c's gog_find) with HOME a folder
    made here as each kind of installation lays it out, one at a time"""
    d = os.path.join(b, 'gogfind')
    if os.path.isdir(d):
        shutil.rmtree(d)
    os.makedirs(os.path.join(d, 'data'))
    exe = os.path.join(d, 'gogfind')
    run([CC] + CFLAGS + ['-I', RUNTIME, '-o', exe, os.path.join(HERE, 'gogfind', 'gogfind.c'),
                         os.path.join(RUNTIME, 'cdimage.c'), os.path.join(RUNTIME, 'sys.c')])
    image = os.path.join(d, 'image')
    cd_image({'HELLO/HELLO.EXE': b'MZ' + bytes(100)}, image)
    home = os.path.join(d, 'home')
    menu = '.local/share/applications/gog_com-Test_Game_1.desktop'
    heroic = '.config/heroic/gog_store/installed.json'
    # (what, the image's place in HOME, other files {path: text}, WINEPREFIX,
    #  must_have, found)
    cases = [
        ('the Linux installer\'s default folder', 'GOG Games/Test Game/data/game.gog', {}, '',
         'HELLO/HELLO.EXE', True),
        ('a folder named in a menu entry\'s Path=', 'mine/Test Game/data/GAME.GOG',
         {menu: '[Desktop Entry]\nName=Test Game\nPath={home}/mine/Test Game/\n'}, '', '', True),
        ('a folder named in a menu entry\'s Exec=', 'mine/Test Game/game.gog',
         {menu: '[Desktop Entry]\nExec="{home}/mine/Test Game/start.sh"'}, '', '', True),
        ('another name in a menu entry, no must_have', 'mine/Other/data/game.gog',
         {menu: '[Desktop Entry]\nPath={home}/mine/Other\n'}, '', '', False),
        ('another name in a menu entry, with must_have', 'mine/Other/data/game.gog',
         {menu: '[Desktop Entry]\nPath={home}/mine/Other\n'}, '', 'HELLO/HELLO.EXE', True),
        ('Heroic\'s list', 'sd/Test Game/game.gog',
         {heroic: '{{"installed": [{{"install_path": "/nowhere"}},\n'
                  ' {{"appName": "1", "install_path": "{home}\\/sd\\/Test Game"}}]}}'},
         '', '', True),
        ('$WINEPREFIX', 'px/drive_c/GOG Games/Test Game/game.gog', {}, '{home}/px', '', True),
        ('a Lutris prefix, GOG Galaxy\'s folder',
         'Games/test-game/drive_c/Program Files (x86)/GOG Galaxy/Games/Test Game/game.gog', {}, '',
         '', True),
        ('nothing', None, {}, '', 'HELLO/HELLO.EXE', False),
    ]
    for what, place, files, prefix, must, ok in cases:
        if os.path.isdir(home):
            shutil.rmtree(home)
        os.makedirs(home)
        if place:
            os.makedirs(os.path.dirname(os.path.join(home, place)))
            shutil.copy(image, os.path.join(home, place))
        for path, text in files.items():
            os.makedirs(os.path.dirname(os.path.join(home, path)), exist_ok=True)
            with open(os.path.join(home, path), 'w') as f:
                f.write(text.format(home=home))
        env = dict(os.environ, HOME=home, DK_DATA_DIR=os.path.join(d, 'data'),
                   WINEPREFIX=prefix.format(home=home))
        r = subprocess.run([exe, 'Test Game'] + ([must] if must else []), cwd=d,
                           capture_output=True, text=True, env=env)
        want = os.path.join(home, place) if ok else 'not found'
        if r.stdout.strip() != want:
            print(r.stdout + r.stderr)
            raise SystemExit(f'selftest FAILED: gog_find ({what})')
    print(f'gog_find ok ({len(cases)} installations)')


def check_pmem(py, b):
    """tests/flat/port.c (the start of FLAT.386 in C over pmem.h) against
    the memory the image should have there, made here from the file: the
    image at BASE, its selector relocations set, SAVED_DS and COUNT as
    START leaves them; memcmp.py --base finds no difference, and finds
    the one put into a copy."""
    base, sels = 0x11F2A0, (0x1C, 0x24)
    exe = os.path.join(b, 'flatport')
    run([CC] + CFLAGS + ['-I', RUNTIME, '-I', os.path.join(PROJ, 'port'), '-o', exe,
                         os.path.join(HERE, 'flat', 'port.c')] +
        [os.path.join(RUNTIME, f) for f in ('pmem.c', 'sys.c', 'sha256.c')])
    port_mem = os.path.join(b, 'flatport.mem')
    print(run([exe, os.path.join(PROJ, 'build', 'files', 'FLAT.386'), '%X' % base, port_mem]))
    f = open(os.path.join(PROJ, 'build', 'files', 'FLAT.386'), 'rb').read()
    _, alloc, _, n, size, _, nrel = struct.unpack_from('<IIBBIIH', f)
    descs = [struct.unpack_from('<II', f, 20 + 8 * i) for i in range(n)]
    mem = bytearray(0x1000000)
    mem[base:base + size] = f[20 + 8 * n:20 + 8 * n + size]
    for i in range(nrel):
        off, d = struct.unpack_from('<IB', f, 20 + 8 * n + size + 5 * i)
        struct.pack_into('<H', mem, base + off, sels[d])
    a = tasm.Assembler(os.path.join(HERE, 'flat', 'FLAT.ASM'))
    a.assemble()
    code, more = base + descs[0][0], base + descs[1][0]
    struct.pack_into('<H', mem, code + a.syms['SAVED_DS'].value, sels[0])
    count, other = code + a.syms['COUNT'].value, more + a.syms['OTHER'].value
    mem[count:count + 4] = mem[other:other + 4]
    orig = os.path.join(b, 'flat.mem')
    with open(orig, 'wb') as out:
        out.write(mem)
    cmd = [py, os.path.join(TOOLS, 'memcmp.py'), 'src/FLAT.hints', orig, port_mem, '--base', '%X' % base]
    print(run(cmd))
    mem[code + a.syms['COUNT'].value + 2] ^= 0xFF
    with open(orig, 'wb') as out:
        out.write(mem)
    r = subprocess.run(cmd, cwd=PROJ, capture_output=True, text=True,
                       env=dict(os.environ, DOSKIT_PROJECT=PROJ))
    if r.returncode != 1 or 'COUNT+2' not in r.stdout:
        print(r.stdout + r.stderr)
        raise SystemExit('selftest FAILED: memcmp.py --base did not find the changed COUNT')
    print('memcmp.py --base: a changed byte found at COUNT+2')


def check_enc32():
    """tests/enc32/ENC32.ASM: what capstone reads from each 32-bit
    instruction tasm.py makes, and its length, as the line's comment says."""
    import re, struct, capstone
    path = os.path.join(HERE, 'enc32', 'ENC32.ASM')
    a = tasm.Assembler(path)
    a.assemble()
    seg = a.segments['CODE']
    img = bytearray(seg.data)
    for off, kind, target, addend in seg.fixups:     # one segment, at 0
        if kind == 'OFF32':
            struct.pack_into('<I', img, off, addend & 0xFFFFFFFF)
        elif kind != 'SEG':
            raise SystemExit(f'selftest FAILED: fixup {kind} in ENC32')
    names = {n: s.value for n, s in a.syms.items() if s.kind in ('label', 'var')}
    want = [l.split('; =', 1)[1] for l in open(path) if '; =' in l and l.split(';')[0].strip()]
    if len(want) != len(a.seq_pc):
        raise SystemExit(f'selftest FAILED: {len(a.seq_pc)} instructions, {len(want)} expectations')
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    bad = 0
    for seq, text in enumerate(want):
        pc = a.seq_pc[seq][1]
        for part in text.split(';'):
            m = re.fullmatch(r'\s*(.*?)\s*\[(\d+)\]\s*', part)
            exp = re.sub(r'[A-Z_][A-Z0-9_]+', lambda n: hex(names[n.group(0)]) if n.group(0) in names else n.group(0), m.group(1))
            i = next(md.disasm(bytes(img[pc:pc + 16]), pc), None)
            got = f'{i.mnemonic} {i.op_str}'.strip() if i else '?'
            if got != exp or i.size != int(m.group(2)):
                print(f'  {pc:04X} {bytes(img[pc:pc + (i.size if i else 4)]).hex(" ")}: '
                      f'{got} [{i.size if i else 0}], expected {exp} [{m.group(2)}]')
                bad += 1
            pc += i.size if i else 1
    if bad:
        raise SystemExit(f'selftest FAILED: {bad} of the 32-bit encodings')
    return len(want)


def cd_image(files, path):
    """a raw CD image (2352-byte Mode 1 sectors, as GOG's game.gog) with an
    ISO 9660 file system holding `files` ({'DIR/NAME.EXT': bytes}, one
    folder deep), as much of it as isox.py and cdimage.c read"""
    def rec(name, lba, size, is_dir):
        r = struct.pack('<BBIIII7sBBBHHB', 0, 0, lba, 0, size, 0, bytes(7),
                        2 if is_dir else 0, 0, 0, 1, 1, len(name)) + name
        r += b'\0' * (len(r) & 1)
        return bytes([len(r)]) + r[1:]

    def directory(lba, parent, entries):
        data = rec(b'\0', lba, 2048, True) + rec(b'\1', parent, 2048, True) + b''.join(entries)
        return data.ljust(2048, b'\0')

    dirs = sorted({f.split('/')[0] for f in files})
    sectors = {}
    lba = 19 + len(dirs)            # 16 the PVD, 17 the terminator, 18 the root, the folders
    root_entries = []
    for i, d in enumerate(dirs):
        entries = []
        for f in sorted(x for x in files if x.split('/')[0] == d):
            data = files[f]
            entries.append(rec(f.split('/')[1].encode() + b';1', lba, len(data), False))
            for k in range(0, max(len(data), 1), 2048):
                sectors[lba] = data[k:k + 2048].ljust(2048, b'\0')
                lba += 1
        sectors[19 + i] = directory(19 + i, 18, entries)
        root_entries.append(rec(d.encode(), 19 + i, 2048, True))
    sectors[18] = directory(18, 18, root_entries)
    pvd = bytearray(2048)
    pvd[0:7] = b'\1CD001\1'
    pvd[156:156 + 34] = rec(b'\0', 18, 2048, True)
    sectors[16] = bytes(pvd)
    sectors[17] = b'\xffCD001\1'.ljust(2048, b'\0')
    with open(path, 'wb') as f:
        for n in range(lba):
            f.write(b'\0' + b'\xff' * 10 + b'\0' + bytes(3) + b'\1' +
                    sectors.get(n, bytes(2048)) + bytes(288))


def main():
    if os.path.isdir(PROJ):
        shutil.rmtree(PROJ)
    os.makedirs(os.path.join(PROJ, 'src'))
    os.makedirs(os.path.join(PROJ, 'port'))

    step('0. 32-bit instructions (tests/enc32)')
    print(f'{check_enc32()} lines as capstone reads them')

    step('1. HELLO.EXE assembled and linked')
    print(f'{make_exe()} bytes; FLAT.386 {make_flat()} bytes; RAWDRV.DRV {make_raw()} bytes; '
          f'RELMOD.MOD %d bytes, %d offsets; PMODE.EXE {make_exe("PMODE")} bytes; '
          f'CDROM.EXE {make_exe("CDROM")} bytes; VGAMODE.EXE {make_exe("VGAMODE")} bytes; '
          f'SB16.EXE {make_exe("SB16")} bytes; CDPLAY.EXE {make_exe("CDPLAY")} bytes'
          % make_relmod())
    shutil.copy(os.path.join(HERE, 'hello', 'src', 'HELLO.hints'), os.path.join(PROJ, 'src'))
    shutil.copy(os.path.join(HERE, 'flat', 'src', 'FLAT.hints'), os.path.join(PROJ, 'src'))
    shutil.copy(os.path.join(HERE, 'raw', 'src', 'RAWDRV.hints'), os.path.join(PROJ, 'src'))
    shutil.copy(os.path.join(HERE, 'relmod', 'src', 'RELMOD.hints'), os.path.join(PROJ, 'src'))
    # every project carries PROVENANCE.md (check.py insists)
    with open(os.path.join(KIT, 'template', 'PROVENANCE.md'), 'rb') as f:
        text = f.read().replace(b'{{NAME}}', b'HELLO (the kit\'s test program)')
    with open(os.path.join(PROJ, 'PROVENANCE.md'), 'wb') as f:
        f.write(text)

    step('2. rebuilt from the hints (check.py)')
    py = sys.executable
    print(run([py, os.path.join(TOOLS, 'symmap.py'), 'port/hello_names.h', 'HELLO',
               'HELLO=src/HELLO.hints']))
    print(run([py, os.path.join(TOOLS, 'symmap.py'), 'port/flat_names.h', 'FLAT',
               'FLAT=src/FLAT.hints']))
    out = run([py, os.path.join(TOOLS, 'check.py')])
    print(out)
    if not out.splitlines()[-1].startswith('all ok'):
        raise SystemExit('selftest FAILED: check.py')
    with open(os.path.join(PROJ, 'build', 'FLAT.ASM')) as f:
        text = f.read()
    for want in ('DW L00D5-C00D6', 'DW L00DE-C00D6', 'DW L00E5-C00D6', '[EDI+C00D6]'):
        if want not in text:
            raise SystemExit(f'selftest FAILED: FLAT.ASM has no {want} (rwords hint)')
    print('FLAT: the switch table written as offsets between labels')
    for want in ('DD L00FC', 'DD L0103'):
        if want not in text:
            raise SystemExit(f'selftest FAILED: FLAT.ASM has no {want} (words stride=8)')
    print('FLAT: the records\' handlers reached and written as labels')
    for want in ('DW L0112-C010E', 'DW L0119-C010E'):
        if want not in text:
            raise SystemExit(f'selftest FAILED: FLAT.ASM has no {want} (rwords stride=4 from=)')
    print('FLAT: offsets in records written as differences of labels')
    for want in ('[ESI+11AH]', '[ESI+119H]'):
        if want not in text:
            raise SystemExit(f'selftest FAILED: FLAT.ASM has no {want} (a field offset into code)')
    print('FLAT: displacements with a register into code written as numbers')
    noraw = os.path.join(PROJ, 'build', 'FLATNORAW.hints')
    with open(os.path.join(HERE, 'flat', 'src', 'FLAT.hints')) as f:
        text = ''.join(l for l in f if not l.startswith('raw'))
    with open(noraw, 'w') as f:
        f.write(text)
    out = run([py, os.path.join(TOOLS, 'build.py'), noraw])
    if 'IDENTICAL' not in out or 'raw CODE:0086' not in out:
        print(out)
        raise SystemExit('selftest FAILED: FLAT without its raw hint')
    print('FLAT without its raw hint: IDENTICAL, CODE:0086 written as DB')
    nostop = os.path.join(PROJ, 'build', 'FLATNOSTOP.hints')
    with open(os.path.join(HERE, 'flat', 'src', 'FLAT.hints')) as f:
        text = ''.join(l for l in f if not l.startswith('stop'))
    with open(nostop, 'w') as f:
        f.write(text)
    out = run([py, os.path.join(TOOLS, 'build.py'), nostop])
    if 'raw CODE:00B7' not in out:
        print(out)
        raise SystemExit('selftest FAILED: FLAT without its stop hint')
    print('FLAT without its stop hint: the data after CODE:00B5 taken for code')
    with open(os.path.join(PROJ, 'build', 'RELMOD.ASM')) as f:
        text = f.read()
    for want in ('DD INIT', 'DD STEP', 'DD NAMETXT+80000000H', 'DD L005E', 'DD L0064',
                 'MOV EAX,OFFSET TABLE', 'MOV ECX,20H', '[EBX*4+TABLE]', 'CMP EAX,[LIMIT]',
                 '\tDB 0BEH\t; mov esi, 0x20\n\tDD NAMETXT\n', '[EBX+NAMETXT]', 'MOV EDX,DS:[SMALL 14H]'):
        if want not in text:
            raise SystemExit(f'selftest FAILED: RELMOD.ASM has no {want} (offrel)')
    if 'DD COUNT' in text or 'OFFSET INIT' in text:
        raise SystemExit('selftest FAILED: RELMOD.ASM has a number written as an offset')
    print('RELMOD: the offrel list\'s dwords written as offsets (in a raw instruction too), the numbers as numbers')
    rel = os.path.join(PROJ, 'build', 'files', 'RELMOD.REL')
    with open(rel, 'rb') as f:
        whole = f.read()
    with open(rel, 'wb') as f:
        # TABLE's first entry, which the words hint still writes as DD
        f.write(b''.join(whole[i:i + 4] for i in range(0, len(whole), 4)
                         if int.from_bytes(whole[i:i + 4], 'little') != 0x18))
    out = run([py, os.path.join(TOOLS, 'build.py'), os.path.join(PROJ, 'src', 'RELMOD.hints')], check=False)
    with open(rel, 'wb') as f:
        f.write(whole)
    if 'which the offrel list has' in out or 'the offrel list does not have' not in out:
        print(out)
        raise SystemExit('selftest FAILED: RELMOD with a shorter offrel list')
    print('RELMOD with a words table\'s dword left out of its offrel list: build.py refuses it')

    step('3. run in the runner (run.py)')
    import run as run_py
    with open(os.path.join(TOOLS, 'run', 'main.c')) as f:
        parsed = set(re.findall(r'strcmp\(a,"(-[a-z]+)"\)', f.read()))
    if parsed != set(run_py.OPTS):
        raise SystemExit('selftest FAILED: run.py\'s OPTS and the runner\'s options differ: '
                         + ' '.join(sorted(parsed ^ set(run_py.OPTS))))
    b = os.path.join(PROJ, 'build')
    out = run([py, os.path.join(TOOLS, 'run.py'), '-until', '1', '-break', 'CODE:0026',
               '-dump', 'counter', '2', '-ram', os.path.join(b, 'orig.ram'),
               '-mem', os.path.join(b, 'orig.mem'),
               '-vram', os.path.join(b, 'orig.vram'), 'HELLO/HELLO.EXE'])
    print('\n'.join(l for l in out.splitlines() if l.startswith(('con:', 'break', 'dump'))))
    if 'con: hello from doskit' not in out:
        raise SystemExit('selftest FAILED: no console line from HELLO.EXE')
    with open(os.path.join(b, 'orig.ram'), 'rb') as f, open(os.path.join(b, 'orig.mem'), 'rb') as g:
        ram, whole = f.read(), g.read()
    if len(whole) != 0x1000000 or whole[:len(ram)] != ram:
        raise SystemExit('selftest FAILED: -mem is not the 16 MB that begin with -ram\'s bytes')
    print('-mem: 16 MB, beginning with -ram\'s 640 KB')
    # -rwatch: the table of two pointers is read by one CALL, a word each;
    # counter by INC and ADD (not by the fetches, not by DOS's AH=9)
    out = run([py, os.path.join(TOOLS, 'run.py'), '-until', '1', '-rwatch', 'counter', '6',
               'HELLO/HELLO.EXE'])
    memr = [l for l in out.splitlines() if l.startswith('[memr]')]
    print('\n'.join(memr))
    readers = [l.split()[1][-2:] + ' ' + l.split()[4] for l in memr if ' read by ' in l and 'times' in l]
    calls = {r.split()[1] for r in readers if r.startswith(('02', '04'))}
    if (sorted(r.split()[0] for r in readers) != ['00', '00', '02', '04'] or len(calls) != 1
            or not all(' 1 times' in l for l in memr if 'times' in l)
            or not memr[-1].endswith('read by 4 readers')):
        print(out)
        raise SystemExit('selftest FAILED: -rwatch on HELLO.EXE\'s counter and table')
    out = run([py, os.path.join(TOOLS, 'run.py'), '-until', '1', 'PMODE/PMODE.EXE'])
    print('\n'.join(l for l in out.splitlines() if l.startswith(('con:', '[cpu]'))))
    if 'con: pmode ok' not in out:
        print(out)
        raise SystemExit('selftest FAILED: PMODE.EXE (the runner\'s protected mode)')
    out = run([py, os.path.join(TOOLS, 'run.py'), '-until', '1', 'CDROM/CDROM.EXE'])
    print('\n'.join(l for l in out.splitlines() if l.startswith('con:')))
    if 'con: cdrom ok' not in out:
        print(out)
        raise SystemExit('selftest FAILED: CDROM.EXE (the runner\'s MSCDEX)')
    out = run([py, os.path.join(TOOLS, 'run.py'), '-until', '1', '-cd', '-cue', make_cue(),
               'CDROM/CDROM.EXE'])
    table = [l for l in out.splitlines() if l.startswith('cd: track') or 'lead-out' in l]
    print('\n'.join(table + [l for l in out.splitlines() if l.startswith('con:')]))
    if 'con: cdrom ok' not in out or table != CUE_TABLE:
        print(out)
        raise SystemExit('selftest FAILED: CDROM.EXE with a cue sheet (the runner\'s -cue)')
    check_cdplay(py, b)
    out = run([py, os.path.join(TOOLS, 'run.py'), '-until', '1', 'VGAMODE/VGAMODE.EXE'])
    print('\n'.join(l for l in out.splitlines() if l.startswith('con:')))
    if 'con: vgamode ok' not in out:
        print(out)
        raise SystemExit('selftest FAILED: VGAMODE.EXE (the runner\'s BIOS mode set)')
    wav = os.path.join(b, 'sb16.wav')
    out = run([py, os.path.join(TOOLS, 'run.py'), '-until', '1', '-wav', wav, 'SB16/SB16.EXE'])
    print('\n'.join(l for l in out.splitlines() if l.startswith('con:')))
    if 'con: sb16 ok' not in out:
        print(out)
        raise SystemExit('selftest FAILED: SB16.EXE (the runner\'s Sound Blaster 16)')
    # 64 words of 1000h (signed 16-bit), 16 bytes of C0h (unsigned 8-bit),
    # both at 3/4 in the WAV (the runner's headroom), 8000 Hz
    import struct
    with open(wav, 'rb') as f:
        data = f.read()
    samples = list(struct.unpack('<%dh' % ((len(data) - 44) // 2), data[44:]))
    rate = struct.unpack('<I', data[24:28])[0]
    print(f'sb16.wav: {len(samples)} samples at {rate} Hz')
    if rate != 8000 or samples != ([0x1000 * 3 // 4] * 64 + [0x40 * 256 * 3 // 4] * 16
                           + [0x1000 * 3 // 4] * 64):
        raise SystemExit('selftest FAILED: SB16.EXE\'s -wav (the samples the Sound Blaster played)')
    # looking does not change the run: a -log on the loop that waits for the
    # Sound Blaster's interrupt (hit every third instruction) and a -shot
    # during the wait leave the same memory as a run without them
    a = tasm.Assembler(os.path.join(HERE, 'sb16', 'SB16.ASM'))
    a.assemble()
    loop = 'SB16.EXE+0000:%04X' % a.syms['WAIT_INNER'].value
    hashes = []
    for extra in ([], ['-log', loop], ['-shot', '0.004', os.path.join(b, 'sb16.png')]):
        out = run([py, os.path.join(TOOLS, 'run.py'), '-until', '1'] + extra + ['SB16/SB16.EXE'])
        hashes.append([l for l in out.splitlines() if l.startswith('hash ')])
        if 'con: sb16 ok' not in out or hashes[-1] != hashes[0]:
            print(out)
            raise SystemExit('selftest FAILED: SB16.EXE with %s ran otherwise' % ' '.join(extra))
    print('SB16.EXE with a -log on its wait loop and with a -shot: the same memory')

    step('4. the C port over the runtime, compared (memcmp.py)')
    exe = os.path.join(b, 'port')
    srcs = [os.path.join(HERE, 'hello', 'port.c')] + [
        os.path.join(RUNTIME, f) for f in ('rmem.c', 'vga.c', 'sys.c', 'sha256.c', 'shot.c',
                                           'plat_null.c')]
    run([CC] + CFLAGS + ['-I', RUNTIME, '-I', os.path.join(PROJ, 'port'), '-o', exe] + srcs)
    out = subprocess.run(
        [exe, os.path.join(PROJ, 'game'), os.path.join(b, 'port.ram'), os.path.join(b, 'port.vram')],
        cwd=PROJ, capture_output=True, text=True,
        env=dict(os.environ, DK_DUMP=os.path.join(b, 'port.ppm'),
                 DK_SHOTS='0:' + os.path.join(b, 'port.png')))
    print(out.stdout.strip())
    if out.returncode or 'con: hello from doskit' not in out.stdout:
        print(out.stderr)
        raise SystemExit('selftest FAILED: the port')
    w, h, rgb = read_png(os.path.join(b, 'port.png'))
    ppm = open(os.path.join(b, 'port.ppm'), 'rb').read()
    if ppm != b'P6\n%d %d\n255\n' % (w, h) + rgb:
        raise SystemExit('selftest FAILED: port.png is not the picture port.ppm holds')
    print(f'port.png: {w}x{h}, {os.path.getsize(os.path.join(b, "port.png"))} bytes, as port.ppm')
    shots = os.path.join(b, 'shots')
    os.makedirs(shots, exist_ok=True)
    exe = os.path.join(b, 'shottest')
    run([CC] + CFLAGS + ['-I', RUNTIME, '-o', exe, os.path.join(HERE, 'shot', 'shottest.c'),
                         os.path.join(RUNTIME, 'shot.c'), os.path.join(RUNTIME, 'sys.c')])
    run([exe, shots])
    for name in ('noise', 'runs', 'far', 'pattern', 'one'):
        w, h, rgb = read_png(os.path.join(shots, name + '.png'))
        pix = open(os.path.join(shots, name + '.bin'), 'rb').read()
        pal = open(os.path.join(shots, name + '.pal'), 'rb').read()
        if len(pix) != w * h or rgb != b''.join(pal[3 * i:3 * i + 3] for i in pix):
            raise SystemExit(f'selftest FAILED: shot.c\'s {name}.png')
        print(f'shot.c {name}.png: {w}x{h}, {os.path.getsize(os.path.join(shots, name + ".png"))}'
              f' bytes for {w * h} pixels')
    exe = os.path.join(b, 'vgamodes')
    run([CC] + CFLAGS + ['-I', RUNTIME, '-o', exe, os.path.join(HERE, 'vgamode', 'runtime.c'),
                         os.path.join(RUNTIME, 'vga.c'), '-lm'])
    out = run([exe])
    if 'vga modes ok' not in out:
        raise SystemExit('selftest FAILED: vga.c\'s 16-colour 200-line modes: ' + out)
    print(out.strip())
    check_update(b)
    check_gogfind(b)
    print(run([py, os.path.join(TOOLS, 'memcmp.py'), 'src/HELLO.hints',
               os.path.join(b, 'orig.ram'), os.path.join(b, 'port.ram'), '--skip', 'STACK',
               '--vram', os.path.join(b, 'orig.vram'), os.path.join(b, 'port.vram')]))
    check_pmem(py, b)

    step('5. every runtime module compiles')
    mods = [f for f in sorted(os.listdir(RUNTIME)) if f.endswith('.c')
            and f not in ('plat_sdl.c', 'plat_win32.c')]
    extra = []
    sdl = subprocess.run(['sh', os.path.join(RUNTIME, 'sdl2-flags.sh')], capture_output=True, text=True)
    if sdl.returncode == 0:
        mods.append('plat_sdl.c')
        # the compiler's options only: -I, -D and -F
        extra = [o for o in sdl.stdout.split() if o.startswith(('-I', '-D', '-F'))]
    objdir = os.path.join(b, 'obj')
    os.makedirs(objdir, exist_ok=True)
    for m in mods:
        run([CC] + CFLAGS + extra + ['-I', RUNTIME, '-c', os.path.join(RUNTIME, m),
                                     '-o', os.path.join(objdir, m[:-2] + '.o')])
    print(f'{len(mods)} modules: {" ".join(mods)}' +
          ('' if 'plat_sdl.c' in mods else ' (SDL2 not found: plat_sdl.c not compiled)'))

    step('6. a project from the template (new_project.py)')
    new = os.path.join(KIT, 'build', 'selftest-new')
    if os.path.isdir(new):
        shutil.rmtree(new)
    run([py, os.path.join(TOOLS, 'new_project.py'), new, 'Test Game', 'testgame',
         '--marker', 'HELLO/HELLO.EXE', '--gog-id', '1234567890', '--no-submodule'])
    if '"1234567890",' not in open(os.path.join(new, 'port', 'src', 'main.c')).read():
        raise SystemExit('selftest FAILED: --gog-id not filled into port/src/main.c')
    os.symlink(KIT, os.path.join(new, 'doskit'))
    shutil.copytree(os.path.join(PROJ, 'game'), os.path.join(new, 'game'))
    r = subprocess.run(['sh', os.path.join(new, 'port', 'build.sh')], capture_output=True, text=True)
    if r.returncode or 'warning' in r.stderr:
        print(r.stdout + r.stderr)
        raise SystemExit('selftest FAILED: the template\'s port/build.sh')
    # a data folder of its own: never the user's
    first = os.path.join(KIT, 'build', 'selftest-data')
    if os.path.isdir(first):
        shutil.rmtree(first)
    os.makedirs(first)
    r = subprocess.run([os.path.join(new, 'port', 'build', 'testgame-headless')], cwd=new,
                       capture_output=True, text=True,
                       env=dict(os.environ, DK_FRAMES='3', DK_DATA_DIR=first))
    if r.returncode:
        print(r.stdout + r.stderr)
        raise SystemExit('selftest FAILED: the template\'s port did not find the game')
    # named, as the folder above (the kit inside a project) may be a project too
    out = subprocess.run([py, os.path.join(TOOLS, 'check.py')], cwd=new, capture_output=True,
                         text=True, env=dict(os.environ, DOSKIT_PROJECT=new))
    print(out.stdout.strip())
    if out.returncode:
        raise SystemExit('selftest FAILED: check.py in the new project')

    gog = os.path.join(KIT, 'build', 'selftest-gog')
    if os.path.isdir(gog):
        shutil.rmtree(gog)
    installed = os.path.join(gog, 'Hello Game')
    os.makedirs(installed)
    with open(os.path.join(installed, 'goggame-1234567890.info'), 'w') as f:
        f.write('{"gameId": "1234567890", "rootGameId": "1234567890", "name": "Hello Game"}')
    with open(os.path.join(installed, 'goggame-1234567891.info'), 'w') as f:
        f.write('{"gameId": "1234567891", "rootGameId": "1234567890", "name": "An add-on"}')
    hello = open(os.path.join(PROJ, 'game', 'HELLO', 'HELLO.EXE'), 'rb').read()
    # not game.gog: an image by another name, in a folder, its cue sheet naming it
    os.makedirs(os.path.join(installed, 'CD'))
    cd_image({'HELLO/HELLO.EXE': hello, 'HELLO/README.TXT': b'hello\r\n' * 500},
             os.path.join(installed, 'CD', 'HELLO.DAT'))
    with open(os.path.join(installed, 'CD', 'HELLO.CUE'), 'w') as f:
        f.write('FILE "HELLO.DAT" BINARY\n  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\n')
    # one installed as a folder (no CD image): copied into game/ below
    folder_game = os.path.join(gog, 'Floppy Game')
    os.makedirs(os.path.join(folder_game, 'HELLO'))
    with open(os.path.join(folder_game, 'goggame-1234567892.info'), 'w') as f:
        f.write('{"gameId": "1234567892", "name": "Floppy Game"}')
    shutil.copy(os.path.join(PROJ, 'game', 'HELLO', 'HELLO.EXE'), os.path.join(folder_game, 'HELLO'))
    # one of GOG's older Mac applications: no goggame-ID.info, an Info.plist
    # without quotes, Boxer's DummyCD.iso beside the game's image (*.dat)
    old_mac = os.path.join(gog, 'Old Mac Game.app', 'Contents')
    box = os.path.join(old_mac, 'Resources', 'Old Mac Game.boxer', 'C Old.harddisk')
    os.makedirs(box)
    with open(os.path.join(old_mac, 'Info.plist'), 'w') as f:
        f.write('<?xml version=1.0 encoding=UTF-8?>\n<!DOCTYPE plist PUBLIC -//Apple//DTD PLIST '
                '1.0//EN http://www.apple.com/DTDs/PropertyList-1.0.dtd>\n<plist version=1.0>\n'
                '<dict>\n\t<key>BXOrganizationName</key>\n\t<string>GOG.com</string>\n'
                '\t<key>CFBundleName</key>\n\t<string>Old Mac Game</string>\n</dict>\n</plist>\n')
    cd_image({'DUMMY/EMPTY.TXT': b'\r\n' * 100000}, os.path.join(old_mac, 'Resources', 'DummyCD.iso'))
    cd_image({'HELLO/HELLO.EXE': hello}, os.path.join(box, 'OLD.DAT'))
    r = subprocess.run([py, os.path.join(TOOLS, 'goglist.py')], capture_output=True, text=True,
                       env=dict(os.environ, DOSKIT_GOG_DIRS=gog))
    if r.returncode or not re.search(r'\(no ID\)  Old Mac Game\n.*\n.*C Old\.harddisk.OLD\.DAT\n',
                                     r.stdout):
        print(r.stdout + r.stderr)
        raise SystemExit('selftest FAILED: goglist.py and an older GOG Mac application')
    print('ok   an older GOG Mac application found, its *.dat the image, not DummyCD.iso')
    chosen =os.path.join(KIT, 'build', 'selftest-chosen')
    if os.path.isdir(chosen):
        shutil.rmtree(chosen)
    # the game (the add-on left out), the slug, the CD's first program, unpacked
    r = subprocess.run([py, os.path.join(TOOLS, 'new_project.py'), chosen, '--no-submodule'],
                       input='2\nhello\n1\ny\n', capture_output=True, text=True,
                       env=dict(os.environ, DOSKIT_GOG_DIRS=gog))
    print(r.stdout.strip().splitlines()[1])
    if r.returncode or 'An add-on' in r.stdout:
        print(r.stdout + r.stderr)
        raise SystemExit('selftest FAILED: new_project.py choosing an installed GOG game')
    main_c = open(os.path.join(chosen, 'port', 'src', 'main.c')).read()
    for want in ('"Hello Game",', '"1234567890",', '"HELLO/HELLO.EXE",', '"CD/HELLO.DAT",',
                 '"",'):
        if want not in main_c:
            raise SystemExit(f'selftest FAILED: {want} not in the chosen project\'s main.c')
    got = os.path.join(chosen, 'game', 'HELLO', 'HELLO.EXE')
    if not os.path.isfile(got) or open(got, 'rb').read() != hello:
        raise SystemExit('selftest FAILED: the chosen game\'s image not unpacked into game/')
    print('ok   chosen from the installed GOG games: main.c filled in, game/ unpacked')

    # the one installed as a folder (no CD image): the folder copied into game/
    copied = os.path.join(KIT, 'build', 'selftest-copied')
    if os.path.isdir(copied):
        shutil.rmtree(copied)
    r = subprocess.run([py, os.path.join(TOOLS, 'new_project.py'), copied, '--no-submodule'],
                       input='1\nfloppy\n1\ny\n', capture_output=True, text=True,
                       env=dict(os.environ, DOSKIT_GOG_DIRS=gog))
    main_c = open(os.path.join(copied, 'port', 'src', 'main.c')).read() if not r.returncode else ''
    got = os.path.join(copied, 'game', 'HELLO', 'HELLO.EXE')
    if (r.returncode or '"HELLO/HELLO.EXE",' not in main_c or '"1234567892",' not in main_c
            or not os.path.isfile(got) or open(got, 'rb').read() != hello):
        print(r.stdout + r.stderr)
        raise SystemExit('selftest FAILED: a game installed as a folder not copied into game/')
    print('ok   one installed as a folder: copied into game/, its program the marker')

    # the template's port on a player's first start: -gog with the folder,
    # with the image, with a folder not the game's, into a data folder of its own
    exe = os.path.join(new, 'port', 'build', 'testgame-headless')
    for what, src, ok in (('folder', folder_game, True),
                          ('image', os.path.join(installed, 'CD', 'HELLO.DAT'), True),
                          ('other folder', os.path.join(gog, 'Hello Game', 'CD'), False)):
        data = os.path.join(KIT, 'build', 'selftest-data')
        if os.path.isdir(data):
            shutil.rmtree(data)
        os.makedirs(data)
        r = subprocess.run([exe, '-gog', src], cwd=data, capture_output=True, text=True,
                           env=dict(os.environ, DK_FRAMES='3', DK_DATA_DIR=data))
        got = os.path.join(data, 'game', 'HELLO', 'HELLO.EXE')
        there = os.path.isfile(got) and open(got, 'rb').read() == hello
        if (r.returncode == 0) != ok or there != ok or os.path.isdir(os.path.join(data, 'game.part')):
            print(r.stdout + r.stderr)
            raise SystemExit(f'selftest FAILED: the template\'s port with -gog (the {what})')
    print('ok   the template\'s port: the installed folder copied, the image unpacked, '
          'another folder refused')

    # game.gog put into the data folder (where a Mac app's README says),
    # found without -gog; run in an empty folder, so that neither a game
    # folder nor game.gog is found in the current directory
    data = os.path.join(KIT, 'build', 'selftest-data')
    empty = os.path.join(KIT, 'build', 'selftest-empty')
    for d in (data, empty):
        if os.path.isdir(d):
            shutil.rmtree(d)
        os.makedirs(d)
    shutil.copy(os.path.join(installed, 'CD', 'HELLO.DAT'), os.path.join(data, 'game.gog'))
    r = subprocess.run([exe], cwd=empty, capture_output=True, text=True,
                       env=dict(os.environ, DK_FRAMES='3', DK_DATA_DIR=data))
    got = os.path.join(data, 'game', 'HELLO', 'HELLO.EXE')
    if r.returncode or not os.path.isfile(got) or open(got, 'rb').read() != hello:
        print(r.stdout + r.stderr)
        raise SystemExit('selftest FAILED: game.gog in the data folder not found')
    print('ok   game.gog in the data folder found and unpacked')

    # the macOS release's app around the template's program
    import macapp, plistlib
    apps = os.path.join(KIT, 'build', 'selftest-app')
    if os.path.isdir(apps):
        shutil.rmtree(apps)
    os.makedirs(apps)
    r = subprocess.run([py, os.path.join(TOOLS, 'macapp.py'), exe, apps, '--name', 'testgame',
                        '--id', 'io.github.test.testgame', '--version', 'v1.2'],
                       capture_output=True, text=True)
    app = os.path.join(apps, 'testgame.app')
    info = os.path.join(app, 'Contents', 'Info.plist')
    if r.returncode or not os.path.isfile(info):
        print(r.stdout + r.stderr)
        raise SystemExit('selftest FAILED: macapp.py')
    with open(info, 'rb') as f:
        plist = plistlib.load(f)
    if (plist != macapp.info('testgame', 'io.github.test.testgame', '1.2')
            or not os.access(os.path.join(app, 'Contents', 'MacOS', 'testgame'), os.X_OK)):
        raise SystemExit('selftest FAILED: macapp.py\'s bundle')
    if sys.platform == 'darwin':
        r = subprocess.run(['codesign', '--verify', '--deep', '--strict', app],
                           capture_output=True, text=True)
        if r.returncode:
            print(r.stdout + r.stderr)
            raise SystemExit('selftest FAILED: macapp.py\'s bundle not validly signed')
    print('ok   macapp.py: the bundle, its Info.plist' +
          (', its signature' if sys.platform == 'darwin' else ''))

    for agent in ('doskit-collector', 'git-committer'):
        p = os.path.join(new, '.claude', 'agents', f'{agent}.md')
        if not os.path.isfile(p):
            raise SystemExit(f'selftest FAILED: the template did not carry {p}')
        if agent not in open(os.path.join(new, 'AGENTS.md')).read():
            raise SystemExit(f'selftest FAILED: AGENTS.md does not mention {agent}')

    print('selftest ok')


if __name__ == '__main__':
    main()
