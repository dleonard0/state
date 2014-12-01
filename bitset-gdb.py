import gdb
import gdb.printing

class BitsetPrinter(object):
    "Print a struct bitset *"

    def __init__(self, val):
    	self.val = val

    def to_string(self):
	if not self.val:
		return "NULL";
	nbits = self.val['nbits']
	if nbits > 10000:
		return '{ nbits=0x%x }' % nbits
	bits = self.val['bits']
	elemsz = bits.type.target().sizeof
	nelems = (nbits + elemsz * 8 - 1) / (elemsz * 8)
	members = []
	for index in range(nelems):
		base = index * elemsz * 8
		el = int(bits[index])
		for j in range(8 * elemsz):
			if el & (1 << j):
				members.append(base + j)
	return '{' + ",".join(map(str, members)) + '}'

    @classmethod
    def matches(cls, value):
	t = value.type.unqualified()
    	return t.code == gdb.TYPE_CODE_PTR and \
	       t.target().strip_typedefs().tag == "bitset" 

class BitsetPrettyPrinter(gdb.printing.PrettyPrinter):
    def __init__(self, name):
    	super(BitsetPrettyPrinter, self).__init__(name, [])
    def __call__(self, val):
        if BitsetPrinter.matches(val):
	    return BitsetPrinter(val)

gdb.printing.register_pretty_printer(gdb.current_objfile(), 
	BitsetPrettyPrinter("bitset"))

