#include <stdio.h>
#include <string.h>

#include "__stdio_impl.h"

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *fp)
{
	unsigned char *dest = ptr;
	size_t len, rem, k;

	/* set lengths */
	len = size * nmemb;
	rem = len;

	/* read from file buffer first */
	if (fp->rpos != fp->rend) {
		k = MIN(fp->rend - fp->rpos, rem);
		memcpy(dest, fp->rpos, k);
		fp->rpos += k;
		dest += k;
		rem -= k;
	}

	/* read the remainder */
	for (; rem; rem -= k, dest += k) {
		k = __to_read(fp) ? 0 : fp->read(fp, dest, rem);
		if (!k)
			return (len - rem) / size;
	}

	return nmemb;
}
