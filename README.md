# doskit

Tools and a C runtime for porting DOS games: from the shipped programs
to readable C that runs natively and needs the player's own installed
copy of the game for its data. Made from the parts of an earlier port
that are not about one game, so the next port does not start from
nothing.

The kit holds no game's code or data, and a port made with it holds none
either: hints files hold addresses, names and comments; the source is
made from the player's copy each time; the port loads the player's files
at run time.

## The method in short

1. **Source from the programs.** `tools/disasm.py` turns a shipped MZ
   program into assembly source that `tools/tasm.py` and `tools/tlink.py`
   assemble and link back to the same bytes. What the analysis cannot see
   by itself (entry points reached through pointers, tables, names,
   comments) goes into a hints file; `build.py` says, line by line, where
   the result differs.
2. **Understanding.** Names, records and rules go into the hints, checked
   by running the original in `tools/run` (`dosrun`), a headless,
   deterministic PC.
3. **The port.** C translated routine by routine, first over the
   program's own memory image (`runtime/rmem.h`, `runtime/vga.h`), so the
   port's memory can be compared with the original's byte for byte at
   the same point (`tools/memcmp.py`); on a platform layer (window, sound,
   keyboard, controllers) for Windows, macOS and Linux.

docs/METHOD.md says how each step goes and what was learned doing it;
docs/NEW-PROJECT.md how to start a new game.

## Layout

- `tools/` (Python 3 with `capstone`; run from the project's folder, see
  `kit.py`):
  - `disasm.py`: analysis and source generator (the hints syntax is in
    its docstring), for MZ programs, pMAX (a DOS extender's) 32-bit
    flat images and raw 32-bit images (`bin`: a driver loaded into a
    segment of its own, or a module its host relocates by a list of
    offsets, `offrel`); `build.py`: generate, assemble, link, compare;
    `tasm.py`, `x86enc.py`, `tlink.py`: assembler and linker work-alikes
    (with switches for other assemblers' encodings, the `asm` hint).
  - `gaps.py`: what of a code segment is not reached yet, and where the
    gaps' addresses appear (with `--cover`, which of the gaps ran in a
    run with the runner's `-cover`); `ptrscan.py`: immediates that look like
    addresses; `cv4.py`: CodeView (NB08) debug information to `name`
    hints; `xfer.py`: carries hints to a second program of the same
    engine.
  - `check.py`: what must hold before a project's commit (every program
    identical, carried hints and name headers up to date);
    `symmap.py`: the hints' names as a C header for the port.
  - `run/`: `dosrun`, the headless PC (386 real, protected and V86 mode, VGA, PIT/PIC,
    keyboard, BIOS, DMA and Sound Blaster 16, MSCDEX with CD audio to a
    WAV file, a small DOS over the game's files with a writable layer); `run.py` its front end, which takes
    addresses by their names in the hints. `memcmp.py` compares a port's
    memory with the original's (`-ram`, or `-mem` and `--base` for a
    protected-mode image).
  - `isox.py`: unpacks a CD image (ISO, or raw 2352-byte sectors as GOG's
    game.gog); `inno.py`: lists and unpacks an Inno Setup installer (GOG's
    Windows setup_*.exe, with its .bin files; GOG Galaxy's parts put back
    together); `flifiles.py`: reads and checks Autodesk FLI animations.
  - `new_project.py`: a new port's repository from `template/`, for a
    game chosen from those installed from GOG (`goglist.py` lists them:
    name, product ID, folder, CD image).
  - `macapp.py`: the macOS release's app around the port's program
    (docs/RELEASE.md).
- `third_party/`: code taken unchanged from elsewhere, each with a
  README saying from where and under which licence: `stb_vorbis` (Ogg
  Vorbis decoding, public domain or MIT).
- `runtime/` (C99; a project compiles the files it needs):
  - `platform.h` with `plat_sdl.c` (SDL2: macOS, Linux, Steam Deck),
    `plat_win32.c` (Windows SDK only) and `plat_null.c` (headless, for
    tests and scripted runs); `sdl2-flags.sh` finds SDL2 (`SDL2_STATIC=1`:
    its static library, for the macOS release).
  - `rmem.h`: the program's memory as under DOS (MZ loader with a
    SHA-256 check, DOS memory blocks); `pmem.h`: a protected-mode
    program's 16 MB of linear memory (pMAX loader with a SHA-256 check,
    the selectors the extender gives); `vga.h`: a VGA as the programs
    program it (ports, planes, Mode X, mode 12h/13h, the picture scanned
    out); `frame.h`: pacing at the mode's refresh, keys to the program's
    INT 9 handler, a tick callback, a key recorder.
  - `modplay.h` (MOD playback, micromod), `opl.h` (an OPL2's FM synthesis,
    the AdLib's chip, from the program's register writes; the runner's
    -oplwav too), `audiofx.h` (optional EQ and a
    headphone mode), `fli.h` (FLI animations), `textmode.h` (an 80x25
    text screen, for a DOS-setup-like launcher), `pad.h` (controller
    buttons to keys), `cdimage.h` (a GOG release found and its image
    unpacked, or its installed folder copied), `inno.h` (GOG's Windows
    installer found and unpacked, for a game GOG sells for Windows only;
    the LZMA SDK's decoders, public domain), `sys.h` (files, folders, the data folder), `sha256.h`,
    `shot.h` (screenshots as PNG: Print Screen, `DK_SHOTS` headless),
    `update.h` (newer releases made known, on the player's say).
- `template/`: the start of a new port's repository (rules, hook, build
  scripts, a C skeleton, the workflow that builds the releases and the
  player's README.txt as docs/RELEASE.md says, `.claude/settings.json`
  that switches on the kit's plugin from the project's `doskit/`).
- `.claude-plugin/`, `agents/`: the kit as a Claude Code plugin, so
  that every project has the same subagents without a copy of them, on
  a cheaper model for the repetitive parts: `collector` (stage 1/2 data
  collection), `cmd-digest` (a command with long output run, only its
  result reported), `git-committer`. In a session they are
  `doskit:collector` and so on. A project names the kit's folder as the
  marketplace (`"source": "directory", "path": "./doskit"`), so the
  agents are those of its submodule's state; they are there from the
  second session on (the first makes the marketplace known).
- `tests/`: `selftest.py`, the whole way on a program of our own
  (`tests/hello`), no game needed; a program in many code segments
  with TLINK's header (`tests/multiseg`); the 32-bit encoder (`tests/enc32`) and
  a pMAX image rebuilt (`tests/flat`).

## Use

    python3 tests/selftest.py            # the kit checks itself
    python3 tools/new_project.py ~/mygame                    # choose an installed GOG game
    python3 tools/new_project.py ~/mygame "My Game" mygame   # or name it

On Windows `python` for `python3`; the runner builds with MSVC
(`tools/run/build.bat`) or cc (`build.sh`). `pip install capstone`.
`tests/selftest.py` itself needs a Unix-style cc (its CFLAGS are gcc/clang
flags, not MSVC's) and Unix tools (`sh`), so it does not run under plain
Windows: use WSL (`wsl --install` once, then a distribution's Python 3
with `pip install capstone`, its own gcc or clang, `sh` and `cc`/`make`
already there) and run it from there (`/mnt/c/...` reaches the same
checkout). Building the runner (`run.py`, used by `-key`/`-break`/etc.
during analysis) also needs a working cc, so this applies whenever the
runner is used on Windows, not only for selftest.py.

## Status

Made independent of the earlier port on 2026-09-29. Checked: the kit's
tools rebuild that port's four programs (hand-written assembly) byte for
byte (with the `asm` switches in their hints), `symmap.py` writes the
same names as its own header, `dosrun` gives the same memory hashes and
pictures as its runner did, and `tests/selftest.py` passes on macOS (clang, SDL2 framework). Not checked:
the runtime and the runner built on Windows and Linux in this form, and
any program not written in assembly (compiled C programs, COM files,
overlays, protected mode: see docs/METHOD.md, "Limits").
