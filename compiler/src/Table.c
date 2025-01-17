#include <string.h>
#include <stdbool.h>
#include <stddef.h> // size_t
#include "memory.h"
#include "Table.h"

// undefed at end of file
#define Item TableItem

// FNV-la hashing algorithm
static unsigned hashString(char *string) {
    unsigned length = (unsigned)strlen(string);
    unsigned hash = 2166136261u;
    for(unsigned i = 0; i < length; ++i) {
        hash ^= (char)string[i];
        hash *= 16777619;
    }
    return hash;
}

static bool compareString(char *s1, char *s2) {
    return strcmp(s1, s2) == 0;
}

void tableInit(Table *t, tableHashFn hashFn, tableCmpFn cmpFn) {
    t->capacity = 0;
    t->used = 0;
    t->items = NULL;
    t->hashFn = hashFn ? hashFn : (tableHashFn)hashString;
    t->cmpFn = cmpFn ? cmpFn : (tableCmpFn)compareString;
}

void tableFree(Table *t) {
    t->capacity = 0;
    t->used = 0;
    if(t->items != NULL) {
        FREE(t->items);
        t->items = NULL;
    }
}

size_t tableSize(Table *t) {
    return t->used;
}

// TODO: as the capacity will be powers of 2, we can use bitwise AND instead of modulo
static Item *findItem(tableCmpFn cmp, Item *items, size_t capacity, void *key, unsigned hash) {
    unsigned index = hash % capacity;
    Item *tombstone = NULL;

    for(;;) {
        Item *item = &items[index];
        
        if(item->is_empty) {
            if(item->value != (void *)0xDEADC0DE) {
                // empty item
                return tombstone != NULL ? tombstone : item;
            } else {
               // tombstone
               if(tombstone == NULL) {
                   tombstone = item;
               }
            }
        } else if(cmp(item->key, key)) {
            // we found the key's item
            return item;
        }

        index = (index + 1) % capacity;
    }
}

static void adjustCapacity(Table *t, size_t newCapacity) {
    // initialize a new empty Item array.
    Item *items = CALLOC(newCapacity, sizeof(Item));
    for(size_t i = 0; i < newCapacity; ++i) {
        items[i].is_empty = true;
        items[i].key = NULL;
        items[i].value = NULL;
    }

    // copy the old array to the new array.
    t->used = 0;
    for(size_t i = 0; i < t->capacity; ++i) {
        Item *item = &t->items[i];
        if(item->is_empty) {
            continue;
        }
        Item *dest = findItem(t->cmpFn, items, newCapacity, item->key, t->hashFn(item->key));
        // ok to do this as table saves pointers, not values.
        dest->is_empty = false;
        dest->key = item->key;
        dest->value = item->value;
        t->used++;
    }

    // free the old array.
    if(t->items != NULL) {
        FREE(t->items);
    }
    // set the table's item array to the new one.
    t->items = items;
    // update the capacity to the new one.
    t->capacity = newCapacity;
}

void *tableSet(Table *t, void *key, void *value) {
    if(t->used + 1 > t->capacity * TABLE_MAX_LOAD) {
        size_t newCapacity = t->capacity == 0 ? TABLE_INITIAL_CAPACITY : t->capacity * 2;
        adjustCapacity(t, newCapacity);
    }

    Item *item = findItem(t->cmpFn, t->items, t->capacity, key, t->hashFn(key));
    void *old_value = NULL;
    // check if new item
    if(item->is_empty) {
        t->used++;
    } else {
        // The item already exists.
        // set the old_value to return
        // so caller can handle destructing it.
        old_value = item->value;
    }

    item->is_empty = false;
    item->key = key;
    item->value = value;
    return old_value;
}

Item *tableGet(Table *t, void *key) {
    if(t->used == 0) {
        return NULL;
    }

    Item *item = findItem(t->cmpFn, t->items, t->capacity, key, t->hashFn(key));
    if(item->is_empty) {
        return NULL;
    }

    return item;
}

void tableMap(Table *t, void (*callback)(Item *item, bool is_last, void *cl), void *cl) {
    size_t valid_item_count = 0;
    for(size_t i = 0; i < t->capacity; ++i) {
        Item *item = &t->items[i];
        if(item->is_empty) {
            continue;
        }
        valid_item_count++;
        callback(item, valid_item_count == t->used, cl);
    }
}

static void table_copy_callback(Item *item, bool is_last, void *dest) {
    (void)is_last; // unused
    Table *d = (Table *)dest;
    tableSet(d, item->key, item->value); // return value can be discarded because no duplicates will occur (as the source table won't have them).
}

void tableCopy(Table *dest, Table *src) {
    tableMap(src, table_copy_callback, (void *)dest);
}

void tableDelete(Table *t, void *key) {
    if(t->used == 0) {
        return;
    }

    Item *item = findItem(t->cmpFn, t->items, t->capacity, key, t->hashFn(key));
    if(item->is_empty) {
        return;
    }

    item->is_empty = true;
    item->key = NULL;
    item->value = (void *)0xDEADC0DE;
    t->used--;
}

void tableClear(Table *t, void (*free_item_callback)(TableItem *item, void *cl), void *cl) {
    for(size_t i = 0; i < t->capacity; ++i) {
        Item *item = &t->items[i];
        if(item->is_empty) {
            continue;
        }
        if(free_item_callback) {
            free_item_callback(item, cl);
        }
        tableDelete(t, (void *)item->key);
    }
}

#undef Item
