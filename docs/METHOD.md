# The method

How a DOS game is taken from its shipped programs to a native port with
the kit, and what was learned doing it for the first game.
Each step says what the tools do and what they leave to the person (or
agent) doing it.

A project (a game) is its own repository (docs/NEW-PROJECT.md):

    src/NAME.hints      what is known about each program (the real work)
    game/               the game's unpacked files (ignored; never committed)
    build/              everything the tools write (ignored)
    port/               the C port, on the kit's runtime
    docs/HANDOFF.md     the state, what was learned, what is next
    doskit/             the kit (a git submodule) or $DOSKIT

The tools find the project from the current folder and the game in
`game/` (tools/kit.py; `$DOSKIT_PROJECT`, `$DOSKIT_GAME` override).

## 0. The game's files

`tools/isox.py IMAGE` unpacks a CD image (a plain ISO or raw 2352-byte
sectors; GOG's `game.gog` is one) into `game/`. A game on floppies or
installed from a GOG DOSBox folder is copied as it is. Look at what is
there: which programs (`.EXE`, `.COM`, drivers), how they call each
other (batch files, EXEC), which files are data. Write it into
HANDOFF.md as a table.

Nothing of `game/` or `build/` goes into the repository, and no bytes
of them into sources or documents: the pre-commit hook refuses the usual
cases, the rule holds beyond them.

## 1. Source from the programs (stage 1)

Goal: `tools/build.py src/NAME.hints` says IDENTICAL for every program.

**Segments.** Start the hints with `exe` and one `segment` line per
segment in image order. Where they are: the relocation table's values
(each relocated word is a segment's frame), the header's SS:SP (the
stack segment), what `build.py` reports when a segment is missing, and
debug information if there is any (`cv4.py` reads CodeView NB08). The
code segment must be called `CODE`, the one DS normally holds `DATA`
(the generated ASSUME uses the names). `relocorder` gives the order of
the relocation table by the segment holding the site (read it from the
original's table). `keeptail` keeps what follows
the image (debug information) as it is.

**The assembler.** The original was assembled with some assembler whose
choices `tasm.py` must repeat where an instruction has two encodings.
`build.py` reports each instruction encoded otherwise; the pattern tells
which assembler it was:

- every jump in its shortest form, no NOP padding: a multi-pass
  assembler (MASM 6, or TASM with /m);
- `LEA reg,[addr]` kept as LEA: MASM (`asm lea_smart=0`);
- `ADD AX,5` as `83 C0 05`: MASM (`asm alu_ax_short=0`);
- `TEST r1,r2` with r1 in r/m: MASM (`asm test_form=rm_reg`).

Single instructions encoded by hand (a DB in the original source) are
`raw` hints; build.py lists them as candidates each round.

**Reaching the code.** disasm.py follows jumps and calls from the entry
points, tracking what DS and ES hold. Then `tools/gaps.py` lists what of
CODE is not reached: every gap is either code reached through a pointer
or data. For code, find the pointer and say where it comes from in a
comment: `code` (an entry point), `words` (a table of near pointers;
with `stride=N` one pointer in each record of N bytes), `rwords` (a compiled switch: 16-bit
offsets from the table's own start, `LEA reg,[reg+table]; JMP reg`;
`stride=N` and `from=OFF` for offsets in records, counted from elsewhere),
`ptr` (an immediate that is a code offset: `MOV DX,offset handler`
before INT 21h AH=25h), `coderange` (a run of handlers). gaps.py shows
where each gap's address appears as a word, which usually is the
pointer. Leave data as data. Code that runs into data after a call
that never returns (an exit through a service the analysis does not
know) gets a `stop` hint on that instruction.

**Addresses written as numbers.** An immediate or displacement that is
really an address does not show in the byte comparison, only once code
is translated or data moves. `tools/ptrscan.py` finds MOV r16,imm whose
register is then used as a base; check each by eye before adding a
`dptr`. A `dptr`/`ptr` on an instruction without an immediate names its
displacement (`CALL [SI+table]`). A small displacement with a register
(below 100h) is taken for a field offset unless such a hint says
otherwise; so is one with a register that lands in reached code (at an
instruction or inside one), as a flat program's records and buffers
have offsets far beyond 100h.

**Two programs, one engine.** When a game ships two builds of one
engine (two episodes, a demo and the full game), analyse one,
then `tools/xfer.py A.hints B.hints` carries its hints to the other by
aligning the instruction streams (a code address only when the next ten
instructions match). B.hints keeps its own lines above the carried block;
the block is written by xfer.py only; check.py tells when it is out of
date.

**Names for the C.** `tools/symmap.py port/src/gen/names.h PREFIX
KEY=src/A.hints ...` writes where every name is in each program, the
file sizes and SHA-256 (the port checks it loads the file the hints
describe). check.py finds the header by its first line and checks it.

What was learned:

- capstone names 98h/99h `cwde`/`cdq` in 16-bit mode; disasm.py renames
  them to CBW/CWD.
- A `code SEG:OFF NAME` also names the label.
- The source is regenerated from the hints every time; never edit
  build/*.ASM.
- Hints and docs may have CRLF line ends (Windows); tools that write
  hints on a Mac or Linux write LF. Keep one convention per repository
  (`.gitattributes` has `* -text`: files go in as they are).

## 2. Understanding (stage 2)

Goal: names, records and rules in the hints, each claim checked by a
run or marked as read from the code ("presumably", "not checked").

**The runner.** `tools/run.py [options] PROGRAM [ARGS]` runs a program
in `dosrun`: a 386 CPU (real mode; protected mode without paging and
task switches, V86 mode; see the top of "protected mode" in
tools/run/cpu.c; INT 15h AH=87h/88h/89h), VGA (planar, chain-4, Mode X, the
retrace), PIT/PIC/keyboard, BIOS, both 8237 DMA controllers and a Sound Blaster 16
(DSP and mixer configuration, so the game's own sound drivers run; `-wav`
writes what it played, `-sb` its transfers), and a small DOS whose drives are
all the game's files with a writable layer in `build/run/state`, each
drive with its own current directory; programs start on D:. Everything
runs on an emulated clock (6 M instructions a second by default):
nothing reads the host's clock, the same arguments give the same run,
and the report ends with hashes of memory and video memory.

Useful options (all at the top of `tools/run/main.c`): `-key T KEY`,
`KEY+`/`KEY-`, `-keys FILE` (key scripts); `-shot`, `-shotevery`
(pictures); `-break ADDR[#N]` (stop at the Nth pass), `-log ADDR`
(registers at each pass), `-watch ADDR` (who writes a byte, when),
`-dump ADDR LEN`, `-dumpevery DT` (a time series), `-trace FILE N`,
`-dos` (every INT 21h call with its file), `-intwatch NN`, `-prof`,
`-ram`/`-vram` (memory for memcmp.py), `-wav`, `-cdwav`, `-put GUEST HOST` (a
configuration file the set-up program would have written), `-loadfix`
(load above 64 KB: EXEPACK's unpacker fails in a program loaded below
it, e.g. a driver EXECed by a small program). run.py translates
addresses by the hints' names (`CODE:1387`, `DATA:9AA0+4`, `L13B2`, a
`name`, `EXE:name` for another program's hints).

To get the game somewhere without a window (a place in a level, a rare
event): key scripts with random presses, many seeds in parallel, a
`-log` on the routine that should be reached; then repeat the seed that
got there with `-dump`/`-shot`.

Not in the runner: paging and task switches (so no extender that needs
them), VCPI/DPMI hosts, x87 instructions, EMS/XMS, a mouse driver (INT 33h),
OPL FM synthesis (the ports answer, nothing sounds), savestates, a
window. CD audio is there as a file: MSCDEX answers for D:, a disc of one
data track or, with `-cue`, the tracks of a cue sheet such as GOG's
`game.inst`; `-cd` prints the plays and the channel settings,
`-cdwav` writes what the drive played (44.1 kHz stereo from t=0, the
tracks' Ogg Vorbis through third_party/stb_vorbis), not mixed with
`-wav` (which starts at the Sound Blaster's first transfer).

**Files.** Each data format gets a tool in the project (`tools/NAMEfiles.py`)
that parses the files and writes them back to identical bytes, and shows
them (pictures as PNG). A format understood this way is understood.

What was learned:

- Programs rely on DOS's per-drive current directories (switch to C:,
  CHDIR, EXEC a driver, switch back): the runner keeps them.
- A timer interrupt must land on the instruction it is due on, also
  inside REP string instructions; a batch of instructions counted as one
  shifted a sound driver's tick by a whole frame.
- A program may wait for the retrace by polling 3DAh; the runner's beam
  moves with the emulated clock.
- In mode 12h the BIOS palette is the 64 EGA colours; in mode 13h the
  program sets its own. Chain-4 (mode 13h) puts CPU byte A into plane A&3
  at address A&~3.
- A mouse driver resets its ranges when the mode is set (programs count
  on it).

## 3. The port (stage 3)

Goal: C that runs the game, checked against the original at the same
point, then free to modernise.

**Over the memory image first.** The port loads the player's program
file into a megabyte of real-mode memory where dosrun loads it
(`rm_load_exe` with `RM_LOAD_PSP`, the SHA-256 from symmap.py's header)
and translates routine by routine; the C reads and writes that memory
through `rb`/`rw`/`frb`... by the hints' names, so records that point at
each other with 16-bit offsets keep working, and a routine not yet
translated can stop the program with its name. Video goes through
`vga.c` (the same ports and planes the assembly used), the frame wait
through `frame.c`. Both are stepping stones: once everything is C, data
moves into structures and pictures into bitmaps, one at a time.

**Compare.** Stop the original in dosrun at a routine (`-break
name#N -ram A -vram AV`) and the port at the same pass (its own counter;
`rm_write`, `vga_write_planes`), then `tools/memcmp.py HINTS A B --vram
AV BV --skip STACK`: the runs of differing bytes with the name at or
before each. Equal but for what has no reason to be (the stack, a saved
vector, a driver's scratch space) is the goal; write down where and how
each check was made. The same keys: `frame_record` writes each key byte
with its picture number, which becomes a key script for dosrun (time =
start + picture / rate: find the start and the rate once with `-log` on
the original's frame wait).
A project may want a `portcmp.py` that does all this for one key script
(not in the kit yet: the one made so far is tied to its game).

**The frame.** A DOS game's frame is paced by the retrace or a timer
tick (often a sound driver's callback). frame.c keeps the video mode's
refresh rate, runs a tick callback and hands the keys over first; for a
loop that waits on the keyboard alone `frame_wait_keys` lets the
program go on before the tick, as a fast PC did. Measure the rate the
original's tick has in dosrun before trusting 70 Hz.

**Sound.** Music modules play through `modplay.c` (micromod) with the
driver's timing where the game depends on it (positions, pattern-jump
callbacks: modplay_jump, modplay_set_jump_hook); effects as notes put
into a channel (modplay_note). Other formats need their own player.

**The platform.** `platform.h`: a window showing an indexed picture in
the 4:3 of a VGA screen (Alt+Enter full screen; Print Screen writes the
picture shown as screenshot_NNNN.png, `shot.h`; full screen on a Steam
Deck), PC scan codes, a monotonic clock, an audio callback, the mouse.
`plat_null.c` runs headless on a virtual clock for tests
(`DK_FRAMES`, `DK_KEYS`, `DK_MOUSE`, `DK_DUMP`, `DK_SHOTS` for PNG
screenshots by picture number). Controllers become keys
(`pad.h`, a table per situation). `textmode.h` draws an 80x25 screen for
a setup program in the style of DOS's. `cdimage.h` finds the GOG
release and unpacks it on the first start (`sys_find_game` then finds
it in the data folder).

**Modernising.** Once equal: a launcher (setup screen: options, sound,
controller), quality of life fixes (switchable, off in comparisons),
skipping intros, the original's bugs fixed only where they bother,
each such change documented as a change from the original.

**Wording.** A port made this way is a native compatibility
implementation requiring an installed copy of the game: it contains only
its own code, and the player's installed release supplies the game's
data at run time. Never "standalone" or "completely native".

## Limits

Worked through for real-mode MZ programs written in assembly (MASM/TASM),
linked by Microsoft LINK, 186/386 instructions in 16-bit code. Not tried:

- compiled programs (Borland C, Microsoft C, Watcom): disasm.py handles
  their code, but the C library is large, far calls and data segments
  many, and the compiler's idioms (switch tables, far pointers in data)
  will want more hints and probably new analysis;
- COM programs (no MZ header), overlays, EXEPACK/PKLITE-packed programs
  (unpack first; the runner runs them as they are);
- protected mode: the analysis and build.py take pMAX flat images
  (`pmax` in the hints: 32-bit code, USE32 source, the descriptors as
  segments; tests/flat, and one game's main program rebuilt byte for
  byte), not DOS/4GW's LE/LX or other extenders' formats. The runner
  emulates the 386's protected mode (tests/pmode: the BIOS's way in, a
  fault, an interrupt, ring 3, a call gate, V86 mode) for an extender
  that switches by itself; no real extender run through yet. A 32-bit instruction with a 16-bit address and no
  register (67h, `[0027h]`) has no source form and stays DB;
- other linkers' header layouts (`build.py`'s write_mz is Microsoft
  LINK's).

Each new game will stretch one of these; extend the kit (with a test in
tests/) rather than the project.
