#include <stdlib.h>
#include <assert.h>

#include "rule.h"
#include "varscope.h"
#include "str.h"
#include "read.h"

/* Unit tests for rule file parsing */

/** A dummy pathname to pass the parser. */
static struct str *PATH;

/*------------------------------------------------------------
 * Dummy reader interface
 *
 * Allows the parser to read from a string, instead of a file.
 */

/** Dummy reader context */
struct test_ctxt {
	const char *p;	/**< next character in string available for read */
};

static void *
test_open(void *fctxt, const struct str *path)
{
	struct test_ctxt *ctxt = malloc(sizeof *ctxt);
	assert(path == PATH);
	ctxt->p = fctxt;
	return ctxt;
}

static int
test_read(void *rctxt, char *dst, unsigned len)
{
	struct test_ctxt *ctxt = rctxt;
	unsigned rlen = 0;
	if (!ctxt->p) {
		return -1;
	}
	while (len && *ctxt->p) {
		len--;
		rlen++;
		*dst++ = *ctxt->p++;
	}
	return rlen;
}

static void
test_close(void *rctxt)
{
	free(rctxt);
}

static const struct reader test_reader = {
	.open = test_open,
	.read = test_read,
	.close = test_close
};

/*------------------------------------------------------------*/

int
main()
{
	PATH = str_new("<internal>");
	{
		struct rule *rules, **rp;
		struct varscope *scope = varscope_new(0);
		rp = rules_parse(&rules, PATH, scope, &test_reader,
			"");
		*rp = 0;
		assert(!rules);
		rules_free(&rules);
	}
	{
		struct rule *rules, **rp;
		struct varscope *scope = varscope_new(0);
		rp = rules_parse(&rules, PATH, scope, &test_reader,
			"X = 1\n"
			"a: ; cmd line\n"
			"# comment\n"
			"b: c d\n"
			"\tcmd$(X)\n"
			"\tcmd2\n"
			);
		*rp = 0;
		/* TODO more tests */

		rules_free(&rules);
	}

	str_free(PATH);
	return 0;
}
