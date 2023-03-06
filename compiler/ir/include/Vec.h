#ifndef VEC_H
#define VEC_H

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h> // offsetof
#include <assert.h>

typedef struct vec_header {
    uint32_t used;
    uint32_t capacity;
    uint32_t element_size;
    uint8_t data[];
} VecHeader;

union align {
    int i;
    long l;
    long *lp;
    void *p;
    void (*fp)(void);
    float f;
    double d;
    long double ld;
};

union header {
    VecHeader h;
    union align a;
};

__attribute__((unused)) static VecHeader *_vec_make_header(uint32_t element_size, uint32_t capacity) {
    VecHeader *h = calloc(1, element_size * capacity + sizeof(union header));
    h->capacity = capacity;
    h->element_size = element_size;
    return h;
}


__attribute__((unused)) static inline VecHeader *_vec_to_header(void *vec) {
    assert(vec);
    return vec - offsetof(VecHeader, data);
}

__attribute__((unused)) static inline void _vec_free_header(VecHeader *h) {
    free(h);
}

__attribute__((unused)) static void _vec_ensure_capacity(void **vec, uint32_t capacity) {
    assert(vec && *vec);
    VecHeader *h = _vec_to_header(*vec);
    if(h->used <= capacity) {
        return;
    }
    h->capacity = capacity;
    h = realloc(h, h->element_size * h->capacity +sizeof(union header));
    *vec = (void *)h->data;
}

__attribute__((unused)) static void *_vec_prepare_push(void **vec) {
    assert(vec && *vec);
    VecHeader *h = _vec_to_header(*vec);
    if(h->used + 1 > h->capacity) {
        h->capacity *= 2;
        h = realloc(h, h->element_size * h->capacity + sizeof(union header));
    }
    h->used++;
    *vec = (void *)h->data;
    return h->data + (h->used - 1) * h->element_size;
}


__attribute__((unused)) static void *_vec_pop(VecHeader *h) {
    assert(h && h->used > 0);
    h->used--;
    return h->data + h->used * h->element_size;
}

__attribute__((unused)) static inline void *_vec_get(VecHeader *h, uint32_t index) {
    assert(h && index < h->used);
    return h->data + index * h->element_size;
}

__attribute__((unused)) static void _vec_copy(void **dest_vec, VecHeader *src) {
    assert(dest_vec && *dest_vec && src);
    VecHeader *dest = _vec_to_header(*dest_vec);
    assert(dest->element_size == src->element_size);
    dest->used = 0; // clear the destination vec.
    _vec_ensure_capacity(dest_vec, src->capacity);
    dest = _vec_to_header(*dest_vec); // needed in case of reallocation in above call.
    // both loops can be replaced with 'memcpy(dest->data, src->data, i * src->element_size);
    for(uint32_t i = 0; i < src->used; ++i) {
        for(uint32_t j = 0; j < src->element_size; ++j) {
            dest->data[i * dest->element_size + j] = src->data[i * src->element_size + j];
        }
    }
    dest->used = src->used;
}

__attribute__((unused)) static void _vec_reverse(void *vec) {
    assert(vec);
    VecHeader *h = _vec_to_header(vec);
    // The division below might result in a non-natural number in which
    // case it will be rounded down which is what is wanted.
    for(uint32_t i = 0; i < h->used / 2; ++i) {
        for(uint32_t j = 0; j < h->element_size; ++j) {
            uint32_t a_idx = i * h->element_size + j;
            uint32_t b_idx = (h->used - i - 1) * h->element_size + j;
            uint8_t tmp = h->data[a_idx];
            h->data[a_idx] = h->data[b_idx];
            h->data[b_idx] = tmp;
        }
    }
}

#define VEC_INITIAL_SIZE 8

// Note: do NOT use type qualifiers along with a Vec(Type) "type".
//       Use only in struct fields when it is needed to show
//       that a pointer is a vec.
#define Vec(Type) Type *

#define VEC_NEW(type) ((type *)(_vec_make_header(sizeof(type), VEC_INITIAL_SIZE)->data))
#define VEC_FREE(vec) (_vec_free_header(_vec_to_header((void *)vec)))

#define VEC_CAPACITY(vec) (_vec_to_header((void *)vec)->capacity)
#define VEC_LENGTH(vec) (_vec_to_header((void *)vec)->used)

#define VEC_PUSH(vec, value) (*(typeof(vec))(_vec_prepare_push((void **)&(vec))) = (value))
#define VEC_POP(vec) (*(typeof(vec))_vec_pop(_vec_to_header((void *)vec)))
#define VEC_GET(vec, index) (*(typeof(vec))_vec_get(_vec_to_header((void *)vec), index))

#define VEC_COPY(dest, src) (_vec_copy((void **)&(dest), _vec_to_header((void *)src)))
#define VEC_CLEAR(vec) ((void)(_vec_to_header((void *)vec)->used = 0))

#define VEC_REVERSE(vec) (_vec_reverse((void *)(vec)))

#define VEC_FOREACH(index_var_name, vec) for(uint32_t index_var_name = 0; index_var_name < VEC_LENGTH(vec); ++index_var_name)
// Note: [vec] is referenced multiple times.
#define VEC_ITERATE(iter_var_name, vec) for(typeof(*vec) *iter_var_name = (vec); iter_var_name - (vec) < VEC_LENGTH(vec); ++iter_var_name)

#endif // VEC_H
