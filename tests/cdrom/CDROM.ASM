; CDROM.EXE - the kit's test of the runner's MSCDEX (tests/selftest.py):
;   1. INT 2Fh AX=1500h: one CD drive, the first D:,
;   2. AX=150Bh: D: is a CD drive (BX ADADh),
;   3. a device request (AX=1510h), IOCTL input 0Ah: done, the disc's
;      tracks from 1,
;   4. IOCTL input 0Bh for the track after the last: an error,
;   5. play audio (HSG, 100 s): the audio status request says busy; stop:
;      paused (status bit 0), not busy,
;   6. INT 21h AH=57h AL=0 on its own file: a date, no error,
; and prints "cdrom ok", or "cdrom FAILED" and the number of the check
; that failed.  Written for the kit; no game's code.
.386
CODE SEGMENT PARA PUBLIC USE16 'CODE'
	ASSUME CS:CODE,DS:DATA,ES:NOTHING,SS:STACK
start:
	MOV AX,DATA
	MOV DS,AX
	MOV ES,AX
	MOV AX,1500H
	XOR BX,BX
	INT 2FH
	CMP BX,1
	MOV BL,'1'
	JNE fail_a
	CMP CX,3
	JNE fail_a
	MOV AX,150BH
	MOV CX,3
	INT 2FH
	MOV DX,BX
	MOV BL,'2'
	CMP AX,0
	JE fail_a
	CMP DX,0ADADH
	JNE fail_a
	JMP check3
fail_a:	JMP failed
check3:	; 3. audio disk info
	MOV BYTE PTR cblock,0AH
	MOV AL,3
	CALL request
	MOV BL,'3'
	CMP AX,0100H
	JNE fail_a
	CMP BYTE PTR cblock+1,1
	JNE fail_a
	MOV AL,cblock+2
	INC AL
	; 4. the track after the last is not there
	MOV BYTE PTR cblock,0BH
	MOV cblock+1,AL
	MOV AL,3
	CALL request
	MOV BL,'4'
	TEST AX,8000H
	JE fail_a
	JMP check5
fail_b:	JMP failed
check5:	; 5. play 7500 frames from frame 150, the audio status, stop
	MOV SI,OFFSET rh
	MOV BYTE PTR [SI+0DH],0
	MOV WORD PTR [SI+0EH],150
	MOV WORD PTR [SI+10H],0
	MOV WORD PTR [SI+12H],7500
	MOV WORD PTR [SI+14H],0
	MOV AL,84H
	CALL request_raw
	MOV BL,'5'
	TEST AX,8000H
	JNE fail_b
	MOV BYTE PTR cblock,0FH
	MOV AL,3
	CALL request
	MOV BL,'5'
	TEST AX,0200H
	JE fail_b
	MOV AL,85H
	CALL request_raw
	MOV BYTE PTR cblock,0FH
	MOV AL,3
	CALL request
	MOV BL,'5'
	TEST AX,0200H
	JNE fail_b
	TEST WORD PTR cblock+1,1
	JE fail_b
	JMP check6
fail_c:	JMP failed
check6:	; 6. its own file's date
	MOV BL,'6'
	MOV DX,OFFSET own_name
	MOV AX,3D00H
	INT 21H
	JC fail_c
	MOV BX,AX
	MOV AX,5700H
	INT 21H
	PUSHF
	MOV AH,3EH
	INT 21H
	POPF
	MOV BL,'6'
	JC fail_c
	CMP DX,0
	JE fail_c
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
text_ok	DB 'cdrom ok',13,10,'$'
own_name DB 'CDROM.EXE',0
text_failed DB 'cdrom FAILED at check '
text_which DB '?',13,10,'$'
DATA ENDS

STACK SEGMENT PARA STACK 'STACK'
	DB 400H DUP (?)
STACK ENDS

	END start
