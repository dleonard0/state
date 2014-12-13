#ifndef prereq_h
#define prereq_h

struct str;

/*
 * A prerequisite expression tree.
 */
struct prereq {
	enum prereq_type {
		PR_STATE,		/* dir@ident */
		PR_ANY,			/* { P ... } */
		PR_ALL,			/* ( P ... ) */
		PR_NOT,			/* ! P */
	} type;
	struct prereq *next;		/* sibling list */
	union {
		struct str *state;
		struct prereq *any;
		struct prereq *all;
		struct prereq *not;
	};
};

/**
 * Create a prereq expression tree from a string representation
 * @param str           the string to parse into a prerequisite tree
 * @param error_return  where to store an error string, when 
 *                      @c NULL is returned
 * @returns a prereq tree, or @c NULL on error
 */
struct prereq *	prereq_make(const struct str *str, 
			    const char **error_return);

/**
 * Releases storage associated with the prereq
 */
void 		prereq_free(struct prereq *p);

#endif /* prereq_h */
