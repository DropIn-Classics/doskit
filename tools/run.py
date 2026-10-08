#!/usr/bin/env python3
"""Run a shipped program on the headless runner (tools/run, the kit's
build/dosrun[.exe]) with addresses given by their names in the hints.

    run.py [options] PROGRAM [ARGS]
    run.py -until 20 -key 3 f1 -break CODE:1251 -dump DATA:9AA0 2 GAME/GAME.EXE 1

PROGRAM is a path in the game's files (GAME/GAME.EXE); the project's hints
file whose `exe` is that program gives the names: a loader and the image
it loads share their base name (LOADER.EXE and its `pmax` IMAGE.386), so
the image's hints are found for the loader too.  The options are the
runner's (see the top of tools/run/main.c); run.py only
  * finds the game's files as the other tools do (kit.py) and passes -game,
    and the project's build/run/state as -state,
  * builds the runner when it is missing or older than its sources,
  * takes -base LIN, run.py's own option (not the runner's): the linear
    address a `pmax`, `le` or `bin` image is loaded at, for the translation
    below (the runner only resolves an MZ program's load segment; an
    image's base is fixed by the emulated loader, e.g. 100F30h),
  * translates the ADDR of -break, -log, -watch, -rwatch, -dump and -poke (both
      of its addresses) when it is
      SEG:OFF   with SEG a segment of the hints (CODE:4CEE, DATA:8A8A),
      a label   of the generated source (L4CEE, D8A8A, C4F05) or a `name`
                or `code` name of the hints,
    segment offsets accept one through eight hex digits; generated label
      offsets accept four through eight, including 32-bit image offsets,
    optionally with +N (hex) added: DATA:9A9A+4.  PROG:ADDR takes the
      names of another program's hints, one PROGRAM starts (a menu that
      runs the game: GAME.EXE:main_loop#100). LE image names translate to
      -base LIN plus the object's base and offset; pMAX and raw image names
      translate to -base LIN plus their offset. Without -base run.py says
      so instead of passing the name on to the runner's "bad address".
    An address in the runner's
    own form (a linear address, SEG:OFF with a hex segment, PROG+SEG:OFF)
    is passed on as it is.
"""
import os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from disasm import Hints, load_program
from kit import KIT, build_dir, game_dir, hints_files

EXE = os.path.join(KIT, 'build', 'dosrun.exe' if os.name == 'nt' else 'dosrun')
SRC = os.path.join(HERE, 'run')
ADDR_OPTS = {'-break': 1, '-log': 1, '-watch': 1, '-rwatch': 1, '-dump': 1, '-poke': 2, '-keyat': 1, '-keysat': 1}
# options and how many arguments they take (to find PROGRAM)
OPTS = {'-game': 1, '-state': 1, '-put': 2, '-until': 1, '-ips': 1, '-key': 2, '-keys': 1,
        '-mouse': 2, '-mice': 1, '-joy': 2, '-joys': 1,
        '-shot': 2, '-shotevery': 2, '-break': 1, '-log': 1, '-watch': 1, '-rwatch': 2, '-trace': 2,
        '-dump': 2, '-poke': 3, '-keyat': 2, '-keysat': 2, '-dumpevery': 1, '-ram': 1, '-mem': 1, '-vram': 1, '-dac': 1, '-wav': 1, '-dos': 0, '-cd': 0, '-cue': 1, '-cdwav': 1, '-oplwav': 1, '-loadfix': 0, '-intwatch': 1, '-prof': 0, '-cover': 1, '-vgastate': 0,
        '-sb': 0, '-v': 0, '-criterr': 2, '-requirements': 0}


def build():
    """Build the runner when a source is newer than the program."""
    # its own sources and the runtime's synthesizer it compiles in
    srcs = [os.path.join(SRC, f) for f in os.listdir(SRC)]
    srcs += [os.path.join(KIT, 'runtime', f) for f in ('opl.c', 'opl.h')]
    newest = max(os.path.getmtime(f) for f in srcs)
    if os.path.exists(EXE) and os.path.getmtime(EXE) >= newest:
        return
    if os.name == 'nt':
        cmd = ['cmd', '/c', os.path.join(SRC, 'build.bat')]
    else:
        cmd = ['sh', os.path.join(SRC, 'build.sh')]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        sys.stdout.write(r.stdout + r.stderr)
        raise SystemExit('run.py: the runner did not build')


def same_program(exe, program):
    """the hints' program is this program: the same path, or the same base
    name without its extension (a loader and the image it loads,
    LOADER.EXE and its `pmax` IMAGE.386)"""
    exe = exe.replace('\\', '/').upper()
    want = program.replace('\\', '/').upper()
    if exe == want:
        return True
    return os.path.splitext(os.path.basename(exe))[0] == os.path.splitext(os.path.basename(want))[0]


def hints_for(program, base_only=False):
    """The hints file whose `exe` is this program (or, base_only, whose
    exe has this base name), or None.  A loader finds its image's hints
    too (see same_program)."""
    for f in hints_files():
        h = Hints(f)
        exe = (h.exe or '').replace('\\', '/').upper()
        if exe and (exe == program.replace('\\', '/').upper() or
                    base_only and os.path.basename(exe) == os.path.basename(program.replace('\\', '/').upper())):
            return h
    for f in hints_files():
        h = Hints(f)
        if (h.exe or '') and same_program(h.exe, program):
            return h
    return None


def translate(names, t, base=None):
    """ADDR or PROG:ADDR -> the runner's form"""
    m = re.fullmatch(r'(\w+\.\w+):(.+)', t, re.I)
    if m:
        h = hints_for(m.group(1), base_only=True)
        if h:
            return Names(h, h.exe, base).translate(m.group(2))
    return names.translate(t) if names else t


class Names:
    """Resolves a name of the hints or the generated source to (segment, offset)."""

    def __init__(self, hints, program, base=None):
        self.h = hints
        self.base = os.path.basename(program.replace('\\', '/')).upper()
        self.image_base = base    # -base LIN: where a flat image is loaded
        self.object_bases = {}
        if hints.kind == 'le':
            image = load_program(hints)
            self.object_bases = {s.frame: image.descs[s.frame][0] for s in hints.segs}
        self.segs = {s.name: s for s in hints.segs}
        self.byname = {}
        for key, name in hints.names.items():
            self.byname[name.upper()] = key
        for s, o, name in hints.code:
            if name:
                self.byname[name.upper()] = (s, o)

    def lookup(self, t):
        m = re.fullmatch(r'([A-Za-z_]\w*):([0-9A-Fa-f]{1,8})', t)
        if m and m.group(1).upper() in self.segs:
            return m.group(1).upper(), int(m.group(2), 16)
        if t.upper() in self.byname:
            return self.byname[t.upper()]
        for s in self.h.segs:              # a label of a segment with prefix=
            if s.own_prefix:
                m = re.fullmatch(r'L?' + re.escape(s.prefix) + r'([0-9A-Fa-f]{4,8})', t, re.I)
                if m:
                    return s.name, int(m.group(1), 16)
        m = re.fullmatch(r'(L?)([A-Za-z])([0-9A-Fa-f]{4,8})', t)
        if m:
            code, letter, off = m.group(1), m.group(2).upper(), int(m.group(3), 16)
            if not code and letter == 'L':
                return 'CODE', off                      # L1234: code in CODE
            segs = [s for s in self.h.segs if s.prefix.upper() == letter]
            if len(segs) == 1:
                return segs[0].name, off
        return None

    def translate(self, t):
        """ADDR[#N] as run.py takes it -> the runner's form, or t unchanged."""
        count = ''
        if '#' in t:
            t, count = t.split('#', 1)
            count = '#' + count
        add = 0
        m = re.fullmatch(r'(.+?)\+([0-9A-Fa-f]+)', t)
        if m and '+' not in m.group(1) and self.lookup(m.group(1)):
            t, add = m.group(1), int(m.group(2), 16)
        r = self.lookup(t)
        if r is None:
            return t + count
        seg, off = r
        if self.h.kind == 'mz':
            return f'{self.base}+{self.segs[seg].frame:04X}:{(off + add) & 0xFFFF:04X}{count}'
        if self.image_base is None:
            raise SystemExit(f'run.py: {t} is in {self.h.path} (a {self.h.kind} image, '
                             f'loaded at no fixed address): pass a linear address, or -base LIN')
        object_base = self.object_bases.get(self.segs[seg].frame, 0)
        return f'{(self.image_base + object_base + off + add):X}{count}'


def main():
    args = sys.argv[1:]
    base = None
    if '-base' in args:           # run.py's own, not the runner's (so not in OPTS)
        i = args.index('-base')
        try:
            base = int(args[i + 1], 16)
        except (IndexError, ValueError):
            raise SystemExit('run.py: -base takes a hex address (the image\'s linear base)')
        del args[i:i + 2]
    # find PROGRAM: the first argument that is not an option or an option's argument
    i, prog, pi = 0, None, len(args)
    while i < len(args):
        a = args[i]
        if a in OPTS:
            i += 1 + OPTS[a]
            continue
        prog, pi = a, i
        break
    if prog is None:
        raise SystemExit(__doc__)
    hints = hints_for(prog)
    names = Names(hints, prog, base) if hints else None
    out = []
    i = 0
    while i < len(args):
        a = args[i]
        if i < pi and a in OPTS:
            n = OPTS[a]
            vals = args[i+1:i+1+n]
            if a in ADDR_OPTS:
                for k in range(ADDR_OPTS[a]):
                    vals[k] = translate(names, vals[k], base)
            out += [a] + vals
            i += 1 + n
        else:
            out.append(a)
            i += 1
    if '-game' not in out:
        out = ['-game', game_dir()] + out
    if '-state' not in out:
        out = ['-state', build_dir('run', 'state')] + out
    build()
    sys.stdout.flush()
    sys.exit(subprocess.run([EXE] + out).returncode)


if __name__ == '__main__':
    main()
