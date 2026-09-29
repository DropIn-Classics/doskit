# Working on doskit

Read README.md (what this is) and docs/METHOD.md (how a port is made
with it) first; this file is the rules for changing the kit. The rules
for a game's project are in its own AGENTS.md and PROVENANCE.md (made
from template/).

## Rules

1. Nothing of any game in the kit: no game's code, data, names,
   addresses, constants or behaviour. The kit holds generic
   infrastructure only (analysis tools, the runner, the platform and
   runtime); what belongs to one game stays in that game's project. The
   tests use programs written for the kit (tests/).
2. `python3 tests/selftest.py` must say `selftest ok` before every commit
   (the hook runs it when tools/, runtime/, template/ or tests/ changed).
   Do not skip the hook (`--no-verify`).
3. A change to a tool or the runtime that a project needs comes with a
   test in tests/ where it can have one. A change that alters what the
   tools make for an existing project (another source, other labels)
   says so in its commit message; check it against a project's hints
   (its check.py) when one is at hand.
4. Work on `master`, commit in small steps with messages that say what
   changed and why, in plain words, in English. Push only when asked.
5. Say what you did not verify: the platforms and compilers a change was
   built with, what was run.
6. template/PROVENANCE.md is the text every project carries; it is
   changed only when the user changes it, and stays free of any one
   game (the game's name is the `{{NAME}}` placeholder).

## Provenance (permanent)

The kit may serve every port, so it must not carry one game's logic
into another. Components are written independently and are generic:
platform abstraction, files, DOS/BIOS/VGA/sound emulation, rendering,
audio, MOD playback, input, controllers, timing, launchers, installers
and GOG detection, extraction, analysis tools. Nothing of a game's
gameplay, physics, state machines, scoring, data structures, algorithms
or constants, from whatever source. Version-control history is never
rewritten.

## Technical conventions

- Python 3, standard library plus `capstone`: a module docstring saying
  what the tool is for and how to call it, short functions, comments
  where the reason is not obvious, no frameworks. Tools find the project
  through kit.py.
- C99 for the runtime and the runner, built with cc (clang, gcc) and
  MSVC; no dependencies but SDL2 (plat_sdl.c) and the Windows SDK
  (plat_win32.c). Header guards `DK_NAME_H`.
- Everything the tools write goes to `build/` (ignored).
- Write in English in the repository.
- No heredocs (`<<EOF`, `@'...'@`) in shell commands; they fail often in
  this environment. Write the content with the file-editing tool instead
  and run it from there.
