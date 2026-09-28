#!/usr/bin/env python3
"""Start a new port: a repository made from the kit's template/.

    new_project.py DIR "Game Name" SLUG [--marker PATH] [--kit URL] [--no-submodule]

DIR must not exist.  "Game Name" is the game's title (in PROVENANCE.md,
the README, the window's title), SLUG a short lower-case name (the
program's, the data folder's on Linux: "mygame").  --marker is a file or
folder the game's files have ("GAME/GAME.EXE"), by which the port knows
a folder of them; it can be filled in later (port/src/main.c).

The template's placeholders are filled in: {{NAME}}, {{SLUG}}, {{ENV}}
(SLUG_GAME in upper case, the environment variable naming the game's
folder), {{MARKER}}, {{DATE}}.  Then `git init`, the hook enabled
(core.hooksPath hooks) and the kit added as the submodule `doskit` from
--kit (default: the kit's repository on GitHub, KIT_URL; a local folder
works too, but its path then stands in .gitmodules).  Nothing is
committed.  Every project carries PROVENANCE.md; check.py insists on it.
"""
import argparse, datetime, os, re, shutil, subprocess, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from kit import KIT

TEMPLATE = os.path.join(KIT, 'template')
KIT_URL = 'git@github.com:mindphluxnet/doskit.git'


def fill(dst, values):
    for d, dirs, files in os.walk(dst):
        dirs[:] = [x for x in dirs if x != '.git']
        for f in files:
            p = os.path.join(d, f)
            data = open(p, 'rb').read()
            new = data
            for k, v in values.items():
                new = new.replace(('{{' + k + '}}').encode(), v.encode())
            if new != data:
                with open(p, 'wb') as fh:      # bytes: line ends stay as they are
                    fh.write(new)


def git(dst, *args):
    return subprocess.run(['git'] + list(args), cwd=dst, capture_output=True, text=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('dir')
    ap.add_argument('name')
    ap.add_argument('slug')
    ap.add_argument('--marker', default='')
    ap.add_argument('--kit', default=KIT_URL)
    ap.add_argument('--no-submodule', action='store_true')
    a = ap.parse_args()
    if not re.fullmatch(r'[a-z][a-z0-9_-]*', a.slug):
        raise SystemExit('SLUG: lower case letters, digits, - and _, starting with a letter')
    if '{{' in a.name or '"' in a.name or '\\' in a.name:
        raise SystemExit('the name cannot hold {{, " or \\')
    dst = os.path.abspath(a.dir)
    if os.path.exists(dst):
        raise SystemExit(f'{dst} is there already')
    shutil.copytree(TEMPLATE, dst)
    fill(dst, {'NAME': a.name, 'SLUG': a.slug,
               'ENV': re.sub(r'[^A-Z0-9]', '_', a.slug.upper()) + '_GAME',
               'MARKER': a.marker, 'DATE': datetime.date.today().isoformat()})
    left = [os.path.relpath(os.path.join(d, f), dst) for d, _, fs in os.walk(dst) for f in fs
            if b'{{' in open(os.path.join(d, f), 'rb').read()
            and not f.endswith('.yml')]         # the workflow's ${{ ... }} are GitHub's
    if left:
        print('placeholders left in: ' + ', '.join(left))
    os.chmod(os.path.join(dst, 'hooks', 'pre-commit'), 0o755)
    git(dst, 'init', '-q', '-b', 'master')
    git(dst, 'config', 'core.hooksPath', 'hooks')
    print(f'{dst}: made from the template, git initialised, hook enabled')
    if a.no_submodule:
        print('no submodule: add the kit as doskit/ yourself (git submodule add URL doskit)')
        return
    # a local kit is added by its path: git allows that only when asked
    r = git(dst, '-c', 'protocol.file.allow=always', 'submodule', 'add', '-q', a.kit, 'doskit')
    if r.returncode:
        print('the kit could not be added as a submodule:\n' + (r.stdout + r.stderr).strip() +
              '\nadd it later: git submodule add URL doskit')
    else:
        print(f'doskit/ added as a submodule from {a.kit}')
    print('next: unpack the game into game/ (python3 doskit/tools/isox.py IMAGE), '
          'then docs/HANDOFF.md, "Next"')


if __name__ == '__main__':
    main()
