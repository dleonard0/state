#include <stdio.h>

#include "nfa-dbg.h"

#define EPSILON_STR "\xce\xb5"	/* epsilon character in UTF-8 */

/* prints a character from a cclass (only used in debugging) */
static void
putc_escaped(unsigned ch, FILE *file) {
	if (ch == '\\' || ch == '-' || ch == ']')
		putc('\\', file);
	if (ch < ' ')
		fprintf(file, "\\x%02x", ch);
	else if (ch < 0x7f)
		putc(ch & 0x7f, file);
	else if (ch < 0x10000)
		fprintf(file, "\\u%04x", ch);
	else 
		fprintf(file, "\\u+%06x", ch);
}

/*
 * Dumps a graph structure to a stdio file,
 * intended for debugging/inspection.
 */
void
nfa_dump(FILE *file, const struct nfa *nfa, int current_state)
{
	unsigned i, j, k;
	for (i = 0; i < nfa->nnodes; ++i) {
		const struct node *n = &nfa->nodes[i];
		fprintf(file, "%c%4u: %c ",
		    current_state >= 0 && current_state == i ? '*' : ' ',
		    i, n->nfinals ? 'F' : ' ');
		for (j = 0; j < n->ntrans; ++j) {
			const struct transition *t = &n->trans[j];
			const cclass *cc = t->cclass;
			if (!cc)
				fprintf(file, EPSILON_STR);
			else {
				if (cc->nintervals == 1 && 
				    cc->interval[0].lo + 1 == 
				    		cc->interval[0].hi)
				{
					unsigned ch = cc->interval[0].lo;
					if (ch == '.' || ch == '|' ||
					    ch == '(' || ch == ')' ||
					    ch == '*' || ch == '?' ||
					    ch == '[') 
					{
						putc('\\', file);
					}
					putc_escaped(ch, file);
				} else {
					putc('[', file);
					for (k = 0; k < cc->nintervals; ++k) {
						unsigned lo, hi;
						lo = cc->interval[k].lo;
						hi = cc->interval[k].hi;
						putc_escaped(lo, file);
						if (lo + 1 < hi) {
							putc('-', file);
							putc_escaped(hi - 1, file);
						}
					}
					putc(']', file);
				}
			}
			fprintf(file, "->%u ", t->dest);
		}
		if (n->nfinals) {
			fprintf(file, "\t\tF={");
			for (j = 0; j < n->nfinals; ++j) {
				if (j) putc(' ', file);
				fprintf(file, "\"%s\"", (const char *)n->finals[j]);
			}
			putc('}', file);
		}
		putc('\n', file);
	}
}
