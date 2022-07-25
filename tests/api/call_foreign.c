#include "call_foreign.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void callForeignApi(AgateVM *vm) {
  agateEnsureSlots(vm, 10);
  agateSlotArrayNew(vm, 0);

  for (int64_t i = 1; i < 10; ++i) {
    agateSlotSetInt(vm, i, i);
    agateSlotArrayInsert(vm, 0, -1, i);
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
  agateEnsureSlots(vm, 1);
  agateGetVariable(vm, "tests/api/call_foreign.agate", "CallForeign", 0);

  AgateHandle *call_foreign_class_handle = agateSlotGetHandle(vm, 0);
  AgateHandle *call_handle = agateMakeCallHandle(vm, "call(_)");

  agateEnsureSlots(vm, 2);
  agateSlotSetHandle(vm, 0, call_foreign_class_handle);
  agateSlotSetString(vm, 1, "parameter");

  printf("slots before %td\n", agateSlotCount(vm));
  agateCall(vm, call_handle);
  printf("slots after %td\n", agateSlotCount(vm));

  agateReleaseHandle(vm, call_handle);
  agateReleaseHandle(vm, call_foreign_class_handle);

  return true;
}
