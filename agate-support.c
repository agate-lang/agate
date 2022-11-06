#include "agate-support.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1024

typedef struct {
  char *data;
  ptrdiff_t capacity;
  ptrdiff_t size;
} AgateCharBuffer;

typedef void (*AgateGenericHandlerFunc)(void);

typedef struct {
  char *unit_name;
  AgateGenericHandlerFunc handler;
} AgateDictItem;

typedef struct {
  AgateDictItem *data;
  ptrdiff_t capacity;
  ptrdiff_t size;
} AgateDict;

typedef struct {
  char *include_paths[AGATE_SUPPORT_INCLUDE_PATH_MAX];
  ptrdiff_t size;
} AgateUnitSupport;

typedef struct {
  AgateReallocFunc reallocate;
  void *user_data;
  AgateUnitSupport unit_support;
  AgateDict foreign_class_support;
  AgateDict foreign_method_support;
} AgateSupport;

// realloc

static void *agateReallocDefault(void *ptr, ptrdiff_t size, void *user_data) {
  if (size == 0) {
    free(ptr);
    return NULL;
  }

  void* next = realloc(ptr, size);
  assert(next);
  return next;
}

static void *agateReallocForward(void *ptr, ptrdiff_t size, void *user_data) {
  AgateSupport *support = user_data;
  return support->reallocate(ptr, size, support->user_data);
}

// duplicate

static char *agateDuplicate(const char *string, AgateSupport *support) {
  size_t length = strlen(string);
  char *duplicate = support->reallocate(NULL, length + 1, support->user_data);
  assert(duplicate);
  memcpy(duplicate, string, length);
  duplicate[length] = '\0';
  return duplicate;
}

// char buffer

static void agateCharBufferCreate(AgateCharBuffer *self) {
  self->data = NULL;
  self->capacity = 0;
  self->size = 0;
}

static void agateCharBufferDestroy(AgateCharBuffer *self, AgateSupport *support) {
  support->reallocate(self->data, 0, support->user_data);
  self->data = NULL;
  self->capacity = 0;
  self->size = 0;
}

static void agateCharBufferReset(AgateCharBuffer *self) {
  self->size = 0;
  memset(self->data, 0, self->capacity);
}

static void agateCharBufferAppend(AgateCharBuffer *self, const char *buffer, size_t size, AgateSupport *support) {
  if (size == 0) {
    return;
  }

  ptrdiff_t needed = self->size + size + 1;

  if (self->capacity < needed) {
    while (self->capacity < needed) {
      if (self->capacity == 0) {
        self->capacity = 16;
      } else {
        self->capacity *= 2;
      }
    }

    char *data = support->reallocate(self->data, self->capacity * sizeof(char), support->user_data);
    assert(data);
    self->data = data;
  }

  memcpy(self->data + self->size, buffer, size);

  self->size += size;
  self->data[self->size] = '\0';
}

// dict

static void agateDictCreate(AgateDict *self) {
  self->data = NULL;
  self->capacity = 0;
  self->size = 0;
}

static void agateDictDestroy(AgateDict *self, AgateSupport *support) {
  for (ptrdiff_t i = 0; i < self->size; ++i) {
    support->reallocate(self->data[i].unit_name, 0, support->user_data);
  }

  support->reallocate(self->data, 0, support->user_data);
  self->data = NULL;
  self->capacity = 0;
  self->size = 0;
}

static void agateDictAppend(AgateDict *self, const char *name, AgateGenericHandlerFunc handler, AgateSupport *support) {
  ptrdiff_t needed = self->size + 1;

  if (self->capacity < needed) {
    while (self->capacity < needed) {
      if (self->capacity == 0) {
        self->capacity = 16;
      } else {
        self->capacity *= 2;
      }
    }

    AgateDictItem *data = support->reallocate(self->data, self->capacity * sizeof(AgateDictItem), support->user_data);
    assert(data);
    self->data = data;
  }

  AgateDictItem *item = &self->data[self->size++];
  item->unit_name = agateDuplicate(name, support);
  item->handler = handler;
}

// unit handling

static void agateUnitSupportCreate(AgateUnitSupport *self) {
  self->size = 0;
}

static void agateUnitSupportDestroy(AgateUnitSupport *self, AgateSupport *support) {
  for (ptrdiff_t i = 0; i < self->size; ++i) {
    support->reallocate(self->include_paths[i], 0, support->user_data);
    self->include_paths[i] = NULL;
  }

  self->size = 0;
}

static const char *agateUnitLoadFile(const char *path, AgateSupport *support) {
  FILE *file = fopen(path, "rb");

  if (file == NULL) {
    return NULL;
  }

  AgateCharBuffer content;
  agateCharBufferCreate(&content);

  char buffer[BUFFER_SIZE];

  while (!feof(file)) {
    size_t size = fread(buffer, sizeof(char), BUFFER_SIZE, file);
    agateCharBufferAppend(&content, buffer, size, support);
  }

  fclose(file);

  return content.data;
}

static const char *agateUnitLoad(const char *name, void *user_data) {
  AgateSupport *support = user_data;
  AgateUnitSupport *units = &support->unit_support;

  AgateCharBuffer path;
  agateCharBufferCreate(&path);

  for (ptrdiff_t i = 0; i < units->size; ++i) {
    agateCharBufferReset(&path);
    const char *include_path = units->include_paths[i];
    agateCharBufferAppend(&path, include_path, strlen(include_path), support);
    agateCharBufferAppend(&path, "/", 1, support);
    agateCharBufferAppend(&path, name, strlen(name), support);
    agateCharBufferAppend(&path, ".agate", 6, support);

    const char *source = agateUnitLoadFile(path.data, support);

    if (source != NULL) {
      agateCharBufferDestroy(&path, support);
      return source;
    }
  }

  agateCharBufferDestroy(&path, support);
  return NULL;
}

static void agateUnitRelease(const char *source, void *user_data) {
  AgateSupport *support = user_data;
  support->reallocate((void *) source, 0, support->user_data);
}

bool agateExUnitAddIncludePath(AgateVM *vm, const char *path) {
  AgateSupport *support = agateGetUserData(vm);
  AgateUnitSupport *units = &support->unit_support;

  if (units->size == AGATE_SUPPORT_INCLUDE_PATH_MAX) {
    return false;
  }

  units->include_paths[units->size++] = agateDuplicate(path, support);
  return true;
}

AgateUnitHandler agateExUnitHandler(AgateVM *vm, const char *name) {
  AgateSupport *support = agateGetUserData(vm);

  AgateUnitHandler handler = {
    agateUnitLoad,
    agateUnitRelease,
    support
  };

  return handler;
}

const char *agateExUnitLoad(AgateVM *vm, const char *unit_name) {
  const char *point = strrchr(unit_name, '.');

  if (point != NULL && strcmp(point, ".agate") == 0) {
    return agateUnitLoadFile(unit_name, agateGetUserData(vm));
  }

  return agateUnitLoad(unit_name, agateGetUserData(vm));
}

void agateExUnitRelease(AgateVM *vm, const char *unit_text) {
  agateUnitRelease(unit_text, agateGetUserData(vm));
}

// foreign class

void agateExForeignClassAddHandler(AgateVM *vm, AgateForeignClassHandlerFunc func, const char *unit_name) {
  AgateSupport *support = agateGetUserData(vm);
  agateDictAppend(&support->foreign_class_support, unit_name, (AgateGenericHandlerFunc) func, support);
}

AgateForeignClassHandler agateExForeignClassHandler(AgateVM *vm, const char *unit_name, const char *class_name) {
  AgateSupport *support = agateGetUserData(vm);

  for (ptrdiff_t i = 0; i < support->foreign_class_support.size; ++i) {
    AgateDictItem *item = &support->foreign_class_support.data[i];

    if (strcmp(item->unit_name, unit_name) == 0) {
      AgateForeignClassHandlerFunc func = (AgateForeignClassHandlerFunc) item->handler;
      return func(vm, unit_name, class_name);
    }
  }

  AgateForeignClassHandler handler = { NULL, NULL };
  return handler;
}

// foreign method

void agateExForeignMethodAddHandler(AgateVM *vm, AgateForeignMethodHandlerFunc func, const char *unit_name) {
  AgateSupport *support = agateGetUserData(vm);
  agateDictAppend(&support->foreign_method_support, unit_name, (AgateGenericHandlerFunc) func, support);
}

AgateForeignMethodFunc agateExForeignMethodHandler(AgateVM *vm, const char *unit_name, const char *class_name, AgateForeignMethodKind kind, const char *signature) {
  AgateSupport *support = agateGetUserData(vm);

  for (ptrdiff_t i = 0; i < support->foreign_method_support.size; ++i) {
    AgateDictItem *item = &support->foreign_method_support.data[i];

    if (strcmp(item->unit_name, unit_name) == 0) {
      AgateForeignMethodHandlerFunc func = (AgateForeignMethodHandlerFunc) item->handler;
      return func(vm, unit_name, class_name, kind, signature);
    }
  }

  return NULL;
}

// support

static void agateExSupportCreate(AgateSupport *self, AgateReallocFunc reallocate, void *user_data) {
  self->reallocate = reallocate;
  self->user_data = user_data;
  agateUnitSupportCreate(&self->unit_support);
  agateDictCreate(&self->foreign_class_support);
  agateDictCreate(&self->foreign_method_support);
}

static void agateSupportDestroy(AgateSupport *self) {
  agateDictDestroy(&self->foreign_method_support, self);
  agateDictDestroy(&self->foreign_class_support, self);
  agateUnitSupportDestroy(&self->unit_support, self);
}

// agate function replacements

AgateVM *agateExNewVM(const AgateConfig *config) {
  assert(config);
  AgateConfig local_config = *config;

  AgateReallocFunc reallocate;

  if (local_config.reallocate) {
    reallocate = local_config.reallocate;
    local_config.reallocate = agateReallocForward;
  } else {
    reallocate = agateReallocDefault;
  }

  AgateSupport *support = reallocate(NULL, sizeof(AgateSupport), local_config.user_data);
  assert(support);
  agateExSupportCreate(support, reallocate, local_config.user_data);

  AgateVM *vm = agateNewVM(&local_config);
  agateSetUserData(vm, support);
  return vm;
}

void agateExDeleteVM(AgateVM *vm) {
  AgateSupport *support = agateGetUserData(vm);
  AgateReallocFunc reallocate = support->reallocate;
  void *user_data = support->user_data;

  // delete vm before support in case of forward allocator
  agateDeleteVM(vm);

  agateSupportDestroy(support);
  reallocate(support, 0, user_data);
}

void *agateExGetUserData(AgateVM *vm) {
  AgateSupport *support = agateGetUserData(vm);
  return support->user_data;
}

void agateExSetUserData(AgateVM *vm, void *user_data) {
  AgateSupport *support = agateGetUserData(vm);
  support->user_data = user_data;
}
