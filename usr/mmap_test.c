#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>

#define BUF_SIZE 	128
#define PATH		"./data"

int main()
{
	void *buf;
	pid_t pid;
	int fd;

	/* open file */
	fd = open(PATH, O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (fd < 0)
		return -1;

	/* grow file */
	buf = malloc(BUF_SIZE);
	memset(buf, 0, BUF_SIZE);
	write(fd, buf, BUF_SIZE);
	free(buf);

	/* create shared buffer */
	buf = mmap(NULL, BUF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (!buf) {
		close(fd);
		return -1;
	}

	strcpy(buf, "hello");

	pid = fork();
	if (pid == 0) {
		printf("Child read : %s\n", buf);
		strcpy(buf, "goodbye");
		exit(0);
	}

	waitpid(pid, NULL, 0);
	printf("Parent read : %s\n", buf);

	/* unmap buffer */
	munmap(buf, BUF_SIZE);
	close(fd);

	return 0;
}
