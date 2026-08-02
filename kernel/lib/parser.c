#include <lib/parser.h>
#include <mm/mm.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stderr.h>

/*
 * Check if a string matches a single pattern (return match locations in args).
 */
static int match_one(char *string, const char *pattern, struct substring args[])
{
	int len, argc = 0, str_len;
	char *meta;

	/* empty pattern = always match */
	if (!pattern)
		return 1;

	for (;;) {
		meta = strchr(pattern, '%');

		/* simple pattern, without format */
		if (!meta)
			return strcmp(string, pattern) == 0;

		/* compare option name */
		if (strncmp(string, pattern, meta - pattern))
			return 0;

		/* go to format */
		string += meta - pattern;
		pattern = meta + 1;

		/* next % = character */
		if (*pattern == '%') {
			if (*string++ != '%')
				return 0;
			pattern++;
			continue;
		}

		/* next digit = length */
		if (ISDIGIT(*pattern))
			len = simple_strtoul(pattern, (char **) &pattern, 10);
		else
			len = -1;

		/* maximum arguments reached */
		if (argc >= MAX_OPT_ARGS)
			return 0;

		/* store argument */
		args[argc].from = string;

		/* decoed format */
		switch (*pattern++) {
			case 's':
				str_len = strlen(string);
				if (str_len == 0)
					return 0;
				if (len == -1 || len > str_len)
					len = str_len;
				args[argc].to = string + len;
				break;
			case 'd':
				simple_strtol(string, &args[argc].to, 0);
				goto num;
			case 'u':
				simple_strtoul(string, &args[argc].to, 0);
				goto num;
			case 'o':
				simple_strtoul(string, &args[argc].to, 8);
				goto num;
			case 'x':
				simple_strtoul(string, &args[argc].to, 16);
num:
				if (args[argc].to == args[argc].from)
					return 0;
				break;
			default:
				return 0;
		}

		/* go to next string */
		string = args[argc++].to;
	}
}

/*
 * Find a token in a string (return match locations in args).
 */
int match_token(char *string, const struct match_token tokens[], struct substring args[])
{
	const struct match_token *token = tokens;

	/* try to find matching pattern */
	for (token = tokens;; token++)
		if (match_one(string, token->pattern, args))
			break;

	return token->token;
}

/*
 * Scan a number.
 */
static int match_number(struct substring *s, int *res, int base)
{
	size_t len = s->to - s->from;
	char *endp, *buf;
	int ret = 0;

	/* allocate a buffer */
	buf = kmalloc(len + 1);
	if (!buf)
		return -ENOMEM;

	/* copy string */
	memcpy(buf, s->from, len);
	buf[len] = 0;

	/* scan number */
	*res = simple_strtol(buf, &endp, base);

	/* check error */
	if (endp == buf)
		ret = -EINVAL;

	/* free buffer */
	kfree(buf);
	return ret;
}

/*
 * Scan a decimal representation of an integer.
 */
int match_int(struct substring *s, int *res)
{
	return match_number(s, res, 0);
}