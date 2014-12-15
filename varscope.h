#ifndef varscope_h
#define varscope_h

#include "scope.h"
#include "var.h"

/**
 * A thin wrapper around @c{struct scope} that ensures the values are
 * vars. This is only so we can exploit typechecking, while keeping
 * the scope implementation decoupled from the value type.
 */
struct varscope {
	struct scope scope;
};

static inline struct varscope *
varscope_new(struct varscope *outer) {
	return (struct varscope *)scope_new((struct scope *)outer, 
		(void (*)(void *))var_free);
}

static inline struct var *
varscope_get(const struct varscope *varscope,
	const char * /*atom*/ varname)
{
	return scope_get((struct scope *)varscope, varname);
}

static inline void
varscope_put(struct varscope *varscope, const char * /*atom*/ varname, struct var *value)
{
	scope_put((struct scope *)varscope, varname, value);
}

static inline struct varscope *
varscope_free(struct varscope *varscope)
{
	return (struct varscope *)scope_free((struct scope *)varscope);
}

#endif /* varscope_h */
