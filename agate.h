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

// tag::write[]
typedef void (*AgateWriteFunc)(AgateVM *vm, uint8_t byte);
// end::write[]

// tag::error[]
typedef enum {
  AGATE_ERROR_COMPILE,
  AGATE_ERROR_RUNTIME,
  AGATE_ERROR_STACKTRACE,
} AgateErrorKind;

typedef void (*AgateErrorFunc)(AgateVM *vm, AgateErrorKind kind, const char *unit_name, int line, const char *message);
// end::error[]

// tag::input[]
typedef void (*AgateInputFunc)(AgateVM *vm, char *buffer, size_t size);
// end::input[]

// tag::unit_handler[]
typedef const char *(*AgateUnitLoadFunc)(const char *name, void *user_data);
typedef void (*AgateUnitReleaseFunc)(const char *source, void *user_data);

typedef struct {
  AgateUnitLoadFunc load;
  AgateUnitReleaseFunc release;
  void *user_data;
} AgateUnitHandler;

typedef AgateUnitHandler (*AgateUnitHandlerFunc)(AgateVM *vm, const char *name);
// end::unit_handler[]

// tag::foreign_class_handler[]
typedef ptrdiff_t (*AgateForeignAllocateFunc)(AgateVM *vm, const char *unit_name, const char *class_name);
typedef uint64_t (*AgateForeignTagFunc)(AgateVM *vm, const char *unit_name, const char *class_name);
typedef void (*AgateForeignDestroyFunc)(AgateVM *vm, const char *unit_name, const char *class_name, void *data);


typedef struct {
  AgateForeignAllocateFunc allocate;
  AgateForeignTagFunc tag;
  AgateForeignDestroyFunc destroy;
} AgateForeignClassHandler;

typedef AgateForeignClassHandler (*AgateForeignClassHandlerFunc)(AgateVM *vm, const char *unit_name, const char *class_name);
// end::foreign_class_handler[]

// tag::foreign_method_handler[]
typedef void (*AgateForeignMethodFunc)(AgateVM *vm);

typedef enum {
  AGATE_FOREIGN_METHOD_INSTANCE,
  AGATE_FOREIGN_METHOD_CLASS,
} AgateForeignMethodKind;

typedef AgateForeignMethodFunc (*AgateForeignMethodHandlerFunc)(AgateVM *vm, const char *unit_name, const char *class_name, AgateForeignMethodKind kind, const char *signature);
// end::foreign_method_handler[]

// tag::config[]
typedef struct {
  AgateReallocFunc reallocate;

  AgateUnitHandlerFunc unit_handler;

  AgateForeignClassHandlerFunc foreign_class_handler;
  AgateForeignMethodHandlerFunc foreign_method_handler;

  AgateParseIntFunc parse_int;
  AgateParseFloatFunc parse_float;

  AgateAssertHandling assert_handling;
  AgatePrintFunc print;
  AgateWriteFunc write;
  AgateErrorFunc error;
  AgateInputFunc input;

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

// tag::type[]
typedef enum {
  AGATE_TYPE_UNKNOWN,
  AGATE_TYPE_ARRAY,
  AGATE_TYPE_BOOL,
  AGATE_TYPE_CHAR,
  AGATE_TYPE_FLOAT,
  AGATE_TYPE_FOREIGN,
  AGATE_TYPE_INT,
  AGATE_TYPE_MAP,
  AGATE_TYPE_NIL,
  AGATE_TYPE_STRING,
  AGATE_TYPE_TUPLE,
} AgateType;
// end::type[]

// tag::initialize[]
void agateConfigInitialize(AgateConfig *config);
// end::initialize[]

// tag::vm_new[]
AgateVM *agateNewVM(const AgateConfig *config);
// end::vm_new[]
// tag::vm_delete[]
void agateDeleteVM(AgateVM *vm);
// end::vm_delete[]

void agateSetArgs(AgateVM *vm, int argc, const char *argv[]);

void *agateMemoryAllocate(AgateVM *vm, void *ptr, ptrdiff_t size);

// tag::handle_call[]
AgateHandle *agateMakeCallHandle(AgateVM *vm, const char *signature);
// end::handle_call[]

// tag::stack[]
void agateStackStart(AgateVM *vm);
void agateStackFinish(AgateVM *vm);
// end::stack[]

// tag::call_handle[]
AgateStatus agateCallHandle(AgateVM *vm, AgateHandle *method);
// end::call_handle[]
// tag::call_string[]
AgateStatus agateCallString(AgateVM *vm, const char *unit, const char *source);
// end::call_string[]

// tag::slot_management[]
ptrdiff_t agateSlotCount(AgateVM *vm);
AgateType agateSlotType(AgateVM *vm, ptrdiff_t slot);
void agateSlotCopy(AgateVM *vm, ptrdiff_t dest, ptrdiff_t orig);
// end::slot_management[]

#define AGATE_RETURN_SLOT 0

// tag::slot_for_call[]
ptrdiff_t agateSlotAllocate(AgateVM *vm);
ptrdiff_t agateSlotAllocateMany(AgateVM *vm, ptrdiff_t count);
// end::slot_for_call[]

// tag::slot_get_primitive[]
bool agateSlotGetBool(AgateVM *vm, ptrdiff_t slot);
uint32_t agateSlotGetChar(AgateVM *vm, ptrdiff_t slot);
int64_t agateSlotGetInt(AgateVM *vm, ptrdiff_t slot);
double agateSlotGetFloat(AgateVM *vm, ptrdiff_t slot);

void *agateSlotGetForeign(AgateVM *vm, ptrdiff_t slot);
uint64_t agateSlotGetForeignTag(AgateVM *vm, ptrdiff_t slot);

const char *agateSlotGetString(AgateVM *vm, ptrdiff_t slot);
const char *agateSlotGetStringSize(AgateVM *vm, ptrdiff_t slot, ptrdiff_t *size);
// end::slot_get_primitive[]

// tag::handle_get[]
AgateHandle *agateSlotGetHandle(AgateVM *vm, ptrdiff_t slot);
// end::handle_get[]

// tag::handle_release[]
void agateReleaseHandle(AgateVM *vm, AgateHandle *handle);
// end::handle_release[]

// tag::slot_set_primitive[]
void agateSlotSetNil(AgateVM *vm, ptrdiff_t slot);
void agateSlotSetBool(AgateVM *vm, ptrdiff_t slot, bool value);
void agateSlotSetChar(AgateVM *vm, ptrdiff_t slot, uint32_t value);
void agateSlotSetInt(AgateVM *vm, ptrdiff_t slot, int64_t value);
void agateSlotSetFloat(AgateVM *vm, ptrdiff_t slot, double value);

void *agateSlotSetForeign(AgateVM *vm, ptrdiff_t slot, ptrdiff_t class_slot);

void agateSlotSetString(AgateVM *vm, ptrdiff_t slot, const char *text);
void agateSlotSetStringSize(AgateVM *vm, ptrdiff_t slot, const char *text, ptrdiff_t size);
// end::slot_set_primitive[]

// tag::handle_set[]
void agateSlotSetHandle(AgateVM *vm, ptrdiff_t slot, AgateHandle *handle);
// end::handle_set[]

// tag::slot_array[]
void agateSlotArrayNew(AgateVM *vm, ptrdiff_t slot);
ptrdiff_t agateSlotArraySize(AgateVM *vm, ptrdiff_t slot);
void agateSlotArrayGet(AgateVM *vm, ptrdiff_t array_slot, ptrdiff_t index, ptrdiff_t element_slot);
void agateSlotArraySet(AgateVM *vm, ptrdiff_t array_slot, ptrdiff_t index, ptrdiff_t element_slot);
void agateSlotArrayInsert(AgateVM *vm, ptrdiff_t array_slot, ptrdiff_t index, ptrdiff_t element_slot);
void agateSlotArrayErase(AgateVM *vm, ptrdiff_t array_slot, ptrdiff_t index, ptrdiff_t element_slot);
// end::slot_array[]

// tag::slot_map[]
void agateSlotMapNew(AgateVM *vm, ptrdiff_t slot);
ptrdiff_t agateSlotMapSize(AgateVM *vm, ptrdiff_t slot);
bool agateSlotMapContains(AgateVM *vm, ptrdiff_t map_slot, ptrdiff_t key_slot);
void agateSlotMapGet(AgateVM *vm, ptrdiff_t map_slot, ptrdiff_t key_slot, ptrdiff_t value_slot);
void agateSlotMapSet(AgateVM *vm, ptrdiff_t map_slot, ptrdiff_t key_slot, ptrdiff_t value_slot);
void agateSlotMapErase(AgateVM *vm, ptrdiff_t map_slot, ptrdiff_t key_slot, ptrdiff_t value_slot);
// end::slot_map[]

// tag::slot_tuple[]
ptrdiff_t agateSlotTupleSize(AgateVM *vm, ptrdiff_t slot);
void agateSlotTupleGet(AgateVM *vm, ptrdiff_t tuple_slot, ptrdiff_t index, ptrdiff_t component_slot);
// end::slot_tuple[]

// tag::slot_object[]
void agateSlotGetField(AgateVM *vm, ptrdiff_t object_slot, ptrdiff_t index, ptrdiff_t result_slot);
void agateSlotSetField(AgateVM *vm, ptrdiff_t object_slot, ptrdiff_t index, ptrdiff_t value_slot);
// end::slot_object[]

// tag::has_unit[]
bool agateHasUnit(AgateVM *vm, const char *unit_name);
// end::has_unit[]
// tag::has_variable[]
bool agateHasVariable(AgateVM *vm, const char *unit_name, const char *variable_name);
// end::has_variable[]
// tag::get_variable[]
void agateGetVariable(AgateVM *vm, const char *unit_name, const char *variable_name, ptrdiff_t slot);
// end::get_variable[]

// tag::abort[]
void agateAbort(AgateVM *vm, ptrdiff_t slot);
// end::abort[]

// tag::user_data[]
void *agateGetUserData(AgateVM *vm);
void agateSetUserData(AgateVM *vm, void *user_data);
// end::user_data[]

#ifdef __cplusplus
}
#endif

#endif /* AGATE_H */
