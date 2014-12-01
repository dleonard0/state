#ifndef rule_h
#define rule_h

struct location;
struct macro_list;

struct rule {
	struct location *location;	/* where the rule was defined */
	struct macro_list *commands;	/* commands to run */
};

struct dependencies...

#endif /* rule_h */
