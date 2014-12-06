#include <stdlib.h>
#include "match.h"
#include "globs.h"


struct match *
match_new(str *str)
{
	struct match *match = malloc(sizeof *match);
	match->str = str;
	match->flags = 0;
	return match;
}

void
match_free(struct match *match)
{
	str_free(match->str);
	free(match);
}


/*------------------------------------------------------------
 * matcher
 */

struct matcher {
	const struct globs *globs;
	const struct generator *generator;
	void *gcontext;
	struct match *matches;
};

struct matcher *
matcher_new(const struct globs *globs,
	    const struct generator *generator,
	    void *context)
{
	struct matcher *matcher;
	struct match *m;

	/* The initial match list contains the deferred empty string */
	m = match_new(0);
	m->next = 0;
	m->stri = stri_str(0);
	m->flags = MATCH_DEFERRED;
	
	matcher = malloc(sizeof *matcher);
	matcher->globs = globs;
	matcher->generator = generator;
	matcher->gcontext = context;
	matcher->matches = m;

	return matcher;
}

static struct match **
matcher_generate(struct matcher *matcher, struct match **mp, struct match *dm)
{
	struct match *m, **tail;
	unsigned len = str_len(dm->str);
	tail = matcher->generator->generate(mp, dm->str, matcher->gcontext);

	*tail = 0;
	for (m = *mp; m; m = m->next) {
		/* Clone the deferred's state into each new match structure */
		stri i = stri_str(m->str);
		stri_inc_by(&i, len);
		m->stri = i;
		m->state = dm->state;
	}
	return tail;
}

str *
matcher_next(struct matcher *matcher, const void **ref_return)
{
	struct match *m, **mp;

	while (matcher->matches) {
		/* Process all the exhausted strings */
		mp = &matcher->matches;
		while ((m = *mp)) {
			if (stri_more(m->stri)) {
				/* Advance the match candidate's state */
				unsigned ch = stri_at(m->stri);
				if (!globs_step(matcher->globs, ch, &m->state)){
					/* Failed to advance; reject it */
					*mp = m->next;
					match_free(m);
				} else {
					/* Step one char */
					stri_inc(m->stri);
				}
			} else if (m->flags & MATCH_DEFERRED) {
				/* The string is exhausted, but it's
				 * a deferred string; so expand it now */
				struct match *head, **tail;

				tail = matcher_generate(matcher, &head, m);

				/* Remove the current match (m),
				 * replacing it with the head..tail that
				 * was just generated. We don't return m. */
				*tail = m->next;
				*mp = head;
				match_free(m);
			} else {
				/* Found a real, exhausted string. 
				 * First, remove it from the list */
				*mp = m->next;

				/* Next, test if it's a real match */
				const void *ref = globs_is_accept_state(
					matcher->globs, m->state);
				if (ref) {
					/* It's real. 
					 * Steal the match.str before freeing */
					str *result = m->str;
					m->str = 0;
					match_free(m);
					if (ref_return) {
						*ref_return = ref;
					}
					return result;
				} else {
					/* Not a match; reject */
					match_free(m);
				}
			}
		}
	}
	return 0;
}

void
matcher_free(struct matcher *matcher)
{
	struct match *m, *mnext;

	if (matcher->generator->free) {
	    matcher->generator->free(matcher->gcontext);
	}
	mnext = matcher->matches;
	while ((m = mnext)) {
		mnext = m->next;
		match_free(m);
	}
	free(matcher);
}

