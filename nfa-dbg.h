#ifndef nfa_dbg_h
#define nfa_dbg_h

#include <stdio.h>
#include "nfa.h"

/* Prints a graph to a stdio file, for debugging */
void graph_dump(FILE *file, const struct graph *g, int current_state);

#endif /* nfa_dbg_h */
