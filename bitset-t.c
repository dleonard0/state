#include <assert.h>
#include "bitset.h"

int main()
{
	unsigned i;
	{
		unsigned const bpw = 8 * sizeof (_bitset_word);
		assert(_bitset_nelem(0) == 0);
		assert(_bitset_nelem(1) == 1);
		assert(_bitset_nelem(bpw - 1) == 1);
		assert(_bitset_nelem(bpw) == 1);

		assert(_bitset_nelem(bpw + 1) == 2);
		assert(_bitset_nelem(bpw + bpw - 1) == 2);
		assert(_bitset_nelem(bpw + bpw) == 2);

		assert(_bitset_nelem(bpw + bpw + 1) == 3);

		assert(_bitset_bit(0) == 1);
		assert(_bitset_bit(1) == 2);
		assert(_bitset_bit(2) == 4);

		assert(_bitset_index(0) == 0);
		assert(_bitset_index(1) == 0);
		assert(_bitset_index(2) == 0);
		assert(_bitset_index(bpw - 1) == 0);
		assert(_bitset_index(bpw) == 1);
		assert(_bitset_index(bpw + 1) == 1);

		assert(_bitset_shift(0) == 0);
		assert(_bitset_shift(1) == 1);
		assert(_bitset_shift(2) == 2);
		assert(_bitset_shift(bpw - 1) == bpw - 1);
		assert(_bitset_shift(bpw) == 0);
		assert(_bitset_shift(bpw + 1) == 1);
	}
	{
		bitset *a = bitset_alloca(10);
		assert(bitset_is_empty(a));
		for (i = 0; i < 10; i++)
			assert(!bitset_contains(a, i));
		assert(bitset_count(a) == 0);
	}
	{
		bitset *a = bitset_alloca(33);
		bitset_insert(a, 7);
		bitset_insert(a, 8);
		bitset_insert(a, 15);
		bitset_insert(a, 16);
		bitset_insert(a, 31);
		bitset_insert(a, 32);

		for (i = 0; i < 33; i++)
			if (i == 7 || i == 8 || i == 15 || i == 16 ||
			    i == 31 || i == 32)
			    	assert(bitset_contains(a, i));
			else
			    	assert(!bitset_contains(a, i));

		assert(bitset_count(a) == 6);

		/* bitset_for */
		char buf[1024], *b = buf;
		static const char buf_exp[] = {7,8,15,16,31,32};
		bitset_for(i, a) {
		    *b++ = i;
		}
		assert(b == buf + 6);
		assert(memcmp(buf, buf_exp, 6) == 0);
	}
	return 0;
}
