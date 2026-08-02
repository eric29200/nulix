#ifndef _PARSER_H_
#define _PARSER_H_

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
int match_int(struct substring *s, int *res);

#endif