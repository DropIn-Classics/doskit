; VGAMODE.EXE - the kit's test of the runner's BIOS mode set (tests/selftest.py):
;   1. INT 10h AX=000Dh: the graphics controller in graphics mode at
;      A0000h (GC 6), the CRTC 40 characters wide (CR 1 = 27h),
;   2. a byte written at A000:0000 read back from plane 0 and plane 3,
;   3. AX=000Eh: 80 characters wide (CR 1 = 4Fh), still graphics,
;   4. AX=0003h: text mode again (GC 6 bit 0 clear),
; and prints "vgamode ok", or "vgamode FAILED" and the number of the check
; that failed.  Written for the kit; no game's code.
.386
CODE SEGMENT PARA PUBLIC USE16 'CODE'
	ASSUME CS:CODE,DS:DATA,ES:NOTHING,SS:STACK
start:
	MOV AX,DATA
	MOV DS,AX
	MOV AX,000DH
	INT 10H
	MOV BL,'1'
	MOV AH,6
	CALL gc_read
	AND AL,0DH
	CMP AL,05H
	JNE failed
	MOV AH,1
	CALL cr_read
	CMP AL,27H
	JNE failed
	; 2. write mode 0, all planes: the byte lands in every plane
	MOV BL,'2'
	MOV AX,0A000H
	MOV ES,AX
	MOV BYTE PTR ES:[0],5AH
	MOV DX,3CEH
	MOV AX,0004H
	OUT DX,AX
	MOV AL,ES:[0]
	CMP AL,5AH
	JNE failed
	MOV AX,0304H
	OUT DX,AX
	MOV AL,ES:[0]
	CMP AL,5AH
	JNE failed
	; 3. mode 0Eh
	MOV BL,'3'
	MOV AX,000EH
	INT 10H
	MOV AH,1
	CALL cr_read
	CMP AL,4FH
	JNE failed
	MOV AH,6
	CALL gc_read
	AND AL,0DH
	CMP AL,05H
	JNE failed
	; 4. back to text
	MOV BL,'4'
	MOV AX,0003H
	INT 10H
	MOV AH,6
	CALL gc_read
	TEST AL,1
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

; graphics controller register AH -> AL
gc_read:
	MOV DX,3CEH
	MOV AL,AH
	OUT DX,AL
	INC DX
	IN AL,DX
	RET

; CRTC register AH -> AL
cr_read:
	MOV DX,3D4H
	MOV AL,AH
	OUT DX,AL
	INC DX
	IN AL,DX
	RET
CODE ENDS

DATA SEGMENT PARA PUBLIC 'DATA'
text_ok	DB 'vgamode ok',13,10,'$'
text_failed DB 'vgamode FAILED at check '
text_which DB '?',13,10,'$'
DATA ENDS

STACK SEGMENT PARA STACK 'STACK'
	DB 400H DUP (?)
STACK ENDS

	END start
