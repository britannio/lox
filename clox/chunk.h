#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

// Each bytecode instruction will have a one byte operation code (opcode).
typedef enum {
    OP_CONSTANT,
    OP_CONSTANT_LONG,
    // Return from the current function
    OP_RETURN,
} OpCode;

// Data stored alongside an instruction
typedef struct {
    int count;
    int capacity;
    uint8_t *code;
    int *lines; // Mirrors the code array in size but stores the source line number corresponding to each op-code.
    ValueArray constants; // A pool of constants
} Chunk;

void initChunk(Chunk *chunk);

void freeChunk(Chunk *chunk);

void writeChunk(Chunk *chunk, uint8_t byte, int line);

int addConstant(Chunk *chunk, Value value);

void writeConstant(Chunk* chunk, Value value, int line);

#endif