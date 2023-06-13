#include <stdio.h>
#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"


void vmChallenge1(Chunk *chunk) {
//    writeConstant(chunk, 3, 123);
//    writeConstant(chunk, 2, 123);
//    writeChunk(chunk, OP_SUBTRACT, 123);
//    writeConstant(chunk, 1, 123);
//    writeChunk(chunk, OP_SUBTRACT, 123);
//    writeChunk(chunk, OP_RETURN, 123);


    // 1 + 2 * 3 - 4 / -5
    // 1 + (2 * 3) - (4 / (-5))
    writeConstant(chunk, 2, 123);
    writeConstant(chunk, 3, 123);
    writeChunk(chunk, OP_MULTIPLY, 123);

    writeConstant(chunk, 1, 123);
    writeChunk(chunk, OP_ADD, 123);

    writeConstant(chunk, 4, 123);
    writeConstant(chunk, 5, 123);
    writeChunk(chunk, OP_NEGATE, 123);
    writeChunk(chunk, OP_DIVIDE, 123);

    writeChunk(chunk, OP_SUBTRACT, 123);
    writeChunk(chunk, OP_RETURN, 123);

}

int main(int argc, const char *argv[]) {
    initVM();

    Chunk chunk;
    initChunk(&chunk);

    // Indexes in the constant pool
//    int constant = addConstant(&chunk, 1.2);
    // The chunk array looks like [OP_CONSTANT, 0] where 0 is the index of the constant in question
//    writeConstant(&chunk, 1.2, 123);
//    writeConstant(&chunk, 3.4, 123);
//
//    writeChunk(&chunk, OP_ADD, 123);
//    writeConstant(&chunk, 5.6, 123);
//
//    writeChunk(&chunk, OP_DIVIDE, 123);
//    writeChunk(&chunk, OP_NEGATE, 123);
//
//    writeChunk(&chunk, OP_RETURN, 123);

    vmChallenge1(&chunk);

//    disassembleChunk(&chunk, "test chunk");
    interpret(&chunk);
    freeVM();
    freeChunk(&chunk);
    return 0;
}
