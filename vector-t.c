#include <assert.h>
#include "vector.h"

int
main()
{
	{
		/* vector_at, vector_append */
		AUTO_VECTOR_OF(int, v);
		vector_append(v, 1);
		vector_append(v, 4);
		vector_append(v, 2);
		assert(v.len == 3);
		assert(vector_at(v, 0) == 1);
		assert(vector_at(v, 1) == 4);
		assert(vector_at(v, 2) == 2);
	}
	{
		/* vector_copy, vector_concat, vector_for */
		AUTO_VECTOR_OF(int, v);
		AUTO_VECTOR_OF(int, v2);
		int *i;

		vector_append(v, 2);
		vector_append(v, 3);
		vector_append(v, 5);
		vector_copy(v2, v);
		assert(v2.len == 3);
		assert(vector_at(v2, 0) == 2);
		assert(vector_at(v2, 1) == 3);
		assert(vector_at(v2, 2) == 5);

		vector_concat(v, v2);
		assert(v.len == 6);
		assert(vector_at(v, 0) == 2);
		assert(vector_at(v, 1) == 3);
		assert(vector_at(v, 2) == 5);
		assert(vector_at(v, 3) == 2);
		assert(vector_at(v, 4) == 3);
		assert(vector_at(v, 5) == 5);

		unsigned sum = 0;
		vector_for(i, v) {
			sum += *i;
		}
		assert(sum == 2+3+5+2+3+5);
	}
	return 0;
}
