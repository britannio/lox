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
    int constant = addConstant(&chunk, 1.2);
    for (int i = 0; i < 255; i++) addConstant(&chunk, i);

    // The chunk array looks like [OP_CONSTANT, 0] where 0 is the index of the constant in question
    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, constant, 123);
    writeConstant(&chunk, 32, 124);
    writeChunk(&chunk, OP_RETURN, 126);

    disassembleChunk(&chunk, "test chunk");
    interpret(&chunk);
    freeVM();
    freeChunk(&chunk);
    return 0;
}

