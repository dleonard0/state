#ifndef graph_h
#define graph_h

#include <stdlib.h>
#include "cclass.h"

/**
 * An NFA or DFA is a graph: a set of nodes with character class transitions
 * to other nodes.
 */
struct graph {
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


/** Initializes existing graph storage */
struct graph *	graph_init(struct graph *g);
/** Releases content of an initialized graph structure */
void		graph_fini(struct graph *g);

/** Allocates a new graph structure. Release with graph_free(). */
struct graph *	graph_new(void);
/** Releases all store associated with the graph structure */
void 		graph_free(struct graph *g);

/** Adds a new, empty node to the graph. Returns its index. */
unsigned 	graph_new_node(struct graph *g);

/** Adds another final pointer to a node's final list */
void		graph_add_final(struct graph *g, unsigned n, const void *final);

/**
 * Adds an epsilon transition to the graph.
 * Caller can convert to a cclass transition by setting the cclass
 * pointer field.
 * @returns a temporary pointer to the transition
 *          (It is only valid until the next time this
 *           function is called; because realloc may adjust
 *           pointers.)
 */
struct transition *graph_new_trans(struct graph *g, unsigned from, unsigned to);

/**
 * Converts a non-deterministic graph into a deterministic one.
 * @param dfa   where to store the deterministc graph
 * @param input the (non-deterministic) input graph
 */
void graph_to_dfa(struct graph *g);

#endif /* graph_h */
