#ifndef clox_value_h
#define clox_value_h

#include "common.h"
#include <string.h>

typedef struct Obj Obj;
typedef struct ObjString ObjString;

#ifdef NAN_BOXING

#define SIGN_BIT  ((uint64_t)0x8000000000000000)
// Bit mask to create a Quiet NaN double
#define QNAN     ((uint64_t)0x7ffc000000000000)

#define TAG_NIL   1 // 01
#define TAG_FALSE 2 // 10
#define TAG_TRUE  3 // 11


typedef uint64_t Value;

// Converts false->true then compares
#define IS_BOOL(value)      (((value) | 1) == TRUE_VAL)
#define IS_NIL(value)       ((value) == NIL_VAL)
#define IS_NUMBER(value)    (((value) & QNAN) != QNAN)
#define IS_OBJ(value) \
  (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define AS_BOOL(value)      ((value) == TRUE_VAL)
#define AS_NUMBER(value)    valueToNum(value)
#define AS_OBJ(value) \
  ((Obj*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

#define BOOL_VAL(b)     ((b) ? TRUE_VAL : FALSE_VAL)
#define FALSE_VAL       ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VAL        ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define NIL_VAL         ((Value)(uint64_t)(QNAN | TAG_NIL))
#define NUMBER_VAL(num) numToValue(num)
#define OBJ_VAL(obj) \
  (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj))

// These memcpy's should be optimised away by the compiler
static inline double valueToNum(Value value) {
  double num;
  memcpy(&num, &value, sizeof(Value));
  return num;
}

static inline Value numToValue(double num) {
  Value value;
  memcpy(&value, &num, sizeof(double));
  return value;
}

#else

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
        Obj *obj;
    } as;
} Value;

// Macros to check the type of a value
#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NIL(value)     ((value).type == VAL_NIL)
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)
#define IS_OBJ(value)  ((value).type == VAL_OBJ)

// Macros to read a value
#define AS_BOOL(value) (((value).as.boolean))
#define AS_NUMBER(value) (((value).as.number))
#define AS_OBJ(value) (((value).as.obj))

// Macros to produce a value
#define BOOL_VAL(value)     ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL             ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value)   ((Value){VAL_NUMBER, {.number = value}})
#define OBJ_VAL(object)     ((Value){VAL_OBJ, {.obj = (Obj*) object}})
#endif

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

uint32_t hashValue(Value value);

uint32_t hashByteArray(const uint8_t *bytes, int length);

#endif
