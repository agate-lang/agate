#ifndef AGATE_SUPPORT_H
#define AGATE_SUPPORT_H

#include <agate.h>

// unit handling

#define AGATE_SUPPORT_INCLUDE_PATH_MAX 8
bool agateExUnitAddIncludePath(AgateVM *vm, const char *path);
AgateUnitHandler agateExUnitHandler(AgateVM *vm, const char *name);

const char *agateExUnitLoad(AgateVM *vm, const char *unit_name);
void agateExUnitRelease(AgateVM *vm, const char *unit_text);

// foreign class

void agateExForeignClassAddHandler(AgateVM *vm, AgateForeignClassHandlerFunc func, const char *unit_name);
AgateForeignClassHandler agateExForeignClassHandler(AgateVM *vm, const char *unit_name, const char *class_name);

// foreign method

void agateExForeignMethodAddHandler(AgateVM *vm, AgateForeignMethodHandlerFunc func, const char *unit_name);
AgateForeignMethodFunc agateExForeignMethodHandler(AgateVM *vm, const char *unit_name, const char *class_name, AgateForeignMethodKind kind, const char *signature);

// agate function replacements

AgateVM *agateExNewVM(const AgateConfig *config);
void agateExDeleteVM(AgateVM *vm);

void *agateExGetUserData(AgateVM *vm);
void agateExSetUserData(AgateVM *vm, void *user_data);

#endif // AGATE_SUPPORT_H
