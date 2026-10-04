; COUNTRY.EXE - runner test for INT 21h AH=38h, current-country information
.186
CODE SEGMENT PARA PUBLIC 'CODE'
	ASSUME CS:CODE,DS:DATA,SS:STACK
start:
	MOV AX,DATA
	MOV DS,AX
	MOV DX,OFFSET country
	MOV AX,3800H
	INT 21H
	JC failed
	CMP BX,1
	JNE failed
	CMP WORD PTR country,0
	JNE failed
	CMP BYTE PTR country+2,'$'
	JNE failed
	CMP BYTE PTR country+3,0
	JNE failed
	CMP BYTE PTR country+7,','
	JNE failed
	CMP BYTE PTR country+9,'.'
	JNE failed
	CMP BYTE PTR country+11,'/'
	JNE failed
	CMP BYTE PTR country+13,':'
	JNE failed
	CMP BYTE PTR country+15,0
	JNE failed
	CMP BYTE PTR country+16,2
	JNE failed
	CMP BYTE PTR country+17,0
	JNE failed
	CMP DWORD PTR country+18,0
	JNE failed
	CMP BYTE PTR country+22,','
	JNE failed
	MOV SI,OFFSET country+24
	MOV CX,10
reserved:
	CMP BYTE PTR [SI],0
	JNE failed
	INC SI
	LOOP reserved

	MOV AX,3801H
	INT 21H
	JNC failed
	CMP AX,2
	JNE failed
	MOV DX,OFFSET passed
	JMP print
failed:
	MOV DX,OFFSET failure
print:
	MOV AH,9
	INT 21H
	MOV AX,4C00H
	INT 21H
CODE ENDS

DATA SEGMENT PARA PUBLIC 'DATA'
country DB 34 DUP (0AAH)
passed DB 'country ok',13,10,'$'
failure DB 'country FAILED',13,10,'$'
DATA ENDS

STACK SEGMENT PARA STACK 'STACK'
	DB 100H DUP (?)
STACK ENDS

	END start
