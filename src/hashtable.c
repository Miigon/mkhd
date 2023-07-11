#include "hashtable.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "tr_malloc.h"

static struct bucket *table_new_bucket(const void *key, void *value) {
	struct bucket *bucket = tr_malloc(sizeof(struct bucket));
	bucket->key = key;
	bucket->value = value;
	bucket->next = NULL;
	return bucket;
}

static struct bucket **table_get_bucket(struct table *table, const void *key) {
	struct bucket **bucket = table->buckets + (table->hash(key) % table->capacity);
	while (*bucket) {
		if (table->compare((*bucket)->key, key)) {
			break;
		}
		bucket = &(*bucket)->next;
	}
	return bucket;
}

void table_init(struct table *table, int capacity, table_hash_func hash, table_compare_func compare) {
	table->count = 0;
	table->capacity = capacity;
	table->hash = hash;
	table->compare = compare;
	table->buckets = tr_malloc(sizeof(struct bucket *) * capacity);
	memset(table->buckets, 0, sizeof(struct bucket *) * capacity);
}

void table_free(struct table *table) {
	for (int i = 0; i < table->capacity; ++i) {
		struct bucket *next, *bucket = table->buckets[i];
		while (bucket) {
			next = bucket->next;
			tr_free(bucket);
			bucket = next;
		}
	}
	if (table->buckets) {
		tr_free(table->buckets);
		table->buckets = NULL;
	}
}

void *table_find(struct table *table, const void *key) {
	struct bucket *bucket = *table_get_bucket(table, key);
	return bucket ? bucket->value : NULL;
}

void table_newkeyvalue(struct table *table, const void *key, void *value, bool do_replace) {
	struct bucket **bucket = table_get_bucket(table, key);
	if (*bucket) {
		if (do_replace || !(*bucket)->value) {
			(*bucket)->value = value;
		}
	} else {
		*bucket = table_new_bucket(key, value);
		++table->count;
	}
}

void table_add(struct table *table, const void *key, void *value) { table_newkeyvalue(table, key, value, false); }

void table_replace(struct table *table, const void *key, void *value) { table_newkeyvalue(table, key, value, true); }

void *table_remove(struct table *table, const void *key) {
	void *result = NULL;
	struct bucket *next, **bucket = table_get_bucket(table, key);
	if (*bucket) {
		result = (*bucket)->value;
		next = (*bucket)->next;
		tr_free(*bucket);
		*bucket = next;
		--table->count;
	}
	return result;
}

void *table_reset(struct table *table, int *count) {
	void **values;
	int capacity;
	int index;
	int item;

	capacity = table->capacity;
	*count = table->count;
	values = tr_malloc(sizeof(void *) * table->count);
	item = 0;

	for (index = 0; index < capacity; ++index) {
		struct bucket *next, **bucket = table->buckets + index;
		while (*bucket) {
			values[item++] = (*bucket)->value;
			next = (*bucket)->next;
			tr_free(*bucket);
			*bucket = next;
			--table->count;
		}
	}

	return values;
}
