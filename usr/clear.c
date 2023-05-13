#include <stdio.h>
#include <unistd.h>

#define CLEAR_SEQ	"\x1b[H\x1b[2J"

int main()
{
	write(STDOUT_FILENO, CLEAR_SEQ, sizeof(CLEAR_SEQ) - 1);

	return 0;
}
