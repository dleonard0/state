#include <stdlib.h>
#include <assert.h>

#include "atom.h"
#include "scope.h"

/* Unit tests for nestable scopes */

int
main(void)
{
	{
		void *m1 = malloc(1);
		void *m2 = malloc(1);
		void *m3 = malloc(1);

		atom A = atom_s("A");
		struct scope *scope = scope_new(0, free);

		/* test missing value */
		assert(!scope_get(scope, A));

		/* test overwriting */
		scope_put(scope, A, m1);
		assert(scope_get(scope, A) == m1);

		scope_put(scope, A, m2);
		assert(scope_get(scope, A) == m2);

		/* test an inner scope */
		struct scope *inner = scope_new(scope, free);
		assert(scope_get(inner, A) == m2);

		scope_put(inner, A, m3);
		assert(scope_get(inner, A) == m3);

		/* leave the inner scope */
		assert(scope_free(inner) == scope);

		/* ensure puts did not go through to the outer */
		assert(scope_get(scope, A) == m2);

		scope_free(scope);
	}
	return 0;
}
