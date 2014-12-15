#ifndef rule_h
#define rule_h

/* State transition rules */

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

/* A command line within a transition rule */
struct command {
	struct command *next;
	struct location location;
	struct macro *macro;
};

/* A rule within a rules file */
struct rule {
	struct rule *next;
	struct location location;	/* where the rule was defined */
	struct {
		struct macro *macro;
		struct str *str;
	} goal;
	struct {
		struct macro *macro;
		struct prereq *prereq;
	} depend;
	struct command *commands;	/* commands to run */
};

/**
 * Parses rules from a file and inserts them into the given list.
 * @param rp    where to store the list of rules
 * @param path  path to file to read rules from
 * @param scope a scope to modify
 * @param fr    file reader
 * @param fctct context to pass fr->open()
 * @returns address of last rule's next pointer
 */
struct rule ** rules_parse(struct rule **rp, const struct str *path,
			   struct varscope *scope,
			   const struct reader *fr, void *fctxt);

#endif /* rule_h */
