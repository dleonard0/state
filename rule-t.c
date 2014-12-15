#include <stdlib.h>
#include <assert.h>

#include "rule.h"
#include "varscope.h"
#include "str.h"
#include "read.h"

/*------------------------------------------------------------
 * a reader that reads from a C string
 */

static struct str *PATH;

struct test_ctxt {
	const char *p;
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
	}
	str_free(PATH);
	return 0;
}
