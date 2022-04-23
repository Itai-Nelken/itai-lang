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

void initTable(Table *t, tableHashFn hashFn, tableCmpFn cmpFn) {
    t->capacity = 0;
    t->used = 0;
    initArray(&t->all);
    t->items = NULL;
    t->hashFn = hashFn ? hashFn : (tableHashFn)hashString;
    t->cmpFn = cmpFn ? cmpFn : (tableCmpFn)compareString;
}

void freeTable(Table *t) {
    t->capacity = 0;
    t->used = 0;
    freeArray(&t->all);
    if(t->items != NULL) {
        FREE(t->items);
        t->items = NULL;
    }
}

// TODO: as the capacity will be powers of 2, we can use bitwise AND instead of modulo
static Item *findItem(tableCmpFn cmp, Item *items, size_t capacity, void *key, unsigned hash) {
    unsigned index = hash % capacity;
    Item *tombstone = NULL;

    for(;;) {
        Item *item = &items[index];
        
        if(item->key == NULL) {
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
    Item *items = CALLOC(newCapacity, sizeof(Item));
    for(size_t i = 0; i < newCapacity; ++i) {
        items[i].key = NULL;
        items[i].value = NULL;
    }

    t->used = 0;
    for(size_t i = 0; i < t->capacity; ++i) {
        Item *item = &t->items[i];
        if(item->key == NULL) {
            continue;
        }
        Item *dest = findItem(t->cmpFn, items, newCapacity, item->key, t->hashFn(item->key));
        // ok to do this as table saves pointers, not values.
        dest->key = item->key;
        dest->value = item->value;
        t->used++;
    }

    if(t->items != NULL) {
        FREE(t->items);
    }
    t->items = items;
    t->capacity = newCapacity;
}

void tableSet(Table *t, void *key, void *value) {
    if(t->used + 1 > t->capacity * TABLE_MAX_LOAD) {
        size_t newCapacity = t->capacity == 0 ? TABLE_INITIAL_CAPACITY : t->capacity * 2;
        adjustCapacity(t, newCapacity);
    }

    Item *item = findItem(t->cmpFn, t->items, t->capacity, key, t->hashFn(key));
    // check if new item
    if(item->key == NULL && item->value != (void *)0xDEADC0DE) {
        t->used++;
    }

    item->key = key;
    item->value = value;
    arrayPush(&t->all, item);
}

Item *tableGet(Table *t, void *key) {
    if(t->used == 0) {
        return NULL;
    }

    Item *item = findItem(t->cmpFn, t->items, t->capacity, key, t->hashFn(key));
    if(item->key == NULL) {
        return NULL;
    }

    return item;
}

void tableMap(Table *t, void (*callback)(Item *item, void *cl), void *cl) {
    for(size_t i = 0; i < t->all.used; ++i) {
        Item *item = ARRAY_GET_AS(Item *, &t->all, i);
        callback(item, cl);
    }
}

void tableDelete(Table *t, void *key) {
    if(t->used == 0) {
        return;
    }

    Item *item = findItem(t->cmpFn, t->items, t->capacity, key, t->hashFn(key));
    if(item->key == NULL) {
        return;
    }

    item->key = NULL;
    item->value = (void *)0xDEADC0DE;
}

#undef Item
