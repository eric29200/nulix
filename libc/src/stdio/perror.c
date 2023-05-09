#include <errno.h>
#include <string.h>

void perror(const char *msg)
{
	char *errstr = strerror(errno);

	/* write message first */
	if (msg && *msg) {
		fwrite(msg, strlen(msg), 1, stderr);
		fputc(':', stderr);
		fputc(' ', stderr);
	}

	/* write error message */
	fwrite(errstr, strlen(errstr), 1, stderr);
	fputc('\n', stderr);
}