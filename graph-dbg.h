#ifndef graph_dbg_h
#define graph_dbg_h

#include <stdio.h>
#include "graph.h"

/* Prints a graph to a stdio file, for debugging */
void graph_dump(FILE *file, const struct graph *g, int current_state);

#endif /* graph_dbg_h */
