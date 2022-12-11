#include "arrays.h"

#include <assert.h>
#include <string.h>

static void tuplesSize(AgateVM *vm) {
  ptrdiff_t size = agateSlotTupleSize(vm, 1);
  agateSlotSetInt(vm, AGATE_RETURN_SLOT, size);
}

static void tuplesGet(AgateVM *vm) {
  ptrdiff_t index = agateSlotGetInt(vm, 2);
  agateSlotTupleGet(vm, 1, index, AGATE_RETURN_SLOT);
}

AgateForeignMethodFunc agateTestTuplesForeignMethodHandler(const char *class_name, AgateForeignMethodKind kind, const char *signature) {
  assert(strcmp(class_name, "Tuples") == 0);
  assert(kind == AGATE_FOREIGN_METHOD_CLASS);

  if (strcmp(signature, "size(_)") == 0) {
    return tuplesSize;
  }

  if (strcmp(signature, "get(_,_)") == 0) {
    return tuplesGet;
  }

  return NULL;
}
