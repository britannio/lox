#include "memory.h"
#include "vm.h"

#include <stdlib.h>
#include <string.h>

void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
  if (newSize == 0) {
    free(pointer);
    return NULL;
  }

  void *result = realloc(pointer, newSize);
  // Out of memory, exit here rather than letting the program run loose and
  // crash elsewhere.
  if (result == NULL) exit(1);
  return result;
}

static void freeObject(Obj *object) {
  switch (object->type) {
    case OBJ_FUNCTION: {
      ObjFunction *function = (ObjFunction *) object;
      freeChunk(&function->chunk);
      FREE(ObjFunction, object);
      break;
    }
    case OBJ_NATIVE: {
      FREE(ObjNative, object);
      break;
    }
    case OBJ_STRING: {
      ObjString *string = (ObjString *) object;
//            FREE_ARRAY(char, string->chars, string->length + 1);
      // sizeof(ObjString) doesn't take the length of the char array into account.
      size_t stringObjectSize = sizeof(ObjString) + (string->length + 1) * sizeof(char);
//            FREE(ObjString, object);
      reallocate(object, stringObjectSize, 0);
      break;
    }
  }
}

void freeObjects() {
  // Walk through the linked list of objects and free each one
  Obj *object = vm.objects;
  while (object != NULL) {
    Obj *next = object->next;
    freeObject(object);
    object = next;
  }
}


void initArray(Array *array, size_t type) {
  array->capacity = 0;
  array->count = 0;
  array->values = NULL;
  array->type = type;
}

void writeArray(Array *array, void *value) {
  // Resize the array if it is full/uninitialised
  if (array->capacity < array->count + 1) {
    int oldCapacity = array->capacity;
    array->capacity = GROW_CAPACITY(oldCapacity);
    array->values = GROW_ARRAY_TYPE_SIZE(array->type, array->values, oldCapacity, array->capacity);
  }

  // Set the value and increment the index of the next item
  memcpy(&((uint8_t *) array->values)[array->count * array->type], value, array->type);
  array->count++;
}

void freeArray(Array *array) {
  FREE_ARRAY(uint8_t, array->values, array->capacity);
  initArray(array, array->type);
}
