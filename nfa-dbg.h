#ifndef nfa_dbg_h
#define nfa_dbg_h

#include <stdio.h>
#include "nfa.h"

/**
 * Prints a graph to a stdio file for debugging.
 *
 * @param file          The output which to print the graph to.
 * @param nfa           The nondeterministic finite automaton to print.
 * @param current_state A state to mark as 'current', or @c -1 if none.
 */
void nfa_dump(FILE *file, const struct nfa *nfa, int current_state);

#endif /* nfa_dbg_h */
