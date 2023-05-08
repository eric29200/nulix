#include "__stdio_impl.h"

void __stdio_exit()
{
	FILE *fp;

	for (fp = files_list; fp != NULL; fp = fp->next)
		fclose(fp);
}