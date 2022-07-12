#include "maps.h"

#include <assert.h>
#include <string.h>

static ptrdiff_t agateTestMapsForeignAllocate(AgateVM *vm, const char *unit_name, const char *class_name) {
  return sizeof(double);
}

AgateForeignClassHandler agateTestMapsForeignClassHandler(const char *class_name) {
  AgateForeignClassHandler handler = { agateTestMapsForeignAllocate, NULL };
  return handler;
}

static void mapsNewMap(AgateVM *vm) {
  agateSlotMapNew(vm, 0);
}

static void mapsInsert(AgateVM *vm) {
  agateSlotMapNew(vm, 0);

  agateEnsureSlots(vm, 3);

  // Insert String
  agateSlotSetString(vm, 1, "England");
  agateSlotSetString(vm, 2, "London");
  agateSlotMapSet(vm, 0, 1, 2);

  // Insert Double
  agateSlotSetFloat(vm, 1, 1.0);
  agateSlotSetFloat(vm, 2, 42.0);
  agateSlotMapSet(vm, 0, 1, 2);

  // Insert Boolean
  agateSlotSetBool(vm, 1, false);
  agateSlotSetBool(vm, 2, true);
  agateSlotMapSet(vm, 0, 1, 2);

  // Insert Null
  agateSlotSetNil(vm, 1);
  agateSlotSetNil(vm, 2);
  agateSlotMapSet(vm, 0, 1, 2);

  // Insert List
  agateSlotSetString(vm, 1, "Empty");
  agateSlotArrayNew(vm, 2);
  agateSlotMapSet(vm, 0, 1, 2);
}

static void mapsContains2(AgateVM *vm) {
  agateSlotSetBool(vm, 0, agateSlotMapContains(vm, 1, 2));
}

static void mapsContains(AgateVM *vm) {
  mapsInsert(vm);

  agateEnsureSlots(vm, 2);
  agateSlotSetString(vm, 1, "England");

  agateSlotSetBool(vm, 0, agateSlotMapContains(vm, 0, 1));
}

static void mapsContainsFalse(AgateVM *vm) {
  mapsInsert(vm);

  agateEnsureSlots(vm, 2);
  agateSlotSetString(vm, 1, "UnknownKey");

  agateSlotSetBool(vm, 0, agateSlotMapContains(vm, 0, 1));
}

static void mapsSize(AgateVM *vm) {
  mapsInsert(vm);

  agateSlotSetInt(vm, 0, agateSlotMapSize(vm, 0));
}

static void mapsSize1(AgateVM *vm) {
  agateSlotSetInt(vm, 0, agateSlotMapSize(vm, 1));
}

static void mapsErase(AgateVM *vm) {
  agateEnsureSlots(vm, 3);
  agateSlotSetString(vm, 2, "key");
  agateSlotMapErase(vm, 1, 2, 0);
}

AgateForeignMethodFunc agateTestMapsForeignMethodHandler(const char *class_name, AgateForeignMethodKind kind, const char *signature) {
  assert(strcmp(class_name, "Maps") == 0);
  assert(kind == AGATE_FOREIGN_METHOD_CLASS);

  if (strcmp(signature, "new_map()") == 0) {
    return mapsNewMap;
  }

  if (strcmp(signature, "insert()") == 0) {
    return mapsInsert;
  }

  if (strcmp(signature, "contains(_,_)") == 0) {
    return mapsContains2;
  }

  if (strcmp(signature, "contains()") == 0) {
    return mapsContains;
  }

  if (strcmp(signature, "contains_false()") == 0) {
    return mapsContainsFalse;
  }

  if (strcmp(signature, "size()") == 0) {
    return mapsSize;
  }

  if (strcmp(signature, "size(_)") == 0) {
    return mapsSize1;
  }

  if (strcmp(signature, "erase(_)") == 0) {
    return mapsErase;
  }

  return NULL;
}
