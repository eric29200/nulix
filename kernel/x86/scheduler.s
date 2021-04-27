global scheduler_do_switch
global enter_user_mode
global return_user_mode

scheduler_do_switch:
	cli
	pusha

	mov eax, [esp + (8 + 1) * 4] 		; load current task's kernel stack address
	mov [eax], esp      			; save esp for current task's kernel stack

	mov eax, [esp + (8 + 2) * 4]     	; load next task's kernel stack to esp
	mov esp, eax

	popa
	sti
	ret

enter_user_mode:
	cli

	mov ax, 0x23			; set user segment registers
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	mov eax, [esp + 4]		; get user stack
	sub eax, 4

	mov ebx, [esp + 12]		; get user return address
	mov dword [eax], ebx

	mov ebx, [esp + 8]		; get user eip

	push 0x23			; stack segment to restore
	push eax
	pushf

	pop eax				; enable interrupts
	or eax, 0x200
	push eax

	push 0x1B			; code segment to restor
	push ebx
	iret

return_user_mode:
	cli

	mov ax, 0x23			; set user segment registers
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	mov eax, [esp + 4]		; get user stack

	push dword [eax + 15*4] 	; user data segment
	push dword [eax + 14*4] 	; push our current stack
	push dword [eax + 13*4]		; EFLAGS
	push dword [eax + 12*4] 	; segment selector
	push dword [eax + 11*4] 	; eip

	mov edi, [eax + 1*4]
	mov esi, [eax + 2*4]
	mov ebp, [eax + 3*4]
	mov ebx, [eax + 5*4]
	mov edx, [eax + 6*4]
	mov ecx, [eax + 7*4]
	mov eax, [eax + 8*4]

	iret
