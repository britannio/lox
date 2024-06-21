#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

void initTable(Table *table) {
  table->count = 0;
  table->capacity = 0;
  table->entries = NULL;
}

void freeTable(Table *table) {
  FREE_ARRAY(Entry, table->entries, table->capacity);
  initTable(table);
}

static Entry *findEntry(Entry *entries, int capacity, Value key) {
  if (IS_NIL(key)) return NULL;
  uint32_t index = hashValue(key) % capacity;
  Entry *tombstone = NULL;
  for (;;) {
    // Get the entry
    Entry *entry = &entries[index];
    switch (tableEntryState(entry)) {
      case PRESENT:
        if (valuesEqual(entry->key, key)) return entry;
        break;
      case ABSENT:
        // Useful when we're searching for a slot to use in `tableSet()`
        return tombstone != NULL ? tombstone : entry;
      case TOMBSTONE:
        // Store the first tombstone we encounter
        if (tombstone == NULL) tombstone = entry;
        break;
    }

    // Move to the next slot/bucket
    index = (index + 1) % capacity;
  }

}

bool tableGet(Table *table, Value key, Value *value) {
  if (table->count == 0) return false;
  Entry *entry = findEntry(table->entries, table->capacity, key);
  if (entry == NULL) return false;
  EntryState state = tableEntryState(entry);
  if (state != PRESENT) return false;

  *value = entry->value;
  return true;
}


static void adjustCapacity(Table *table, int capacity) {
  Entry *entries = ALLOCATE(Entry, capacity);
  for (int i = 0; i < capacity; i++) {
    entries[i].key = NIL_VAL;
    entries[i].value = NIL_VAL;
  }

  // While tombstones go towards count, they are not transferred to the new table
  table->count = 0;
  for (int i = 0; i < table->capacity; i++) {
    Entry *entry = &table->entries[i];
    if (tableEntryState(entry) != PRESENT) continue;

    Entry *dest = findEntry(entries, capacity, entry->key);
    dest->key = entry->key;
    dest->value = entry->value;
    table->count++;
  }

  // The old array is no longer needed.
  FREE_ARRAY(Entry, table->entries, table->capacity);
  table->entries = entries;
  table->capacity = capacity;
}

bool tableSet(Table *table, Value key, Value value) {
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    // If this new entry does not fit then grow the table
    int capacity = GROW_CAPACITY(table->capacity);
    adjustCapacity(table, capacity);
  }

  Entry *entry = findEntry(table->entries, table->capacity, key);
  EntryState state = tableEntryState(entry);
  bool isNewKey = state == ABSENT;
  // tombstones have already been factored into table->count, so we only increment the
  // count if the entry is not a tombstone entry.
  if (isNewKey) table->count++;

  entry->key = key;
  entry->value = value;

  return isNewKey;
}

bool tableDelete(Table *table, Value key) {
  if (table->count == 0) return false;

  // Find the entry
  Entry *entry = findEntry(table->entries, table->capacity, key);
  if (entry == NULL || tableEntryState(entry) != PRESENT) return false;
//  if (entry->key == NULL) return false;

  // Place a tombstone in the entry
  entry->key = NIL_VAL;
  entry->value = BOOL_VAL(true);
  return true;
}


void tableAddAll(Table *from, Table *to) {
  for (int i = 0; i < from->capacity; i++) {
    Entry *entry = &from->entries[i];
    if (tableEntryState(entry) == PRESENT) {
      tableSet(to, entry->key, entry->value);
    }
  }
}

ObjString *tableFindString(const Table *table, const char *chars,
                           int length, uint32_t hash) {
  if (table->count == 0) return NULL;

  uint32_t index = hash % table->capacity;
  for (;;) {
    Entry *entry = &table->entries[index];
    switch (tableEntryState(entry)) {
      case ABSENT:
        return NULL;
      case PRESENT: {
        Value key = entry->key;
        // Assume that the table uses string keys!
        ObjString *keyStr = AS_STRING(key);
        if (keyStr->length == length && keyStr->obj.hash == hash &&
            memcmp(keyStr->chars, chars, length) == 0) {
          // we found a slot with a matching ptrKey
          return keyStr;
        }
      }
      case TOMBSTONE:
        break;
    }

    // Move to the next slot
    index = (index + 1) % table->capacity;
  }
}

void tableRemoveWhite(Table *table) {
  for (int i = 0; i < table->capacity; i++) {
    Entry *entry = &table->entries[i];
    if (entry->key.type != VAL_NIL && !entry->key.as.obj->isMarked) {
      tableDelete(table, entry->key);
    }
  }
}

void markTable(Table *table) {
  for (int i = 0; i < table->capacity; i++) {
    Entry *entry = &table->entries[i];
//    markObject((Obj *) entry->key);
    markValue(entry->key);
    markValue(entry->value);
  }
}

EntryState tableEntryState(Entry *entry) {
  // Is this the right thing to do?
  if (entry == NULL) return ABSENT;

  if (entry->key.type == VAL_NIL) {
    // Could be absent or a tombstone depending on the value
    if (entry->value.type == VAL_NIL) {
      return ABSENT;
    } else {
      return TOMBSTONE;
    }
  } else {
    return PRESENT;
  }

}

