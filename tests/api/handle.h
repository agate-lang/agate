#ifndef AGATE_TESTS_API_HANDLE_H
#define AGATE_TESTS_API_HANDLE_H

#include <agate.h>

AgateForeignMethodFunc agateTestHandleForeignMethodHandler(const char *class_name, AgateForeignMethodKind kind, const char *signature);

#endif // AGATE_TESTS_API_HANDLE_H
