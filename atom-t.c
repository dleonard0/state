#include <assert.h>

#include "str.h"
#include "atom.h"

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
	return 0;
}
