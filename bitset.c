#include <stdlib.h>
#include "bitset.h"

asm(".pushsection \".debug_gdb_scripts\", \"MS\",@progbits,1\n"
    ".byte 1\n"
    ".asciz \"bitset-gdb.py\"\n"
    ".popsection\n");

bitset *
bitset_new(unsigned nbits) {
	return _bitset_init(malloc(_bitset_size(nbits)), nbits);
}

void
bitset_free(bitset *a) {
        free(a);
}


bitset *
_bitset_init_dup(bitset *dup, const bitset *a) {
	memcpy(dup, a, _bitset_size(a->nbits));
	return dup;
}

bitset *
bitset_dup(const bitset *a) {
	return _bitset_init_dup(malloc(_bitset_size(a->nbits)), a);
}

unsigned
_bitset_next(const bitset *s, unsigned i)
{
	unsigned max_el = _bitset_nelem(s->nbits);
	unsigned el = _bitset_index(i);
	_bitset_word bit = _bitset_bit(_bitset_shift(i));

	/* skip sparse sets */
	while (el < max_el && s->bits[el] < bit) {
		el++;
		bit = 1;
		i = el * (8 * sizeof (_bitset_word));
	}
	if (el >= max_el)
		return s->nbits;
	while ((s->bits[el] & bit) == 0) {
		++i;
		bit <<= 1;
	}
	return i;
}

/* Count the number of elements inside the index'th element */
static unsigned
count_el(const bitset *s, unsigned index)
{
#if __GNUC__
	return __builtin_popcount(s->bits[index]);
#else
	_bitset_word v = s->bits[index];
	unsigned count = 0;
	while (v) {
		if (v & 1) ++count;
		v >>= 1;
	}
	return count;
#endif
}

unsigned
bitset_count(const bitset *s)
{
	unsigned count, i;

	for (count = i = 0; i < _bitset_nelem(s->nbits); ++i)
		count += count_el(s, i);
	return count;
}
