#ifndef pr_h
#define pr_h

struct location;
extern enum verbosity {
	V_ERROR = 0,	/* errors only */
	V_WARNING,	/* errors+warnings (default) */
	V_VERBOSE,	/* errors+warnings+verbose */
	V_DEBUG		/* errors+warnings+verbose+debug */
} verbosity;

/*
 * The pr_X() functions print a message followed by a newline.
 * The arguments are printf-like in nature.
 * The messages are printed depending on the global #verbosity
 * variable.
 */

#define pr_error(...)		prl_error(0, __VA_ARGS__)
#define pr_warning(...)		prl_warning(0, __VA_ARGS__)
#define pr_verbose(...)		prl_verbose(0, __VA_ARGS__)
#define pr_debug(...)		prl_debug(0, __VA_ARGS__)

#define prl_error(l, ...)	prl(V_ERROR,   l, __VA_ARGS__)
#define prl_warning(l, ...)	prl(V_WARNING, l, __VA_ARGS__)
#define prl_verbose(l, ...)	prl(V_VERBOSE, l, __VA_ARGS__)
#define prl_debug(l, ...)	prl(V_DEBUG,   l, __VA_ARGS__)


#define prl(level, loc, ...)	do { 				\
	const enum verbosity _v = (level);			\
	if (_v >= verbosity) 					\
		prl_(__FILE__, __LINE__, _v, loc, __VA_ARGS__);	\
    } while (0)
void prl_(const char *file, int line, 
	enum verbosity level, const struct location *loc,
	const char *fmt, ...)
	__attribute__((format(printf,5,6)));

#endif /* pr_h */
