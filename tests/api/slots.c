#include "slots.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

static ptrdiff_t agateTestSlotsForeignAllocate(AgateVM *vm, const char *unit_name, const char *class_name) {
  return sizeof(double);
}

AgateForeignClassHandler agateTestSlotsForeignClassHandler(const char *class_name) {
  AgateForeignClassHandler handler = { agateTestSlotsForeignAllocate, NULL };
  return handler;
}

static void slotsNoSet(AgateVM *vm) {
  // nothing to do
}

static void slotsGetSlots(AgateVM *vm) {
  bool result = true;

  if (agateSlotGetBool(vm, 1) != true) {
    result = false;
  }

  ptrdiff_t size;
  const char *string = agateSlotGetStringSize(vm, 2, &size);

  if (size != 5) {
    result = false;
  }

  if (memcmp(string, "by\0te", size) != 0) {
    result = false;
  }

  if (agateSlotGetFloat(vm, 3) != 1.5) {
    result = false;
  }

  if (strcmp(agateSlotGetString(vm, 4), "str") != 0) {
    result = false;
  }

  AgateHandle * handle = agateSlotGetHandle(vm, 5);

  if (result) {
    agateSlotSetHandle(vm, 0, handle);
  } else {
    agateSlotSetBool(vm, 0, false);
  }

  agateReleaseHandle(vm, handle);
}

static void slotsSetSlots(AgateVM *vm) {
  AgateHandle * handle = agateSlotGetHandle(vm, 1);

  agateSlotSetBool(vm, 1, true);
  agateSlotSetStringSize(vm, 2, "by\0te", 5);
  agateSlotSetFloat(vm, 3, 1.5);
  agateSlotSetString(vm, 4, "str");
  agateSlotSetNil(vm, 5);

  bool result = true;

  if (agateSlotGetBool(vm, 1) != true) {
    result = false;
  }

  ptrdiff_t size;
  const char *string = agateSlotGetStringSize(vm, 2, &size);

  if (size != 5) {
    result = false;
  }

  if (memcmp(string, "by\0te", size) != 0) {
    result = false;
  }

  if (agateSlotGetFloat(vm, 3) != 1.5) {
    result = false;
  }

  if (strcmp(agateSlotGetString(vm, 4), "str") != 0) {
    result = false;
  }

  if (agateSlotType(vm, 5) != AGATE_TYPE_NIL) {
    result = false;
  }


  if (result) {
    agateSlotSetHandle(vm, 0, handle);
  } else {
    agateSlotSetBool(vm, 0, false);
  }

  agateReleaseHandle(vm, handle);
}

static void slotsSlotTypes(AgateVM *vm) {
  bool result = agateSlotType(vm, 1) == AGATE_TYPE_BOOL
      && agateSlotType(vm, 2) == AGATE_TYPE_FOREIGN
      && agateSlotType(vm, 3) == AGATE_TYPE_ARRAY
      && agateSlotType(vm, 4) == AGATE_TYPE_MAP
      && agateSlotType(vm, 5) == AGATE_TYPE_NIL
      && agateSlotType(vm, 6) == AGATE_TYPE_FLOAT
      && agateSlotType(vm, 7) == AGATE_TYPE_STRING
      && agateSlotType(vm, 8) == AGATE_TYPE_UNKNOWN;

  agateSlotSetBool(vm, 0, result);
}

static void slotsEnsure(AgateVM *vm) {
  ptrdiff_t before = agateSlotCount(vm);
  agateEnsureSlots(vm, 20);
  ptrdiff_t after = agateSlotCount(vm);

  for (int64_t i = 0; i < 20; ++i) {
    agateSlotSetInt(vm, i, i);
  }

  int64_t sum = 0;

  for (int64_t i = 0; i < 20; ++i) {
    sum += agateSlotGetInt(vm, i);
  }

  char result[128];
  snprintf(result, sizeof(result), "%td -> %td (%" PRIi64 ")", before, after, sum);
  agateSlotSetString(vm, 0, result);
}


static void slotsEnsureOutsideForeign(AgateVM *vm) {
  AgateConfig config;
  agateConfigInitialize(&config);
  AgateVM *other_vm = agateNewVM(&config);


  ptrdiff_t before = agateSlotCount(other_vm);
  agateEnsureSlots(other_vm, 20);
  ptrdiff_t after = agateSlotCount(other_vm);

  for (int64_t i = 0; i < 20; ++i) {
    agateSlotSetInt(other_vm, i, i);
  }

  int64_t sum = 0;

  for (int64_t i = 0; i < 20; ++i) {
    sum += agateSlotGetInt(other_vm, i);
  }

  agateDeleteVM(other_vm);

  char result[128];
  snprintf(result, sizeof(result), "%td -> %td (%" PRIi64 ")", before, after, sum);
  agateSlotSetString(vm, 0, result);
}

static void slotsGetArraySize(AgateVM *vm) {
  agateSlotSetInt(vm, 0, agateSlotArraySize(vm, 1));
}

static void slotsGetArrayElement(AgateVM *vm) {
  int64_t index = agateSlotGetInt(vm, 2);
  agateSlotArrayGet(vm, 1, index, 0);
}

static void slotsGetMapSize(AgateVM *vm) {
  agateSlotSetInt(vm, 0, agateSlotMapSize(vm, 1));
}

static void slotsGetMapValue(AgateVM *vm) {
  agateSlotMapGet(vm, 1, 2, 0);
}

AgateForeignMethodFunc agateTestSlotsForeignMethodHandler(const char *class_name, AgateForeignMethodKind kind, const char *signature) {
  assert(strcmp(class_name, "Slots") == 0);
  assert(kind == AGATE_FOREIGN_METHOD_CLASS);

  if (strcmp(signature, "no_set") == 0) {
    return slotsNoSet;
  }

  if (strcmp(signature, "get_slots(_,_,_,_,_)") == 0) {
    return slotsGetSlots;
  }

  if (strcmp(signature, "set_slots(_,_,_,_,_)") == 0) {
    return slotsSetSlots;
  }

  if (strcmp(signature, "slot_types(_,_,_,_,_,_,_,_)") == 0) {
    return slotsSlotTypes;
  }

  if (strcmp(signature, "ensure()") == 0) {
    return slotsEnsure;
  }

  if (strcmp(signature, "ensure_outside_foreign()") == 0) {
    return slotsEnsureOutsideForeign;
  }

  if (strcmp(signature, "get_array_size(_)") == 0) {
    return slotsGetArraySize;
  }

  if (strcmp(signature, "get_array_element(_,_)") == 0) {
    return slotsGetArrayElement;
  }

  if (strcmp(signature, "get_map_size(_)") == 0) {
    return slotsGetMapSize;
  }

  if (strcmp(signature, "get_map_value(_,_)") == 0) {
    return slotsGetMapValue;
  }

  assert(false);
  return NULL;
}
