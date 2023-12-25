
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
    // size of 'flexible array member'
    size_t size = sizeof(ObjString) + (length + 1) * sizeof(char);
    Obj *obj = allocateObject(size, OBJ_STRING);
    ObjString *string = (ObjString *) obj;
    obj->hash = hash;
    string->length = length;
    strcpy(string->chars, chars);
    Value *key = stringToValue(string);
    tableSet(&vm.strings, key, NIL_VAL);
    // The old string is no longer needed
    FREE_ARRAY(char, chars, length + 1);
    return string;
}

Value* stringToValue(ObjString* str) {
    Value value = OBJ_VAL(str);
    // TODO fix this memory leak...
    Value *ptr = ALLOCATE(Value, 1);
    memcpy(ptr, &value, sizeof(Value));
    return ptr;
}

static uint32_t hashString(const char *key, int length) {
    return hashByteArray((uint8_t *) key, length);
}

// Converts a C string into an ObjString, sharing the memory.
ObjString *takeString(char *chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString *interned = tableFindString(&vm.strings, chars, length, hash);

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
    ObjString *interned = tableFindString(&vm.strings, chars, length, hash);

    // If the string has already been interned then use it
    if (interned != NULL) return interned;

    // Otherwise, create a copy, leaving space for the null terminating character
    // Note that we don't free these interned strings.
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