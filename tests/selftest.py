#!/usr/bin/env python3
"""The kit's own check: the whole way from a program to its port, on a
program of our own (tests/hello/HELLO.ASM), so no game is needed.

    python3 tests/selftest.py

In build/selftest (a project as a game's would be, see kit.py):

  1. HELLO.ASM assembled and linked with tasm.py and tlink.py into
     game/HELLO/HELLO.EXE (the "shipped program");
  2. check.py: disasm.py makes its source from tests/hello/src/HELLO.hints,
     build.py rebuilds it byte for byte; PROVENANCE.md (the template's)
     is there; the names header symmap.py wrote is up to date;
  3. run.py: the program in the runner, stopped at its end (CODE:0026),
     its console line read, memory and video memory written out;
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


def make_exe():
    a = tasm.Assembler(os.path.join(HERE, 'hello', 'HELLO.ASM'))
    a.assemble()
    out = tlink.link([tlink.module_from_asm(a, 'HELLO')])
    exe = build.write_mz(out, None, [0])
    os.makedirs(os.path.join(PROJ, 'game', 'HELLO'))
    with open(os.path.join(PROJ, 'game', 'HELLO', 'HELLO.EXE'), 'wb') as f:
        f.write(exe)
    return len(exe)


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
    print(f'{make_exe()} bytes')
    shutil.copy(os.path.join(HERE, 'hello', 'src', 'HELLO.hints'), os.path.join(PROJ, 'src'))
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

    step('3. run in the runner (run.py)')
    b = os.path.join(PROJ, 'build')
    out = run([py, os.path.join(TOOLS, 'run.py'), '-until', '1', '-break', 'CODE:0026',
               '-dump', 'counter', '2', '-ram', os.path.join(b, 'orig.ram'),
               '-vram', os.path.join(b, 'orig.vram'), 'HELLO/HELLO.EXE'])
    print('\n'.join(l for l in out.splitlines() if l.startswith(('con:', 'break', 'dump'))))
    if 'con: hello from doskit' not in out:
        raise SystemExit('selftest FAILED: no console line from HELLO.EXE')

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
    out = subprocess.run([py, os.path.join(TOOLS, 'check.py')], cwd=new, capture_output=True,
                         text=True, env={k: v for k, v in os.environ.items() if k != 'DOSKIT_PROJECT'})
    print(out.stdout.strip())
    if out.returncode:
        raise SystemExit('selftest FAILED: check.py in the new project')

    print('selftest ok')


if __name__ == '__main__':
    main()
