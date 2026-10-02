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
- the blue backdrop with a light grey bar at the top and one at the
  bottom (black text, the keys in red);
- the game name, port name and version in the title bar;
- the menu: a centred window of double lines with its title in yellow,
  the actions, the pages to open and, when one setting is all a group
  has, that choice itself (not a page of one item), a gap and "Quit" last, the
  selection a cyan bar; Esc in the menu only moves the selection to
  "Quit" (Enter there leaves), it never leaves by itself;
- the pages of settings: one centred window each, sized by its content,
  its title in the frame, the labels in light grey in a column, the
  values in yellow in a second, arrows at the selected choice, headings
  in light cyan, scrolling and the selected item's help line in light
  cyan below the window;
- the wording and controls shown in the bottom help bar;
- keyboard and controller navigation; and
- the dialogs about the game's files and their copy progress.

These are not examples or defaults to restyle.  They are the launcher.
Their exact dimensions, attributes and rendering stay in
`runtime/launcher.c`, so ports do not duplicate them as constants.

## What a port owns

A port supplies only the information that differs between games:

- `LauncherApp`: the game, program and version names;
- `LauncherPage` titles and their order: page 0 is the menu, the others
  are opened from it by items of kind `LI_PAGE`;
- `LauncherItem` headings, actions, settings, key bindings, values and
  short contextual help;
- the optional short footer; and
- the facts required by the common game-file dialogs, through the
  functions already provided by `launcher.h`.

Use the existing item kinds.  The menu holds the normal way to start the
game first, then a gap and one `LI_PAGE` per group of settings; "Quit" is
added by the launcher.  Group related settings on clearly named pages of
a few items each (one window, at most 15 rows before it scrolls); keep
labels and values short enough for the screen, and the help line to one
line.  A heading names a group, an
action is a verb, and help explains a consequence that is not already
clear from the label.  Do not embed alignment spaces, box characters or
colour codes in port strings.

## Changes that are not port customisation

A port must not:

- draw its own setup screen with `textmode.h` or a platform GUI;
- copy, fork or override `launcher.c` in the game repository;
- replace the font, palette, backdrop, bars, menu, frames, shadows,
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

## Origin

The design is that of pddnative's setup screen, which this document
makes the standard: a menu of pages in place of tabs over one long page
(the earlier design, replaced in October 2026 together with its tests;
the ports built on it are to be rebuilt with the new kit and their pages
split into a menu and pages: pddnative has a launcher of its own with
this look and no code in common yet).  The title bar's left side is the
game's name alone, the menu's window is titled "Setup".

## Review checklist

Before accepting a launcher change, check that:

1. the port describes pages and items and calls `launcher_run`;
2. all common game-file UI uses the `launcher_*` dialog functions;
3. no launcher presentation constants or drawing code were added to the
   port;
4. only game-specific content and behaviour differ from another port;
5. the menu, opening each page and coming back, scrolling, key capture and
   the common dialogs were exercised; and
6. the result still looks like `runtime/launcher.c`, because that code
   drew it.
