#include <stdio.h>
#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"


int main(int argc, const char *argv[]) {
    initVM();

    Chunk chunk;
    initChunk(&chunk);

    // Indexes in the constant pool
//    int constant = addConstant(&chunk, 1.2);
    // The chunk array looks like [OP_CONSTANT, 0] where 0 is the index of the constant in question
    writeConstant(&chunk, 1.2, 123);
    writeConstant(&chunk, 3.4, 123);

    writeChunk(&chunk, OP_ADD, 123);
    writeConstant(&chunk, 5.6, 123);

    writeChunk(&chunk, OP_DIVIDE, 123);
    writeChunk(&chunk, OP_NEGATE, 123);

    writeChunk(&chunk, OP_RETURN, 123);

//    disassembleChunk(&chunk, "test chunk");
    interpret(&chunk);
    freeVM();
    freeChunk(&chunk);
    return 0;
}

