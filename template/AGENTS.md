# Working on {{SLUG}}

Read README.md (what this is), PROVENANCE.md (where the code may come
from) and docs/HANDOFF.md (the state, what was learned, what is next)
first; this file is the rules. The method and the tools are doskit's
(doskit/README.md, doskit/docs/METHOD.md).

## Rules

1. No game data in the repository: nothing from `game/` or `build/`, no
   program or data files, no bytes of them pasted into sources or docs
   (hints hold addresses, names and comments). The pre-commit hook
   refuses the usual cases; the rule holds beyond them.
2. `python3 doskit/tools/check.py` must say `all ok` before every commit
   (the hook runs it when `src/`, `tools/` or `port/src/gen/` changed).
   Do not skip the hook (`--no-verify`).
3. Work on `master`, commit in small steps with messages that say what
   changed and why, in plain words, in English. Push only when asked.
4. Say what you did not verify. A guess in a hints comment says it is a
   guess ("presumably", "not checked"). Numbers and addresses are copied
   from tool output, not from memory.
5. What was learned goes into docs/HANDOFF.md, what the port does and how
   it was checked into port/README.md, with the change that brought it.
6. A block of a hints file below `; ==== carried over` is written by
   `doskit/tools/xfer.py` only.
7. A change to the kit (a tool, the runtime) goes into doskit with a
   test there, not into a copy here.
8. A release is made as doskit/docs/RELEASE.md says, on every platform,
   with nothing left out: the packages the workflow builds, the
   player's port/dist/README.txt filled in for the game, each package
   started from a download before the release is announced.
9. The setup screen follows doskit/docs/LAUNCHER.md. Its visual design
   and common dialogs belong to doskit; the port supplies pages, items
   and game-specific behaviour through launcher.h and does not redesign,
   copy or override the launcher.

## Subagents

The kit is a Claude Code plugin (`doskit/.claude-plugin/`, the agents
in `doskit/agents/`), switched on for this project in
`.claude/settings.json`. It brings three: `doskit:collector` for
read-only stage 1/2 collection (disasm.py, gaps.py, ptrscan.py, run.py,
memcmp.py output) that the lead then interprets and writes into the
hints, `doskit:cmd-digest` for a command with long output (a build,
check.py, the port's comparisons) of which only the result is needed,
and `doskit:git-committer` for every commit and push, so the checks and
the message stay uniform. Delegate to them instead of doing their job
inline. An agent is changed in the kit, not copied here.

## Provenance (permanent)

PROVENANCE.md is part of the project and binding; it holds in full. In
short:

1. {{NAME}}'s behaviour is found only from its own programs, its data
   files and runs of them (doskit/tools/run). That another game does
   something alike is never evidence for how this one does it.
2. Other games' code, their ports and reimplementations, and any
   (purported) original source are never used, translated, adapted,
   copied or taken as a model for game-specific code: gameplay, physics,
   state machines, scoring, data structures, algorithms, constants.
   Generic, independently written infrastructure (doskit) may be shared.
3. The project is "a native compatibility implementation requiring an
   installed copy of the original game": never "standalone", "complete"
   or "official".
4. Version-control history is never rewritten.

## Technical conventions

- Python 3, standard library plus `capstone`; C99 for the port.
- Hints: syntax in the docstring of `doskit/tools/disasm.py`. The code
  segment is named `CODE`, the one DS normally holds `DATA`. Addresses
  are `SEG:OFFSET` in hex, four digits.
- Everything the tools write goes to `build/` (ignored). The game's
  files are in `game/` (ignored; `$DOSKIT_GAME` names another folder).
- Write in English in the repository.
