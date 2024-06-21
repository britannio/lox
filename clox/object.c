
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
  object->isMarked = false;
  // Insert the object at the head of the 'objects' linked list.
  object->next = vm.objects;
  vm.objects = object;
#ifdef DEBUG_LOG_GC
  printf("%p allocate %zu for %d\n", (void *) object, size, type);
#endif
  return object;
}

ObjClass *newClass(ObjString *name) {
  ObjClass *klass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
  klass->name = name;
  return klass;
}

ObjClosure *newClosure(ObjFunction *function) {
  ObjUpvalue **upvalues = ALLOCATE(ObjUpvalue*, function->upvalueCount);

  for (int i = 0; i < function->upvalueCount; i++) {
    upvalues[i] = NULL;
  }

  ObjClosure *closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
  closure->function = function;
  closure->upvalues = upvalues;
  closure->upvalueCount = function->upvalueCount;
  return closure;
}

ObjFunction *newFunction() {
  // Create and initialise a function object
  ObjFunction *function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
  function->arity = 0;
  function->upvalueCount = 0;
  function->name = NULL;
  initChunk(&function->chunk);
  return function;
}

ObjInstance *newInstance(ObjClass *klass) {
  ObjInstance *instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
  instance->klass = klass;
  initTable(&instance->fields);
  return instance;
}

ObjNative *newNative(NativeFn function) {
  ObjNative *native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
  native->function = function;
  return native;
}

static ObjString *allocateString(char *chars, int length, uint32_t hash) {
  // size of 'flexible array member'
  size_t size = sizeof(ObjString) + (length + 1) * sizeof(char);
  Obj *obj = allocateObject(size, OBJ_STRING);
  ObjString *string = (ObjString *) obj;
  obj->hash = hash;
  string->length = length;
  strcpy(string->chars, chars);
  Value key = OBJ_VAL(string);
  push(key); // Make it GC safe
  tableSet(&vm.strings, key, NIL_VAL);
  pop();
  // The old string is no longer needed
  FREE_ARRAY(char, chars, length + 1);
  return string;
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

ObjUpvalue *newUpvalue(Value *slot) {
  ObjUpvalue *upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
  upvalue->closed = NIL_VAL;
  upvalue->location = slot;
  upvalue->next = NULL;
  return upvalue;
}

static void printFunction(ObjFunction *function) {
  if (function->name == NULL) {
    printf("<script>");
    return;
  }
  printf("<fn %s>", function->name->chars);
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
    case OBJ_CLASS:
      printf("%s", AS_CLASS(value)->name->chars);
      break;
    case OBJ_CLOSURE:
      printFunction(AS_CLOSURE(value)->function);
      break;
    case OBJ_FUNCTION:
      printFunction(AS_FUNCTION(value));
      break;
    case OBJ_INSTANCE:
      printf("%s instance", AS_INSTANCE(value)->klass->name->chars);
      break;
    case OBJ_NATIVE:
      printf("<native fn>");
      break;
    case OBJ_STRING:
      printf("%s", AS_CSTRING(value));
      break;
    case OBJ_UPVALUE:
      printf("upvalue");
      break;
  }
}