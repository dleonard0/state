#include <stdlib.h>

#include "var.h"
#include "str.h"
#include "macro.h"
#include "expand.h"

/* Macro variables */

struct var *
var_new(enum var_type type)
{
	struct var *var = malloc(sizeof *var);
	var->type = type;
	return var;
}

void
var_free(struct var *var)
{
	if (var) {
		switch (var->type) {
		case VAR_IMMEDIATE:
			str_free(var->immediate);
			break;
		case VAR_DELAYED:
			macro_free(var->delayed);
			break;
		}
	}
	free(var);
}
