#ifndef macroscope_h
#define macroscope_h

#include "scope.h"
#include "macro.h"

/**
 * A thin wrapper around @c{struct scope} that ensures the values are
 * macros. This is only so we can exploit typechecking, while keeping
 * the scope implementation decoupled from the value type.
 */
struct macroscope {
	struct scope scope;
};

static inline struct macroscope *
macroscope_new(struct macroscope *outer) {
	return (struct macroscope *)scope_new((struct scope *)outer, 
		(void (*)(void *))macro_free);
}

static inline struct macro *
macroscope_get(const struct macroscope *macroscope,
	const char * /*atom*/ varname)
{
	return scope_get((struct scope *)macroscope, varname);
}

static inline void
macroscope_put(struct macroscope *macroscope, const char * /*atom*/ varname, struct macro *value)
{
	scope_put((struct scope *)macroscope, varname, value);
}

static inline struct macroscope *
macroscope_free(struct macroscope *macroscope)
{
	return (struct macroscope *)scope_free((struct scope *)macroscope);
}

#endif /* macroscope_h */
