#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"


typedef enum {
  ABSENT = 0, PRESENT, TOMBSTONE
} EntryState;

typedef struct {
  Value key;
  Value value;
  EntryState state;
} Entry;

typedef struct {
  int count;
  int capacity;
//    ValueType keyType;
  // array of entries
  Entry *entries;
} Table;

void initTable(Table *table);

void freeTable(Table *table);

bool tableGet(Table *table, Value *key, Value *value);

bool tableSet(Table *table, Value key, Value value);

bool tableDelete(Table *table, Value *key);

void tableAddAll(Table *from, Table *to);

ObjString *tableFindString(const Table *table, const char *chars, int length, uint32_t hash);

#endif //clox_table_h
