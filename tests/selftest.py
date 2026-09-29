#!/usr/bin/env python3
"""The kit's own check: the whole way from a program to its port, on a
program of our own (tests/hello/HELLO.ASM), so no game is needed.

    python3 tests/selftest.py

In build/selftest (a project as a game's would be, see kit.py):

  1. HELLO.ASM assembled and linked with tasm.py and tlink.py into
     game/HELLO/HELLO.EXE (the "shipped program"); tests/flat/FLAT.ASM,
     32-bit, the same way into a pMAX image, build/files/FLAT.386 (as a
     project's tool would unpack it); tests/pmode/PMODE.ASM into
     game/PMODE/PMODE.EXE, tests/cdrom/CDROM.ASM into game/CDROM/CDROM.EXE,
     tests/vgamode/VGAMODE.ASM into game/VGAMODE/VGAMODE.EXE;
  2. check.py: disasm.py makes their sources from tests/hello/src/HELLO.hints
     and tests/flat/src/FLAT.hints, build.py rebuilds them byte for byte;
     PROVENANCE.md (the template's) is there; the names header symmap.py
     wrote is up to date; FLAT without its raw hint rebuilds too (the
     line tasm.py refuses written as DB by build.py);
  3. run.py: the program in the runner, stopped at its end (CODE:0026),
     its console line read, memory and video memory written out; PMODE.EXE
     in the runner, which checks the runner's protected mode itself (into
     it through INT 15h AH=89h, a #GP, IRQ0 through the IDT, ring 3, a
     call gate, V86 mode with the I/O bitmap, back to real mode; then its
     own file opened for writing, with no layer folder yet) and says
     "pmode ok"; CDROM.EXE, which checks the runner's MSCDEX (the drive,
     device requests, an audio play and stop; INT 21h AH=57h) and says
     "cdrom ok", on the default disc and on the tracks of a cue sheet
     (-cue: a data track and two audio tracks, WAVE and Ogg, made here),
     whose table the runner prints as expected; VGAMODE.EXE, which checks
     the runner's BIOS mode set (modes 0Dh and 0Eh planar at A0000h, back
     to text) and says "vgamode ok";
  4. the C runtime: tests/hello/port.c (HELLO in C over rmem.h and vga.h,
     on plat_null.c) built with cc, run on the same program; its memory
     compared with the runner's by memcmp.py (CODE, DATA and video memory;
     the stack is not the same and not compared);
  5. every runtime module compiled with warnings as errors (plat_sdl.c
     only when runtime/sdl2-flags.sh finds SDL2; plat_win32.c not here);
  6. new_project.py: a project made from template/ in build/selftest-new
     (the kit linked in as doskit/), its port built with its build.sh and
     run headless on HELLO's files; its check.py says all ok.

Prints `selftest ok` at the end, exit status 0 then.  Needs cc (clang
or gcc); on Windows it is not written for MSVC yet.
"""
import os, shutil, subprocess, sys

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


def run(cmd, **kw):
    r = subprocess.run(cmd, cwd=PROJ, capture_output=True, text=True,
                       env=dict(os.environ, DOSKIT_PROJECT=PROJ), **kw)
    out = (r.stdout + r.stderr).strip()
    if r.returncode:
        print(out)
        raise SystemExit(f'selftest FAILED: {" ".join(os.path.basename(c) for c in cmd[:2])}')
    return out


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


def make_cue():
    """a cue sheet as GOG writes them (Windows names, other case than
    the files) over a data track, a WAVE and an Ogg made here"""
    import struct
    d = os.path.join(PROJ, 'build', 'cue')
    os.makedirs(os.path.join(d, 'music'), exist_ok=True)
    with open(os.path.join(d, 'data.bin'), 'wb') as f:
        f.write(bytes(2352 * 300))
    with open(os.path.join(d, 'music', 'track02.wav'), 'wb') as f:
        n = 2352 * 150
        f.write(b'RIFF' + struct.pack('<I', 36 + n) + b'WAVEfmt ' +
                struct.pack('<IHHIIHH', 16, 1, 2, 44100, 44100 * 4, 4, 16) +
                b'data' + struct.pack('<I', n) + bytes(n))

    def page(granule, body):
        return b'OggS' + bytes(2) + struct.pack('<q', granule) + bytes(12) + b'\x01' + bytes([len(body)]) + body
    with open(os.path.join(d, 'music', 'track03.ogg'), 'wb') as f:
        f.write(page(0, b'\x01vorbis' + struct.pack('<IBI', 0, 2, 44100) + bytes(13)) +
                page(44100, bytes(40)))
    cue = os.path.join(d, 'game.inst')
    with open(cue, 'w') as f:
        f.write('FILE "DATA.BIN" BINARY\n\tTRACK 01 MODE2/2352\n\t INDEX 01 00:00:00\n'
                'FILE "MUSIC\\Track02.wav" WAVE\n\tTRACK 02 AUDIO\n\t INDEX 01 00:00:00\n'
                'FILE "MUSIC\\TRACK03.OGG" MP3\n\tTRACK 03 AUDIO\n\tPREGAP 00:02:00\n'
                '\t INDEX 01 00:00:00\n')
    return cue


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


def main():
    if os.path.isdir(PROJ):
        shutil.rmtree(PROJ)
    os.makedirs(os.path.join(PROJ, 'src'))
    os.makedirs(os.path.join(PROJ, 'port'))

    step('0. 32-bit instructions (tests/enc32)')
    print(f'{check_enc32()} lines as capstone reads them')

    step('1. HELLO.EXE assembled and linked')
    print(f'{make_exe()} bytes; FLAT.386 {make_flat()} bytes; PMODE.EXE {make_exe("PMODE")} bytes; '
          f'CDROM.EXE {make_exe("CDROM")} bytes; VGAMODE.EXE {make_exe("VGAMODE")} bytes')
    shutil.copy(os.path.join(HERE, 'hello', 'src', 'HELLO.hints'), os.path.join(PROJ, 'src'))
    shutil.copy(os.path.join(HERE, 'flat', 'src', 'FLAT.hints'), os.path.join(PROJ, 'src'))
    # every project carries PROVENANCE.md (check.py insists)
    with open(os.path.join(KIT, 'template', 'PROVENANCE.md'), 'rb') as f:
        text = f.read().replace(b'{{NAME}}', b'HELLO (the kit\'s test program)')
    with open(os.path.join(PROJ, 'PROVENANCE.md'), 'wb') as f:
        f.write(text)

    step('2. rebuilt from the hints (check.py)')
    py = sys.executable
    print(run([py, os.path.join(TOOLS, 'symmap.py'), 'port/hello_names.h', 'HELLO',
               'HELLO=src/HELLO.hints']))
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

    step('3. run in the runner (run.py)')
    b = os.path.join(PROJ, 'build')
    out = run([py, os.path.join(TOOLS, 'run.py'), '-until', '1', '-break', 'CODE:0026',
               '-dump', 'counter', '2', '-ram', os.path.join(b, 'orig.ram'),
               '-vram', os.path.join(b, 'orig.vram'), 'HELLO/HELLO.EXE'])
    print('\n'.join(l for l in out.splitlines() if l.startswith(('con:', 'break', 'dump'))))
    if 'con: hello from doskit' not in out:
        raise SystemExit('selftest FAILED: no console line from HELLO.EXE')
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
    out = run([py, os.path.join(TOOLS, 'run.py'), '-until', '1', 'VGAMODE/VGAMODE.EXE'])
    print('\n'.join(l for l in out.splitlines() if l.startswith('con:')))
    if 'con: vgamode ok' not in out:
        print(out)
        raise SystemExit('selftest FAILED: VGAMODE.EXE (the runner\'s BIOS mode set)')

    step('4. the C port over the runtime, compared (memcmp.py)')
    exe = os.path.join(b, 'port')
    srcs = [os.path.join(HERE, 'hello', 'port.c')] + [
        os.path.join(RUNTIME, f) for f in ('rmem.c', 'vga.c', 'sys.c', 'sha256.c', 'plat_null.c')]
    run([CC] + CFLAGS + ['-I', RUNTIME, '-I', os.path.join(PROJ, 'port'), '-o', exe] + srcs)
    out = subprocess.run(
        [exe, os.path.join(PROJ, 'game'), os.path.join(b, 'port.ram'), os.path.join(b, 'port.vram')],
        cwd=PROJ, capture_output=True, text=True,
        env=dict(os.environ, DK_DUMP=os.path.join(b, 'port.ppm')))
    print(out.stdout.strip())
    if out.returncode or 'con: hello from doskit' not in out.stdout:
        print(out.stderr)
        raise SystemExit('selftest FAILED: the port')
    print(run([py, os.path.join(TOOLS, 'memcmp.py'), 'src/HELLO.hints',
               os.path.join(b, 'orig.ram'), os.path.join(b, 'port.ram'), '--skip', 'STACK',
               '--vram', os.path.join(b, 'orig.vram'), os.path.join(b, 'port.vram')]))

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
         '--marker', 'HELLO/HELLO.EXE', '--no-submodule'])
    os.symlink(KIT, os.path.join(new, 'doskit'))
    shutil.copytree(os.path.join(PROJ, 'game'), os.path.join(new, 'game'))
    r = subprocess.run(['sh', os.path.join(new, 'port', 'build.sh')], capture_output=True, text=True)
    if r.returncode or 'warning' in r.stderr:
        print(r.stdout + r.stderr)
        raise SystemExit('selftest FAILED: the template\'s port/build.sh')
    r = subprocess.run([os.path.join(new, 'port', 'build', 'testgame-headless')], cwd=new,
                       capture_output=True, text=True, env=dict(os.environ, DK_FRAMES='3'))
    if r.returncode:
        print(r.stdout + r.stderr)
        raise SystemExit('selftest FAILED: the template\'s port did not find the game')
    # named, as the folder above (the kit inside a project) may be a project too
    out = subprocess.run([py, os.path.join(TOOLS, 'check.py')], cwd=new, capture_output=True,
                         text=True, env=dict(os.environ, DOSKIT_PROJECT=new))
    print(out.stdout.strip())
    if out.returncode:
        raise SystemExit('selftest FAILED: check.py in the new project')

    print('selftest ok')


if __name__ == '__main__':
    main()
