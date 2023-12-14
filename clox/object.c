#include "object.h"

#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "value.h"
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

static ObjString *allocateString(char *chars, int length) {
    //    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    // size of 'flexible array member'
    size_t size = sizeof(ObjString) + (length + 1) * sizeof(char);
    ObjString *string = (ObjString *) allocateObject(size, OBJ_STRING);
    string->length = length;
    strcpy(string->chars, chars);
    return string;
}

// Converts a C string into an ObjString, sharing the memory.
ObjString *takeString(char *chars, int length) {
    return allocateString(chars, length);
}

ObjString *copyString(const char *chars, int length) {
    char *heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(heapChars, length);
}

void printObject(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
    }
}