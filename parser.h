#ifndef parse_h
#define parse_h

#include "atom.h"
#include "macro.h"
#include "str.h"

/* Values of 'defkind' in the define() callback */
#define DEFKIND_DELAYED		'='
#define DEFKIND_IMMEDIATE	':'
#define DEFKIND_WEAK		'?'
#define DEFKIND_APPEND		'+'

/* Values of 'condkind' in the condition() callback */
#define CONDKIND_IFDEF		'd'
#define CONDKIND_IFEQ   	'='

/**
 * The parser context is provided during the #parse() call
 * for parsing a Staterules file.
 */
struct parser;

/** Callback interface from parser to application */
struct parser_cb {
	/**
	 * Called by parser to obtain some raw data to parse.
	 * This is expected to behave pretty much like the read()
	 * system call.
	 * @param p   parser context
	 * @param dst destination buffer
	 * @param len maximum number of UTF-8 bytes to read
	 * @return number of bytes copied into dst, or
	 *         0 at EOF, or
	 *         -1 if there was an error
	 *
	 * Required.
	 */
        int (*read)(struct parser *p, char *dst, unsigned len);

	/**
	 * Called by parser to notify that a variable definition has been
	 * encountered.
	 * This event may happen anywhere.
	 * @param p       parser context
	 * @param lhs     the left-hand-side being defined. It should be
	 *                expanded and then trimmed
	 * @param defkind the kind of definition
	 * @param text    the text, which must be TAKEN or freed
	 *
	 * Default behaviour: macro_free(lhs); macro_free(text)
	 */
	void (*define)(struct parser *p, macro *lhs, int defkind, macro *text);

	/**
	 * Called by parser to notify that a .directive line has
	 * been encountered.
	 * This event may happen anywhere.
	 * @param p     parser context
	 * @param ident the directive (excludes the '.')
	 * @param text  the line of text which must be TAKEN or freed
	 *
	 * Default behaviour: macro_free(text)
	 */
	void (*directive)(struct parser *p, atom ident, macro *text);
	
	/**
	 * Called by the parser to evaluate an 'if' conditional.
	 * This request may be made at any time.
	 * @param p        parser context
	 * @param condkind the kind of conditional (#CONDKIND_IFDEF etc)
	 * @param t1       the text following the condition
	 * @param t2       the text following the condition
	 * @returns true if the condition is true.
	 *
	 * Default behaviour: macro_free(t1); macro_free(t2); return 0;
	 */
	int  (*condition)(struct parser *p, int condkind, macro *t1, macro *t2);

	/**
	 * Called when a rule "goal: depends" starts.
	 * It will eventually be followed by a call to #end_rule().
	 * Rules do not nest.
	 * @param p       parser context
	 * @param goal    the goal text
	 * @param depends the dependency text
	 *
	 * Default behaviour: macro_free(goal); macro_free(depends);
	 */
	void (*rule)(struct parser *p, macro *goal, macro *depends);

	/**
	 * Called when a command line is encountered.
	 * This is only ever called within a rule.
	 * @param p    parser context
	 * @param text the command line, without leading TAB character
	 *
	 * Default behaviour: macro_free(text)
	 */
	void (*command)(struct parser *p, macro *text);

	/**
	 * Called when the end of a rule has been detected.
	 * @param p    parser context
	 *
	 * Default behaviour: nothing
	 */
	void (*end_rule)(struct parser *p);

	/**
	 * Called when there is an unrecoverable parse error.
	 * @param lineno  the line number of the error
	 * @param msg     error text
	 * Default behaviour: nothing.
	 */
	void (*error)(struct parser *p, unsigned lineno, const char *msg);
};

/**
 * Parses some input to completion.
 * The parser will invoke methods on @a cb to obtain
 * text, and then to inform the cb of parse events.
 * 
 * Parsing stops after cb->read() returns 0 or a negative number.
 *
 * @param cb   callback handler for handling parse events.
 * @param read a function that is used to supply a UTF-8 stream.
 * @returns the same value as the last call to cb->read().
 */
int parse(const struct parser_cb *cb, void *context);

/**
 * @returns the context parameter that was given
 *          to the #parse() function.
 */
void *parser_get_context(const struct parser *p);

#endif /* parse_h */