#pragma once

typedef unsigned long (*table_hash_func)(void *key);
typedef int (*table_compare_func)(void *key_a, void *key_b);

struct bucket
{
    void *key;
    void *value;
    struct bucket *next;
};
struct table
{
    int count;
    int capacity;
    table_hash_func hash;
    table_compare_func compare;
    struct bucket **buckets;
};

void table_init(struct table *table, int capacity, table_hash_func hash, table_compare_func compare);
void table_free(struct table *table);

void *table_find(struct table *table, void *key);
void table_add(struct table *table, void *key, void *value);
void table_replace(struct table *table, void *key, void *value);
void *table_remove(struct table *table, void *key);
void *table_reset(struct table *table, int *count);
