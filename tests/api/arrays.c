#include "arrays.h"

#include <assert.h>
#include <string.h>

static void arraysNewArray(AgateVM *vm) {
  agateSlotArrayNew(vm, agateSlotForReturn(vm));
}

static inline void arraysHelperInsertFloat(AgateVM *vm, ptrdiff_t array_slot, ptrdiff_t index, ptrdiff_t value_slot, double value) {
  agateSlotSetFloat(vm, value_slot, value);
  agateSlotArrayInsert(vm, array_slot, index, value_slot);
}

static inline void arraysHelperAppendFloat(AgateVM *vm, ptrdiff_t array_slot, ptrdiff_t value_slot, double value) {
  agateSlotSetFloat(vm, value_slot, value);
  agateSlotArrayInsert(vm, array_slot, -1, value_slot);
}

static void arraysInsert(AgateVM *vm) {
  ptrdiff_t array_slot = agateSlotForReturn(vm);
  agateSlotArrayNew(vm, array_slot);
  ptrdiff_t value_slot = agateSlotAllocate(vm);

  // appending
  arraysHelperInsertFloat(vm, array_slot, 0, value_slot, 1.0);
  arraysHelperInsertFloat(vm, array_slot, 1, value_slot, 2.0);
  arraysHelperInsertFloat(vm, array_slot, 2, value_slot, 3.0);

  // inserting
  arraysHelperInsertFloat(vm, array_slot, 0, value_slot, 4.0);
  arraysHelperInsertFloat(vm, array_slot, 1, value_slot, 5.0);
  arraysHelperInsertFloat(vm, array_slot, 2, value_slot, 6.0);

  // negative indices
  arraysHelperInsertFloat(vm, array_slot, -1, value_slot, 7.0);
  arraysHelperInsertFloat(vm, array_slot, -2, value_slot, 8.0);
  arraysHelperInsertFloat(vm, array_slot, -3, value_slot, 9.0);
}

static void arraysSet(AgateVM *vm) {
  ptrdiff_t array_slot = agateSlotForReturn(vm);
  agateSlotArrayNew(vm, array_slot);
  ptrdiff_t value_slot = agateSlotAllocate(vm);

  arraysHelperAppendFloat(vm, array_slot, value_slot, 1.0);
  arraysHelperAppendFloat(vm, array_slot, value_slot, 2.0);
  arraysHelperAppendFloat(vm, array_slot, value_slot, 3.0);
  arraysHelperAppendFloat(vm, array_slot, value_slot, 4.0);

  // array[2] = 33
  agateSlotSetFloat(vm, value_slot, 33);
  agateSlotArraySet(vm, array_slot, 2, value_slot);

  // array[-1] = 44
  agateSlotSetFloat(vm, value_slot, 44);
  agateSlotArraySet(vm, array_slot, -1, value_slot);
}

static void arraysGet(AgateVM *vm) {
  ptrdiff_t index = agateSlotGetInt(vm, agateSlotForArg(vm, 2));
  agateSlotArrayGet(vm, agateSlotForArg(vm, 1), index, agateSlotForReturn(vm));
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
