import gdb
import gdb.printing

class MacroPrinter(object):
    "Print a struct macro *"

    def __init__(self, val):
    	self.val = val

    def to_string(self):
	result = ""
        mp = self.val
	while mp:
	    if len(result) > 1000:
	    	return result + '...'
	    t = str(mp['type'])
	    if t == 'MACRO_ATOM':
		atom = mp['atom']
		if atom:
	            result = result + atom.string()
		else:
	            result = result + "[NULL atom]"
	    elif t == 'MACRO_LITERAL':
		literal = mp['literal']
		if literal:
	            result = result + MacroPrinter.str_to_string(literal)
		else:
	            result = result + "[NULL literal]"
	    elif t == 'MACRO_REFERENCE':
		l = mp['reference']
		if l:
	    	    result = result + '$('
		    while l:
		        result = result + MacroPrinter.str_to_string(l['macro'])
		        l = l['next']
		        if l: result = result + ','
		    result = result + ')'
		else:
		    result = result + "[NULL reference]"
	    else:
	        result = result + '[BAD type]'
	    mp = mp['next']
	return result

    @classmethod
    def str_to_string(cls, sp):
    	result = ""
	while sp:
	    if len(result) > 1000:
	        return result + '...'
	    seg = sp['seg']
	    if seg:
		length = int(sp['len'])
	        if not seg['refs']:
		    result = result + "[FREE seg!]"
		elif length:
	            offset = int(sp['offset'])
		    base = (seg['data'].address + offset).dereference()
	            result = result + base.string('','replace',length)
		else:
		    result = result + "[ZERO len!]"
	    else:
	        result = result + "[NULL seg!]"
	    sp = sp['next']
	return result

    def display_hint(self):
    	return 'string'

    @classmethod
    def matches(cls, value):
	t = value.type.unqualified()
    	return t.code == gdb.TYPE_CODE_PTR and \
	       t.target().strip_typedefs().tag == "macro" 

class MacroPrettyPrinter(gdb.printing.PrettyPrinter):
    def __init__(self, name):
    	super(MacroPrettyPrinter, self).__init__(name, [])
    def __call__(self, val):
        if MacroPrinter.matches(val):
	    return MacroPrinter(val)

gdb.printing.register_pretty_printer(gdb.current_objfile(), 
	MacroPrettyPrinter("macro"))

