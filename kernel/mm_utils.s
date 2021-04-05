[GLOBAL copy_page_physical]
copy_page_physical:
	push ebx              ; save ebx content and eflags
	pushf

	cli                   ; disable interrupts

	mov ebx, [esp+12]     ; src address
	mov ecx, [esp+16]     ; dst address

	mov edx, cr0          ; disable paging
	and edx, 0x7fffffff
	mov cr0, edx

	mov edx, 1024         ; size to copy (1024 * 4 bytes)

.loop:
	mov eax, [ebx]        ; copy
	mov [ecx], eax
	add ebx, 4
	add ecx, 4
	dec edx
	jnz .loop

	mov edx, cr0          ; enable paging
	or  edx, 0x80000000
	mov cr0, edx

	popf                  ; restore ebx contant and eflags
	pop ebx
	ret
