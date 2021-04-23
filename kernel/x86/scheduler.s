global scheduler_do_switch
global enter_usermode

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

enter_usermode:
	cli

	mov ax,0x23
	mov ds,ax
	mov es,ax
	mov fs,ax
	mov gs,ax

	mov eax, [esp + 4]
	sub eax, 4
	mov ebx, [esp + 12]
	mov dword [eax], ebx

	mov ebx, [esp + 8] ;

	push 0x23
	push eax
	pushf

	pop eax
	or eax, 0x200
	push eax

	push 0x1B
	push ebx
	iret
