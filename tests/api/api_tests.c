#include "api_tests.h"

#include <string.h>

#include "arrays.h"
#include "call.h"
#include "call_foreign.h"
#include "handle.h"
#include "maps.h"
#include "slots.h"

bool agateTestUseForeign(const char *path) {
  return strncmp(path, "tests/api", 9) == 0;
}

AgateForeignClassHandler agateTestForeignClassHandler(AgateVM *vm, const char *unit_name, const char *class_name) {
  AgateForeignClassHandler handler = { NULL, NULL };

  if (strcmp(unit_name, "tests/api/maps.agate") == 0) {
    return agateTestMapsForeignClassHandler(class_name);
  }

  if (strcmp(unit_name, "tests/api/slots.agate") == 0) {
    return agateTestSlotsForeignClassHandler(class_name);
  }

  return handler;
}

AgateForeignMethodFunc agateTestForeignMethodHandler(AgateVM *vm, const char *unit_name, const char *class_name, AgateForeignMethodKind kind, const char *signature) {
  if (strcmp(unit_name, "tests/api/arrays.agate") == 0) {
    return agateTestArraysForeignMethodHandler(class_name, kind, signature);
  }

  if (strcmp(unit_name, "tests/api/call_foreign.agate") == 0) {
    return agateTestCallForeignForeignMethodHandler(class_name, kind, signature);
  }

  if (strcmp(unit_name, "tests/api/handle.agate") == 0) {
    return agateTestHandleForeignMethodHandler(class_name, kind, signature);
  }

  if (strcmp(unit_name, "tests/api/maps.agate") == 0) {
    return agateTestMapsForeignMethodHandler(class_name, kind, signature);
  }

  if (strcmp(unit_name, "tests/api/slots.agate") == 0) {
    return agateTestSlotsForeignMethodHandler(class_name, kind, signature);
  }

  return NULL;
}

bool agateTestRunNative(AgateVM *vm, const char *unit_name) {
  if (strcmp(unit_name, "tests/api/call.agate") == 0) {
    return agateTestCallRunNative(vm);
  }

  if (strcmp(unit_name, "tests/api/call_foreign.agate") == 0) {
    return agateTestCallForeignRunNative(vm);
  }

  return true;
}
