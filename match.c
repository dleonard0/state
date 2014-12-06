#include <stdlib.h>
#include "match.h"
#include "nfa.h"

/*------------------------------------------------------------
 * matcher
 */

struct matcher {
	const struct pattern *pattern;
	const struct generator *generator;
	void *generator_context;
	struct match *matches, **current;
};

struct matcher *
matcher_new(const struct pattern *pattern,
	    const struct generator *generator, void *context)
{
	struct matcher *matcher;
	struct match *m;

	/* The initial match list contains the deferred empty string */
	m = malloc(sizeof *m);
	m->next = 0;
	m->str = 0;
	m->stri = stri_str(0);
	m->flags = MATCH_DEFERRED;
	
	matcher = malloc(sizeof *matcher);
	matcher->pattern = pattern;
	matcher->generator = generator;
	matcher->generator_context = context;
	matcher->current = &matcher->matches;
	matcher->matches = m;

	return matcher;
}

str *
matcher_next(struct matcher *matcher)
{
	struct match *m, **mp;

again:
	/* Return the next fully matched string */
	while ((m = *matcher->current)) {
		if (!stri_more(m->stri)) {
			if (m->flags & MATCH_DEFERRED) {
				/* expand string at defer point */
				struct match *generated = 
					matcher->generator->generate(m->str,
						matcher->generator_context);
				struct match **tail = &generated;

				while (*tail) tail = &(*tail)->next;
				*tail = m->next;
				*matcher->current = generated;
				str_free(m->str);
				free(m);
			} else {
				/* complete match */
				str *result = m->str;
				*matcher->current = m->next;
				free(m);
				return result;
			}
		} else {
			matcher->current = &m->next;
		}
	}

	if (!stri_more(matcher->patterni)) {
		/* pattern exhausted */
		return 0;
	}

	/* Remove matches that couldn't match and increment patterni */
	// TODO pattern_step(matcher);
	TODO

	/* Prepare to walk the new list of matches */
	matcher->current = &matcher->matches;
	goto again;
}

void
matcher_free(struct matcher *matcher)
{
	struct match *m, *mnext;

	if (matcher->generator->free) {
	    matcher->generator->free(matcher->generator_context);
	}
	str_free(pattern);
	mnext = matcher->matches;
	while ((m = mnext)) {
		mnext = m->next;
		str_free(m->str);
		free(m);
	}
	free(matcher);
}
