import gdb
import gdb.printing

class CclassPrinter(object):
    "Print a struct cclass *"

    def __init__(self, val):
    	self.val = val

    def to_string(self):
	if not self.val:
		return u"\u03b5"
    	result = "["
	interval = self.val['interval'];
	lasthi = -1
	for i in range(int(self.val['nintervals'])):
	    lo = int(interval[i]['lo'])
	    hi = int(interval[i]['hi'])
	    if lo <= lasthi:
	        result = result + "*OVERLAP*"
	    result = result + CclassPrinter.ch_to_str(lo)
	    if hi > lo + 2:
	    	result = result + '-'
	    if hi != 0x110000 and hi > lo + 1:
	    	result = result + CclassPrinter.ch_to_str(hi - 1)
	    lasthi = hi
	return result + ']'

    @classmethod
    def ch_to_str(cls, ch):
    	if ch == 0: return "\\0"
	if ch in map(ord, "\\-]"): return "\\%c" % ch
    	if ch == ord('\n'): return "\\n"
    	if ch == ord('\r'): return "\\r"
    	if ch == ord('\t'): return "\\t"
    	if ch < 0x20: return "\\x%02X" % ch
	if ch < 0x7f: return chr(ch)
	if ch <= 0xffff: return "\\u%04X" % ch
	if ch <= 0x10ffff: return "\\u+%06X" % ch;
	if ch == 0x110000: return "*MAXCHAR*"
	return "*TOOBIG(0x%X)*" % ch

    @classmethod
    def matches(cls, value):
	t = value.type.unqualified()
    	return t.code == gdb.TYPE_CODE_PTR and \
	       t.target().strip_typedefs().tag == "cclass" 

class CclassPrettyPrinter(gdb.printing.PrettyPrinter):
    def __init__(self, name):
    	super(CclassPrettyPrinter, self).__init__(name, [])
    def __call__(self, val):
        if CclassPrinter.matches(val):
	    return CclassPrinter(val)

gdb.printing.register_pretty_printer(gdb.current_objfile(), 
	CclassPrettyPrinter("cclass"))

