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

static ptrdiff_t mapsPopulate(AgateVM *vm) {
  ptrdiff_t map_slot = agateSlotAllocate(vm);
  agateSlotMapNew(vm, map_slot);

  ptrdiff_t key_slot = agateSlotAllocate(vm);
  ptrdiff_t value_slot = agateSlotAllocate(vm);

  // Insert String
  agateSlotSetString(vm, key_slot, "England");
  agateSlotSetString(vm, value_slot, "London");
  agateSlotMapSet(vm, map_slot, key_slot, value_slot);

  // Insert Double
  agateSlotSetFloat(vm, key_slot, 1.0);
  agateSlotSetFloat(vm, value_slot, 42.0);
  agateSlotMapSet(vm, map_slot, key_slot, value_slot);

  // Insert Boolean
  agateSlotSetBool(vm, key_slot, false);
  agateSlotSetBool(vm, value_slot, true);
  agateSlotMapSet(vm, map_slot, key_slot, value_slot);

  // Insert Nil
  agateSlotSetNil(vm, key_slot);
  agateSlotSetNil(vm, value_slot);
  agateSlotMapSet(vm, map_slot, key_slot, value_slot);

  // Insert List
  agateSlotSetString(vm, key_slot, "Empty");
  agateSlotArrayNew(vm, value_slot);
  agateSlotMapSet(vm, map_slot, key_slot, value_slot);

  return map_slot;
}

static void mapsNewMap(AgateVM *vm) {
  agateSlotMapNew(vm, AGATE_RETURN_SLOT);
}

static void mapsInsert(AgateVM *vm) {
  ptrdiff_t map_slot = mapsPopulate(vm);
  agateSlotCopy(vm, AGATE_RETURN_SLOT, map_slot);
}

static void mapsContains2(AgateVM *vm) {
  bool result = agateSlotMapContains(vm, 1, 2);
  agateSlotSetBool(vm, AGATE_RETURN_SLOT, result);
}

static void mapsContains(AgateVM *vm) {
  ptrdiff_t map_slot = mapsPopulate(vm);

  ptrdiff_t key_slot = agateSlotAllocate(vm);
  agateSlotSetString(vm, key_slot, "England");

  bool result = agateSlotMapContains(vm, map_slot, key_slot);
  agateSlotSetBool(vm, AGATE_RETURN_SLOT, result);
}

static void mapsContainsFalse(AgateVM *vm) {
  ptrdiff_t map_slot = mapsPopulate(vm);

  ptrdiff_t key_slot = agateSlotAllocate(vm);
  agateSlotSetString(vm, key_slot, "UnknownKey");

  bool result = agateSlotMapContains(vm, map_slot, key_slot);
  agateSlotSetBool(vm, AGATE_RETURN_SLOT, result);
}

static void mapsSize(AgateVM *vm) {
  ptrdiff_t map_slot = mapsPopulate(vm);
  agateSlotSetInt(vm, AGATE_RETURN_SLOT, agateSlotMapSize(vm, map_slot));
}

static void mapsSize1(AgateVM *vm) {
  agateSlotSetInt(vm, AGATE_RETURN_SLOT, agateSlotMapSize(vm, 1));
}

static void mapsErase(AgateVM *vm) {
  ptrdiff_t key_slot = agateSlotAllocate(vm);
  agateSlotSetString(vm, key_slot, "key");
  agateSlotMapErase(vm, 1, key_slot, AGATE_RETURN_SLOT);
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
