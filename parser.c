
#include <ctype.h>
#include <string.h>

#include "parser.h"

#define EOF (-1)

#define CONDKIND_NEGATE 0x80
#define MAX_LOOKAHEAD 1024

struct parser {
	const struct parser_cb *cb;
	void *context;
	unsigned lineno;
	unsigned utf8col;
	char lookahead_buf[MAX_LOOKAHEAD];
	char *lookahead;
	char *lookahead_end;
	int last_read;
	unsigned in_rule;
	unsigned if_endepth;    /* depth of ifs */
	unsigned if_disabled;   /* depth of ifs inside endepth */
};


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

/* Fill lookahead[0..n-1], returning 0 if it couldn't be done. */
static int
lookahead(struct parser *p, unsigned n)
{
	unsigned avail;

	for (;;) {
		avail = p->lookahead_end - p->lookahead;
		if (n <= avail)
			return 1;
		if (p->lookahead > p->lookahead_buf) {
			memmove(p->lookahead_buf, p->lookahead, avail);
			p->lookahead = p->lookahead_buf;
			p->lookahead_end = p->lookahead_buf + avail;
		}
		if (p->last_read > 0) {
			p->last_read = p->cb->read(p, p->lookahead_end,
						   MAX_LOOKAHEAD - avail);
		}
		if (p->last_read <= 0) {
			return 0;
		}
		p->lookahead_end += p->last_read;
	}
}

/* Peeks at the next character in lookahead, or returns EOF */
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

/* Consumes and returns the next character from the input */
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

/* Skips over n characters from input */
static void
skip(struct parser *p, unsigned n)
{
	while (n--) {
		(void) next(p);
	}
}


/* Returns true if the given string could be consumed */
static int
could_read_(struct parser *p, const char *s, int len)
{
	return lookahead(p, len) && memcmp(p->lookahead, s, len) == 0;
}
#define could_read(p, s) could_read_(p, s, sizeof s - 1)

/* Returns true if the given string was able to be consumed */
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
#define can_read(p, s) can_read_(p, s, sizeof s - 1)

/*
 * Returns true if the given string could be consumed,
 * AND it would be followed by a non-letter/digit/EOF
 */
static int
could_read_w_(struct parser *p, const char *s, int len)
{
	return lookahead(p, len + 1) &&
	       memcmp(p->lookahead, s, len) == 0 &&
	       !isalnum(p->lookahead[len]);
}
#define could_read_w(p, s) could_read_w_(p, s, sizeof s - 1)

/*
 * Returns true if the given string was able to be consumed,
 * AND the next (unconsumed) character is not a non-letter/digit
 */
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
#define can_read_w(p, s) can_read_w_(p, s, sizeof s - 1)

/* skip space and tab */
static int
skip_sp(struct parser *p)
{
	int ch;
	while ((ch = peek(p)) != EOF && isspace(ch) && ch != '\n')
		next(p);
	return ch;
}

/* skip space and tab and newline */
static int
skip_wsp(struct parser *p)
{
	int ch;
	while ((ch = peek(p)) != EOF && isspace(ch))
		next(p);
	return ch;
}

/* skip to end of line character, but do not consume the \n */
static int
skip_to_eol(struct parser *p)
{
	int ch;
	while ((ch = peek(p)) != EOF && ch != '\n')
		next(p);
	return ch;
}

/* Skip ch iff it is there */
static void
skip_ch(struct parser *p, int ch)
{
	if (peek(p) == ch)
		next(p);
}

/* report an error to the callback */
static int
error(struct parser *p, const char *msg)
{
	if (p->cb->error) {
		p->cb->error(p, p->lineno, p->utf8col, msg);
	}
	return 0;
}

/* return true if we could read ch */
static int
can_readch(struct parser *p, int ch)
{
	if (peek(p) != ch)
		return 0;
	next(p);
	return 1;
}

/* consume up to eol, and return true if it was all blank or comments */
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

/*
 * Try to convert the first macro in a macro_list into an atom.
 * This is for when $(FOO BAR) is parsed, and the FOO atom
 * can be resolved early once.
 * It doesn't work for (the outermost form of) $($(VAR)), though
 * so that case is left alone.
 */
void
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

/*
 * Remove the trailing '?' or '+' character from a macro
 * and return the character removed (or '\0').
 * This is used for handling ?= and += definitions.
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

#define MAX_UTF8  8

/**
 * Read one UTF-8 character into the buffer.
 * @param utf8 return storage. It will be NUL terminated.
 * @return 1 on success
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
 * Parse a simple C-like identifier.
 * @param ident_return where to store the identifier
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

#define CLOSE_RPAREN	(1 <<  0)
#define CLOSE_RBRACE	(1 <<  1)
#define CLOSE_SPACE	(1 <<  2)
#define CLOSE_COMMA	(1 <<  3)
#define CLOSE_LF	(1 <<  4)
#define CLOSE_HASH	(1 <<  5)
#define CLOSE_COLON	(1 <<  6)
#define CLOSE_EQUALS	(1 <<  7)
#define CLOSE_SEMICOLON	(1 <<  8)

int
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
 * The macro parse workhorse. Scans text to build up
 * a macro structure.
 *
 * @param close bit flags indicating what characters to stop on.
 * @param mp    pointer to the macro to return the result at.
 *              caller should deallocate this regardless of the
 *              return code.
 * @returns 1 on success, 0 on failure
 */
int
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

	    /* '$' can also be followed by one UTF8 character */
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

/* End any open rule state; and inform the callback if necessary */
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
 * Parses a single line.
 * If there's a problem, it returns 0 and expects the
 * caller to ditch the rest of the line.
 * @returns 1 if a line was parsed OK.
 */
static int
parse_one(struct parser *p)
{
	int enabled = !p->if_disabled;

	/* TAB command */
	if (peek(p) == '\t') {
		macro *m;
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
			p->cb->command(p, m);
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
			p->cb->directive(p, ident, text);
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
		    result = p->cb->condition(p, condkind, t1, t2);
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
	if (ch == '#' || ch == '\n' || ch == EOF) {
		macro_free(lead);
		return error(p, "missing separator");
	}

	/* assignment */

	int defch;
	if (could_read(p, ":=")) {
		defch = next(p);
		ch = '=';
	} else if (ch == '=') {
		defch = macro_erase_last_assign_prefix(lead);
	}
	if (ch == '=') {
		macro *text = 0;
		next(p); /* skip '=' */
		skip_sp(p);
		if (!parse_macro(p, CLOSE_LF | CLOSE_HASH, &text)) {
			macro_free(text);
			return 0;
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
			    text);
		} else {
			macro_free(lead);
			macro_free(text);
		}
		return 1;
	}

	/* rule */

	if (ch == ':') {
		macro *depends = 0;

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
			p->cb->rule(p, lead, depends);
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
			    p->cb->command(p, m);
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
	    skip_ch(&p, '\n');
	}
	maybe_end_rule(&p);

	return p.last_read;
}

