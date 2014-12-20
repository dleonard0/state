#include <assert.h>

#include "str.h"
#include "atom.h"

/* Unit tests for atoms */

int
main(void)
{
	{
		assert(!atom_s(0));

		atom empty = atom_s("");
		assert(empty);
		assert(!*empty);
		assert(atom_from_str(0) == empty);
	}
	{
		atom A = atom_s("A");
		assert(A[0] == 'A');
		assert(A[1] == '\0');

		assert(A == atom_s("A"));
		STR A2 = str_new("A");
		assert(A == atom_from_str(A2));

		atom B = atom_s("B");
		assert(B != A);

		STR s = atom_to_str(B);
		assert(str_eq(s, "B"));

		assert(!atom_to_str(0));
		assert(!atom_to_str(atom_s("")));
	}
	{
		atom A = atom_s("A");
		atom B = atom_s("B");
		str *s;
		str **x = &s;
		x = atom_xstr(x, A);
		x = atom_xstr(x, B);
		*x = 0;
		assert(str_eq(s, "AB"));
		str_free(s);
	}
	{
		atom C = atom_s("C");
		atom D = atom_sn("DX", 1);

		assert(atom_sn("CX", 1) == C);
		assert(atom_s("D") == D);
	}
	return 0;
}
