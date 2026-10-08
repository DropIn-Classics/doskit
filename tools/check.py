#!/usr/bin/env python3
"""Everything that must hold before a commit of a project.

    check.py [HINTS...]

  * every program with a hints file in src/ (or the ones named) rebuilds
    byte for byte (build.py);
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
import os, re, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from kit import hints_files, project_root
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
    hints = sys.argv[1:] or hints_files()
    bad = []
    for h in hints:
        rc, out = run([os.path.join(HERE, 'build.py'), h])
        # build.py's verdict line; a list of instructions written as DB
        # may follow it
        last = next((l for l in reversed(out.splitlines())
                     if ': IDENTICAL (' in l or ': differs (' in l), '')
        name = os.path.basename(h)
        if rc or 'IDENTICAL' not in last:
            bad.append(name)
            print(f'FAIL {name}\n' + '\n'.join('     ' + l for l in out.splitlines()[-15:]))
        else:
            print(f'ok   {name}: {last.split(";")[0]}')
    if not sys.argv[1:]:
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
