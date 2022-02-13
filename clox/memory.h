#ifndef clox_memory_h
#define clox_memory_h

#include "common.h"

/* Growing by a factor of 2 (i.e., growing proportionally to size) is what gives us O(1) amortized performance when
 * adding elements to an array that may be full.
 *
 * 8 is an arbitrary minimum initial size of the array. */
#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), \
        sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

void *reallocate(void *pointer, size_t oldSize, size_t newSize);


#endif
