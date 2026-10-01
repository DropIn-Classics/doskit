; RWATCH.EXE - the kit's test of the runner's -rwatch on a table with
; many readers (tests/selftest.py): 200 bytes at DATA:0000 (the program's
; frame 0004, after the code), each read once by the LODSB of a loop and
; the first 100 of them once more by a CMP, so 300 readers (an instruction
; and the byte it read), more than the 64 the runner once kept.  Prints
; "rwatch ok" when the sum is the table's.  Written for the kit; no
; game's code.
CODE SEGMENT PARA PUBLIC 'CODE'
	ASSUME CS:CODE,DS:DATA,ES:NOTHING,SS:STACK
start:
	MOV AX,DATA
	MOV DS,AX
	CLD
	XOR SI,SI
	XOR BX,BX
	MOV CX,200
sum:
	LODSB
	MOV AH,0
	ADD BX,AX
	LOOP sum
	XOR SI,SI
	MOV CX,100
again:
	CMP BYTE PTR [SI],3
	JNE failed
	INC SI
	LOOP again
	CMP BX,600
	JNE failed
	MOV DX,OFFSET text_ok
	MOV AH,9
	INT 21H
	MOV AX,4C00H
	INT 21H
failed:
	MOV DX,OFFSET text_failed
	MOV AH,9
	INT 21H
	MOV AX,4C01H
	INT 21H
CODE ENDS

DATA SEGMENT PARA PUBLIC 'DATA'
table	DB 200 DUP (3)
text_ok	DB 'rwatch ok',13,10,'$'
text_failed DB 'rwatch FAILED',13,10,'$'
DATA ENDS

STACK SEGMENT PARA STACK 'STACK'
	DB 100H DUP (?)
STACK ENDS

	END start
