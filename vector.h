#ifndef vector_h
#define vector_h

#include <stdlib.h>
#include <string.h>

/*
 * A vector is a resizable array. It has
 *   length    The number of readable elements
 *   capacity  The number of writable elements
 *
 * Declare with:
 *   VECTOR_OF(int) vi = VECTOR_INIT;
 *   vector_append(vi, 2);
 *   vector_at(vi, 0) = 1; 
 *   vector_free(&vi);
 *
 * An auto-freeing vector:
 *   AUTO_VECTOR_OF(int, vi);
 */

#define VECTOR_OF(T) 				\
	struct { 				\
		T *elem;			\
		unsigned len, capacity;		\
	}

#define VECTOR_INIT	{ 0, 0, 0 }

#define AUTO_VECTOR_OF(T, var)	 		\
	VECTOR_OF(T) 				\
	var 					\
	__attribute__((cleanup(vector_free)))	\
	= VECTOR_INIT

/* pointer type */
#define _vector_T(v) 		typeof((v).elem[0])

/*
 * Sets the length of a vector, increasing capacity
 * to fit.
 * Returns a pointer to elem[newlen], that is one place
 * beyond the last element.
 */
static inline void *
_vector_set_len(void *vp, size_t elemsz, unsigned newlen)
{
	VECTOR_OF(char) *vect = vp;
	if (newlen > vect->capacity) {
		unsigned newcapacity = (newlen + 31) & ~31;
		vect->elem = realloc(vect->elem, elemsz * newcapacity);
		vect->capacity = newcapacity;
	}
	vect->len = newlen;
	return &vect->elem[newlen * elemsz];
}

/** Access the ith element in vector v (not bounds checked) */
#define vector_at(v, i) \
	(v).elem[i]

/** Append a value to a vector */
#define vector_append(v, value)  \
	((_vector_T(v) *)_vector_set_len(&(v), \
		sizeof *(v).elem, (v).len + 1))[-1] = (value)

/* Iterate a pointer over the elements of a vector */
#define vector_for(var, vec) \
	for ((var) = &(vec).elem[0]; (var) < &(vec).elem[(vec).len]; ++(var))

/* Copies src into dst; resizes dst to the same as src */
#define vector_copy(dst, src) do {					\
	(dst).len = 0;							\
	vector_concat(dst, src);					\
    } while (0)

/* Copies src onto end of dst */
#define vector_concat(dst, src) do {					\
	unsigned _pos = (dst).len;					\
	_vector_set_len(&(dst), sizeof *(dst).elem, 			\
					_pos + (src).len); 		\
	if ((src).len) {						\
		(dst).elem[_pos] = (src).elem[0];			\
		memcpy(&(dst).elem[_pos + 1], &(src).elem[1],		\
			((src).len - 1) * sizeof (src).elem[0]);	\
	}								\
    } while (0)

static inline void
vector_free(void *vp)
{
	VECTOR_OF(char) *vect = vp;
	free(vect->elem);
	vect->len = vect->capacity = 0;
	vect->elem = 0;
}

#define vector_qsort(vec, cmp) do {					\
        int (*_cmp)(const _vector_T(vec) *, 				\
		    const _vector_T(vec) *) = (cmp);			\
	qsort((vec).elem, (vec).len, sizeof (vec).elem[0], 		\
		(int(*)(const void *, const void *))_cmp);		\
    } while (0)

#define vector_bsearch(vec, key, cmp) 					\
	(_vector_T(vec) *)						\
	bsearch(key, (vec).elem, (vec).len, sizeof (vec).elem[0], cmp)

#endif /* vector_h */
