; A relocatable raw 32-bit module for the kit's test of `offrel`
; (tests/selftest.py): written for the kit, no game's code.  Shaped like a
; module a host loads anywhere and relocates by adding its base to every
; dword of a list: a header of pointers (one with a flag in its top bit),
; code the host calls through them, offsets in immediates and
; displacements, and numbers that only look like offsets.  The selftest
; writes the image and the list of its OFF32 fixups (the offrel list),
; then disasm.py and build.py rebuild it from tests/relmod/src/RELMOD.hints.

.386
CODE SEGMENT PARA PUBLIC USE32 'CODE'
ASSUME CS:CODE, DS:CODE

HEADER      DD INIT, STEP, 0, NAMETXT+80000000H
COUNT       DD 0
LIMIT       DD 30H                  ; a number, as small as an offset
TABLE       DD STEP_A, STEP_B
NAMETXT     DB 'RELMOD', 0

INIT:
    MOV DWORD PTR COUNT, 0
    MOV EAX, OFFSET TABLE
    MOV ECX, 20H                    ; a number: where INIT's code is
    RET

STEP:
    MOV EBX, COUNT
    AND EBX, 1
    CALL DWORD PTR TABLE[EBX*4]
    INC DWORD PTR COUNT
    MOV EAX, COUNT
    CMP EAX, LIMIT
    RET

STEP_A:
    MOV ESI, OFFSET NAMETXT
    RET
STEP_B:
    LEA EDI, [EBX+NAMETXT]
    RET

CODE ENDS
END
