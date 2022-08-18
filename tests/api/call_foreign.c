#include "call_foreign.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void callForeignApi(AgateVM *vm) {
  ptrdiff_t array_slot = agateSlotForReturn(vm);
  agateSlotArrayNew(vm, array_slot);

  for (int64_t i = 1; i < 10; ++i) {
    ptrdiff_t slot = agateSlotAllocate(vm);
    agateSlotSetInt(vm, slot, i);
    agateSlotArrayInsert(vm, array_slot, -1, slot);
  }
}

AgateForeignMethodFunc agateTestCallForeignForeignMethodHandler(const char *class_name, AgateForeignMethodKind kind, const char *signature) {
  assert(strcmp(class_name, "CallForeign") == 0);
  assert(kind == AGATE_FOREIGN_METHOD_CLASS);

  if (strcmp(signature, "api()") == 0) {
    return callForeignApi;
  }

  assert(false);
  return NULL;
}

bool agateTestCallForeignRunNative(AgateVM *vm) {
  agateStackStart(vm);
  ptrdiff_t call_slot = agateSlotAllocate(vm);
  agateGetVariable(vm, "tests/api/call_foreign.agate", "CallForeign", call_slot);

  AgateHandle *call_foreign_class_handle = agateSlotGetHandle(vm, call_slot);
  agateStackFinish(vm);

  AgateHandle *call_handle = agateMakeCallHandle(vm, "call(_)");

  agateStackStart(vm);
  ptrdiff_t arg0 = agateSlotAllocate(vm);
  agateSlotSetHandle(vm, arg0, call_foreign_class_handle);
  ptrdiff_t arg1 = agateSlotAllocate(vm);
  agateSlotSetString(vm, arg1, "parameter");

  printf("slots before %td\n", agateSlotCount(vm));
  agateCallHandle(vm, call_handle);
  printf("slots after %td\n", agateSlotCount(vm));

  agateStackFinish(vm);

  agateReleaseHandle(vm, call_handle);
  agateReleaseHandle(vm, call_foreign_class_handle);

  return true;
}
