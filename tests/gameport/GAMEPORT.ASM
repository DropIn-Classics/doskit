; GAMEPORT.EXE - the kit's test of the runner's game port (tests/selftest.py):
;   1. port 201h reads FFh with nothing attached: the buttons up (bits 4-7
;      set) and the axis bits set,
;   2. after a write to 201h (which starts the one-shots) the axis bits
;      stay set for 10000 reads: no stick answers, as on a PC without one
;      (a program that times the bits takes that for "no joystick"; axis
;      bits that read 0 at once are a stick in its upper left corner),
; and prints "gameport ok", or "gameport FAILED" and the number of the check
; that failed.  Written for the kit; no game's code.
CODE SEGMENT PARA PUBLIC 'CODE'
	ASSUME CS:CODE,DS:DATA,ES:NOTHING,SS:STACK
start:
	MOV AX,DATA
	MOV DS,AX
	MOV BL,'1'
	MOV DX,201H
	IN AL,DX
	CMP AL,0FFH
	JNE failed
	MOV BL,'2'
	OUT DX,AL
	MOV CX,10000
poll:
	IN AL,DX
	AND AL,0FH
	CMP AL,0FH
	JNE failed
	LOOP poll
	MOV DX,OFFSET text_ok
	MOV AH,9
	INT 21H
	MOV AX,4C00H
	INT 21H
failed:
	MOV text_which,BL
	MOV DX,OFFSET text_failed
	MOV AH,9
	INT 21H
	MOV AX,4C01H
	INT 21H
CODE ENDS

DATA SEGMENT PARA PUBLIC 'DATA'
text_ok	DB 'gameport ok',13,10,'$'
text_failed DB 'gameport FAILED at check '
text_which DB '?',13,10,'$'
DATA ENDS

STACK SEGMENT PARA STACK 'STACK'
	DB 400H DUP (?)
STACK ENDS

	END start
