; HELLO.EXE - the kit's own test program (tests/selftest.py): prints a
; line, sets mode 13h, then calls two routines through a table of near
; pointers, which the analysis cannot follow without a `words` hint.
; Written for the kit; no game's code.
.186
CODE SEGMENT PARA PUBLIC 'CODE'
	ASSUME CS:CODE,DS:DATA,ES:NOTHING,SS:STACK
start:
	MOV AX,DATA
	MOV DS,AX
	MOV DX,OFFSET greeting
	MOV AH,9
	INT 21H
	MOV AX,13H
	INT 10H
	MOV AX,0A000H
	MOV ES,AX
	XOR BX,BX
next_step:
	MOV SI,BX
	SHL SI,1
	CALL WORD PTR [SI+steps]
	INC BX
	CMP BX,2
	JB next_step
	MOV AX,4C00H
	INT 21H
fill_band:
	XOR DI,DI
	MOV CX,6400
	MOV AL,2
	REP STOSB
	INC WORD PTR [counter]
	RET
fill_dot:
	MOV DI,32160
	MOV BYTE PTR ES:[DI],4
	ADD WORD PTR [counter],10H
	RET
CODE ENDS

DATA SEGMENT PARA PUBLIC 'DATA'
greeting DB 'hello from doskit',13,10,'$'
counter	DW 0
steps	DW fill_band,fill_dot
DATA ENDS

STACK SEGMENT PARA STACK 'STACK'
	DB 100H DUP (?)
STACK ENDS

	END start
