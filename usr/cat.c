#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

static char buf[BUFSIZ];

static int __copy_fd(int fd)
{
	int n;

	while ((n = read(fd, buf, BUFSIZ)) > 0)
		if (write(STDOUT_FILENO, buf, n) != n)
			return EXIT_FAILURE;

	return n < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
	int ret, fd, i;

	if (argc <= 1) {
		ret = __copy_fd(STDIN_FILENO);
		//if (ret)
		//	perror("stdin");

		return ret;
	}

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-' && argv[i][1] == '\0') {
			fd = STDIN_FILENO;
		} else if ((fd = open(argv[i], O_RDONLY)) < 0) {
			//perror(argv[i]);
			return EXIT_FAILURE;
		}

		ret = __copy_fd(fd);
		if (ret) {
			//perror(argv[i]);
			close(fd);
			return ret;
		}

		if (fd != STDIN_FILENO)
			close(fd);
	}

	return EXIT_SUCCESS;
}
