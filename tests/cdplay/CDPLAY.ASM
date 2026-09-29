; CDPLAY.EXE - the kit's test of what the runner's CD drive plays
; (tests/selftest.py, with the cue sheet it makes and -cdwav):
;   1. IOCTL output 03h: output 0 plays input 1 at FFh, output 1 input 0
;      at 80h, outputs 2 and 3 silent,
;   2. IOCTL input 04h reads the same back,
;   3. about half a second later (nine BIOS ticks) play audio (HSG)
;      frames 525 to 825: the last second of track 2 (a WAVE), the two
;      seconds of track 3's pregap, all of track 3 (an Ogg); then the
;      device request's busy bit until it goes, and nine ticks more,
; and prints "cdplay ok", or "cdplay FAILED" and the number of the check
; that failed.  selftest.py checks the samples in the WAV.  Written for
; the kit; no game's code.
.386
CODE SEGMENT PARA PUBLIC USE16 'CODE'
	ASSUME CS:CODE,DS:DATA,ES:NOTHING,SS:STACK
start:
	MOV AX,DATA
	MOV DS,AX
	MOV ES,AX
	; 1. the channels
	MOV SI,OFFSET chans
	MOV DI,OFFSET cblock
	MOV CX,9
	CLD
	REP MOVSB
	MOV AL,0CH
	CALL request
	MOV BL,'1'
	CMP AX,0100H
	JNE fail_a
	; 2. read back
	MOV BYTE PTR cblock,4
	MOV AL,3
	CALL request
	MOV BL,'2'
	CMP AX,0100H
	JNE fail_a
	MOV SI,OFFSET chans+1
	MOV DI,OFFSET cblock+1
	MOV CX,8
	REPE CMPSB
	JNE fail_a
	JMP check3
fail_a:	JMP failed
check3:	; 3. play 300 frames from frame 525, wait for its end
	CALL wait_ticks
	MOV SI,OFFSET rh
	MOV BYTE PTR [SI+0DH],0
	MOV WORD PTR [SI+0EH],525
	MOV WORD PTR [SI+10H],0
	MOV WORD PTR [SI+12H],300
	MOV WORD PTR [SI+14H],0
	MOV AL,84H
	CALL request_raw
	MOV BL,'3'
	TEST AX,8000H
	JNE failed
	TEST AX,0200H
	JE failed
wait_end:
	MOV BYTE PTR cblock,0FH
	MOV AL,3
	CALL request
	TEST AX,0200H
	JNE wait_end
	CALL wait_ticks
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

; nine ticks of the BIOS clock (INT 1Ah), about half a second
wait_ticks:
	MOV AH,0
	INT 1AH
	MOV BP,DX
wait_tick:
	MOV AH,0
	INT 1AH
	SUB DX,BP
	CMP DX,9
	JB wait_tick
	RET

; a device request with the control block (IOCTL), command AL -> AX its status
request:
	MOV SI,OFFSET rh
	MOV WORD PTR [SI+0EH],OFFSET cblock
	MOV [SI+10H],DS
	MOV WORD PTR [SI+12H],16
; the request header as it is, command AL -> AX its status
request_raw:
	MOV SI,OFFSET rh
	MOV BYTE PTR [SI],20
	MOV BYTE PTR [SI+1],0
	MOV [SI+2],AL
	MOV WORD PTR [SI+3],0
	MOV BX,SI
	MOV CX,3
	MOV AX,1510H
	INT 2FH
	MOV AX,[SI+3]
	RET
CODE ENDS

DATA SEGMENT PARA PUBLIC 'DATA'
rh	DB 20H DUP (0)
cblock	DB 16 DUP (0)
chans	DB 3, 1,0FFH, 0,80H, 2,0, 3,0
text_ok	DB 'cdplay ok',13,10,'$'
text_failed DB 'cdplay FAILED at check '
text_which DB '?',13,10,'$'
DATA ENDS

STACK SEGMENT PARA STACK 'STACK'
	DB 400H DUP (?)
STACK ENDS

	END start
