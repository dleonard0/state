#include <stdlib.h>
#include <ctype.h>

#include "macro.h"
#include "str.h"

asm(".pushsection \".debug_gdb_scripts\", \"MS\",@progbits,1\n"
    ".byte 1\n"
    ".asciz \"macro-gdb.py\"\n"
    ".popsection\n");

int macro_count;
int macro_list_count;

static struct macro *
macro_alloc()
{
	macro_count++;
	return malloc(sizeof (struct macro));
}

static void
macro_dealloc(struct macro *m)
{
	free(m);
	macro_count--;
}


static struct macro_list *
macro_list_alloc()
{
	macro_list_count++;
	return malloc(sizeof (struct macro_list));
}

static void
macro_list_dealloc(struct macro_list *ml)
{
	free(ml);
	macro_list_count--;
}



struct macro *
macro_new_atom(const char *atom)
{
	struct macro *m = macro_alloc();
	m->next = 0;
	m->type = MACRO_ATOM;
	m->atom = atom;
	return m;
}

struct macro *
macro_new_str(struct str *str)
{
	struct macro *m = macro_alloc();
	m->next = 0;
	m->type = MACRO_STR;
	m->str = str;
	return m;
}

struct macro *
macro_new_reference(void)
{
	struct macro *m = macro_alloc();
	m->next = 0;
	m->type = MACRO_REFERENCE;
	m->reference = 0;
	return m;
}

struct macro **
macro_cons(struct macro **mp, struct macro *m)
{
	*mp = m;
	while (*mp)
		mp = &(*mp)->next;
	return mp;
}

void
macro_free(struct macro *next)
{
	struct macro *m;
	while ((m = next)) {
		next = m->next;
		switch (m->type) {
		case MACRO_ATOM:
			break;
		case MACRO_STR:
			str_free(m->str);
			break;
		case MACRO_REFERENCE:
			macro_list_free(m->reference);
			break;
		}
		macro_dealloc(m);
	}
}

struct macro_list **
macro_list_cons(struct macro_list **lp, struct macro *macro)
{
	struct macro_list *ml = macro_list_alloc();
	ml->next = 0;
	ml->macro = macro;
	*lp = ml;
	return &ml->next;
}

void
macro_list_free(struct macro_list *next)
{
	struct macro_list *ml;

	while ((ml = next)) {
		next = ml->next;
		macro_free(ml->macro);
		macro_list_dealloc(ml);
	}
}

void
macro_ltrim(macro **mp)
{
        macro *m;

	while ((m = *mp)) {
            if (m->type != MACRO_STR)
	        break;
	    str_ltrim(&m->str);
	    if (m->str)
		break;
	    *mp = m->next;
            m->next = 0;
	    macro_free(m);
        }
}

void
macro_rtrim(macro **mp)
{
        macro *m = *mp;

        if (!m)
		return;
        macro_rtrim(&m->next);
	if (m->next)
		return;
        if (m->type != MACRO_STR)
		return;
        str_rtrim(&m->str);
	if (m->str)
		return;
        macro_free(m);
        *mp = 0;
}

/*
 * roughly splits a macro at the first non-atomic whitespace;
 * returns the right side, which will start with the found whitespace
 */
static macro *
macro_rough_split(macro **mp)
{
	int pos;
	macro *m, *m2;
	str **sp, *s, *s2;

	/* Hunt for a non-atomic string macro section */
	while (*mp) {
	    if ((*mp)->type == MACRO_STR) {
		/* Hunt for a whitespace character */
		sp = &(*mp)->str;
		while (*sp) {
		    s = *sp;
		    for (pos = 0; pos < s->len; pos++) {
			if (isspace(s->seg->data[s->offset + pos])) {
			    if (pos == 0) {
			        /* Found space at beginning of string,
				 * which makes it easy to split the macro */
				m2 = *mp;
				*mp = 0;
			    } else {
				/*
				 * Split the string at the whitespace
				 * then graft the rest of the macro chain
				 * onto a new macro section.
				 */
				s2 = str_split_at(sp, pos);
				m = *mp;
				m2 = macro_new_str(s2);
				m2->next = m->next;
				m->next = 0;
			    }
			    return m2;
			}
		    }
		    sp = &(*sp)->next;
		}
	    }
	    mp = &(*mp)->next;
	}
	return 0;
}

struct macro_list *
macro_split(macro *m)
{
	struct macro_list *result = 0;
	struct macro_list **acc = &result;

	macro_ltrim(&m);
	macro_rtrim(&m);
	while (m) {
	    macro *right = macro_rough_split(&m);
	    acc = macro_list_cons(acc, m);
	    m = right;
	    macro_ltrim(&m);
	}
	return result;
}
