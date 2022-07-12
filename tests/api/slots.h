#ifndef AGATE_TESTS_API_SLOTS_H
#define AGATE_TESTS_API_SLOTS_H

#include <agate.h>

AgateForeignClassHandler agateTestSlotsForeignClassHandler(const char *class_name);
AgateForeignMethodFunc agateTestSlotsForeignMethodHandler(const char *class_name, AgateForeignMethodKind kind, const char *signature);

#endif // AGATE_TESTS_API_SLOTS_H
