; ADLIB.EXE - the kit's test of the runner's OPL2 timers (tests/selftest.py):
;   1. register 4 = 60h (both timers masked and stopped), then 80h (the
;      flags cleared): the status's bits 5-7 read 0,
;   2. register 2 = FFh (timer 1 overflows after 80 us), register 4 = 21h
;      (timer 2 masked, timer 1 started): within 10000 reads the status's
;      bits 5-7 read C0h (IRQ and timer 1, not the masked timer 2),
;   3. register 4 = 80h: they read 0 again,
;   4. register 3 = FFh, register 4 = 42h (timer 1 masked, timer 2
;      started): within 10000 reads they read A0h,
;   5. register 4 = 80h, register 2 = 00h (20.48 ms), register 4 = 21h:
;      the first 20 reads still read 0 (the timer runs on time, not at once),
; and prints "adlib ok", or "adlib FAILED" and the number of the check that
; failed.  The usual way a driver probes for the chip.  Then it plays one
; note for -oplwav: a sine of 440 Hz at full level on channel 0 for six
; overflows of timer 2 (491.52 ms), then the key up and two more (the
; release, then silence); selftest.py checks the WAV.  Written for the kit,
; no game's code.
CODE SEGMENT PARA PUBLIC 'CODE'
	ASSUME CS:CODE,DS:DATA,ES:NOTHING,SS:STACK
start:
	MOV AX,DATA
	MOV DS,AX
	MOV BL,'1'
	MOV AX,6004H
	CALL opl_out
	MOV AX,8004H
	CALL opl_out
	CALL status
	JNE failed
	MOV BL,'2'
	MOV AX,0FF02H
	CALL opl_out
	MOV AX,2104H
	CALL opl_out
	MOV AH,0C0H
	CALL wait_for
	JNE failed
	MOV BL,'3'
	MOV AX,8004H
	CALL opl_out
	CALL status
	JNE failed
	MOV BL,'4'
	MOV AX,0FF03H
	CALL opl_out
	MOV AX,4204H
	CALL opl_out
	MOV AH,0A0H
	CALL wait_for
	JNE failed
	MOV BL,'5'
	MOV AX,8004H
	CALL opl_out
	MOV AX,0002H
	CALL opl_out
	MOV AX,2104H
	CALL opl_out
	MOV CX,20
early:
	CALL status
	JNE failed
	LOOP early
	MOV AX,6004H
	CALL opl_out
	MOV AX,8004H
	CALL opl_out
	CALL play
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

; register AL = AH
opl_out:
	MOV DX,388H
	OUT DX,AL
	INC DX
	MOV AL,AH
	OUT DX,AL
	RET

; the status's bits 5-7 in AL, ZF set when they are 0
status:
	MOV DX,388H
	IN AL,DX
	AND AL,0E0H
	RET

; up to 10000 reads until the status's bits 5-7 are AH; ZF set when they were
wait_for:
	MOV CX,10000
wait_loop:
	CALL status
	CMP AL,AH
	JE wait_done
	LOOP wait_loop
	CMP AL,AH
wait_done:
	RET

; CX overflows of timer 2 from register 3 = 00h (81.92 ms each)
ticks:
	PUSH CX
	MOV AX,0003H
	CALL opl_out
	MOV AX,4204H                    ; timer 1 masked, timer 2 started
	CALL opl_out
	POP CX
tick_loop:
	PUSH CX
	MOV AX,8004H                    ; the flags cleared
	CALL opl_out
tick_wait:
	CALL status
	CMP AL,0A0H
	JNE tick_wait
	POP CX
	LOOP tick_loop
	MOV AX,6004H
	CALL opl_out
	MOV AX,8004H
	CALL opl_out
	RET

play:
	; channel 0's modulator silent (attack rate 0), its carrier
	; a sine at full level (multiple 1, sustained, attack 15, decay 0,
	; sustain level 0, release 15), F-number 244h in block 4 (440 Hz)
	MOV SI,OFFSET note
	MOV CX,note_n
note_loop:
	LODSW
	PUSH CX
	CALL opl_out
	POP CX
	LOOP note_loop
	MOV CX,6
	CALL ticks
	MOV AX,12B0H                    ; the key up, the same F-number
	CALL opl_out
	MOV CX,2
	CALL ticks
	RET
CODE ENDS

DATA SEGMENT PARA PUBLIC 'DATA'
; register, value pairs as opl_out takes them (AL, AH)
note	DW 0001H, 0120H, 2123H, 3F40H, 0043H, 0060H, 0F063H, 0080H, 0F83H
	DW 01C0H, 44A0H, 32B0H
note_n	EQU 12
text_ok	DB 'adlib ok',13,10,'$'
text_failed DB 'adlib FAILED at check '
text_which DB '?',13,10,'$'
DATA ENDS

STACK SEGMENT PARA STACK 'STACK'
	DB 400H DUP (?)
STACK ENDS

	END start
