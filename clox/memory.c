#include "memory.h"
#include "vm.h"
#include "compiler.h"

#ifdef DEBUG_LOG_GC

#include <stdio.h>
#include "debug.h"

#endif

#include <stdlib.h>
#include <string.h>

void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
  vm.bytesAllocated += newSize - oldSize;
  if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
    // Perform garbage collection whenever we get more memory
    collectGarbage();
#endif
    if (vm.bytesAllocated > vm.nextGC) {
      collectGarbage();
    }
  }
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

void markObject(Obj *object) {
  if (object == NULL) return;
  // Objects can be cyclic, this avoids marking a marked object forever
  if (object->isMarked) return;
#ifdef DEBUG_LOG_GC
  printf("%p mark ", (void *) object);
  printValue(OBJ_VAL(object));
  printf("\n");
#endif

  object->isMarked = true;

  // Grow vm.grayStack array if necessary
  if (vm.grayCapacity < vm.grayCount + 1) {
    vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
    // We use realloc here instead of our reallocate because reallocate could
    // trigger a recursive GC
    vm.grayStack = (Obj **) realloc(vm.grayStack, sizeof(Obj *) * vm.grayCapacity);

    // We don't have enough memory to contine GC
    if (vm.grayStack == NULL) exit(1);
  }

  vm.grayStack[vm.grayCount++] = object;
}

void markValue(Value value) {
  if (IS_OBJ(value)) markObject(AS_OBJ(value));
}

static void markArray(ValueArray *array) {
  for (int i = 0; i < array->count; ++i) {
    markValue(array->values[i]);
  }
}

static void blackenObject(Obj *object) {
#ifdef DEBUG_LOG_GC
  printf("%p blacken ", (void *) object);
  printValue(OBJ_VAL(object));
  printf("\n");
#endif

  switch (object->type) {
    case OBJ_BOUND_METHOD: {
      ObjBoundMethod *bound = (ObjBoundMethod *) object;
      markValue(bound->receiver);
      markObject((Obj *) bound->method);
      break;
    }
    case OBJ_CLASS: {
      ObjClass *klass = (ObjClass *) object;
      markObject((Obj *) klass->name);
      markTable(&klass->methods);
      break;
    }
    case OBJ_CLOSURE: {
      ObjClosure *closure = (ObjClosure *) object;
      markObject((Obj *) closure->function);
      for (int i = 0; i < closure->upvalueCount; ++i) {
        markObject((Obj *) closure->upvalues[i]);
      }
      break;
    }
    case OBJ_FUNCTION: {
      ObjFunction *function = (ObjFunction *) object;
      markObject((Obj *) function->name);
      markArray(&function->chunk.constants);
      break;
    }
    case OBJ_INSTANCE: {
      ObjInstance *instance = (ObjInstance *) object;
      markObject((Obj *) instance->klass);
      markTable(&instance->fields);
      break;
    }
    case OBJ_UPVALUE:
      markValue(((ObjUpvalue *) object)->closed);
    case OBJ_NATIVE:
    case OBJ_STRING:
      break;
  }
}

static void freeObject(Obj *object) {

#ifdef DEBUG_LOG_GC
  printf("%p free type %d\n", (void *) object, object->type);
#endif

  switch (object->type) {
    case OBJ_BOUND_METHOD: {
      FREE(ObjBoundMethod, object);
      break;
    }
    case OBJ_CLASS: {
      ObjClass *klass = (ObjClass *) object;
      freeTable(&klass->methods);
      FREE(ObjClass, object);
      break;
    }
    case OBJ_CLOSURE: {
      ObjClosure *closure = (ObjClosure *) object;
      FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
      // The closure doesn't own the function.
      // For example, there may be many closures pointing to the same function.
      // Which is why we don't free the function here too.
      FREE(ObjClosure, object);
      break;
    }
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
    case OBJ_INSTANCE: {
      ObjInstance *instance = (ObjInstance *) object;
      freeTable(&instance->fields);
      FREE(ObjInstance, object);
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
    case OBJ_UPVALUE:
      FREE(ObjUpvalue, object);
      break;
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

  free(vm.grayStack);
}

static void markRoots() {
  // Mark every stack value
  for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
    markValue(*slot);
  }

  // Mark the closure of the call frame
  for (int i = 0; i < vm.frameCount; i++) {
    markObject((Obj *) vm.frames[i].closure);
  }

  // Mark the open upvalue list
  for (ObjUpvalue *upvalue = vm.openUpvalues; upvalue != NULL; upvalue = upvalue->next) {
    markObject((Obj *) upvalue);
  }

  markTable(&vm.globals);
  markCompilerRoots();
  markObject((Obj *) vm.initString);
}

static void traceReferences() {
  while (vm.grayCount > 0) {
    // Pop a gray object from the stack and blacken it by marking all objects
    // it references as gray.
    Obj *object = vm.grayStack[--vm.grayCount];
    blackenObject(object);
  }
}

static void sweep() {
  Obj *previous = NULL;
  // All objects can be found in this linked list
  // We free memory by finding objects in here that weren't marked and getting
  // rid of them
  Obj *object = vm.objects;
  while (object != NULL) {
    if (object->isMarked) {
      // Clear it for the next GC run
      object->isMarked = false;
      // Keep it and move to the next one
      previous = object;
      object = object->next;
    } else {
      Obj *unreached = object;
      object = object->next;
      if (previous != NULL) {
        // Removes the original object from the linked list
        previous->next = object;
      } else {
        // previous == NULL so this is the first element of the linked list
        vm.objects = object;
      }

      freeObject(unreached);
    }
  }
}

void collectGarbage() {
#ifdef DEBUG_LOG_GC
  printf("-- gc begin\n");
  size_t before = vm.bytesAllocated;
#endif

  markRoots();
  traceReferences();
  tableRemoveWhite(&vm.strings);
  sweep();

  vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
  printf("-- gc end\n");
  printf("   collected %zu bytes (from %zu to %zu) next at %zu\n",
         before - vm.bytesAllocated, before, vm.bytesAllocated,
         vm.nextGC);
#endif

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
