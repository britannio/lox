#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"



// modding by a power of two is equivalent to performing a logical AND with
// the number - 1 which is significantly faster.
#define MOD_POW2(input, mod) ((input) & ((mod) - 1))

typedef enum {
  ABSENT = 0, PRESENT, TOMBSTONE
} EntryState;

typedef struct {
  Value key;
  Value value;
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

bool tableGet(Table *table, Value key, Value *value);

bool tableSet(Table *table, Value key, Value value);

bool tableDelete(Table *table, Value key);

void tableAddAll(Table *from, Table *to);

ObjString *tableFindString(const Table *table, const char *chars, int length, uint32_t hash);

void tableRemoveWhite(Table *table);

void markTable(Table *table);

EntryState tableEntryState(Entry *entry);

#endif //clox_table_h
