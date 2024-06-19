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

#define GROW_ARRAY_TYPE_SIZE(type_size, pointer, oldCapacity, newCapacity) \
    reallocate(pointer, type_size * (oldCapacity), type_size * (newCapacity))

// Not using free() as we want a central place to keep track of memory usage for GC purposes.
#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

#define GC_HEAP_GROW_FACTOR 1

void *reallocate(void *pointer, size_t oldSize, size_t newSize);

void markObject(Obj *object);

void markValue(Value value);

void collectGarbage();

void freeObjects();

typedef struct {
  int capacity;
  int count;
  void *values;
  size_t type;
} Array;

void initArray(Array *array, size_t type);

void writeArray(Array *array, void *value);

void freeArray(Array *array);


#define READ_AS(type, array, i) (((type*) (array)->values)[i])

#endif
