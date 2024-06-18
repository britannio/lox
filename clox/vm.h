#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "table.h"
#include "value.h"
#include "object.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

// An ongoing function call
typedef struct {
  ObjFunction *function;
  uint8_t *ip;
  // A pointer to the first value stack slot available to the function
  Value *slots;
} CallFrame;

typedef struct {
    CallFrame frames[FRAMES_MAX];
    // The number of ongoing function calls
    int frameCount;
    Value stack[STACK_MAX];
    Value* stackTop;
    Table globals;
    Table strings;
    Obj* objects; // linked list of allocated objects
} VM;

// Used to set the error code of the clox program.
typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;

void initVM();

void freeVM();

InterpretResult interpret(const char* source);

void push (Value value);
Value pop();

#endif
