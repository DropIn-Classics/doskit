#!/usr/bin/env python3
"""The GOG games installed on this computer: name, product ID, folder and
CD image.  new_project.py offers them to choose from.

    goglist.py

Where it looks: on Windows the registry (GOG.com\\Games\\ID, values
gameName and PATH); everywhere the folders GOG's installers and the usual
launchers put games in (on a Mac the applications in /Applications and
~/Applications), each game's folder holding GOG's goggame-ID.info (JSON:
gameId, name; on a Mac in Contents/Resources, with a dot in front).
GOG's older Mac applications (DOSBox or Boxer bundles, about 2012-2014)
carry no such file: an application whose Info.plist says it is GOG.com's
(BXOrganizationName, or an identifier com.gog.*) is taken by its
CFBundleName, without a product ID.
GOG's Windows installers not installed (setup_*.exe, Inno Setup; a game
GOG sells for Windows only comes as one) are listed too, from the home
folder, Downloads, Documents and Desktop: their name and ID from the
setup, their folder the setup itself (inno.py unpacks it).
$DOSKIT_GOG_DIRS (folders, separated as in PATH) is looked in instead of
all that.  An add-on (its rootGameId another game's) is left out.  The
image is a file below the game's folder holding an ISO 9660 file system:
one a cue sheet names, or one named *.gog, *.iso, *.bin, *.img, *.ins,
*.inst or *.dat (GOG's names vary; Boxer's own DummyCD.iso is not one);
games on floppies, or installed as a folder, have none.  The
registry's gameName is taken as GOG's installer is said to write it (not
checked on an installation; PATH is).
"""
import glob, json, os, plistlib, re, string, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import inno, isox


class Game:
    setup = None                            # an installer's path, not an installation

    def __init__(self, gid, name, folder):
        self.id, self.name, self.folder = gid, name, folder
        self.image = find_image(folder)

    def folder_name(self):
        """GOG's name of the folder ("My Game"), as the port's release has it"""
        return re.sub(r'\.app$', '', os.path.basename(os.path.normpath(self.folder)))

    def image_path(self):
        """the image's path in the game's folder ("game.gog", "CD/GAME.INS"), as
        the port's release names it; for a Mac application only its name,
        the Windows release's folder is not known from it"""
        if not self.image:
            return ''
        if self.folder.endswith('.app'):
            return os.path.basename(self.image)
        return os.path.relpath(self.image, self.folder).replace(os.sep, '/')

    def mac_bundle(self):
        """the image's path inside /Applications (or ~/Applications), else ''"""
        for apps in ('/Applications', os.path.expanduser('~/Applications')):
            if self.image and self.image.startswith(apps + os.sep):
                return os.path.relpath(self.image, apps).replace(os.sep, '/')
        return ''


class SetupGame(Game):
    """a game GOG's Windows installer holds: what it installs unknown
    until unpacked (unpack)"""

    def __init__(self, gid, name, setup):
        self.id, self.name, self.folder, self.setup, self.image = gid, name, setup, setup, None

    def folder_name(self):
        return self.name                    # GOG's installer names the folder so

    def unpack(self, folder):
        """the setup unpacked into folder, then taken for the installation"""
        inno.Setup(self.setup).unpack(folder)
        self.folder, self.image = folder, find_image(folder)


def setups(dirs, have):
    """[SetupGame] of GOG's installers (setup_*.exe) in dirs whose IDs are
    not among have"""
    out, seen = [], set(have)
    for d in dirs:
        for p in sorted(glob.glob(os.path.join(glob.escape(d), 'setup_*.exe'))):
            try:
                s = inno.Setup(p)
            except (inno.InnoError, OSError):
                continue
            gid = s.app_id if re.fullmatch(r'[0-9]+', s.app_id) else ''
            m = re.search(r'_\(([0-9]+)\)\.exe$', p)         # GOG's names end so
            gid = gid or (m.group(1) if m else '')
            if gid and gid in seen:
                continue
            seen.add(gid)
            out.append(SetupGame(gid, s.name, p))
    return out


def setup_dirs():
    env = os.environ.get('DOSKIT_GOG_DIRS')
    if env is not None:
        return [d for d in env.split(os.pathsep) if d]
    home = os.path.expanduser('~')
    return [d for d in [home] + [os.path.join(home, x) for x in ('Downloads', 'Documents', 'Desktop')]
            if os.path.isdir(d)]


def is_cd_image(path):
    try:
        return isox.Image(path).sector(16)[1:6] == b'CD001'
    except (OSError, ValueError, IndexError):
        return False


IMAGE_EXTS = ('.gog', '.iso', '.bin', '.img', '.ins', '.inst', '.dat')
# the empty CD Boxer's standalone bundles carry for themselves, no game's
NOT_IMAGES = ('dummycd.iso',)


def cue_files(cue):
    """the files a cue sheet names (FILE "name" BINARY), beside it"""
    try:
        text = open(cue, encoding='latin-1').read()
    except OSError:
        return []
    return [os.path.join(os.path.dirname(cue), m)
            for m in re.findall(r'^\s*FILE\s+"([^"]+)"', text, re.M | re.I)]


def find_image(folder):
    found = set()
    for d, dirs, files in os.walk(folder):
        for f in files:
            p = os.path.join(d, f)
            if f.lower() in NOT_IMAGES:
                continue
            if f.lower().endswith(IMAGE_EXTS):
                found.add(p)
            elif f.lower().endswith('.cue'):
                found.update(x for x in cue_files(p) if os.path.isfile(x))
    found = list(found)
    # a Mac release's DOSBox bundle may hold several: its CD folder's
    # first (Boxer's *.cdmedia), else the largest, the whole CD likely
    found.sort(key=lambda p: (not os.path.dirname(p).lower().endswith('.cdmedia'),
                              -os.path.getsize(p), p))
    return next((p for p in found if is_cd_image(p)), None)


def read_info(path):
    """(gameId, name) from a goggame-ID.info, None for an add-on or an unreadable one"""
    try:
        with open(path, encoding='utf-8') as f:
            info = json.load(f)
        gid, name = str(info['gameId']), str(info['name'])
    except (OSError, ValueError, KeyError):
        return None
    if info.get('rootGameId') and str(info['rootGameId']) != gid:
        return None
    return gid, name


def roots():
    env = os.environ.get('DOSKIT_GOG_DIRS')
    if env is not None:
        return [d for d in env.split(os.pathsep) if d]
    home = os.path.expanduser('~')
    r = ['/Applications', os.path.join(home, 'Applications'), os.path.join(home, 'GOG Games'),
         os.path.join(home, 'Games', 'Heroic'),
         os.path.join(home, '.wine', 'drive_c', 'GOG Games')]
    if sys.platform == 'win32':
        r += [f'{c}:\\GOG Games' for c in string.ascii_uppercase[2:]]
        pf = os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')
        r.append(os.path.join(pf, 'GOG Galaxy', 'Games'))
    return [d for d in r if os.path.isdir(d)]


def from_folders(dirs):
    """{id: (name, folder)} from the goggame-ID.info in each folder below `dirs`"""
    out = {}
    for root in dirs:
        for folder in sorted(glob.glob(os.path.join(glob.escape(root), '*'))):
            for where in (folder, os.path.join(folder, 'Contents', 'Resources')):
                try:
                    names = sorted(os.listdir(where))
                except OSError:
                    continue
                for n in names:
                    if not re.fullmatch(r'\.?goggame-\d+\.info', n):
                        continue
                    got = read_info(os.path.join(where, n))
                    if got and got[0] not in out:
                        out[got[0]] = (got[1], folder)
    return out


def gog_mac_app(folder):
    """the name of an application GOG.com made without a goggame-ID.info (its
    older Mac releases), else None"""
    try:
        with open(os.path.join(folder, 'Contents', 'Info.plist'), 'rb') as f:
            data = f.read()
    except OSError:
        return None
    # some of GOG's write <?xml version=1.0 ...?>, the DOCTYPE and <plist
    # version=1.0> without quotes, which macOS reads and expat does not:
    # what comes before the <dict> made plain
    if not data.startswith(b'bplist'):
        data = re.sub(rb'^.*?(?=<dict>)', b'<plist version="1.0">', data, count=1, flags=re.S)
    try:
        info = plistlib.loads(data)
    except Exception:   # not a plist (expat's errors are not ValueError)
        return None
    if not isinstance(info, dict):
        return None
    ident = str(info.get('CFBundleIdentifier', ''))
    if info.get('BXOrganizationName') != 'GOG.com' and not ident.startswith('com.gog.'):
        return None
    return str(info.get('CFBundleName') or os.path.basename(folder)[:-len('.app')])


def old_mac_apps(dirs, known):
    """[(name, folder)] of GOG's applications below `dirs` not among the `known` folders"""
    known = {os.path.normpath(f) for f in known}
    out = []
    for root in dirs:
        for folder in sorted(glob.glob(os.path.join(glob.escape(root), '*.app'))):
            name = None if os.path.normpath(folder) in known else gog_mac_app(folder)
            if name:
                out.append((name, folder))
    return out


def from_registry():
    """{id: (name, folder)} from GOG's registry keys (Windows)"""
    import winreg
    out = {}
    for base in (r'SOFTWARE\WOW6432Node\GOG.com\Games', r'SOFTWARE\GOG.com\Games'):
        try:
            games = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, base)
        except OSError:
            continue
        i = 0
        while True:
            try:
                gid = winreg.EnumKey(games, i)
            except OSError:
                break
            i += 1
            try:
                with winreg.OpenKey(games, gid) as k:
                    name = winreg.QueryValueEx(k, 'gameName')[0]
                    folder = winreg.QueryValueEx(k, 'PATH')[0]
            except OSError:
                continue
            if os.path.isdir(folder) and gid not in out:
                out[gid] = (name, folder)
    return out


def installed():
    """the installed games, by name"""
    found = {}
    if sys.platform == 'win32' and 'DOSKIT_GOG_DIRS' not in os.environ:
        found.update(from_registry())
    dirs = roots()
    for gid, v in from_folders(dirs).items():
        found.setdefault(gid, v)
    games = [Game(gid, name, folder) for gid, (name, folder) in found.items()]
    games += [Game('', name, folder)
              for name, folder in old_mac_apps(dirs, [f for _, f in found.values()])]
    games += setups(setup_dirs(), [g.id for g in games if g.id])
    return sorted(games, key=lambda g: g.name.lower())


def main():
    games = installed()
    for g in games:
        print(f'{g.id or "(no ID)":>12}  {g.name}\n              {g.folder}\n'
              f'              {"(Windows installer)" if g.setup else g.image or "(no CD image)"}')
    if not games:
        print('no GOG games found')


if __name__ == '__main__':
    main()
