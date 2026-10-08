; Independently written test of the registers a REP string instruction
; leaves: STOS moves DI and CX only and SI stays, MOVS moves SI and DI;
; with counts of 16 and more (the runner's block path) and below (its
; loop), forwards and backwards.
.186
CODE SEGMENT PARA PUBLIC 'CODE'
	ASSUME CS:CODE,DS:DATA,ES:DATA,SS:STACK
start:
	MOV AX,DATA
	MOV DS,AX
	MOV ES,AX
	CLD
; REP STOSB of 32 bytes: SI stays, DI is 32 further, CX is 0
	MOV SI,1234H
	MOV DI,OFFSET buf
	MOV CX,32
	MOV AL,5AH
	REP STOSB
	CMP SI,1234H
	JNE failed
	OR CX,CX
	JNE failed
	SUB DI,OFFSET buf
	CMP DI,32
	JNE failed
	CMP BYTE PTR buf,5AH
	JNE failed
	CMP BYTE PTR buf+31,5AH
	JNE failed
	CMP BYTE PTR buf+32,0
	JNE failed
; REP STOSW of 16 words
	MOV SI,0FFF0H
	MOV DI,OFFSET buf
	MOV CX,16
	MOV AX,0A55AH
	REP STOSW
	CMP SI,0FFF0H
	JNE failed
	OR CX,CX
	JNE failed
	SUB DI,OFFSET buf
	CMP DI,32
	JNE failed
	CMP WORD PTR buf+30,0A55AH
	JNE failed
	CMP BYTE PTR buf+32,0
	JNE failed
; REP STOSB of 8 bytes, below the block path
	MOV SI,4321H
	MOV DI,OFFSET buf
	MOV CX,8
	MOV AL,11H
	REP STOSB
	CMP SI,4321H
	JNE failed
	SUB DI,OFFSET buf
	CMP DI,8
	JNE failed
	CMP BYTE PTR buf+7,11H
	JNE failed
	CMP BYTE PTR buf+8,5AH
	JNE failed
	JMP SHORT moves
failed:
	MOV DX,OFFSET failure
	JMP print
moves:
; REP MOVSB of 32 bytes: SI and DI are both 32 further
	MOV SI,OFFSET buf
	MOV DI,OFFSET copy
	MOV CX,32
	REP MOVSB
	OR CX,CX
	JNE failed
	SUB SI,OFFSET buf
	CMP SI,32
	JNE failed
	SUB DI,OFFSET copy
	CMP DI,32
	JNE failed
	CMP BYTE PTR copy,11H
	JNE failed
	CMP WORD PTR copy+30,0A55AH
	JNE failed
; REP STOSB of 32 bytes backwards
	STD
	MOV SI,1234H
	MOV DI,OFFSET copy
	ADD DI,31
	MOV CX,32
	MOV AL,77H
	REP STOSB
	CLD
	CMP SI,1234H
	JNE failed
	OR CX,CX
	JNE failed
	SUB DI,OFFSET copy
	CMP DI,0FFFFH
	JNE failed
	CMP BYTE PTR copy,77H
	JNE failed
	CMP BYTE PTR copy+31,77H
	JNE failed
	CMP BYTE PTR buf+32,0
	JNE failed
	MOV DX,OFFSET passed
print:
	MOV AH,9
	INT 21H
	MOV AX,4C00H
	INT 21H
CODE ENDS
DATA SEGMENT PARA PUBLIC 'DATA'
passed DB 'strreg ok',13,10,'$'
failure DB 'strreg FAILED',13,10,'$'
buf DB 40H DUP (0)
copy DB 40H DUP (0)
DATA ENDS
STACK SEGMENT PARA STACK 'STACK'
	DB 100H DUP (?)
STACK ENDS
	END start
