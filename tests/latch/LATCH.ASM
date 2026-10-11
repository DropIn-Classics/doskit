; Independently written test of the VGA's latches in a planar mode
; (0Dh): a byte written with bits masked out by the bit mask register
; takes those bits from the latches, which hold the four planes' bytes
; of the last read, not from the place written to.  With the bit mask 0
; a read and a write copy all four planes (a picture moved in video
; memory).
.186
CODE SEGMENT PARA PUBLIC 'CODE'
	ASSUME CS:CODE,DS:DATA,SS:STACK
start:
	MOV AX,DATA
	MOV DS,AX
	MOV AX,0DH
	INT 10H
	MOV AX,0A000H
	MOV ES,AX
; the source at 0: 3Ch in plane 0, 0A5h in plane 1, 0 in planes 2 and 3
	MOV AH,0FFH
	CALL bit_mask
	MOV AH,1
	CALL map_mask
	MOV BYTE PTR ES:[0],3CH
	MOV AH,2
	CALL map_mask
	MOV BYTE PTR ES:[0],0A5H
; 0Fh in all planes at 200h, where a copy is to replace it
	MOV AH,0FH
	CALL map_mask
	MOV BYTE PTR ES:[200H],0FH
; bit mask 0: read the source, write anything to an empty place
	MOV AH,0
	CALL bit_mask
	MOV AL,ES:[0]
	MOV BYTE PTR ES:[100H],0FFH
	MOV BX,100H
	MOV CX,0A53CH
	MOV DX,0
	CALL check
; the same over the place that held 0Fh
	MOV AL,ES:[0]
	MOV BYTE PTR ES:[200H],0
	MOV BX,200H
	MOV CX,0A53CH
	MOV DX,0
	CALL check
; a REP MOVSB of one byte does the same
	PUSH DS
	MOV AX,0A000H
	MOV DS,AX
	XOR SI,SI
	MOV DI,280H
	MOV CX,1
	CLD
	REP MOVSB
	POP DS
	MOV BX,280H
	MOV CX,0A53CH
	MOV DX,0
	CALL check
; bit mask 0F0h: the high four bits written, the low four the latches'
	MOV AH,0F0H
	CALL bit_mask
	MOV AL,ES:[0]
	MOV BYTE PTR ES:[300H],0FFH
	MOV BX,300H
	MOV CX,0F5FCH
	MOV DX,0F0F0H
	CALL check
	MOV AH,0FFH
	CALL bit_mask
	MOV AX,3
	INT 10H
	MOV DX,OFFSET passed
	JMP print
failed:
	MOV AX,3
	INT 10H
	MOV DX,OFFSET failure
print:
	MOV AH,9
	INT 21H
	MOV AX,4C00H
	INT 21H

; the bit mask register (graphics controller 8) = AH
bit_mask:
	MOV DX,3CEH
	MOV AL,8
	OUT DX,AX
	RET
; the map mask register (sequencer 2) = AH
map_mask:
	MOV DX,3C4H
	MOV AL,2
	OUT DX,AX
	RET
; the byte at ES:BX is CL in plane 0, CH in plane 1, DL in 2, DH in 3
check:
	PUSH DX
	MOV AH,0
	CALL read_map
	CMP ES:[BX],CL
	JNE check_failed
	MOV AH,1
	CALL read_map
	CMP ES:[BX],CH
	JNE check_failed
	POP CX
	MOV AH,2
	CALL read_map
	CMP ES:[BX],CL
	JNE failed
	MOV AH,3
	CALL read_map
	CMP ES:[BX],CH
	JNE failed
	RET
check_failed:
	POP DX
	JMP failed
; the read map select register (graphics controller 4) = AH
read_map:
	MOV DX,3CEH
	MOV AL,4
	OUT DX,AX
	RET
CODE ENDS
DATA SEGMENT PARA PUBLIC 'DATA'
passed DB 'latch ok',13,10,'$'
failure DB 'latch FAILED',13,10,'$'
DATA ENDS
STACK SEGMENT PARA STACK 'STACK'
	DB 100H DUP (?)
STACK ENDS
	END start
