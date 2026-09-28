# {{SLUG}}

{{NAME}} (DOS) taken apart, with the aim of a native compatibility
implementation requiring an installed copy of the original game: our
own C code, which runs the game with the data of the player's GOG
release. See PROVENANCE.md.

1. **Source from the programs** (`src/*.hints`): `doskit/tools/build.py`
   makes each program's assembly source from the player's copy and
   checks that it assembles back to the same bytes.
2. **Understanding**: names, structures and rules in the hints, checked
   by runs of the original (`doskit/tools/run.py`).
3. **The port** (`port/`): C on doskit's runtime.

No game files go into this repository, and no bytes of them.

## Use

    git submodule update --init          # doskit
    git config core.hooksPath hooks      # once per clone
    python3 doskit/tools/isox.py /path/to/game.gog   # -> game/
    python3 doskit/tools/build.py src/NAME.hints
    python3 doskit/tools/check.py
    sh port/build.sh                     # port\build.bat on Windows

Python 3 with `capstone` (`pip install capstone`).

## Layout

- `src/*.hints`: what is known about each program.
- `port/`: the implementation in C (its README says how far it is).
- `docs/HANDOFF.md`: state, what was learned, what is next.
- `AGENTS.md`: the rules for working on this; `PROVENANCE.md`: where the
  code comes from.
- `doskit/`: the tools and the runtime (a git submodule).
