; PMODE.EXE - the kit's test of the runner's protected mode
; (tests/selftest.py): enters it through the BIOS (INT 15h AH=89h), then
;   1. writes and reads memory above 1 MB through a flat 32-bit selector,
;   2. loads a selector beyond the GDT: #GP with the selector as the error
;      code, the handler steps over the instruction,
;   3. waits with HLT for IRQ0, through the IDT (the PIC's base from BH),
;   4. goes to ring 3 by IRETD; INT 30h from there runs on the ring 0
;      stack of the TSS; a far CALL through a call gate comes back to
;      ring 0,
;   5. goes to V86 mode by IRETD: INT 30h from there, IN from a port the
;      I/O bitmap allows, OUT to one it refuses (#GP), INT 31h back,
;   6. returns to real mode and opens its own file for writing (the
;      runner's DOS copies it into its writable layer, whose folder does
;      not exist yet in the selftest),
; and prints "pmode ok", or "pmode FAILED" and the number of the check
; that failed.
; tasm.py has no system instructions: they are written as DB.  Written
; for the kit; no game's code.
.386
CODE SEGMENT PARA PUBLIC USE16 'CODE'
	ASSUME CS:CODE,DS:DATA,ES:NOTHING,SS:STACK
start:
	MOV AX,DATA
	MOV DS,AX
	MOV v86_ds,AX
	MOV v86_cs,CS
	MOV AX,STACK
	MOV v86_ss,AX
	MOV AH,88H
	INT 15H
	MOV r_extkb,AX
	; the descriptors' bases
	MOV AX,DATA
	CALL seg_lin
	MOV SI,OFFSET d_data
	CALL set_base
	MOV SI,OFFSET d_r3data
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
	MOV AX,BX
	MOV DX,CX
	ADD AX,OFFSET tss
	ADC DX,0
	MOV SI,OFFSET d_tss
	CALL set_base
	MOV AX,STACK
	CALL seg_lin
	MOV SI,OFFSET d_stack
	CALL set_base
	MOV SI,OFFSET d_stack32
	CALL set_base
	MOV AX,CODE
	CALL seg_lin
	MOV SI,OFFSET d_code
	CALL set_base
	MOV AX,CODE32
	CALL seg_lin
	MOV SI,OFFSET d_code32
	CALL set_base
	MOV SI,OFFSET d_r3code
	CALL set_base
	IN AL,21H
	MOV pic1_mask,AL
	; into protected mode: ES:SI the GDT, IRQs at 20h and 28h
	CLI
	PUSH DS
	POP ES
	MOV SI,OFFSET gdt
	MOV BX,2028H
	MOV AH,89H
	INT 15H
	; CS 30h, DS 18h, ES 20h, SS 28h
	MOV AX,48H
	DB 0FH,00H,0D8H                 ; LTR AX
	DB 66H,0EAH                     ; JMP FAR 40h:start32
	DW OFFSET start32,0
	DW 40H

; 16-bit protected mode, from start32: to real mode
back16:
	MOV AX,18H
	MOV DS,AX
	MOV ES,AX
	MOV FS,AX
	MOV GS,AX
	MOV AX,28H
	MOV SS,AX
	MOV SP,400H
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
	CMP r_extkb,0
	JE failed
	CMP WORD PTR r_flat,5678H
	JNE failed
	CMP WORD PTR r_flat+2,1234H
	JNE failed
	MOV BL,'2'
	CMP r_gperr,0F8H
	JNE failed
	MOV BL,'3'
	CMP ticks,0
	JE failed
	MOV BL,'4'
	MOV AX,r_r3cs
	AND AX,3
	CMP AX,3
	JNE failed
	CMP r_r3ss,60H
	JNE failed
	MOV AX,r_gatecs
	AND AX,3
	CMP AX,3
	JNE failed
	CMP r_gatecpl,40H
	JNE failed
	JMP checks5
failed_b:
	JMP failed
checks5:
	MOV BL,'5'
	MOV AX,v86_cs
	CMP r_v86cs,AX
	JNE failed_b
	TEST WORD PTR r_v86fl+2,2       ; VM
	JZ failed_b
	CMP r_v86mark,1234H
	JNE failed_b
	CMP r_v86in,1
	JNE failed_b
	CMP gp_count,2
	JNE failed_b
	CMP r_v86err,0
	JNE failed_b
	MOV BL,'6'
	MOV DX,OFFSET own_name
	MOV AX,3D02H
	INT 21H
	JC failed_b
	MOV BX,AX
	MOV AH,3EH
	INT 21H
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

; V86 mode, entered from ring 0 (DS = DATA)
v86_code:
	MOV r_v86mark,1234H
	INT 30H
	MOV AX,int30_cs
	MOV r_v86cs,AX
	MOV AX,WORD PTR int30_fl
	MOV WORD PTR r_v86fl,AX
	MOV AX,WORD PTR int30_fl+2
	MOV WORD PTR r_v86fl+2,AX
	MOV gp_err,0FFFFH
	IN AL,61H                       ; allowed by the bitmap
	MOV AX,gp_count
	MOV r_v86in,AX
	OUT 80H,AL                      ; refused: #GP, stepped over
	MOV AX,gp_err
	MOV r_v86err,AX
	INT 31H                         ; back to protected mode
CODE ENDS

CODE32 SEGMENT PARA PUBLIC USE32 'CODE'
	ASSUME CS:CODE32,DS:DATA,ES:NOTHING,SS:NOTHING
start32:
	MOV AX,60H
	MOV SS,AX
	MOV ESP,300H
	MOV AX,18H
	MOV DS,AX
	MOV AX,20H
	MOV ES,AX
	; 1. above 1 MB
	MOV DWORD PTR ES:[200000H],12345678H
	MOV EAX,ES:[200000H]
	MOV r_flat,EAX
	; 2. a selector beyond the GDT
	MOV AX,0F8H
	MOV DS,AX
	MOV AX,gp_err
	MOV r_gperr,AX
	; 3. IRQ0 alone
	MOV AL,0FEH
	OUT 21H,AL
	STI
wait_tick:
	HLT
	CMP ticks,0
	JE wait_tick
	CLI
	; 4. ring 3
	PUSH 5BH                        ; SS
	PUSH OFFSET r3stack_top         ; ESP
	PUSH 2                          ; EFLAGS
	PUSH 53H                        ; CS
	PUSH OFFSET ring3               ; EIP
	IRETD

ring3:
	MOV AX,5BH
	MOV DS,AX
	INT 30H
	MOV AX,int30_cs
	MOV r_r3cs,AX
	MOV AX,int30_ss
	MOV r_r3ss,AX
	DB 9AH                          ; CALL FAR 6Bh:0, the call gate
	DD 0
	DW 6BH

; the call gate's target, ring 0 on the TSS's stack
gate_entry:
	MOV AX,18H
	MOV DS,AX
	MOV EAX,[ESP+4]
	MOV r_gatecs,AX
	MOV AX,CS
	MOV r_gatecpl,AX
	ADD ESP,16
	; 5. V86 mode
	PUSH 0                          ; GS
	PUSH 0                          ; FS
	MOVZX EAX,WORD PTR v86_ds
	PUSH EAX                        ; DS
	PUSH 0                          ; ES
	MOVZX EAX,WORD PTR v86_ss
	PUSH EAX                        ; SS
	PUSH 100H                       ; ESP
	PUSH 23002H                     ; EFLAGS: VM, IOPL 3
	MOVZX EAX,WORD PTR v86_cs
	PUSH EAX                        ; CS
	PUSH OFFSET v86_code            ; EIP
	IRETD

; INT 31h from V86 mode: its frame dropped, on to real mode
int31_entry:
	ADD ESP,36
	MOV AX,18H
	MOV DS,AX
	DB 0EAH                         ; JMP FAR 30h:back16
	DW OFFSET back16,0
	DW 30H

; #GP: counted, its error code kept, the 2-byte instruction stepped over
gp_handler:
	PUSH DS
	PUSH EAX
	MOV AX,18H
	MOV DS,AX
	INC gp_count
	MOV EAX,[ESP+8]
	MOV gp_err,AX
	ADD DWORD PTR [ESP+12],2
	POP EAX
	POP DS
	ADD ESP,4
	IRETD

irq0_handler:
	PUSH DS
	PUSH EAX
	MOV AX,18H
	MOV DS,AX
	INC ticks
	MOV AL,20H
	OUT 20H,AL
	POP EAX
	POP DS
	IRETD

; INT 30h: the caller's CS and EFLAGS, the stack it runs on
int30_handler:
	PUSH DS
	PUSH EAX
	MOV AX,18H
	MOV DS,AX
	MOV EAX,[ESP+12]
	MOV int30_cs,AX
	MOV EAX,[ESP+16]
	MOV int30_fl,EAX
	MOV AX,SS
	MOV int30_ss,AX
	POP EAX
	POP DS
	IRETD
CODE32 ENDS

DATA SEGMENT PARA PUBLIC 'DATA'
; the GDT: 08h..30h as INT 15h AH=89h takes them, then the test's own
gdt	DQ 0
d_gdt	DW gdt_end-gdt-1,0,0,0
d_idt	DW idt_end-idt-1,0,0,0
d_data	DW 0FFFFH,0,9200H,0             ; 18h DATA, 16-bit
	DW 0FFFFH,0,9200H,00CFH         ; 20h flat 4 GB, 32-bit
d_stack	DW 0FFFFH,0,9200H,0             ; 28h STACK, 16-bit
d_code	DW 0FFFFH,0,9A00H,0             ; 30h CODE, 16-bit
	DQ 0                            ; 38h the BIOS's
d_code32 DW 0FFFFH,0,9A00H,0040H        ; 40h CODE32, 32-bit
d_tss	DW tss_end-tss-1,0,8900H,0      ; 48h the TSS, 32-bit
d_r3code DW 0FFFFH,0,0FA00H,0040H       ; 50h CODE32 at ring 3
d_r3data DW 0FFFFH,0,0F200H,0040H       ; 58h DATA at ring 3
d_stack32 DW 0FFFFH,0,9200H,0040H       ; 60h STACK, 32-bit (the TSS's ring 0 stack)
	DW OFFSET gate_entry,40H,0EC00H,0   ; 68h a call gate for ring 3
gdt_end:
; the IDT up to 31h
idt	DQ 13 DUP (0)
	DW OFFSET gp_handler,40H,8E00H,0    ; 0Dh #GP
	DQ 18 DUP (0)
	DW OFFSET irq0_handler,40H,8E00H,0  ; 20h IRQ0
	DQ 15 DUP (0)
	DW OFFSET int30_handler,40H,0EE00H,0  ; 30h, ring 3 may call it
	DW OFFSET int31_entry,40H,0EE00H,0    ; 31h
idt_end:
real_idt DW 3FFH,0,0
; the TSS: ring 0's stack, the I/O bitmap at 68h (ports 0-FFh, 80h refused)
tss	DD 0,300H,60H
	DD 22 DUP (0)
	DW 0,68H
	DB 16 DUP (0),01H,15 DUP (0)
	DB 0FFH
tss_end:
r3stack	DB 100H DUP (0)
r3stack_top:
v86_cs	DW 0
v86_ss	DW 0
v86_ds	DW 0
pic1_mask DB 0
ticks	DD 0
gp_count DW 0
gp_err	DW 0
int30_cs DW 0
int30_ss DW 0
int30_fl DD 0
r_extkb	DW 0
r_flat	DD 0
r_gperr	DW 0
r_r3cs	DW 0
r_r3ss	DW 0
r_gatecs DW 0
r_gatecpl DW 0
r_v86mark DW 0
r_v86cs	DW 0
r_v86fl	DD 0
r_v86in	DW 0
r_v86err DW 0
text_ok	DB 'pmode ok',13,10,'$'
own_name DB 'PMODE.EXE',0
text_failed DB 'pmode FAILED at check '
text_which DB '?',13,10,'$'
DATA ENDS

STACK SEGMENT PARA STACK 'STACK'
	DB 400H DUP (?)
STACK ENDS

	END start
