#include <sys/types.h>
#include <dirent.h>

#include "str.h"
#include "atom.h"
#include "match.h"
#include "fsgen.h"

#define PATHMAX 4096

struct match **
fs_generate(struct match **mp, const str *prefix)
{
	char path[PATHMAX];
	DIR *dir;
	struct match *m;
	atom slash = atom_s("/");

	if (!prefix) {
		/* Add the root entry, / */ 
		m = match_new(atom_to_str(slash));
		m->flags |= MATCH_DEFERRED;
		*mp = m;
		mp = &m->next;

		/* And return the bare content of . */
		path[0] = '.';
		path[1] = '\0';
	} else {
		unsigned pathlen = str_copy(prefix, path, 0, PATHMAX - 1);
		path[pathlen] = '\0';
	}
	dir = opendir(path);
	if (dir) {
		/* Every directory entry is returned,
		 * including . and .. */
		struct dirent *de;
		while ((de = readdir(dir))) {
			if (!de->d_name[0])
				continue;
			str *s, **x;
			x = &s;
			x = str_xcat(x, prefix);
			x = str_xcats(x, de->d_name);
			*x = 0;
			m = match_new(s);
			*mp = m; mp = &m->next;

			/* Directories have the / appended */
			if (de->d_type == DT_DIR) {
				str *ss;
				x = &ss;
				x = str_xcat(x, s);
				*x = atom_to_str(slash);
				m = match_new(ss);
				m->flags |= MATCH_DEFERRED;
				*mp = m; mp = &m->next;
			}
		}
		closedir(dir);
	}
	return mp;
}
