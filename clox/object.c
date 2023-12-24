
#include <stdio.h>
#include <string.h>

#include "object.h"
#include "table.h"
#include "value.h"
#include "memory.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType) \
    (type *)allocateObject(sizeof(type), objectType)

static Obj *allocateObject(size_t size, ObjType type) {
    Obj *object = (Obj *) reallocate(NULL, 0, size);
    object->type = type;
    // Insert the object at the head of the 'objects' linked list.
    object->next = vm.objects;
    vm.objects = object;
    return object;
}

static ObjString *allocateString(char *chars, int length, uint32_t hash) {
    //    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    // size of 'flexible array member'
    size_t size = sizeof(ObjString) + (length + 1) * sizeof(char);
    ObjString *string = (ObjString *) allocateObject(size, OBJ_STRING);
    string->length = length;
    string->hash = hash;
    tableSet(&vm.strings, string, NIL_VAL);
    strcpy(string->chars, chars);
    // The old string is no longer needed
    FREE_ARRAY(char, chars, length + 1);
    return string;
}

// FNV-1a hash function
static uint32_t hashString(const char *key, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; ++i) {
        hash ^= (uint8_t) key[i];
        hash *= 16777619;
    }
    return hash;
}

// Converts a C string into an ObjString, sharing the memory.
ObjString *takeString(char *chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm.strings, chars, length, hash);

    if (interned != NULL) {
        // We found an interned string with the same content
        // Free this extra array and use the interned string
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return allocateString(chars, length, hash);
}

ObjString *copyString(const char *chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm.strings, chars, length, hash);

    if (interned != NULL) return interned;

    char *heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(heapChars, length, hash);
}

void printObject(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
    }
}