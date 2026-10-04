; REQ.EXE - runner test for -requirements: one used device/memory interface
; and one unsupported port, BIOS service and DOS service.
.186
CODE SEGMENT PARA PUBLIC 'CODE'
	ASSUME CS:CODE,DS:CODE,ES:NOTHING,SS:STACK
start:
	MOV DX,3DAH
	IN AL,DX
	MOV AX,0A000H
	MOV ES,AX
	XOR DI,DI
	MOV AL,1
	STOSB
	MOV DX,123H
	IN AL,DX
	OUT DX,AL
	MOV AX,0FF00H
	INT 10H
	MOV AX,6600H
	INT 21H
	DB 0D9H,0D0H                 ; FNOP: unsupported x87 diagnostic
	DB 0DBH,0E3H                 ; FNINIT: a second temporary diagnostic
	MOV AX,4C00H
	INT 21H
CODE ENDS

STACK SEGMENT PARA STACK 'STACK'
	DB 100H DUP (?)
STACK ENDS

	END start
