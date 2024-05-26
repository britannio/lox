#ifndef clox_memory_h
#define clox_memory_h

#include "common.h"
#include "object.h"


#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))


#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0);

/* Growing by a factor of 2 (i.e., growing proportionally to size) is what gives us O(1) amortized performance when
 * adding elements to an array that may be full.
 *
 * 8 is an arbitrary minimum initial size of the array. */
#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCapacity, newCapacity) \
    (type*)reallocate(pointer, sizeof(type) * (oldCapacity), sizeof(type) * (newCapacity))

// Not using free() as we want a central place to keep track of memory usage for GC purposes.
#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

void *reallocate(void *pointer, size_t oldSize, size_t newSize);
void freeObjects();

typedef struct {
  int capacity;
  int count;
  uint8_t *values;
} ByteArray;

void initByteArray(ByteArray *array);

void writeByteArray(ByteArray *array, uint8_t value);

void freeByteArray(ByteArray *array);

#endif
