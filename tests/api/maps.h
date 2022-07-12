#ifndef AGATE_TESTS_API_MAPS_H
#define AGATE_TESTS_API_MAPS_H

#include <agate.h>

AgateForeignClassHandler agateTestMapsForeignClassHandler(const char *class_name);
AgateForeignMethodFunc agateTestMapsForeignMethodHandler(const char *class_name, AgateForeignMethodKind kind, const char *signature);

#endif // AGATE_TESTS_API_MAPS_H
