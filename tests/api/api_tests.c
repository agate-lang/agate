#include "api_tests.h"

#include <string.h>

#include "handle.h"

bool agateTestIsApi(const char *path) {
  return strncmp(path, "tests/api", 9) == 0;
}

AgateForeignClassHandler agateTestForeignClassHandler(AgateVM *vm, const char *unit_name, const char *class_name) {
  AgateForeignClassHandler handler = { NULL, NULL };

  return handler;
}

AgateForeignMethodFunc agateTestForeignMethodHandler(AgateVM *vm, const char *unit_name, const char *class_name, AgateForeignMethodKind kind, const char *signature) {
  if (strcmp(unit_name, "tests/api/handle.agate") == 0) {
    return agateTestHandleForeignMethodHandler(class_name, kind, signature);
  }

  return NULL;
}

bool agateTestRunNative(AgateVM *vm, const char *path) {
  return true;
}
