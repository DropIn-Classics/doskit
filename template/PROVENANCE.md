# Provenance

## Purpose

This document records the provenance of the code in this repository.

The project is an independently implemented native compatibility runtime for a legally installed copy of the original game.

It does not contain or distribute the original game's executable, source code, graphics, music, sound, text, levels, or other game data.

The original game data is supplied by the user's own installed GOG release and is accessed or copied locally by the compatibility runtime as required.

---

## Development model

Game-specific behaviour is reconstructed exclusively from the legally obtained GOG release of the game.

Analysis may include:

- execution and runtime observation
- debugging and tracing
- examination of executable behaviour
- examination of memory and register state
- inspection of file access patterns
- analysis of game data formats
- input/output comparison
- behavioural testing
- automated differential and regression testing

The implementation is written independently from the results of that analysis.

No original game source code is used as implementation material.

---

## Game-specific code

All game-specific code in this repository must be derived from behaviour observed in this game's own GOG release.

This includes, but is not limited to:

- gameplay logic
- physics
- scoring
- state machines
- table or level behaviour
- AI
- timing behaviour
- game-specific data structures
- game-specific algorithms
- game-specific constants
- file formats specific to the game

Implementations from other games must not be used to infer undocumented behaviour of this game.

If two games appear to behave similarly, the behaviour must still be verified independently against this game's original executable before being implemented.

---

## Shared infrastructure

The project may reuse independently written, game-agnostic infrastructure developed for other compatibility projects.

Examples include:

- platform abstraction
- filesystem helpers
- DOS compatibility helpers
- BIOS compatibility helpers
- CPU emulation or translation support
- VGA and palette handling
- renderer infrastructure
- audio output
- audio mixing
- MOD playback
- MIDI support
- input handling
- controller support
- timing facilities
- logging
- build systems
- installers
- launchers
- GOG installation detection
- extraction helpers
- debugging tools
- assemblers, disassemblers, and other analysis tools

Such components must not contain game-specific logic originating from another title.

Reuse of generic infrastructure does not permit reuse of another game's gameplay implementation.

---

## Original game data

No original game data is distributed with this repository or its binary releases.

The compatibility runtime requires the user to possess and install the supported GOG release.

Where the GOG release stores game data inside disc images or other containers, the runtime may locate and extract those files locally on the user's system.

These extracted files remain original game data and are not part of this project.

---

## Original executable

The original executable may be examined for the purpose of understanding runtime behaviour.

The original executable is not redistributed.

No binary blobs, machine-code fragments, or copied sections of the original executable may be embedded in the compatibility runtime unless their redistribution rights are separately established and documented.

---

## Original source code

Original source code for the game is not used as an implementation source.

If purported original source code, leaked source code, historical source trees, or unofficial source archives become available, they must not be used to implement or modify game-specific code.

Developers and coding agents working on this project should treat such material as out of scope.

---

## Other games and related projects

Other game implementations may be consulted only for independently written, generic infrastructure.

They must not be used as authoritative references for this game's behaviour.

In particular, game-specific code from another title must not be:

- copied
- translated
- adapted
- structurally reproduced
- used to infer undocumented behaviour
- used as a substitute for analysis of this game's own executable

Behaviour must be established independently from this game's supported GOG release.

---

## AI-assisted development

AI coding tools may be used for analysis and implementation.

They are subject to the same provenance rules as human contributors.

When working on game-specific code, AI tools must not use source code or game-logic implementations from other titles as implementation material.

Prompts and persistent project instructions should explicitly preserve this restriction.

---

## Verification

Where practical, reconstructed behaviour should be verified against the original GOG version using reproducible tests.

Possible verification methods include:

- identical input sequences
- save-state comparison
- memory-state comparison
- rendering comparison
- audio-event comparison
- score comparison
- physics comparison
- deterministic replay
- regression tests

The purpose of verification is to confirm behavioural compatibility, not to reproduce the internal structure of the original program.

---

## Repository policy

Contributors must not add:

- original game assets
- original executables
- original source code
- decompiled source intended as implementation material
- copied machine-code fragments
- game-specific code derived from unrelated ports
- copyrighted material whose redistribution rights are unclear

Any exception must be explicitly documented together with its license and provenance.

---

## Project description

The preferred description of this project is:

> A native compatibility implementation requiring an installed copy of the original game.

The project should not be described as:

- a standalone version of the game
- a complete redistribution of the game
- an official port
- an official remaster

unless such a description becomes factually and legally accurate.

---

## Supported original release

Game:
`{{NAME}}`

Supported release:
`GOG release`

Required original installation:
`Yes`

Original game data redistributed:
`No`

Original executable redistributed:
`No`

Original source code used:
`No`

Game-specific implementation derived from:
`Runtime analysis and reverse engineering of the supported GOG release`

---

## Provenance changes

If the development method changes in a way that affects provenance, this document must be updated.

Do not rewrite or falsify version-control history in order to create the appearance of cleaner provenance.

The purpose of this document is to record the actual development process accurately.