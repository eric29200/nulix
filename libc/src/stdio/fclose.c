#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "__stdio_impl.h"

int fclose(FILE *fp)
{
	int ret;

	/* flush stream */
	ret = fflush(fp);

	/* close file */
	ret |= close(fp->fd);

	/* unlist file */
	__file_del(fp);

	/* free file */
	free(fp);

	return ret;
}
