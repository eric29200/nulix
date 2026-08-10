#ifndef _PARSER_H_
#define _PARSER_H_

#include <stdio.h>

#define MAX_OPT_ARGS	3

/*
 * Substring.
 */
struct substring {
	char *		from;
	char *		to;
};

/*
 * Match token.
 */
struct match_token {
	int		token;
	const char *	pattern;
};

int match_token(char *string, const struct match_token tokens[], struct substring args[]);
size_t match_strlcpy(char *dest, const struct substring *src, size_t size);
int match_int(struct substring *s, int *res);

#endif