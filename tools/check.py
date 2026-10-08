#!/usr/bin/env python3
"""Everything that must hold before a commit of a project.

    check.py [--fresh] [HINTS...]

  * every program with a hints file in src/ (or the ones named) rebuilds
    byte for byte (build.py). A rebuild that held is remembered in
    build/check/ with a hash of what it depends on: the hints file, the
    files it names (the program, an offrel list) and the kit's tools.
    While none of them changed, the program is not built again (`ok ...
    (cached)`); --fresh builds every one;
  * every hints file with a block carried over by xfer.py has it up to
    date;
  * every header written by symmap.py (found by its first line, anywhere
    in the project but build/ and game/) is up to date;
  * PROVENANCE.md is there (from doskit's template/, which every project
    using the kit carries) with the game's name filled in;
  * the project's own checks: tools/check_project.py of the project, when
    there is one (exit status 0 when all holds, its last line printed).

Exit status 0 when all holds, and the last line says `all ok`.  The
pre-commit hook (hooks/pre-commit) runs it."""
import glob, hashlib, os, re, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from kit import build_dir, hints_files, project_root
import symmap, xfer

SKIP_DIRS = {'build', 'game', '.git', '__pycache__'}


def run(args):
    r = subprocess.run([sys.executable] + args, cwd=project_root(), capture_output=True, text=True)
    return r.returncode, (r.stdout + r.stderr).strip()


def report(bad, what, rc, out):
    last = out.splitlines()[-1] if out else ''
    if rc:
        bad.append(what)
        print(f'FAIL {what}\n' + '\n'.join('     ' + l for l in out.splitlines()[-15:]))
    else:
        print(f'ok   {last}')


def build_key(hints):
    """a hash of what a rebuild from these hints depends on"""
    from disasm import program_path
    h = hashlib.sha256()
    files = [hints]
    for line in open(hints, encoding='utf-8'):
        f = line.split(';', 1)[0].split()
        if len(f) > 1 and f[0] in ('exe', 'pmax', 'bin', 'le', 'offrel'):
            files.append(program_path(f[1]))
    files += sorted(glob.glob(os.path.join(HERE, '*.py')))
    for path in files:
        h.update(os.path.basename(path).encode() + b'\0')
        with open(path, 'rb') as fh:
            h.update(hashlib.sha256(fh.read()).digest())
    return h.hexdigest()


def cached(hints, key):
    """the verdict of the last rebuild that held with this key, or None"""
    try:
        with open(build_dir('check', os.path.basename(hints)), encoding='utf-8') as f:
            was, verdict = f.read().split('\n', 1)
    except (OSError, ValueError):
        return None
    return verdict.strip() if was == key else None


def remember(hints, key, verdict):
    path = build_dir('check', os.path.basename(hints))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8') as f:
        f.write(key + '\n' + verdict + '\n')


def provenance(root):
    """None when PROVENANCE.md is there and filled in, else what is wrong"""
    path = os.path.join(root, 'PROVENANCE.md')
    if not os.path.exists(path):
        return 'PROVENANCE.md missing (copy doskit/template/PROVENANCE.md, fill in the game)'
    text = open(path, encoding='utf-8', errors='replace').read()
    if '{{' in text:
        return 'PROVENANCE.md has a placeholder left ({{...}})'
    return None


def carried_blocks(hints):
    """(from, to) for each hints file with a block of xfer.py"""
    out = []
    for h in hints:
        for line in open(h, encoding='utf-8'):
            if line.startswith(xfer.MARK):
                src = line[len(xfer.MARK):].split()[0]
                out.append((os.path.join(os.path.dirname(h), src), h))
                break
    return out


def symmap_headers(root):
    """the argument lists of the headers symmap.py wrote"""
    out = []
    for d, dirs, files in os.walk(root):
        dirs[:] = [x for x in dirs if x not in SKIP_DIRS]
        for f in files:
            if f.endswith('.h'):
                with open(os.path.join(d, f), encoding='utf-8', errors='replace') as fh:
                    first = fh.readline()
                m = re.match(r'/\* ' + re.escape(symmap.TAG) + r' (.*)$', first.rstrip())
                if m:
                    out.append(m.group(1).split())
    return out


def main():
    t0 = time.time()
    root = project_root()
    fresh = '--fresh' in sys.argv[1:]
    named = [a for a in sys.argv[1:] if a != '--fresh']
    hints = named or hints_files()
    bad = []
    for h in hints:
        name = os.path.basename(h)
        key = build_key(h)
        verdict = None if fresh else cached(h, key)
        if verdict:
            print(f'ok   {name}: {verdict} (cached)')
            continue
        rc, out = run([os.path.join(HERE, 'build.py'), h])
        # build.py's verdict line; a list of instructions written as DB
        # may follow it
        last = next((l for l in reversed(out.splitlines())
                     if ': IDENTICAL (' in l or ': differs (' in l), '')
        if rc or 'IDENTICAL' not in last:
            bad.append(name)
            print(f'FAIL {name}\n' + '\n'.join('     ' + l for l in out.splitlines()[-15:]))
        else:
            print(f'ok   {name}: {last.split(";")[0]}')
            remember(h, key, last.split(';')[0])
    if not named:
        why = provenance(root)
        report(bad, 'PROVENANCE.md', 1 if why else 0, why or 'PROVENANCE.md there and filled in')
        for src, dst in carried_blocks(hints):
            rc, out = run([os.path.join(HERE, 'xfer.py'), src, dst, '--check'])
            report(bad, f'{os.path.basename(dst)} (carried block)', rc,
                   out if rc else f'{os.path.basename(dst)} carried block up to date')
        for args in symmap_headers(root):
            rc, out = run([os.path.join(HERE, 'symmap.py')] + args + ['--check'])
            report(bad, f'{args[0]} (symmap.py)', rc, out)
        own = os.path.join(root, 'tools', 'check_project.py')
        if os.path.exists(own):
            rc, out = run([own])
            report(bad, 'tools/check_project.py', rc, out)
    print(f'{"FAILED: " + ", ".join(bad) if bad else "all ok"} ({time.time() - t0:.0f} s)')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
