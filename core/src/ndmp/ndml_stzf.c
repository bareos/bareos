/*
 * Copyright 2000
 *	Traakan, Inc., Los Altos, CA
 *	All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Project:  NDMJOB
 * Ident:    $Id: $
 *
 * Description:
 *	Stanza files look about like this:
 *		[stanza name line]
 *		stanza body lines
 *
 *	These are used for config files.
 */


#include "ndmlib.h"


int
ndmstz_getline (FILE *fp, char *buf, int n_buf)
{
	int		c;
	char *		p;

  again:
	c = getc (fp);
	if (c == EOF)
		return EOF;

	if (c == '[') {
		/* end-of-stanza */
		ungetc (c, fp);
		return -2;
	}

	if (c == '#') {
		/* comment */
		while ((c = getc(fp)) != EOF && c != '\n')
			continue;
		goto again;
	}

	ungetc (c, fp);
	p = buf;
	while ((c = getc(fp)) != EOF && c != '\n') {
		if (p < &buf[n_buf-1])
			*p++ = c;
	}
	*p = 0;
	return p - buf;
}

int
ndmstz_getstanza (FILE *fp, char *buf, int n_buf)
{
	int		c;
	char *		p;

  again:
	c = getc (fp);
	if (c == EOF)
		return EOF;

	if (c == '\n')
		goto again;	/* blank line */

	if (c != '[') {
		/* not a stanza header, eat line */
		while ((c = getc(fp)) != EOF && c != '\n')
			continue;
		goto again;
	}

	p = buf;
	while ((c = getc(fp)) != EOF && c != '\n' && c != ']') {
		if (p < &buf[n_buf-1])
			*p++ = c;
	}
	*p = 0;

	if (c == ']') {
		/* eat rest of line */
		while ((c = getc(fp)) != EOF && c != '\n')
			continue;
	}

	/* fp is left pointing to begining of first line */

	return p - buf;
}

int
ndmstz_parse (char *buf, char *argv[], int max_argv)
{
	char *		p = buf;
	char *		q = buf;
	int		inword = 0;
	int		inquote = 0;
	int		argc = 0;
	int		c;

	while ((c = *p++) != 0) {
		if (inquote) {
			if (c == inquote) {
				inquote = 0;
			} else {
				*q++ = c;
			}
			continue;
		}

		if (isspace(c)) {
			if (inword) {
				*q++ = 0;
				inword = 0;
			}
			continue;
		}

		if (!inword) {
			if (argc > max_argv-1)
				break;
			argv[argc++] = q;
			inword = 1;
		}

		if (c == '"' || c == '\'') {
			inquote = c;
			continue;
		}

		*q++ = c;
	}
	if (inword)
		*q++ = 0;
	argv[argc] = 0;

	return argc;
}



#ifdef SELF_TEST

int
main (int ac, char *av[])
{
	int		i, found, argc;
	FILE *		fp;
	char		buf[512];
	char *		argv[100];

	if (ac < 2) {
		printf ("bad usage\n");
		return 1;
	}

	fp = fopen (av[1], "r");
	if (!fp) {
		perror (av[1]);
		return 2;
	}

	if (ac == 2) {
		while (ndmstz_getstanza (fp, buf, sizeof buf) >= 0)
			printf ("%s\n", buf);
	} else {
		for (i = 2; i < ac; i++) {
			rewind (fp);
			found = 0;
			while (ndmstz_getstanza (fp, buf, sizeof buf) >= 0) {
				if (strcmp (av[i], buf) == 0) {
					found = 1;
					break;
				}
			}
			if (!found) {
				printf ("Search for '%s' failed\n", av[i]);
				continue;
			}
			printf ("'%s'\n", buf);
			printf ("========================================\n");
			while (ndmstz_getline (fp, buf, sizeof buf) >= 0) {
				printf ("= %s", buf);
				argc = ndmstz_parse (buf, argv, 100);
				printf (" [%d]\n", argc);
			}
			printf ("========================================\n");
		}
	}

	fclose (fp);

	return 0;
}

#endif /* SELF_TEST */
