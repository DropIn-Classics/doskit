#!/usr/bin/env python3
"""Start a new port: a repository made from the kit's template/.

    new_project.py DIR
    new_project.py DIR "Game Name" SLUG [--marker PATH] [--gog-id ID]
                   [--gog-folder NAME] [--gog-image PATH] [--mac-bundle PATH]
                   [--image FILE | --copy FOLDER]
    (both: [--kit URL] [--no-submodule])

DIR must not exist.  With DIR alone the GOG games installed here are
listed (goglist.py) and the one to port is chosen; its name, product ID,
folder and (on a Mac) the image's place in the application come from
the installation, the slug and the marker are asked for (the CD's
programs offered), and the CD image, if it has one, is unpacked into the
project's game/ when wanted; a game installed as a folder (no CD image:
from floppies, or a DOSBox folder) has that folder copied into game/
instead, its programs offered for the marker.

Otherwise "Game Name" is the game's title (in PROVENANCE.md, the README,
the window's title), SLUG a short lower-case name (the program's, the
data folder's on Linux: "mygame").  --marker is a file or folder the
game's files have ("GAME/GAME.EXE"), by which the port knows a folder of
them.  --gog-id is GOG's product ID of the game, the number in the
goggame-ID.info in the installed folder: on Windows the port finds the
installation by it in the registry.  --gog-folder is the installed
folder's name (default: the game's name), --gog-image the CD image's
path in it (default: game.gog), --mac-bundle the image's path inside
/Applications on a Mac.  --image FILE is unpacked into game/, --copy
FOLDER (an installed game's) copied into it.
What is not given is filled in later (port/src/main.c); a missing
--gog-id is said.

The template's placeholders are filled in: {{NAME}}, {{SLUG}}, {{ENV}}
(SLUG_GAME in upper case, the environment variable naming the game's
folder), {{MARKER}}, {{GOG_ID}}, {{GOG_FOLDER}}, {{GOG_IMAGE}},
{{MAC_BUNDLE}}, {{DATE}}.  Then `git init`, the hook enabled (core.hooksPath hooks) and
the kit added as the submodule `doskit` from --kit (default: the kit's
repository on GitHub, KIT_URL; a local folder works too, but its path
then stands in .gitmodules).  Nothing is committed.  Every project
carries PROVENANCE.md; check.py insists on it.
"""
import argparse, datetime, os, re, shutil, struct, subprocess, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from kit import KIT
import goglist, isox

TEMPLATE = os.path.join(KIT, 'template')
KIT_URL = 'https://github.com/mindphluxnet/doskit.git'


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


def ask(prompt, default=''):
    try:
        got = input(f'{prompt} [{default}]: ' if default else f'{prompt}: ').strip()
    except EOFError:
        raise SystemExit('\nstopped')
    return got or default


def cd_programs(image):
    """the programs on the CD image: its .EXE and .COM files' paths"""
    img = isox.Image(image)
    root = img.sector(16)[156:156 + 34]
    return [p for p, _, _, isdir in isox.walk(img, struct.unpack_from('<I', root, 2)[0],
                                               struct.unpack_from('<I', root, 10)[0])
            if not isdir and p.upper().endswith(('.EXE', '.COM'))]


def folder_programs(folder):
    """the programs in an installed game's folder: its .EXE and .COM files'
    paths in it"""
    out = []
    for d, dirs, files in os.walk(folder):
        dirs.sort()
        out += [os.path.relpath(os.path.join(d, f), folder).replace(os.sep, '/')
                for f in sorted(files) if f.upper().endswith(('.EXE', '.COM'))]
    return out


def pick_marker(progs):
    for i, p in enumerate(progs, 1):
        print(f'{i:3}  {p}')
    k = ask('marker, the file the game\'s folder is known by (number or path; Enter: later)')
    return progs[int(k) - 1] if k.isdigit() and 1 <= int(k) <= len(progs) else k


def choose(a):
    """a's name, slug, gog_id, gog_folder, mac_bundle, marker and image (or
    folder to copy) from an installed GOG game the user picks"""
    games = goglist.installed()
    if not games:
        raise SystemExit('no GOG games found here: name the game and its slug '
                         '(new_project.py DIR "Game Name" SLUG ...)')
    for i, g in enumerate(games, 1):
        print(f'{i:3}  {g.name}  ({g.id or "no GOG ID"}{"" if g.image else ", no CD image"})')
    while True:
        k = ask('the game to port (number)')
        if k.isdigit() and 1 <= int(k) <= len(games):
            break
    g = games[int(k) - 1]
    print(f'{g.name}: {g.folder}')
    a.name, a.gog_id, a.gog_folder = g.name, g.id, g.folder_name()
    a.gog_image, a.mac_bundle = g.image_path(), g.mac_bundle()
    a.slug = ask('slug (lower case: the program\'s name)',
                 re.sub(r'[^a-z0-9]+', '-', g.name.lower()).strip('-'))
    if g.image:
        print(f'CD image: {g.image}')
        a.marker = pick_marker(cd_programs(g.image))
        if ask('unpack the image into game/ (y/n)', 'y').lower().startswith('y'):
            a.image = g.image
    else:
        # installed as a folder (from floppies, or a DOSBox folder): all of it
        print('no CD image: the installed folder holds the game\'s files')
        a.marker = pick_marker(folder_programs(g.folder))
        if ask('copy the installed folder into game/ (y/n)', 'y').lower().startswith('y'):
            a.copy = g.folder


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('dir')
    ap.add_argument('name', nargs='?')
    ap.add_argument('slug', nargs='?')
    ap.add_argument('--marker', default='')
    ap.add_argument('--gog-id', default='')
    ap.add_argument('--gog-folder', default='')
    ap.add_argument('--gog-image', default='')
    ap.add_argument('--mac-bundle', default='')
    ap.add_argument('--image')
    ap.add_argument('--copy')
    ap.add_argument('--kit', default=KIT_URL)
    ap.add_argument('--no-submodule', action='store_true')
    a = ap.parse_args()
    dst = os.path.abspath(a.dir)
    if os.path.exists(dst):
        raise SystemExit(f'{dst} is there already')
    if a.name is None:
        choose(a)
    elif a.slug is None:
        raise SystemExit('the game\'s name and its slug, or neither (to choose from the '
                         'installed GOG games)')
    if not re.fullmatch(r'[a-z][a-z0-9_-]*', a.slug):
        raise SystemExit('SLUG: lower case letters, digits, - and _, starting with a letter')
    for what, v in (('the name', a.name), ('the GOG folder', a.gog_folder),
                    ('the GOG image', a.gog_image),
                    ('the Mac bundle', a.mac_bundle), ('the marker', a.marker)):
        if '{{' in v or '"' in v or '\\' in v:
            raise SystemExit(f'{what} cannot hold {{{{, " or \\')
    if a.gog_id and not re.fullmatch(r'[0-9]+', a.gog_id):
        raise SystemExit('--gog-id: the number in goggame-ID.info, digits only')
    if a.image and not goglist.is_cd_image(a.image):
        raise SystemExit(f'{a.image}: not a CD image')
    if a.image and a.copy:
        raise SystemExit('--image or --copy, not both')
    if a.copy and not os.path.isdir(a.copy):
        raise SystemExit(f'{a.copy}: not a folder')
    shutil.copytree(TEMPLATE, dst)
    fill(dst, {'NAME': a.name, 'SLUG': a.slug,
               'ENV': re.sub(r'[^A-Z0-9]', '_', a.slug.upper()) + '_GAME',
               'MARKER': a.marker, 'GOG_ID': a.gog_id, 'GOG_FOLDER': a.gog_folder or a.name,
               'GOG_IMAGE': a.gog_image, 'MAC_BUNDLE': a.mac_bundle, 'DATE': datetime.date.today().isoformat()})
    left = [os.path.relpath(os.path.join(d, f), dst) for d, _, fs in os.walk(dst) for f in fs
            if b'{{' in open(os.path.join(d, f), 'rb').read()
            and not f.endswith('.yml')]         # the workflow's ${{ ... }} are GitHub's
    if left:
        print('placeholders left in: ' + ', '.join(left))
    os.chmod(os.path.join(dst, 'hooks', 'pre-commit'), 0o755)
    git(dst, 'init', '-q', '-b', 'master')
    git(dst, 'config', 'core.hooksPath', 'hooks')
    print(f'{dst}: made from the template, git initialised, hook enabled')
    if not a.gog_id:
        print('no --gog-id: fill in GOG\'s product ID (the number in goggame-ID.info) in '
              'port/src/main.c\'s release, else Windows\' registry is not looked at')
    if not a.marker:
        print('no marker: fill it in in port/src/main.c')
    if a.image:
        print(f'{isox.unpack(a.image, os.path.join(dst, "game"))} files unpacked into game/')
    if a.copy:
        shutil.copytree(a.copy, os.path.join(dst, 'game'), symlinks=True)
        n = sum(len(fs) for _, _, fs in os.walk(os.path.join(dst, 'game')))
        print(f'{n} files copied into game/ from {a.copy}')
    if a.no_submodule:
        print('no submodule: add the kit as doskit/ yourself (git submodule add URL doskit)')
    else:
        # a local kit is added by its path: git allows that only when asked
        r = git(dst, '-c', 'protocol.file.allow=always', 'submodule', 'add', '-q', a.kit,
                'doskit')
        if r.returncode:
            print('the kit could not be added as a submodule:\n' + (r.stdout + r.stderr).strip() +
                  '\nadd it later: git submodule add URL doskit')
        else:
            print(f'doskit/ added as a submodule from {a.kit}')
    print('next: ' + ('' if a.image or a.copy else 'the game\'s files into game/ (python3 '
                      'doskit/tools/isox.py IMAGE), then ') + 'docs/HANDOFF.md, "Next"')


if __name__ == '__main__':
    main()
