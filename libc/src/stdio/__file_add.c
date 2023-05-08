#include <stdio.h>

#include "__stdio_impl.h"

FILE *files_list = NULL;
 
FILE *__file_add(FILE *fp)
{
	fp->prev = NULL;
	fp->next = files_list;

	if (files_list)
		files_list->prev = fp;

	files_list = fp;

	return fp;
}
