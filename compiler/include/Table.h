#ifndef TABLE_H
#define TABLE_H

#include <stddef.h> // size_t
#include <stdbool.h>

#define TABLE_INITIAL_CAPACITY 16
#define TABLE_MAX_LOAD 0.75 // 75%

typedef struct item {
    void *key;
    void *value;
} TableItem;

typedef unsigned (*tableHashFn)(void *key);
typedef bool (*tableCmpFn)(void *a, void *b);

typedef struct table {
    size_t used, capacity;
    TableItem *items;
    tableHashFn hashFn;
    tableCmpFn cmpFn;
} Table;

/***
 * Initialize a table.
 * 
 * @param t A table to initialize
 * @param hashFn A hash function or NULL for strings.
 * @param cmpFn A comparing function or NULL for strings.
 ***/
void initTable(Table *t, tableHashFn hashFn, tableCmpFn cmpFn);

/***
 * Free an initialized table.
 * 
 * @param t A table to free.
 ***/
void freeTable(Table *t);

/***
 * Insert a value to a table.
 * NOTE: the values are owned by the caller.
 * 
 * @param t An initialized table.
 * @param key The key of the new value.
 * @param value The value of the new value.
 ***/
void tableSet(Table *t, void *key, void *value);

/***
 * Retrive a value from a table.
 * 
 * @param t An initialized table.
 * @param key The key of the item to retrive.
 * 
 * @return A pointer to the item or NULL if not found.
 ***/
TableItem *tableGet(Table *t, void *key);

/***
 * Delete an item in a table.
 * 
 * @param t An initialized table.
 * @param key The key of the item to delete.
 ***/
void tableDelete(Table *t, void *key);

#endif // TABLE_H
