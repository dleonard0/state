#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "cclass.h"

#define CCINC 8

asm(".pushsection \".debug_gdb_scripts\", \"MS\",@progbits,1\n"
    ".byte 1\n"
    ".asciz \"cclass-gdb.py\"\n"
    ".popsection\n");

cclass *
cclass_new()
{
	cclass *cc = malloc(sizeof *cc);
	cc->nintervals = 0;
	cc->interval = 0;
	return cc;
}

void
cclass_free(cclass *cc)
{
	if (cc) {
		free(cc->interval);
		free(cc);
	}
}

cclass *
cclass_dup(const cclass *cc)
{
	cclass *dup = cclass_new();

	if (cc->nintervals) {
		dup->nintervals = cc->nintervals;
		dup->interval = malloc((CCINC + cc->nintervals) *
			sizeof cc->interval[0]);
		memcpy(dup->interval, cc->interval,
			sizeof dup->interval[0] * dup->nintervals);
	}
	return dup;
}

int
cclass_is_single(const cclass *cc)
{
	return cc->nintervals == 1 &&
	       cc->interval->lo + 1 == cc->interval->hi;
}

int
cclass_is_empty(const cclass *cc)
{
	return cc->nintervals == 0;
}

/* Inserts interval [lo,hi) into the cclass at position i, unchecked. */
static void
cclass_insert_before(cclass *cc, unsigned i, unsigned lo, unsigned hi)
{
	unsigned n = cc->nintervals;
	if (n % CCINC == 0) {
		cc->interval = realloc(cc->interval,
			(n + CCINC) * sizeof cc->interval[0]);
	}
	memmove(cc->interval + i + 1, cc->interval + i,
		(n - i) * sizeof *cc->interval);
	cc->interval[i].lo = lo;
	cc->interval[i].hi = hi;
	cc->nintervals++;
}

/* Removes the ith interval from cclass, unchecked */
static void
cclass_remove(cclass *cc, unsigned i)
{
	unsigned n = cc->nintervals;
	memmove(cc->interval + i, cc ->interval + i + 1,
		(n - i - 1) * sizeof *cc->interval);
	cc->nintervals--;
}

void
cclass_add(cclass *cc, unsigned lo, unsigned hi)
{
	unsigned i;

	if (lo >= hi) 
		return;

	/* skip all the intervals distinctly less than the new, 
	 * [i.lo,i.hi) << [lo,hi) */
	i = 0;
	while (i < cc->nintervals && cc->interval[i].hi < lo)
		++i;
	/* If at end, or the current interval is dictinctly 
	 * greater than the new, [lo,hi) << [i.lo,i.hi), then we
	 * immediately insert */
	if (i == cc->nintervals || hi < cc->interval[i].lo) {
		cclass_insert_before(cc, i, lo, hi);
		return;
	}
	/* We know now that lo <= i.hi and hi >= i.lo */
	/* We're going to replace this interval, so find its new lo bound */
	if (cc->interval[i].lo < lo) {
		lo = cc->interval[i].lo;
	}
	if (cc->interval[i].hi > hi) {
		hi = cc->interval[i].hi;
	}
	/* Look forward to see if our hi bound will consume the following 
	 * interval */
	while (i + 1 < cc->nintervals && cc->interval[i + 1].lo <= hi) {
		if (cc->interval[i + 1].hi > hi)
			hi = cc->interval[i + 1].hi;
		cclass_remove(cc, i + 1);
	}

	cc->interval[i].lo = lo;
	cc->interval[i].hi = hi;
}

int
cclass_contains(const cclass *cc, unsigned lo, unsigned hi)
{
	unsigned i;
	for (i = 0; i < cc->nintervals; ++i) {
		if (lo >= cc->interval[i].lo && hi <= cc->interval[i].hi)
			return 1;
		if (lo < cc->interval[i].lo)
			break;
	}
	return 0;
}

int
cclass_contains_ch(const cclass *cc, unsigned ch)
{
	unsigned i;
	for (i = 0; i < cc->nintervals; ++i) {
		if (ch >= cc->interval[i].lo && ch < cc->interval[i].hi)
			return 1;
		if (ch < cc->interval[i].lo)
			break;
	}
	return 0;
}

int
cclass_eq(const cclass *c1, const cclass *c2)
{
	unsigned i;
	if (c1->nintervals != c2->nintervals)
		return 0;
	for (i = 0; i < c1->nintervals; i++)
		if (c1->interval[i].lo != c2->interval[i].lo ||
		    c1->interval[i].hi != c2->interval[i].hi)
		    	return 0;
	return 1;
}

cclass *
cclass_split(cclass *cc, unsigned p)
{
	cclass *upper;
	unsigned i;

	/* Find the interval that contains p */
	for (i = 0; i < cc->nintervals; ++i)
		if (p < cc->interval[i].hi)
			break;
	assert(i < cc->nintervals);
	assert(p >= cc->interval[i].lo);
	assert(p > cc->interval[0].lo);

	upper = cclass_new();
	upper->interval = malloc((CCINC + cc->nintervals - i)
		* sizeof upper->interval[0]);
	upper->nintervals = cc->nintervals - i;
	memcpy(upper->interval, cc->interval + i,
		upper->nintervals * sizeof upper->interval[0]);
	if (cc->interval[i].lo == p) {
		cc->nintervals = i;
	} else {
		upper->interval[0].lo = p;
		cc->interval[i].hi = p;
		cc->nintervals = i + 1;
	}
	return upper;
}

cclass *
cclass_invert(cclass *cc)
{
	unsigned i, j, lasthi, lo, hi;

	lasthi = 0;
	for (i = j = 0; i < cc->nintervals; i++) {
		lo = cc->interval[i].lo;
		hi = cc->interval[i].hi;
		cc->interval[j].lo = lasthi;
		cc->interval[j].hi = lo;
		if (lasthi != lo) {
			/* Avoid leading with [0,0) */
			j++;	/* j <= i */
		}
		lasthi = hi;
	}
	if (lasthi != MAXCHAR) {
		/* Not ending with [MAX,MAX) */
		if (j < cc->nintervals) {
			cc->interval[j].lo = lasthi;
			cc->interval[j].hi = MAXCHAR;
		} else {
			cclass_insert_before(cc, cc->nintervals, lasthi, MAXCHAR);
		}
		j++;
	}
	cc->nintervals = j;
	return cc;
}

void
cclass_addcc(cclass *c1, const cclass *c2)
{
	unsigned i;

	for (i = 0; i < c2->nintervals; ++i)
		cclass_add(c1, c2->interval[i].lo, c2->interval[i].hi);
}

int
cclass_contains_cc(const cclass *big, const cclass *small)
{
	unsigned si, bi = 0;

	for (si = 0; si < small->nintervals; ++si) {
		unsigned lo = small->interval[si].lo;
		unsigned hi = small->interval[si].hi;

		while (bi < big->nintervals && big->interval[bi].hi <= lo)
			++bi;
		if (bi == big->nintervals)
			return 0;
		/* lo < bi->interval[bi].hi */
		if (lo < big->interval[bi].lo)
			return 0;
		if (hi > big->interval[bi].hi)
			return 0;
	}
	return 1;
}

int
cclass_intersects(const cclass *cc1, const cclass *cc2)
{
	unsigned i1 = 0, i2 = 0;
	while (i1 < cc1->nintervals && i2 < cc2->nintervals) {
		if (cc1->interval[i1].hi <= cc2->interval[i2].lo) {
			i1++;
		} else if (cc2->interval[i2].hi <= cc1->interval[i1].lo) {
			i2++;
		} else {
			return 1;
		}
	}
	return 0;
}
