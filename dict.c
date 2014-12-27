#include <stdlib.h>
#include "dict.h"

#define HASHSZ 257

struct dict {
	void (*free_value)(void *value);
	int (*key_cmp)(const void *key1, const void *key2);
	unsigned (*key_hash)(const void *key);
	unsigned count;
	struct entry {
		struct entry *next;	/**< Single linked list per bucket */
		const void *key;
		void *value;
	} *bucket[HASHSZ];
};

struct dict_iter {
	const struct dict *dict;
	struct entry * const *entry;
	unsigned b;
};

/** Default key comparator: compares by pointer address */
static int
default_cmp(const void *k1, const void *k2)
{
	return (char *)k1 < (char *)k2 ? -1 : (char *)k1 > (char *)k2;
}

/** Default hash function: hashes key pointer addresses.  */
static unsigned
default_hash(const void *k)
{
	unsigned n = (unsigned)k;

	/* Marsaglia's xorshift */
	n ^= n >> 12;
	n ^= n << 25;
	n ^= n >> 27;
	n *= 2685821657736338717;
	return n;
}

struct dict *
dict_new(void (*free_value)(void *),
	 int (*key_cmp)(const void *, const void *),
	 unsigned (*key_hash)(const void *))
{
	unsigned i;
	struct dict *dict;

	if (!key_cmp)
		key_cmp = default_cmp;
	if (!key_hash)
		key_hash = default_hash;

	dict = malloc(sizeof *dict);
	dict->count = 0;
	dict->free_value = free_value;
	dict->key_cmp = key_cmp;
	dict->key_hash = key_hash;
	for (i = 0; i < HASHSZ; ++i)
		dict->bucket[i] = NULL;
	return dict;
}

void
dict_free(struct dict *dict)
{
	unsigned i;

	for (i = 0; i < HASHSZ; ++i) {
		struct entry *e, *next = dict->bucket[i];
		while ((e = next)) {
			next = e->next;
			if (dict->free_value)
				dict->free_value(e->value);
			free(e);
		}
	}
	free(dict);
}

int
dict_put(struct dict *dict, const void *key, void *value)
{
	struct entry **e = &dict->bucket[dict->key_hash(key) % HASHSZ];
	while (*e && dict->key_cmp((*e)->key, key))
		e = &((*e)->next);
	if (!*e) {
		if (value) {
			*e = malloc(sizeof **e);
			(*e)->key = key;
			(*e)->value = value;
			(*e)->next = NULL;
			dict->count++;
		}
		return 0; /* key did not previously exist */
	} else {
		if (dict->free_value)
			dict->free_value((*e)->value);
		if (value)
			(*e)->value = value;
		else {
			struct entry *next = (*e)->next;
			free(*e);
			*e = next;
			dict->count--;
		}
		return 1; /* key previously existed */
	}
}

void *
dict_get(const struct dict *dict, const void *key)
{
	struct entry *const *e = &dict->bucket[dict->key_hash(key) % HASHSZ];

	while (*e && dict->key_cmp((*e)->key, key))
		e = &((*e)->next);
	return *e ? (*e)->value : NULL;
}

unsigned
dict_count(const struct dict *dict)
{
	return dict->count;
}

struct dict_iter *
dict_iter_new(const struct dict *dict)
{
	struct dict_iter *iter = malloc(sizeof *iter);
	iter->dict = dict;
	iter->b = 0;
	iter->entry = &dict->bucket[0];
	return iter;
}

void
dict_iter_free(struct dict_iter *iter)
{
	free(iter);
}

int
dict_iter_next(struct dict_iter *iter, const void **key_ret, void **value_ret)
{
	while (iter->b < HASHSZ && !*iter->entry) {
		iter->entry = &iter->dict->bucket[++iter->b];
	}
	if (iter->b == HASHSZ)
		return 0;
	if (key_ret)
		*key_ret = (*iter->entry)->key;
	if (value_ret)
		*value_ret = (*iter->entry)->value;
	iter->entry = &(*iter->entry)->next;
	return 1;
}
