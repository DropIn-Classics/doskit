# The launcher design

This is the binding design contract for every setup screen made with
doskit.  The common appearance is part of the ports' identity: a player
who has seen one port should immediately recognise and know how to use
another one.

`runtime/launcher.c` is the visual reference and owns the presentation.
A port supplies content through `runtime/launcher.h`; it does not design
another launcher around that content.

## What doskit owns

The kit owns all visual and interaction design shared by the ports:

- the 80 by 25 DOS text screen, its font and CGA colours;
- the cyan title and help bars on the blue backdrop;
- the game name, port name and version in the title bar;
- the page tabs, framed window and shadow;
- the label and value columns, headings, selection highlight, arrows,
  scrolling and contextual help line;
- the wording and controls shown in the bottom help bar;
- keyboard and controller navigation; and
- the dialogs about the game's files and their copy progress.

These are not examples or defaults to restyle.  They are the launcher.
Their exact dimensions, attributes and rendering stay in
`runtime/launcher.c`, so ports do not duplicate them as constants.

## What a port owns

A port supplies only the information that differs between games:

- `LauncherApp`: the game, program and version names;
- `LauncherPage` titles and their order;
- `LauncherItem` headings, actions, settings, key bindings, values and
  short contextual help;
- the optional short footer; and
- the facts required by the common game-file dialogs, through the
  functions already provided by `launcher.h`.

Use the existing item kinds.  Group related settings on clearly named
pages; put the normal way to start the game first, and keep labels and
values short enough for the fixed columns.  A heading names a group, an
action is a verb, and help explains a consequence that is not already
clear from the label.  Do not embed alignment spaces, box characters or
colour codes in port strings.

## Changes that are not port customisation

A port must not:

- draw its own setup screen with `textmode.h` or a platform GUI;
- copy, fork or override `launcher.c` in the game repository;
- replace the font, palette, backdrop, title bar, tabs, frames, shadows,
  columns, selection style, help bar or navigation;
- add game art, logos, decorative backgrounds or a per-game theme; or
- redraw the common game-file questions, errors or progress display.

When a port needs a control or behaviour that `LauncherItem` cannot
express, extend the generic API and implementation in doskit, with a
generic test.  The extension must make sense for other ports and must
preserve the established design.  A deliberate redesign is a doskit-wide
product decision: update this document, the implementation and its tests
together, then review existing ports.  It is never an incidental part of
adding one game.

## Review checklist

Before accepting a launcher change, check that:

1. the port describes pages and items and calls `launcher_run`;
2. all common game-file UI uses the `launcher_*` dialog functions;
3. no launcher presentation constants or drawing code were added to the
   port;
4. only game-specific content and behaviour differ from another port;
5. the first page, every page transition, scrolling, key capture and the
   common dialogs were exercised; and
6. the result still looks like `runtime/launcher.c`, because that code
   drew it.
