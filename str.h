#ifndef str_h
#define str_h

/*
 * A 'str' is a linked list of shared text segments, designed to be
 * space and speed efficient.
 * Each segment is reference counted.
 * (Use the #STR macro to get self-releasing strings.)
 */
struct str {
        struct str_seg {
                unsigned refs;
                char data[1];
        } *seg;				/* never NULL */
        unsigned offset;                /* offset into seg->data[] */
        unsigned len;                   /* nonzero len within seg->data[] */
        struct str *next;               /* tail of the string */
};

/** A string iterator */
struct str_iter {
	const struct str *str;	/** @c NULL when after end of string */
	unsigned pos;
};

typedef struct str str;
typedef struct str_iter stri;

/**
 * A type declarator for str pointer variables that will be automatically
 * released with @str_freep() when the variable goes out of scope.
 */
#define STR  str * const __attribute__((__cleanup__(str_freep)))

/**
 * Allocates a new STR from binary data.
 * The binary data may contain NULs.
 * @param data	binary data
 * @param len   length of binary data
 * @return a newly allocated STR; must be deallocated with str_free,
 *         or @c NULL if len is zero.
 */
#define str_newn(data, len) str_newn_(data, len, __FILE__, __LINE__)
str *	  str_newn_(const char *data, unsigned len, const char *, unsigned);

/**
 * Allocates a new STR from a C string.
 * @param cstr   non-NULL pointer to a NUL-terminated C string
 * @return a newly allocated STR; must be deallocated with str_free,
 *         or @c NULL if the C string is empty (but non- @c NULL).
 */
#define str_new(cstr)       str_new_(cstr, __FILE__, __LINE__)
str *	  str_new_(const char *cstr,                const char *, unsigned);

/**
 * Deallocates a STR.
 * @param sp pointer to pointer returned by #str_new() or #str_newn().
 */
void      str_free(str * s);
void      str_freep(str * const * sp);

/**
 * Construct the concatenation of two strings.
 * This operation is storage-efficient, as STRs share their underlying
 * string data.
 * @param a first string
 * @param b second string
 * @return a newly allocated string that is the concatenation of @a a and @a b.
 */
str *     str_cat(const str *a, const str *b);

/**
 * Compare two strings.
 * @param a first string
 * @param b second string
 * @return -1, 0 or +1 if @a a sorts earlier, the same or later respectively
 *         to @a b.
 */
int       str_cmp(const str *a, const str *b);

/**
 * Compare a STR against a C string for equality.
 * @param a  a STR
 * @param cs a NUL-terminated C string
 */
int       str_eq(const str *s, const char *cs);

/**
 * Duplicate a string.
 * @param s the string to duplicate
 * @return a pointer to a STR that must be released by #str_free().
 */
str *     str_dup(const str *s);

/**
 * Duplicates a string, attaching it to the end
 * of another, returning the end reference.
 *
 * The caller should not initialize the content of
 * @c{*str_ret}; but the returned pointer will
 * be to uninitialized memory.
 *
 * @param str_ret pointer to where to attach the string
 * @param s       the string to duplicate
 * @return pointer to the (unintialized) next pointer
 *         of the last element of the copied string,
 *         or @a str_ret if @a s was empty.
 */
str **    str_xcat(str **str_ret, const str *s);

/**
 * Appends the string segment from @a begin to @a end
 * onto the end of a destination string.
 *
 * @param dst    pointer where to attach string
 * @param begin  beginning of substring to attach
 * @param end    end of substring, or {0,0} to mean all
 * @returns same as #str_xcat()
 */
str **    str_xcatr(str **dst, const stri begin, const stri end);

/**
 * Extract a copy of a section of an existing STR.
 * @param s      a STR
 * @param offset offset into @a str; may exceed the length of @a s
 * @param len    length of the substring to return
 * @return a new STR that is a copy of the @a s STR beginning at @a offset,
 *         at most @a len bytes long. If The @a offset exceeds the length
 *         of @a s then @c NULL is returned.
 */
str *     str_substr(const str *s, unsigned offset, unsigned len);

/**
 * Calculate the length of a STR.
 * This operation is not O(1).
 * @param s     a STR
 * @return the length of @a s in bytes
 */
unsigned  str_len(const str *s);

/**
 * Extract a character from a string.
 * Complexity O(n).
 *
 * @param s    a STR
 * @param pos  a position
 * @return the character at @a pos or @c NUL if @a pos is outside @a s.
 */
char      str_at(const str *s, unsigned pos);

/**
 * Extract the next token from the string.
 * The tokens of a string can be extracted by first initializing the
 * string iterator to the beginning of a string, and then calling
 * this function repeatedly until it returns @c NULL.
 *
 * @param stri  pointer to a valid string iterator
 * @param sep   C string containing delimiter characters
 * @return a new STR containing the next token, or @c NULL if there are
 *         no more. The returned STR should be released with #str_free().
 */
str *     str_tok(stri *stri, const char *sep);

/**
 * Compute a hash over the given string.
 * @return a number from 0 to UINT_MAX.
 */
unsigned  str_hash(const str *s);

/**
 * Pack a string so that it shares more segments with another.
 * @param fixed    the fixed string, which remains unaltered
 * @param packable the string to try and share more with fixed
 */
void	  str_pack(const str *fixed, str *packable);

/**
 * Copy the content of the string into a buffer.
 * @param s      the source string
 * @param dst    the destination to start copying to
 * @param offset offset to start copying from
 * @param len    maximum number of bytes to copy
 * @returns the actual number of bytes copied
 */
unsigned  str_copy(const str *s, char *dst, unsigned offset, unsigned len);

/**
 * Trims whitespace off the beginning of the string.
 * @param sp string pointer to modify
 */
void	  str_ltrim(str **sp);

/**
 * Trims whitespace off the end of the string.
 * @param sp string pointer to modify
 */
void	  str_rtrim(str **sp);

/**
 * Splits a string at the given offset.
 * @param sp string pointer to be replaced with the left side, which
 *           will not be longer than @a offset bytes
 * @param offset the length of the resulting left side of the split
 * @return right hand side of string, commencing with byte @a offset
 */
str *	  str_split_at(str **sp, unsigned offset);

/**
 * Initialize a string iterator to point to the beginning of a string.
 * Complexity O(1).
 *
 * @param s   a STR that must remain valid while the iterator has more data.
 * @return an iterator value
 */
static inline stri stri_str(const str *s) {
	stri ret = { s, 0 };
	return ret;
}

/**
 * Tests if the string iterator points to a character, and/or 
 * can be incremented.
 * Complexity O(1).
 *
 * @param i  a string iterator value
 * @return true if the string iterator can iterate over more bytes
 */
static inline int  stri_more(const stri i) {
	return !!i.str;
}

/**
 * Increments a string iterator.
 * This is a macro that takes the address of its first argument.
 * Complexity O(1).
 *
 * @param i  reference to a string iterator, for which #stri_more() returns true
 */
#define stri_inc(i)  stri_inc_by(&(i), 1)

/**
 * Tests to see if the stri can be advanced safely.
 * Complexity O(n)
 *
 * @param i   the current string iterator
 * @param inc the candidate increment
 * @return true if the iterator can iterate over at least @a inc more bytes
 */
static inline int stri_more_by(const stri i, unsigned inc) {
	const str *s = i.str;
	unsigned pos = i.pos + inc;
	while (s && pos >= s->len) {
		pos -= s->len;
		s = s->next;
	}
	return pos == 0 || s;
}

/*
 * Helper function to increment a string iterator by a given increment.
 * Complexity O(n).
 *
 * @param iptr pointer to a string iterator
 * @param inc  increment by which to advance; it must not exceed
 *             the remaining length of the string
 */
static inline void stri_inc_by(stri *iptr, unsigned inc) {
	iptr->pos += inc;
	while (iptr->pos >= iptr->str->len) {
		iptr->pos -= iptr->str->len;
		iptr->str = iptr->str->next;
		if (!iptr->pos) {
			break;
		}
	}
}

/**
 * Accesses the character referred to by an iterator.
 * Complexity O(1).
 * @param i  string iterator value, for which #stri_more() returns true
 * @return the character under the iterator, or @c NUL if the iterator 
 *         is invalid.
 */
static inline char stri_at(const stri i) {
	return i.str->seg->data[i.str->offset + i.pos];
}

/**
 * Advances the string iterator over the next UTF-8 character.
 * @param i  a string iterator to advance.
 * @return the decoded character
 */
unsigned stri_utf8_inc(stri *i);

#define IS_INVALID_UTF8(c) (((c) & ~0x7fu) == 0xdc80u)

/**
 * Decodes the next characters in a string as UTF-8.
 * @param i  a string iterator.
 * @return the decoded character
 */
static inline unsigned stri_utf8_at(stri i) { return stri_utf8_inc(&i); }

#endif /* str_h */
