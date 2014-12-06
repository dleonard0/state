
CPPFLAGS = -MMD
CFLAGS = -ggdb -Wall

CFLAGS += -std=gnu99

default: check

TESTS  = t-str t-dict t-atom t-macro t-scope t-parser t-cclass t-bitset t-nfa
TESTS += t-globs t-vector t-expand t-match

t-str:    str-t.o    str.o
t-dict:   dict-t.o   dict.o
t-atom:   atom-t.o   str.o dict.o atom.o
t-macro:  macro-t.o  str.o dict.o atom.o macro.o
t-scope:  scope-t.o  str.o dict.o atom.o scope.o
t-parser: parser-t.o str.o dict.o atom.o macro.o parser.o
t-cclass: cclass-t.o cclass.o
t-bitset: bitset-t.o bitset.o
t-nfa:    nfa-t.o    cclass.o bitset.o nfa.o nfa-dbg.o
t-globs:  globs-t.o  cclass.o bitset.o nfa.o str.o globs.o nfa-dbg.o
t-vector: vector-t.o
t-expand: expand-t.o str.o dict.o atom.o macro.o parser.o scope.o expand.o
t-match:  match-t.o  cclass.o bitset.o nfa.o str.o globs.o match.o nfa-dbg.o
$(TESTS):
	$(LINK.c) -o $@ $^

check: $(TESTS:=.tested)
%.tested: %
	@if $(abspath $<); \
	 then printf '%-10s ... PASS\n' $(basename $<); \
	 else printf '%-10s ... FAIL\n' $(basename $<); exit 1; \
	 fi
.PHONY: check 

-include *.d

SRCS = $(wildcard *.c *.h)
TAGS: $(SRCS)
	etags $(SRCS)
tags: TAGS
.PHONY: tags

clean:
	$(RM) $(TESTS)
	$(RM) *.o *.d
	$(RM) TAGS
