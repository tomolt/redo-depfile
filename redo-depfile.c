/*
 * redo-depfile: A companion tool for redo that can import dependency-only Makefiles.
 *
 * Copyright (c) 2021 Thomas Oltmann
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * SPDX-License-Identifier: ISC
 */

#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#define IFCHANGE_CMD "redo-ifchange"
//#define IFCHANGE_CMD "echo"

/* 
 * early on we remap syntactically relevant characters in the input file
 * to distinguish them from escaped (literal) characters.
 */
#define SYN_WS  0x1F /* ascii unit separator */
#define SYN_EQ  0x1E /* ascii record separator */
#define SYN_COL 0x1D /* ascii group separator */
#define SYN_SC  0x1C /* ascii file separator */

#define IS_CTRL(c) ((c)<0x20)

static char **deps;
static size_t ndeps;
static size_t cdeps;

static FILE  *file;

static char  *line;
static size_t linesz;
static char  *part;
static size_t partsz;

static void
die(const char *msg)
{
	int err = errno;
	fputs(msg, stderr);
	if (*msg && msg[strlen(msg)-1] == ':') {
		fputc(' ', stderr);
		fputs(strerror(err), stderr);
	}
	fputc('\n', stderr);
	exit(EXIT_FAILURE);
}

/*
 * not all platforms offer reallocarray, so we provide our own.
 */
void *
reallocarray(void *ptr, size_t m, size_t n)
{
	if (n && m > -1 / n) {
		errno = ENOMEM;
		return 0;
	}
	return realloc(ptr, n * m);
}

/*
 * strbut: find the first occurrence of any other character than c in str,
 * or NULL if str contains only c.
 */
static char *
strbut(const char *str, int c)
{
	while (*str == c) str++;
	return *str ? (char *) str : NULL;
}

static void
putdep(char *dep)
{
	if (ndeps == cdeps) {
		cdeps *= 2;
		deps = reallocarray(deps, cdeps, sizeof *deps);
		if (!deps) die("realloc:");
	}
	deps[ndeps++] = dep;
}

static void
extendline(size_t len)
{
	if (getline(&part, &partsz, file) < 0) {
		if (ferror(file)) die("couldn't read file:");
		else die("missing line after backslash");
	}
	linesz = len + partsz;
	line = realloc(line, linesz);
	if (!line) die("realloc:");
	memcpy(line + len, part, partsz);
}

/*
 * nextline: read a new line and put it in the global variable line.
 * perform all sorts of preprocessing to make parsing trivial.
 */
static int
nextline(void)
{
	size_t r = 0, w = 0;
	if (getline(&line, &linesz, file) < 0) {
		if (ferror(file)) die("couldn't read file:");
		return 0;
	}
	for (;;) {
		assert(w <= r);
		assert(r < linesz);
		switch (line[r]) {
		/* forbid problematic control characters */
		case SYN_WS: case SYN_EQ: case SYN_COL: case SYN_SC:
			die("file may not contain ASCII separator characters");
			break;
		/* handle backslash escape sequences */
		case '\\':
			switch (line[++r]) {
			case '\0': case '\r': case '\n':
				line[w++] = SYN_WS;
				extendline(w);
				r = w + 1;
				break;
			case ' ': case '\\': case ':': case '=': case ';':
				line[w++] = line[r++];
				break;
			default:
				die("invalid backslash escape sequence");
			}
			break;
		/* handle dollar escape sequences / macro substitutions */
		case '$':
			if (line[r + 1] == '$') {
				line[w++] = '$';
				r += 2;
			} else {
				die("macro substitutions are not supported");
			}
			break;
		/* remap syntactically relevant characters */
		case '\r': case '\n':
		case ' ':  line[w++] = SYN_WS;  r++; break;
		case '=':  line[w++] = SYN_EQ;  r++; break;
		case ':':  line[w++] = SYN_COL; r++; break;
		case ';':  line[w++] = SYN_SC;  r++; break;
		/* strip comments */
		case '#':  line[w++] = '\0';    r++; break;
		/* copy anything else verbatim */
		case '\0': line[w++] = '\0'; return 1;
		default:   line[w++] = line[r++];
		}
	}
}

int
main(int argc, const char *argv[])
{
	char *c, *dep;
	size_t l;

	cdeps = 32;
	deps = calloc(cdeps, sizeof *deps);

	if (argc != 2) die("usage: redo-depfile FILE");
	file = stdin;
	if (strcmp(argv[1], "-")) {
		file = fopen(argv[1], "r");
		if (!file) die("couldn't open file:");
	}

	while (nextline()) {
		/* ignore recipe definitions */
		if (line[0] == '\t') {
			fprintf(stderr, "ignoring recipe definitions\n");
			continue;
		}
		/* skip empty lines */
		if (!strbut(line, SYN_WS)) {
			continue;
		}
		/* skip macro definition lines */
		if (strchr(line, SYN_EQ)) {
			continue;
		}
		/* recognize rule definition lines */
		if ((c = strchr(line, SYN_COL))) {
			c++;
			while ((c = strbut(c, SYN_WS))) {
				if (*c == SYN_SC) {
					fprintf(stderr, "ignoring recipe definitions\n");
					break;
				}
				if (IS_CTRL(*c)) {
					die("invalid rule definition syntax");
				}
				for (l = 0; !IS_CTRL(c[l]); l++) {}
				dep = strndup(c, l);
				if (!dep) die("malloc:");
				putdep(dep);
				c += l;
			}
			continue;
		}
		/* ignore any unrecognized lines */
		fprintf(stderr, "ignoring unrecognized line\n");
	}

#if 1
	fclose(file);
	free(line);
	free(part);
#endif

	putdep(NULL);
	execvp(IFCHANGE_CMD, deps);
	die("unable to execute " IFCHANGE_CMD ":");
}

