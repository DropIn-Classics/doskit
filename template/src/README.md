# Hints

One `NAME.hints` per program of the game (syntax: the docstring of
doskit/tools/disasm.py). Start with:

    ; NAME.EXE: what it is
    ; Hints for doskit/tools/disasm.py; numbers are hex.

    exe FOLDER/NAME.EXE

    ; the segments, from the relocations and the header (SS:SP = ....)
    segment CODE  0000 CODE
    segment DATA  xxxx DATA
    segment STACK xxxx STACK stack size=xxx

    relocorder CODE

then `python3 doskit/tools/build.py src/NAME.hints` and
`doskit/tools/gaps.py` in turn (doskit/docs/METHOD.md, stage 1).
