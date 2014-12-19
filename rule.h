#ifndef rule_h
#define rule_h

/* A state transition rules */

struct str;
struct macro;
struct macro_list;
struct varscope;
struct reader;

/** A location in a Staterules file, used for error reporting */
struct location {
	struct str *filename;
	unsigned lineno;
};

/** A command line within a transition rule. */
struct command {
	struct command *next;
	struct location location;
	struct macro *macro;
};

/** A rule within a rules file. */
struct rule {
	struct rule *next;
	struct location location;	/* where the rule was defined */
	struct {
		struct macro *macro;
		struct str *str;	/* (left NULL after parse) */
	} goal;
	struct {
		struct macro *macro;
		struct prereq *prereq;	/* (left NULL after parse) */
	} depend;
	struct command *commands;	/* commands to run */
};

/**
 * Parses rules and definitions from a file and returns
 * a list of rules.
 * 
 * @param rp    where to store the list of rules
 * @param path  path of the file to read rules from
 * @param scope a scope to use and modify
 * @param fr    file reader
 * @param fctct context to pass fr->open()
 *
 * @returns address of last rule's next pointer
 */
struct rule ** rules_parse(struct rule **rp, const struct str *path,
			   struct varscope *scope,
			   const struct reader *fr, void *fctxt);

/**
 * Releases storage associated with a given rule
 * Take care: any globs created from the rule goals may still
 * have a reference to the rule.
 */
void rule_free(struct rule *rule);

/** Frees a list of rules, and stores a NULL pointer */
void rules_free(struct rule **rules);

#endif /* rule_h */
