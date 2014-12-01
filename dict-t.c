#include <assert.h>
#include "dict.h"

static int counter;
static void
inc_counter(void *value)
{
	counter++;
}

int
main(void) {
	
	void *A = "A";
	void *B = "B";
	void *C = "C";

	/* Basic use */
	{
		struct dict *d;
		counter = 0;
		d = dict_new(inc_counter, 0, 0);
		assert(counter == 0);
		assert(!dict_get(d, 0));
		assert(!dict_get(d, A));
		assert(dict_put(d, A, B) == 0);
		assert(dict_get(d, A) == B);
		assert(dict_count(d) == 1);
		assert(counter == 0);

		/* delete */
		assert(dict_put(d, A, 0));
		assert(dict_put(d, A, 0) == 0);
		assert(dict_count(d) == 0);
		assert(counter == 1);
		dict_free(d);
	}
	{
		struct dict *d;
		struct dict_iter *di;
		unsigned i, r;
		const void *k;
		void *v;

		counter = 0;
		d = dict_new(inc_counter, 0, 0);

		assert(dict_put(d, A, B) == 0);
		assert(dict_put(d, B, C) == 0);
		assert(dict_put(d, C, A) == 0);
		assert(dict_put(d, C, A) == 1);
		assert(counter == 1);

		r = 1;
		di = dict_iter_new(d);
		for (i = 0; i < 3; ++i) {
		    assert(dict_iter_next(di, &k, &v));
		    if (k == A) {
		    	r *= 2;
			assert(v == B);
		    } else if (k == B) {
		    	r *= 3;
			assert(v == C);
		    } else {
		    	assert(k == C);
			r *= 5;
			assert(v == A);
		    }
		}
		assert(r == 2 * 3 * 5);
		assert(!dict_iter_next(di, &k, &v));

		dict_iter_free(di);
		dict_free(d);
	}

	return 0;
}

