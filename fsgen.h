#ifndef fsgen_h
#define fsgen_h

struct match;
struct str;

/**
 * Generates candidate match objects from the filesystem.
 * The initial blank prefix expands to the content of the
 * current directory, plus /.
 *
 * @param mp      pointer to storage to hold the resulting
 *                match list
 * @param prefix  the path prefix to search; usually
 *                ends with '/'.
 *
 * @returns pointer to next place to add a match.
 *
 */
struct match **fs_generate(struct match **mp, const struct str *prefix);

#endif /* fsgen_h */
