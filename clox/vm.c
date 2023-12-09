#include <stdarg.h>
#include <stdio.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "vm.h"

VM vm;

static void resetStack() {
    // The stack is defined statically inside the VM struct, so it need not be allocated/freed.
    vm.stackTop = vm.stack;
}

static void runtimeError(const char *format, ...) {
    // This is a variadic function, one that can take a varying number of arguments.

    va_list args;
    va_start(args, format);
    // A variant of printf() that takes a va_list
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    // -1 because we advance past instructions before executing them thus the failed instruction is the previous one.
    size_t instruction = vm.ip - vm.chunk->code - 1;
    int line = vm.chunk->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
    resetStack();
}


static Value peek(int distance) {
    return vm.stackTop[-1 - distance];
}

static bool isFalsey(Value value) {
    // nil or false
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

void initVM() {
    resetStack();
}

void freeVM() {

}

static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define READ_CONSTANT_LONG() (vm.chunk->constants.values[(READ_BYTE() << 16) + (READ_BYTE() << 8) + READ_BYTE()])
// The do block permits additional semicolons when the macro is used so BINARY_OP(+); compiles.
#define BINARY_OP(valueType, op) \
    do {              \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
            runtimeError("Operands must be numbers.");    \
            return INTERPRET_RUNTIME_ERROR;\
        }\
        double b = AS_NUMBER(pop()); \
        double a = AS_NUMBER(pop()); \
        push(valueType(a op b)); \
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
        disassembleInstruction(vm.chunk, (int) (vm.ip - vm.chunk->code));
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
            case OP_EQUAL: {
                Value b = pop();
                Value a = pop();
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
                BINARY_OP(NUMBER_VAL, +);
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
                // Get the value on the stack, negate it and return it to the stack.
            case OP_NEGATE:
                if (!IS_NUMBER(peek(0))) {
                    runtimeError("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(NUMBER_VAL(-AS_NUMBER(pop())));
                // Optimisation (without benchmark) to update the stack top in place
                //  *(vm.stackTop - 1) = -*(vm.stackTop - 1);
                break;
            case OP_RETURN:
                printValue(pop());
                printf("\n");
                return INTERPRET_OK;
        }
    }


#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_CONSTANT_LONG
#undef BINARY_OP
}

InterpretResult interpret(const char *source) {
    // Create a chunk to store our resulting bytecode
    Chunk chunk;
    initChunk(&chunk);

    // Perform compilation, if successful, [chunk] will be populated with the corresponding bytecode.
    if (!compile(source, &chunk)) {
        freeChunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    // Update the virtual machine with our bytecode chunk
    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    // Execute the bytecode
    InterpretResult result = run();

    freeChunk(&chunk);
    return result;
}

void push(Value value) {
    // The stack top points to the next empty space.
    // Beware that the stack could be full...
    *vm.stackTop = value;
    vm.stackTop++;
}

Value pop() {
    vm.stackTop--;
    return *vm.stackTop;
}