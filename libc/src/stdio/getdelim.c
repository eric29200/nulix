#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "__stdio_impl.h"

ssize_t getdelim(char **lineptr, size_t *size, int delim, FILE *fp)
{
	size_t k, n = 0, m;
	unsigned char *z;
	char *tmp;
	int c;

	/* check arguments */
	if (!lineptr || !size) {
		fp->flags |= F_ERR;
		errno = EINVAL;
		return -1;
	}

	/* init string */	
	if (!*lineptr)
		*lineptr = NULL;

	for (;;) {
		/* find delimiter in file buffer */
		if (fp->rpos != fp->rend) {
			z = memchr(fp->rpos, delim, fp->rend - fp->rpos);
			k = z ? z - fp->rpos + 1 : fp->rend - fp->rpos;
		} else {
			z = NULL;
			k = 0;
		}

		/* grow lineptr if needed */
		if (n + k >= *size) {
			/* reallocate lineptr */
			m = n + k + 2;
			tmp = realloc(*lineptr, m);
			if (!tmp) {
				/* copy remaining buffer */
				k = *size - n;
				memcpy(*lineptr + n, fp->rpos, k);
				fp->rpos += k;

				/* error */
				fp->flags |= F_ERR;
				errno = ENOMEM;
				return -1;
			}

			/* update lineptr */
			*lineptr = tmp;
			*size = m;
		}

		/* copy file buffer to result */
		if (k) {
			memcpy(*lineptr + n, fp->rpos, k);
			fp->rpos += k;
			n += k;
		}

		/* delimiter found */
		if (z)
			break;

		/* read next character */
		c = getc(fp);
		if (c < 0) {
			if (!n || !feof(fp))
				return -1;

			break;
		}

		/* if the byte read won't fit in the output buffer, push it back in file buffer (for next iteration)*/
		if (n + 1 >= *size) {
			*--fp->rpos = c;
			continue;
		}

		/* else store byte */
		(*lineptr)[n++] = c;
		if (c == delim)
			break;
	}

	/* end string */
	(*lineptr)[n] = 0;

	return n;
}