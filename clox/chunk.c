#include <stdio.h>

#include "chunk.h"
#include "memory.h"


void initChunk(Chunk *chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    initValueArray(&chunk->constants);
}

void freeChunk(Chunk *chunk) {
    // Free the memory in the code array
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lines, chunk->capacity);
    // Free the constants
    freeValueArray(&chunk->constants);
    // Reset the state of the chunk
    initChunk(chunk);
}

// Append a byte to the provided chunk
void writeChunk(Chunk *chunk, uint8_t byte, int line) {
    // Grow the array if it is full
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
        chunk->lines = GROW_ARRAY(int, chunk->lines, oldCapacity, chunk->capacity);
    }

    // Write the byte to the array
    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

int addConstant(Chunk *chunk, Value value) {
    writeValueArray(&chunk->constants, value);
    return chunk->constants.count - 1;
}

void writeConstant(Chunk *chunk, Value value, int line) {
    int constIndex = addConstant(chunk, value);
    // OP_CONSTANT uses a single byte to store to its operand.
    // This limits the number of constants it can reference to 256 (0-255).
    // OP_CONSTANT_LONG resolves this by using a 24 bit operand.
    bool isShortOp = constIndex <= 255;
    if (isShortOp) {
        writeChunk(chunk, OP_CONSTANT, line);
        writeChunk(chunk, constIndex, line);
    } else {
        writeChunk(chunk, OP_CONSTANT_LONG, line);
        uint8_t byte1 = constIndex >> 16;
        uint8_t byte2 = constIndex >> 8;
        uint8_t byte3 = constIndex;
        writeChunk(chunk, byte1, line);
        writeChunk(chunk, byte2, line);
        writeChunk(chunk, byte3, line);
    }
}

