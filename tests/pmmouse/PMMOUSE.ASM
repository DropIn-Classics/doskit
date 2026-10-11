; PMMOUSE.EXE - the kit's test of the runner's mouse driver under a
; protected-mode program (tests/selftest.py).  It installs a real-mode
; event handler (INT 33h AX=000Ch), enters protected mode through the
; BIOS (INT 15h AH=89h, the PICs at 20h and 28h) and waits there for the
; moves and the click the selftest supplies.  As a DOS extender does, its
; handler for IRQ 12 (vector 2Ch) goes to real mode, calls the real-mode
; INT 74h and comes back.  Back in real mode for good it checks
;   1. that installing the handler enabled IRQ 12 and the cascade,
;   2. the handler's three calls and what they were given,
;   3. that they came through the interrupt,
;   4. that the handler ran in real mode,
; and prints "pmmouse ok", or "pmmouse FAILED" and the number of the
; check that failed.
; tasm.py has no system instructions: they are written as DB.  Written
; for the kit; no game's code.
.386
CODE SEGMENT PARA PUBLIC USE16 'CODE'
	ASSUME CS:CODE,DS:DATA,ES:NOTHING,SS:STACK
start:
	MOV AX,DATA
	MOV DS,AX
	; the descriptors' bases
	MOV AX,DATA
	CALL seg_lin
	MOV SI,OFFSET d_data
	CALL set_base
	MOV SI,OFFSET d_extra
	CALL set_base
	MOV BX,AX
	MOV CX,DX
	ADD AX,OFFSET gdt
	ADC DX,0
	MOV SI,OFFSET d_gdt
	CALL set_base
	MOV AX,BX
	MOV DX,CX
	ADD AX,OFFSET idt
	ADC DX,0
	MOV SI,OFFSET d_idt
	CALL set_base
	MOV WORD PTR pm_idt+2,AX
	MOV WORD PTR pm_idt+4,DX
	MOV AX,STACK
	CALL seg_lin
	MOV SI,OFFSET d_stack
	CALL set_base
	MOV AX,CODE
	CALL seg_lin
	MOV SI,OFFSET d_code
	CALL set_base
	; the driver and the handler
	XOR AX,AX
	INT 33H
	MOV r_reset,AX
	MOV AX,CODE
	MOV ES,AX
	MOV DX,OFFSET mouse_handler
	MOV CX,7FH
	MOV AX,0CH
	INT 33H
	IN AL,21H
	MOV pic1_mask,AL
	IN AL,0A1H
	MOV pic2_mask,AL
	; into protected mode: ES:SI the GDT, IRQs at 20h and 28h
	CLI
	PUSH DS
	POP ES
	MOV SI,OFFSET gdt
	MOV BX,2028H
	MOV AH,89H
	INT 15H
	; CS 30h, DS 18h, ES 20h, SS 28h; IRQ 12 and the cascade alone
	MOV AL,0FBH
	OUT 21H,AL
	MOV AL,0EFH
	OUT 0A1H,AL
	STI
wait_events:
	CMP calls,3
	JB wait_events
	CLI
	DB 0FH,20H,0C0H                 ; MOV EAX,CR0
	AND AL,0FEH
	DB 0FH,22H,0C0H                 ; MOV CR0,EAX
	DB 0EAH                         ; JMP FAR CODE:real_again
	DW OFFSET real_again
	DW CODE
real_again:
	MOV AX,DATA
	MOV DS,AX
	MOV ES,AX
	MOV AX,STACK
	MOV SS,AX
	MOV SP,400H
	DB 0FH,01H,1EH                  ; LIDT [real_idt]
	DW OFFSET real_idt
	MOV AL,11H                      ; the PICs back at 08h and 70h
	OUT 20H,AL
	MOV AL,08H
	OUT 21H,AL
	MOV AL,04H
	OUT 21H,AL
	MOV AL,01H
	OUT 21H,AL
	MOV AL,pic1_mask
	OUT 21H,AL
	MOV AL,11H
	OUT 0A0H,AL
	MOV AL,70H
	OUT 0A1H,AL
	MOV AL,02H
	OUT 0A1H,AL
	MOV AL,01H
	OUT 0A1H,AL
	MOV AL,pic2_mask
	OUT 0A1H,AL
	STI
	JMP checks
failed:
	MOV text_which,BL
	MOV DX,OFFSET text_failed
	MOV AH,9
	INT 21H
	MOV AX,4C01H
	INT 21H
checks:
	MOV BL,'1'
	CMP r_reset,0FFFFH
	JNE failed
	TEST pic1_mask,04H
	JNZ failed
	TEST pic2_mask,10H
	JNZ failed
	MOV BL,'2'
	CMP calls,3
	JNE failed
	MOV AX,r_mask
	AND AX,7FH
	CMP AX,7
	JNE failed
	CMP r_x,120
	JNE failed
	CMP r_y,70
	JNE failed
	CMP r_buttons,0
	JNE failed
	MOV BL,'3'
	CMP reflects,0
	JE failed
	CMP reflects,3
	JA failed
	MOV BL,'4'
	CMP r_pe,0
	JNE failed
	MOV DX,OFFSET text_ok
	MOV AH,9
	INT 21H
	MOV AX,4C00H
	INT 21H

; AX a segment -> DX:AX its linear address
seg_lin:
	MOV DX,AX
	SHL AX,4
	SHR DX,12
	RET
; the base DX:AX into the descriptor at DS:SI
set_base:
	MOV [SI+2],AX
	MOV [SI+4],DL
	MOV [SI+7],DH
	RET

; IRQ 12 in protected mode: to real mode, the real-mode INT 74h, and back
irq12_reflect:
	PUSHA
	PUSH DS
	PUSH ES
	MOV AX,18H
	MOV DS,AX
	MOV ES,AX
	INC reflects
	DB 0FH,20H,0C0H                 ; MOV EAX,CR0
	AND AL,0FEH
	DB 0FH,22H,0C0H                 ; MOV CR0,EAX
	DB 0EAH                         ; JMP FAR CODE:reflect_real
	DW OFFSET reflect_real
	DW CODE
reflect_real:
	MOV AX,DATA
	MOV DS,AX
	MOV ES,AX
	MOV AX,STACK                    ; the same memory as selector 28h: SP stays
	MOV SS,AX
	DB 0FH,01H,1EH                  ; LIDT [real_idt]
	DW OFFSET real_idt
	INT 74H
	CLI
	DB 0FH,20H,0C0H                 ; MOV EAX,CR0
	OR AL,1
	DB 0FH,22H,0C0H                 ; MOV CR0,EAX
	DB 0EAH                         ; JMP FAR 30h:reflect_back
	DW OFFSET reflect_back
	DW 30H
reflect_back:
	MOV AX,18H
	MOV DS,AX
	MOV ES,AX
	MOV AX,28H
	MOV SS,AX
	DB 0FH,01H,1EH                  ; LIDT [pm_idt]
	DW OFFSET pm_idt
	POP ES
	POP DS
	POPA
	IRET

; the driver's event handler, a real-mode far routine: AX the events, BX
; the buttons, CX and DX the position
mouse_handler:
	PUSH DS
	PUSH AX
	MOV AX,DATA
	MOV DS,AX
	POP AX
	INC calls
	OR r_mask,AX
	MOV r_buttons,BX
	MOV r_x,CX
	MOV r_y,DX
	DB 0FH,20H,0C0H                 ; MOV EAX,CR0
	AND AX,1
	OR r_pe,AX
	POP DS
	RETF
CODE ENDS

DATA SEGMENT PARA PUBLIC 'DATA'
; the GDT as INT 15h AH=89h takes it
gdt	DQ 0
d_gdt	DW gdt_end-gdt-1,0,0,0
d_idt	DW idt_end-idt-1,0,0,0
d_data	DW 0FFFFH,0,9200H,0             ; 18h DATA
d_extra	DW 0FFFFH,0,9200H,0             ; 20h DATA again
d_stack	DW 0FFFFH,0,9200H,0             ; 28h STACK
d_code	DW 0FFFFH,0,9A00H,0             ; 30h CODE
	DQ 0                            ; 38h the BIOS's
gdt_end:
; the IDT: IRQ 12 alone, a 16-bit interrupt gate at 2Ch
idt	DQ 44 DUP (0)
	DW OFFSET irq12_reflect,30H,8600H,0
idt_end:
real_idt DW 3FFH,0,0
pm_idt	DW idt_end-idt-1,0,0
pic1_mask DB 0
pic2_mask DB 0
calls	DW 0
reflects DW 0
r_reset	DW 0
r_mask	DW 0
r_buttons DW 0
r_x	DW 0
r_y	DW 0
r_pe	DW 0
text_ok	DB 'pmmouse ok',13,10,'$'
text_failed DB 'pmmouse FAILED at check '
text_which DB '?',13,10,'$'
DATA ENDS

STACK SEGMENT PARA STACK 'STACK'
	DB 400H DUP (?)
STACK ENDS

	END start
