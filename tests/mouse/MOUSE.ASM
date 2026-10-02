; MOUSE.EXE - the kit's INT 33h mouse-driver test.  It checks the standard
; state calls itself, then waits for the deterministic moves and clicks that
; tests/selftest.py supplies and checks their callback and button records.
.386
CODE SEGMENT PARA PUBLIC 'CODE'
	ASSUME CS:CODE,DS:DATA,ES:NOTHING,SS:STACK
start:
	MOV AX,DATA
	MOV DS,AX
	MOV AX,13H
	INT 10H

	XOR AX,AX
	INT 33H
	CMP AX,0FFFFH
	LAHF
	AND AH,40H
	XOR AH,40H
	OR [failed_flag],AH
	CMP BX,3
	LAHF
	AND AH,40H
	XOR AH,40H
	OR [failed_flag],AH

	MOV AX,24H
	XOR BX,BX
	INT 33H
	CMP BX,0705H
	LAHF
	AND AH,40H
	XOR AH,40H
	OR [failed_flag],AH
	CMP CH,4
	LAHF
	AND AH,40H
	XOR AH,40H
	OR [failed_flag],AH

	MOV AX,7
	MOV CX,10
	MOV DX,300
	INT 33H
	MOV AX,8
	MOV CX,20
	MOV DX,180
	INT 33H
	MOV AX,31H
	INT 33H
	CMP AX,10
	LAHF
	AND AH,40H
	XOR AH,40H
	OR [failed_flag],AH
	CMP BX,20
	LAHF
	AND AH,40H
	XOR AH,40H
	OR [failed_flag],AH
	CMP CX,300
	LAHF
	AND AH,40H
	XOR AH,40H
	OR [failed_flag],AH
	CMP DX,180
	LAHF
	AND AH,40H
	XOR AH,40H
	OR [failed_flag],AH

	MOV AX,4
	MOV CX,123
	MOV DX,77
	INT 33H
	MOV AX,3
	INT 33H
	CMP CX,123
	LAHF
	AND AH,40H
	XOR AH,40H
	OR [failed_flag],AH
	CMP DX,77
	LAHF
	AND AH,40H
	XOR AH,40H
	OR [failed_flag],AH

	MOV AX,1AH
	MOV BX,31
	MOV CX,47
	MOV DX,59
	INT 33H
	MOV AX,1BH
	INT 33H
	CMP BX,31
	LAHF
	AND AH,40H
	XOR AH,40H
	OR [failed_flag],AH
	CMP CX,47
	LAHF
	AND AH,40H
	XOR AH,40H
	OR [failed_flag],AH
	CMP DX,59
	LAHF
	AND AH,40H
	XOR AH,40H
	OR [failed_flag],AH

	MOV AX,15H
	INT 33H
	CMP BX,2048
	JBE state_size_ok
	MOV BYTE PTR [failed_flag],1
state_size_ok:
	MOV AX,DATA
	MOV ES,AX
	MOV DX,OFFSET saved_state
	MOV AX,16H
	INT 33H
	MOV AX,4
	XOR CX,CX
	XOR DX,DX
	INT 33H
	MOV AX,17H
	MOV DX,OFFSET saved_state
	INT 33H
	CMP AX,17H
	LAHF
	AND AH,40H
	XOR AH,40H
	OR [failed_flag],AH
	MOV AX,3
	INT 33H
	CMP CX,123
	LAHF
	AND AH,40H
	XOR AH,40H
	OR [failed_flag],AH
	CMP DX,77
	LAHF
	AND AH,40H
	XOR AH,40H
	OR [failed_flag],AH

	MOV AX,CODE
	MOV ES,AX
	MOV DX,OFFSET mouse_callback
	MOV CX,7FH
	MOV AX,0CH
	INT 33H
	MOV AX,1
	INT 33H

wait_press:
	MOV AX,3
	INT 33H
	TEST BX,1
	JZ wait_press
wait_release:
	MOV AX,3
	INT 33H
	TEST BX,1
	JNZ wait_release

	CMP WORD PTR [callback_count],3
	LAHF
	AND AH,1
	OR [failed_flag],AH
	MOV AX,[callback_mask]
	AND AX,7
	CMP AX,7
	LAHF
	AND AH,40H
	XOR AH,40H
	OR [failed_flag],AH
	CMP WORD PTR [callback_x],120
	LAHF
	AND AH,40H
	XOR AH,40H
	OR [failed_flag],AH
	CMP WORD PTR [callback_y],70
	LAHF
	AND AH,40H
	XOR AH,40H
	OR [failed_flag],AH

	MOV AX,5
	XOR BX,BX
	INT 33H
	CMP BX,1
	LAHF
	AND AH,40H
	XOR AH,40H
	OR [failed_flag],AH
	CMP CX,120
	LAHF
	AND AH,40H
	XOR AH,40H
	OR [failed_flag],AH
	CMP DX,70
	LAHF
	AND AH,40H
	XOR AH,40H
	OR [failed_flag],AH
	MOV AX,6
	XOR BX,BX
	INT 33H
	CMP BX,1
	LAHF
	AND AH,40H
	XOR AH,40H
	OR [failed_flag],AH
	CMP CX,120
	LAHF
	AND AH,40H
	XOR AH,40H
	OR [failed_flag],AH
	CMP DX,70
	LAHF
	AND AH,40H
	XOR AH,40H
	OR [failed_flag],AH

	MOV AX,0BH
	INT 33H
	OR CX,DX
	LAHF
	AND AH,40H
	OR [failed_flag],AH
	CMP BYTE PTR [failed_flag],0
	JNE failed

	MOV DX,OFFSET ok_text
	MOV AH,9
	INT 21H
	MOV AX,4C00H
	INT 21H

failed:
	MOV DX,OFFSET fail_text
	MOV AH,9
	INT 21H
	MOV AX,4C01H
	INT 21H

mouse_callback:
	INC WORD PTR [callback_count]
	OR [callback_mask],AX
	MOV [callback_buttons],BX
	MOV [callback_x],CX
	MOV [callback_y],DX
	RETF
CODE ENDS

DATA SEGMENT PARA PUBLIC 'DATA'
failed_flag DB 0
callback_count DW 0
callback_mask DW 0
callback_buttons DW 0
callback_x DW 0
callback_y DW 0
ok_text DB 'mouse ok',13,10,'$'
fail_text DB 'mouse failed',13,10,'$'
saved_state DB 2048 DUP (0)
DATA ENDS

STACK SEGMENT PARA STACK 'STACK'
	DB 100H DUP (?)
STACK ENDS

	END start
