#include "handle.h"

#include <assert.h>
#include <string.h>

static AgateHandle *handle = NULL;

static void handleValueGetter(AgateVM *vm) {
  if (handle != NULL) {
    agateSlotSetHandle(vm, AGATE_RETURN_SLOT, handle);
    agateReleaseHandle(vm, handle);
    handle = NULL;
  }
}

static void handleValueSetter(AgateVM *vm) {
  if (handle != NULL) {
    agateReleaseHandle(vm, handle);
  }

  handle = agateSlotGetHandle(vm, 1);
}

AgateForeignMethodFunc agateTestHandleForeignMethodHandler(const char *class_name, AgateForeignMethodKind kind, const char *signature) {
  assert(strcmp(class_name, "Handle") == 0);
  assert(kind == AGATE_FOREIGN_METHOD_CLASS);

  if (strcmp(signature, "value") == 0) {
    return handleValueGetter;
  }

  if (strcmp(signature, "value=(_)") == 0) {
    return handleValueSetter;
  }

  assert(false);
  return NULL;
}
