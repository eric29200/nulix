global outb
global inb
global inw

section .text:

outb:
	mov al, [esp+8]
	mov dx, [esp+4]
	out dx, al
	ret

inb:
	mov dx, [esp+4]
	in al, dx
	ret

inw:
	mov dx, [esp+4]
	in ax, dx
	ret