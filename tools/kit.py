"""Where things are, for all the tools: the project, its build folder and
the game's unpacked files.

The tools live in the kit and work on a project (a game being ported):

  * the project is $DOSKIT_PROJECT, else the nearest folder at or above
    the current directory that has a `src` folder with hints files in it,
    else the current directory;
  * the project's output goes to its `build/` folder (to be ignored by
    version control);
  * the game's files are $DOSKIT_GAME, else `game/` of the project, else
    `game/` of the project's main checkout (a git worktree has none of
    its own).  Nothing of them goes into a repository.
"""
import glob, os, subprocess

KIT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))


def project_root():
    env = os.environ.get('DOSKIT_PROJECT')
    if env:
        return os.path.abspath(env)
    d = os.getcwd()
    while True:
        if glob.glob(os.path.join(d, 'src', '*.hints')):
            return d
        up = os.path.dirname(d)
        if up == d:
            return os.getcwd()
        d = up


def build_dir(*parts):
    """a path in the project's build folder, which is made if needed"""
    d = os.path.join(project_root(), 'build')
    os.makedirs(d, exist_ok=True)
    return os.path.join(d, *parts)


def game_dir():
    env = os.environ.get('DOSKIT_GAME')
    if env:
        return env
    root = project_root()
    own = os.path.join(root, 'game')
    if os.path.isdir(own):
        return own
    try:
        common = subprocess.run(['git', 'rev-parse', '--git-common-dir'], cwd=root,
                                capture_output=True, text=True).stdout.strip()
        if common:
            main = os.path.normpath(os.path.join(root, common, '..', 'game'))
            if os.path.isdir(main):
                return main
    except OSError:
        pass
    return own


def hints_files():
    """the project's hints files, src/*.hints"""
    return sorted(glob.glob(os.path.join(project_root(), 'src', '*.hints')))
