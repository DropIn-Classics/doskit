; VGAMODE.EXE - the kit's test of the runner's BIOS mode set (tests/selftest.py):
;   1. INT 10h AX=000Dh: the graphics controller in graphics mode at
;      A0000h (GC 6), the CRTC 40 characters wide (CR 1 = 27h),
;   2. a byte written at A000:0000 read back from plane 0 and plane 3,
;   3. AX=000Eh: 80 characters wide (CR 1 = 4Fh), still graphics,
;   4. AX=0003h: text mode again (GC 6 bit 0 clear),
;   5. VESA: AX=4F00h succeeds, its block says "VESA", its mode list
;      (a far pointer) holds 101h,
;   6. AX=4F01h CX=101h: 640 x 480, 8 bits a pixel,
;   7. AX=4F02h BX=101h: CR 1 = 4Fh (8 pixels a character clock), CR 12h
;      = DFh (480 lines), chain-4 on; AX=4F03h gives 101h back,
;   8. AX=4F02h BX=103h: CR 1 = 63h, CR 12h = 57h (600 lines, bit 9 in
;      CR 7); BX=105h fails,
;   9. AX=0003h: text mode again,
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
	CALL vesa56
	JC failed
	CALL vesa7
	JC failed
	CALL vesa8
	JC failed
	CALL text9
	JC failed
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

; VESA checks, BL the number: CF set when one fails
vesa56:
	; 5. the VESA block
	MOV BL,'5'
	MOV AX,DATA
	MOV ES,AX
	MOV DI,OFFSET vbe
	MOV AX,4F00H
	INT 10H
	CMP AX,004FH
	JNE vesa56_f
	CMP WORD PTR vbe,'EV'
	JNE vesa56_f
	CMP WORD PTR vbe+2,'AS'
	JNE vesa56_f
	LES DI,DWORD PTR vbe+0EH
find_mode:
	MOV AX,ES:[DI]
	CMP AX,0FFFFH
	JE vesa56_f
	ADD DI,2
	CMP AX,101H
	JNE find_mode
	; 6. mode 101h's block
	MOV BL,'6'
	MOV AX,DATA
	MOV ES,AX
	MOV DI,OFFSET vbe
	MOV AX,4F01H
	MOV CX,101H
	INT 10H
	CMP AX,004FH
	JNE vesa56_f
	CMP WORD PTR vbe+12H,640
	JNE vesa56_f
	CMP WORD PTR vbe+14H,480
	JNE vesa56_f
	CMP BYTE PTR vbe+19H,8
	JNE vesa56_f
	CLC
	RET
vesa56_f:
	STC
	RET

; VESA checks, BL the number: CF set when one fails
vesa7:
	; 7. mode 101h set
	MOV BL,'7'
	MOV AX,4F02H
	MOV BX,101H
	INT 10H
	MOV BL,'7'
	CMP AX,004FH
	JNE vesa7_f
	MOV AH,1
	CALL cr_read
	CMP AL,4FH
	JNE vesa7_f
	MOV AH,12H
	CALL cr_read
	CMP AL,0DFH
	JNE vesa7_f
	MOV DX,3C4H
	MOV AL,4
	OUT DX,AL
	INC DX
	IN AL,DX
	TEST AL,8
	JE vesa7_f
	MOV AX,4F03H
	INT 10H
	CMP BX,101H
	MOV BL,'7'
	JNE vesa7_f
	CLC
	RET
vesa7_f:
	STC
	RET

; VESA checks, BL the number: CF set when one fails
vesa8:
	; 8. mode 103h, and one there is not
	MOV BL,'8'
	MOV AX,4F02H
	MOV BX,103H
	INT 10H
	MOV BL,'8'
	CMP AX,004FH
	JNE vesa8_f
	MOV AH,1
	CALL cr_read
	CMP AL,63H
	JNE vesa8_f
	MOV AH,12H
	CALL cr_read
	CMP AL,57H
	JNE vesa8_f
	MOV AX,4F02H
	MOV BX,105H
	INT 10H
	MOV BL,'8'
	CMP AX,004FH
	JE vesa8_f
	CLC
	RET
vesa8_f:
	STC
	RET

; 9. back to text: CF set when it fails
text9:
	MOV BL,'9'
	MOV AX,0003H
	INT 10H
	MOV AH,6
	CALL gc_read
	TEST AL,1
	JNE vesa8_f
	CLC
	RET

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
vbe	DB 256 DUP (0)
DATA ENDS

STACK SEGMENT PARA STACK 'STACK'
	DB 400H DUP (?)
STACK ENDS

	END start
