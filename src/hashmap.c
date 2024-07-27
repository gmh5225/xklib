#include "hashmap.h"

/* start with 4 buckets */
#define HASHMAP_MIN_CAP_BITS 2

static void hashmap_add_entry(struct hashmap_entry **pprev,
			      struct hashmap_entry *entry)
{
	entry->next = *pprev;
	*pprev = entry;
}

static void hashmap_del_entry(struct hashmap_entry **pprev,
			      struct hashmap_entry *entry)
{
	*pprev = entry->next;
	entry->next = NULL;
}

void hashmap__init(struct hashmap *map, hashmap_hash_fn hash_fn,
		   hashmap_equal_fn equal_fn, void *ctx)
{
	map->hash_fn = hash_fn;
	map->equal_fn = equal_fn;
	map->ctx = ctx;

	map->buckets = NULL;
	map->cap = 0;
	map->cap_bits = 0;
	map->sz = 0;
}

struct hashmap *hashmap__new(hashmap_hash_fn hash_fn, hashmap_equal_fn equal_fn,
			     void *ctx)
{
	struct hashmap *map = kmalloc(sizeof(*map), GFP_KERNEL);

	if (!map)
		return 0;
	hashmap__init(map, hash_fn, equal_fn, ctx);
	return map;
}

void hashmap__clear(struct hashmap *map)
{
	struct hashmap_entry *cur, *tmp;
	size_t bkt;

	hashmap__for_each_entry_safe(map, cur, tmp, bkt) {
		kfree(cur);
	}
	kfree(map->buckets);
	map->buckets = NULL;
	map->cap = map->cap_bits = map->sz = 0;
}

void hashmap__free(struct hashmap *map)
{
	if (IS_ERR_OR_NULL(map))
		return;

	hashmap__clear(map);
	kfree(map);
}

size_t hashmap__size(const struct hashmap *map)
{
	return map->sz;
}

size_t hashmap__capacity(const struct hashmap *map)
{
	return map->cap;
}

static bool hashmap_needs_to_grow(struct hashmap *map)
{
	/* grow if empty or more than 75% filled */
	return (map->cap == 0) || ((map->sz + 1) * 4 / 3 > map->cap);
}

static size_t hashmap_grow(struct hashmap *map)
{
	struct hashmap_entry **new_buckets;
	struct hashmap_entry *cur, *tmp;
	size_t new_cap_bits, new_cap;
	size_t h, bkt;

	new_cap_bits = map->cap_bits + 1;
	if (new_cap_bits < HASHMAP_MIN_CAP_BITS)
		new_cap_bits = HASHMAP_MIN_CAP_BITS;

	new_cap = 1UL << new_cap_bits;
	new_buckets = kcalloc(new_cap, sizeof(new_buckets[0]), GFP_KERNEL);
	if (!new_buckets)
		return XKLIB_ENOMEM;

	hashmap__for_each_entry_safe(map, cur, tmp, bkt) {
		h = hash_bits(map->hash_fn(cur->key, map->ctx), new_cap_bits);
		hashmap_add_entry(&new_buckets[h], cur);
	}

	map->cap = new_cap;
	map->cap_bits = new_cap_bits;
	kfree(map->buckets);
	map->buckets = new_buckets;

	return 0;
}

static bool hashmap_find_entry(const struct hashmap *map, const long key,
			       size_t hash, struct hashmap_entry ***pprev,
			       struct hashmap_entry **entry)
{
	struct hashmap_entry *cur, **prev_ptr;

	if (!map->buckets)
		return false;

	for (prev_ptr = &map->buckets[hash], cur = *prev_ptr; cur;
	     prev_ptr = &cur->next, cur = cur->next) {
		if (map->equal_fn(cur->key, key, map->ctx)) {
			if (pprev)
				*pprev = prev_ptr;
			*entry = cur;
			return true;
		}
	}

	return false;
}

size_t hashmap_insert(struct hashmap *map, long key, long value,
		      enum hashmap_insert_strategy strategy, long *old_key,
		      long *old_value)
{
	struct hashmap_entry *entry;
	size_t h;
	size_t err;

	if (old_key)
		*old_key = 0;
	if (old_value)
		*old_value = 0;

	h = hash_bits(map->hash_fn(key, map->ctx), map->cap_bits);
	if (strategy != HASHMAP_APPEND &&
	    hashmap_find_entry(map, key, h, NULL, &entry)) {
		if (old_key)
			*old_key = entry->key;
		if (old_value)
			*old_value = entry->value;

		if (strategy == HASHMAP_SET || strategy == HASHMAP_UPDATE) {
			entry->key = key;
			entry->value = value;
			return 0;
		} else if (strategy == HASHMAP_ADD) {
			return XKLIB_EEXIST;
		}
	}

	if (strategy == HASHMAP_UPDATE)
		return XKLIB_ENOENT;

	if (hashmap_needs_to_grow(map)) {
		err = hashmap_grow(map);
		if (err)
			return err;
		h = hash_bits(map->hash_fn(key, map->ctx), map->cap_bits);
	}

	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return XKLIB_ENOMEM;

	entry->key = key;
	entry->value = value;
	hashmap_add_entry(&map->buckets[h], entry);
	map->sz++;

	return 0;
}

bool hashmap_find(const struct hashmap *map, long key, long *value)
{
	struct hashmap_entry *entry;
	size_t h;

	h = hash_bits(map->hash_fn(key, map->ctx), map->cap_bits);
	if (!hashmap_find_entry(map, key, h, NULL, &entry))
		return false;

	if (value)
		*value = entry->value;
	return true;
}

bool hashmap_delete(struct hashmap *map, long key, long *old_key,
		    long *old_value)
{
	struct hashmap_entry **pprev, *entry;
	size_t h;

	h = hash_bits(map->hash_fn(key, map->ctx), map->cap_bits);
	if (!hashmap_find_entry(map, key, h, &pprev, &entry))
		return false;

	if (old_key)
		*old_key = entry->key;
	if (old_value)
		*old_value = entry->value;

	hashmap_del_entry(pprev, entry);
	kfree(entry);
	map->sz--;

	return true;
}