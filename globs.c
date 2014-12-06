#include <stdlib.h>
#include "globs.h"
#include "str.h"
#include "nfa.h"
#include "cclass.h"

struct globs {
	struct nfa dfa;
};

/*------------------------------------------------------------
 * glob parser
 */

/*
 * An intermediate sub-automaton generated during Thompson's
 * construction method.
 *
 * Implementation note: never return a subnfa with a *backwards*
 * epislon transition from exit to entry.
 * Although, a forward epsilon transition from entry to exit is OK.
 * This means that any subnfa returned can safely have a forward-
 * or backwards epsilon transition placed on it
 */
struct subnfa {
        unsigned entry, exit;
	const char *error;
};

#define IS_ERROR_SUBNFA(sub) ((sub).error)

/** Returns a pure-error subnfa. */
static struct subnfa
subnfa_error(const char *error)
{
	struct subnfa sub;
	sub.entry = 0;
	sub.exit = 0;
	sub.error = error;
	return sub;
}

/*
 * Builds an empty subnfa frame, with
 * two newly allocated states, entry and exit
 * but no transitions.  It is  ready for
 * other functions to construct some
 * matching automaton within it.
 *
 *    ┌──────┐
 *    │frame │
 *    ○      ●
 *    └──────┘
 */
static struct subnfa
subnfa_frame(struct nfa *nfa)
{
	struct subnfa sub;
	sub.entry = nfa_new_node(nfa);
	sub.exit = nfa_new_node(nfa);
	sub.error = 0;
	return sub;
}

/*
 * Builds a subnfa with the inner subnfa simply linked.
 * This is a helper function for constructing more 
 * complicated subnfas.
 *
 *    ┌───────────────┐
 *    │box            │
 *    │   ┌───────┐   │
 *    ○─ε→○ inner ●─ε→●
 *    │   └───────┘   │
 *    └───────────────┘
 */
static struct subnfa
subnfa_box(struct nfa *nfa, struct subnfa inner)
{
	struct subnfa box = subnfa_frame(nfa);
	nfa_new_trans(nfa, box.entry, inner.entry);
	nfa_new_trans(nfa, inner.exit, box.exit);
	return box;
}

/* Forward declaration for the globs parser */
static struct subnfa parse_sequence(struct nfa *nfa, stri *i);

/*
 * Parse one of: ?(.|..) *(.|..) +(.|..) @(.|..) !(.|..)
 *
 * First the alt ::= (seq|..|seq) is parsed:
 *    ┌──────────────┐
 *    │alt           │
 *    │   ┌─────┐    │
 *    │┌ε→○ seq ●─ε┐ │
 *    ││  └─────┘  │ │
 *    ○┼     :     ┼→●
 *    ││  ┌─────┐  │ │
 *    │└ε→○ seq ●─ε┘ │
 *    │   └─────┘    │
 *    └──────────────┘
 * Then the alt is wrapped in + * ? (@ means no change)
 *    ┌─────────────┐  ┌─────────────┐ 
 *    │+            │  │*            │  ┌─────────────┐
 *    │  ┌───ε───┐  │  │  ┌───ε───┐  │  │?            │
 *    │  ↓┌─────┐│  │  │  ↓┌─────┐│  │  │   ┌─────┐   │
 *    ○─ε→○ alt ●┴ε→●  ○┬ε→○ alt ●┴ε→●  ○┬ε→○ alt ●─ε→●
 *    │   └─────┘   │  ││  └─────┘  ↑│  ││  └─────┘  ↑│
 *    └─────────────┘  │└───────────┘│  │└───────────┘│
 *                     └─────────────┘  └─────────────┘
 */
static struct subnfa
parse_group(struct nfa *nfa, stri *i, unsigned kind)
{
	if (kind == '!') {
		return subnfa_error("!(...) is not supported");
	}
	stri_inc(*i); /* '(' */

	struct subnfa alt = subnfa_frame(nfa);
	int first = 1;
	while (stri_more(*i) && stri_at(*i) != ')') {
		if (!first && stri_at(*i) == '|') {
			stri_inc(*i); /* '|' */
			if (!stri_more(*i)) break;
		}
		first = 0;
		struct subnfa seq = parse_sequence(nfa, i);
		if (IS_ERROR_SUBNFA(seq)) {
			return seq;
		}
		nfa_new_trans(nfa, alt.entry, seq.entry);
		nfa_new_trans(nfa, seq.exit, alt.exit);
	}
	if (!stri_more(*i)) {
		return subnfa_error("unclosed (");
	}
	stri_inc(*i); /* ')' */

	if (nfa->nodes[alt.entry].ntrans == 0) {
		/* empty alt, () */
		nfa_new_trans(nfa, alt.entry, alt.exit);
	}

	struct subnfa ret = subnfa_box(nfa, alt);
	switch (kind) {
	case '?':
		nfa_new_trans(nfa, ret.entry, ret.exit);
		break;
	case '*':
		nfa_new_trans(nfa, ret.entry, ret.exit);
		nfa_new_trans(nfa, alt.exit, alt.entry);
		break;
	case '+':
		nfa_new_trans(nfa, alt.exit, alt.entry);
		break;
	case '@':
		break;
	}
	return ret;
}

/* Parse a [...] character class; after the '[' has been consumed. */
static struct subnfa
parse_cclass(struct nfa *nfa, stri *i)
{
	struct subnfa sub = subnfa_frame(nfa);
	cclass *cc = cclass_new();
	int invert = 0;
	struct transition *trans;
	trans = nfa_new_trans(nfa, sub.entry, sub.exit);
	trans->cclass = cc;

	if (stri_more(*i) && 
	    (stri_at(*i) == '!' || stri_at(*i) == '^'))
	{
		invert = 1;
		stri_inc(*i);
	}
	if (stri_more(*i) && stri_at(*i) == ']') {
		cclass_add(cc, ']', ']' + 1);
		stri_inc(*i);
	}
	for (;;) {
		unsigned lo, hi;
		if (!stri_more(*i)) {
			return subnfa_error("unclosed [");
		}
		lo = stri_utf8_inc(i);
		if (lo == ']')
			break;
		if (stri_more(*i) && lo == '\\')
			lo = stri_utf8_inc(i);
		if (stri_more(*i) && stri_at(*i) == '-') {
			stri_inc(*i);
			if (!stri_more(*i)) {
				return subnfa_error("unclosed [");
			}
			hi = stri_utf8_inc(i);
			if (stri_more(*i) && hi == '\\')
				hi = stri_utf8_inc(i);
		} else {
			hi = lo;
		}
		if (hi < lo) {
			return subnfa_error("bad character class");
		}
		if (lo == '/' || hi == '/') {
			return subnfa_error(
				"cannot have / in character class");
		}
		if (lo < '/' && '/' < hi) {
			/* remove / from implied range */
			cclass_add(cc, lo, '/');
			cclass_add(cc, '/' + 1, hi + 1);
		} else {
			cclass_add(cc, lo, hi + 1);
		}
	}
	if (invert) {
		/* add / now so that it is removed during inversion */
		cclass_add(cc, '/', '/' + 1);
		trans->cclass = cclass_invert(cc);
		cclass_free(cc);
		cc = trans->cclass;
	}
	return sub;
}

/* Create the cclass corresponding to the globs "?" */
static cclass *
question_cclass()
{
	cclass *cc = cclass_new();
	cclass_add(cc, 1, '/');
	cclass_add(cc, '/' + 1, MAXCHAR);
	return cc;
}

/* Parses a non-sequence globs expression,
 *   ?(...) *(...) +(...) @(...) !(...)
 *   [...] * ? \c c
 */
static struct subnfa
parse_atom(struct nfa *nfa, stri *i)
{
	/* assert(stri_more(*i)); */
	struct subnfa sub;
	unsigned ch = stri_utf8_inc(i);
	cclass *cc;

	if (stri_more(*i) && stri_at(*i) == '(' &&
		(ch == '?' || ch == '*' || ch == '+' || ch == '@' || ch == '!'))
	{
		return parse_group(nfa, i, ch);
	}
	if (stri_more(*i) && ch == '[') {
		return parse_cclass(nfa, i);
	}
	if (ch == '*') {
		sub = subnfa_frame(nfa);
		struct subnfa q = subnfa_frame(nfa); /* The "?" subnfa */
		cc = question_cclass();
		nfa_new_trans(nfa, sub.entry, q.entry);
		nfa_new_trans(nfa, q.entry, q.exit)->cclass = cc;
		nfa_new_trans(nfa, q.exit, sub.exit);
		nfa_new_trans(nfa, q.entry, q.exit);
		nfa_new_trans(nfa, q.exit, q.entry);
		return sub;
	}

	if (ch == '?') {
		cc = question_cclass();
	} else {
		cc = cclass_new();
		if (stri_more(*i) && ch == '\\') {
			ch = stri_utf8_inc(i);
		}
		cclass_add(cc, ch, ch + 1);
	}
	sub = subnfa_frame(nfa);
	nfa_new_trans(nfa, sub.entry, sub.exit)->cclass = cc;
	return sub;
}

/*
 * Parses a sequence of globs atoms, finishing when
 * we hit a '|' or ')'.
 *    ┌──────────────────────────────────────────┐
 *    │seq                                       │
 *    │   ┌──────┐   ┌──────┐         ┌─────┐    │
 *    ○─ε→○ atom ●─ε→○ atom ●─ // ─ ε→○ atom ●─ε→●
 *    │   └──────┘   └──────┘         └─────┘    │
 *    └──────────────────────────────────────────┘
 */
static struct subnfa
parse_sequence(struct nfa *nfa, stri *i)
{
	struct subnfa seq = subnfa_frame(nfa);
	unsigned last = seq.entry;

	while (stri_more(*i)) {
		char ch = stri_at(*i);
		if (ch == '|' || ch == ')') {
			break;
		}

		struct subnfa atom = parse_atom(nfa, i);
		if (IS_ERROR_SUBNFA(atom)) {
			return atom;
		}
		nfa_new_trans(nfa, last, atom.entry);
		last = atom.exit;
	}
	nfa_new_trans(nfa, last, seq.exit);
	return seq;
}

struct globs *
globs_new()
{
	struct globs *globs = malloc(sizeof *globs);

	nfa_init(&globs->dfa);
	return globs;
}

void
globs_free(struct globs *globs)
{
	nfa_fini(&globs->dfa);
	free(globs);
}

const char *
globs_add(struct globs *globs, const str *globstr, const void *ref)
{
	struct nfa *nfa = &globs->dfa;
	stri ip = stri_str(globstr);
	struct subnfa outer = subnfa_frame(nfa);
	struct subnfa seq = parse_sequence(nfa, &ip);
	if (IS_ERROR_SUBNFA(seq)) {
		return seq.error;
	}
	nfa_new_trans(nfa, outer.entry, seq.entry);
	nfa_new_trans(nfa, seq.exit, outer.exit);
	nfa_add_final(nfa, outer.exit, ref);
	return NULL;
}

void
globs_compile(struct globs *globs)
{
	nfa_to_dfa(&globs->dfa);
}

int
globs_step(const struct globs *globs, unsigned ch, unsigned *statep)
{
        const struct node *node = &globs->dfa.nodes[*statep];
        unsigned j;

        /* TODO replace this with a binary search, because the
         * transition array of a dfa node will be ordered by 
         * their (non-overlapping) cclass fields */
        for (j = 0; j < node->ntrans; ++j) {
                const struct transition *trans = &node->trans[j];
                if (cclass_contains_ch(trans->cclass, ch)) {
                        *statep = trans->dest;
                        return 1;
                }
        }
        return 0;
}

const void *
globs_is_accept_state(const struct globs *globs, unsigned state)
{
        const struct node *node = &globs->dfa.nodes[state];

	if (!node->nfinals)
		return NULL;
	return node->finals[0];
}
