#include <stdio.h>
#include <getopt.h>

int getopt(int argc, char *const argv[], const char *optstring)
{
	return getopt_long(argc, argv, optstring, NULL, 0);
}