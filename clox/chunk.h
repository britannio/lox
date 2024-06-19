#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

// TODO: What is the motivation behind supporting multiple chunks?
// Possibly to give each function its own chunk

// Each bytecode instruction will have a one byte operation code (opcode).
typedef enum {
  OP_CONSTANT,
  OP_CONSTANT_LONG,
  OP_NIL,
  OP_TRUE,
  OP_FALSE,
  OP_POP,
  OP_GET_LOCAL,
  OP_SET_LOCAL,
  OP_GET_GLOBAL,
  OP_DEFINE_GLOBAL,
  OP_SET_GLOBAL,
  OP_GET_UPVALUE,
  OP_SET_UPVALUE,
  OP_EQUAL,
  // Preserves the first expression on the stack
  OP_EQUAL_PRESERVE,
  OP_GREATER,
  OP_LESS,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_NOT,
  OP_NEGATE,
  OP_PRINT,
  OP_JUMP,
  OP_JUMP_IF_FALSE,
  OP_LOOP,
  OP_CALL,
  OP_CLOSURE,
  OP_CLOSE_UPVALUE,
  // Return from the current function
  OP_RETURN,
} OpCode;

// Data stored alongside an instruction
typedef struct {
  int count;
  int capacity;
  uint8_t *code;
  int *lines; // Mirrors the code array in size but stores the source line
  // number corresponding to each op-code.
  ValueArray constants; // A pool of constants
} Chunk;

void initChunk(Chunk *chunk);

void freeChunk(Chunk *chunk);

void writeChunk(Chunk *chunk, uint8_t byte, int line);

int addConstant(Chunk *chunk, Value value);

void writeConstant(Chunk *chunk, Value value, int line);

#endif
