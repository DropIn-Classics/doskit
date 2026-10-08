; JOY.EXE - the kit's test of a stick at the runner's game port
; (tests/selftest.py runs it with -joy 0 0,300,600,1: stick 0 with
; one-shots of 300 and 600 microseconds and its first button held, and
; with a -joys file that brings a second stick and takes it away again):
;   1. before any write to 201h the stick's axis bits are 0 (its one-shots
;      are not running), the missing stick's are set, button 1 of stick 0
;      reads 0 (held) and the other three 1: ECh,
;   2. right after a write the stick's two axis bits are set: EFh,
;   3. the X bit falls after 300 microseconds: at the runner's 6 million
;      instructions a second and four instructions a poll some 450 polls
;      (300 to 600 are taken),
;   4. the Y bit falls after 600: some 900 polls from the write (700 to
;      1100),
;   5. the port then reads ECh again,
; and prints "joy ok", or "joy FAILED" and the number of the check that
; failed.  Written for the kit; no game's code.
CODE SEGMENT PARA PUBLIC 'CODE'
	ASSUME CS:CODE,DS:DATA,ES:NOTHING,SS:STACK
start:
	MOV AX,DATA
	MOV DS,AX
	MOV BL,'1'
	MOV DX,201H
	IN AL,DX
	CMP AL,0ECH
	JNE failed
	MOV BL,'2'
	OUT DX,AL
	IN AL,DX
	CMP AL,0EFH
	JNE failed
	MOV BL,'3'
	XOR CX,CX
poll_x:
	INC CX
	IN AL,DX
	TEST AL,1
	JNE poll_x
	CMP CX,300
	JB failed
	CMP CX,600
	JA failed
	MOV BL,'4'
poll_y:
	INC CX
	IN AL,DX
	TEST AL,2
	JNE poll_y
	CMP CX,700
	JB failed
	CMP CX,1100
	JA failed
	MOV BL,'5'
	IN AL,DX
	CMP AL,0ECH
	JNE failed
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
text_ok	DB 'joy ok',13,10,'$'
text_failed DB 'joy FAILED at check '
text_which DB '?',13,10,'$'
DATA ENDS

STACK SEGMENT PARA STACK 'STACK'
	DB 400H DUP (?)
STACK ENDS

	END start
