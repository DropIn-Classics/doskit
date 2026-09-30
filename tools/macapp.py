#!/usr/bin/env python3
"""Make a port's macOS app: DIR/NAME.app around one program.

    macapp.py PROGRAM DIR --name NAME --id BUNDLE_ID [--version V]

PROGRAM is the port built for x86_64 and arm64 with SDL2 linked in
(runtime/sdl2-flags.sh with SDL2_STATIC=1): the bundle holds nothing
else, so there is no second piece of code for Gatekeeper to block
(docs/RELEASE.md).  NAME is the app's name and the program's in it,
BUNDLE_ID a reverse-DNS name ("io.github.owner.mygame"), V the release's
version (a leading "v" dropped; default 0).  An old NAME.app is
replaced.  On a Mac the bundle is signed ad hoc as a whole: the linker's
signature covers the program only, and Gatekeeper calls a bundle whose
Info.plist is not sealed damaged instead of offering to open it.
"""
import argparse, os, plistlib, shutil, subprocess, sys

MIN_MACOS = '10.13'         # arm64 programs start at 11 whatever is asked


def info(name, bundle_id, version):
    return {
        'CFBundleDevelopmentRegion': 'en',
        'CFBundleDisplayName': name,
        'CFBundleExecutable': name,
        'CFBundleIdentifier': bundle_id,
        'CFBundleInfoDictionaryVersion': '6.0',
        'CFBundleName': name,
        'CFBundlePackageType': 'APPL',
        'CFBundleShortVersionString': version,
        'CFBundleVersion': version,
        'LSApplicationCategoryType': 'public.app-category.games',
        'LSMinimumSystemVersion': MIN_MACOS,
        'NSHighResolutionCapable': True,
        'NSSupportsAutomaticGraphicsSwitching': True,
    }


def make(program, out, name, bundle_id, version):
    app = os.path.join(out, name + '.app')
    if os.path.isdir(app):
        shutil.rmtree(app)
    macos = os.path.join(app, 'Contents', 'MacOS')
    os.makedirs(macos)
    shutil.copy2(program, os.path.join(macos, name))
    with open(os.path.join(app, 'Contents', 'Info.plist'), 'wb') as f:
        plistlib.dump(info(name, bundle_id, version), f)
    with open(os.path.join(app, 'Contents', 'PkgInfo'), 'w') as f:
        f.write('APPL????')
    if sys.platform == 'darwin':
        subprocess.run(['codesign', '--force', '--sign', '-', app], check=True)
    return app


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('program')
    ap.add_argument('dir')
    ap.add_argument('--name', required=True)
    ap.add_argument('--id', required=True)
    ap.add_argument('--version', default='')
    a = ap.parse_args()
    version = a.version[1:] if a.version.startswith('v') else a.version
    print(make(a.program, a.dir, a.name, a.id, version or '0'))


if __name__ == '__main__':
    main()
