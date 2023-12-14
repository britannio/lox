#ifndef clox_value_h
#define clox_value_h

#include "common.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;

// Tagged unions are the pairing of a ValueType with a value itself.
typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJ, // Heap allocated values
} ValueType;

// [type, value]
typedef struct {
    ValueType type;
    // The struct makes space for the largest of these types.
    union {
        bool boolean;
        double number;
        Obj* obj;
    } as;
} Value;

// Macros to check the type of a value
#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NIL(value)     ((value).type == VAL_NIL)
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)
#define IS_OBJ(value)  ((value).type == VAL_OBJ)

// Macros to read a value
#define AS_BOOL(value) ((value.as.boolean))
#define AS_NUMBER(value) ((value.as.number))
#define AS_OBJ(value) ((value.as.obj))

// Macros to produce a value
#define BOOL_VAL(value)     ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL             ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value)   ((Value){VAL_NUMBER, {.number = value}})
#define OBJ_VAL(object)   ((Value){VAL_OBJ, {.obj = (Obj*) object}})

typedef struct {
    int capacity;
    int count;
    Value *values;
} ValueArray;

bool valuesEqual(Value a, Value b);

void initValueArray(ValueArray *array);

void writeValueArray(ValueArray *array, Value value);

void freeValueArray(ValueArray *array);

void printValue(Value value);

#endif
