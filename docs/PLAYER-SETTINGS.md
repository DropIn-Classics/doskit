# The player's settings: volume, headphones, keys, controller

How a port gives the player the volume keys in play, the headphone mix,
their own keys and a controller's buttons, with the kit's parts only.
None of it touches the program's memory or its comparisons: everything
is applied after the setup screen (`launcher.h`), which a port shows in
a window and not in the headless runs that are compared with the
original.  The setup screen's look and its rules are `docs/LAUNCHER.md`;
this file says which parts to wire together and how.

## The parts

| Part | What it does |
| --- | --- |
| `launcher.h` | the pages, a choice (`LI_CHOICE`) or a key (`LI_KEY`) each, kept in the port's settings file |
| `audiofx.h` | the headphone mix (and bass, treble, oomph) on the stereo output, then a gain |
| `hud.h` | the box at the top of the picture: "VOLUME" with a bar, or "MUTE" |
| `frame.h` | `frame_set_hud` (the box and the keypad's + - * /), `frame_set_keymap` (the player's keys) |
| `pad.h` | a controller's buttons become keys, by a table the port sets |
| `platform.h` | `plat_read_control` (the sound keys), `plat_audio_lock` |

Build `audiofx.c`, `hud.c` and `pad.c` with the runtime (the port's
`build.sh` and `build.bat` list them).

## The volume and the mute key

Keep the volume (0 to 10, a choice on a "Sound" page with the name
`volume`) and a flag "muted" in the port.  The output's gain is
`muted ? 0 : volume / 10`; the port's fill function of
`plat_audio_start` hands it to `audiofx_process` (below), which the audio
thread calls, so reading two ints there needs no lock.

In play the keypad's + and - step the volume (and end the mute), * turns
the mute on and off:

```c
static void hud_control(int c)
{
    if (c == PLAT_VOLUME_UP || c == PLAT_VOLUME_DOWN) {
        if (c == PLAT_VOLUME_UP && volume < 10)
            volume++;
        else if (c == PLAT_VOLUME_DOWN && volume > 0)
            volume--;
        muted = 0;
    } else if (c == PLAT_MUTE)
        muted = !muted;
    else
        return;                 /* PLAT_EQ: unused unless the port has one */
    if (muted)
        hud_show("MUTE", 0, 0, 140);
    else
        hud_show("VOLUME", volume, 10, 140);
}
```

and after the setup screen, before the game:

```c
frame_set_hud(hud_draw, hud_control);
```

With a handler set, `frame.c` keeps the keypad's + - * / from the
program; a game that needs them for itself cannot have the volume keys
there.  The box lasts the pictures given (140: two seconds at 70 a
second).  The volume changed in play is not written back to the settings
file unless the port saves it at the end.

## The headphone mix

A choice `headphone` (No, Yes) on the same page.  The output stays
stereo: a port whose sound is mono (an OPL, the PC speaker) writes each
sample to both sides first.  Then:

```c
/* once, where the stream starts */
audiofx_init(RATE);

/* when the setting changed (the launcher's `changed` callback) and once
 * after launcher_load */
plat_audio_lock();
audiofx_set(0, 0, 0, headphone);          /* bass, treble, oomph in dB */
plat_audio_unlock();

/* at the end of the fill function, over all the frames written */
audiofx_process(buf, frames, muted ? 0.0f : volume / 10.0f);
```

`audiofx_init` clears the filters, not the settings, so it may come after
`audiofx_set`.  With headphone 0 and the dB at 0 the sound is as before
but for a DC blocker.  Only the window's audio thread runs it: the
headless build has no stream, so no comparison sees it.

## The player's keys

Find the game's own keys from its code and data (a table of scancodes,
the INT 9 handler's checks) and write them down in the port with where
they were read.  For each action the player may change, keep the game's
make code and the player's (`LI_KEY` items, named `key_...`, the game's
code as the default).  The keymap turns the player's key into the
game's:

```c
static unsigned char keymap[256];

for (i = 0; i < 256; i++)
    keymap[i] = (unsigned char)i;
for (k = 0; k < K_COUNT; k++)
    if (player_key[k] && player_key[k] != game_key[k])
        keymap[player_key[k]] = (unsigned char)game_key[k];
frame_set_keymap(keymap);
```

Make codes are scan code set 1, the E0 keys + 80h (pad.h).  The game's
key keeps working beside the player's, and the player's key no longer
reaches the program as itself (a letter mapped to "up" cannot be typed
in the game's text fields): say so in the help line where it matters.
When the game has several keys for one action (the arrows and the
keypad), map onto one of them and name the others in the help line.

## The controller

`pad.h` turns buttons into keys by a table (`PadKeys`, up to two make
codes a button).  The launcher uses `pad_menu_keys` itself; after it the
port sets its own:

- a list of what a button can do (`"nothing"`, the game's actions, the
  keys its menus need: Esc, Enter, Y, N, ...) as an `LI_CHOICE` per
  button on a "Controller" page;
- their names in the settings file from `pad_button_name`:
  `snprintf(name, n, "pad_%s", pad_button_name(b))`, set before
  `launcher_load`;
- the table built from the choices, a game action given as the
  *player's* key (the keymap then makes it the game's), and set with
  `pad_set_keys(&table)` before the game starts.

The D-pad and the left stick are the same buttons (`PAD_UP` ...).

## The order at the start

1. `pad_names()`, `launcher_load(cfg, ...)`, the sound's settings
   applied (`audiofx_set`);
2. `launcher_run(..., changed)`; `changed` applies the headphone (and
   full screen) at once;
3. `launcher_save`;
4. when the game is started: the keymap, the pad's table,
   `frame_set_hud(hud_draw, hud_control)`.

Without the setup screen (headless, the comparisons) none of these is
set: the keys are the game's, the controller does nothing, no hud.

## Checking it

- The settings file after the setup screen holds `volume`, `headphone`,
  the `key_...` and `pad_...` lines, and a second start reads them back.
- In a window: + - * show the box and change the loudness; the
  headphone choice changes the sound at once; a key given to an action
  does it in the game, the game's own key too; each button does what its
  page says.
- The comparisons with the original are unchanged (they run without the
  setup screen).
