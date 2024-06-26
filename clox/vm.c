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
  vm.openUpvalues = NULL;
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
    ObjFunction *function = frame->closure->function;
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
  tableSet(&vm.globals, vm.stack[0], vm.stack[1]);
  pop();
  pop();
}

static Value peek(int distance) { return vm.stackTop[-1 - distance]; }


static bool call(ObjClosure *closure, int argCount) {
  if (argCount != closure->function->arity) {
    runtimeError("Expected %d arguments but got %d", closure->function->arity, argCount);
    return false;
  }

  // We can invoke a limited number of functions in a chain.
  if (vm.frameCount == FRAMES_MAX) {
    runtimeError("Stack overflow.");
    return false;
  }

  CallFrame *frame = &vm.frames[vm.frameCount++];
  frame->closure = closure;
  frame->ip = closure->function->chunk.code;
  frame->slots = vm.stackTop - argCount - 1;
  return true;
}

static bool callValue(Value callee, int argCount) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
      case OBJ_BOUND_METHOD: {
        ObjBoundMethod *bound = AS_BOUND_METHOD(callee);
        // Store 'this' in the hidden first slot for methods.
        vm.stackTop[-argCount - 1] = bound->receiver;
        return call(bound->method, argCount);
      }
      case OBJ_CLASS: {
        ObjClass *klass = AS_CLASS(callee);
        // Replace 'callee' with the class instance
        vm.stackTop[-argCount - 1] = OBJ_VAL(newInstance(klass));
        Value initialiser;
        Value keyV = OBJ_VAL(vm.initString);
        // If we have an init() method, call it
        if (tableGet(&klass->methods, keyV, &initialiser)) {
          return call(AS_CLOSURE(initialiser), argCount);
        } else if (argCount != 0) {
          runtimeError("Expected 0 arguments but got %d.", argCount);
          return false;
        }
        return true;
      }
      case OBJ_CLOSURE:
        return call(AS_CLOSURE(callee), argCount);
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

static bool invokeFromClass(ObjClass *klass, ObjString *name, int argCount) {
  Value method;
  // Find the desired method in the class
  if (!tableGet(&klass->methods, OBJ_VAL(name), &method)) {
    runtimeError("Undefined property '%s'.", name->chars);
    return false;
  }
  // Invoke it
  return call(AS_CLOSURE(method), argCount);
}

static bool invoke(ObjString *name, int argCount) {
  Value receiver = peek(argCount);

  if (!IS_INSTANCE(receiver)) {
    runtimeError("Only instances have methods.");
    return false;
  }

  ObjInstance *instance = AS_INSTANCE(receiver);

  // Checks if we are invoking a field? This is possible if the field was
  // assigned a function.
  Value value;
  if (tableGet(&instance->fields, OBJ_VAL(name), &value)) {
    vm.stackTop[-argCount - 1] = value;
    return callValue(value, argCount);
  }

  return invokeFromClass(instance->klass, name, argCount);
}

// Return true if we found a method
static bool bindMethod(ObjClass *klass, ObjString *name) {
  Value method;
  Value nameKey = OBJ_VAL(name);
  if (!tableGet(&klass->methods, nameKey, &method)) {
    runtimeError("Undefined property '%s'.", name->chars);
    return false;
  }

  ObjBoundMethod *bound = newBoundMethod(peek(0), AS_CLOSURE(method));

  pop();
  push(OBJ_VAL(bound));
  return true;
}

static ObjUpvalue *captureUpvalue(Value *local) {
  // The VM keeps track of all open upvalues in case multiple upvalues from
  // different functions point to the same local variable.

  ObjUpvalue *prevUpvalue = NULL;
  ObjUpvalue *upvalue = vm.openUpvalues;
  // We can do a > comparison on pointers because we know that we're dealing
  // with stack variables. In fact, the value resides in an array in the
  // CallFrame struct.
  while (upvalue != NULL && upvalue->location > local) {
    prevUpvalue = upvalue;
    upvalue = upvalue->next;
  }

  if (upvalue != NULL && upvalue->location == local) {
    // An upvalue capturing the provided local variable already exists!
    return upvalue;
  }

  ObjUpvalue *createdUpvalue = newUpvalue(local);
  // Insert the upvalue into the VM link list.
  createdUpvalue->next = upvalue;
  if (prevUpvalue == NULL) {
    // The list is empty, create the head.
    vm.openUpvalues = createdUpvalue;
  } else {
    prevUpvalue->next = createdUpvalue;
  }

  return createdUpvalue;
}

static void closeUpvalues(Value *last) {
  while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last) {
    ObjUpvalue *upvalue = vm.openUpvalues;
    // Copy value
    upvalue->closed = *upvalue->location;
    // Point the upvalue to its own field
    upvalue->location = &upvalue->closed;
    // This value has been moved to the heap so it can be removed from the vm
    vm.openUpvalues = upvalue->next;

  }
}

static void defineMethod(ObjString *name) {
  Value method = peek(0);
  ObjClass *klass = AS_CLASS(peek(1));
  Value keyV = OBJ_VAL(name);
  tableSet(&klass->methods, keyV, method);
  pop(); // Closure
}

static bool isFalsey(Value value) {
  // nil or false
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate() {
  // Peeking instead of popping for GC safety
  ObjString *b = AS_STRING(peek(0));
  ObjString *a = AS_STRING(peek(1));

  int length = a->length + b->length;
  char *chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  ObjString *result = takeString(chars, length);
  pop();
  pop();
  push(OBJ_VAL(result));
}

void initVM() {
  resetStack();
  // No allocated objects at the start.
  // Objects are linked list nodes pointing to other objects.
  // The end of the linked list chain should be null so we know when we've
  // reached the last object.
  vm.objects = NULL;
  vm.bytesAllocated = 0;
  vm.nextGC = 1024 * 1024;

  vm.grayCount = 0;
  vm.grayCapacity = 0;
  vm.grayStack = NULL;

  initTable(&vm.globals);
  initTable(&vm.strings);

  // Copy string may trigger a GC and read uninitialised memory :/
  vm.initString = NULL;
  vm.initString = copyString("init", 4);

  defineNative("clock", clockNative);
}

void freeVM() {
  freeTable(&vm.globals);
  freeTable(&vm.strings);
  vm.initString = NULL;
  freeObjects();
}

static InterpretResult run() {
  CallFrame *frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_CONSTANT_LONG()                                                   \
  (frame->closure->function->chunk.constants                                                         \
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
#ifdef DEBUG_LOG_GC
      if (IS_OBJ(*slot)) {
        // Print the pointer
        printf("ptr:%p ", AS_OBJ(*slot));
      }
#endif
      printValue(*slot);
      printf(" ]");
    }
    printf("\n");
    disassembleInstruction(&frame->closure->function->chunk, (int) (frame->ip - frame->closure->function->chunk.code));
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
        Value nameKey = OBJ_VAL(name);
        Value value;
        if (!tableGet(&vm.globals, nameKey, &value)) {
          runtimeError("Undefined variable '%s'.", name->chars);
        }
        push(value);
        break;
      }
      case OP_DEFINE_GLOBAL: {
        ObjString *name = READ_STRING();
        Value nameKey = OBJ_VAL(name);
        tableSet(&vm.globals, nameKey, peek(0));
        pop();
        break;
      }
      case OP_SET_GLOBAL: {
        ObjString *name = READ_STRING();
        Value nameKey = OBJ_VAL(name);
        if (tableSet(&vm.globals, nameKey, peek(0))) {
          tableDelete(&vm.globals, nameKey);
          runtimeError("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_GET_UPVALUE: {
        // Functions have an upvalue array. Slot is an index into it.
        uint8_t slot = READ_BYTE();
        push(*frame->closure->upvalues[slot]->location);
        break;
      }
      case OP_SET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        *frame->closure->upvalues[slot]->location = peek(0);
        break;
      }
      case OP_GET_SUPER: {
        ObjString *name = READ_STRING();
        ObjClass *superclass = AS_CLASS(pop());

        if (!bindMethod(superclass, name)) {
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
      case OP_GET_PROPERTY: {
        // We can only use the .property notation on class instances
        if (!IS_INSTANCE(peek(0))) {
          runtimeError("Only instances have properties.");
          return INTERPRET_RUNTIME_ERROR;
        }

        ObjInstance *instance = AS_INSTANCE(peek(0));
        ObjString *name = READ_STRING();
        Value nameKey = OBJ_VAL(name);

        Value value;
        if (tableGet(&instance->fields, nameKey, &value)) {
          pop(); // Instance
          push(value);
          break;
        }

        if (!bindMethod(instance->klass, name)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_SET_PROPERTY: {
        if (!IS_INSTANCE(peek(1))) {
          runtimeError("Only instances have fields.");
          return INTERPRET_RUNTIME_ERROR;
        }

        ObjInstance *instance = AS_INSTANCE(peek(1));
        ObjString *key = READ_STRING();
        Value keyv = OBJ_VAL(key);
        tableSet(&instance->fields, keyv, peek(0));
        Value value = pop();
        pop(); // Pop the instance
        push(value);
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
      case OP_INVOKE: {
        ObjString *method = READ_STRING();
        int argCount = READ_BYTE();
        if (!invoke(method, argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }
      case OP_SUPER_INVOKE: {
        ObjString *method = READ_STRING();
        int argCount = READ_BYTE();
        ObjClass *superclass = AS_CLASS(pop());
        if (!invokeFromClass(superclass, method, argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }
      case OP_CLOSURE: {
        ObjFunction *function = AS_FUNCTION(READ_CONSTANT());
        ObjClosure *closure = newClosure(function);
        push(OBJ_VAL(closure));
        for (int i = 0; i < closure->upvalueCount; i++) {
          uint8_t isLocal = READ_BYTE();
          uint8_t index = READ_BYTE();
          if (isLocal) {
            closure->upvalues[i] = captureUpvalue(frame->slots + index);
          } else {
            closure->upvalues[i] = frame->closure->upvalues[index];
          }
        }
        break;
      }
      case OP_CLOSE_UPVALUE:
        closeUpvalues(vm.stackTop - 1);
        pop();
        break;
      case OP_RETURN: {
        // The returned value will be top of stack
        // We save it, pop the function, then restore it
        Value result = pop();
        closeUpvalues(frame->slots);
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
      case OP_CLASS:
        // Create a new class object with the given name.
        push(OBJ_VAL(newClass(READ_STRING())));
        break;
      case OP_INHERIT: {
        Value superclass = peek(1);
        if (!IS_CLASS(superclass)) {
          runtimeError("Superclass must be a class.");
          return INTERPRET_RUNTIME_ERROR;
        }
        ObjClass *subclass = AS_CLASS(peek(0));
        // Inheriting a class simply copies all methods from the superclass to
        // the subclass.
        // This doesn't affect inheritance as we perform this before parsing
        // the class methods
        tableAddAll(&AS_CLASS(superclass)->methods, &subclass->methods);
        break;
      }
      case OP_METHOD:
        defineMethod(READ_STRING());
        break;
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

  // Put the function on the stack (for GC purposes)
  push(OBJ_VAL(function));
  ObjClosure *closure = newClosure(function);
  // Now replace the function with a closure that wraps the function
  pop();
  push(OBJ_VAL(closure));
  // Then call the closure
  call(closure, 0);

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
