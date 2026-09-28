# port: {{NAME}} in C

A native compatibility implementation requiring an installed copy of the
original game, on doskit's runtime (doskit/runtime). The game's data is
read from the player's copy at run time; none of it is in this
repository.

## State

Started {{DATE}} from doskit's template: finds or unpacks the game's
files and shows a text screen. Nothing of the game is translated yet.

## Build and run

    sh port/build.sh          # macOS, Linux (SDL2 for the window)
    port\build.bat            # Windows (MSVC)
    port/build/{{SLUG}} -game game

## Checked

(what was compared with the original, where and how)
