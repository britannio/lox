#include "value.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "object.h"
#include "memory.h"

void initValueArray(ValueArray *array) {
    array->count = 0;
    array->capacity = 0;
    array->values = NULL;
}

// Append {value} to {array}
void writeValueArray(ValueArray *array, Value value) {
    // Resize the array if it is full/uninitialised
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values =
                GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
    }

    // Set the value and increment the index of the next item
    array->values[array->count] = value;
    array->count++;
}

void freeValueArray(ValueArray *array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    initValueArray(array);
}

void printValue(Value value) {
#ifdef NAN_BOXING
  if (IS_BOOL(value)) {
    printf(AS_BOOL(value) ? "true" : "false");
  } else if (IS_NIL(value)) {
    printf("nil");
  } else if (IS_NUMBER(value)) {
    printf("%g", AS_NUMBER(value));
  } else if (IS_OBJ(value)) {
    printObject(value);
  }
#else
    switch (value.type) {
        case VAL_BOOL:
            printf(AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_NIL:
            printf("nil");
            break;
        case VAL_NUMBER:
            printf("%g", AS_NUMBER(value));
            break;
        case VAL_OBJ:
            printObject(value);
            break;
    }
#endif
}

bool valuesEqual(Value a, Value b) {
#ifdef NAN_BOXING
  if (IS_NUMBER(a) && IS_NUMBER(b)) {
    // Technically nan != nan
    return AS_NUMBER(a) == AS_NUMBER(b);
  }
  return a == b;
#else
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_BOOL:
            return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NIL:
            return true;
        case VAL_NUMBER:
            return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ:
            return AS_OBJ(a) == AS_OBJ(b);
        default:
            return false;
    }
#endif
}


static uint32_t hashDouble(double number) {
    // Normalisation so that doubles with the same value hold the same bytes
    if (isnan(number)) number = 0.0;
    if (isinf(number)) number = (number > 0) ? INFINITY : -INFINITY;
    if (number == -0.0) number = 0.0;

    uint8_t bytes[sizeof(double)];
    memcpy(bytes, &number, sizeof(double));
    return hashByteArray(bytes, sizeof(double));
}

uint32_t hashValue(const Value value) {
  if (IS_BOOL(value)) {
    return AS_BOOL(value) ? 1 : 0;
  } else if (IS_NIL(value)) {
    return 0;
  } else if (IS_NUMBER(value)) {
    return hashDouble(AS_NUMBER(value));
  } else if (IS_OBJ(value)) {
    return AS_OBJ(value)->hash;
  }
  return 0;
}


// FNV-1a hash function
uint32_t hashByteArray(const uint8_t *bytes, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; ++i) {
        hash ^= bytes[i];
        hash *= 16777619;
    }
    return hash;
}
