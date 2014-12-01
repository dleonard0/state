import gdb
import gdb.printing

class StrPrinter(object):
    "Print a struct str *"

    def __init__(self, val):
    	self.val = val

    def to_string(self):
    	result = ""
	sp = self.val
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
	       t.target().strip_typedefs().tag == "str" 

class StrPrettyPrinter(gdb.printing.PrettyPrinter):
    def __init__(self, name):
    	super(StrPrettyPrinter, self).__init__(name, [])
    def __call__(self, val):
        if StrPrinter.matches(val):
	    return StrPrinter(val)

gdb.printing.register_pretty_printer(gdb.current_objfile(), 
	StrPrettyPrinter("str"))

