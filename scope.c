#include <stdlib.h>

#include "dict.h"
#include "scope.h"

struct scope *
scope_new(struct scope *outer, void (*freefn)(void *))
{
	struct scope *scope;

	scope = malloc(sizeof *scope);
	scope->dict = dict_new(freefn, 0, 0);
	scope->outer = outer;
	return scope;
}

void *
scope_get(const struct scope *scope, const char * /*atom*/ varname)
{
	void *value = 0;

	while (!value && scope) {
		value = dict_get(scope->dict, varname);
		scope = scope->outer;
	}
	return value;
}

void
scope_put(struct scope *scope, const char * /*atom*/ varname, 
	  void *value)
{
	dict_put(scope->dict, varname, value);
}

struct scope *
scope_free(struct scope *scope)
{
	struct scope *outer;
	
	outer = scope->outer;
	dict_free(scope->dict);
	free(scope);
	return outer;
}
