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

typedef struct AgateVM AgateVM;

typedef enum {
  AGATE_ASSERT_ABORT, // failed asserts abort the script (default)
  AGATE_ASSERT_NIL,   // failed asserts return nil
  AGATE_ASSERT_NONE,  // asserts are not compiled
} AgateAssertHandling;

typedef void * (*AgateReallocFunc)(void *ptr, ptrdiff_t size, void *user_data);

typedef bool (*AgateParseIntFunc)(const char *text, ptrdiff_t size, int base, int64_t *result);
typedef bool (*AgateParseFloatFunc)(const char *text, ptrdiff_t size, double *result);

// print

typedef void (*AgatePrintFunc)(AgateVM *vm, const char* text);

typedef enum {
  AGATE_ERROR_COMPILE,
  AGATE_ERROR_RUNTIME,
  AGATE_ERROR_STACKTRACE,
} AgateErrorKind;

typedef void (*AgateErrorFunc)(AgateVM *vm, AgateErrorKind kind, const char *module_name, int line, const char *message);


// module

typedef const char *(*AgateModuleLoadFunc)(const char *name, void *user_data);
typedef void (*AgateModuleReleaseFunc)(const char *source, void *user_data);

typedef struct {
  AgateModuleLoadFunc load;
  AgateModuleReleaseFunc release;
  void *user_data;
} AgateModuleHandler;

typedef AgateModuleHandler (*AgateModuleHandlerFunc)(AgateVM *vm, const char *name);

// foreign class

typedef ptrdiff_t (*AgateForeignAllocateFunc)(AgateVM *vm, const char *module_name, const char *class_name);
typedef void (*AgateForeignDestroyFunc)(AgateVM *vm, const char *module_name, const char *class_name, void *data);


typedef struct {
  AgateForeignAllocateFunc allocate;
  AgateForeignDestroyFunc destroy;
} AgateForeignClassHandler;

typedef AgateForeignClassHandler (*AgateForeignClassHandlerFunc)(AgateVM *vm, const char *module_name, const char *class_name);

// foreign method

typedef void (*AgateForeignMethodFunc)(AgateVM *vm);

typedef enum {
  AGATE_FOREIGN_METHOD_INSTANCE,
  AGATE_FOREIGN_METHOD_CLASS,
} AgateForeignMethodKind;

typedef AgateForeignMethodFunc (*AgateForeignMethodHandlerFunc)(AgateVM *vm, const char *module_name, const char *class_name, AgateForeignMethodKind kind, const char *signature);

// config

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

typedef enum {
  AGATE_STATUS_OK,
  AGATE_STATUS_COMPILE_ERROR,
  AGATE_STATUS_RUNTIME_ERROR,
} AgateStatus;

void agateConfigInitialize(AgateConfig *config);

AgateVM *agateNewVM(const AgateConfig *config);
void agateDeleteVM(AgateVM *vm);

AgateStatus agateInterpret(AgateVM *vm, const char *module, const char *source);

void *agateGetUserData(AgateVM *vm);
void agateSetUserData(AgateVM *vm, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* AGATE_H */
