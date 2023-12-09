#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

static void repl() {
    // Max number of characters to evaluate at once
    char line[1024];
    // REPL = Read-Eval-Print-Loop
    // Here is the loop
    for (;;) {
        printf("> ");

        // fgets will read 1024 characters (we need space for the null character) and store it in {line}.
        if (!fgets(line, sizeof(line), stdin)) {
            // We failed to read the line
            printf("/n");
            break;
        }

        interpret(line);
    }

}

static char *readFile(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        // Exit if the file cannot be open (doesn't exist, insufficient perms etc)
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    // To read a file, we need its size so enough memory can be allocated to store it.
    // But knowing the size of a file requires it to be read.
    // Fortunately, we can just 'seek' to the end of the file and use ftell() to get this info.
    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    // Is that casting a pointer to a char pointer?
    // What if malloc fails?
    char *buffer = (char *) malloc(fileSize + 1);
    if (buffer == NULL) {
        // Not enough memory to store the file...
        fprintf(stderr, "Not enough memory to read file \"%s\".\n", path);
        exit(74);
    }
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) {
        // We failed to read the whole file.
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }
    buffer[bytesRead] = '\0';

    fclose(file);
    return buffer;
}

static void runFile(const char *path) {
    // Reads a file then executes the code inside it
    char *source = readFile(path);
    InterpretResult result = interpret(source);
    free(source);

    if (result == INTERPRET_COMPILE_ERROR) exit(65);
    if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

int main(int argc, const char *argv[]) {
    // argc is the number of arguments?
    initVM();

    if (argc == 1) {
        repl();
    } else if (argc == 2) {
        runFile(argv[1]);
    } else {
        fprintf(stderr, "Usage: clox [path]\n");
        // 64?
        exit(64);
    }


    freeVM();
    return 0;
}
