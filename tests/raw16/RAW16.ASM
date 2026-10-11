; A raw 16-bit image for the kit's test of `bin ... use16` (tests/selftest.py):
; written for the kit, no game's code.  Shaped like a real-mode driver a
; program loads into a segment of its own and calls through pointers: a
; text header, a table of near entry offsets the host reads, the entries
; returning far, and a hardware interrupt handler the host never calls.
; Assembled with tasm.py and linked with tlink.py, written as the image
; alone; then disasm.py and build.py rebuild it from tests/raw16/src/RAW16.hints.

CODE SEGMENT PARA PUBLIC 'CODE'
ASSUME CS:CODE, DS:CODE

HEADER      DB 'raw 16-bit image of the kit', 0
ENTRIES     DW INIT, SETVOL, TICK
STATE       DW 0
VOLUME      DB 0

INIT:
    MOV WORD PTR CS:STATE, 1
    XOR AX, AX
    RETF
SETVOL:
    PUSH BP
    MOV BP, SP
    MOV AL, [BP+6]
    MOV CS:VOLUME, AL
    POP BP
    RETF
TICK:
    INC WORD PTR CS:STATE
    MOV AX, CS:STATE
    RETF
IRQ:
    PUSH AX
    INC WORD PTR CS:STATE
    MOV AL, 20H
    OUT 20H, AL
    POP AX
    IRET

CODE ENDS
END
