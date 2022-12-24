#ifndef ARRAYS_H
#define ARRAYS_H

#include <stddef.h> // size_t
#include "memory.h" // Allocator

#define ARRAY_INITIAL_CAPACITY 8

typedef struct array {
    void **data;
    size_t used, capacity;
    Allocator allocator;
} Array;

/***
 * Initialize an Array.
 *
 * @param a The Array to initialize.
 ***/
void arrayInit(Array *a);

/***
 * Initialize an Array with size [size].
 * NOTE: if [size] is 0, the array will be initialized with the default initial capacity.
 *
 * @param a The Array to initialize.
 * @param size The size.
 ***/
void arrayInitSized(Array *a, size_t size);

/***
 * Initialize an Array using allocator [alloc] with size [size].
 *
 * @param a The Array to initialize.
 * @param alloc The allocator to use.
 * @param size The size.
 ***/
void arrayInitAllocatorSized(Array *a, Allocator alloc, size_t size);

/***
 * Free an Array.
 *
 * @param a The Array to free.
 ***/
void arrayFree(Array *a);

/***
 * Return the length (element count) of an Array.
 * NOTE: The element count and NOT the last index are returned.
 *       to get the index of the last element, subtract 1 from the returned value.
 *
 * @param a An Array.
 * @return The length of the Array.
 ***/
size_t arrayLength(Array *a);

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
 * Insert a value at index [index].
 * NOTE: will abort if index is out of bounds.
 *
 * @param a The Array to use.
 * @param index The index to insert the value at.
 * @param value The value to insert.
 ***/
void arrayInsert(Array *a, size_t index, void *value);

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
void arrayMap(Array *a, void (*callback)(void *item, void *cl), void *cl);

/***
 * Call 'callback' for every element in the Array passing the index of the element.
 *
 * @param a The Array to use.
 * @param callback The callback.
 * @param cl Custom data passed to the callback.
 ***/
void arrayMapIndex(Array *a, void (*callback)(void *item, size_t index, void *cl), void *cl);

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
