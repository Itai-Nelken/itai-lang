#ifndef ARRAYS_H
#define ARRAYS_H

#include <stddef.h> // size_t

#define ARRAY_INITIAL_CAPACITY 8

typedef struct array {
    void **data;
    size_t used, capacity;
} Array;

/***
 * Initialize an Array.
 *
 * @param a The Array to initialize.
 ***/
void arrayInit(Array *a);

/***
 * Free an Array.
 *
 * @param a The Array to free.
 ***/
void arrayFree(Array *a);

/***
 * Push a value to an Array.
 *
 * @param a The Array to use.
 * @param value The value to push.
 * @return The index of the value in the Array.
 ***/
size_t arrayPush(Array *a, void *value);

/***
 * Pop a value from an Array.
 *
 * @param a The Array to use.
 * @return The value or NULL if the Array is empty.
 ***/
void *arrayPop(Array *a);

/***
 * Get the value at index 'index' in an Array.
 *
 * @param a The array to use.
 * @param index The index of the value.
 * @return The value or NULL if the index is out of bounds.
 ***/
void *arrayGet(Array *a, size_t index);

/***
 * Clear an Array.
 *
 * @param a The Array to clear.
 ***/
void arrayClear(Array *a);

/***
 * Copy Array 'src' to 'dest'.
 * NOTE: 'dest' is cleared before the copy.
 *
 * @param dest The destination Array.
 * @param src The source Array.
 ***/
void arrayCopy(Array *dest, Array *src);

/***
 * Call 'callback' for every element in the Array.
 *
 * @param a The Array to use.
 * @param callback The callback.
 * @param cl Custom data passed to the callback.
 ***/
void arrayMap(Array *a, void(*callback)(void *item, void *cl), void *cl);

/***
 * Pop a value from an Array and cast it to type 'type'.
 *
 * @param type The type to cast the value to.
 * @param array The Array to use.
 * @return The value cast to 'type' or NULL if the Array is empty.
 ***/
#define ARRAY_POP_AS(type, array) ((type)arrayPop(array))

/***
 * Get the value at index 'index' in an Array and cast it to type 'type'.
 *
 * @param type The type to cast the value to.
 * @param array The Array to use.
 * @param index The index of the value.
 * @return The value cast to 'type' or NULL if 'index' is out of bounds.
 ***/
#define ARRAY_GET_AS(type, array, index) ((type)arrayGet(array, index))

#endif // ARRAYS_H
