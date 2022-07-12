#include "arrays.h"

#include <assert.h>
#include <string.h>

static void arraysNewArray(AgateVM *vm) {
  agateSlotArrayNew(vm, 0);
}

static inline void arraysHelperInsertFloat(AgateVM *vm, ptrdiff_t index, double value) {
  agateSlotSetFloat(vm, 1, value);
  agateSlotArrayInsert(vm, 0, index, 1);
}

static inline void arraysHelperAppendFloat(AgateVM *vm, double value) {
  agateSlotSetFloat(vm, 1, value);
  agateSlotArrayInsert(vm, 0, -1, 1);
}

static void arraysInsert(AgateVM *vm) {
  agateSlotArrayNew(vm, 0);
  agateEnsureSlots(vm, 2);

  // appending
  arraysHelperInsertFloat(vm, 0, 1.0);
  arraysHelperInsertFloat(vm, 1, 2.0);
  arraysHelperInsertFloat(vm, 2, 3.0);

  // inserting
  arraysHelperInsertFloat(vm, 0, 4.0);
  arraysHelperInsertFloat(vm, 1, 5.0);
  arraysHelperInsertFloat(vm, 2, 6.0);

  // negative indices
  arraysHelperInsertFloat(vm, -1, 7.0);
  arraysHelperInsertFloat(vm, -2, 8.0);
  arraysHelperInsertFloat(vm, -3, 9.0);
}

static void arraysSet(AgateVM *vm) {
  agateSlotArrayNew(vm, 0);
  agateEnsureSlots(vm, 2);

  arraysHelperAppendFloat(vm, 1.0);
  arraysHelperAppendFloat(vm, 2.0);
  arraysHelperAppendFloat(vm, 3.0);
  arraysHelperAppendFloat(vm, 4.0);

  // array[2] = 33
  agateSlotSetFloat(vm, 1, 33);
  agateSlotArraySet(vm, 0, 2, 1);

  // array[-1] = 44
  agateSlotSetFloat(vm, 1, 44);
  agateSlotArraySet(vm, 0, -1, 1);
}

static void arraysGet(AgateVM *vm) {
  agateSlotArrayGet(vm, 1, agateSlotGetInt(vm, 2), 0);
}

AgateForeignMethodFunc agateTestArraysForeignMethodHandler(const char *class_name, AgateForeignMethodKind kind, const char *signature) {
  assert(strcmp(class_name, "Arrays") == 0);
  assert(kind == AGATE_FOREIGN_METHOD_CLASS);

  if (strcmp(signature, "new_array()") == 0) {
    return arraysNewArray;
  }

  if (strcmp(signature, "insert()") == 0) {
    return arraysInsert;
  }

  if (strcmp(signature, "set()") == 0) {
    return arraysSet;
  }

  if (strcmp(signature, "get(_,_)") == 0) {
    return arraysGet;
  }

  return NULL;
}
