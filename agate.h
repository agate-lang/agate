// SPDX-License-Identifier: MIT
// Copyright (c) 2013-2021 Robert Nystrom and Wren Contributors
// Copyright (c) 2022 Julien Bernard
#ifndef AGATE_H
#define AGATE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AGATE_VERSION_MAJOR 0
#define AGATE_VERSION_MINOR 1
#define AGATE_VERSION_PATCH 0

#define AGATE_VERSION_STRING "0.1.0"

// tag::vm[]
typedef struct AgateVM AgateVM;
// end::vm[]

typedef struct AgateHandle AgateHandle;

// tag::assert[]
typedef enum {
  AGATE_ASSERT_ABORT,
  AGATE_ASSERT_NIL,
  AGATE_ASSERT_NONE,
} AgateAssertHandling;
// end::assert[]

// tag::realloc[]
typedef void * (*AgateReallocFunc)(void *ptr, ptrdiff_t size, void *user_data);
// end::realloc[]

// tag::number_parsing[]
typedef bool (*AgateParseIntFunc)(const char *text, ptrdiff_t size, int base, int64_t *result);
typedef bool (*AgateParseFloatFunc)(const char *text, ptrdiff_t size, double *result);
// end::number_parsing[]

// tag::print[]
typedef void (*AgatePrintFunc)(AgateVM *vm, const char* text);
// end::print[]

// tag::error[]
typedef enum {
  AGATE_ERROR_COMPILE,
  AGATE_ERROR_RUNTIME,
  AGATE_ERROR_STACKTRACE,
} AgateErrorKind;

typedef void (*AgateErrorFunc)(AgateVM *vm, AgateErrorKind kind, const char *module_name, int line, const char *message);
// end::error[]

// tag::module[]
typedef const char *(*AgateModuleLoadFunc)(const char *name, void *user_data);
typedef void (*AgateModuleReleaseFunc)(const char *source, void *user_data);

typedef struct {
  AgateModuleLoadFunc load;
  AgateModuleReleaseFunc release;
  void *user_data;
} AgateModuleHandler;

typedef AgateModuleHandler (*AgateModuleHandlerFunc)(AgateVM *vm, const char *name);
// end::module[]

// tag::foreign_class[]
typedef ptrdiff_t (*AgateForeignAllocateFunc)(AgateVM *vm, const char *module_name, const char *class_name);
typedef void (*AgateForeignDestroyFunc)(AgateVM *vm, const char *module_name, const char *class_name, void *data);


typedef struct {
  AgateForeignAllocateFunc allocate;
  AgateForeignDestroyFunc destroy;
} AgateForeignClassHandler;

typedef AgateForeignClassHandler (*AgateForeignClassHandlerFunc)(AgateVM *vm, const char *module_name, const char *class_name);
// end::foreign_class[]

// tag::foreign_method[]
typedef void (*AgateForeignMethodFunc)(AgateVM *vm);

typedef enum {
  AGATE_FOREIGN_METHOD_INSTANCE,
  AGATE_FOREIGN_METHOD_CLASS,
} AgateForeignMethodKind;

typedef AgateForeignMethodFunc (*AgateForeignMethodHandlerFunc)(AgateVM *vm, const char *module_name, const char *class_name, AgateForeignMethodKind kind, const char *signature);
// end::foreign_method[]

// tag::config[]
typedef struct {
  AgateReallocFunc reallocate;

  AgateModuleHandlerFunc module_handler;

  AgateForeignClassHandlerFunc foreign_class_handler;
  AgateForeignMethodHandlerFunc foreign_method_handler;

  AgateParseIntFunc parse_int;
  AgateParseFloatFunc parse_float;

  AgateAssertHandling assert_handling;
  AgatePrintFunc print;
  AgateErrorFunc error;

  void *user_data;
} AgateConfig;
// end::config[]

// tag::status[]
typedef enum {
  AGATE_STATUS_OK,
  AGATE_STATUS_COMPILE_ERROR,
  AGATE_STATUS_RUNTIME_ERROR,
} AgateStatus;
// end::status[]

typedef enum {
  AGATE_TYPE_UNKOWN,
  AGATE_TYPE_ARRAY,
  AGATE_TYPE_BOOL,
  AGATE_TYPE_CHAR,
  AGATE_TYPE_FLOAT,
  AGATE_TYPE_FOREIGN,
  AGATE_TYPE_INT,
  AGATE_TYPE_MAP,
  AGATE_TYPE_NIL,
  AGATE_TYPE_STRING,
} AgateType;

// tag::initialize[]
void agateConfigInitialize(AgateConfig *config);
// end::initialize[]

// tag::vm_new[]
AgateVM *agateNewVM(const AgateConfig *config);
// end::vm_new[]
// tag::vm_delete[]
void agateDeleteVM(AgateVM *vm);
// end::vm_delete[]

// tag::interpret[]
AgateStatus agateInterpret(AgateVM *vm, const char *module, const char *source);
// end::interpret[]

AgateHandle *agateMakeCallHandle(AgateVM *vm, const char *signature);
AgateStatus agateCall(AgateVM *vm, AgateHandle *method);

ptrdiff_t agateSlotCount(AgateVM *vm);
void agateEnsureSlots(AgateVM *vm, ptrdiff_t slots_count);
AgateType agateSlotType(AgateVM *vm, ptrdiff_t slot);

bool agateSlotGetBool(AgateVM *vm, ptrdiff_t slot);
uint32_t agateSlotGetChar(AgateVM *vm, ptrdiff_t slot);
int64_t agateSlotGetInt(AgateVM *vm, ptrdiff_t slot);
double agateSlotGetFloat(AgateVM *vm, ptrdiff_t slot);
void *agateSlotGetForeign(AgateVM *vm, ptrdiff_t slot);
const char *agateSlotGetString(AgateVM *vm, ptrdiff_t slot);
AgateHandle *agateSlotGetHandle(AgateVM *vm, ptrdiff_t slot);

void agateReleaseHandle(AgateVM *vm, AgateHandle *handle);

void agateSlotSetNil(AgateVM *vm, ptrdiff_t slot);
void agateSlotSetBool(AgateVM *vm, ptrdiff_t slot, bool value);
void agateSlotSetChar(AgateVM *vm, ptrdiff_t slot, uint32_t value);
void agateSlotSetInt(AgateVM *vm, ptrdiff_t slot, int64_t value);
void agateSlotSetFloat(AgateVM *vm, ptrdiff_t slot, double value);

void agateSlotSetString(AgateVM *vm, ptrdiff_t slot, const char *text);
void agateSlotSetStringLength(AgateVM *vm, ptrdiff_t slot, const char *text, ptrdiff_t length);

void agateSlotSetHandle(AgateVM *vm, ptrdiff_t slot, AgateHandle *handle);

void agateSlotArrayNew(AgateVM *vm, ptrdiff_t slot);
ptrdiff_t agateSlotArraySize(AgateVM *vm, ptrdiff_t slot);
void agateSlotArrayGet(AgateVM *vm, ptrdiff_t array_slot, ptrdiff_t index, ptrdiff_t element_slot);
void agateSlotArraySet(AgateVM *vm, ptrdiff_t array_slot, ptrdiff_t index, ptrdiff_t element_slot);
void agateSlotArrayInsert(AgateVM *vm, ptrdiff_t array_slot, ptrdiff_t index, ptrdiff_t element_slot);
void agateSlotArrayErase(AgateVM *vm, ptrdiff_t array_slot, ptrdiff_t index, ptrdiff_t element_slot);

void agateSlotMapNew(AgateVM *vm, ptrdiff_t slot);
ptrdiff_t agateSlotMapCount(AgateVM *vm, ptrdiff_t slot);
bool agateSlotMapContains(AgateVM *vm, ptrdiff_t map_slot, ptrdiff_t key_slot);
void agateSlotMapGet(AgateVM *vm, ptrdiff_t map_slot, ptrdiff_t key_slot, ptrdiff_t value_slot);
void agateSlotMapSet(AgateVM *vm, ptrdiff_t map_slot, ptrdiff_t key_slot, ptrdiff_t value_slot);
void agateSlotMapErase(AgateVM *vm, ptrdiff_t map_slot, ptrdiff_t key_slot, ptrdiff_t value_slot);

bool agateHasModule(AgateVM *vm, const char *module_name);
bool agateHasVariable(AgateVM *vm, const char *module_name, const char *variable_name);
void agateGetVariable(AgateVM *vm, const char *module_name, const char *variable_name, ptrdiff_t slot);

void agateAbort(AgateVM *vm, ptrdiff_t slot);

void *agateGetUserData(AgateVM *vm);
void agateSetUserData(AgateVM *vm, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* AGATE_H */