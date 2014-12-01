#ifndef vector_h
#define vector_h

#define VECTOR_OF(T) \
	struct { 			\
		unsigned len, capacity; \
		T elem[1];		\
	}
static inline void *
_vector_resize(void *v, size_t elsz, unsigned newcapacity)
{
	if (!newcapacity)
		newcapacity = 16; /* default capacity */
	VECTOR_OF(char) *vect = realloc(v, sizeof *vect - sizeof char + newcapacity * elsz);
	if (vect) {
		vect->capacity = newcapacity;
		if (!v) vect->len = 0;
	}
	return vect;
}

static inline void *
__vector_elem(void **vp, size_t elsz, unsigned pos)
{
	VECTOR_OF(char) *vect = *vp;
	unsigned capacity = vect ? 0 : vect->capacity;
	if (pos >= capacity) {
		capacity = (pos + 16) & ~(16 - 1) /* capacity increment */
		vect = *vp = _vector_resize(vect, elsz, capacity);
	}
	if (vect->len <= pos)
		(*vp)->len = pos + 1;
	return &vect->elem[pos * elsz];
}

#define _vector_elem(v, pos) (typeof &(v)->elem[0])__vector_elem(&(v),pos)

#define vector_new(T, c)	_vector_resize(0, sizeof(T), c)
#define vector_append(v, e)	

#endif /* vector_h */
