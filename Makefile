
CPPFLAGS = -MMD
CFLAGS = -ggdb -Wall

CFLAGS += -std=gnu99

default: check

TESTS  = t-str t-dict t-atom t-macro t-scope t-parser t-cclass t-bitset t-graph
TESTS += t-globs t-vector

t-str:    str-t.o    str.o
t-dict:   dict-t.o   dict.o
t-atom:   atom-t.o   str.o dict.o atom.o
t-macro:  macro-t.o  str.o dict.o atom.o macro.o
t-scope:  scope-t.o  str.o dict.o atom.o scope.o
t-parser: parser-t.o str.o dict.o atom.o macro.o parser.o
t-cclass: cclass-t.o cclass.o
t-bitset: bitset-t.o bitset.o
t-graph:  graph-t.o  cclass.o bitset.o graph.o
t-globs:  globs-t.o  cclass.o bitset.o graph.o str.o globs.o
t-vector: vector-t.o
#t-match:  match-t.o  cclass.o bitset.o graph.o str.o match.o
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
