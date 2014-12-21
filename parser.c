#include <ctype.h>
#include <string.h>

#include "parser.h"

#define EOF (-1)

#define CONDKIND_NEGATE 0x80		/* or'd with other CONDKIND_* flags */
#define MAX_LOOKAHEAD 1024		/* probably overkill */

struct parser {
	const struct parser_cb *cb;	/**< caller's callback */
	void *context;			/**< caller's callback context */
	unsigned lineno;		/**< line number of #next() char */
	unsigned utf8col;		/**< column number of #next() char */
	char lookahead_buf[MAX_LOOKAHEAD];
	char *lookahead;		/**< next pointer into lookahead */
	char *lookahead_end;		/**< end of valid in lookahead_buf[] */
	int last_read;			/**< return value from last cb.read */
	unsigned in_rule;		/**< true if in a rule block */
	unsigned if_endepth;		/**< depth of true ifs */
	unsigned if_disabled;		/**< depth of false ifs within trues */
};


/**
 * Initializes a parser structure for a new file.
 *
 * @param p        the parser structure to initialize
 * @param cb       the callback structure
 * @param context  the callback's context pointer
 */
static void
parser_init(struct parser *p, const struct parser_cb *cb, void *context)
{
	p->cb = cb;
	p->context = context;
	p->lineno = 1;
	p->utf8col = 1;
	p->lookahead = p->lookahead_buf;
	p->lookahead_end = p->lookahead_buf;
	p->last_read = 1;
	p->in_rule = 0;
	p->if_endepth = 0;
	p->if_disabled = 0;
}

void *
parser_get_context(const struct parser *p)
{
	return p->context;
}

/*------------------------------------------------------------
 * lookahead logic
 */

/**
 * Attempts to ensure there are n bytes of data in the lookahead buffer.
 * Reads bytes from the cb iterface if there is insufficient data.
 *
 * @param p   the parser context holding the la buffer and cb pointer
 * @param n   the number of bytes desired to be valid at p->lookahead
 *
 * @returns 0 on failure.
 */
static int
lookahead(struct parser *p, unsigned n)
{
	unsigned avail;

	for (;;) {
		avail = p->lookahead_end - p->lookahead;
		if (n <= avail)
			return 1;
		if (p->lookahead > p->lookahead_buf) {
			/* Pack the buffer down */
			memmove(p->lookahead_buf, p->lookahead, avail);
			p->lookahead = p->lookahead_buf;
			p->lookahead_end = p->lookahead_buf + avail;
		}
		if (p->last_read > 0) {
			/* Try to read as much as possible each time */
			p->last_read = p->cb->read(p, p->lookahead_end,
						   MAX_LOOKAHEAD - avail);
		}
		if (p->last_read <= 0) {
			return 0;
		}
		p->lookahead_end += p->last_read;
	}
}

/**
 * Peeks at what the next character would be without altering state.
 * @returns what #next() would return.
 */
static int
peek(struct parser *p)
{
	int ret;

	if (!lookahead(p, 1)) {
		ret = EOF;
	} else {
		ret = *p->lookahead;
	}
	return ret;
}

/**
 * Consumes and returns the next character from the input stream.
 * @return EOF if there is no more data.
 */
static int
next(struct parser *p)
{
	int ret = peek(p);

	if (ret != EOF) {
		++p->lookahead; /* advance peek pointer */
		if ((ret & ~0x7f) == 0x00 || /* ASCII */
		    (ret & ~0x3f) == 0xc0)   /* UTF-8 start sequence */
		{
			++p->utf8col;
		}
	}
	if (ret == '\n') {
		++p->lineno;
		p->utf8col = 1;
	}
	return ret;
}

/**
 * Skips over n characters from input.
 * This is like calling #next() @a n times.
 */
static void
skip(struct parser *p, unsigned n)
{
	while (n--) {
		(void) next(p);
	}
}


/**
 * Tests if a string _could_ be consumed from input next.
 * This function has no effect on the input pointer.
 *
 * @param p  the parser
 * @param s  the C string to consider
 *
 * @returns true if the string @a s appears to be next on the input.
 */
#define could_read(p, s) could_read_(p, s, sizeof s - 1)
static int
could_read_(struct parser *p, const char *s, int len)
{
	return lookahead(p, len) && memcmp(p->lookahead, s, len) == 0;
}

/**
 * Tries to consume the given string from the input.
 * If @a s is not immediately next on the input, this function
 * has no effect. Otherwise, the string is conumed and the
 * input pointer is advanced past it.
 *
 * @param p  the parser
 * @param s  the C string to be consumed
 *
 * @return true iff @a s could be consumed from the input.
 */
#define can_read(p, s) can_read_(p, s, sizeof s - 1)
static int
can_read_(struct parser *p, const char *s, int len)
{
	if (could_read_(p, s, len)) {
		skip(p, len);
		return 1;
	} else {
		return 0;
	}
}

/**
 * Test if the given string could be consumed as the next "word".
 * That is, test if the string is present AND it
 * would be followed immediately by a non-alnum or EOF.
 * This allows us to check for identifiers that are not part of
 * a longer identifier.
 *
 * @param p  the parser context
 * @param s  the word you're looking for (a literal constant)
 *
 * @return true iff the string could be consumed and it would
 *              be followed by a non-alnum character, or EOF
 */
#define could_read_w(p, s) could_read_w_(p, s, sizeof s - 1)
static int
could_read_w_(struct parser *p, const char *s, int len)
{
	return lookahead(p, len + 1) &&
	       memcmp(p->lookahead, s, len) == 0 &&
	       !isalnum(p->lookahead[len]);
}

/**
 * Returns true if the given string was able to be consumed as
 * the next "word" on the input, leaving a non-alphanumeric
 * character or EOF as the first next input character.
 *
 * @param p  the parser context
 * @param s  the word to consume (a literal constant)
 *
 * @return true iff the word was found and consumed
 */
#define can_read_w(p, s) can_read_w_(p, s, sizeof s - 1)
static int
can_read_w_(struct parser *p, const char *s, int len)
{
	if (could_read_w_(p, s, len)) {
		skip(p, len);
		return 1;
	} else {
		return 0;
	}
}

/**
 * Skips over any whitespace (but not newline) characters on input.
 *
 * @param p the parser
 * @returns a peek at the next character, @see #peek()
 */
static int
skip_sp(struct parser *p)
{
	int ch;
	while ((ch = peek(p)) != EOF && isspace(ch) && ch != '\n')
		next(p);
	return ch;
}

/**
 * Skips over any whitespace (newline included) on input.
 *
 * @param p the parser
 * @returns a peek at the next character, @see #peek()
 */
static int
skip_wsp(struct parser *p)
{
	int ch;
	while ((ch = peek(p)) != EOF && isspace(ch))
		next(p);
	return ch;
}

/**
 * Skips to the end of the current line, but does not consume the \n.
 *
 * @param p the parser
 * @returns a peek at the next character, @see #peek()
 */
static int
skip_to_eol(struct parser *p)
{
	int ch;
	while ((ch = peek(p)) != EOF && ch != '\n')
		next(p);
	return ch;
}

/**
 * Consumes a character iff it is next on the input.
 * If the next character is different, this function has no effect.
 *
 * @param p  the parser
 * @param ch the character to consume
 * @returns 1 iff @a ch was consumed
 */
static int
can_readch(struct parser *p, int ch)
{
	if (peek(p) != ch)
		return 0;
	next(p);
	return 1;
}

/**
 * Reports an error to the caller via the callback's optional
 * error handler.
 *
 * @param p   the parser
 * @param msg the message to report
 * @returns 0
 */
static int
error(struct parser *p, const char *msg)
{
	if (p->cb->error) {
		p->cb->error(p, p->lineno, p->utf8col, msg);
	}
	return 0;
}

/**
 * Consumes up to the next end-of-line, excluding the \n.
 *
 * @param p  the parser context
 *
 * @returns 1 iff what was consumed was all comments or whitespace.
 */
static int
expect_eol(struct parser *p)
{
	int allblank = 1;
	int comment = 0;
	int ch;
	while ((ch = peek(p)) != EOF && ch != '\n') {
		if (ch == '#' && allblank)
			comment = 1;
		else if (!isspace(ch))
			allblank = 0;
		next(p);
	}
	return allblank || comment;
}

/*------------------------------------------------------------
 * Parsing Staterules
 */

/**
 * Tries to convert the first macro of a macro_list into an atom.
 * This is used when $(FOO BAR) is parsed, and the FOO atom
 * is being resolved as an atom early.
 * It doesn't work for forms like $($(VAR)).
 *
 * @param ref  the string macro to convert into an atom. The macro is
 *             converted in-place, if at all.
 */
static void
maybe_make_reference_atom(macro *ref)
{
	if (ref->type == MACRO_REFERENCE &&
	    ref->reference &&
	    ref->reference->macro &&
	    ref->reference->macro->type == MACRO_STR &&
	    ref->reference->macro->str &&
	    !ref->reference->macro->str->next)
	{
	    atom a = atom_from_str(ref->reference->macro->str);
	    str_free(ref->reference->macro->str);
	    ref->reference->macro->type = MACRO_ATOM;
	    ref->reference->macro->atom = a;
	}
}

/**
 * Removes the trailing '?' or '+' character from a macro
 * and return the character removed (or '\0').
 * This is used for handling ?= and += definitions.
 *
 * @param m the macro to trim off the last character
 *
 * @returns the last '?' or '+' character removed from @a m, or
 *          '\0' if no character was erased.
 */
static int
macro_erase_last_assign_prefix(macro *m)
{
	int ch = '\0';

	if (!m) {
		return ch;
	}
	while (m->next) {
		m = m->next;
	}
	if (m->type != MACRO_STR) {
		return ch;
	}

	str *s = m->str;
	if (!s) {
		return ch;
	}
	while (s->next) {
		s = s->next;
	}

	ch = s->seg->data[s->offset + s->len - 1];
	if (ch == '?' || ch == '+') {
		s->len--;
	} else {
		ch = '\0';
	}
	return ch;
}

/** Maximum number of bytes supported in a UTF-8 encoding
 *  of a single unicode code point. */
#define MAX_UTF8  8

/**
 * Attempts to read a UTF-8 encoded codepoint from the
 * input stream. The UTF-8 bytes are not actually decoded;
 * just stored in the utf8[] buffer.
 *
 * @param utf8 return storage. It will be NUL-terminated on success.
 *
 * @return 1 on success, 0 if bad UTF-8.
 */
static int
parse_utf8(struct parser *p, char utf8[static MAX_UTF8])
{
	char *out = utf8;
	int ch;

	ch = next(p);
	if (ch == EOF)
		return error(p, "expected character but got EOF");
	*out++ = ch;
	if (ch & 0x80) {
		while (ch & 0x40) {
		    int c2 = next(p);
		    if (c2 == -1 || (c2 & 0xc0) != 0x80)
			    return error(p, "bad UTF-8");
		    *out++ = c2;
		    ch <<= 1;
		}
	}
	*out = '\0';
	return 1;
}

/**
 * Parse a simple C-like identifier from the input stream.
 * The identifier should match [a-zA-Z][a-zA-Z0-9_$]*
 *
 * @param ident_return where to store the identifier
 *
 * @return 1 on success
 */
static int
parse_ident(struct parser *p, atom *ident_return)
{
	int ch;
	char buf[1024];
	char *b;
	char * const bufend = buf + sizeof buf - 2;

	ch = peek(p);
	if (!isalpha(ch)) {
		return error(p, "expected identifier");
	}
	b = buf;
	while (b < bufend) {
	    *b++ = next(p);
	    ch = peek(p);
	    if (ch != '_' && ch != '$' && !isalnum(ch))
		break;
	}
	if (b == bufend) {
		return error(p, "identifier too long");
	}
	*b = '\0';
	*ident_return = atom_s(buf);
	return 1;
}

/*
 * macro           ::= <empty>
 *                 |   literal_macro   macro
 *	           |   reference_macro macro
 *                 ;
 * literal_macro   ::= '$$'
 *                 |   '\#'
 *                 |   '\' EOL
 *                 |   (char)+              -- char not '$' or '#'
 *                 ;
 * reference_macro ::= '$(' macro_list ')'
 *                 |   '${' macro_list '}'
 *                 |   '$' char             -- char not whitespace or '$'
 *                 ;
 */

/* Close flag bits that inform #parse_macro when it should stop parsing */
#define CLOSE_RPAREN	(1 <<  0)	/**< stop on ')' */
#define CLOSE_RBRACE	(1 <<  1)	/**< stop on '}' */
#define CLOSE_SPACE	(1 <<  2)	/**< stop on ' ' or '\t' */
#define CLOSE_COMMA	(1 <<  3)	/**< stop on ',' */
#define CLOSE_LF	(1 <<  4)	/**< stop on '\n' */
#define CLOSE_HASH	(1 <<  5)	/**< stop on '#' */
#define CLOSE_COLON	(1 <<  6)	/**< stop on ':' */
#define CLOSE_EQUALS	(1 <<  7)	/**< stop on '=' */
#define CLOSE_SEMICOLON	(1 <<  8)	/**< stop on ';' */

static int
is_close(int ch, unsigned close)
{
	if ((close & CLOSE_RPAREN) && ch == ')')
	    return 1;
	if ((close & CLOSE_RBRACE) && ch == '}')
	    return 1;
	if ((close & CLOSE_COMMA) && ch == ',')
	    return 1;
	if ((close & CLOSE_HASH) && ch == '#')
	    return 1;
	if ((close & CLOSE_SPACE) && (ch == ' ' || ch == '\t'))
	    return 1;
	if ((close & CLOSE_LF) && ch == '\n')
	    return 1;
	if ((close & CLOSE_COLON) && ch == ':')
	    return 1;
	if ((close & CLOSE_EQUALS) && ch == '=')
	    return 1;
	if ((close & CLOSE_SEMICOLON) && ch == ';')
	    return 1;
	return 0;
}

/**
 * Parses a macro from the input.
 * This is the workhorse function of the parser. It scans text to build up
 * a macro list, stopping when it encounters a "close" character.
 * On failure, the error field of the parser @a p is filled in (but a
 * macro is still returned).
 *
 * @param p	the parser context
 * @param close flag bits indicating what characters to stop on.
 * @param mp    pointer to where to store the resulting terminated macro.
 *              The caller should initialize this to @c NULL then
 *		deallocate/take the pointer on return, regardless of the
 *              return code.
 * @returns 1 on success, 0 on failure
 */
static int
parse_macro(struct parser *p, unsigned close, macro **mp)
{
	char buf[2048];
	char *b;
	int ch;

again:
	if (is_close(peek(p), close)) {
	    return 1;
	}

	/* literal str macro */

	b = buf;
	while (b < buf + sizeof buf - 2) {
	    if (can_read(p, "$$")) {	/* immediately convert $$ -> $ */
		*b++ = '$';
		continue;
	    }
	    ch = peek(p);
	    if (ch == '$')		/* always stop on $ */
		break;
	    if (is_close(ch, close))
		break;

	    if (ch == '\\') {		/* always consume char after \ */
		*b++ = next(p);
		ch = peek(p);
	    }
	    if (ch == EOF)
		    break;
	    *b++ = next(p);
	}
	if (b != buf) {
	    mp = macro_cons(mp, macro_new_str(str_newn(buf, b - buf)));
	    goto again;
	}

	/* reference macro */

	if (peek(p) == '$') {
	    next(p); /* skip '$' */
	    ch = peek(p);
	    if (ch == '(' || ch == '{') {
	        char closech = (ch == '(' ? ')' : '}');
		macro *args_macro = macro_new_reference();
		struct macro_list **args = &args_macro->reference;
		unsigned flags = (ch == '(' ? CLOSE_RPAREN : CLOSE_RBRACE) |
		                 CLOSE_COMMA | CLOSE_SPACE;
		next(p); /* skip ( or { */
		for (;;) {
		    macro *m = NULL;
		    if (!parse_macro(p, flags, &m)) {
			macro_free(args_macro);
			macro_free(m);
			return 0;
		    }
		    args = macro_list_cons(args, m);
		    if (flags & CLOSE_SPACE) {
		        skip_wsp(p);
			flags &= ~CLOSE_SPACE; /* just commas after this */
			if (peek(p) == closech)
			    break;
		    } else {
			if (peek(p) != ',')
			    break;
			next(p);
		    }
		    if (peek(p) == EOF) {
			macro_free(args_macro);
		        return error(p, "unexpected EOF in macro");
		    }
		}
		maybe_make_reference_atom(args_macro);
		mp = macro_cons(mp, args_macro);
		if (peek(p) != closech) {
		    return error(p, closech == ')' ? "unclosed ("
		                                   : "unclosed {");
		}
		next(p); /* skip closech */
	        goto again;
	    }

	    if (ch == EOF)
	        return error(p, "unexpected EOF after $");
	    if (isspace(ch))
	        return error(p, "unexpected whitespace after $");

	    /* '$' can also be followed by one valid UTF8 character */
	    {
		char utf8[MAX_UTF8];
	        macro *args_macro = macro_new_reference();
		struct macro_list **args = &args_macro->reference;

		if (!parse_utf8(p, utf8)) {
			macro_free(args_macro);
			return 0;
		}
		macro_list_cons(args, macro_new_atom(atom_s(utf8)));
		mp = macro_cons(mp, args_macro);
		goto again;
	    }
	}

	return 1;
}

/**
 * Reads [ \t\n]* and appends it to the macro.
 * This is used by the 'define'/'endef' logic.
 *
 * @param p  parser context
 * @param mp an initialized pointer that will be updated
 *           with a macro containing whitespace.
 */
static void
parse_macro_nlsp(struct parser *p, macro **mp)
{
	char buf[2048];
	char *b;
	int ch;

again:
	b = buf;
	while (b < buf + sizeof buf - 2) {
	    ch = peek(p);
	    if (!(ch == ' ' || ch == '\t' || ch == '\n'))
		break;
	    *b++ = next(p);
	}
	if (b != buf) {
	    mp = macro_cons(mp, macro_new_str(str_newn(buf, b - buf)));
	    goto again;
	}
}

/**
 * Ends any open rule state in the parser context, notifying
 * the callback if it can. This can be called harmlessly
 * just before a non-rule parse event is likely.
 *
 * @param p parser state.
 */
static void
maybe_end_rule(struct parser *p)
{
	if (p->in_rule) {
		p->in_rule = 0;
		if (p->cb->end_rule)
		    p->cb->end_rule(p);
	}
}

/**
 * Parses a single line from the input.
 * May call the callback functions to inform it of what was parsed.
 *
 * @param p the parser context
 *
 * @returns 1 if a line was parsed OK, or
 *          0 if there was a problem: the caller should discard til '\n'.
 */
static int
parse_one(struct parser *p)
{
	int enabled = !p->if_disabled;

	/* TAB command */
	if (peek(p) == '\t') {
		macro *m;
		unsigned lineno = p->lineno;
		next(p);

		m = 0;
		if (!parse_macro(p, CLOSE_LF, &m)) {
			macro_free(m);
			return 0;
		}
		if (!p->in_rule) {
			macro_free(m);
			return error(p, "commands commence before rule");
		}
		if (enabled && p->cb->command) {
			p->cb->command(p, m, lineno);
		} else {
			macro_free(m);
		}
		return 1;
	}

	skip_sp(p);
	if (peek(p) == '#') {
		/* #comment */
		skip_to_eol(p);
	}
	if (peek(p) == '\n' || peek(p) == EOF) {
		/* blank line */
		return 1;
	}

	/* .directive */
	if (peek(p) == '.') {
		macro *text = 0;
		atom ident;
		unsigned lineno = p->lineno;

		next(p);
		if (!parse_ident(p, &ident)) {
			return 0;
		}
		skip_sp(p);
		if (!parse_macro(p, CLOSE_LF | CLOSE_HASH, &text)) {
			macro_free(text);
			return 0;
		}

		maybe_end_rule(p);

		if (enabled && p->cb->directive) {
			p->cb->directive(p, ident, text, lineno);
		} else {
			macro_free(text);
		}
		return 1;
	}

	/* conditionals */

	unsigned condkind = 0;
	if (can_read_w(p, "ifdef"))
		condkind = CONDKIND_IFDEF;
	else if (can_read_w(p, "ifndef"))
		condkind = CONDKIND_IFDEF | CONDKIND_NEGATE;
	else if (can_read_w(p, "ifeq"))
		condkind = CONDKIND_IFEQ;
	else if (can_read_w(p, "ifneq"))
		condkind = CONDKIND_IFEQ | CONDKIND_NEGATE;
	if (condkind) {
		int negate = condkind & CONDKIND_NEGATE;
		int result;
		unsigned lineno = p->lineno;
		macro *t1 = 0, *t2 = 0;
		condkind -= negate;
		skip_sp(p);
		if (condkind == CONDKIND_IFDEF) {
		    if (!parse_macro(p, CLOSE_LF|CLOSE_HASH|CLOSE_SPACE, &t1)) {
			macro_free(t1);
			return 0;
		    }
		    if (!expect_eol(p)) {
		        macro_free(t1);
			return error(p, "unexpected data after ifdef argument");
		    }
		} else {
		    if (!can_readch(p, '(')) {
			return error(p, "expected '(' after ifeq/ifneq");
		    }
		    if (!parse_macro(p, CLOSE_COMMA, &t1)) {
			macro_free(t1);
			return 0;
		    }
		    if (!can_readch(p, ',')) {
			macro_free(t1);
			return error(p, "expected ',' after ifeq/ifneq");
		    }
		    if (!parse_macro(p, CLOSE_RPAREN, &t2)) {
			macro_free(t2);
			macro_free(t1);
			return 0;
		    }
		    if (!can_readch(p, ')')) {
			macro_free(t2);
			macro_free(t1);
			return error(p, "expected ')' after ifeq/ifneq");
		    }
		    if (!expect_eol(p)) {
			macro_free(t2);
			macro_free(t1);
			return error(p, "expected nothing after ')'");
		    }
		}
		if (enabled && p->cb->condition) {
		    result = p->cb->condition(p, condkind, t1, t2, lineno);
		    if (negate) result = !result;
		} else {
		    macro_free(t1);
		    macro_free(t2);
		    result = 0;
		}
		if (enabled && result) {
		    p->if_endepth++;
		} else {
		    p->if_disabled++;
		}
		return 1;
	}
	if (can_read_w(p, "else")) {
		if (p->if_disabled == 1) {
			p->if_disabled = 0;
			p->if_endepth++;
		} else if (p->if_endepth && !p->if_disabled) {
			p->if_endepth--;
			p->if_disabled = 1;
		} else if (!p->if_endepth && !p->if_disabled) {
			return error(p, "unexpected else");
		}
		if (!expect_eol(p))
			return error(p, "expected nothing after else");
		return 1;
	}
	if (can_read_w(p, "endif")) {
		if (p->if_disabled)
			p->if_disabled--;
		else if (p->if_endepth)
			p->if_endepth--;
		else
			return error(p, "unexpected endif");
		if (!expect_eol(p))
			return error(p, "expected nothing after endif");
		return 1;
	}

	unsigned define = 0; /* depth of nested 'define'/'endef' */
	if (can_read_w(p, "define")) {
		define = 1;
		skip_sp(p);
	}

	/* rule or assignment, read up to a : or = */

	macro *lead = 0;
	if (!parse_macro(p,
		         CLOSE_LF | CLOSE_HASH | CLOSE_COLON | CLOSE_EQUALS,
		         &lead))
	{
		macro_free(lead);
		return 0;
	}
	int ch = peek(p);
	if (ch == EOF) {
		macro_free(lead);
		return error(p, "unexpected EOF");
	}
	if (!define && (ch == '#' || ch == '\n')) {
		macro_free(lead);
		return error(p, "missing separator");
	}

	/* assignment */

	int defch = 0;
	if (could_read(p, ":=")) {
		defch = next(p); /* ':' */
		ch = '=';
	} else if (ch == '=') {
		defch = macro_erase_last_assign_prefix(lead);
	}
	if (define || ch == '=') {
		macro *text = 0;
		unsigned lineno = p->lineno;
		if (ch == '=')
			next(p); /* skip '=' */
		skip_sp(p);
		if (!parse_macro(p, CLOSE_LF | CLOSE_HASH, &text)) {
			macro_free(text);
			macro_free(lead);
			return 0;
		}
		if (define) {
			macro *define_text = 0;
			macro **mp = &define_text;

			mp = macro_cons(mp, text);
			text = 0;
			skip_to_eol(p);
			while (define) {
				macro *part = 0;
				parse_macro_nlsp(p, &part);
				mp = macro_cons(mp, part);
				if (define == 1 && can_read_w(p, "endef")) {
					break;
				}
				if (could_read_w(p, "define"))
					define++;
				else if (could_read_w(p, "endef"))
					define--;
				part = 0;
				if (!parse_macro(p, CLOSE_LF, &part)) {
					macro_free(part);
					macro_free(define_text);
					macro_free(lead);
					return 0;
				}
				mp = macro_cons(mp, part);
			}
			text = define_text;
			skip_sp(p);
			if (peek(p) == '#') {
				skip_to_eol(p);
			}
			macro_ltrim(&text);
		}
		maybe_end_rule(p);
		if (enabled && p->cb->define) {
			macro_rtrim(&lead);
			macro_rtrim(&text);
			p->cb->define(
			    p,
			    lead,
			    defch == ':' ? DEFKIND_IMMEDIATE :
			    defch == '?' ? DEFKIND_WEAK :
			    defch == '+' ? DEFKIND_APPEND :
					   DEFKIND_DELAYED,
			    text, lineno);
		} else {
			macro_free(lead);
			macro_free(text);
		}
		return 1;
	}

	/* rule */

	if (ch == ':') {
		macro *depends = 0;
		unsigned lineno = p->lineno;

		next(p);
		macro_rtrim(&lead);
		skip_sp(p);
		if (!parse_macro(
			p,
			CLOSE_LF | CLOSE_HASH | CLOSE_SEMICOLON,
			&depends))
		{
			macro_free(depends);
			macro_free(lead);
			return 0;
		}
		if (peek(p) != ';' && !expect_eol(p)) {
			macro_free(depends);
			macro_free(lead);
			return error(p, "unexpected text after rule");
		}
		maybe_end_rule(p);
		if (enabled && p->cb->rule) {
			p->cb->rule(p, lead, depends, lineno);
		} else {
			macro_free(depends);
			macro_free(lead);
		}

		p->in_rule = enabled;
		if (peek(p) == ';') {
			macro *m = 0;
			next(p);
			skip_sp(p);
			if (!parse_macro(p, CLOSE_LF, &m)) {
			    macro_free(m);
			    return 0;
			}
			if (enabled && p->cb->command) {
			    p->cb->command(p, m, lineno);
			} else {
			    macro_free(m);
			}
		}
		return 1;
	}

	macro_free(lead);
	return error(p, "unexpected parse error");
}

int
parse(const struct parser_cb *cb, void *context)
{
	struct parser p;

	parser_init(&p, cb, context);

	while (p.last_read > 0) {
	    if (!parse_one(&p)) {
		skip_to_eol(&p);
	    }
	    can_readch(&p, '\n');	/* skips \n if it is there */
	}
	maybe_end_rule(&p);

	return p.last_read;
}

