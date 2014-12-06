#ifndef nfa_h
#define nfa_h

#include <stdlib.h>
#include "cclass.h"

/**
 * A graph structure intended for use as an NFA or DFA.
 * Each graph is a set of nodes (possible states) each with
 * a set of character class transitions to other nodes.
 * DFAs are guaranteed to have the properties of:
 *   - no epsilon transitions (transition.cclass != NULL)
 *   - unique transitions for any character
 *     (the transition.cclass do not overlap).
 *
 * A weaker guarantee is that the 'finals' set for a node
 * has at most one member. Or, at least, its first member
 * is the most important.
 */
struct nfa {
	unsigned nnodes;
	struct node {
		unsigned nfinals;
		const void **finals;
		unsigned ntrans;
		struct transition {
			cclass *cclass;
			unsigned dest;
		} *trans;
	} *nodes;
};


/** Initializes existing graph storage. Release with #nfa_fini() */
struct nfa *	nfa_init(struct nfa *nfa);

/** Releases content of an initialized graph structure */
void		nfa_fini(struct nfa *nfa);

/** Allocates a new graph structure. Release with #nfa_free(). */
struct nfa *	nfa_new(void);

/** Releases all store associated with the graph structure */
void		nfa_free(struct nfa *nfa);

/** Adds a new, empty node to the graph.
 * @returns the new node's index into @c{g->nodes[]}. */
unsigned	nfa_new_node(struct nfa *nfa);

/**
 * Appends a pointer value to a node's #node.finals array.
 * Final values are handy for automaton users that want
 * to associate some states with differnt kinds of
 * accept status.
 *
 * @param g     the graph containing the node
 * @param n     the node's index
 * @param final the final value to add to the node.
 */
void		nfa_add_final(struct nfa *nfa, unsigned n, const void *final);

/**
 * Adds an epsilon transition to the graph.
 * An epsilon transition in a NFA means that the machine may
 * take the transition at any time.
 * Normaly though, the caller can immediately convert the
 * transition into a non-epsilon by setting the #transition.cclass
 * field to a non-null pointer.
 *
 * @param g    the graph into which to add the transition
 * @param from the node index to transition from
 * @param to   the node index to transition to
 *
 * @returns a temporary pointer to the transition
 * (It's only valid until the call to this function, because
 * realloc may adjust pointers.)
 */
struct transition *nfa_new_trans(struct nfa *nfa, unsigned from, unsigned to);

/**
 * Converts a non-deterministic graph into a deterministic one.
 * @param dfa   where to store the deterministc graph
 * @param input the (non-deterministic) input graph
 */
void nfa_to_dfa(struct nfa *nfa);

#endif /* nfa_h */
