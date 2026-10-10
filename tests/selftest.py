#!/usr/bin/env python3
"""The kit's own check: the whole way from a program to its port, on a
program of our own (tests/hello/HELLO.ASM), so no game is needed.

    python3 tests/selftest.py

In build/selftest (a project as a game's would be, see kit.py):

  1. HELLO.ASM assembled and linked with tasm.py and tlink.py into
     game/HELLO/HELLO.EXE (the "shipped program"); tests/flat/FLAT.ASM,
     32-bit, the same way into a pMAX image, build/files/FLAT.386 (as a
     project's tool would unpack it) and an LE image with page maps and
     selector fixups, build/files/LE.EXE; tests/raw/RAWDRV.ASM into a raw
     32-bit image, build/files/RAWDRV.DRV, tests/relmod/RELMOD.ASM into a
     relocatable one, build/files/RELMOD.MOD with its offrel list
     RELMOD.REL (the linker's OFF32 fixups); tests/pmode/PMODE.ASM into
     game/PMODE/PMODE.EXE, tests/cdrom/CDROM.ASM into game/CDROM/CDROM.EXE,
     tests/vgamode/VGAMODE.ASM into game/VGAMODE/VGAMODE.EXE,
     tests/gameport/GAMEPORT.ASM into game/GAMEPORT/GAMEPORT.EXE,
     tests/joy/JOY.ASM into game/JOY/JOY.EXE,
     tests/mouse/MOUSE.ASM into game/MOUSE/MOUSE.EXE,
     tests/rwatch/RWATCH.ASM into game/RWATCH/RWATCH.EXE,
     tests/strreg/STRREG.ASM into game/STRREG/STRREG.EXE,
     tests/adlib/ADLIB.ASM into game/ADLIB/ADLIB.EXE,
     tests/sb16/SB16.ASM into game/SB16/SB16.EXE, tests/cdplay/CDPLAY.ASM
     into game/CDPLAY/CDPLAY.EXE, tests/country/COUNTRY.ASM into
     game/COUNTRY/COUNTRY.EXE, tests/lol/LOL.ASM into game/LOL/LOL.EXE,
     tests/multiseg/MULTISEG.ASM into
     game/MULTISEG/MULTISEG.EXE with TLINK's header and its relocations
     in reverse order;
  2. check.py: disasm.py makes their sources from tests/hello/src/HELLO.hints
     and tests/flat/src/FLAT.hints, tests/raw/RAWDRV.ASM's raw image from
     tests/raw/src/RAWDRV.hints, RELMOD's from tests/relmod/src/RELMOD.hints
     (its offsets exactly the offrel list's, the numbers that look like
     offsets left numbers), build.py rebuilds them byte for byte; RELMOD
     with a dword of a words table taken out of its offrel list fails
     build.py's check; MULTISEG (code segments beginning in the middle of
     a paragraph, labels at the same offsets in two of them) from
     tests/multiseg/src/MULTISEG.hints (start=, prefix=, linker tlink,
     relocorder original; a segment stored before it is defined; Borland
     C's encodings with the asm switches imm8_alu and xchg_ax_short; a
     displacement written in full though small, taken for an address; a
     FAR PTR call within its segment; routines in FOUR reached only
     through a ptr and a words hint), with no instruction as DB, and
     without its prefix= hints build.py stops and says the labels collide;
     HELLO with a name at two addresses is refused (disasm.py's check
     for a name given twice);
     xfer.py carries MULTISEG's hints to MULTIS2 (its source with FOUR one
     byte further on, ONE and FOUR renamed), which rebuilds from them;
     MULTISEG with 512 more zero bytes in its header rebuilds with
     `linker tlink 30 header=original` and not without;
     PROVENANCE.md (the template's) is there; the names headers symmap.py
     wrote (HELLO's, FLAT's) are up to date; FLAT without its raw hint rebuilds too (the
     line tasm.py refuses written as DB by build.py);
  3. run.py: its table of the runner's options has every option the
     runner parses (one it lacks is taken for PROGRAM, and the addresses
     after it go untranslated); it finds MULTISEG's labels by their
     prefixes; the program in the runner, stopped at
     its end (CODE:0026), its console line read, memory and video memory written out; its
     -cover file, with which gaps.py --cover marks the routines of HELLO's
     table as run once its words hint is taken out; PMODE.EXE
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
     "vgamode ok"; GAMEPORT.EXE, which reads the game port as a PC
     without a joystick has it (FFh, the axis bits never falling after
     the one-shots are started) and says "gameport ok"; JOY.EXE, which
     with a scripted stick (-joy) finds its buttons, its two one-shots
     running as long as the script says and no second stick, and says
     "joy ok"; MOUSE.EXE, which
     checks the INT 33h driver's ranges, position, sensitivity, saved state,
     movement and button callbacks from three scripted events, and its cursor
     in a PNG; RWATCH.EXE, whose
     table of 200 bytes -rwatch reports with all its 300 readers (an
     instruction and the byte it read; more than a fixed table of 64
     kept); STRREG.EXE, which checks the registers REP STOS and REP MOVS
     leave (SI untouched by STOS, on the runner's block path of 16 and
     more elements, in its loop and backwards) and says "strreg ok";
     ADLIB.EXE, which
     probes the runner's OPL2 as drivers do (the timers' flags in the
     status, masked, cleared, not set before their time) and says "adlib
     ok"; SB16.EXE, which checks the runner's
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
     its colour, 70 Hz; the VESA modes 101h and 103h: size, rate, a
     planar pixel of a narrowed line in the mode's middle) says "vga
     modes ok"; tests/launcher/launchtest.c (launcher.c: a choice,
     a key, the pages, an action by keys; the settings file written and
     read; frame.c's keymap) says "launcher ok";
     tests/launcher/dialogtest.c (launcher.c's dialog about the game's
     files: the copy taken, declined, the two messages, the bar) says
     "dialog ok"; tests/hud/hudtest.c
     (hud.c: the box, a letter, the bar, the size at 800x600, the
     pictures shown) says "hud ok"; tests/opl/opltest.c (opl.c: a
     note's frequency and level with the rhythm mode off and on, the
     tom-tom a sine, the cymbal a mixture of high frequencies at its
     envelope's level, the hi-hat noise, the snare tone and noise, each
     silent after its key) says "opl ok"; tests/oplq/oplqtest.c (oplq.c
     on plat_null.c's virtual audio device, DK_WAV: a short tune's
     register writes stamped with their samples by a program that makes
     its timer's interrupts by the samples played; the frames those of
     the virtual clock's time, the samples those of a chip written
     exactly at the stamps, silence up to a note's sample; a stamp that
     goes back, a late one, a full queue) says "oplq ok", and the WAVE
     file holds those frames, the first note at its sample;
     tests/platmouse/platmousetest.c
     (platform.h's mouse as a device on plat_null.c with DK_MOUSEMOVE:
     the movement summed and given once, the buttons held, a grab) says
     "platmouse ok"; tests/cdaudio/cdatest.c
     (cdaudio.c on the cue sheet of step 3: the table, the WAVE's samples,
     the Ogg's tones by loudness, the channels, the clock) says "cdaudio
     ok"; tests/update/updatetest.c
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
     run headless on HELLO's files; its check.py says all ok; its
     port/dist/uninstall.sh (on Windows uninstall.cmd) run on a made-up
     data folder: nothing removed unasked, --yes, --yes --all, and (not
     on Windows) the answers typed on a terminal.  Then one
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
     found; GOG's Windows installer (tests/inno/mkinno.py's, holding
     HELLO.EXE) unpacked with -gog, and found in ~/Downloads without it; the template's program made into an app by macapp.py (its
     Info.plist read back; on a Mac the bundle's signature verified).
  7. inno.py: Inno Setup installers as GOG's Windows ones are, made by
     tests/inno/mkinno.py (the data in the .exe, and in two .bin slices
     with a chunk across them): the files listed and unpacked as they
     were put in (an LZMA chunk with two files, one through the CALL/JMP
     filter; GOG Galaxy's deflated parts, English and German, their
     dependency left out); a byte changed in the data is caught;
     goglist.py offers such an installer, new_project.py --setup makes a
     project of it (GOG's ID from it, its files in game/); the runtime's
     inno.c (tests/inno/innotest.c) unpacks the same setups alike,
     refuses one without the marker before writing anything, takes the
     CD image (Mode 2) out of one that holds an image beside a partial
     installation with the marker and unpacks the image, and
     catches a changed byte, leaving nothing behind.

Prints `selftest ok` at the end, exit status 0 then.  Needs a C compiler:
cc (clang or gcc; $CC names another), on Windows MSVC (cl.exe on PATH,
or found through vcvars64.bat as tools/run/build.bat finds it) unless
$CC is set.  What differs on Windows: the template's port is built by
its build.bat, plat_win32.c is compiled in place of plat_sdl.c (step 5),
update.c copies the file of a file:// address itself (curl elsewhere),
and gog_find is checked with GOG Galaxy's folder under a
%ProgramFiles(x86)% made here (the registry's key and X:\\GOG Games are
the machine's own; the Linux and Mac layouts are checked there).
"""
import lzma, os, re, shutil, struct, subprocess, sys, zlib

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
# MSVC, taken on Windows when $CC is not set: the warnings the template's
# build.bat asks for (/W4), as errors, and the libraries the runtime's
# Windows parts need
MSVC = os.name == 'nt' and 'CC' not in os.environ
MSVC_FLAGS = ['/nologo', '/O1', '/W4', '/WX', '/D_CRT_SECURE_NO_WARNINGS']
MSVC_LIBS = ['advapi32.lib', 'shell32.lib', 'winhttp.lib']
EXE = '.exe' if os.name == 'nt' else ''
_msvc_env = None


def msvc_env():
    """the environment with cl.exe on PATH: this one when it is there, else
    what vcvars64.bat sets (the VS2019 Build Tools first, then whatever
    vswhere finds, as tools/run/build.bat looks for it)"""
    global _msvc_env
    if _msvc_env is not None:
        return _msvc_env
    if shutil.which('cl'):
        _msvc_env = dict(os.environ)
        return _msvc_env
    pf = os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')
    bats = [os.path.join(pf, 'Microsoft Visual Studio', '2019', 'BuildTools', 'VC', 'Auxiliary',
                         'Build', 'vcvars64.bat')]
    vswhere = os.path.join(pf, 'Microsoft Visual Studio', 'Installer', 'vswhere.exe')
    if os.path.isfile(vswhere):
        r = subprocess.run([vswhere, '-latest', '-products', '*', '-requires',
                            'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
                            '-property', 'installationPath'], capture_output=True, text=True)
        bats += [os.path.join(p, 'VC', 'Auxiliary', 'Build', 'vcvars64.bat')
                 for p in r.stdout.splitlines() if p.strip()]
    for bat in bats:
        if not os.path.isfile(bat):
            continue
        r = subprocess.run(f'"{bat}" >nul 2>&1 && set', shell=True, capture_output=True, text=True)
        env = dict(l.split('=', 1) for l in r.stdout.splitlines() if '=' in l)
        if r.returncode == 0 and shutil.which('cl', path=env.get('PATH', env.get('Path', ''))):
            _msvc_env = env
            return env
    raise SystemExit('selftest FAILED: no C compiler (cl.exe not on PATH, no vcvars64.bat found; '
                     'or set CC to a gcc or clang)')


def cc(out, srcs, incs=(), defs=(), obj=False):
    """compiles srcs into the program `out` (or, obj, the one source into
    the object file `out`) with every warning an error; the program's path
    comes back (with .exe on Windows)"""
    incs = [RUNTIME] + list(incs)
    if not MSVC:
        cmd = [CC] + CFLAGS + list(defs)
        for i in incs:
            cmd += ['-I', i]
        if obj:
            run(cmd + ['-c'] + srcs + ['-o', out])
            return out
        run(cmd + ['-o', out + EXE] + srcs + ['-lm'])
        return out + EXE
    env = dict(msvc_env(), DOSKIT_PROJECT=PROJ)
    # by its path: the program is looked for on this process's PATH, not env's
    cl = shutil.which('cl', path=env.get('PATH', env.get('Path', '')))
    cmd = [cl] + MSVC_FLAGS + ['/D' + d[2:] for d in defs if d.startswith('-D')]
    cmd += ['/I' + i for i in incs if i] + ['/I' + d[2:] for d in defs if d.startswith('-I')]
    if obj:
        cmd += ['/c', '/Fo' + out] + srcs
    else:
        objdir = out + '.obj'
        os.makedirs(objdir, exist_ok=True)
        cmd += ['/Fo' + objdir + os.sep, '/Fe' + out + EXE] + srcs + MSVC_LIBS
    r = subprocess.run(cmd, cwd=PROJ, capture_output=True, text=True, env=env)
    if r.returncode:
        print((r.stdout + r.stderr).strip())
        raise SystemExit(f'selftest FAILED: cl {os.path.basename(out)}')
    return out if obj else out + EXE


def on_terminal(cmd, env, answers):
    """runs cmd with a terminal of its own (not on Windows) and types an
    answer and Enter each time it asks "[y/N] "; its exit status and all
    it said come back"""
    import pty
    import select
    pid, fd = pty.fork()
    if pid == 0:
        os.execvpe(cmd[0], cmd, env)
    said, sent = b'', 0
    while True:
        if not select.select([fd], [], [], 30)[0]:
            os.kill(pid, 9)
            break
        try:
            more = os.read(fd, 4096)
        except OSError:                         # Linux: the other end is closed
            break
        if not more:
            break
        said += more
        if said.count(b'[y/N] ') > sent and sent < len(answers):
            os.write(fd, answers[sent].encode() + b'\n')
            sent += 1
    status = os.waitpid(pid, 0)[1]
    return os.WEXITSTATUS(status) if os.WIFEXITED(status) else -1, said.decode(errors='replace')


def check_uninstall(new):
    """the template's uninstall script as the new project `new` got it
    (uninstall.sh; on Windows uninstall.cmd), in a made-up package folder
    with a made-up data folder (DK_DATA_DIR, never the user's): asked
    without a terminal it removes nothing; --yes removes the copied game
    files there and those left beside the program and keeps the saves;
    --yes --all removes the data folder; an option it does not know
    removes nothing; the program's folder always stays.  With a terminal
    (not on Windows) the answers n and n, Enter and Enter keep
    everything, y and n remove the game files only, y and y and n and
    yes the folder."""
    name = 'uninstall.cmd' if os.name == 'nt' else 'uninstall.sh'
    src = os.path.join(new, 'port', 'dist', name)
    text = open(src, 'rb').read()
    if b'{{' in text or b'testgame' not in text or b'Test Game' not in text:
        raise SystemExit(f'selftest FAILED: {name}: the template\'s names not filled in')
    if os.name != 'nt' and not os.access(src, os.X_OK):
        raise SystemExit(f'selftest FAILED: {name} cannot be executed')
    root = os.path.join(KIT, 'build', 'selftest-uninstall')
    pkg, data = os.path.join(root, 'the package'), os.path.join(root, 'data folder')
    script = os.path.join(pkg, name)
    cmd = (['cmd', '/c', script] if os.name == 'nt' else ['sh', script])
    env = dict(os.environ, DK_DATA_DIR=data)

    def fresh(leftover=False):
        if os.path.isdir(root):
            shutil.rmtree(root)
        for d in [pkg, os.path.join(data, 'game', 'SUB'), os.path.join(data, 'save')] + (
                [os.path.join(pkg, 'game')] if leftover else []):
            os.makedirs(d)
            with open(os.path.join(d, 'FILE.DAT'), 'w') as f:
                f.write('x')
        shutil.copy(src, script)

    def there():
        return ''.join(c for c, p in (('g', os.path.join(data, 'game', 'SUB', 'FILE.DAT')),
                                      ('s', os.path.join(data, 'save', 'FILE.DAT')),
                                      ('l', os.path.join(pkg, 'game', 'FILE.DAT')),
                                      ('p', script)) if os.path.isfile(p))

    def unasked(args):
        # bytes, read here: what Windows' own commands say (choice, pause) is in
        # the console's code page, not the one Python would decode with
        r = subprocess.run(cmd + args, capture_output=True, env=env, stdin=subprocess.DEVNULL)
        return r.returncode, (r.stdout + r.stderr).decode('ascii', errors='replace')

    def expect(what, args, left, code=0, says=None, answers=None, leftover=False):
        fresh(leftover)
        if answers is None:
            got, said = unasked(args)
        else:
            got, said = on_terminal(cmd + args, env, answers)
        if got != code or there() != left or (says and says not in said):
            print(said)
            raise SystemExit(f'selftest FAILED: {name} {what}: exit {got} for {code}, '
                             f'left {there()} for {left}')

    expect('without a terminal', [], 'gslp', says='kept', leftover=True)
    expect('--yes', ['--yes'], 'sp', says='removed', leftover=True)
    expect('--yes --all', ['--yes', '--all'], 'p', says='removed')
    expect('--all alone', ['--all'], 'gsp', code=2)
    expect('an unknown option', ['--everything'], 'gsp', code=2)
    fresh()
    shutil.rmtree(data)
    got, said = unasked(['--yes', '--all'])
    if got or 'Nothing removed' not in said or there() != 'p':
        print(said)
        raise SystemExit(f'selftest FAILED: {name} with nothing to remove')
    asked = 'not on a terminal (Windows)'
    if os.name != 'nt':
        expect('n, n', [], 'gsp', answers=['n', 'n'], says='Nothing removed')
        expect('Enter, Enter', [], 'gsp', answers=['', ''], says='Nothing removed')
        expect('y, n', [], 'sp', answers=['y', 'n'])
        expect('y, y', [], 'p', answers=['y', 'y'])
        expect('n, yes', [], 'p', answers=['n', 'yes'])
        expect('x, maybe', [], 'gsp', answers=['x', 'maybe'], says='Nothing removed')
        asked = 'asked on a terminal: n n, Enter Enter, y n, y y, n yes, x maybe'
    print(f'ok   {name}: nothing without a terminal, --yes, --yes --all, unknown options, '
          f'nothing there; {asked}')


def link_dir(target, link):
    """`link` a folder that is `target`: a symbolic link, on Windows a
    junction (which needs no privilege)"""
    if os.name == 'nt':
        import _winapi
        _winapi.CreateJunction(target, link)
    else:
        os.symlink(target, link)


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
    """Width, height and RGB bytes of an 8-bit indexed or RGB PNG made by
    the kit (filter 0 on every row)."""
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
            if depth != 8 or color not in (2, 3):
                raise SystemExit(f'selftest FAILED: {path} is not 8-bit indexed or RGB')
        elif kind == b'PLTE':
            plte = body
        elif kind == b'IDAT':
            idat += body
        pos += 12 + n
    raw = zlib.decompress(idat)
    rgb = bytearray()
    rowbytes = w * (3 if color == 2 else 1)
    for y in range(h):
        row = raw[y * (rowbytes + 1):(y + 1) * (rowbytes + 1)]
        if row[0] != 0:
            raise SystemExit(f'selftest FAILED: {path}: filter {row[0]}')
        if color == 2:
            rgb += row[1:]
        else:
            for i in row[1:]:
                rgb += plte[3 * i:3 * i + 3]
    return w, h, bytes(rgb)


def check_oplq(b):
    """tests/oplq/oplqtest.c on plat_null.c's virtual audio device, and the
    WAVE file DK_WAV asked for: 44100 Hz stereo, the frames the test
    counted, both sides the same, silent up to the first note's sample."""
    import struct
    exe = cc(os.path.join(b, 'oplqtest'), [os.path.join(HERE, 'oplq', 'oplqtest.c')] + [
        os.path.join(RUNTIME, f) for f in ('oplq.c', 'opl.c', 'plat_null.c', 'shot.c', 'sys.c')])
    path = os.path.join(b, 'oplq.wav')
    out = subprocess.run([exe], cwd=PROJ, capture_output=True, text=True,
                         env=dict(os.environ, DK_WAV=path, DK_AUDIO=''))
    m = re.search(r'oplq ok: (\d+) frames, \d+ writes, the first note at (\d+),', out.stdout)
    if out.returncode or not m:
        print(out.stdout + out.stderr)
        raise SystemExit('selftest FAILED: oplq.c on the virtual audio device')
    frames, first = int(m.group(1)), int(m.group(2))
    with open(path, 'rb') as f:
        data = f.read()
    chans, rate = struct.unpack('<HI', data[22:28])
    size = struct.unpack('<I', data[40:44])[0]
    s = struct.unpack('<%dh' % ((len(data) - 44) // 2), data[44:])
    left, right = s[0::2], s[1::2]
    loud = [i for i, x in enumerate(left) if x]
    if (rate, chans, size, len(left)) != (44100, 2, frames * 4, frames) or left != right \
            or not loud or not first <= loud[0] < first + 32:
        raise SystemExit(f'selftest FAILED: DK_WAV\'s file ({rate} Hz, {chans} channels, '
                         f'{len(left)} frames for {frames}, the first sound at '
                         f'{loud[0] if loud else None} for {first})')
    # without DK_AUDIO and DK_WAV there is no device, as before
    out2 = subprocess.run([exe], cwd=PROJ, capture_output=True, text=True,
                          env=dict(os.environ, DK_WAV='', DK_AUDIO=''))
    if 'no virtual audio device' not in out2.stdout:
        print(out2.stdout + out2.stderr)
        raise SystemExit('selftest FAILED: plat_null.c has an audio device without DK_AUDIO')
    print(out.stdout.strip())
    print(f'oplq.wav: {frames} frames at {rate} Hz, the first sound at sample {loud[0]}')


def check_adlib_wav(path):
    """ADLIB.EXE's note in the runner's -oplwav: 49716 Hz mono; silence,
    then 491.52 ms (six overflows of timer 2) and the release (release
    rate 15, under 2 ms) of a 440 Hz sine at an operator's full scale
    (4096), then silence for most of two more overflows."""
    import struct
    with open(path, 'rb') as f:
        data = f.read()
    rate, chans = struct.unpack('<I', data[24:28])[0], struct.unpack('<H', data[22:24])[0]
    s = struct.unpack('<%dh' % ((len(data) - 44) // 2), data[44:])
    loud = [i for i, x in enumerate(s) if x]
    if rate != 49716 or chans != 1 or not loud:
        raise SystemExit(f'selftest FAILED: ADLIB.EXE\'s -oplwav ({rate} Hz, {chans} channels, '
                         f'{len(loud)} samples not 0)')
    first, last = loud[0], loud[-1]
    mid = s[first + rate // 10:last - rate // 10]
    cross = sum(1 for a, b in zip(mid, mid[1:]) if (a < 0) != (b < 0))
    hz = cross / 2 / (len(mid) / rate)
    peak = max(abs(x) for x in mid)
    dur, tail = (last - first) / rate, (len(s) - last) / rate
    print(f'adlib.wav: {dur * 1000:.1f} ms at {hz:.1f} Hz, peak {peak}, then {tail * 1000:.0f} ms silent')
    if not (0.485 < dur < 0.505 and 438 < hz < 442 and 4000 <= peak <= 4096 and tail > 0.14):
        raise SystemExit('selftest FAILED: ADLIB.EXE\'s -oplwav (the note the OPL2 played)')


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


def make_le():
    """Wrap FLAT's two objects in a small, page based LE executable. The
    generated image includes internal selector fixups and an MZ pointer to
    the LE header, exercising both ordinary and DOS/4GW-style discovery."""
    import math
    a = tasm.Assembler(os.path.join(HERE, 'flat', 'FLAT.ASM'))
    a.assemble()
    out = tlink.link([tlink.module_from_asm(a, 'LE')])
    names = list(a.segorder)
    segs = [out.byname[n] for n in names]
    page_size = 4096
    objects = []
    first = 1
    for i, (name, sg) in enumerate(zip(names, segs)):
        size = sg['end'] - sg['start']
        count = math.ceil(size / page_size)
        objects.append((name, 0x10000 + i * 0x10000, size, 0x2005 if i == 0 else 0x2003,
                        first, count, sg['pieces'][0].base))
        first += count
    npages = first - 1
    fixups = [[] for _ in range(npages)]
    for address, source_frame, _ in out.relocs:
        source_obj = next(i for i, x in enumerate(objects)
                          if x[6] <= address < x[6] + x[2])
        name, _, _, _, page_first, _, base = objects[source_obj]
        local = address - base
        page = page_first - 1 + local // page_size
        target_frame = struct.unpack_from('<H', out.img, address)[0]
        target_obj = next(i for i, x in enumerate(objects) if out.byname[x[0]]['frame'] == target_frame)
        fixups[page].append(struct.pack('<BBHB', 2, 0, local % page_size, target_obj + 1))
    fixup_data, fixup_offsets = bytearray(), [0]
    for records in fixups:
        records.sort(key=lambda x: struct.unpack_from('<H', x, 2)[0])
        fixup_data.extend(b''.join(records))
        fixup_offsets.append(len(fixup_data))
    leoff = 0x40
    object_off = 0xb0
    map_off = object_off + 24 * len(objects)
    fix_page_off = map_off + 4 * npages
    fix_rec_off = fix_page_off + 4 * (npages + 1)
    data_off = (leoff + fix_rec_off + len(fixup_data) + page_size - 1) // page_size * page_size
    entry_obj = next(i for i, x in enumerate(objects) if out.byname[x[0]]['frame'] == out.cs) + 1
    header = bytearray(0xb0)
    header[:2] = b'LE'
    struct.pack_into('<IIIII', header, 0x14, npages, entry_obj, out.ip, 0, 0)
    struct.pack_into('<II', header, 0x28, page_size, page_size)
    struct.pack_into('<I', header, 0x30, len(fixup_data))
    struct.pack_into('<II', header, 0x40, object_off, len(objects))
    struct.pack_into('<I', header, 0x48, map_off)
    struct.pack_into('<II', header, 0x68, fix_page_off, fix_rec_off)
    struct.pack_into('<I', header, 0x80, data_off)
    wrapper = bytearray(leoff)
    wrapper[:2] = b'MZ'
    struct.pack_into('<H', wrapper, 8, leoff // 16)
    struct.pack_into('<I', wrapper, 0x3c, leoff)
    result = wrapper + header
    for _, base, size, flags, page_first, count, _ in objects:
        result.extend(struct.pack('<6I', size, base, flags, page_first, count, 0))
    for page in range(npages):
        result.extend((page + 1).to_bytes(3, 'big') + b'\0')
    result.extend(b''.join(struct.pack('<I', x) for x in fixup_offsets))
    result.extend(fixup_data)
    result.extend(bytes(data_off - len(result)))
    logical = 0
    for _, _, size, _, _, count, linked in objects:
        segment = out.img[linked:linked + size]
        segment = bytearray(segment)
        for page in range(count):
            pageoff = page * page_size
            take = min(page_size, size - pageoff)
            for rec in fixups[logical + page]:
                source_off = struct.unpack_from('<H', rec, 2)[0]
                target_obj = rec[4]
                # LE selector fixup slots can contain a nonzero saved word;
                # the writer must preserve it while retaining the fixup.
                struct.pack_into('<H', segment, pageoff + source_off,
                                 0xA500 | target_obj)
            result.extend(segment[pageoff:pageoff + take])
            result.extend(bytes(page_size - take))
        logical += count
    path = os.path.join(PROJ, 'build', 'files', 'LE.EXE')
    with open(path, 'wb') as f:
        f.write(result)
    fallback = bytearray(result)
    struct.pack_into('<I', fallback, 0x3c, 0xffffffff)
    fallback_path = os.path.join(PROJ, 'build', 'files', 'LE_FALLBACK.EXE')
    with open(fallback_path, 'wb') as f:
        f.write(fallback)
    from disasm import LEProgram
    if LEProgram(fallback_path).leoff != leoff:
        raise SystemExit('selftest FAILED: LE header discovery without a valid MZ pointer')
    # Bind the independent image after another MZ program. Its file-relative
    # page offset must stay relative to the inner executable, for both reads
    # and writes; table offsets remain relative to the LE header.
    prefix = bytearray(0x230)
    prefix[:2] = b'MZ'
    struct.pack_into('<H', prefix, 8, 4)
    struct.pack_into('<I', prefix, 0x3c, 0xffffffff)
    bound_path = os.path.join(PROJ, 'build', 'files', 'LE_BOUND.EXE')
    with open(bound_path, 'wb') as f:
        f.write(prefix + result)
    plain, bound = LEProgram(path), LEProgram(bound_path)
    if (bound.file_origin != len(prefix) or bound.leoff != len(prefix) + leoff or
            bound.data_off != len(prefix) + data_off or bound.img != plain.img or
            bound.selector_sites != plain.selector_sites or bound.entry_object != plain.entry_object or
            bound.ip != plain.ip):
        raise SystemExit('selftest FAILED: bound LE image origin and page mapping')
    return len(result)


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


def make_multiseg():
    """tests/multiseg/MULTISEG.ASM encoded as Borland C does (AND and OR
    with a word constant, XCHG AX,reg as 87h /r), with TLINK's header, the relocations
    in reverse order (not by address, as TLINK writes them in the order of
    its object records)"""
    a = tasm.Assembler(os.path.join(HERE, 'multiseg', 'MULTISEG.ASM'))
    a.imm8_alu = {'add', 'adc', 'sbb', 'sub', 'cmp', 'xor'}    # as Borland C encodes
    a.xchg_ax_short = False
    a.assemble()
    out = tlink.link([tlink.module_from_asm(a, 'MULTISEG')])
    exe = tlink.write_mz(out, reloc_order=list(reversed(out.relocs)), version=0x30)
    # tick's JMP FAR PTR within FOUR (frame 7, FOUR:002B): EAh with offset
    # and segment, not a near jump
    img = exe[struct.unpack_from('<H', exe, 8)[0] * 16:]
    if img[0x9B:0xA0] != bytes([0xEA, 0x26, 0, 7, 0]):
        raise SystemExit('selftest FAILED: MULTISEG\'s JMP FAR PTR in its own segment is '
                         + img[0x9B:0xA0].hex(' '))
    os.makedirs(os.path.join(PROJ, 'game', 'MULTISEG'))
    with open(os.path.join(PROJ, 'game', 'MULTISEG', 'MULTISEG.EXE'), 'wb') as f:
        f.write(exe)
    return len(exe)


xfer_mark = '; ==== carried over from '


def check_xfer(py):
    """xfer.py carries MULTISEG's hints to a sibling: its source with an
    instruction put before FOUR's first routine (all of FOUR one byte
    further on) and ONE and FOUR renamed UNO and QUAD; the hints into code
    segments other than CODE and the segment names in them come across
    mapped, and the sibling rebuilds byte for byte"""
    with open(os.path.join(HERE, 'multiseg', 'MULTISEG.ASM')) as f:
        text = f.read()
    text = text.replace('\tASSUME CS:FOUR\n', '\tASSUME CS:FOUR\n\tINC DX\n')
    text = re.sub(r'\bFOUR\b', 'QUAD', re.sub(r'\bONE\b', 'UNO', text))
    src = os.path.join(PROJ, 'build', 'MULTIS2.ASM')
    with open(src, 'w') as f:
        f.write(text)
    a = tasm.Assembler(src)
    a.imm8_alu = {'add', 'adc', 'sbb', 'sub', 'cmp', 'xor'}
    a.xchg_ax_short = False
    a.assemble()
    out = tlink.link([tlink.module_from_asm(a, 'MULTIS2')])
    exe = tlink.write_mz(out, reloc_order=list(reversed(out.relocs)), version=0x30)
    os.makedirs(os.path.join(PROJ, 'game', 'MULTIS2'))
    with open(os.path.join(PROJ, 'game', 'MULTIS2', 'MULTIS2.EXE'), 'wb') as f:
        f.write(exe)
    # the sibling's own lines: exe, segments (renamed), linker, relocorder, asm
    own = ['exe MULTIS2/MULTIS2.EXE']
    with open(os.path.join(HERE, 'multiseg', 'src', 'MULTISEG.hints')) as f:
        for line in f:
            w = line.split()
            if w and w[0] in ('segment', 'linker', 'relocorder', 'asm'):
                own.append(re.sub(r'\bFOUR\b', 'QUAD', re.sub(r'\bONE\b', 'UNO', line.rstrip())))
    dst = os.path.join(PROJ, 'src', 'MULTIS2.hints')
    with open(dst, 'w') as f:
        f.write('\n'.join(own) + '\n')
    run([py, os.path.join(TOOLS, 'xfer.py'), 'src/MULTISEG.hints', 'src/MULTIS2.hints'])
    with open(dst, 'rb') as f:
        if b'\r' in f.read():
            raise SystemExit('selftest FAILED: xfer.py wrote the hints with CR LF line ends')
    with open(dst) as f:
        carried = f.read()
    for want in ('ptr QUAD:001F QUAD', 'name QUAD:0028 tick', 'words DATA:0015 1 QUAD',
                 'name QUAD:0031 tock', 'name UNO:0007 first', 'name QUAD:0005 fourth'):
        if not re.search('^' + re.escape(want) + r'\b', carried, re.M):
            print(carried)
            raise SystemExit(f'selftest FAILED: xfer.py did not carry {want!r}')
    if 'not mapped' in carried:
        print(carried)
        raise SystemExit('selftest FAILED: xfer.py left hints of MULTISEG not mapped')
    # the comment above a carried hint comes along; the file's header and
    # the comment above its asm line (not carried) describe MULTISEG and stay
    if 'reached only through pointers' not in carried:
        raise SystemExit('selftest FAILED: xfer.py dropped the comment of a carried hint')
    for stale in ("the kit's test of a program", 'encoded as Borland C does'):
        if stale in carried.split(xfer_mark)[1]:
            raise SystemExit(f'selftest FAILED: xfer.py carried a comment about MULTISEG ({stale!r})')
    out = run([py, os.path.join(TOOLS, 'build.py'), 'src/MULTIS2.hints'])
    if 'IDENTICAL' not in out:
        print(out)
        raise SystemExit('selftest FAILED: MULTIS2 from the carried hints')
    out = run([py, os.path.join(TOOLS, 'xfer.py'), 'src/MULTISEG.hints', 'src/MULTIS2.hints',
               '--check'])
    # a name of the file's own lines is not replaced by a carried one at the
    # same address, nor given again at another (the sibling may use that data
    # otherwise)
    own = os.path.join(PROJ, 'build', 'OWNNAME.hints')
    with open(os.path.join(PROJ, 'src', 'MULTIS2.hints')) as f:
        text = f.read()
    with open(own, 'w') as f:
        f.write(text.split(xfer_mark)[0] + 'name DATA:0015 mine\n' + xfer_mark +
                text.split(xfer_mark)[1] + 'name DATA:0015 theirs\nname DATA:0016 mine\n')
    sys.path.insert(0, TOOLS)
    import disasm
    names = disasm.Hints(own).names
    if names.get(('DATA', 0x15)) != 'mine' or ('DATA', 0x16) in names:
        raise SystemExit('selftest FAILED: a carried name replaced a name of the file itself')
    # one header for both: the names are matched though a segment is named
    # otherwise in the sibling (FOUR is QUAD there), its frames in FRAMES
    run([py, os.path.join(TOOLS, 'symmap.py'), 'build/multi_names.h', 'MULTI',
         'MULTISEG=src/MULTISEG.hints', 'MULTIS2=src/MULTIS2.hints'])
    with open(os.path.join(PROJ, 'build', 'multi_names.h')) as f:
        names = f.read()
    for want in ('X(FOUR, fourth, 0x0004, 0x0005)', 'X(FOUR, fourth, MULTISEG_FOUR, MULTIS2_QUAD)',
                 'X(ONE, first, MULTISEG_ONE, MULTIS2_UNO)'):
        if want not in names:
            print(names)
            raise SystemExit(f'selftest FAILED: symmap.py did not write {want!r} for the sibling')
    return len(exe)


def check_hdrpad(py):
    """MULTISEG with 512 more zero bytes in its header (as TLINK 5.0 leaves
    at times): rebuilt identical with `linker tlink 30 header=original`,
    not without it"""
    with open(os.path.join(PROJ, 'game', 'MULTISEG', 'MULTISEG.EXE'), 'rb') as f:
        d = bytearray(f.read())
    hdr = struct.unpack_from('<H', d, 8)[0] * 16
    d[hdr:hdr] = bytes(512)
    struct.pack_into('<H', d, 8, (hdr + 512) // 16)
    struct.pack_into('<HH', d, 2, len(d) % 512, (len(d) + 511) // 512)
    os.makedirs(os.path.join(PROJ, 'game', 'MULTIPAD'))
    with open(os.path.join(PROJ, 'game', 'MULTIPAD', 'MULTIPAD.EXE'), 'wb') as f:
        f.write(d)
    with open(os.path.join(HERE, 'multiseg', 'src', 'MULTISEG.hints')) as f:
        text = f.read().replace('exe MULTISEG/MULTISEG.EXE', 'exe MULTIPAD/MULTIPAD.EXE')
    for opt, want in ((' header=original', 'IDENTICAL'), ('', 'differs')):
        hints = os.path.join(PROJ, 'build', 'MULTIPAD.hints')
        with open(hints, 'w') as f:
            f.write(text.replace('linker tlink 30', 'linker tlink 30' + opt))
        out = run([py, os.path.join(TOOLS, 'build.py'), hints], check=False)
        if want not in out:
            print(out)
            raise SystemExit(f'selftest FAILED: MULTIPAD with "linker tlink 30{opt}" not {want}')
    return len(d)


def check_update(b):
    """tests/update/updatetest.c in a data folder of its own: a first start
    fetching a latest.json made here, then one the same day using the kept
    file (a fetch would fail: the address is nowhere)"""
    # curl does the fetch but on Windows (WinHTTP, and the file copied there)
    if os.name != 'nt' and not shutil.which('curl'):
        print('curl not found: update.c\'s fetch not checked')
        return
    d = os.path.join(b, 'update')
    if os.path.isdir(d):
        shutil.rmtree(d)
    os.makedirs(os.path.join(d, 'data'))
    exe = cc(os.path.join(d, 'updatetest'), [os.path.join(HERE, 'update', 'updatetest.c'),
                                             os.path.join(RUNTIME, 'update.c'),
                                             os.path.join(RUNTIME, 'sha256.c'),
                                             os.path.join(RUNTIME, 'sys.c')])
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
    exe = cc(os.path.join(d, 'gogfind'), [os.path.join(HERE, 'gogfind', 'gogfind.c'),
                                          os.path.join(RUNTIME, 'cdimage.c'),
                                          os.path.join(RUNTIME, 'sys.c')])
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
    if sys.platform == 'darwin':
        # Mac: only the application bundles are searched, not Linux's
        # folders, menus and Wine prefixes. The old ~/Desktop scan may
        # prompt for access (presumably; no prompt was observed) and held
        # the app before it reached its setup screen.
        linux = [(what + ', not looked at on a Mac', place, files, prefix, must, False)
                 for what, place, files, prefix, must, ok in cases if ok]
        cases = [
            ('the application in ~/Applications', 'Applications/Test Game.app/game.gog', {}, '',
             '', True),
            ('the application, with must_have', 'Applications/Test Game.app/game.gog', {}, '',
             'HELLO/HELLO.EXE', True),
            ('another application', 'Applications/Other.app/game.gog', {}, '', '', False),
        ] + linux + [('nothing', None, {}, '', 'HELLO/HELLO.EXE', False)]
    if os.name == 'nt':
        # Windows: GOG Galaxy's folder under %ProgramFiles(x86)%, which is
        # set to the folder made here.  The registry's key of a product ID
        # and X:\GOG Games are the machine's own and not made here.
        galaxy = 'GOG Galaxy/Games/Test Game/game.gog'
        cases = [
            ('GOG Galaxy\'s folder', galaxy, {}, '', '', True),
            ('GOG Galaxy\'s folder, with must_have', galaxy, {}, '', 'HELLO/HELLO.EXE', True),
            ('an image without the must_have', galaxy, {}, '', 'NOPE/NOPE.EXE', False),
            ('another game\'s folder', 'GOG Galaxy/Games/Other/game.gog', {}, '', '', False),
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
        if os.name == 'nt':
            env['ProgramFiles(x86)'] = home
        r = subprocess.run([exe, 'Test Game'] + ([must] if must else []), cwd=d,
                           capture_output=True, text=True, env=env)
        want = os.path.normpath(os.path.join(home, place)) if ok else 'not found'
        if r.stdout.strip() != want:
            print(r.stdout + r.stderr)
            raise SystemExit(f'selftest FAILED: gog_find ({what})')
    print(f'gog_find ok ({len(cases)} installations)')


def check_goglist_linux(b):
    """goglist.py finds games installed by GOG's Linux .sh installer"""
    if not sys.platform.startswith('linux'):
        return
    d = os.path.join(b, 'goglist-linux')
    if os.path.isdir(d):
        shutil.rmtree(d)
    home = os.path.join(d, 'home')
    install = os.path.join(home, 'GOG Games', 'Test Game')
    os.makedirs(os.path.join(install, '.mojosetup'))
    with open(os.path.join(install, 'gameinfo'), 'w') as f:
        f.write('Test Game\n1.0\nn/a\n')
    os.makedirs(os.path.join(install, 'data'))
    cd_image({'HELLO/HELLO.EXE': b'MZ' + bytes(100)}, os.path.join(install, 'data', 'game.gog'))
    apps = os.path.join(home, '.local', 'share', 'applications')
    os.makedirs(apps)
    with open(os.path.join(apps, 'gog_com-Test_Game_1.desktop'), 'w') as f:
        f.write('[Desktop Entry]\nName=Test Game\nPath=' + install + '\n')
    env = dict(os.environ, HOME=home)
    env.pop('DOSKIT_GOG_DIRS', None)
    r = subprocess.run([sys.executable, os.path.join(TOOLS, 'goglist.py')], capture_output=True,
                       text=True, env=env)
    if r.returncode or 'Test Game' not in r.stdout or 'game.gog' not in r.stdout:
        print(r.stdout + r.stderr)
        raise SystemExit('selftest FAILED: goglist.py and a GOG Linux .sh installation')
    project = os.path.join(d, 'project')
    r = subprocess.run([sys.executable, os.path.join(TOOLS, 'new_project.py'), project,
                        '--no-submodule'], input='1\ntestgame\n1\ny\n', capture_output=True,
                       text=True, env=env)
    if r.returncode or not os.path.isfile(os.path.join(project, 'game', 'HELLO', 'HELLO.EXE')):
        print(r.stdout + r.stderr)
        raise SystemExit('selftest FAILED: new_project.py and a GOG Linux .sh installation')
    print('ok   GOG Linux .sh installation found and offered by new_project.py')


def check_inno(b):
    """tools/inno.py on installers tests/inno/mkinno.py makes"""
    sys.path.insert(0, os.path.join(HERE, 'inno'))
    import inno, mkinno
    d = os.path.join(b, 'inno')
    if os.path.isdir(d):
        shutil.rmtree(d)
    os.makedirs(d)
    for slices in (0, 2):
        setup = os.path.join(d, f'setup_test_{slices}.exe')
        want = mkinno.write(setup, slices)
        for lang in ('en-US', 'de-DE'):
            out = os.path.join(d, f'out_{slices}_{lang}')
            inno.Setup(setup, lang).unpack(out)
            got = {os.path.relpath(os.path.join(r, f), out).replace(os.sep, '/'):
                   open(os.path.join(r, f), 'rb').read() for r, _, fs in os.walk(out) for f in fs}
            if got != want[lang]:
                raise SystemExit(f'selftest FAILED: inno.py ({slices} slices, {lang}): '
                                 f'{sorted(k for k in set(got) | set(want[lang]) if got.get(k) != want[lang].get(k))}')
    # a byte of README.TXT's chunk changed: its checksum (or LZMA) says so
    setup = os.path.join(d, 'setup_test_0.exe')
    data = bytearray(open(setup, 'rb').read())
    data[0x440 + 40] ^= 0x55
    open(setup, 'wb').write(data)
    try:
        inno.Setup(setup).unpack(os.path.join(d, 'bad'))
    except (inno.InnoError, lzma.LZMAError):
        pass
    else:
        raise SystemExit('selftest FAILED: inno.py took a changed byte')
    # more files than may be open at once: each closed when written
    many = {f'DATA/F{i:03}.DAT': b'%d\r\n' % i for i in range(300)}
    setup = os.path.join(d, 'setup_many.exe')
    mkinno.write(setup, 0, many)
    out = os.path.join(d, 'many')
    try:
        import resource
        soft, hard = resource.getrlimit(resource.RLIMIT_NOFILE)
        resource.setrlimit(resource.RLIMIT_NOFILE, (min(100, hard), hard))
    except (ImportError, ValueError, OSError):
        resource = None                     # Windows: the unpack alone
    try:
        inno.Setup(setup).unpack(out)
    except OSError as e:
        raise SystemExit(f'selftest FAILED: inno.py on 300 files: {e}')
    finally:
        if resource:
            resource.setrlimit(resource.RLIMIT_NOFILE, (soft, hard))
    if any(open(os.path.join(out, *k.split('/')), 'rb').read() != v for k, v in many.items()):
        raise SystemExit('selftest FAILED: inno.py on 300 files: not the same')
    print('inno ok (in the .exe and in slices, two languages, a changed byte caught, '
          'more files than may be open)')

    # the same with the runtime's inno.c (tests/inno/innotest.c)
    exe = cc(os.path.join(d, 'innotest'), [os.path.join(HERE, 'inno', 'innotest.c')] +
             [os.path.join(RUNTIME, f) for f in ('inno.c', 'cdimage.c', 'sys.c', 'sha256.c')])

    def c_unpack(setup, out, must_have):
        r = subprocess.run([exe, setup, out, must_have], capture_output=True, text=True)
        return r.returncode, r.stdout.strip()

    def tree(out):
        return {os.path.relpath(os.path.join(r, f), out).replace(os.sep, '/'):
                open(os.path.join(r, f), 'rb').read() for r, _, fs in os.walk(out) for f in fs}
    for slices in (0, 2):
        setup = os.path.join(d, f'setup_c_{slices}.exe')
        want = mkinno.write(setup, slices)
        out = os.path.join(d, f'c_{slices}')
        rc, said = c_unpack(setup, out, 'game/prog.exe')
        if rc or tree(out) != want['en-US'] or os.path.exists(out + '.part'):
            raise SystemExit(f'selftest FAILED: inno.c ({slices} slices): {said}')
    out = os.path.join(d, 'c_missing')
    rc, said = c_unpack(setup, out, 'NOPE/NOPE.EXE')
    if not rc or 'not the game' not in said or os.path.exists(out) or os.path.exists(out + '.part'):
        raise SystemExit(f'selftest FAILED: inno.c took a setup without the marker: {said}')
    # a setup holding the game's CD image (Mode 2) beside a partial
    # installation that has the marker too, as GOG installs some CD games:
    # the image unpacked, not the partial installation
    image = os.path.join(d, 'image')
    cd_image({'HELLO/HELLO.EXE': b'MZ' + bytes(100), 'HELLO/README.TXT': b'hello\r\n'}, image, 2)
    setup = os.path.join(d, 'setup_c_image.exe')
    mkinno.write(setup, 0, {'CD/game.gog': open(image, 'rb').read(),
                            'HELLO/HELLO.EXE': b'the partial installation'})
    out = os.path.join(d, 'c_image')
    rc, said = c_unpack(setup, out, 'HELLO/HELLO.EXE')
    if (rc or tree(out) != {'HELLO/HELLO.EXE': b'MZ' + bytes(100), 'HELLO/README.TXT': b'hello\r\n'}
            or os.path.exists(out + '.setup')):
        raise SystemExit(f'selftest FAILED: inno.c on a setup holding a CD image: {said}')
    # a changed byte: refused, nothing left behind
    setup = os.path.join(d, 'setup_c_0.exe')
    data = bytearray(open(setup, 'rb').read())
    data[0x440 + 40] ^= 0x55
    open(setup, 'wb').write(data)
    out = os.path.join(d, 'c_bad')
    rc, said = c_unpack(setup, out, '')
    if not rc or os.path.exists(out) or os.path.exists(out + '.part'):
        raise SystemExit(f'selftest FAILED: inno.c took a changed byte: {said}')
    said = [subprocess.run([exe, p], capture_output=True, text=True).stdout.strip()
            for p in (setup, image)]
    if said != ['setup', 'not a setup']:
        raise SystemExit(f'selftest FAILED: inno_is_setup: {said}')
    print('inno.c ok (the same setups; a missing marker refused, a CD image in a setup unpacked, '
          'a changed byte caught)')

    # goglist.py offers it (a GOG installer lying about), new_project.py
    # --setup makes a project of it: the ID from the setup, its files in game/
    mkinno.write(os.path.join(d, 'setup_test_(1234567890).exe'))
    env = dict(os.environ, DOSKIT_GOG_DIRS=d)
    r = subprocess.run([sys.executable, os.path.join(TOOLS, 'goglist.py')], capture_output=True,
                       text=True, env=env)
    if 'Test Setup' not in r.stdout or '(Windows installer)' not in r.stdout:
        print(r.stdout + r.stderr)
        raise SystemExit('selftest FAILED: goglist.py did not offer the installer')
    proj = os.path.join(d, 'project')
    r = subprocess.run([sys.executable, os.path.join(TOOLS, 'new_project.py'), proj, 'Test Setup',
                        'testsetup', '--marker', 'GAME/PROG.EXE', '--setup',
                        os.path.join(d, 'setup_test_(1234567890).exe'), '--no-submodule'],
                       capture_output=True, text=True, env=env)
    main_c = os.path.join(proj, 'port', 'src', 'main.c')
    got = os.path.join(proj, 'game', 'DATA', 'BIG.DAT')
    if (r.returncode or '"1234567890",' not in open(main_c).read() or not os.path.isfile(got)
            or open(got, 'rb').read() != want['en-US']['DATA/BIG.DAT']):
        print(r.stdout + r.stderr)
        raise SystemExit('selftest FAILED: new_project.py --setup')
    # --setup without name and slug: the installer's game taken as if chosen
    # (slug and marker asked for), no list, the project made
    proj = os.path.join(d, 'project2')
    r = subprocess.run([sys.executable, os.path.join(TOOLS, 'new_project.py'), proj, '--setup',
                        os.path.join(d, 'setup_test_(1234567890).exe'), '--no-submodule'],
                       input='\nGAME/PROG.EXE\ny\n', capture_output=True, text=True, env=env)
    main_c = os.path.join(proj, 'port', 'src', 'main.c')
    got = os.path.join(proj, 'game', 'DATA', 'BIG.DAT')
    if (r.returncode or not os.path.isfile(main_c) or '"1234567890",' not in open(main_c).read()
            or '"GAME/PROG.EXE"' not in open(main_c).read() or not os.path.isfile(got)):
        print(r.stdout + r.stderr)
        raise SystemExit('selftest FAILED: new_project.py --setup without a name')
    print('ok   the installer offered by goglist.py, made a project by new_project.py --setup '
          '(with and without a name)')


def check_pmem(py, b):
    """tests/flat/port.c (the start of FLAT.386 in C over pmem.h) against
    the memory the image should have there, made here from the file: the
    image at BASE, its selector relocations set, SAVED_DS and COUNT as
    START leaves them; memcmp.py --base finds no difference, and finds
    the one put into a copy."""
    base, sels = 0x11F2A0, (0x1C, 0x24)
    exe = cc(os.path.join(b, 'flatport'), [os.path.join(HERE, 'flat', 'port.c')] +
             [os.path.join(RUNTIME, f) for f in ('pmem.c', 'sys.c', 'sha256.c')],
             incs=[os.path.join(PROJ, 'port')])
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


def cd_image(files, path, mode=1):
    """a raw CD image (2352-byte Mode 1 sectors, as GOG's game.gog, or
    Mode 2 Form 1 ones with mode=2) with an ISO 9660 file system holding
    `files` ({'DIR/NAME.EXT': bytes}, one folder deep), as much of it as
    isox.py and cdimage.c read"""
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
            sync = b'\0' + b'\xff' * 10 + b'\0' + bytes(3)
            if mode == 1:
                f.write(sync + b'\1' + sectors.get(n, bytes(2048)) + bytes(288))
            else:                           # its subheader, the data, EDC/ECC
                f.write(sync + b'\2' + bytes(8) + sectors.get(n, bytes(2048)) + bytes(280))


def check_plugin(new):
    """The kit as a Claude Code plugin: its manifests, its agents, and
    the new project's settings and AGENTS.md that use them."""
    import json
    def load(*path):
        with open(os.path.join(*path)) as f:
            return json.load(f)
    name = load(KIT, '.claude-plugin', 'plugin.json')['name']
    market = load(KIT, '.claude-plugin', 'marketplace.json')
    if [(p['name'], p['source']) for p in market['plugins']] != [(name, './')]:
        raise SystemExit('selftest FAILED: marketplace.json does not offer the kit\'s plugin')
    key = f'{name}@{market["name"]}'
    rules = open(os.path.join(new, 'AGENTS.md')).read()
    agents = sorted(f[:-3] for f in os.listdir(os.path.join(KIT, 'agents')) if f.endswith('.md'))
    for agent in agents:
        head = open(os.path.join(KIT, 'agents', agent + '.md')).read().split('---\n')[1]
        fields = dict(l.split(': ', 1) for l in head.splitlines())
        if fields.get('name') != agent or not fields.get('description') or 'tools' not in fields:
            raise SystemExit(f'selftest FAILED: agents/{agent}.md: its name, description or tools')
        if f'`{name}:{agent}`' not in rules:
            raise SystemExit(f'selftest FAILED: the template\'s AGENTS.md does not mention {name}:{agent}')
    # the project takes the plugin from its submodule, the kit from itself
    for root, path in ((new, './doskit'), (KIT, './')):
        s = load(root, '.claude', 'settings.json')
        src = s['extraKnownMarketplaces'][market['name']]['source']
        if src != {'source': 'directory', 'path': path} or s['enabledPlugins'].get(key) is not True:
            raise SystemExit(f'selftest FAILED: {root}/.claude/settings.json does not switch on {key}')
    if os.path.exists(os.path.join(new, '.claude', 'agents')):
        raise SystemExit('selftest FAILED: the template carries a copy of the agents')
    print(f'ok   the plugin: {", ".join(agents)}; switched on in a new project and in the kit')


def main():
    if os.path.isdir(PROJ):
        shutil.rmtree(PROJ)
    os.makedirs(os.path.join(PROJ, 'src'))
    os.makedirs(os.path.join(PROJ, 'port'))

    step('0. 32-bit instructions (tests/enc32)')
    print(f'{check_enc32()} lines as capstone reads them')

    step('1. HELLO.EXE assembled and linked')
    print(f'{make_exe()} bytes; FLAT.386 {make_flat()} bytes; LE.EXE {make_le()} bytes; '
          f'RAWDRV.DRV {make_raw()} bytes; '
          f'RELMOD.MOD %d bytes, %d offsets; PMODE.EXE {make_exe("PMODE")} bytes; '
          f'CDROM.EXE {make_exe("CDROM")} bytes; VGAMODE.EXE {make_exe("VGAMODE")} bytes; '
          f'GAMEPORT.EXE {make_exe("GAMEPORT")} bytes; '
          f'JOY.EXE {make_exe("JOY")} bytes; '
          f'MOUSE.EXE {make_exe("MOUSE")} bytes; '
          f'RWATCH.EXE {make_exe("RWATCH")} bytes; '
          f'ADLIB.EXE {make_exe("ADLIB")} bytes; '
          f'SB16.EXE {make_exe("SB16")} bytes; CDPLAY.EXE {make_exe("CDPLAY")} bytes; '
          f'INT24.EXE {make_exe("INT24")} bytes; '
          f'COUNTRY.EXE {make_exe("COUNTRY")} bytes; '
          f'LOL.EXE {make_exe("LOL")} bytes; '
          f'IVT.EXE {make_exe("IVT")} bytes; '
          f'STRREG.EXE {make_exe("STRREG")} bytes; '
          f'REQ.EXE {make_exe("REQ")} bytes; '
          f'MULTISEG.EXE {make_multiseg()} bytes'
          % make_relmod())
    shutil.copy(os.path.join(HERE, 'hello', 'src', 'HELLO.hints'), os.path.join(PROJ, 'src'))
    shutil.copy(os.path.join(HERE, 'flat', 'src', 'FLAT.hints'), os.path.join(PROJ, 'src'))
    flat_hints = open(os.path.join(HERE, 'flat', 'src', 'FLAT.hints'), encoding='utf-8').read()
    flat_hints = flat_hints.replace('pmax       build/files/FLAT.386', 'le        build/files/LE.EXE')
    with open(os.path.join(PROJ, 'src', 'LE.hints'), 'w', encoding='utf-8') as f:
        f.write(flat_hints)
    with open(os.path.join(PROJ, 'src', 'LE_BOUND.hints'), 'w', encoding='utf-8') as f:
        f.write(flat_hints.replace('build/files/LE.EXE', 'build/files/LE_BOUND.EXE'))
    form_asm = os.path.join(PROJ, 'build', 'REGFORM.ASM')
    with open(form_asm, 'w', encoding='utf-8') as f:
        f.write('CODE SEGMENT USE32\n MOV EBP,ESP\n XOR ECX,EDX\n RET\nCODE ENDS\nEND\n')
    form_hints = os.path.join(PROJ, 'src', 'REGFORM.hints')
    with open(form_hints, 'w', encoding='utf-8') as f:
        f.write('bin build/files/REGFORM.BIN\nsegment CODE 0 CODE\nasm regreg_form=rm_reg\n')
    import disasm
    form = tasm.Assembler(form_asm)
    for key, value in disasm.Hints(form_hints).asm.items():
        setattr(form, key, value)
    form.assemble()
    if bytes(form.segments['CODE'].data) != bytes.fromhex('89e531d1c3'):
        raise SystemExit('selftest FAILED: register encoding option from hints')
    with open(os.path.join(PROJ, 'build', 'files', 'REGFORM.BIN'), 'wb') as f:
        f.write(form.segments['CODE'].data)
    shutil.copy(os.path.join(HERE, 'raw', 'src', 'RAWDRV.hints'), os.path.join(PROJ, 'src'))
    shutil.copy(os.path.join(HERE, 'relmod', 'src', 'RELMOD.hints'), os.path.join(PROJ, 'src'))
    shutil.copy(os.path.join(HERE, 'multiseg', 'src', 'MULTISEG.hints'), os.path.join(PROJ, 'src'))
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
    # a second run takes the rebuilds that held from build/check/; a hints
    # file changed since, or --fresh, builds again
    again = run([py, os.path.join(TOOLS, 'check.py')])
    if not re.search(r'HELLO\.hints: .* \(cached\)', again) or \
            not again.splitlines()[-1].startswith('all ok'):
        raise SystemExit('selftest FAILED: check.py did not take its cache\n' + again)
    hello = os.path.join(PROJ, 'src', 'HELLO.hints')
    with open(hello, 'a') as f:
        f.write('; changed\n')
    again = run([py, os.path.join(TOOLS, 'check.py'), 'src/HELLO.hints'])
    if '(cached)' in again or not again.splitlines()[-1].startswith('all ok'):
        raise SystemExit('selftest FAILED: check.py cached a changed hints file\n' + again)
    again = run([py, os.path.join(TOOLS, 'check.py'), '--fresh', 'src/HELLO.hints'])
    if '(cached)' in again:
        raise SystemExit('selftest FAILED: check.py --fresh took its cache\n' + again)
    # MULTISEG's instructions as the assembler writes them, none as DB (a
    # far call in its own segment, the word forms, the full displacement)
    if not re.search(r'MULTISEG\.hints: .* 0 as DB', out):
        raise SystemExit('selftest FAILED: MULTISEG has instructions written as DB')
    with open(os.path.join(PROJ, 'build', 'MULTISEG.ASM')) as f:
        text = f.read()
    for want in ('MOV AX,OFFSET tick', 'tick:\n\tINC ', 'DW tock', 'tock:\n\tDEC '):
        if want not in text:
            raise SystemExit(f'selftest FAILED: MULTISEG.ASM has no {want!r} (ptr and words '
                             'hints into a code segment other than CODE)')
    print('MULTISEG: routines reached through pointers into FOUR, a segment not named CODE')
    print(f'MULTIS2.EXE ({check_xfer(py)} bytes): MULTISEG\'s hints carried by xfer.py '
          'into renamed and shifted code segments, IDENTICAL')
    print(f'MULTIPAD.EXE ({check_hdrpad(py)} bytes, a longer header): IDENTICAL with '
          'header=original, differs without')
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
    hints = os.path.join(PROJ, 'src', 'MULTISEG.hints')
    with open(hints) as f:
        whole = f.read()
    with open(os.path.join(PROJ, 'build', 'NOPREFIX.hints'), 'w') as f:
        # without the names too: TWO's and THREE's routines both L0006
        f.write(re.sub(r'^name .*\n', '', re.sub(r' prefix=\S+', '', whole), flags=re.M))
    out = run([py, os.path.join(TOOLS, 'build.py'), os.path.join(PROJ, 'build', 'NOPREFIX.hints')], check=False)
    if 'give the segments their own prefix=' not in out:
        print(out)
        raise SystemExit('selftest FAILED: MULTISEG without prefix= hints')
    print('MULTISEG without its prefix= hints: build.py says the labels collide')
    hints = os.path.join(PROJ, 'src', 'HELLO.hints')
    with open(hints) as f:
        whole = f.read()
    with open(os.path.join(PROJ, 'build', 'DUPNAME.hints'), 'w') as f:
        # next_step at another address too: a name given twice is refused
        f.write(whole + 'name CODE:002B next_step\n')
    out = run([py, os.path.join(TOOLS, 'build.py'), os.path.join(PROJ, 'build', 'DUPNAME.hints')], check=False)
    if 'name next_step given twice (CODE:18 and CODE:2B)' not in out:
        print(out)
        raise SystemExit('selftest FAILED: HELLO with a name given twice')
    print('HELLO with a name given twice: build.py refuses it')
    import disasm as disasm_py
    with open(os.path.join(PROJ, 'build', 'SAMENAME.hints'), 'w') as f:
        # the same name at the same address again: still fine
        f.write(whole + 'name CODE:0018 next_step\n')
    disasm_py.Hints(os.path.join(PROJ, 'build', 'SAMENAME.hints'))
    print('HELLO with a name repeated at its own address: taken')

    step('3. run in the runner (run.py)')
    import run as run_py
    with open(os.path.join(TOOLS, 'run', 'main.c')) as f:
        parsed = set(re.findall(r'strcmp\(a,"(-[a-z]+)"\)', f.read()))
    if parsed != set(run_py.OPTS):
        raise SystemExit('selftest FAILED: run.py\'s OPTS and the runner\'s options differ: '
                         + ' '.join(sorted(parsed ^ set(run_py.OPTS))))
    import disasm
    names = run_py.Names(disasm.Hints(os.path.join(PROJ, 'src', 'MULTISEG.hints')), 'MULTISEG/MULTISEG.EXE')
    got = [names.lookup(t) for t in ('LTWO_000C', 'LTHR_0023', 'ONE_0006', 'third')]
    if got != [('TWO', 0xC), ('THREE', 0x23), ('ONE', 6), ('THREE', 0xC)]:
        raise SystemExit(f'selftest FAILED: run.py\'s names of segments with prefix=: {got}')
    print('run.py finds the labels of segments with their own prefix')
    for a, b, want in (('GAME/GAME.EXE', 'GAME.EXE', True),
                       ('build/files/IMAGE.386', 'IMAGE.EXE', True),
                       ('build/files/IMAGE.386', 'OTHER.EXE', False),
                       ('GAME/GAME.EXE', 'GAME/GAME.EXE', True)):
        if run_py.same_program(a, b) != want:
            raise SystemExit(f'selftest FAILED: run.py matches {a} with {b} as {not want}')
    print('run.py matches a loader with the image it loads')
    b = os.path.join(PROJ, 'build')
    with open(os.path.join(b, 'IMAGE.hints'), 'w') as f:
        f.write('pmax build/files/IMAGE.386\nsegment CODE 0 CODE\n'
                'code CODE:9D83 PLAY\nname CODE:02A3 ENTRY\n')
    img = disasm.Hints(os.path.join(b, 'IMAGE.hints'))
    got = [run_py.Names(img, 'IMAGE.EXE', 0x100F30).translate(t)
           for t in ('CODE:9D83', 'PLAY', 'ENTRY+4', 'PLAY#3')]
    if got != ['10ACB3', '10ACB3', '1011D7', '10ACB3#3']:
        raise SystemExit(f'selftest FAILED: run.py translates no image address with -base: {got}')
    print("run.py translates an image's names with -base")
    wide = run_py.Names(img, 'IMAGE.EXE', 0x100F30)
    got = [wide.translate(t) for t in
           ('CODE:12345', 'L12345+4#2', 'CODE:FFFFFFFF', '0180:12345')]
    if got != ['113275', '113279#2', '100100F2F', '0180:12345']:
        raise SystemExit(f'selftest FAILED: run.py truncates a 32-bit offset: {got}')
    if wide.lookup('CODE:100000000') is not None:
        raise SystemExit('selftest FAILED: run.py accepts an offset wider than 32 bits')
    got = names.lookup('LTWO_12345')
    if got != ('TWO', 0x12345):
        raise SystemExit(f'selftest FAILED: run.py truncates a label with prefix=: {got}')
    print('run.py translates 32-bit offsets and generated labels')
    le_hints = disasm.Hints(os.path.join(PROJ, 'src', 'LE.hints'))
    previous_project = os.environ.get('DOSKIT_PROJECT')
    os.environ['DOSKIT_PROJECT'] = PROJ
    try:
        le = disasm.load_program(le_hints)
        expected = 0x100000 + le.descs[1][0] + 4
        got = run_py.Names(le_hints, 'LE.EXE', 0x100000).translate('OTHER')
        wide_le = run_py.Names(le_hints, 'LE.EXE', 0x100000)
        wide_expected = 0x100000 + le.descs[0][0] + 0x12345 + 4
        wide_got = wide_le.translate('CODE:12345+4#2')
    finally:
        if previous_project is None:
            os.environ.pop('DOSKIT_PROJECT', None)
        else:
            os.environ['DOSKIT_PROJECT'] = previous_project
    if got != f'{expected:X}':
        raise SystemExit(f'selftest FAILED: run.py omitted the LE object base: {got}')
    if wide_got != f'{wide_expected:X}#2':
        raise SystemExit(f'selftest FAILED: run.py truncates an LE object offset: {wide_got}')
    print('run.py translates LE names with the object base and -base')
    try:
        run_py.Names(img, 'IMAGE.EXE').translate('PLAY')
    except SystemExit as e:
        if 'no fixed address' not in str(e):
            raise SystemExit(f'selftest FAILED: run.py says the wrong thing without -base: {e}')
    else:
        raise SystemExit('selftest FAILED: run.py translates an image address without -base')
    print('run.py refuses an image address without -base')
    out = run([py, os.path.join(TOOLS, 'run.py'), '-until', '1', '-break', 'CODE:0026',
               '-dump', 'counter', '2', '-ram', os.path.join(b, 'orig.ram'),
               '-mem', os.path.join(b, 'orig.mem'),
               '-vram', os.path.join(b, 'orig.vram'), '-dac', os.path.join(b, 'orig.dac'),
               '-vgastate', 'HELLO/HELLO.EXE'])
    print('\n'.join(l for l in out.splitlines() if l.startswith(('con:', 'break', 'dump'))))
    # -dac: the 256 colours, the first 32 as -vgastate prints them
    with open(os.path.join(b, 'orig.dac'), 'rb') as f:
        dac = f.read()
    shown = bytes.fromhex(''.join(''.join(m.split()) for m in re.findall(r'dac [0-9A-F]{2}:((?: [0-9A-F]{6})+)', out)))
    if len(dac) != 768 or len(shown) != 96 or dac[:96] != shown:
        raise SystemExit('selftest FAILED: -dac is not the 768 bytes of the DAC -vgastate shows')
    print('-dac: 768 bytes, the first 32 colours as -vgastate shows them')
    if 'con: hello from doskit' not in out:
        raise SystemExit('selftest FAILED: no console line from HELLO.EXE')
    with open(os.path.join(b, 'orig.ram'), 'rb') as f, open(os.path.join(b, 'orig.mem'), 'rb') as g:
        ram, whole = f.read(), g.read()
    if len(whole) != 0x1000000 or whole[:len(ram)] != ram:
        raise SystemExit('selftest FAILED: -mem is not the 16 MB that begin with -ram\'s bytes')
    print('-mem: 16 MB, beginning with -ram\'s 640 KB')
    # -cover and gaps.py --cover: with the hints as they are no gap ran;
    # without the words hint the two routines of the table are gaps, and
    # both ran
    cov = os.path.join(b, 'hello.cover')
    run([py, os.path.join(TOOLS, 'run.py'), '-until', '1', '-cover', cov, 'HELLO/HELLO.EXE'])
    out = run([py, os.path.join(TOOLS, 'gaps.py'), 'src/HELLO.hints', '--cover', cov])
    if not out.endswith('0 gaps ran in that run'):
        print(out)
        raise SystemExit('selftest FAILED: gaps.py --cover finds a gap of HELLO that ran')
    with open(os.path.join(PROJ, 'src', 'HELLO.hints')) as f:
        whole = f.read()
    with open(os.path.join(b, 'NOWORDS.hints'), 'w') as f:
        f.write(re.sub(r'^words .*\n', '', whole, flags=re.M))
    out = run([py, os.path.join(TOOLS, 'gaps.py'), os.path.join(b, 'NOWORDS.hints'), '--cover', cov])
    if (not re.search(r'^002B-\S+ .* RAN: \d+ instructions, the first at CODE:002B$', out, re.M)
            or not out.endswith('1 gaps ran in that run')):
        print(out)
        raise SystemExit('selftest FAILED: gaps.py --cover without HELLO\'s words hint')
    print('-cover: the routines of HELLO\'s table ran; gaps.py --cover shows them once unreached')
    # an LE image runs where its extender mapped the objects, not at the MZ
    # stub's load segment: the load line names each object's linear
    # address (OBJ@LINEAR), and gaps.py takes the image offsets from those.
    # A made-up run of LE.EXE without its words hint: the table's three
    # routines ran with CODE at 178000h, MORE at 190000h.
    with open(os.path.join(PROJ, 'src', 'LE.hints')) as f:
        whole = f.read()
    nowords = os.path.join(b, 'LENOWORDS.hints')
    with open(nowords, 'w') as f:
        f.write(re.sub(r'^words .*\n', '', whole, count=1, flags=re.M))
    previous_project = os.environ.get('DOSKIT_PROJECT')
    os.environ['DOSKIT_PROJECT'] = PROJ
    try:
        le = disasm.load_program(disasm.Hints(nowords))
    finally:
        if previous_project is None:
            os.environ.pop('DOSKIT_PROJECT', None)
        else:
            os.environ['DOSKIT_PROJECT'] = previous_project
    code_base = le.descs[0][0]
    targets = [int.from_bytes(le.img[code_base + 6 + 4 * i:code_base + 10 + 4 * i], 'little')
               for i in range(3)]
    lecov = os.path.join(b, 'le.cover')
    with open(lecov, 'w') as f:
        f.write('load LE.EXE 1234 1@178000 2@190000\n')
        f.write(''.join(f'{0x178000 + t:X}\n' for t in sorted(targets)))
    out = run([py, os.path.join(TOOLS, 'gaps.py'), nowords, '--cover', lecov])
    first = min(targets)
    if (not re.search(rf'RAN: \d+ instructions, the first at CODE:{first:04X}$', out, re.M)
            or out.endswith('0 gaps ran in that run')):
        print(out)
        raise SystemExit('selftest FAILED: gaps.py --cover does not use the LE objects\' linear addresses')
    with open(lecov, 'w') as f:
        f.write('load LE.EXE 1234\n' + ''.join(f'{0x178000 + t:X}\n' for t in targets))
    out = run([py, os.path.join(TOOLS, 'gaps.py'), nowords, '--cover', lecov], check=False)
    if 'no LE object' not in out:
        raise SystemExit('selftest FAILED: gaps.py --cover takes an LE run without object addresses')
    print('-cover: an LE image\'s gaps ran at the objects\' linear addresses, not the MZ load segment')
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
    # -keyat: Left Shift down at next_step's first pass (the runner's INT 9
    # keeps the shift state in the BIOS data area, 417h bit 1), and at a
    # third pass, which never comes
    for spec, want in (('CODE:0018', 2), ('CODE:0018#3', 0)):
        out = run([py, os.path.join(TOOLS, 'run.py'), '-until', '1', '-keyat', spec, 'lshift+',
                   '-ram', os.path.join(b, 'keyat.ram'), 'HELLO/HELLO.EXE'])
        with open(os.path.join(b, 'keyat.ram'), 'rb') as f:
            shift = f.read()[0x417] & 2
        said = [l for l in out.splitlines() if l.startswith('keyat ') and ' t=' in l]
        if shift != want or len(said) != (1 if want else 0):
            print(out)
            raise SystemExit(f'selftest FAILED: -keyat {spec} lshift+ on HELLO.EXE: shift bit {shift}')
    print('-keyat: Left Shift down at the first pass of HELLO\'s loop, none at a third')
    # -keysat: the same keys from a file
    for lines, want in (('1 lshift+\n', 2), ('# a comment\n3 lshift+\n', 0)):
        keys = os.path.join(b, 'keysat.txt')
        with open(keys, 'w') as f:
            f.write(lines)
        run([py, os.path.join(TOOLS, 'run.py'), '-until', '1', '-keysat', 'CODE:0018', keys,
             '-ram', os.path.join(b, 'keyat.ram'), 'HELLO/HELLO.EXE'])
        with open(os.path.join(b, 'keyat.ram'), 'rb') as f:
            shift = f.read()[0x417] & 2
        if shift != want:
            raise SystemExit(f'selftest FAILED: -keysat {lines!r} on HELLO.EXE: shift bit {shift}')
    # several -keysat, each its own list: the first's key at a pass not reached, the second's at the first
    # (HELLO takes one key a pass)
    keys2 = os.path.join(b, 'keysat2.txt')
    with open(keys, 'w') as f:
        f.write('3 lctrl+\n')
    with open(keys2, 'w') as f:
        f.write('1 lshift+\n')
    run([py, os.path.join(TOOLS, 'run.py'), '-until', '1', '-keysat', 'CODE:0018', keys, '-keysat', 'CODE:0018', keys2,
         '-ram', os.path.join(b, 'keyat.ram'), 'HELLO/HELLO.EXE'])
    with open(os.path.join(b, 'keyat.ram'), 'rb') as f:
        shift = f.read()[0x417] & 6
    if shift != 2:
        raise SystemExit(f'selftest FAILED: two -keysat lists on HELLO.EXE: shift and ctrl bits {shift}, not 2')
    print('-keysat: the same keys from a file, several lists')
    # a table with more readers than the runner once kept (64): RWATCH.EXE's
    # 200 bytes, each read by its LODSB, the first 100 by its CMP as well
    out = run([py, os.path.join(TOOLS, 'run.py'), '-until', '1', '-rwatch', 'RWATCH.EXE+0004:0000', 'C8',
               'RWATCH/RWATCH.EXE'])
    memr = [l for l in out.splitlines() if l.startswith('[memr]')]
    readers = [(int(l.split()[1].split('+')[1], 16), l.split()[4]) for l in memr if ' read by ' in l and 'times' in l]
    by_ip = {}
    for off, ip in readers:
        by_ip.setdefault(ip, []).append(off)
    print(f'{memr[-1]}; ' + ' '.join(l for l in out.splitlines() if l.startswith('con:')))
    if ('con: rwatch ok' not in out or sorted(map(sorted, by_ip.values()), key=len) != [list(range(100)), list(range(200))]
            or len(set(readers)) != 300 or any('not kept' in l for l in memr)
            or not all(' 1 times' in l for l in memr if 'times' in l)
            or not memr[-1].endswith('read by 300 readers')):
        print(out)
        raise SystemExit('selftest FAILED: -rwatch on RWATCH.EXE\'s table of 300 readers')
    out = run([py, os.path.join(TOOLS, 'run.py'), '-until', '1', 'PMODE/PMODE.EXE'])
    print('\n'.join(l for l in out.splitlines() if l.startswith(('con:', '[cpu]'))))
    if 'con: pmode ok' not in out:
        print(out)
        raise SystemExit('selftest FAILED: PMODE.EXE (the runner\'s protected mode)')
    out = run([py, os.path.join(TOOLS, 'run.py'), '-until', '0.02', 'PMODE/PMODE.EXE'])
    if 'pm CR0=' not in out or 'pm bases CS=' not in out or 'pm EAX=' not in out:
        print(out)
        raise SystemExit('selftest FAILED: protected-mode register report')
    out = run([py, os.path.join(TOOLS, 'run.py'), '-until', '1', 'INT24/INT24.EXE'])
    if 'con: int24 no fault ok' not in out:
        print(out)
        raise SystemExit('selftest FAILED: INT24.EXE without a critical error')
    out = run([py, os.path.join(TOOLS, 'run.py'), '-until', '1', '-criterr', '3d', '15', 'INT24/INT24.EXE'])
    if 'con: int24 ok' not in out:
        print(out)
        raise SystemExit('selftest FAILED: -criterr through INT 24')
    print('-criterr: INT 24 returns Fail and its INT 21 caller gets the injected error')
    out = run([py, os.path.join(TOOLS, 'run.py'), '-until', '1', '-requirements',
               'COUNTRY/COUNTRY.EXE'])
    if 'con: country ok' not in out or 'unimplemented DOS: AH=38' in out or 'unimplemented DOS: AH=63' in out:
        print(out)
        raise SystemExit('selftest FAILED: INT 21h AH=38h country information')
    print('INT 21h AH=38h: current US country data and unsupported country code')
    out = run([py, os.path.join(TOOLS, 'run.py'), '-until', '1', '-requirements', 'LOL/LOL.EXE'])
    if 'con: lol ok' not in out or 'unimplemented DOS: AH=52' in out:
        print(out)
        raise SystemExit('selftest FAILED: INT 21h AH=52h List of Lists')
    print('INT 21h AH=52h: first MCB, NUL device header, no SFT or DPB chain')
    out = run([py, os.path.join(TOOLS, 'run.py'), '-until', '1', 'IVT/IVT.EXE'])
    if 'con: ivt ok' not in out:
        print(out)
        raise SystemExit('selftest FAILED: unused user interrupt vectors')
    print('IVT: unused user vectors share an IRET; DOS and mouse services still work')
    out = run([py, os.path.join(TOOLS, 'run.py'), '-until', '1', 'STRREG/STRREG.EXE'])
    if 'con: strreg ok' not in out:
        print(out)
        raise SystemExit('selftest FAILED: the registers after REP STOS and REP MOVS')
    print('REP STOS leaves SI, REP MOVS moves it: block path, loop and backwards')
    out = run([py, os.path.join(TOOLS, 'run.py'), '-until', '1', '-requirements', 'REQ/REQ.EXE'])
    want = ('requirements observed:', 'devices: VGA', 'memory: VGA read/write',
            'unknown ports: 123', 'unimplemented DOS: AH=66', 'unimplemented BIOS: INT 10h/AH=FF AL=00',
            'unsupported CPU: x87 esc D9 x87 esc DB')
    if not all(s in out for s in want):
        print(out)
        raise SystemExit('selftest FAILED: -requirements summary')
    print('-requirements: used VGA interfaces and missing port, BIOS and DOS services')
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
    out = run([py, os.path.join(TOOLS, 'run.py'), '-until', '1', 'GAMEPORT/GAMEPORT.EXE'])
    print('\n'.join(l for l in out.splitlines() if l.startswith('con:')))
    if 'con: gameport ok' not in out:
        print(out)
        raise SystemExit('selftest FAILED: GAMEPORT.EXE (the runner\'s game port)')
    joys = os.path.join(b, 'joy.joys')
    with open(joys, 'w') as f:
        f.write('# a second stick that goes away again before the program looks\n0 1,100,100,3\n0 1,off\n')
    out = run([py, os.path.join(TOOLS, 'run.py'), '-until', '1', '-joy', '0', '0,300,600,1',
               '-joys', joys, 'JOY/JOY.EXE'])
    print('\n'.join(l for l in out.splitlines() if l.startswith('con:')))
    if 'con: joy ok' not in out:
        print(out)
        raise SystemExit('selftest FAILED: JOY.EXE (the runner\'s -joy and -joys)')
    mouse_png = os.path.join(b, 'mouse.png')
    out = run([py, os.path.join(TOOLS, 'run.py'), '-until', '1',
               '-mouse', '0.02', '100,60,0', '-mouse', '0.04', '120,70,1',
               '-mouse', '0.06', '120,70,0', '-shot', '0.05', mouse_png,
               'MOUSE/MOUSE.EXE'])
    print('\n'.join(l for l in out.splitlines() if l.startswith(('con:', 'shot '))))
    if 'con: mouse ok' not in out:
        print(out)
        raise SystemExit('selftest FAILED: MOUSE.EXE (the runner\'s INT 33h driver)')
    w, h, rgb = read_png(mouse_png)
    lit = [(i % w, i // w) for i in range(w * h) if rgb[i*3:i*3+3] != b'\0\0\0']
    if (w, h) != (320, 200) or not (5 <= len(lit) <= 100) or not all(115 <= x <= 138 and 58 <= y <= 80 for x, y in lit):
        raise SystemExit(f'selftest FAILED: the mouse-cursor shot: {w}x{h}, {len(lit)} lit pixels')
    oplwav = os.path.join(b, 'adlib.wav')
    out = run([py, os.path.join(TOOLS, 'run.py'), '-until', '2', '-oplwav', oplwav, 'ADLIB/ADLIB.EXE'])
    print('\n'.join(l for l in out.splitlines() if l.startswith('con:')))
    if 'con: adlib ok' not in out:
        print(out)
        raise SystemExit('selftest FAILED: ADLIB.EXE (the runner\'s OPL2 timers)')
    check_adlib_wav(oplwav)
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
    srcs = [os.path.join(HERE, 'hello', 'port.c')] + [
        os.path.join(RUNTIME, f) for f in ('rmem.c', 'vga.c', 'sys.c', 'sha256.c', 'shot.c',
                                           'plat_null.c')]
    exe = cc(os.path.join(b, 'port'), srcs, incs=[os.path.join(PROJ, 'port')])
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
    exe = cc(os.path.join(b, 'shottest'), [os.path.join(HERE, 'shot', 'shottest.c'),
                                           os.path.join(RUNTIME, 'shot.c'),
                                           os.path.join(RUNTIME, 'sys.c')])
    run([exe, shots])
    for name in ('noise', 'runs', 'far', 'pattern', 'one'):
        w, h, rgb = read_png(os.path.join(shots, name + '.png'))
        pix = open(os.path.join(shots, name + '.bin'), 'rb').read()
        pal = open(os.path.join(shots, name + '.pal'), 'rb').read()
        if len(pix) != w * h or rgb != b''.join(pal[3 * i:3 * i + 3] for i in pix):
            raise SystemExit(f'selftest FAILED: shot.c\'s {name}.png')
        print(f'shot.c {name}.png: {w}x{h}, {os.path.getsize(os.path.join(shots, name + ".png"))}'
              f' bytes for {w * h} pixels')
    vsync = os.path.join(b, 'vsync')
    os.makedirs(vsync, exist_ok=True)
    exe = cc(os.path.join(b, 'vsynctest'), [os.path.join(HERE, 'vsync', 'vsynctest.c')] + [
        os.path.join(RUNTIME, f) for f in ('plat_null.c', 'shot.c', 'sys.c')])
    out = subprocess.run(
        [exe, vsync],
        cwd=PROJ, capture_output=True, text=True,
        env=dict(os.environ, DOSKIT_PROJECT=PROJ,
                 DK_SHOTS='0:' + os.path.join(vsync, 'got0.png') +
                          ' 1:' + os.path.join(vsync, 'got1.png') +
                          ' 2:' + os.path.join(vsync, 'got2.png')))
    if out.returncode or 'vsync ok' not in out.stdout:
        print(out.stdout + out.stderr)
        raise SystemExit('selftest FAILED: plat_set_vsync')
    exe = cc(os.path.join(b, 'platmousetest'), [os.path.join(HERE, 'platmouse', 'platmousetest.c')] + [
        os.path.join(RUNTIME, f) for f in ('plat_null.c', 'shot.c', 'sys.c')])
    out = subprocess.run(
        [exe], cwd=PROJ, capture_output=True, text=True,
        env=dict(os.environ,
                 DK_MOUSEMOVE='2:5,-3,1 2:1,1,1 4:0,0,0 6:-7,2,6 8:9,9,0 9:4,0,0'))
    if out.returncode or 'platmouse ok' not in out.stdout:
        print(out.stdout + out.stderr)
        raise SystemExit('selftest FAILED: plat_mouse_motion')
    print(out.stdout.strip())
    rgb0 = rgb1 = None
    for got, exp in (('got0', 'expA'), ('got1', 'expB'), ('got2', 'expA')):
        w, h, rgb = read_png(os.path.join(vsync, got + '.png'))
        w2, h2, rgb2 = read_png(os.path.join(vsync, exp + '.png'))
        if (w, h, rgb) != (w2, h2, rgb2):
            raise SystemExit(f'selftest FAILED: plat_set_vsync: {got}.png is not {exp}.png')
        if got == 'got0':
            rgb0 = rgb
        if got == 'got1':
            rgb1 = rgb
    if rgb1 == rgb0:
        raise SystemExit('selftest FAILED: plat_set_vsync: the pictures did not change')
    print('plat_set_vsync off/on/off: the three shots as handed in')
    exe = cc(os.path.join(b, 'vgamodes'), [os.path.join(HERE, 'vgamode', 'runtime.c'),
                                           os.path.join(RUNTIME, 'vga.c')])
    out = run([exe])
    if 'vga modes ok' not in out:
        raise SystemExit('selftest FAILED: vga.c\'s 16-colour and VESA modes: ' + out)
    print(out.strip())
    exe = cc(os.path.join(b, 'cdatest'), [os.path.join(HERE, 'cdaudio', 'cdatest.c')] + [
        os.path.join(RUNTIME, f) for f in ('cdaudio.c', 'cdimage.c', 'plat_null.c', 'sys.c', 'shot.c')])
    cue = make_cue()
    out = run([exe, cue])
    if 'cdaudio ok' not in out:
        raise SystemExit('selftest FAILED: cdaudio.c: ' + out)
    copy = os.path.join(b, 'cuecopy')
    if os.path.isdir(copy):
        shutil.rmtree(copy)
    out = run([exe, cue, copy])
    if 'cdaudio ok' not in out or not os.path.isfile(os.path.join(copy, 'MUSIC', 'TRACK03.OGG')):
        raise SystemExit('selftest FAILED: cdimage.c\'s cd_copy_disc: ' + out)
    print('cd_copy_disc: the disc copied, its copy read: ' + out.strip())
    boxer = os.path.join(b, 'Illusions.boxer')
    source = os.path.join(boxer, 'C.harddisk', 'illusion', 'Illusions')
    shutil.copytree(os.path.dirname(cue), source)
    track = os.path.join(source, 'music', 'track03.ogg')
    cdmedia = os.path.join(boxer, 'game.cdmedia')
    os.makedirs(cdmedia)
    shutil.copy(track, cdmedia)
    os.remove(track)
    boxed_cue = os.path.join(source, 'game.inst')
    boxed_copy = os.path.join(b, 'boxer-cuecopy')
    out = run([exe, boxed_cue, boxed_copy])
    if 'cdaudio ok' not in out or not os.path.isfile(os.path.join(boxed_copy, 'MUSIC', 'TRACK03.OGG')):
        raise SystemExit('selftest FAILED: cdimage.c\'s Boxer .cdmedia fallback: ' + out)
    print('cd_copy_disc: the Boxer .cdmedia track copied and read: ' + out.strip())
    exe = cc(os.path.join(b, 'launchtest'), [os.path.join(HERE, 'launcher', 'launchtest.c')] + [
        os.path.join(RUNTIME, f) for f in ('launcher.c', 'textmode.c', 'pad.c', 'frame.c', 'vga.c',
                                           'plat_null.c', 'shot.c', 'sys.c')])
    out = run([exe, os.path.join(b, 'launchtest.cfg')])
    if 'launcher ok' not in out:
        raise SystemExit('selftest FAILED: launcher.c / frame.c\'s keymap: ' + out)
    print(out.strip())
    exe = cc(os.path.join(b, 'dialogtest'), [os.path.join(HERE, 'launcher', 'dialogtest.c')] + [
        os.path.join(RUNTIME, f) for f in ('launcher.c', 'textmode.c', 'pad.c', 'plat_null.c',
                                           'shot.c', 'sys.c')])
    out = run([exe])
    if 'dialog ok' not in out:
        raise SystemExit('selftest FAILED: launcher.c\'s dialog about the game\'s files: ' + out)
    print(out.strip())
    exe = cc(os.path.join(b, 'hudtest'), [os.path.join(HERE, 'hud', 'hudtest.c'),
                                           os.path.join(RUNTIME, 'hud.c')])
    out = run([exe])
    if 'hud ok' not in out:
        raise SystemExit('selftest FAILED: hud.c: ' + out)
    print(out.strip())
    exe = cc(os.path.join(b, 'opltest'), [os.path.join(HERE, 'opl', 'opltest.c'),
                                           os.path.join(RUNTIME, 'opl.c')])
    out = run([exe])
    if 'opl ok' not in out:
        raise SystemExit('selftest FAILED: opl.c: ' + out)
    print(out.strip())
    check_oplq(b)
    check_update(b)
    check_gogfind(b)
    check_goglist_linux(b)
    print(run([py, os.path.join(TOOLS, 'memcmp.py'), 'src/HELLO.hints',
               os.path.join(b, 'orig.ram'), os.path.join(b, 'port.ram'), '--skip', 'STACK',
               '--vram', os.path.join(b, 'orig.vram'), os.path.join(b, 'port.vram')]))
    check_pmem(py, b)

    step('5. every runtime module compiles')
    mods = [f for f in sorted(os.listdir(RUNTIME)) if f.endswith('.c')
            and f not in ('plat_sdl.c', 'plat_win32.c')]
    extra = []
    if os.name == 'nt':
        # Windows' own platform; SDL2 is not looked for there
        mods.append('plat_win32.c')
    else:
        sdl = subprocess.run(['sh', os.path.join(RUNTIME, 'sdl2-flags.sh')], capture_output=True,
                             text=True)
        if sdl.returncode == 0:
            mods.append('plat_sdl.c')
            # the compiler's options only: -I, -D and -F
            extra = [o for o in sdl.stdout.split() if o.startswith(('-I', '-D', '-F'))]
    objdir = os.path.join(b, 'obj')
    os.makedirs(objdir, exist_ok=True)
    for m in mods:
        cc(os.path.join(objdir, m[:-2] + ('.obj' if MSVC else '.o')), [os.path.join(RUNTIME, m)],
           defs=extra, obj=True)
    print(f'{len(mods)} modules: {" ".join(mods)}' +
          ('' if 'plat_sdl.c' in mods or os.name == 'nt'
           else ' (SDL2 not found: plat_sdl.c not compiled)'))

    step('6. a project from the template (new_project.py)')
    new = os.path.join(KIT, 'build', 'selftest-new')
    if os.path.isdir(new):
        shutil.rmtree(new)
    run([py, os.path.join(TOOLS, 'new_project.py'), new, 'Test Game', 'testgame',
         '--marker', 'HELLO/HELLO.EXE', '--gog-id', '1234567890', '--no-submodule'])
    if '"1234567890",' not in open(os.path.join(new, 'port', 'src', 'main.c')).read():
        raise SystemExit('selftest FAILED: --gog-id not filled into port/src/main.c')
    link_dir(KIT, os.path.join(new, 'doskit'))
    shutil.copytree(os.path.join(PROJ, 'game'), os.path.join(new, 'game'))
    if os.name == 'nt':
        # MSVC says its warnings on the standard output
        r = subprocess.run(['cmd', '/c', os.path.join(new, 'port', 'build.bat')],
                           capture_output=True, text=True)
        script = 'build.bat'
    else:
        r = subprocess.run(['sh', os.path.join(new, 'port', 'build.sh')], capture_output=True,
                           text=True)
        script = 'build.sh'
    if r.returncode or 'warning' in r.stderr or (os.name == 'nt' and 'warning' in r.stdout):
        print(r.stdout + r.stderr)
        raise SystemExit(f'selftest FAILED: the template\'s port/{script}')
    # a data folder of its own: never the user's
    first = os.path.join(KIT, 'build', 'selftest-data')
    if os.path.isdir(first):
        shutil.rmtree(first)
    os.makedirs(first)
    r = subprocess.run([os.path.join(new, 'port', 'build', 'testgame-headless' + EXE)], cwd=new,
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
    check_uninstall(new)

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
    exe = os.path.join(new, 'port', 'build', 'testgame-headless' + EXE)
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

    # found by itself, the copy is offered first (asked headless only with
    # keys) in launcher.c's dialog: Esc, or Down to "Quit" and Enter, leave
    # the data folder without the game; Enter on "Copy the files" copies it
    for keys, ok in (('2:01', False), ('2:E0-50 4:1C', False), ('2:1C', True)):
        shutil.rmtree(os.path.join(data, 'game'), ignore_errors=True)
        r = subprocess.run([exe], cwd=empty, capture_output=True, text=True,
                           env=dict(os.environ, DK_FRAMES='20', DK_DATA_DIR=data,
                                    DK_KEYS=keys))
        there = os.path.isfile(got) and open(got, 'rb').read() == hello
        if (r.returncode == 0) != ok or there != ok:
            print(r.stdout + r.stderr)
            raise SystemExit(f'selftest FAILED: the copy offered (keys {keys})')
    print('ok   the copy offered: Esc and "Quit" decline it, "Copy the files" copies')

    # GOG's Windows installer, where GOG sells a game for Windows only:
    # named by -gog, and found in ~/Downloads (HOME a folder of its own)
    # by its product ID without it
    sys.path.insert(0, os.path.join(HERE, 'inno'))
    import mkinno
    home = os.path.join(KIT, 'build', 'selftest-home')
    if os.path.isdir(home):
        shutil.rmtree(home)
    os.makedirs(os.path.join(home, 'Downloads'))
    setup = os.path.join(home, 'Downloads', 'setup_hello_game_1.0_(1).exe')
    mkinno.write(setup, 0, {'HELLO/HELLO.EXE': hello})
    for what, args in (('-gog', ['-gog', setup]), ('found in Downloads', [])):
        shutil.rmtree(data)
        os.makedirs(data)
        r = subprocess.run([exe] + args, cwd=empty, capture_output=True, text=True,
                           env=dict(os.environ, DK_FRAMES='3', DK_DATA_DIR=data, HOME=home,
                                    USERPROFILE=home))
        if r.returncode or not os.path.isfile(got) or open(got, 'rb').read() != hello:
            print(r.stdout + r.stderr)
            raise SystemExit(f'selftest FAILED: the template\'s port and a setup ({what})')
    print('ok   the template\'s port: GOG\'s Windows installer unpacked, named and found')

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

    check_plugin(new)

    step('7. Inno Setup installers (inno.py)')
    check_inno(b)
    print('selftest ok')


if __name__ == '__main__':
    main()
