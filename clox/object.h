#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_STRING(value) isObjType(value, OBJ_STRING)

#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)

typedef enum { OBJ_STRING } ObjType;

// Structs that store an Obj as the first field can be casted to an Obj to
// access Obj properties. This is possible as C mandates that the memory layout
// of structs matches the order that the struct fields are defined in
struct Obj {
    ObjType type;
    struct Obj* next;
};

struct ObjString {
    Obj obj;
    int length;
    uint32_t hash;
    // Ending a struct with an array leverages the 'flexible array member' feature of C.
    // If this were a char* pointer, it'd cause an extra indirection.
    char chars[];
};

ObjString* takeString(char* chars, int length);
ObjString* copyString(const char* chars, int length);
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
    // This is not put directly in IS_X macros as it would inadvertently
    // evaluate the expression passed to `value` twice which is problematic if
    // the expression has side effects.
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif