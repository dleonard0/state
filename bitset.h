#ifndef bitset_h
#define bitset_h

#include <string.h>
#include <stdlib.h>

/** A compact set representation using bits */
typedef unsigned _bitset_word;
typedef struct bitset {
	unsigned nbits;		/**< number of valid bits in the array */
	_bitset_word bits[1];	/**< packed bit store; overallocated */
} bitset;

/* Round up to the next multiple of div */
static inline unsigned roundup(unsigned n, unsigned div) {
	unsigned rem = n % div;
	return rem ? n - rem + div : n;
}

static inline _bitset_word _bitset_bit(unsigned shift) {
	return (_bitset_word)1U << shift;
}

/* (Number of valid array elements in bits[] given nbits */
static inline unsigned _bitset_nelem(unsigned nbits) {
	return roundup(nbits, 8 * sizeof (_bitset_word)) /
			     (8 * sizeof (_bitset_word));
}

/* (Allocation size for a bitset structure; overallocated by one word) */
static inline size_t _bitset_size(unsigned nbits) {
	return sizeof (struct bitset)
	     + sizeof (_bitset_word) * _bitset_nelem(nbits);
}

/** Clears a bitset so that it is the empty set */
static inline void bitset_clear(bitset *a) {
	memset(a->bits, 0, _bitset_nelem(a->nbits) * sizeof (_bitset_word));
}

/* (Initializes a bitset to a size, and makes it empty) */
static inline bitset * _bitset_init(bitset *a, unsigned nbits) {
	a->nbits = nbits;
	bitset_clear(a);
	return a;
}

/** Allocates a new, empty bitset */
bitset *bitset_new(unsigned nbits);

/** Allocates a copy of a bitset */
bitset *bitset_dup(const bitset *a);

/** (Initialize dup with a copy of a) */
bitset *_bitset_init_dup(bitset *dup, const bitset *a);

/** Releases a bitset allocated with bitset_new() */
void bitset_free(bitset *a);

/** Allocate an empty bitset on the stack */
#define bitset_alloca(nbits) \
	_bitset_init(alloca(_bitset_size(nbits)), nbits)
#define bitset_alloca_copy(bs) \
	_bitset_init_dup(alloca(_bitset_size((bs)->nbits)), bs)

/** Compares two bitsets */
static inline int bitset_cmp(const bitset *a, const bitset *b) {
	return memcmp(a->bits, b->bits,
		_bitset_nelem(a->nbits) * sizeof (_bitset_word));
}

/** Assigns the content of one bitset into another */
static inline void bitset_copy(bitset *dst, const bitset *src) {
	memcpy(dst->bits, src->bits,
		_bitset_nelem(dst->nbits) * sizeof (_bitset_word));
}

#define _bitset_index(bit) ((bit) / (8 * sizeof (_bitset_word)))
#define _bitset_shift(bit) ((bit) % (8 * sizeof (_bitset_word)))

/** Inserts a number into the set, return true if newly added */
static inline int bitset_insert(bitset *s, unsigned bit) {
	const unsigned index = _bitset_index(bit);
	const unsigned shift = _bitset_shift(bit);
	int ret = !(s->bits[index] & _bitset_bit(shift));
	s->bits[index] |= _bitset_bit(shift);
	return ret;
}

/** Tests if a number is a member of the set */
static inline int bitset_contains(const bitset *s, unsigned bit) {
	const unsigned index = _bitset_index(bit);
	const unsigned shift = _bitset_shift(bit);
	return (s->bits[index] & _bitset_bit(shift)) != 0;
}

/** Removes a number from the set, if it was a member */
static inline void bitset_remove(bitset *s, unsigned bit) {
	const unsigned index = _bitset_index(bit);
	const unsigned shift = _bitset_shift(bit);
	s->bits[index] &= ~_bitset_bit(shift);
}

/** Inserts all the members of set @a s into set @a acc */
static inline void bitset_or_with(bitset *acc, const bitset *s) {
	unsigned i;
	for (i = 0; i < _bitset_nelem(s->nbits); ++i)
		acc->bits[i] |= s->bits[i];
}

/** Retains only the members in set @a acc that are also present in set @a s */
static inline void bitset_and_with(bitset *acc, const bitset *s) {
	unsigned i;
	for (i = 0; i < _bitset_nelem(s->nbits); ++i)
		acc->bits[i] &= s->bits[i];
}

/** Tests if the set is empty */
static inline int bitset_is_empty(const bitset *s) {
	unsigned i;
	for (i = 0; i < _bitset_nelem(s->nbits); ++i)
		if (s->bits[i]) return 0;
	return 1;
}

/* iterate i over bs */
#define bitset_for(i, bs) for ((i) = _bitset_next(bs, 0); \
			       (i) < (bs)->nbits; \
			       (i) = _bitset_next(bs, (i) + 1))
/* (Finds the next member in s with value >= i, or returns nbits) */
unsigned _bitset_next(const bitset *s, unsigned i);

/* Count the number of elements in the bitset */
unsigned bitset_count(const bitset *s);

#endif /* bitset_h */
