#ifndef AGATE_TESTS_API_FOREIGN_CLASS_H
#define AGATE_TESTS_API_FOREIGN_CLASS_H

#include <agate.h>

AgateForeignClassHandler agateTestForeignClassForeignClassHandler(const char *class_name);
AgateForeignMethodFunc agateTestForeignClassForeignMethodHandler(const char *class_name, AgateForeignMethodKind kind, const char *signature);

#endif // AGATE_TESTS_API_FOREIGN_CLASS_H
