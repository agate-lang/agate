#ifndef AGATE_TESTS_REENTRANCY_H
#define AGATE_TESTS_REENTRANCY_H

#include <agate.h>

AgateForeignMethodFunc agateTestReentrancyForeignMethodHandler(const char *class_name, AgateForeignMethodKind kind, const char *signature);

#endif // AGATE_TESTS_REENTRANCY_H
