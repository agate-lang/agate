#ifndef AGATE_TESTS_API_CALL_FOREIGN_H
#define AGATE_TESTS_API_CALL_FOREIGN_H

#include <agate.h>

AgateForeignMethodFunc agateTestCallForeignForeignMethodHandler(const char *class_name, AgateForeignMethodKind kind, const char *signature);
bool agateTestCallForeignRunNative(AgateVM *vm);

#endif // AGATE_TESTS_API_CALL_FOREIGN_H
