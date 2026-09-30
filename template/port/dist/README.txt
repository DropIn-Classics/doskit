{{SLUG}}
========

A native compatibility implementation requiring an installed copy of
{{NAME}}: this package contains only the program, our own code.
The game's data comes from the player's {{NAME}} of GOG.com and is
read from it each time the program runs; without it nothing can be
played.

Starting
--------

Start {{SLUG}} ({{SLUG}}.exe on Windows, {{SLUG}}.app on a Mac). The
first time it looks for your GOG release: where GOG installed it (on
Windows found through the registry too), the GOG app in /Applications
or ~/Applications on a Mac, beside the program. It offers to copy the
game's files from it into a folder "game" beside the program (on a Mac
into ~/Library/Application Support/{{NAME}}; on Linux, where the
program's folder cannot be written, ~/.local/share/{{SLUG}}). If it is
not found, copy game.gog beside the program (on a Mac into
~/Library/Application Support/{{NAME}}), or name it:

    {{SLUG}} -gog /path/to/game.gog

Windows: {{SLUG}}.exe needs nothing else. It is not signed, so Windows
may say it protected your PC: click "More info", then "Run anyway".

macOS (10.13 or newer, Intel and Apple silicon): {{SLUG}}.app needs
nothing else; move it to Applications if you like. It is not signed by
Apple, so the first start is refused ("cannot be verified"): close that
message, open System Settings > Privacy & Security, click "Open Anyway"
at the bottom and confirm. After that it starts with a double click. On
macOS 14 and older a right click on the app, "Open" and "Open" again
does the same. Or, in the Terminal, in the folder of the app:

    xattr -cr {{SLUG}}.app

Linux and the Steam Deck: keep libSDL2-2.0.so.0 beside {{SLUG}} (the
package brings SDL2 along; nothing needs to be installed). On the Deck
the game starts full screen; in Game Mode add {{SLUG}} as a non-Steam
game.

Keys
----

Alt+Enter: full screen on and off. Print Screen: a screenshot.
(the game's own keys)

Licences
--------

MOD playback: micromod, by Martin Cameron (LICENCE-micromod.txt).
Linux and macOS: SDL2, by Sam Lantinga and others (LICENCE-SDL2.txt).
