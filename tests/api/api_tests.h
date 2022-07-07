#ifndef AGATE_TESTS_API_TESTS_H
#define AGATE_TESTS_API_TESTS_H

#include <agate.h>

bool agateTestIsApi(const char *path);

AgateForeignClassHandler agateTestForeignClassHandler(AgateVM *vm, const char *unit_name, const char *class_name);
AgateForeignMethodFunc agateTestForeignMethodHandler(AgateVM *vm, const char *unit_name, const char *class_name, AgateForeignMethodKind kind, const char *signature);

bool agateTestRunNative(AgateVM *vm, const char *path);

#endif // AGATE_TESTS_API_TESTS_H
