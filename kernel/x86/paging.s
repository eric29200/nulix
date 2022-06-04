global copy_page_physical

copy_page_physical:
	push ebx			; save ebx and eflags
	pushf

	mov ebx, [esp+12]		; src address
	mov ecx, [esp+16]		; dst address

	mov edx, cr0			; disable paging
	and edx, 0x7fffffff
	mov cr0, edx

	mov edx, 1024			; 1024 * 4 bytes = 4096 bytes to copy

.loop:
	mov eax, [ebx]			; get the word at the source address
	mov [ecx], eax			; store it at the dest address
	add ebx, 4			; src address += sizeof(word)
	add ecx, 4			; dst address += sizeof(word)
	dec edx				; one less word to do
	jnz .loop

	mov edx, cr0			; enable paging
	or	edx, 0x80000000
	mov cr0, edx

	popf				; restore eflags and ebx
	pop ebx
	ret
