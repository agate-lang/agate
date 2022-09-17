#include "foreign_class.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * ForeignClass
 */

static int g_finalized = 0;

static void foreignClassFinalized(AgateVM *vm) {
  ptrdiff_t return_slot = agateSlotForReturn(vm);
  agateSlotSetInt(vm, return_slot, g_finalized);
}

/*
 * Counter
 */

static ptrdiff_t agateTestCounterForeignAllocate(AgateVM *vm, const char *unit_name, const char *class_name) {
  return sizeof(double);
}

static void counterIncrement(AgateVM *vm) {
  ptrdiff_t counter_slot = agateSlotForArg(vm, 0);
  double *value = agateSlotGetForeign(vm, counter_slot);

  ptrdiff_t increment_slot = agateSlotForArg(vm, 1);
  double increment = agateSlotGetFloat(vm, increment_slot);

  *value += increment;
}

static void counterValue(AgateVM *vm) {
  ptrdiff_t counter_slot = agateSlotForArg(vm, 0);
  double *value = agateSlotGetForeign(vm, counter_slot);

  ptrdiff_t return_slot = agateSlotForReturn(vm);
  agateSlotSetFloat(vm, return_slot, *value);
}

/*
 * Point
 */

struct Point {
  double x;
  double y;
  double z;
};

static ptrdiff_t agateTestPointForeignAllocate(AgateVM *vm, const char *unit_name, const char *class_name) {
  return sizeof(struct Point);
}

static void pointTranslate(AgateVM *vm) {
  ptrdiff_t point_slot = agateSlotForArg(vm, 0);
  struct Point *point = agateSlotGetForeign(vm, point_slot);

  point->x += agateSlotGetFloat(vm, agateSlotForArg(vm, 1));
  point->y += agateSlotGetFloat(vm, agateSlotForArg(vm, 2));
  point->z += agateSlotGetFloat(vm, agateSlotForArg(vm, 3));
}

static void pointToS(AgateVM *vm) {
  ptrdiff_t point_slot = agateSlotForArg(vm, 0);
  struct Point *point = agateSlotGetForeign(vm, point_slot);

#define AGATE_TEST_POINT_BUFFER_SIZE 128
  char buffer[AGATE_TEST_POINT_BUFFER_SIZE];
  snprintf(buffer, AGATE_TEST_POINT_BUFFER_SIZE, "(%g, %g, %g)", point->x, point->y, point->z);

  ptrdiff_t return_slot = agateSlotForReturn(vm);
  agateSlotSetString(vm, return_slot, buffer);
}

/*
 * Resource
 */

static ptrdiff_t agateTestResourceForeignAllocate(AgateVM *vm, const char *unit_name, const char *class_name) {
  return sizeof(int);
}

void agateTestResourceForeignDestroy(AgateVM *vm, const char *unit_name, const char *class_name, void *data) {
  int *value = data;

  if (*value != 123) {
//     exit(EXIT_FAILURE);
  }

  ++g_finalized;
}

/*
 * API
 */

AgateForeignClassHandler agateTestForeignClassForeignClassHandler(const char *class_name) {
  if (strcmp(class_name, "Counter") == 0) {
    AgateForeignClassHandler handler = { agateTestCounterForeignAllocate, NULL };
    return handler;
  }

  if (strcmp(class_name, "Point") == 0) {
    AgateForeignClassHandler handler = { agateTestPointForeignAllocate, NULL };
    return handler;
  }

  if (strcmp(class_name, "Resource") == 0) {
    AgateForeignClassHandler handler = { agateTestResourceForeignAllocate, agateTestResourceForeignDestroy };
    return handler;
  }

  AgateForeignClassHandler handler = { NULL, NULL };
  return handler;
}


AgateForeignMethodFunc agateTestForeignClassForeignMethodHandler(const char *class_name, AgateForeignMethodKind kind, const char *signature) {
  if (strcmp(class_name, "ForeignClass") == 0) {
    assert(kind == AGATE_FOREIGN_METHOD_CLASS);

    if (strcmp(signature, "finalized") == 0) {
      return foreignClassFinalized;
    }
  }

  if (strcmp(class_name, "Counter") == 0) {
    assert(kind == AGATE_FOREIGN_METHOD_INSTANCE);

    if (strcmp(signature, "increment(_)") == 0) {
      return counterIncrement;
    }

    if (strcmp(signature, "value") == 0) {
      return counterValue;
    }
  }

  if (strcmp(class_name, "Point") == 0) {
    assert(kind == AGATE_FOREIGN_METHOD_INSTANCE);

    if (strcmp(signature, "translate(_,_,_)") == 0) {
      return pointTranslate;
    }

    if (strcmp(signature, "to_s") == 0) {
      return pointToS;
    }
  }

  return NULL;
}
