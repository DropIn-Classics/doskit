; SB16.EXE - the kit's test of the runner's Sound Blaster 16 (tests/selftest.py):
;   1. a DSP reset at 220h answers AAh,
;   2. the mixer's configuration: register 80h IRQ 7, 81h DMA 1 and 5,
;   3. 41h (8000 Hz) and B0h (16-bit, single-cycle, signed mono) over
;      64 words on DMA 5: IRQ 7 comes once, mixer 82h says 16-bit
;      interrupt, and DMA 5's address has moved on by the 64 words,
;   4. C0h (8-bit, single-cycle, unsigned mono) over 16 bytes on DMA 1:
;      IRQ 7 again, mixer 82h says 8-bit interrupt, the address 16 on,
; and prints "sb16 ok", or "sb16 FAILED" and the number of the check
; that failed.  The interrupt handler acknowledges both kinds (22Eh,
; 22Fh).  selftest.py runs it with -wav and checks the 80 samples that
; were played.  Written for the kit; no game's code.
.386
CODE SEGMENT PARA PUBLIC USE16 'CODE'
	ASSUME CS:CODE,DS:DATA,ES:NOTHING,SS:STACK
start:
	MOV AX,DATA
	MOV DS,AX
	; 1. reset
	MOV BL,'1'
	MOV DX,226H
	MOV AL,1
	OUT DX,AL
	MOV AL,0
	OUT DX,AL
	MOV DX,22EH
	MOV CX,0
wait_reset:
	IN AL,DX
	TEST AL,80H
	JNZ reset_data
	LOOP wait_reset
	JMP failed
reset_data:
	MOV DX,22AH
	IN AL,DX
	CMP AL,0AAH
	JNE fail_a
	; 2. the mixer's configuration
	MOV BL,'2'
	MOV AH,80H
	CALL mixer_read
	CMP AL,04H
	JNE fail_a
	MOV AH,81H
	CALL mixer_read
	AND AL,22H
	CMP AL,22H
	JNE fail_a
	JMP check3
fail_a:	JMP failed
check3:	; IRQ 7 (INT 0Fh) to irq7, unmasked at the PIC
	CLI
	XOR AX,AX
	MOV ES,AX
	MOV WORD PTR ES:[0FH*4],OFFSET irq7
	MOV ES:[0FH*4+2],CS
	IN AL,21H
	AND AL,7FH
	OUT 21H,AL
	STI
	; 3. DMA 5: single, no auto-init, read (memory to card)
	MOV BL,'3'
	CALL buf_phys16
	MOV SI,AX
	MOV AL,05H
	OUT 0D4H,AL
	OUT 0D8H,AL
	MOV AL,49H
	OUT 0D6H,AL
	MOV AL,DL
	OUT 8BH,AL
	MOV AX,SI
	OUT 0C4H,AL
	MOV AL,AH
	OUT 0C4H,AL
	MOV AL,63
	OUT 0C6H,AL
	MOV AL,0
	OUT 0C6H,AL
	MOV AL,01H
	OUT 0D4H,AL
	MOV AL,41H
	CALL dsp_write
	MOV AL,1FH
	CALL dsp_write
	MOV AL,40H
	CALL dsp_write
	MOV AL,0B0H
	CALL dsp_write
	MOV AL,10H
	CALL dsp_write
	MOV AL,63
	CALL dsp_write
	MOV AL,0
	CALL dsp_write
	CALL wait_irq
	JC fail_a
	TEST irq_status,02H
	JE fail_a
	; the address: 64 words on (read low, high after a flip-flop clear)
	OUT 0D8H,AL
	IN AL,0C4H
	MOV CL,AL
	IN AL,0C4H
	MOV CH,AL
	SUB CX,SI
	CMP CX,64
	JNE fail_b
	JMP check4
fail_b:	JMP failed
check4:	; 4. DMA 1, 8-bit
	MOV BL,'4'
	CALL buf_phys8
	MOV SI,AX
	MOV AL,05H
	OUT 0AH,AL
	OUT 0CH,AL
	MOV AL,49H
	OUT 0BH,AL
	MOV AL,DL
	OUT 83H,AL
	MOV AX,SI
	OUT 02H,AL
	MOV AL,AH
	OUT 02H,AL
	MOV AL,15
	OUT 03H,AL
	MOV AL,0
	OUT 03H,AL
	MOV AL,01H
	OUT 0AH,AL
	MOV AL,0C0H
	CALL dsp_write
	MOV AL,00H
	CALL dsp_write
	MOV AL,15
	CALL dsp_write
	MOV AL,0
	CALL dsp_write
	CALL wait_irq
	JC fail_b
	TEST irq_status,01H
	JE fail_b
	OUT 0CH,AL
	IN AL,02H
	MOV CL,AL
	IN AL,02H
	MOV CH,AL
	SUB CX,SI
	CMP CX,16
	JNE fail_b
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

; mixer register AH -> AL
mixer_read:
	MOV DX,224H
	MOV AL,AH
	OUT DX,AL
	INC DX
	IN AL,DX
	RET

; AL to the DSP, when it takes a byte
dsp_write:
	PUSH AX
	MOV DX,22CH
dsp_busy:
	IN AL,DX
	TEST AL,80H
	JNZ dsp_busy
	POP AX
	OUT DX,AL
	RET

; wait for the next interrupt (irqs counts them); carry when none came
wait_irq:
	MOV AL,irqs
	MOV DI,40
wait_outer:
	MOV CX,0
wait_inner:
	CMP AL,irqs
	JNE wait_done
	LOOP wait_inner
	DEC DI
	JNZ wait_outer
	STC
	RET
wait_done:
	CLC
	RET

; the 16-bit buffer as DMA 5 takes it: AX the word address, DL the page
buf_phys16:
	CALL seg_phys
	ADD AX,OFFSET buf16
	ADC DX,0
	SHR DX,1
	RCR AX,1
	SHL DX,1
	RET

; the 8-bit buffer as DMA 1 takes it: AX the address, DL the page
buf_phys8:
	CALL seg_phys
	ADD AX,OFFSET buf8
	ADC DX,0
	RET

; DS as a linear address in DX:AX
seg_phys:
	MOV AX,DS
	MOV DX,AX
	SHL AX,4
	SHR DX,12
	RET

; IRQ 7: the mixer's interrupt status kept, both kinds acknowledged
irq7:
	PUSH AX
	PUSH DX
	PUSH DS
	MOV AX,DATA
	MOV DS,AX
	MOV DX,224H
	MOV AL,82H
	OUT DX,AL
	INC DX
	IN AL,DX
	MOV irq_status,AL
	MOV DX,22EH
	IN AL,DX
	MOV DX,22FH
	IN AL,DX
	INC irqs
	MOV AL,20H
	OUT 20H,AL
	POP DS
	POP DX
	POP AX
	IRET
CODE ENDS

DATA SEGMENT PARA PUBLIC 'DATA'
buf16	DW 64 DUP (1000H)
buf8	DB 16 DUP (0C0H)
irqs	DB 0
irq_status DB 0
text_ok	DB 'sb16 ok',13,10,'$'
text_failed DB 'sb16 FAILED at check '
text_which DB '?',13,10,'$'
DATA ENDS

STACK SEGMENT PARA STACK 'STACK'
	DB 400H DUP (?)
STACK ENDS

	END start
