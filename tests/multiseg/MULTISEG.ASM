; MULTISEG.EXE, a test program of the kit's own (tests/selftest.py): code
; in several segments that are BYTE and WORD aligned, so each begins in the
; middle of a paragraph after the one before (as a compiled program's
; modules do), with routines at the same offsets in different segments;
; a segment's value stored into memory before the segment is defined;
; written with TLINK's header and its relocations in an order that is not
; the address order (selftest.py makes it so).
.186
CODE SEGMENT PARA PUBLIC 'CODE'
	ASSUME CS:CODE,DS:DATA,ES:NOTHING,SS:NOTHING
start:
	MOV AX,DATA
	MOV DS,AX
	CALL FAR PTR first
	CALL FAR PTR second
	CALL FAR PTR third
	CALL FAR PTR fourth
	MOV AH,9
	MOV DX,OFFSET text
	INT 21H
	MOV AX,4C00H
	INT 21H
	NOP			; CODE ends in the middle of a paragraph
CODE ENDS

ONE SEGMENT BYTE PUBLIC 'CODE'
	ASSUME CS:ONE
count	DB 0
first	PROC FAR
	INC BYTE PTR CS:[count]
	CMP BYTE PTR CS:[count],3
	JB first_done
	MOV BYTE PTR CS:[count],0
first_done:
	RETF
first	ENDP
ONE ENDS

TWO SEGMENT WORD PUBLIC 'CODE'
	ASSUME CS:TWO
second	PROC FAR
	PUSH DS
	MOV AX,DATA
	MOV DS,AX
	MOV BYTE PTR DS:[text],'M'
	POP DS
	CALL FAR PTR first
	RETF
second	ENDP
	DB 'second ends..', 0	; so that third is at second's offset (6)
TWO ENDS

THREE SEGMENT BYTE PUBLIC 'CODE'
	ASSUME CS:THREE
third	PROC FAR
	PUSH BP
	MOV BP,SP
	PUSH AX
	MOV WORD PTR [BP-2],DATA	; a segment defined later, stored (C7 /0)
	POP AX
	POP BP
	MOV AX,SEG second
	OR AX,AX
	JZ third_done
	CALL FAR PTR first
third_done:
	RETF
third	ENDP
THREE ENDS

; encoded as Borland C does (selftest.py assembles it so; the hints'
; asm line says it): AND and OR with a small constant in the word form,
; ADD in the byte form, XCHG AX,reg as 87h /r; an array in DATA at a small
; offset indexed by BX (its displacement written in full, as the linker
; fills it in); a far call to a routine of the same segment
FOUR SEGMENT BYTE PUBLIC 'CODE'
	ASSUME CS:FOUR
fourth	PROC FAR
	PUSH BP
	MOV BP,SP
	AND WORD PTR [BP+6],3
	OR DI,0CH
	ADD DI,4
	XCHG AX,BX
	MOV AL,BYTE PTR [BX+text+2]
	CALL FAR PTR fourth_end
	POP BP
	RETF
fourth	ENDP
fourth_end PROC FAR
	RETF
fourth_end ENDP
FOUR ENDS

DATA SEGMENT PARA PUBLIC 'DATA'
text	DB 'multiseg', 13, 10, '$'
	DD first
	DD second
DATA ENDS

STACK SEGMENT PARA STACK 'STACK'
	DB 100H DUP (?)
STACK ENDS
	END start
