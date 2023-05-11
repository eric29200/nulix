#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#include "__stdio_impl.h"

/*
 * Private cookie holding a string.
 */
struct cookie_t {
	char *		str;		/* string */
	size_t		rem;		/* remain string length */
};

/*
 * Write to a string.
 */
static size_t str_write(FILE *fp, const unsigned char *buf, size_t len)
{
	struct cookie_t *cookie = fp->cookie;
	size_t n;

	/* write buffered characters */
	n = MIN(cookie->rem, (size_t) (fp->wpos - fp->wbase));
	if (n) {
		memcpy(cookie->str, fp->wbase, n);
		cookie->str += n;
		cookie->rem -= n;
	}

	/* write string */
	n = MIN(cookie->rem, len);
	if (n) {
		memcpy(cookie->str, buf, n);
		cookie->str += n;
		cookie->rem -= n;
	}

	/* end string */
	*cookie->str = 0;

	/* clear buffer */
	fp->wpos = fp->wbase = fp->buf;

	return len;
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
	unsigned char buf[1];
	char dummy[1];
	struct cookie_t cookie = {
		.str		= size ? str : dummy,
		.rem		= size ? size - 1 : 0,
	};
	FILE fp = {
		.lbf		= EOF,
		.write		= str_write,
		.buf		= buf,
		.cookie		= &cookie,
	};

	if (size > INT_MAX) {
		errno = EOVERFLOW;
		return -1;
	}

	*cookie.str = 0;
	return vfprintf(&fp, format, ap);
}
