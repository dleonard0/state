#include <stdio.h>
#include <stdarg.h>

#include "rule.h"
#include "str.h"
#include "pr.h"

/* Log/message printing */

enum verbosity verbosity = V_WARNING;

/* Prefixes to printr at the various log levels */
static const char *prefix[] = {
	[V_ERROR]   = "error: ",
	[V_WARNING] = "warn: ",
	[V_VERBOSE] = "",
	[V_DEBUG]   = "debug: ",
};

void
prl_(const char *file, int line, enum verbosity level,
	const struct location *loc, const char *fmt, ...)
{
	va_list ap;

	if (loc) {
		char path[1024];
		unsigned pathlen;

		pathlen = str_copy(loc->filename, path, 0, sizeof path);
		fprintf(stderr, "%.*s:%u: ", pathlen, path, loc->lineno);
	}

	fputs(prefix[level], stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	putc('\n', stderr);
}
