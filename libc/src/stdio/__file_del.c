#include <stdio.h>

#include "__stdio_impl.h"

void __file_del(FILE *fp)
{
	if (fp->prev)
		fp->prev->next = fp->next;
	if (fp->next)
		fp->next->prev = fp->prev;

	if (files_list == fp)
		files_list = fp->next;
}