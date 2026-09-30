# Starting a new game

1. The repository, from the template, for a game installed from GOG:

       python3 ~/doskit/tools/new_project.py ~/mygame

   It lists the GOG games installed here (`tools/goglist.py`), and the
   one chosen gives the name, GOG's product ID, the installed folder's
   name and the CD image's path (on a Mac also its path inside the
   application); it asks for the slug and the marker (the CD's programs
   offered) and unpacks the image into `game/` if wanted. Without the
   game installed here, everything is named:

       python3 ~/doskit/tools/new_project.py ~/mygame "My Game" mygame \
           --marker GAME/GAME.EXE --gog-id 1234567890 [--image PATH/game.gog]

   It copies template/, fills in the game's name everywhere (PROVENANCE.md
   among them), runs `git init`, enables the hook and adds the kit as the
   submodule `doskit/` from its GitHub repository (--kit names another
   URL). Nothing is committed: look at it, then make the first commit.
   --gog-id is GOG's product ID, the number in the `goggame-ID.info` in
   the installed folder (on a Mac inside the app, `Contents/Resources`);
   on Windows the port finds the installation by it in the registry.

2. PROVENANCE.md: read it; it is binding for everyone working on the
   project, people and agents alike, and it goes into every package the
   project releases (the template's CI does that). check.py fails without
   it or with a placeholder left in it.

3. The game's files into `game/` (ignored), unless new_project.py
   unpacked them: `python3 doskit/tools/isox.py IMAGE` for a CD image
   (GOG's is often, not always, `game.gog`), else copy the installed
   folder. List the
   programs and data files in docs/HANDOFF.md.

4. Stage 1 for each program (doskit/docs/METHOD.md): `src/NAME.hints`,
   then `doskit/tools/build.py` and `doskit/tools/gaps.py` in turn until
   IDENTICAL, `doskit/tools/check.py` before every commit.

5. The port: port/src/main.c finds the game (and unpacks the GOG image on
   the first start); its `release` has what new_project.py knew: fill in
   what is missing (the GOG folder name, the image's path in it, GOG's
   product ID, the Mac release's path) and check what was taken from
   another platform's release (from a Mac's, the Windows image's path is
   only its name). Then stage 3 as METHOD.md says:
   `doskit/tools/symmap.py port/src/gen/names.h PREFIX KEY=src/NAME.hints`,
   the program over `rmem.h`, compared with the runner.

A kit update in a project: `git -C doskit pull`, then check.py and the
port's comparisons again, and commit the new submodule state.
