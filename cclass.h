#ifndef cclass_h
#define cclass_h

#define MAXCHAR 0x110000

/**
 * A character class is a sorted set of character ranges. For example,
 *   {[a,d),[g,k)} === {a,b,c, g,h,i,j}
 * The NULL cclass is called ε (epsilon).
 * The empty cclass matches no characters.
 */
typedef struct cclass {
	unsigned nintervals;
	struct {
		unsigned lo;
		unsigned hi;
	} *interval;
} cclass;

cclass * cclass_new(void);
void 	 cclass_free(cclass *cc);
cclass * cclass_dup(const cclass *cc);
int 	 cclass_is_single(const cclass *cc);
int 	 cclass_is_empty(const cclass *cc);

/** Adds interval [lo,hi) into the cclass. */
void 	 cclass_add(cclass *cc, unsigned lo, unsigned hi);

/** Merges cc2 into cc1. */
void 	 cclass_addcc(cclass *cc1, const cclass *cc2);

/** Tests if [lo,hi) is continuous within cc */
int 	 cclass_contains(const cclass *cc, unsigned lo, unsigned hi);

/** Tests if ch is an element of cc */
int 	 cclass_contains_ch(const cclass *cc, unsigned ch);

/** Tests if cc2 is a subset of the element of cc1 */
int 	 cclass_contains_cc(const cclass *cc1, const cclass *cc2);

/** Tests if any element of cc1 is an element of cc2 */
int 	 cclass_intersects(const cclass *cc1, const cclass *cc2);

/* Returns true if two cclasses are equal */
int 	 cclass_eq(const cclass *c1, const cclass *c2);

/*
 * Splits the cclass.
 * @param cc  the cclass to truncate. It must not have @a p as its
 *            smallest member. After splitting @a cc will have been
 *            truncated to no longer contain @a p
 * @param p   the character to split on. It must be a member of @a cc
 * @returns   the upper part of the original @a cc, having @a p as its
 *            smallest member.
 */
cclass *cclass_split(cclass *cc, unsigned p);

/*
 * Inverts a cclass, in-place.
 * The inverse of [] is [0,MAXCHAR).
 * @param cc the cclass to invert
 * @returns @a cc
 */
cclass *cclass_invert(cclass *cc);

#endif /* cclass_h */
