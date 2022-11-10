#ifndef TABLE_H
#define TABLE_H

#include <stddef.h> // size_t
#include <stdbool.h>

#define TABLE_INITIAL_CAPACITY 16
#define TABLE_MAX_LOAD 0.75 // 75%

typedef struct item {
    // Can't use a NULL key to indicate an empty item
    // as the key might be 0 (NULL).
    bool is_empty;
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
void tableInit(Table *t, tableHashFn hashFn, tableCmpFn cmpFn);

/***
 * Free an initialized table.
 * 
 * @param t A table to free.
 ***/
void tableFree(Table *t);

/***
 * Insert a value to a table. If the key already exists in the table, its value is changed.
 * NOTE: the values are owned by the caller.
 * 
 * @param t An initialized table.
 * @param key The key of the new value.
 * @param value The value of the new value.
 * @return The old value if the key already exists or NULL of the key doesn't already exist.
 ***/
void *tableSet(Table *t, void *key, void *value);

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
 * Call callback for every item in the table.
 *
 * @param item An item.
 * @param callback The callback to call for every item.
 * @param cl user data to pass to the callback.
 ***/
void tableMap(Table *t, void (*callback)(TableItem *item, bool is_last, void *cl), void *cl);

/***
 * Delete an item in a table.
 *
 * @param t An initialized table.
 * @param key The key of the item to delete.
 ***/
void tableDelete(Table *t, void *key);

/***
 * Delete all items in a table.
 *
 * @param t An initialized table.
 * @param free_item_callback The callback to call on every item before deleting it (can be NULL).
 * @param cl Custom data for the callback (can be NULL).
 ***/
void tableClear(Table *t, void (*free_item_callback)(TableItem *item, void *cl), void *cl);

#endif // TABLE_H
