#ifndef var_h
#define var_h

struct str;
struct macro;

/* Variable references. Convertible to a string using #expand_var() */
struct var {
	enum var_type {
		VAR_IMMEDIATE,
		VAR_DELAYED
	} type;
	union {
		struct str *immediate;
		struct macro *delayed;
	};
};

struct var *var_new(enum var_type type);
void var_free(struct var *);

#endif /* var_h */
