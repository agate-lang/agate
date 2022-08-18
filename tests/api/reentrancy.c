#include "reentrancy.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void reentrancyCallHandle(AgateVM *vm) {
  ptrdiff_t param_slot = agateSlotForArg(vm, 1);
  assert(agateSlotType(vm, param_slot) == AGATE_TYPE_STRING);
  AgateHandle *param_handle = agateSlotGetHandle(vm, param_slot);
  AgateHandle *print_handle = agateMakeCallHandle(vm, "print(_)");

  agateStackStart(vm);
  ptrdiff_t arg0 = agateSlotAllocate(vm);
  assert(agateHasVariable(vm, "tests/api/reentrancy.agate", "Reentrancy"));
  agateGetVariable(vm, "tests/api/reentrancy.agate", "Reentrancy", arg0);

  ptrdiff_t arg1 = agateSlotAllocate(vm);
  agateSlotSetHandle(vm, arg1, param_handle);

  agateCallHandle(vm, print_handle);
  agateStackFinish(vm);

  agateReleaseHandle(vm, print_handle);
  agateReleaseHandle(vm, param_handle);
}

static void reentrancyCallString(AgateVM *vm) {
  agateCallString(vm, "tests/api/reentrancy.agate", "Reentrancy.print(\"String\\n\")\n");
}

AgateForeignMethodFunc agateTestReentrancyForeignMethodHandler(const char *class_name, AgateForeignMethodKind kind, const char *signature) {
  assert(strcmp(class_name, "Reentrancy") == 0);
  assert(kind == AGATE_FOREIGN_METHOD_CLASS);

  if (strcmp(signature, "call_handle(_)") == 0) {
    return reentrancyCallHandle;
  }

  if (strcmp(signature, "call_string()") == 0) {
    return reentrancyCallString;
  }

  return NULL;
}
