; LOL.EXE - runner test for INT 21h AH=52h, the List of Lists
.186
JUMPS
CODE SEGMENT PARA PUBLIC 'CODE'
	ASSUME CS:CODE,DS:DATA,SS:STACK
start:
	MOV AX,DATA
	MOV DS,AX
	MOV AH,52H
	INT 21H
	; the first MCB: a chain block ('M' or 'Z') that walks to a 'Z'
	MOV AX,ES:[BX-2]
	CMP AX,60H
	JNE failed
	PUSH ES
mcb_walk:
	MOV ES,AX
	CMP BYTE PTR ES:[0],'Z'
	JE mcb_end
	CMP BYTE PTR ES:[0],'M'
	JNE mcb_failed
	ADD AX,ES:[3]
	INC AX
	JMP mcb_walk
mcb_failed:
	POP ES
	JMP failed
mcb_end:
	POP ES
	; no DPB chain and no SFT in guest memory: end-of-list pointers
	CMP WORD PTR ES:[BX+0],0FFFFH
	JNE failed
	CMP WORD PTR ES:[BX+4],0FFFFH
	JNE failed
	CMP WORD PTR ES:[BX+6],0FFFFH
	JNE failed
	CMP WORD PTR ES:[BX+10H],512
	JNE failed
	CMP BYTE PTR ES:[BX+21H],26
	JNE failed
	; the NUL device header, the end of the driver chain
	CMP WORD PTR ES:[BX+22H],0FFFFH
	JNE failed
	CMP WORD PTR ES:[BX+26H],8004H
	JNE failed
	MOV SI,OFFSET nulname
	LEA DI,[BX+2CH]
	MOV CX,8
	CLD
	REPE CMPSB
	JNE failed
	; its strategy and interrupt entries can be called
	MOV AX,ES:[BX+28H]
	MOV entry,AX
	MOV entry+2,ES
	CALL DWORD PTR entry
	MOV AX,ES:[BX+2AH]
	MOV entry,AX
	CALL DWORD PTR entry
	CMP BYTE PTR ES:[BX+43H],3
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
nulname DB 'NUL     '
entry DW 0,0
passed DB 'lol ok',13,10,'$'
failure DB 'lol FAILED',13,10,'$'
DATA ENDS

STACK SEGMENT PARA STACK 'STACK'
	DB 100H DUP (?)
STACK ENDS

	END start
