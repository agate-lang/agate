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
  agateSlotSetInt(vm, AGATE_RETURN_SLOT, g_finalized);
}

/*
 * Counter
 */

static ptrdiff_t agateTestCounterForeignAllocate(AgateVM *vm, const char *unit_name, const char *class_name) {
  return sizeof(double);
}

static void counterNew(AgateVM *vm) {
  double *value = agateSlotGetForeign(vm, 0);
  *value = 0.0;
}

static void counterIncrement(AgateVM *vm) {
  double *value = agateSlotGetForeign(vm, 0);
  double increment = agateSlotGetFloat(vm, 1);
  *value += increment;
}

static void counterValue(AgateVM *vm) {
  double *value = agateSlotGetForeign(vm, 0);
  agateSlotSetFloat(vm, AGATE_RETURN_SLOT, *value);
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

static void pointConstruct(AgateVM *vm) {
  struct Point *point = agateSlotGetForeign(vm, 0);

  point->x = agateSlotGetFloat(vm, 1);
  point->y = agateSlotGetFloat(vm, 2);
  point->z = agateSlotGetFloat(vm, 3);
}

static void pointTranslate(AgateVM *vm) {
  struct Point *point = agateSlotGetForeign(vm, 0);

  point->x += agateSlotGetFloat(vm, 1);
  point->y += agateSlotGetFloat(vm, 2);
  point->z += agateSlotGetFloat(vm, 3);
}

static void pointToS(AgateVM *vm) {
  struct Point *point = agateSlotGetForeign(vm, 0);

#define AGATE_TEST_POINT_BUFFER_SIZE 128
  char buffer[AGATE_TEST_POINT_BUFFER_SIZE];
  snprintf(buffer, AGATE_TEST_POINT_BUFFER_SIZE, "(%g, %g, %g)", point->x, point->y, point->z);

  agateSlotSetString(vm, AGATE_RETURN_SLOT, buffer);
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
    exit(EXIT_FAILURE);
  }

  ++g_finalized;
}

static void resourceNew(AgateVM *vm) {
  int *value = agateSlotGetForeign(vm, 0);
  *value = 123;
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

    if (strcmp(signature, "init new()") == 0) {
      return counterNew;
    }

    if (strcmp(signature, "increment(_)") == 0) {
      return counterIncrement;
    }

    if (strcmp(signature, "value") == 0) {
      return counterValue;
    }
  }

  if (strcmp(class_name, "Point") == 0) {
    assert(kind == AGATE_FOREIGN_METHOD_INSTANCE);

    if (strcmp(signature, "__construct(_,_,_)") == 0) {
      return pointConstruct;
    }

    if (strcmp(signature, "translate(_,_,_)") == 0) {
      return pointTranslate;
    }

    if (strcmp(signature, "to_s") == 0) {
      return pointToS;
    }
  }

  if (strcmp(class_name, "Resource") == 0) {
    assert(kind == AGATE_FOREIGN_METHOD_INSTANCE);

    if (strcmp(signature, "init new()") == 0) {
      return resourceNew;
    }
  }

  return NULL;
}
