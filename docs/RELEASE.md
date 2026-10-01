# Releases: what every port ships

Binding for every port made with the kit. A release that falls short of
this is not made; the point of a port is that the player gets a game
that starts with a double click, on every platform, the same way for
every game. The template's workflow (`template/.github/workflows/
build.yml`) builds exactly this; a project changes it only to add what
its game needs (another licence, a data file of its own), never to drop
a point below.

## For the player, on every platform

1. **Download, unpack, double click.** No installer, no Terminal, no
   command line, nothing else to install (no runtime, no SDL, no
   redistributable). The one step the platform insists on for a program
   not signed by a paid certificate (Windows' SmartScreen, macOS's
   Gatekeeper) is described in README.txt, in the platform's own words,
   and is needed once. So is keeping the download when a browser holds
   back a package that few have fetched yet (Chrome: a "dangerous" or
   "suspicious" download, as seen with a new build on Linux): on the
   organisation's website, where players find the packages, and in
   README.txt.
2. **One program.** Nothing beside it that the system could block or
   the player could lose, except where a platform leaves no choice
   (Linux: `libSDL2-2.0.so.0`).
3. **The game found by itself.** On the first start the port finds the
   player's GOG release (`cdimage.h`: registry, GOG's folders, the Mac
   app; on Linux GOG's Linux installer, `.sh`, with its menu entry,
   Heroic, Lutris, Minigalaxy, Bottles, Wine) and offers to copy the
   game's files; the setup screen comes first. If it is not found, the
   message says what to do, and the program looks where README.txt says
   a `game.gog` can be put. The offer, the copy's progress and the
   messages are the kit's dialog about the game's files, the same in
   every port (`launcher.h`: `launcher_offer_copy`, `launcher_no_game`,
   `launcher_copy_failed`, `launcher_copy_progress`; a window "The
   game's files" on the setup screen's backdrop, "Copy the files" or
   "Quit"). A port gives its names and says whether the CD's image and
   music are copied too; it does not draw a question of its own.
4. **Nothing written into the program's package.** Settings, saves and
   the copied game go into the user's data folder, `sys_data_dir`:
   `%LOCALAPPDATA%\NAME` on Windows, `~/.local/share/SLUG` (or
   `$XDG_DATA_HOME/SLUG`) on Linux, `~/Library/Application Support/NAME`
   on a Mac. The package's folder can so be replaced by a newer one
   without losing anything. What an earlier version wrote beside the
   program is moved there on the first start (`sys_data_migrate`).
5. **The version shown.** A release is a tag `vX.Y`; its packages carry
   it (`PORT_VERSION`: the setup screen's title bar, the Mac app's
   Info.plist).
6. **README.txt** (`port/dist/README.txt`, from the template), plain
   text for players, not developers: the wording of the provenance (a
   native compatibility implementation requiring an installed copy),
   how to start, where the game is looked for and where it can be put,
   the first-start step for each platform, the keys, the licences.
   Nothing in it that only a developer needs.
7. **Newer versions made known, on the player's say** (`update.h`). On
   the first start the setup screen asks once whether the program may
   look for new versions; nobody is made to. The answer can be changed
   on the setup screen later. With a yes, a release build (one with
   `PORT_VERSION`) fetches its release page's `latest.json` from GitHub
   at most once a day, in the background, and says nothing when that
   fails. When a newer version is there, the setup screen shows it with
   its notes, and a key opens its release page in the browser: the
   player downloads the package and puts its folder in place of the
   old one (point 4 keeps the settings, saves and game). Nothing but
   that one file is fetched and nothing is sent; README.txt says so,
   and how to switch it off.

## The packages

Named `SLUG-windows-x64.zip`, `SLUG-linux-x64.tar.gz`, `SLUG-macos.zip`,
each unpacking into one folder of the same name that holds:

| | Windows | Linux | macOS |
|---|---|---|---|
| program | `SLUG.exe` | `SLUG`, `libSDL2-2.0.so.0` | `SLUG.app` |
| README.txt | yes | yes | yes |
| licences | `LICENCE-micromod.txt` | `LICENCE-micromod.txt`, `LICENCE-SDL2.txt` | `LICENCE-micromod.txt`, `LICENCE-SDL2.txt` |

Beside the three packages each release carries `latest.json`, which
point 7 fetches through `https://github.com/OWNER/REPO/releases/latest/
download/latest.json` (a plain redirect: no API, no token, no rate
limit):

    {"version": "v1.3", "page": "https://github.com/OWNER/REPO/releases/tag/v1.3",
     "notes": "the tag's message, for players",
     "packages": {"windows-x64": {"file": "SLUG-windows-x64.zip", "sha256": "..."}, ...}}

The notes are the annotated tag's message (`git tag -a v1.3`), written
for players; a release is tagged that way. The builds get the address
as `PORT_UPDATE_URL`, always in the ports' organisation,
`https://github.com/DropIn-Classics/REPO/...`: a repository is moved
there at its first public release, and a release built before the move
must still find its successors. The Mac app's identifier is
`io.github.dropin-classics.SLUG` for the same reason.

- **Fetching**: WinHTTP on Windows (part of the system); the system's
  `curl` on macOS (always there) and Linux (there on most systems, the
  Steam Deck's among them; where it is missing nothing is looked for).
  No TLS library in the program.

- **Windows**: x64, MSVC, the C runtime linked in (`/MT`), the window
  subsystem (no console). SmartScreen: "More info", "Run anyway".
- **Linux**: x86_64, built on the oldest Ubuntu runner there is (older
  glibcs); SDL2 built from SDL's release source (its X11, Wayland and
  sound backends loaded only when present), put beside the program,
  whose search path is only `$ORIGIN`. Runs on the Steam Deck's
  read-only system (full screen there; a non-Steam game in Game Mode).
- **macOS**: one app, `SLUG.app`, x86_64 and arm64 in one program, for
  macOS 10.13 and newer; SDL2 built from SDL's release source as a
  static library and linked in (`SDL2_STATIC=1`, `runtime/
  sdl2-flags.sh`); the bundle made by `tools/macapp.py` and signed ad
  hoc as a whole. Never a framework or library beside the program: a
  download's quarantine reaches every unpacked file, and each unsigned
  piece of code is blocked on its own (SDL's release framework is signed
  ad hoc only). The first start: System Settings > Privacy & Security >
  "Open Anyway" (macOS 15 and newer; the right click "Open" no longer
  passes Gatekeeper there), a right click and "Open" on older ones, or
  `xattr -cr SLUG.app`.

## What the workflow checks

- Linux: the libraries the program links, the newest glibc version it
  needs, and with the build machine's SDL2 moved away that `ldd` finds
  the one beside the program.
- macOS: both architectures in the program, no SDL2 among the libraries
  it links, the bundle's signature valid (`codesign --verify --deep
  --strict`).

## Before a release is announced

Each package is downloaded from the release page with a browser (so
that it carries the quarantine or the mark of the web), unpacked with
the system's own tool and started with a double click on its platform,
with the GOG release installed where GOG puts it: the setup screen must
come up, the game's files copied, a game played; and the release
before it, with looking for new versions switched on, must show the new
one and open its page. Where that was not
done for a platform, the project's port/README.md says so.

## Not done yet

Signing with a paid certificate (Apple's Developer ID with
notarisation, Authenticode on Windows) would take away the first-start
step; it needs an account the user decides on. An AppImage or Flatpak
for Linux, a Windows ARM64 build and application icons are not part of
the standard yet.

A program that replaces itself (the package fetched, checked against
`latest.json`'s SHA-256, unpacked over the old one) is not either: on
Windows and Linux it could follow point 7; on a Mac an app started from
the download folder runs from a read-only copy (App Translocation), and
an update fetched by the program itself would pass Gatekeeper by, the
check the first start asks the player to pass. It waits for signing.
