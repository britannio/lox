# include <stdio.h>

#include "debug.h"
#include "value.h"

void disassembleChunk(Chunk *chunk, const char *name) {
    printf("== %s ==\n", name);

    for (int offset = 0; offset < chunk->count;) {
        offset = disassembleInstruction(chunk, offset);
    }
}

static int constantInstruction(const char *name, Chunk *chunk, int offset) {
    // The next item in the chunk's code array is the index of the constant in the chunk's constants array
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %9d '", name, constant);
    // Printing the index of the constant in the chunk alone isn't useful.
    // Here, we also print the value itself.
    printValue(chunk->constants.values[constant]);
    printf("'\n");
    // The beginning of the next instruction.
    return offset + 2;

}

static int constantInstructionLong(const char *name, Chunk *chunk, int offset) {
    // The next item in the chunk's code array is the index of the constant in the chunk's constants array
    uint32_t constant = (chunk->code[offset + 1] << 16) + (chunk->code[offset + 2] << 8) + (chunk->code[offset + 3]);
    printf("%-16s %9d '", name, constant);
    // Printing the index of the constant in the chunk alone isn't useful.
    // Here, we also print the value itself.
    printValue(chunk->constants.values[constant]);
    printf("'\n");
    // The beginning of the next instruction.
    return offset + 4;
}

static int simpleInstruction(const char *name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

int disassembleInstruction(Chunk *chunk, int offset) {
    // Print the offset of the instruction within the chunk code array
    printf("%04d ", offset);

    // Print the line number
    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
        // The previous instruction was on the same source line.
        printf("   | ");
    } else {
        printf("%4d ", chunk->lines[offset]);
    }

    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
        case OP_CONSTANT:
            return constantInstruction("OP_CONSTANT", chunk, offset);
        case OP_CONSTANT_LONG:
            return constantInstructionLong("OP_CONSTANT_LONG", chunk, offset);
        case OP_NIL:
            return simpleInstruction("OP_NIL", offset);
        case OP_TRUE:
            return simpleInstruction("OP_TRUE", offset);
        case OP_FALSE:
            return simpleInstruction("OP_FALSE", offset);
        case OP_EQUAL:
            return simpleInstruction("OP_EQUAL", offset);
        case OP_GREATER:
            return simpleInstruction("OP_GREATER", offset);
        case OP_LESS:
            return simpleInstruction("OP_LESS", offset);
        case OP_ADD:
            return simpleInstruction("OP_ADD", offset);
        case OP_SUBTRACT:
            return simpleInstruction("OP_SUBTRACT", offset);
        case OP_MULTIPLY:
            return simpleInstruction("OP_MULTIPLY", offset);
        case OP_DIVIDE:
            return simpleInstruction("OP_DIVIDE", offset);
        case OP_NOT:
            return simpleInstruction("OP_NOT", offset);
        case OP_NEGATE:
            return simpleInstruction("OP_NEGATE", offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}
