#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"
#include "chunk.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_CLOSURE(value)   isObjType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value)  isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value)    isObjType(value, OBJ_NATIVE)
#define IS_STRING(value)    isObjType(value, OBJ_STRING)

#define AS_CLOSURE(value)   ((ObjClosure*) AS_OBJ(value))
#define AS_FUNCTION(value)  ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value) \
    (((ObjNative*)AS_OBJ(value))->function)
#define AS_STRING(value)    ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value)   (((ObjString *)AS_OBJ(value))->chars)

typedef enum {
  OBJ_CLOSURE,
  OBJ_FUNCTION,
  OBJ_NATIVE,
  OBJ_STRING,
  OBJ_UPVALUE
} ObjType;

// Structs that store an Obj as the first field can be casted to an Obj to
// access Obj properties. This is possible as C mandates that the memory layout
// of structs matches the order that the struct fields are defined in
struct Obj {
  ObjType type;
  uint32_t hash;
  struct Obj *next;
};

typedef struct {
  Obj obj;
  int arity;
  // How many variables does this use that are defined outside the function
  int upvalueCount;
  Chunk chunk;
  ObjString* name;
} ObjFunction;

typedef Value (*NativeFn)(int argCount, Value *args);

typedef struct {
  Obj obj;
  NativeFn function;
} ObjNative;

struct ObjString {
  Obj obj;
  int length;
  // Ending a struct with an array leverages the 'flexible array member' feature
  // of C. If this were a char* pointer, it'd cause an extra indirection.
  char chars[];
};

typedef struct ObjUpvalue {
  Obj obj;
  // A pointer to the closed over variable to capture
  Value *location;
  // Holds a closed value.
  Value closed;
  struct ObjUpvalue* next;
} ObjUpvalue;

typedef struct {
  Obj obj;
  ObjFunction *function;
  ObjUpvalue **upvalues;
  // A duplicate of function->upvalueCount that the GC can access after freeing
  // 'function'
  int upvalueCount;
} ObjClosure;

ObjClosure *newClosure(ObjFunction *function);
ObjFunction *newFunction();
ObjNative *newNative(NativeFn function);
ObjString *takeString(char *chars, int length);
ObjString *copyString(const char *chars, int length);
ObjUpvalue *newUpvalue(Value *slot);
void printObject(Value value);

Value *stringToValue(ObjString *str);

static inline bool isObjType(Value value, ObjType type) {
  // This is not put directly in IS_X macros as it would inadvertently
  // evaluate the expression passed to `value` twice which is problematic if
  // the expression has side effects.
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif
