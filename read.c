#include <stdio.h>
#include <err.h>

#include "read.h"
#include "str.h"

/* A reader interface for stdio */

static void *
stdio_open(void *fctxt, const struct str *path_str)
{
	char path[1024];
	unsigned len = str_len(path_str);
	FILE *f;

	if (len >= sizeof path - 1)
		errx(1, "path too long");
	str_copy(path_str, path, 0, len);
	path[len] = '\0';
	f = fopen(path, "rb");
	return f;
}

static int
stdio_read(void *rctxt, char *dst, unsigned len)
{
	return fread(dst, 1, len, (FILE *)rctxt);
}

static void
stdio_close(void *rctxt)
{
	fclose(rctxt);
}

const struct reader stdio_reader = {
	stdio_open,
	stdio_read,
	stdio_close
};
