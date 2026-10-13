# The hint card

What a port says to the player beside the game - a tutorial's step, a
first visit's explanation, a note before the game - is said with one
card, the same in every port.  This is its design contract, as
docs/LAUNCHER.md is the setup screen's: `runtime/hint.c` owns the
presentation, a port supplies words and places through
`runtime/hint.h`.

## What doskit owns

- the card: a window in the setup screen's style (blue, a frame of
  double lines in light grey with the title in yellow in it, the text in
  white, the line of keys in light cyan at its foot, a shadow), drawn
  with `textmode.h`'s font;
- its size and place: as wide as its text needs (at most 56 characters
  a line, wrapped), centred across; away from the place it talks about,
  else in the middle while the program is held and at the bottom over
  the running game;
- the frame around a place: yellow with a black line on both sides;
- the dimmed picture (but for the place) while the program is held;
- the layer it is drawn into: the window's own pixels
  (`plat_set_layer`), a character 16 pixels high in a window of 480
  lines and larger by whole steps with the window, so the text is sharp
  where the picture has the video mode's few pixels;
- the answers: Enter, Space or the left button go on (`HINT_NEXT`),
  Backspace or Left go back (`HINT_BACK`), Esc ends (`HINT_END`); and
- the holding of the program meanwhile (`frame_hold`): no tick, no keys
  or buttons for the program, the answering key's release kept too.

## What a port owns

- the words: title, text and the line of keys, in the player's language
  (UTF-8; the font has ASCII and the German letters, a letter it has not
  shows as `?` - add letters to `textmode.c`, with a test);
- the place: a rectangle in the game's own coordinates;
- when a card is shown and what an answer does (the next step, the
  tutorial switched off, "seen" written to its settings file); and
- what it reads of the game to decide that.  The card itself reads and
  writes nothing of the program: with it or without it the program's
  memory is the same, and so are the comparisons with the original
  (headless runs have no layer unless they set one).

Keep a card short: a step's one thought, a few lines.  The line of keys
names what `hint_run` answers to, in the words of the port's language.
Do not put box characters, alignment spaces or colours into the strings.

## How to wire it

Build `hint.c` (with `textmode.c` and `frame.c`) into the port.  After
`plat_init`, in the windowed program:

```c
plat_set_layer(hint_layer, NULL);
```

A card that waits for the player, from where the port's loop is between
two pictures:

```c
static const Hint step = {
    "The title", "What to do here, in a sentence or two.",
    "Enter: next   Esc: end the tutorial",
    40, 30, 120, 60, 320, 200           /* the place, of the game's 320x200 */
};

switch (hint_run(&step)) {
case HINT_NEXT: /* the next step */ break;
case HINT_BACK: /* the one before */ break;
case HINT_END:  /* the tutorial off */ break;
case HINT_CLOSED: /* the window was closed: leave as frame_wait's 0 */ break;
}
```

A card that stays while the player does what it says: `hint_show(&goal)`
until the port sees it done, then `hint_show(NULL)`.  The program runs
and gets all input; such a card has no line of keys.

While the program is held its timer does not tick, so a game's clock
that runs on `frame.h`'s tick stands still.  A port that counts time in
another way (a thread, the audio's samples) stops that itself around
`hint_run`.  Sound that is playing plays on.

## Shown once

That the player has read a note, or how far they are in a tutorial, is
kept with the port's settings, not in the game's files: an item of kind
`LI_KEPT` (`launcher.h`) on a page that no item of the menu opens, so
it is read and written with the settings and never shown.

```c
static int note_seen;
static LauncherItem kept[] = {
    { LI_KEPT, NULL, "note_seen", NULL, &note_seen, 0, NULL },
};
/* the last of the pages: { NULL, kept, 1 } */
```

After `launcher_load`: when `note_seen` is 0, show the card, set it
and `launcher_save` at once (not only at a clean exit).  A player who
deletes the settings file sees the note again.  A tutorial also gets a
choice on a page of the setup screen ("Tutorial": Off, On), so that it
can be had again without deleting anything.

## Changes that are not port customisation

A port must not draw a card of its own into the picture or the layer,
copy or override `hint.c`, or change the card's colours, font, frame,
place or answers.  When a port needs something the card cannot do (a
picture in it, a choice of several answers), extend `hint.h` in the kit
with a test, for all ports.

## Checking

`tests/hint/hinttest.c` checks the card's pixels and the holding on
`plat_null.c`.  A port's headless run sees a card with
`DK_LAYER=WIDTHxHEIGHT` (the window's size to stand for) and
`DK_LAYERDUMP=file`, which writes the last picture with the layer over
it as a PPM file.
