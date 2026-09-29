; A 32-bit program for the kit's test of pMAX images (tests/selftest.py):
; written for the kit, no game's code.  Assembled with tasm.py, linked
; with tlink.py and written as a pMAX image by build.write_pmax; then
; disasm.py and build.py rebuild it from tests/flat/src/FLAT.hints.
; CODE (descriptor 0) holds code and data, as a flat program has them;
; MORE (descriptor 1) is a second segment reached through a selector.

.386
CODE SEGMENT PARA PUBLIC USE32 'CODE'
ASSUME CS:CODE, DS:CODE

SAVED_DS    DW 0
COUNT       DD 0
HANDLERS    DD H0, H1, H2
FAR_SUB     DF H1
TEXT        DB 'flat image of the kit', 0

START:
    MOV AX, CODE
    MOV DS, AX
    MOV SAVED_DS, AX
    MOV AX, MORE
    MOV ES, AX
    MOV EAX, ES:OTHER               ; a label in MORE, through ES
    MOV COUNT, EAX
    MOV ESI, OFFSET TEXT            ; an offset (ptr hint)
    CALL SHOW
    XOR EBX, EBX
NEXT:
    CALL DWORD PTR HANDLERS[EBX*4]  ; the table of near pointers (words hint)
    INC EBX
    CMP EBX, 3
    JB NEXT
    CALL FWORD PTR FAR_SUB          ; a 48-bit far pointer
    MOV EDX, [EBX+ESI*2-200H]       ; a negative 32-bit displacement
    MOV CX, WORD PTR [EDX+4]
    SHL CX, 1
    PUSH FS
    MOV FS, SAVED_DS
    LODS BYTE PTR FS:[ESI]
    POP FS
    REP MOVSD
    DB 0DBH, 0E3H                   ; FNINIT: tasm.py has no x87, nor has build.py then
    MOV EAX, COUNT
    RETF

SHOW:
    LODSB
    OR AL, AL
    JNZ SHOW
    RET

H0: INC COUNT
    RET
H1: ADD COUNT, 10H
    RET
H2: SUB DWORD PTR COUNT, 1000H
    JC QUIT
    JMP SWITCH
QUIT:
    MOV AX, 0
    INT 93H                         ; does not return (stop hint)
    DB 0C0H, 70H, 0, 0A0H           ; data, which decodes as SAL /6

SWITCH:                             ; a compiled switch (rwords hint)
    MOVZX EDI, BYTE PTR COUNT
    AND EDI, 3
    MOVSX EDI, WORD PTR SWTAB[EDI*2]
    LEA EDI, [EDI+SWTAB]
    JMP EDI
S0: RET                             ; before the table: a negative offset
SWTAB DW S0-SWTAB, S1-SWTAB, S2-SWTAB, S0-SWTAB
S1: INC COUNT
    RET
S2: DEC COUNT
    RET

CODE ENDS

MORE SEGMENT PARA PUBLIC USE32 'DATA'
            DB 'more'
OTHER       DD 12345678H
MORE ENDS

END START
