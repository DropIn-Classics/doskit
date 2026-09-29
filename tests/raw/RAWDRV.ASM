; A raw 32-bit image for the kit's test of the `raw` kind (tests/selftest.py):
; written for the kit, no game's code.  Shaped like a driver a program
; loads into a segment of its own and calls at offset 0: a jump over its
; data to a dispatcher that takes a command number in EAX and returns far.
; Assembled with tasm.py and linked with tlink.py, written as the image
; alone; then disasm.py and build.py rebuild it from tests/raw/src/RAWDRV.hints.

.386
CODE SEGMENT PARA PUBLIC USE32 'CODE'
ASSUME CS:CODE, DS:CODE

    JMP DISPATCH
BANNER      DB 'raw image of the kit', 0
STATE       DD 0
COMMANDS    DD C0, C1, C2

DISPATCH:
    CMP EAX, 3
    JAE BAD
    CALL DWORD PTR CS:COMMANDS[EAX*4]
    CLC
    RETF
BAD:
    STC
    RETF

C0: MOV DWORD PTR CS:STATE, 1
    RET
C1: ADD DWORD PTR CS:STATE, EBX
    RET
C2: MOV EAX, CS:STATE
    MOV ESI, OFFSET BANNER
    RET

CODE ENDS
END
