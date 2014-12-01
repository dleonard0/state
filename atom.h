#ifndef atom_h
#define atom_h

/**
 * An atom is a constant C string pointer that
 * is guaranteed to have different content
 * to all other atoms. We use it as a fast key.
 */
typedef const char *atom;

struct str;

/** 
 * Returns the atom associated with the C string.
 * The empty strings map to the "" atom.
 * The NULL string pointer maps to the NULL atom.
 * @param  s the string form of the atom
 * @return a unique value for the given string.
 */
atom atom_s(const char *s);

/**
 * Returns the atom associated with a string.
 * The NULL string maps to the "" atom.
 * @param str the string form of the atom.
 * @return a unique value for the given string
 */
atom atom_from_str(struct str *str);

/**
 * Creates a string from an atom.
 * @param a the atom
 * @return a new string that must eventually be
 *         released using #str_free()
 */
struct str *atom_to_str(atom a);

#endif /* atom_h */
