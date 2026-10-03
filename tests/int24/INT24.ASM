; INT24.EXE - runner test: an INT 24 handler may return Fail (AL=3) to
; a forced INT 21h/AH=3Dh critical error.  Without forcing, the ordinary
; missing-file result must not enter the handler.
.186
CODE SEGMENT PARA PUBLIC 'CODE'
	ASSUME CS:CODE,DS:DATA,SS:STACK
start:
	MOV AX,DATA
	MOV DS,AX
	MOV DX,OFFSET critical
	PUSH CS
	POP DS
	MOV AX,2524H
	INT 21H
	MOV AX,DATA
	MOV DS,AX
	MOV DX,OFFSET missing
	MOV AX,3D00H
	INT 21H
	JNC failed
	CMP WORD PTR hits,0
	JNE injected
	CMP AX,2
	JNE failed
	MOV DX,OFFSET no_fault
	JMP print
injected:
	CMP WORD PTR hits,1
	JNE failed
	CMP AX,0CH
	JNE failed
	MOV DX,OFFSET passed
print:
	MOV AH,9
	INT 21H
	MOV AX,4C00H
	INT 21H
failed:
	MOV AX,4C01H
	INT 21H

critical:
	PUSH DS
	MOV AX,DATA
	MOV DS,AX
	INC WORD PTR hits
	POP DS
	MOV AL,3
	IRET
CODE ENDS

DATA SEGMENT PARA PUBLIC 'DATA'
hits DW 0
missing DB 'MISSING.DAT',0
no_fault DB 'int24 no fault ok',13,10,'$'
passed DB 'int24 ok',13,10,'$'
DATA ENDS

STACK SEGMENT PARA STACK 'STACK'
	DB 100H DUP (?)
STACK ENDS

	END start
