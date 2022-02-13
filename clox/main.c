#include "common.h"
#include "chunk.h"
#include "debug.h"


int main(int argc, const char *argv[]) {
    Chunk chunk;
    initChunk(&chunk);

    int constant = addConstant(&chunk, 1.2);
    // The chunk array looks like [OP_CONSTANT, 0] where 0 is the index of the constant in question
    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, constant, 123);

    writeChunk(&chunk, OP_RETURN, 123);
    disassembleChunk(&chunk, "test chunk");
    freeChunk(&chunk);
    return 0;
}

