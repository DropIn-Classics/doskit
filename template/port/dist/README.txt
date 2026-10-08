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
or ~/Applications on a Mac, on Linux where GOG's installer (the .sh)
put it, also in a folder of your choice, or where Heroic, Lutris,
Minigalaxy, Bottles or Wine usually put it; beside the program. It
offers to copy the game's files from it into its data folder, where the
settings and saves go too:

    Windows  %LOCALAPPDATA%\{{NAME}}
    macOS    ~/Library/Application Support/{{NAME}}
    Linux    ~/.local/share/{{SLUG}}

If it is not found, copy game.gog into the data folder or beside the
program, or name it:

    {{SLUG}} -gog /path/to/game.gog

Where GOG offers the game for Windows only (on a Mac or Linux too): get
its Windows installer from your GOG library, the "offline backup game
installer" setup_....exe (with the setup_...-1.bin files beside it, if
there are any), and leave it in Downloads, on the Desktop, in Documents
or beside the program. It is unpacked, not run; nothing is installed.
Or name it:

    {{SLUG}} -gog /path/to/setup_....exe

The download: a browser may hold back a package that few people have
downloaded yet, as each new release is (Chrome calls it a dangerous or
suspicious download). Open the browser's list of downloads and keep the
file: in Chrome "Keep", or in the entry's menu "Download dangerous
file" (or "suspicious file"). Take the package only from the port's
release page on GitHub.

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

New versions
------------

On the first start {{SLUG}} asks whether it may check for new versions;
F2 turns checks on or off later. With a yes it checks GitHub at most
once a day and sends nothing. When a newer version is ready, the setup
shows it and its notes. Press U to confirm installation on Windows or
Linux: the package is downloaded and verified, then the program closes,
replaces its folder and starts again. Any other key defers it until the
next run. On macOS, U opens the release page; download and open the app
yourself so Gatekeeper can check it. The settings, saves and the game's
files stay in the data folder.

Removing
--------

Everything {{SLUG}} wrote is in the data folder named above: the copy
of the game's files, your saves and the settings. The script beside
this file removes it and asks before each step: first whether to remove
the copied game files, then whether to remove the saves and settings
too. Nothing is removed unless you answer yes.

    Windows  double click uninstall.cmd (the key Y is yes)
    macOS    double click uninstall.command, or in the Terminal:
             sh uninstall.command
    Linux    in a terminal, in this folder: sh uninstall.sh

Then delete this folder yourself (on a Mac the app too, wherever you
moved it); the script leaves it alone. Your GOG release is never
touched.

Keys
----

Alt+Enter: full screen on and off. Print Screen: a screenshot.
(the game's own keys)

Licences
--------

MOD playback: micromod, by Martin Cameron (LICENCE-micromod.txt).
Linux and macOS: SDL2, by Sam Lantinga and others (LICENCE-SDL2.txt).
