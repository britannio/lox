#include "vm.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"

VM vm;

static Value clockNative(int argCount, Value *args) {
  return NUMBER_VAL((double) clock() / CLOCKS_PER_SEC);
}

static void resetStack() {
  // The stack is defined statically inside the VM struct, so it need not be
  // allocated/freed.
  vm.stackTop = vm.stack;
  vm.frameCount = 0;
}

static void runtimeError(const char *format, ...) {
  // This is a variadic function, one that can take a varying number of
  // arguments.

  va_list args;
  va_start(args, format);
  // A variant of printf() that takes a va_list
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  // Print the full stack trace starting from the top
  for (int i = vm.frameCount - 1; i >= 0; i--) {
    CallFrame *frame = &vm.frames[i];
    ObjFunction *function = frame->function;
    size_t instruction = frame->ip - function->chunk.code - 1;
    fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);
    if (function->name == NULL) {
      fprintf(stderr, "script\n");
    } else {
      fprintf(stderr, "%s()\n", function->name->chars);
    }
  }

  // -1 because we advance past instructions before executing them thus the
  // failed instruction is the previous one.
//  CallFrame *frame = &vm.frames[vm.frameCount - 1];
//  size_t instruction = frame->ip - frame->function->chunk.code - 1;
//  int line = frame->function->chunk.lines[instruction];
//
//  fprintf(stderr, "[line %d] in script\n", line);
  resetStack();
}

static void defineNative(const char *name, NativeFn function) {
  // The pushing and popping is because the stack isn't GC'd
  push(OBJ_VAL(copyString(name, (int) strlen(name))));
  push(OBJ_VAL(newNative(function)));
  // TODO the pointer to stack[0] is invalid after the second pop()
  tableSet(&vm.globals, vm.stack[0], vm.stack[1]);
  pop();
  pop();
}

static Value peek(int distance) { return vm.stackTop[-1 - distance]; }


static bool call(ObjFunction *function, int argCount) {
  if (argCount != function->arity) {
    runtimeError("Expected %d arguments but got %d", function->arity, argCount);
    return false;
  }

  // We can invoke a limited number of functions in a chain.
  if (vm.frameCount == FRAMES_MAX) {
    runtimeError("Stack overflow.");
    return false;
  }

  CallFrame *frame = &vm.frames[vm.frameCount++];
  frame->function = function;
  frame->ip = function->chunk.code;
  frame->slots = vm.stackTop - argCount - 1;
  return true;
}

static bool callValue(Value callee, int argCount) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
      case OBJ_FUNCTION:
        return call(AS_FUNCTION(callee), argCount);
      case OBJ_NATIVE: {
        NativeFn native = AS_NATIVE(callee);
        // Invocation of the C function
        Value result = native(argCount, vm.stackTop - argCount);
        vm.stackTop -= argCount + 1;
        push(result);
        return true;
      }
      default:
        break; // Non-callable object type.

    }
  }
  runtimeError("Can only call functions and classes.");
  return false;
}

static bool isFalsey(Value value) {
  // nil or false
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate() {
  ObjString *b = AS_STRING(pop());
  ObjString *a = AS_STRING(pop());

  int length = a->length + b->length;
  char *chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  ObjString *result = takeString(chars, length);
  push(OBJ_VAL(result));
}

void initVM() {
  resetStack();
  // No allocated objects at the start.
  // Objects are linked list nodes pointing to other objects.
  // The end of the linked list chain should be null so we know when we've
  // reached the last object.
  vm.objects = NULL;
  initTable(&vm.globals);
  initTable(&vm.strings);

  defineNative("clock", clockNative);
}

void freeVM() {
  freeTable(&vm.globals);
  freeTable(&vm.strings);
  freeObjects();
}

static InterpretResult run() {
  CallFrame *frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])
#define READ_CONSTANT_LONG()                                                   \
  (frame->function->chunk.constants                                                         \
       .values[(READ_BYTE() << 16) + (READ_BYTE() << 8) + READ_BYTE()])
// Read a pair of bytes as a big endian uint16_t
#define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_STRING() AS_STRING(READ_CONSTANT())
// The do block permits additional semicolons when the macro is used so
// BINARY_OP(+); compiles.
#define BINARY_OP(valueType, op)                                               \
  do {                                                                         \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {                          \
      runtimeError("Operands must be numbers.");                               \
      return INTERPRET_RUNTIME_ERROR;                                          \
    }                                                                          \
    double b = AS_NUMBER(pop());                                               \
    double a = AS_NUMBER(pop());                                               \
    push(valueType(a op b));                                                   \
  } while (false)

  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    // Print the stack
    printf("          ");
    for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }
    printf("\n");
    disassembleInstruction(&frame->function->chunk, (int) (frame->ip - frame->function->chunk.code));
#endif
    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
      case OP_CONSTANT: {
        Value constant = READ_CONSTANT();
        push(constant);
        break;
      }
      case OP_CONSTANT_LONG: {
        Value constant = READ_CONSTANT_LONG();
        push(constant);
        break;
      }
      case OP_NIL:
        push(NIL_VAL);
        break;
      case OP_TRUE:
        push(BOOL_VAL(true));
        break;
      case OP_FALSE:
        push(BOOL_VAL(false));
        break;
      case OP_POP:
        pop();
        break;
      case OP_GET_LOCAL: {
        uint8_t slot = READ_BYTE();
        push(frame->slots[slot]);
        break;
      }
      case OP_SET_LOCAL: {
        uint8_t slot = READ_BYTE();
        frame->slots[slot] = peek(0);
        break;
      }
      case OP_GET_GLOBAL: {
        ObjString *name = READ_STRING();
        Value *nameKey = stringToValue(name);
        Value value;
        if (!tableGet(&vm.globals, nameKey, &value)) {
          runtimeError("Undefined variable '%s'.", name->chars);
        }
        push(value);
        break;
      }
      case OP_DEFINE_GLOBAL: {
        ObjString *name = READ_STRING();
        Value *nameKey = stringToValue(name);
        tableSet(&vm.globals, *nameKey, peek(0));
        pop();
        break;
      }
      case OP_SET_GLOBAL: {
        ObjString *name = READ_STRING();
        Value *nameKey = stringToValue(name);
        if (tableSet(&vm.globals, *nameKey, peek(0))) {
          tableDelete(&vm.globals, nameKey);
          runtimeError("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_EQUAL: {
        Value b = pop();
        Value a = pop();
        push(BOOL_VAL(valuesEqual(a, b)));
        break;
      }
      case OP_EQUAL_PRESERVE: {
        Value b = pop();
        Value a = peek(0);
        push(BOOL_VAL(valuesEqual(a, b)));
        break;
      }
      case OP_GREATER:
        BINARY_OP(BOOL_VAL, >);
        break;
      case OP_LESS:
        BINARY_OP(BOOL_VAL, <);
        break;
      case OP_ADD:
        if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
          concatenate();
        } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
          double b = AS_NUMBER(pop());
          double a = AS_NUMBER(pop());
          push(NUMBER_VAL(a + b));
          // BINARY_OP(NUMBER_VAL, +);
        } else {
          runtimeError("Operands must be two numbers or two strings");
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      case OP_SUBTRACT:
        BINARY_OP(NUMBER_VAL, -);
        break;
      case OP_MULTIPLY:
        BINARY_OP(NUMBER_VAL, *);
        break;
      case OP_DIVIDE:
        BINARY_OP(NUMBER_VAL, /);
        break;
      case OP_NOT:
        push(BOOL_VAL(isFalsey(pop())));
        break;
        // Get the value on the stack, negate it and return it to the
        // stack.
      case OP_NEGATE:
        if (!IS_NUMBER(peek(0))) {
          runtimeError("Operand must be a number.");
          return INTERPRET_RUNTIME_ERROR;
        }
        push(NUMBER_VAL(-AS_NUMBER(pop())));
        // Optimisation (without benchmark) to update the stack top in
        // place
        //  *(vm.stackTop - 1) = -*(vm.stackTop - 1);
        break;
      case OP_PRINT:
        printValue(pop());
        printf("\n");
        break;
      case OP_JUMP: {
        uint16_t offset = READ_SHORT();
        frame->ip += offset;
        break;
      }
      case OP_JUMP_IF_FALSE: {
        uint16_t offset = READ_SHORT();
        if (isFalsey(peek(0)))
          frame->ip += offset;
        break;
      }
      case OP_LOOP: {
        uint16_t offset = READ_SHORT();
        // Jump backwards by 'offset' bytes.
        frame->ip -= offset;
        break;
      }
      case OP_CALL: {
        int argCount = READ_BYTE();
        if (!callValue(peek(argCount), argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        // callValue() will produce a frame on the CallFrame stack for the new fn.
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }
      case OP_RETURN: {
        // The returned value will be top of stack
        // We save it, pop the function, then restore it
        Value result = pop();
        vm.frameCount--;
        if (vm.frameCount == 0) {
          // This wasn't a `return` keyword but just the end of the <script>
          pop();
          return INTERPRET_OK;
        }

        vm.stackTop = frame->slots;
        // restore the return value
        push(result);
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }
    }
  }

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_CONSTANT_LONG
#undef READ_SHORT
#undef READ_STRING
#undef BINARY_OP
}

InterpretResult interpret(const char *source) {
  // Compiling our code produces a top level <script> function (function without a name)
  ObjFunction *function = compile(source);
  if (function == NULL) return INTERPRET_COMPILE_ERROR;

  // Put the function on the stack
  push(OBJ_VAL(function));
  call(function, 0);

  // Execute the bytecode
  return run();
}

void push(Value value) {
  // An if statement here isn't ideal given that it's at the heart of the VM.
  // We pay the price of it per instruction. But I haven't profiled it and
  // having this check here earlier would've saved some time debugging
  // segfaults.
  if (vm.stackTop - vm.stack >= STACK_MAX) {
    runtimeError("Stack overflow m8.");
  }
  // The stack top points to the next empty space.
  *vm.stackTop = value;
  vm.stackTop++;
}

Value pop() {
  vm.stackTop--;
  return *vm.stackTop;
}
