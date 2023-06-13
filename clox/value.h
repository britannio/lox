#ifndef clox_value_h
#define clox_value_h

#include "common.h"

/* A Value is a double. We also have a ValueArray to manage an array of values.
 * Later on, a Value will represent a wider variety of types. */

typedef double Value;

typedef struct {
    int capacity;
    int count;
    Value *values;
} ValueArray;

void initValueArray(ValueArray *array);

void writeValueArray(ValueArray *array, Value value);

void freeValueArray(ValueArray *array);

void printValue(Value value);

#endif
