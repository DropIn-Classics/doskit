# The method

How a DOS game is taken from its shipped programs to a native port with
the kit, and what was learned doing it. Each step says what the tools do
and what they leave to the person (or agent) doing it.

A project (a game) is its own repository (docs/NEW-PROJECT.md):

    src/NAME.hints      what is known about each program (the real work)
    game/               the game's unpacked files (ignored; never committed)
    build/              everything the tools write (ignored)
    port/               the C port, on the kit's runtime
    tools/              the project's own tools (its file formats)
    docs/HANDOFF.md     the state, what was learned, what is next
    doskit/             the kit (a git submodule) or $DOSKIT

The tools find the project from the current folder and the game in
`game/` (tools/kit.py; `$DOSKIT_PROJECT`, `$DOSKIT_GAME` override).
`tools/check.py` rebuilds every program from its hints (skipping those
whose inputs have not changed) and checks the generated headers and
carried blocks; it must say `all ok` before a commit.

## 0. The game's files

`tools/new_project.py` makes the project and finds the GOG release
(`goglist.py`). `tools/isox.py IMAGE` unpacks a CD image (a plain ISO or
raw 2352-byte sectors; GOG's `game.gog` is one) into `game/`. A game on
floppies or installed from a GOG DOSBox folder is copied as it is. A
game GOG sells for Windows only comes as its installer: `tools/inno.py
SETUP.exe -x game` unpacks what it would install (on any system,
without running it). Look at what is there: which programs (`.EXE`,
`.COM`, drivers), how they call each other (batch files, EXEC), which
files are data. Write it into HANDOFF.md as a table.

Nothing of `game/` or `build/` goes into the repository, and no bytes
of them into sources or documents: the pre-commit hook refuses the usual
cases, the rule holds beyond them.

## 1. Source from the programs (stage 1)

Goal: `tools/build.py src/NAME.hints` says IDENTICAL for every program.
The hints' syntax is the docstring of tools/disasm.py.

**The image.** `exe` (MZ), `pmax` (a pMAX flat image), `le` (an LE
image, also inside a DOS/4GW MZ file) or `bin` (a raw image without a
header, 32-bit or with `use16` 16-bit: a driver a program loads and
calls). A packed program is unpacked first; the runner runs it as it
is. LZEXE 0.91 ("LZ91" at 1Ch; 0.90 is refused) is the kit's:
`tools/unlzexe.py` writes the program to build/unpacked/, its hints
name that file (`exe build/unpacked/...`) and say `linker unpacked
GAME/GAME.EXE`, as the packer kept neither the header's layout nor the
relocations' segments; build.py and check.py then unpack it themselves
when the file is not there, run.py takes the hints' names for the
packed program it runs, and the port loads the packed file as the
program it unpacks to (`rmem.h`). Check the unpacked image against the
packed program stopped at its own entry in the runner (`-break`,
`-ram`). EXEPACK and PKLITE need an unpacker from elsewhere.

**Segments.** One `segment` line per segment in image order. Where they
are: the relocation table's values (each relocated word is a segment's
frame), the header's SS:SP (the stack segment), what `build.py` reports
when a segment is missing, and debug information if there is any
(`cv4.py` reads CodeView NB08). The code segment must be called `CODE`,
the one DS normally holds `DATA` (the generated ASSUME uses the names).
The rebuild keeps the hints' image order, also when code follows data.
`keeptail` keeps what follows the image (debug information) as it is.

**Many code segments.** A program made of modules (a compiled one, or
assembly in many source files) has a code segment per module, BYTE or
WORD aligned: each begins where the one before ends, in the middle of a
paragraph, and its frame is that paragraph. `start=N` on the segment
line says at which offset of its frame it begins; the lowest offset a
far CALL/JMP enters it by is the first guess (build.py then reports a
jump to a label that is not there, or a segment grown by a paragraph,
where the guess is wrong). Labels are the segment's first letter and the
offset, so two code segments' labels collide: give each segment its own
`prefix=` (disasm.py stops and says so).

**The linker.** Microsoft LINK's header (relocations from 1Eh) is the
default. Borland's TLINK (`linker tlink VER`) writes its mark 01 00 FB
VER 6A 72 at 1Ch and the relocations from 3Eh; `layout=compact` is the
shorter form with the relocations from 22h and the marker word 6A72h at
20h; `header=original` keeps the original's zero padding after the
relocations. TLINK writes the relocations in the order it meets the
fixups in the object records, which the source does not say:
`relocorder original` takes the order from the original's table (the
set is still compared); `relocorder SEG...` gives it by the segment
holding the site.

**The assembler or compiler.** The original was made by some assembler
or compiler whose choices `tasm.py` must repeat where an instruction has
two encodings. `build.py` reports each instruction encoded otherwise;
the pattern tells which it was and the `asm` line follows it:

- every jump in its shortest form, no NOP padding: a multi-pass
  assembler (MASM 6, or TASM with /m);
- `LEA reg,[addr]` kept as LEA: MASM (`lea_smart=0`);
- `ADD AX,5` as `83 C0 05`: MASM (`alu_ax_short=0`);
- `TEST r1,r2` with r1 in r/m (`test_form=rm_reg`); register-to-register
  ALU and MOV with the destination in r/m (`regreg_form=rm_reg`);
- Borland C: `AND`/`OR` with a small constant in the word form
  (`imm8_alu=add,adc,sbb,sub,cmp,xor`), `XCHG AX,reg` as 87h /r
  (`xchg_ax_short=0`), `test_form=rm_reg`.

What does not follow a rule is a `raw` hint on the instruction: a DB in
the original source, a compiler's jump at the edge of the short range
sized otherwise than TASM's estimate, a run time library's assembly
module among compiled code. build.py lists the candidates each round.

**Reaching the code.** disasm.py follows jumps and calls from the entry
points, tracking what DS and ES hold. Then `tools/gaps.py` lists what of
CODE is not reached: every gap is either code reached through a pointer
or data. For code, find the pointer and say where it comes from in a
comment: `code` (an entry point; also names the label), `words` (a table
of near pointers; `stride=N` for one in each record of N bytes),
`rwords` (a compiled switch: 16-bit offsets from the table's own start;
`stride=N`, `from=OFF`), `ptr` (an immediate that is a code offset: `MOV
DX,offset handler` before INT 21h AH=25h), `coderange` (a run of
handlers). gaps.py shows where each gap's address appears as a word,
which usually is the pointer. Whether a gap is code the analysis misses
or code that never runs, a run tells: `run.py -cover FILE ...` writes
where instructions began (for an LE program also where the extender
mapped its objects), `gaps.py --seg all --cover FILE` marks the gaps
that ran (`RAN`, with the first address); a `-break` there gives SS:SP,
a second run with `-dump SS:SP N` the return address of its caller.
Leave data as data. Code that runs into data after a call that never
returns (an exit through a service the analysis does not know) gets a
`stop` hint on that instruction.

**Addresses written as numbers.** An immediate or displacement that is
really an address does not show in the byte comparison, only once code
is translated or data moves. `tools/ptrscan.py` finds MOV r16,imm whose
register is then used as a base; check each by eye before adding a
`dptr`. A `dptr`/`ptr` on an instruction without an immediate names its
displacement (`CALL [SI+table]`). A displacement with a register below
100h is taken for a field offset unless such a hint says otherwise, or
it is written in two bytes though it fits one (the assembler did not
know it: an address, a compiler's `array[BX]`), or it lands in reached
code.

**Two programs, one engine.** When a game ships two builds of one
engine (two episodes, a demo and the full game), analyse one, then
`tools/xfer.py A.hints B.hints` carries its hints to the other by
aligning the instruction streams (a code address only when the next ten
instructions match). A program of many code segments is aligned as all
of them in image order; the segment names map by where the matched code
lies and by the relocated words in it. B.hints keeps its own lines
(exe, segment, linker, relocorder, asm, keeptail and its own names) above
the carried block; the block is written by xfer.py only, and check.py
tells when it is out of date. A.hints may lie in a folder below
B.hints' (`src/old/A.hints`, a build no longer worked on: check.py
builds `src/*.hints` only); the block's first line names it from
B.hints' folder.

**Names for the C.** `tools/symmap.py port/src/gen/names.h PREFIX
KEY=src/A.hints ...` writes where every name is in each program, the
file sizes and SHA-256 (the port checks it loads the file the hints
describe). check.py finds the header by its first line and checks it.

What was learned:

- capstone names 98h/99h `cwde`/`cdq` in 16-bit mode; disasm.py renames
  them to CBW/CWD.
- Shifts and rotates of memory by CL keep an explicit BYTE/WORD/DWORD
  PTR: CL cannot tell the operand's width.
- A name given twice is refused; give each place its own.
- The source is regenerated from the hints every time; never edit
  build/*.ASM.
- Hints and docs may have CRLF line ends (Windows); tools that write
  hints on a Mac or Linux write LF. Keep one convention per repository
  (`.gitattributes` has `* -text`: files go in as they are).

## 2. Understanding (stage 2)

Goal: names, records and rules in the hints, each claim checked by a
run or marked as read from the code ("presumably", "not checked").

**The runner.** `tools/run.py [options] PROGRAM [ARGS]` runs a program
in `dosrun`, on an emulated clock (6 M instructions a second by default,
`-ips`): nothing reads the host's clock, the same arguments give the
same run, and the report ends with hashes of memory and video memory.
It has:

- a 386 CPU: real mode, protected mode without paging and task
  switches, V86 mode (see "protected mode" in tools/run/cpu.c), INT 15h
  AH=87h/88h/89h; no x87;
- VGA (planar, chain-4, Mode X, the retrace) and as much VESA as a
  program needs that sets a mode and then programs the card itself
  (4F00h..4F03h, modes 100h, 101h, 103h, 256 KB, no bank switching);
- PIT, PIC, keyboard, BIOS; a Microsoft-compatible INT 33h mouse (its
  event handlers called on the spot in real mode, through IRQ 12 when
  the program is in protected mode); a game port (201h; no stick unless
  `-joy` gives one);
- both 8237 DMA controllers, a Sound Blaster 16 (DSP and mixer
  configuration, so a game's own drivers run) and an OPL2 at 388h with
  its timers;
- MSCDEX for D:, a disc of one data track or, with `-cue`, the tracks
  of a cue sheet such as GOG's `game.inst` (Ogg Vorbis through
  third_party/stb_vorbis);
- a small DOS whose drives are all the game's files with a writable
  layer (`-state`, default `build/run/state`), each drive with its own
  current directory; programs start on D:. It answers what programs ask
  of DOS's internals with fixed values: a US country block (AH=38h), an
  empty DBCS table (AX=6300h), a List of Lists at F000:0EA0 with only
  the first MCB, LASTDRIVE, the boot drive and the NUL device (AH=52h:
  no DPBs, SFT or CDS, those pointers FFFF:FFFF). Unused vectors
  60h..FFh share one IRET, so an extender finds free ones by comparing
  them.

Options (all at the top of `tools/run/main.c`):

- input: `-key T KEY`, `KEY+`/`KEY-`, `-keys FILE`; `-mouse T X,Y,B`,
  `-mice FILE`; `-joy T N,X,Y,B`, `-joys FILE` (X and Y the microseconds
  the one-shots stay high, some 570 for a centred stick; `N,off` takes
  it away); `-keyat ADDR[#N] KEY+`, `-keysat ADDR FILE` (keys by the
  passes of a frame loop, not by time); `-put GUEST HOST` (a
  configuration file the set-up program would have written);
- stopping and watching: `-until T`, `-break ADDR[#N]`, `-log ADDR`,
  `-poke ADDR[#N] TARGET HEX`, `-watch ADDR` (who writes a byte),
  `-rwatch ADDR LEN` (who reads), `-trace FILE N`, `-dos` (every INT 21h
  call), `-intwatch NN`, `-prof`, `-cover FILE`, `-criterr AH ERR` (the
  next such call fails through INT 24h), `-requirements` (which devices
  and memory interfaces the run used), `-v`;
- results: `-dump ADDR LEN`, `-dumpevery DT`, `-ram`, `-mem` (all 16 MB
  linear), `-vram`, `-dac`, `-vgastate`, `-shot`, `-shotevery`, `-wav`
  and `-sb` (the Sound Blaster), `-oplwav` (the OPL2 through
  runtime/opl.c: the chip's sound, not its exact samples), `-cd`,
  `-cdwav` (from t=0, not mixed with `-wav`, which starts at the first
  transfer);
- `-loadfix` (load above 64 KB: EXEPACK's unpacker fails below it).

run.py translates addresses by the hints' names (`CODE:1387`,
`DATA:9AA0+4`, `L13B2`, a `name`, `PROG:name` for another program's
hints, offsets up to eight digits for 32-bit images); a loader shares
its base name with the image it loads, whose addresses translate with
`-base LIN` (the image's linear base, found in a run). A stop in
protected mode prints the registers, the cached segment bases, the
linear address, CR0 and the GDT: use those with `-mem`, not selectors
as real-mode segments.

To get the game somewhere without a window (a place in a level, a rare
event): key scripts with random presses, many seeds in parallel, a
`-log` on the routine that should be reached; then repeat the seed that
got there with `-dump`/`-shot`.

Not in the runner: paging and task switches (so no extender that needs
them), VCPI/DPMI hosts, x87 instructions, EMS/XMS, the PC speaker's
sound, the OPL at the Sound Blaster's ports (2x8h, 2x9h), savestates, a
window.

**Files.** Each data format gets a tool in the project
(`tools/NAMEfiles.py`) that parses the files and writes them back to
identical bytes, and shows them (pictures as PNG). A format understood
this way is understood. Formats many games share are the kit's
(`flifiles.py` for FLI/FLC).

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
(`rmem.h`: `rm_load_exe` with `RM_LOAD_PSP`, the SHA-256 from symmap.py's
header; the BIOS data area filled as the runner fills it) and translates
routine by routine; the C reads and writes that memory through
`rb`/`rw`/`frb`... by the hints' names, so records that point at each
other with 16-bit offsets keep working, and a routine not yet translated
can stop the program with its name. Video goes through `vga.c` (the same
ports and planes the assembly used; it starts in text mode 3, as DOS
leaves the card), the frame wait through `frame.c`. Both are stepping
stones: once everything is C, data moves into structures and pictures
into bitmaps, one at a time.

A protected-mode program goes into `pmem.h`'s 16 MB of linear memory
instead: `pm_load` puts the image where the extender put it in dosrun
and gives its selector relocations the selectors the extender gave
(both found once in a run: `-break` at the image's entry, `-mem`, the
image's bytes searched for), and the C reads by 32-bit offsets at
`pm_ds` (`rb`/`rw`/`rd`...) or by linear address (`lrb`...). The
extender's own tables and heap are not modelled; compare with `-mem A`
in dosrun, `pm_write` in the port and `memcmp.py ... --base LINEAR`.

**Compare.** Stop the original in dosrun at a routine (`-break name#N
-ram A -vram AV`) and the port at the same pass (its own counter;
`rm_write`, `vga_write_planes`), then `tools/memcmp.py HINTS A B --vram
AV BV --skip STACK`: the runs of differing bytes with the name at or
before each. Equal but for what has no reason to be (the stack, a saved
vector, a driver's scratch space) is the goal; write down where and how
each check was made. The same keys: `frame_record` writes each key byte
with its picture number, which becomes a key script for dosrun (time =
start + picture / rate: find the start and the rate once with `-log` on
the original's frame wait), or `-keysat` on the frame wait directly.
A script that does all this for one key script belongs to the project.

**The frame.** A DOS game's frame is paced by the retrace or a timer
tick (often a sound driver's callback). frame.c keeps the video mode's
refresh rate, runs a tick callback and hands the keys over first; for a
loop that waits on the keyboard alone `frame_wait_keys` lets the program
go on before the tick, as a fast PC did. Measure the rate the original's
tick has in dosrun before trusting 70 Hz.

**Sound.** Music modules play through `modplay.c` (micromod) with the
driver's timing where the game depends on it (`modplay_jump`,
`modplay_set_jump_hook`); effects as notes put into a channel
(`modplay_note`). AdLib music goes through `opl.h` (the OPL2) and
`oplq.h`: a port that makes its timer's interrupts on its own thread
counts them by the samples played and stamps each register write with
its sample (`oplq_played` plus a lead), and `oplq_render` in the audio
callback makes each write at its sample, the same headless and in a
window. CD audio (MSCDEX's plays) comes from the release's cue sheet
through `cdaudio.c`, mixed in the audio callback (`cda_mix`). Other
formats need their own player.

**Keys through the BIOS.** A program with no INT 9 handler of its own
asked INT 16h: `bioskey.c` is that keyboard (hand `bioskey_put` to
`frame_set_keyboard`; `bioskey_peek` and `bioskey_read` are AH=1 and
AH=0), with the characters the runner's BIOS gives, so a key script
means the same in the port.

**The platform.** `platform.h`: a window showing an indexed picture in
the 4:3 of a VGA screen (Alt+Enter full screen; tear-free presents with
`plat_set_vsync`; Print Screen writes screenshot_NNNN.png, `shot.h`;
full screen on a Steam Deck), PC scan codes, a monotonic clock, an audio
callback, the mouse (as a pointer on the picture, `plat_mouse`, or as a
device with buttons held and movement, `plat_mouse_motion`, kept to the
window by `plat_mouse_grab`). `plat_null.c` runs headless on a virtual
clock for tests (`DK_FRAMES`, `DK_KEYS`, `DK_MOUSE`, `DK_MOUSEMOVE`,
`DK_DUMP`, `DK_SHOTS`); with `DK_AUDIO=1` or `DK_WAV=file` it has a
virtual audio device whose callback is asked for the clock's samples
each time `plat_sleep_ms` moves it on. That clock moves only in
`plat_sleep_ms`: what the program does between two sleeps takes no time.
Controllers become keys (`pad.h`, a table per situation). `textmode.h`
draws an 80x25 screen in the style of DOS's; `hud.h` a box at the top of
the picture (the volume keys, docs/PLAYER-SETTINGS.md); `fli.h` plays
FLI/FLC. `cdimage.h` finds the GOG release and on the first start
unpacks its CD image or copies its folder; `inno.h` reads a Windows
installer; `update.h` offers a newer release when the player agrees.

**Modernising.** Once equal: the common launcher (setup screen: options,
sound, controller) described by docs/LAUNCHER.md, with the port
supplying only its pages and items; quality of life fixes (switchable,
off in comparisons), skipping intros, the original's bugs fixed where
they bother, each such change documented as a change from the original.

**Releases.** Every port ships the same way on every platform, as
docs/RELEASE.md says: one program to double click (on a Mac one app),
nothing to install, the GOG release found by itself, a README.txt for
players, uninstall scripts. The template's workflow builds these
packages.

**Wording.** A port made this way is a native compatibility
implementation requiring an installed copy of the game: it contains only
its own code, and the player's installed release supplies the game's
data at run time. Never "standalone" or "completely native".

## Limits

Worked through to a port: real-mode MZ programs written in assembly
(MASM/TASM), linked by LINK, 186/386 instructions in 16-bit code;
pMAX flat images (the port on `pmem.h`); raw 32-bit modules (`offrel`)
and 16-bit drivers (`use16`); Watcom C. Rebuilt byte for byte, port
under way: compiled Borland C programs of many code segments linked by
TLINK. Not, or only in part:

- Microsoft C: not tried; expect other idioms and new `asm` options;
- overlays: not tried;
- LE images: objects with uncompressed pages are read and rebuilt,
  internal selector fixups used; iterated and compressed pages are
  refused. LE programs have been rebuilt byte for byte from their
  hints; a DOS/4GW program runs in the runner as far as its extender
  needs no paging.

Each new game will stretch one of these; extend the kit (with a test in
tests/) rather than the project.
