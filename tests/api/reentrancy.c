#include "reentrancy.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void reentrancyCall(AgateVM *vm) {
  printf("reentrancyCall Begin\n");
  assert(agateSlotType(vm, 1) == AGATE_TYPE_STRING);
  AgateHandle *param_handle = agateSlotGetHandle(vm, 1);
  AgateHandle *print_handle = agateMakeCallHandle(vm, "print(_)");

  agateEnsureSlots(vm, 2);
  assert(agateHasVariable(vm, "tests/api/reentrancy.agate", "Reentrancy"));
  agateGetVariable(vm, "tests/api/reentrancy.agate", "Reentrancy", 0);
  agateSlotSetHandle(vm, 1, param_handle);
  agateCall(vm, print_handle);

  agateReleaseHandle(vm, print_handle);
  agateReleaseHandle(vm, param_handle);
  printf("reentrancyCall End\n");
}

static void reentrancyInterpret(AgateVM *vm) {
  printf("reentrancyInterpret Begin\n");
  agateInterpret(vm, "tests/api/reentrancy.agate", "Reentrancy.print(\"Interpret\\n\")\n");
  printf("reentrancyInterpret End\n");
}

AgateForeignMethodFunc agateTestReentrancyForeignMethodHandler(const char *class_name, AgateForeignMethodKind kind, const char *signature) {
  assert(strcmp(class_name, "Reentrancy") == 0);
  assert(kind == AGATE_FOREIGN_METHOD_CLASS);

  if (strcmp(signature, "call(_)") == 0) {
    return reentrancyCall;
  }

  if (strcmp(signature, "interpret()") == 0) {
    return reentrancyInterpret;
  }

  return NULL;
}
