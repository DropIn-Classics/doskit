# Starting a new game

1. The repository, from the template:

       python3 doskit/tools/new_project.py ~/mygame "My Game" mygame \
           --marker GAME/GAME.EXE --kit https://github.com/OWNER/doskit.git

   It copies template/, fills in the game's name everywhere (PROVENANCE.md
   among them), runs `git init`, enables the hook and adds the kit as the
   submodule `doskit/` (without --kit from the local kit's folder). Nothing
   is committed: look at it, then make the first commit.

2. PROVENANCE.md: read it; it is binding for everyone working on the
   project, people and agents alike, and it goes into every package the
   project releases (the template's CI does that). check.py fails without
   it or with a placeholder left in it.

3. The game's files into `game/` (ignored): `python3 doskit/tools/isox.py
   PATH/game.gog` for a CD image, else copy the installed folder. List the
   programs and data files in docs/HANDOFF.md.

4. Stage 1 for each program (doskit/docs/METHOD.md): `src/NAME.hints`,
   then `doskit/tools/build.py` and `doskit/tools/gaps.py` in turn until
   IDENTICAL, `doskit/tools/check.py` before every commit.

5. The port: port/src/main.c finds the game (and unpacks the GOG image on
   the first start); fill in the GOG folder name and, if there is one, the
   Mac release's path in its `release`. Then stage 3 as METHOD.md says:
   `doskit/tools/symmap.py port/src/gen/names.h PREFIX KEY=src/NAME.hints`,
   the program over `rmem.h`, compared with the runner.

A kit update in a project: `git -C doskit pull`, then check.py and the
port's comparisons again, and commit the new submodule state.
