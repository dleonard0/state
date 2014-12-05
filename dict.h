#ifndef dict_h
#define dict_h

/**
 * A dictionary is a mapping from keys (pointers) to
 * values (pointers). A user of a dictionary must supply
 * three functions:
 *     - how to free a value pointer
 *     - how to compare keys for equality
 *     - how to efficiently hash keys
 * XXX should at least support freeing keys too
 */
struct dict;

/**
 * Constructs a new dictionary.
 * Dictionaries are collection of immutable keys associated with
 * mutable values.
 * @param free_value  a function that will release values.
 *                    or @c NULL to indicate nothing
 * @param key_cmp     a function to compare two keys are the same.
 *                    or @c NULL to use pointer equality
 * @param key_hash    a function to hash a key into 0..UINT_MAX
 *                    or @c NULL to use a simple pointer address hash
 * @return a pointer that should be deallocated with #dict_free().
 */
struct dict *dict_new(void (*free_value)(void *value),
                      int (*key_cmp)(const void *k1, const void *k2),
		      unsigned (*key_hash)(const void *key));

/*
 * Deallocates the dictionary allocated by #dict_new().
 * All previously put values will be deallocated by calling the
 * @a free_value function.
 * @param dict the dictionary to deallocate
 */
void dict_free(struct dict *dict);

/*
 * Stores or replaces a key-value relationship in the dictioanry.
 * Any previously stored value will be released by calling the @a free_value
 * function.
 * @param dict  the dictionary into which to store the key-value pair
 * @param key   the immutable key pointer
 * @param value pointer to the mutable value, or @c NULL to delete the pair
 * @return true if the key previously existed
 */
int dict_put(struct dict *dict, const void *key, void *value);

/*
 * Retreives a previously stored key-value pair from the dictionary.
 * @param dict   the dictionary to query
 * @param key    the immutable key to look up; only the pointer value is used
 * @return a pointer to the mutable value, or @c NULL if no pair in the
 * dictionary has the requested @a key. 
 * The pointer returned must not be deallocated by the caller.
 */
void *dict_get(const struct dict *dict, const void *key);

/**
 * Compute the number of key-value pairs stored in the dictionary.
 * @param dict the dictionary to count
 * @return number of key-value pairs stored in the dictionary.
 */
unsigned dict_count(const struct dict *dict);

/** An iterator over all the key-value pairs in a dictionary. */
struct dict_iter;

/**
 * Allocate a new dictionary iterator.
 * @param dict  the dictionary to iterate over. This must have a lifetime
 *              longer than the returned iterator.
 * @return a pointer that must eventually be freed by #dict_iter_free()
 */
struct dict_iter *dict_iter_new(const struct dict *dict);


/**
 * Releases the storage allocated by #dict_iter_new().
 */
void dict_iter_free(struct dict_iter *iter);

/**
 * Returns a key-value pair and increments the iterator.
 * The dictionary should not be modified while iterating.
 * TODO make this operation delete-safe
 * @param key    pointer to storage to hold the key
 * @param value  pointer to storage to hold the value pointer. The value
 *               pointer must not be deallocated
 * @return true if no key-value pair was stored, and the iterator is spent.
 */
int dict_iter_next(struct dict_iter *iter, const void **key, void **value);

#endif /* dict_h */
