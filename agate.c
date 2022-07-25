// SPDX-License-Identifier: MIT
// Copyright (c) 2013-2021 Robert Nystrom and Wren Contributors
// Copyright (c) 2022 Julien Bernard
#include "agate.h"

#include <assert.h>
#include <errno.h>
#include <float.h>
#include <inttypes.h>
#include <locale.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * Outline of the file:
 * - macros
 * - limits and tweaks
 * - types
 *   - generic array
 *   - value
 *   - table
 *   - bytecode
 *   - entities
 *     - string
 *     - unit
 *     - function, native, upvalue and closure
 *     - class, instance and foreign
 *     - array, map and range
 *   - opcodes
 *   - vm
 *   - misc
 * - memory
 *   - basics
 * - vm
 *   - root
 * - generic array
 * - utf8
 * - value
 *   - basics
 * - entity
 *   - basics
 * - equals
 * - hash
 * - bytecode
 * - value array
 * - hash table
 * - entities
 *   - new
 * - symbol table
 * - entities
 *   - unit
 * - debug
 * - memory
 *   - gc
 * - vm
 *   - defaults
 *   - run
 *   - validation
 *   - core
 * - api
 * - types
 *   - parser
 * - parser
 *   - error
 *   - constants
 *   - scanner
 *   - textutils
 *   - tokens
 *   - utils
 *   - bytecode
 *   - scopes
 *   - signature
 *   - loop
 *   - expression
 *   - statement
 *   - declaration
 *   - gc
 */

/*
 * macros
 */

#define AGATE_ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

#define AGATE_INDEX_ERROR PTRDIFF_MAX


/*
 * limits and tweaks
 */

// #define AGATE_DEBUG_TRACE_EXECUTION
// #define AGATE_DEBUG_PRINT_CODE
// #define AGATE_DEBUG_STRESS_GC
// #define AGATE_DEBUG_LOG_GC

#define AGATE_INITIAL_STACK_CAPACITY 1024
#define AGATE_INITIAL_FRAMES_CAPACITY 64

#define AGATE_MAX_INTERPOLATION_NESTING 8

#define AGATE_MAX_CONSTANTS (1 << 16)
#define AGATE_MAX_UNIT_OBJECTS (1 << 16)
#define AGATE_MAX_LOCALS (1 << 8)
#define AGATE_MAX_UPVALUES (1 << 7)
#define AGATE_MAX_FIELDS UINT8_MAX

#define AGATE_MAX_BREAKS 32

#define AGATE_MAX_VARIABLE_NAME_SIZE 64

#define AGATE_MAX_PARAMETERS 16
#define AGATE_MAX_METHOD_NAME_SIZE 64
#define AGATE_MAX_METHOD_SIGNATURE_SIZE (AGATE_MAX_METHOD_NAME_SIZE + (AGATE_MAX_PARAMETERS * 2) + 1)

#define AGATE_GC_HEAP_GROW_FACTOR 2

#define AGATE_TABLE_MAX_LOAD_NUM 3
#define AGATE_TABLE_MAX_LOAD_DEN 4

/*
 * types - generic array
 */

#define AGATE_ARRAY_DECLARE(name, type)                                   \
typedef struct {                                                          \
  type *data;                                                             \
  ptrdiff_t size;                                                         \
  ptrdiff_t capacity;                                                     \
} Agate ## name;

/*
 * types - value
 */

typedef struct AgateEntity AgateEntity;

typedef enum {
  AGATE_VALUE_UNDEFINED,
  AGATE_VALUE_NIL,
  AGATE_VALUE_BOOL,
  AGATE_VALUE_CHAR,
  AGATE_VALUE_INT,
  AGATE_VALUE_FLOAT,
  AGATE_VALUE_ENTITY,
} AgateValueKind;

typedef struct {
  AgateValueKind kind;

  union {
    bool bool_value;
    uint32_t char_value;
    int64_t int_value;
    double float_value;
    AgateEntity *entity_value;
  } as;
} AgateValue;

/*
 * types - table
 */

typedef struct {
  AgateValue key;
  AgateValue value;
  uint64_t hash;
} AgateTableEntry;

typedef struct {
  ptrdiff_t capacity;
  ptrdiff_t size;
  AgateTableEntry *entries;
} AgateTable;

/*
 * types - bytecode
 */

AGATE_ARRAY_DECLARE(BytecodeArray, uint8_t)
AGATE_ARRAY_DECLARE(ValueArray, AgateValue)

typedef struct {
  int line;
  ptrdiff_t start;
  ptrdiff_t size;
} AgateLineInfo;

AGATE_ARRAY_DECLARE(LineInfoArray, AgateLineInfo)

typedef struct {
  AgateBytecodeArray code;
  AgateValueArray constants;
  AgateLineInfoArray lines;
} AgateBytecode;

/*
 * types - entities
 */

typedef enum {
  AGATE_ENTITY_ARRAY,
  AGATE_ENTITY_CLASS,
  AGATE_ENTITY_CLOSURE,
  AGATE_ENTITY_FOREIGN,
  AGATE_ENTITY_FUNCTION,
  AGATE_ENTITY_INSTANCE,
  AGATE_ENTITY_MAP,
  AGATE_ENTITY_RANGE,
  AGATE_ENTITY_STRING,
  AGATE_ENTITY_UNIT,
  AGATE_ENTITY_UPVALUE,
} AgateEntityKind;

typedef enum {
  AGATE_ENTITY_STATUS_WHITE,
  AGATE_ENTITY_STATUS_GRAY,
} AgateEntityStatus;

typedef struct AgateClass AgateClass;

struct AgateEntity {
  AgateEntityKind kind;
  AgateEntityStatus status;
  AgateClass *type;
  struct AgateEntity *next;
};

/*
 * types - entities - string
 */

typedef struct {
  AgateEntity base;
  ptrdiff_t size;
  uint64_t hash;
  char data[];
} AgateString;

/*
 * types - entities - unit
 */

typedef struct {
  AgateEntity base;
  AgateValueArray object_values;
  AgateTable object_names;
  AgateString *name;
} AgateUnit;

/*
 * types - entities - function, native, upvalue and closure
 */

typedef struct {
  AgateEntity base;
  AgateBytecode bc;
  AgateUnit *unit;
  int arity;
  ptrdiff_t slot_count;
  ptrdiff_t upvalue_count;
  AgateString *name;
} AgateFunction;

typedef enum {
  AGATE_CAPTURE_UPVALUE = 0,
  AGATE_CAPTURE_LOCAL   = 1,
} AgateCapture;

typedef struct AgateUpvalue {
  AgateEntity base;
  AgateValue *location;
  AgateValue closed;
  struct AgateUpvalue *next;
} AgateUpvalue;

typedef struct {
  AgateEntity base;
  AgateFunction *function;
  ptrdiff_t upvalue_count;
  AgateUpvalue *upvalues[];
} AgateClosure;

typedef bool (*AgateNativeFunc)(AgateVM *vm, int argc, AgateValue *args);

/*
 * types - entities - class, instance and foreign
 */

typedef enum {
  AGATE_METHOD_NONE,
  AGATE_METHOD_NATIVE,
  AGATE_METHOD_FOREIGN,
  AGATE_METHOD_FOREIN_ALLOCATE,
  AGATE_METHOD_FOREIN_DESTROY,
  AGATE_METHOD_CLOSURE,
} AgateMethodKind;

typedef struct {
  AgateMethodKind kind;
  union {
    AgateNativeFunc native;
    AgateForeignMethodFunc foreign;
    AgateForeignAllocateFunc foreign_allocate;
    AgateForeignDestroyFunc foreign_destroy;
    AgateClosure *closure;
  } as;
} AgateMethod;

AGATE_ARRAY_DECLARE(MethodArray, AgateMethod)

struct AgateClass {
  AgateEntity base;
  AgateUnit *unit;
  AgateClass *supertype;
  ptrdiff_t field_count;
  AgateMethodArray methods;
  AgateString *name;
};

typedef struct {
  AgateEntity base;
  ptrdiff_t field_count;
  AgateValue fields[];
} AgateInstance;

typedef struct {
  AgateEntity base;
  ptrdiff_t data_size;
  uint8_t data[];
} AgateForeign;

/*
 * types - entities - array, map and range
 */

typedef struct {
  AgateEntity base;
  AgateValueArray elements;
} AgateArray;

typedef struct {
  AgateEntity base;
  AgateTable members;
} AgateMap;

typedef enum {
  AGATE_RANGE_INCLUSIVE,
  AGATE_RANGE_EXCLUSIVE,
} AgateRangeKind;

typedef struct {
  AgateEntity base;
  int64_t from;
  int64_t to;
  AgateRangeKind kind;
} AgateRange;

/*
 * types - opcodes
 */

//  NAME,    STACK_EFFECT, ARGS_BYTES
#define AGATE_OPCODE_LIST     \
  X(CONSTANT,           1,  2)  \
  X(NIL,                1,  0)  \
  X(FALSE,              1,  0)  \
  X(TRUE,               1,  0)  \
  X(GLOBAL_LOAD,        1,  2)  \
  X(GLOBAL_STORE,       0,  2)  \
  X(LOCAL_LOAD,         1,  1)  \
  X(LOCAL_STORE,        0,  1)  \
  X(UPVALUE_LOAD,       1,  1)  \
  X(UPVALUE_STORE,      0,  1)  \
  X(FIELD_LOAD,         1,  1)  \
  X(FIELD_STORE,        0,  1)  \
  X(FIELD_LOAD_THIS,    1,  1)  \
  X(FIELD_STORE_THIS,   0,  1)  \
  X(JUMP_FORWARD,       0,  2)  \
  X(JUMP_BACKWARD,      0,  2)  \
  X(JUMP_IF,           -1,  2)  \
  X(AND,               -1,  2)  \
  X(OR,                -1,  2)  \
  X(CALL,               0,  1)  \
  X(INVOKE,             0,  3)  \
  X(SUPER,              0,  5)  \
  X(CLOSURE,            1, -1)  \
  X(CLOSE_UPVALUE,     -1,  0)  \
  X(POP,               -1,  0)  \
  X(RETURN,             0,  0)  \
  X(CLASS,             -1,  1)  \
  X(CLASS_FOREIGN,     -1,  0)  \
  X(CONSTRUCT,          0,  0)  \
  X(CONSTRUCT_FOREIGN,  0,  0)  \
  X(METHOD_INSTANCE,   -2,  2)  \
  X(METHOD_CLASS,      -2,  2)  \
  X(IMPORT_UNIT,        1,  2)  \
  X(IMPORT_OBJECT,      1,  2)  \
  X(END_UNIT,           1,  0)  \
  X(END,                0,  0)

typedef enum {
  #define X(name, stack, bytes) AGATE_OP_ ## name,
  AGATE_OPCODE_LIST
  #undef X
} AgateOpCode;


/*
 * types - vm
 */

typedef struct AgateCompiler AgateCompiler;

typedef struct {
  AgateClosure *closure;
  uint8_t *ip;
  AgateValue *stack_start;
} AgateCallFrame;

AGATE_ARRAY_DECLARE(CallFrameArray, AgateCallFrame)

struct AgateHandle {
  AgateValue value;
  AgateHandle* prev;
  AgateHandle* next;
};

#define AGATE_ROOTS_COUNT_MAX 8

struct AgateVM {
  AgateStatus status;
  AgateConfig config;

  // units

  AgateTable units;

  // core types

  AgateClass *array_class;
  AgateClass *bool_class;
  AgateClass *char_class;
  AgateClass *class_class;
  AgateClass *float_class;
  AgateClass *fn_class;
  AgateClass *int_class;
  AgateClass *map_class;
  AgateClass *nil_class;
  AgateClass *object_class;
  AgateClass *range_class;
  AgateClass *string_class;

  AgateTable method_names;

  // runtime

  AgateValue *stack;
  AgateValue *stack_top;
  ptrdiff_t stack_capacity;

  AgateValue *api_stack;

  AgateCallFrame *frames;
  ptrdiff_t frames_count;
  ptrdiff_t frames_capacity;

  AgateUpvalue *open_upvalues;

  AgateValue error;

  // parser

  AgateCompiler *compiler;
  AgateUnit *last_unit;

  // memory

  AgateEntity *roots[AGATE_ROOTS_COUNT_MAX];
  ptrdiff_t roots_count;

  ptrdiff_t bytes_allocated;
  ptrdiff_t bytes_threshold;

  AgateEntity *entities;
  AgateHandle *handles;

  ptrdiff_t gray_count;
  ptrdiff_t gray_capacity;
  AgateEntity **gray_stack;
};

/*
 * types - misc
 */

AGATE_ARRAY_DECLARE(CharArray, char)

/*
 * memory - basics
 */

static inline ptrdiff_t agateGrowCapacity(ptrdiff_t capacity) {
  return capacity < 16 ? 16 : capacity * 2;
}

static void *agateMemoryHandle(AgateVM *vm, void *previous, ptrdiff_t old_size, ptrdiff_t new_size);

#define agateAllocate(vm, type, count) ((type *) agateMemoryHandle((vm), NULL, 0, sizeof(type) * (count)))
#define agateFree(vm, type, ptr) agateMemoryHandle((vm), (ptr), sizeof(type), 0)
#define agateFreeArray(vm, type, ptr, count) agateMemoryHandle((vm), (ptr), sizeof(type) * (count), 0)

#define agateGrowArray(vm, type, ptr, old_count, new_count) ((type*) agateMemoryHandle((vm), (ptr), sizeof(type) * (old_count), sizeof(type) * (new_count)))

#define agateAllocateFlex(vm, type, intype, count) ((type *) agateMemoryHandle((vm), NULL, 0, sizeof(type) + sizeof(intype) * (count)))
#define agateFreeFlex(vm, type, ptr, intype, count) agateMemoryHandle((vm), (ptr), sizeof(type) + sizeof(intype) * (count), 0)

/*
 * vm - root
 */

static inline void agatePushRoot(AgateVM *vm, AgateEntity *entity) {
  assert(entity != NULL);
  assert(vm->roots_count < AGATE_ROOTS_COUNT_MAX);
  vm->roots[vm->roots_count++] = entity;
}

static inline void agatePopRoot(AgateVM *vm) {
  assert(vm->roots_count > 0);
  --vm->roots_count;
}

/*
 * generic array
 */

#define AGATE_ARRAY_DEFINE(name, type)                                            \
static void agate ## name ## Create(Agate ## name *self) {                        \
  self->data = NULL;                                                              \
  self->size = 0;                                                                 \
  self->capacity = 0;                                                             \
}                                                                                 \
static void agate ## name ## Destroy(Agate ## name *self, AgateVM *vm) {          \
  assert(self);                                                                   \
  agateFreeArray(vm, type, self->data, self->capacity);                           \
  self->data = NULL;                                                              \
  self->size = 0;                                                                 \
  self->capacity = 0;                                                             \
}                                                                                 \
static inline void agate ## name ## Grow(Agate ## name *self, ptrdiff_t size, AgateVM *vm) { \
  assert(self);                                                                   \
  ptrdiff_t old_capacity = self->capacity;                                        \
  while (self->capacity < size) {                                                 \
    self->capacity = agateGrowCapacity(self->capacity);                           \
  }                                                                               \
  self->data = agateGrowArray(vm, type, self->data, old_capacity, self->capacity); \
}                                                                                 \
static inline void agate ## name ## Append(Agate ## name *self, type val, AgateVM *vm) { \
  assert(self);                                                                   \
  if (self->size == self->capacity) {                                             \
    agate ## name ## Grow(self, self->size + 1, vm);                              \
  }                                                                               \
  self->data[self->size] = val;                                                   \
  ++self->size;                                                                   \
}                                                                                 \

// Resize
#define AGATE_ARRAY_DEFINE_RESIZE(name, type)                                     \
static void agate ## name ## Resize(Agate ## name *self, ptrdiff_t size, type val, AgateVM *vm) { \
  assert(self);                                                                   \
  if (self->size < size) {                                                        \
    agate ## name ## Grow(self, size, vm);                                        \
    for (ptrdiff_t i = self->size; i < size; ++i) {                               \
      self->data[i] = val;                                                        \
    }                                                                             \
  }                                                                               \
  self->size = size;                                                              \
}

// AppendMultiple
#define AGATE_ARRAY_DEFINE_APPEND_MULTIPLE(name, type)                            \
static void agate ## name ## AppendMultiple(Agate ## name *self, const type *start, ptrdiff_t size, AgateVM *vm) { \
  assert(self);                                                                   \
  if (self->size + size > self->capacity) {                                       \
    agate ## name ## Grow(self, self->size + size, vm);                           \
  }                                                                               \
  for (ptrdiff_t i = 0; i < size; ++i) {                                          \
    self->data[self->size++] = start[i];                                          \
  }                                                                               \
  assert(self->size <= self->capacity);                                           \
}

AGATE_ARRAY_DEFINE(CharArray, char)
AGATE_ARRAY_DEFINE_APPEND_MULTIPLE(CharArray, char)

AGATE_ARRAY_DEFINE(BytecodeArray, uint8_t)
AGATE_ARRAY_DEFINE(ValueArray, AgateValue)
AGATE_ARRAY_DEFINE(LineInfoArray, AgateLineInfo)

AGATE_ARRAY_DEFINE(MethodArray, AgateMethod)
AGATE_ARRAY_DEFINE_RESIZE(MethodArray, AgateMethod)

/*
 * utf8
 */

#define AGATE_INVALID_CHAR UINT32_C(0xFFFFFFFF)
#define AGATE_CHAR_BUFFER_SIZE 8

static ptrdiff_t agateUtf8DecodeSize(const char *text) {
  uint8_t c = *text;

  if ((c & 0x80) == 0x00) { return 1; }
  if ((c & 0xE0) == 0xC0) { return 2; }
  if ((c & 0xF0) == 0xE0) { return 3; }
  if ((c & 0xF8) == 0xF0) { return 4; }

  assert(false);
  return 0;
}

static uint32_t agateUtf8Decode(const char *text, ptrdiff_t size, const char **end) {
  if (size == 0) {
    return AGATE_INVALID_CHAR;
  }

  uint8_t c = *text;

  if (c < 0x80) {
    if (end != NULL) {
      *end = text + 1;
    }

    return c;
  }

  ptrdiff_t expected;
  uint32_t codepoint;

  if ((c & 0xE0) == 0xC0) {
    expected = 2;
    codepoint = (c & 0x1F);
  } else if ((c & 0xF0) == 0xE0) {
    expected = 3;
    codepoint = (c & 0x0F);
  } else if ((c & 0xF8) == 0xF0) {
    expected = 4;
    codepoint = (c & 0x07);
  } else {
    return AGATE_INVALID_CHAR;
  }

  if (size < expected) {
    return AGATE_INVALID_CHAR;
  }

  for (ptrdiff_t i = 1; i < expected; ++i) {
    c = text[i];

    if ((c & 0xC0) != 0x80) {
      return AGATE_INVALID_CHAR;
    }

    codepoint = (codepoint << 6) | (c & 0x3F);
  }

  if (0xD800 <= codepoint && codepoint <= 0xDFFF) {
    // codepoints for surrogate pairs in UTF-16
    return AGATE_INVALID_CHAR;
  }

  if (end != NULL) {
    *end = text + expected;
  }

  return codepoint;
}

static inline ptrdiff_t agateUtf8EncodeSize(uint32_t c) {
  if (c < 0x80) { return 1; }
  if (c < 0x800) { return 2; }
  if (c < 0x10000) { return 3; }
  if (c < 0x110000) { return 4; }
  assert(false);
  return 0;
}

static ptrdiff_t agateUtf8Encode(uint32_t c, char *text) {
  if (c < 0x80) {
    *text++ = (char) (c & 0x7F);
    return 1;
  }

  if (c < 0x800) {
    *text++ = (char) (((c >> 6) & 0x1F) | 0xC0);
    *text++ = (char) (((c >> 0) & 0x3F) | 0x80);
    return 2;
  }

  if (c < 0x10000) {
    *text++ = (char) (((c >> 12) & 0x0F) | 0xE0);
    *text++ = (char) (((c >>  6) & 0x3F) | 0x80);
    *text++ = (char) (((c >>  0) & 0x3F) | 0x80);
    return 3;
  }

  if (c < 0x110000) {
    *text++ = (char) (((c >> 18) & 0x07) | 0xF0);
    *text++ = (char) (((c >> 12) & 0x3F) | 0x80);
    *text++ = (char) (((c >>  6) & 0x3F) | 0x80);
    *text++ = (char) (((c >>  0) & 0x3F) | 0x80);
    return 4;
  }

  assert(false);
  return 0;
}

/*
 * value - basics
 */

static inline bool agateIsUndefined(AgateValue value) { return value.kind == AGATE_VALUE_UNDEFINED; }
static inline bool agateIsNil(AgateValue value) { return value.kind == AGATE_VALUE_NIL; }
static inline bool agateIsBool(AgateValue value) { return value.kind == AGATE_VALUE_BOOL; }
static inline bool agateIsChar(AgateValue value) { return value.kind == AGATE_VALUE_CHAR; }
static inline bool agateIsInt(AgateValue value) { return value.kind == AGATE_VALUE_INT; }
static inline bool agateIsFloat(AgateValue value) { return value.kind == AGATE_VALUE_FLOAT; }
static inline bool agateIsEntity(AgateValue value) { return value.kind == AGATE_VALUE_ENTITY; }

static inline bool agateAsBool(AgateValue value) { return value.as.bool_value; }
static inline uint32_t agateAsChar(AgateValue value) { return value.as.char_value; }
static inline int64_t agateAsInt(AgateValue value) { return value.as.int_value; }
static inline double agateAsFloat(AgateValue value) { return value.as.float_value; }
static inline AgateEntity *agateAsEntity(AgateValue value) { return value.as.entity_value; }

static inline AgateValue agateUndefinedValue() {
  AgateValue value;
  value.kind = AGATE_VALUE_UNDEFINED;
  value.as.int_value = 0xDEADBEEF;
  return value;
}

static inline AgateValue agateNilValue() {
  AgateValue value;
  value.kind = AGATE_VALUE_NIL;
  value.as.int_value = 0xDEADBEEF;
  return value;
}

static inline AgateValue agateBoolValue(bool raw) {
  AgateValue value;
  value.kind = AGATE_VALUE_BOOL;
  value.as.bool_value = raw;
  return value;
}

static inline AgateValue agateCharValue(uint32_t raw) {
  AgateValue value;
  value.kind = AGATE_VALUE_CHAR;
  value.as.char_value = raw;
  return value;
}

static inline AgateValue agateIntValue(int64_t raw) {
  AgateValue value;
  value.kind = AGATE_VALUE_INT;
  value.as.int_value = raw;
  return value;
}

static inline AgateValue agateFloatValue(double raw) {
  AgateValue value;
  value.kind = AGATE_VALUE_FLOAT;
  value.as.float_value = raw;
  return value;
}

static inline AgateValue agateEntityValue(void *raw) {
  AgateValue value;
  value.kind = AGATE_VALUE_ENTITY;
  value.as.entity_value = (AgateEntity *) raw;
  return value;
}

static inline bool agateIsFalsey(AgateValue value) {
  return agateIsNil(value) || (agateIsBool(value) && !agateAsBool(value));
}

/*
 * entity - basics
 */

static inline AgateEntityKind agateEntityKind(AgateValue value) { return agateAsEntity(value)->kind; }
static inline bool agateIsEntityKind(AgateValue value, AgateEntityKind kind) { return agateIsEntity(value) && agateAsEntity(value)->kind == kind; }

static inline bool agateIsArray(AgateValue value) { return agateIsEntityKind(value, AGATE_ENTITY_ARRAY); }
static inline bool agateIsClass(AgateValue value) { return agateIsEntityKind(value, AGATE_ENTITY_CLASS); }
static inline bool agateIsClosure(AgateValue value) { return agateIsEntityKind(value, AGATE_ENTITY_CLOSURE); }
static inline bool agateIsForeign(AgateValue value) { return agateIsEntityKind(value, AGATE_ENTITY_FOREIGN); }
static inline bool agateIsFunction(AgateValue value) { return agateIsEntityKind(value, AGATE_ENTITY_FUNCTION); }
static inline bool agateIsInstance(AgateValue value) { return agateIsEntityKind(value, AGATE_ENTITY_INSTANCE); }
static inline bool agateIsMap(AgateValue value) { return agateIsEntityKind(value, AGATE_ENTITY_MAP); }
static inline bool agateIsRange(AgateValue value) { return agateIsEntityKind(value, AGATE_ENTITY_RANGE); }
static inline bool agateIsString(AgateValue value) { return agateIsEntityKind(value, AGATE_ENTITY_STRING); }
static inline bool agateIsUnit(AgateValue value) { return agateIsEntityKind(value, AGATE_ENTITY_UNIT); }
static inline bool agateIsUpvalue(AgateValue value) { return agateIsEntityKind(value, AGATE_ENTITY_UPVALUE); }

static inline AgateArray *agateAsArray(AgateValue value) { return (AgateArray *) agateAsEntity(value); }
static inline AgateClass *agateAsClass(AgateValue value) { return (AgateClass *) agateAsEntity(value); }
static inline AgateClosure *agateAsClosure(AgateValue value) { return (AgateClosure *) agateAsEntity(value); }
static inline AgateForeign *agateAsForeign(AgateValue value) { return (AgateForeign *) agateAsEntity(value); }
static inline AgateFunction *agateAsFunction(AgateValue value) { return (AgateFunction *) agateAsEntity(value); }
static inline AgateInstance *agateAsInstance(AgateValue value) { return (AgateInstance *) agateAsEntity(value); }
static inline AgateMap *agateAsMap(AgateValue value) { return (AgateMap *) agateAsEntity(value); }
static inline AgateRange *agateAsRange(AgateValue value) { return (AgateRange *) agateAsEntity(value); }
static inline AgateString *agateAsString(AgateValue value) { return (AgateString *) agateAsEntity(value); }
static inline AgateUnit *agateAsUnit(AgateValue value) { return (AgateUnit *) agateAsEntity(value); }
static inline AgateUpvalue *agateAsUpvalue(AgateValue value) { return (AgateUpvalue *) agateAsEntity(value); }

static inline const char *agateAsCString(AgateValue value) { return ((AgateString *) agateAsEntity(value))->data; }

static inline bool agateStringCompare(AgateValue value, const char *data, ptrdiff_t size) {
  if (!agateIsString(value)) {
    return false;
  }

  AgateString *string = agateAsString(value);
  return string->size == size && memcmp(string->data, data, size) == 0;
}

static inline AgateClass *agateValueGetClass(AgateValue value, AgateVM *vm) {
  switch (value.kind) {
    case AGATE_VALUE_UNDEFINED:
      assert(false);
      return NULL;
    case AGATE_VALUE_NIL:
      return vm->nil_class;
    case AGATE_VALUE_BOOL:
      return vm->bool_class;
    case AGATE_VALUE_CHAR:
      return vm->char_class;
    case AGATE_VALUE_INT:
      return vm->int_class;
    case AGATE_VALUE_FLOAT:
      return vm->float_class;
    case AGATE_VALUE_ENTITY:
      return agateAsEntity(value)->type;
  }

  assert(false);
  return NULL;
}

/*
 * equals
 */

static inline bool agateEntityEquals(AgateValue a, AgateValue b) {
  assert(agateIsEntity(a) && agateIsEntity(b));
  AgateEntityKind kind = agateEntityKind(a);

  if (agateEntityKind(b) != kind) {
    return false;
  }

  switch (kind) {
    case AGATE_ENTITY_RANGE: {
      AgateRange *ra = agateAsRange(a);
      AgateRange *rb = agateAsRange(b);
      return ra->from == rb->from && ra->to == rb->to && ra->kind == rb->kind;
    }

    case AGATE_ENTITY_STRING: {
      AgateString *sa = agateAsString(a);
      AgateString *sb = agateAsString(b);
      return sa->hash == sb->hash && sa->size == sb->size && memcmp(sa->data, sb->data, sa->size) == 0;
    }

    default:
      return false;
  }
  assert(false);
  return false;
}

static inline bool agateValueSame(AgateValue a, AgateValue b) {
  AgateValueKind kind = a.kind;

  if (b.kind != kind) {
    return false;
  }

  switch (kind) {
    case AGATE_VALUE_UNDEFINED:
      return true;
    case AGATE_VALUE_NIL:
      return true;
    case AGATE_VALUE_BOOL:
      return agateAsBool(a) == agateAsBool(b);
    case AGATE_VALUE_CHAR:
      return agateAsChar(a) == agateAsChar(b);
    case AGATE_VALUE_INT:
      return agateAsInt(a) == agateAsInt(b);
    case AGATE_VALUE_FLOAT:
      return agateAsFloat(a) == agateAsFloat(b);
    case AGATE_VALUE_ENTITY:
      return agateAsEntity(a) == agateAsEntity(b);
  }

  assert(false);
  return false;
}

static bool agateValueEquals(AgateValue a, AgateValue b) {
  if (agateValueSame(a, b)) {
    return true;
  }

  if (agateIsEntity(a) && agateIsEntity(b)) {
    return agateEntityEquals(a, b);
  }

  return false;
}

/*
 * hash
 */

static inline bool agateHasNativeHash(AgateValue value) {
  switch (value.kind) {
    case AGATE_VALUE_NIL:
    case AGATE_VALUE_BOOL:
    case AGATE_VALUE_CHAR:
    case AGATE_VALUE_INT:
    case AGATE_VALUE_FLOAT:
      return true;
    case AGATE_VALUE_ENTITY:
      switch (agateEntityKind(value)) {
        case AGATE_ENTITY_STRING:
        case AGATE_ENTITY_CLASS:
        case AGATE_ENTITY_RANGE:
          return true;
        default:
          return false;
      }
    default:
      return false;
  }

  return false;
}

// based on https://xorshift.di.unimi.it/splitmix64.c
static inline uint64_t agateSplitMix64(uint64_t hash) {
  hash += UINT64_C(0x9e3779b97f4a7c15);
  hash = (hash ^ (hash >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
  hash = (hash ^ (hash >> 27)) * UINT64_C(0x94d049bb133111eb);
  return hash ^ (hash >> 31);
}

static inline uint64_t agateRangeHash(const AgateRange *range) {
  return agateSplitMix64(range->from) ^ agateSplitMix64(range->to);
}

static uint64_t agateEntityHash(const AgateEntity *entity) {
  switch (entity->kind) {
    case AGATE_ENTITY_ARRAY:
    case AGATE_ENTITY_CLOSURE:
    case AGATE_ENTITY_FOREIGN:
    case AGATE_ENTITY_INSTANCE:
    case AGATE_ENTITY_MAP:
    case AGATE_ENTITY_UNIT:
    case AGATE_ENTITY_UPVALUE:
      assert(false);
      return 0;

    case AGATE_ENTITY_CLASS: {
      const AgateClass *klass = (const AgateClass *) entity;
      return klass->name->hash;
    }

    case AGATE_ENTITY_RANGE: {
      const AgateRange *range = (const AgateRange *) entity;
      return agateRangeHash(range);
    }

    case AGATE_ENTITY_STRING: {
      const AgateString *string = (const AgateString *) entity;
      return string->hash;
    }

    // internal use
    case AGATE_ENTITY_FUNCTION: {
      const AgateFunction *function = (const AgateFunction *) entity;
      return function->name->hash ^ agateSplitMix64(function->arity);
    }
  }

  assert(false);
  return 0;
}

static inline uint64_t agateHashNil() {
  return 0;
}

static inline uint64_t agateHashBool(bool value) {
  return value ? 1 : 2;
}

static inline uint64_t agateHashInt(int64_t value) {
  return agateSplitMix64(value);
}

static inline uint64_t agateHashFloat(double value) {
  uint64_t bits;
  memcpy(&bits, &value, sizeof(value));
  return agateSplitMix64(bits);
}

static inline uint64_t agateHashChar(uint32_t value) {
  return agateSplitMix64(value);
}

static uint64_t agateValueHash(AgateValue value) {
  switch (value.kind) {
    case AGATE_VALUE_UNDEFINED:
      assert(false);
      return 0;
    case AGATE_VALUE_NIL:
      return agateHashNil();
    case AGATE_VALUE_BOOL:
      return agateHashBool(agateAsBool(value));
    case AGATE_VALUE_CHAR:
      return agateHashChar(agateAsChar(value));
    case AGATE_VALUE_INT:
      return agateHashInt(agateAsInt(value));
    case AGATE_VALUE_FLOAT:
      return agateHashFloat(agateAsFloat(value));
    case AGATE_VALUE_ENTITY:
      return agateEntityHash(agateAsEntity(value));
  }

  assert(false);
  return 0;
}

/*
 * bytecode
 */

static void agateBytecodeCreate(AgateBytecode *self) {
  agateBytecodeArrayCreate(&self->code);
  agateValueArrayCreate(&self->constants);
  agateLineInfoArrayCreate(&self->lines);
}

static void agateBytecodeDestroy(AgateBytecode *self, AgateVM *vm) {
  agateLineInfoArrayDestroy(&self->lines, vm);
  agateValueArrayDestroy(&self->constants, vm);
  agateBytecodeArrayDestroy(&self->code, vm);
}

static void agateBytecodeWrite(AgateBytecode *self, uint8_t byte, int line, AgateVM *vm) {
  if (self->lines.size == 0 || self->lines.data[self->lines.size - 1].line != line) {
    AgateLineInfo info;
    info.line = line;
    info.start = self->code.size;
    info.size = 1;
    agateLineInfoArrayAppend(&self->lines, info, vm);
  } else {
    ++self->lines.data[self->lines.size - 1].size;
  }

  agateBytecodeArrayAppend(&self->code, byte, vm);
}

static ptrdiff_t agateBytecodeAddConstant(AgateBytecode *self, AgateValue value, AgateVM *vm) {
  if (agateIsEntity(value)) {
    agatePushRoot(vm, agateAsEntity(value));
  }

  agateValueArrayAppend(&self->constants, value, vm);

  if (agateIsEntity(value)) {
    agatePopRoot(vm);
  }

  return self->constants.size - 1;
}

static int agateBytecodeLineFromOffset(const AgateBytecode *bc, ptrdiff_t offset) {
  // TODO: binary search
  for (ptrdiff_t i = 0; i < bc->lines.size; ++i) {
    AgateLineInfo *info = &bc->lines.data[i];

    if (info->start <= offset && offset < info->start + info->size) {
      return info->line;
    }
  }

  return -1;
}

/*
 * value array
 */

static void agateValueArrayInsert(AgateValueArray *self, ptrdiff_t index, AgateValue value, AgateVM *vm) {
  assert(self);
  assert(0 <= index && index <= self->size);

  if (self->size == self->capacity) {
    if (agateIsEntity(value)) {
      agatePushRoot(vm, agateAsEntity(value));
    }

    agateValueArrayGrow(self, self->size + 1, vm);

    if (agateIsEntity(value)) {
      agatePopRoot(vm);
    }
  }

  for (ptrdiff_t current = self->size; current > index; --current) {
    self->data[current] = self->data[current - 1];
  }

  self->data[index] = value;
  ++self->size;
}

static AgateValue agateValueArrayErase(AgateValueArray *self, ptrdiff_t index) {
  assert(self);
  assert(0 <= index && index < self->size);

  AgateValue value = self->data[index];

  for (ptrdiff_t current = index; current < self->size - 1; ++current) {
    self->data[current] = self->data[current + 1];
  }

  --self->size;
  return value;
}

static ptrdiff_t agateValueArrayFind(AgateValueArray *self, AgateValue needle) {
  assert(self);
  const ptrdiff_t size = self->size;

  for (ptrdiff_t i = 0; i < size; ++i) {
    if (agateValueEquals(self->data[i], needle)) {
      return i;
    }
  }

  return -1;
}

AGATE_ARRAY_DEFINE_RESIZE(ValueArray, AgateValue)

/*
 * hash table
 */

// 64-bit FNV-1a hash
static uint64_t agateStringHash(const char *data, ptrdiff_t size) {
  uint64_t hash = UINT64_C(14695981039346656037);

  for (ptrdiff_t i = 0; i < size; ++i) {
    hash ^= (uint8_t) data[i];
    hash *= UINT64_C(1099511628211);
  }

  return hash;
}

static void agateTableCreate(AgateTable *self) {
  assert(self);
  self->capacity = 0;
  self->size = 0;
  self->entries = NULL;
}

static void agateTableClear(AgateTable *self) {
  assert(self);
  self->size = 0;

  for (ptrdiff_t i = 0; i < self->capacity; ++i) {
    self->entries[i].key = agateUndefinedValue();
    self->entries[i].value = agateBoolValue(false);
  }
}

static void agateTableDestroy(AgateTable *self, AgateVM *vm) {
  assert(self);
  agateFreeArray(vm, AgateTableEntry, self->entries, self->capacity);
  self->capacity = 0;
  self->size = 0;
  self->entries = NULL;
}

static bool agateEntriesSearch(AgateTableEntry *entries, ptrdiff_t capacity, AgateValue key, uint64_t hash, AgateTableEntry **result) {
  assert(result);

  if (capacity == 0) {
    return false;
  }

  ptrdiff_t start = hash % capacity;
  ptrdiff_t index = start;

  AgateTableEntry *tombstone = NULL;

  do {
    AgateTableEntry *entry = &entries[index];

    if (agateIsUndefined(entry->key)) {
      assert(agateIsBool(entry->value));

      if (agateAsBool(entry->value)) {
        // tombstone
        if (tombstone == NULL) {
          tombstone = entry;
        }
      } else {
        // empty slot, end of the search
        *result = tombstone != NULL ? tombstone : entry;
        return false;
      }
    } else if (agateValueEquals(entry->key, key)) {
      *result = entry;
      return true;
    }

    index = (index + 1) % capacity;
  } while (index != start);

  assert(tombstone != NULL);
  *result = tombstone;
  return false;
}

static bool agateEntriesInsert(AgateTableEntry *entries, ptrdiff_t capacity, AgateValue key, AgateValue value, uint64_t hash) {
  assert(entries != NULL);
  assert(capacity > 0);

  AgateTableEntry *entry;

  if (agateEntriesSearch(entries, capacity, key, hash, &entry)) {
    // present
    entry->value = value;
    return false;
  }

  entry->key = key;
  entry->value = value;
  entry->hash = hash;
  return true;
}

static void agateTableGrow(AgateTable *self, ptrdiff_t capacity, AgateVM *vm) {
  AgateTableEntry *entries = agateAllocate(vm, AgateTableEntry, capacity);

  for (ptrdiff_t i = 0; i < capacity; ++i) {
    entries[i].key = agateUndefinedValue();
    entries[i].value = agateBoolValue(false);
  }

  if (self->capacity > 0) {
    for (ptrdiff_t i = 0; i < self->capacity; ++i) {
      AgateTableEntry *entry = &self->entries[i];

      if (agateIsUndefined(entry->key)) {
        continue;
      }

      agateEntriesInsert(entries, capacity, entry->key, entry->value, entry->hash);
    }
  }

  agateFreeArray(vm, AgateTableEntry, self->entries, self->capacity);
  self->entries = entries;
  self->capacity = capacity;
}

static AgateValue agateTableFind(AgateTable *self, AgateValue key, uint64_t hash) {
  AgateTableEntry *entry;

  if (agateEntriesSearch(self->entries, self->capacity, key, hash, &entry)) {
    return entry->value;
  }

  return agateUndefinedValue();
}

static inline AgateValue agateTableHashFind(AgateTable *self, AgateValue key) {
  return agateTableFind(self, key, agateValueHash(key));
}

static bool agateTableInsert(AgateTable *self, AgateValue key, AgateValue value, uint64_t hash, AgateVM *vm) {
  if (self->size + 1 > self->capacity * AGATE_TABLE_MAX_LOAD_NUM / AGATE_TABLE_MAX_LOAD_DEN) {
    ptrdiff_t capacity = agateGrowCapacity(self->capacity);
    agateTableGrow(self, capacity, vm);
  }

  if (agateEntriesInsert(self->entries, self->capacity, key, value, hash)) {
    ++self->size;
    return true;
  }

  return false;
}

static inline bool agateTableHashInsert(AgateTable *self, AgateValue key, AgateValue value, AgateVM *vm) {
  return agateTableInsert(self, key, value, agateValueHash(key), vm);
}

static AgateValue agateTableErase(AgateTable *self, AgateValue key, uint64_t hash) {
  AgateTableEntry *entry;

  if (!agateEntriesSearch(self->entries, self->capacity, key, hash, &entry)) {
    return agateNilValue();
  }

  AgateValue value = entry->value;
  entry->key = agateUndefinedValue();
  entry->value = agateBoolValue(true); // tombstone
  entry->hash = 0;

  --self->size;

  return value;
}

static inline AgateValue agateTableHashErase(AgateTable *self, AgateValue key) {
  return agateTableErase(self, key, agateValueHash(key));
}

/*
 * entities - new
 */

static AgateEntity *agateEntityNew(ptrdiff_t size, AgateEntityKind kind, ptrdiff_t insize, ptrdiff_t count, AgateClass *klass, AgateVM *vm) {
  ptrdiff_t total = size + insize * count;
  AgateEntity *entity = (AgateEntity *) agateMemoryHandle(vm, NULL, 0, total);
  entity->kind = kind;
  entity->status = AGATE_ENTITY_STATUS_WHITE;
  entity->type = klass;
  entity->next = vm->entities;

  vm->entities = entity;

#ifdef AGATE_DEBUG_LOG_GC
  if (insize == 0) {
    printf("%p allocate %td for kind %d\n", (void *) entity, size, kind);
  } else {
    printf("%p allocate %td (%td + %td * %td) for kind %d\n", (void *) entity, total, size, insize, count, kind);
  }
#endif

  return entity;
}

#define agateAllocateEntity(vm, type, kind, klass) ((type *) agateEntityNew(sizeof(type), (kind), 0, 0, (klass), (vm)))
#define agateAllocateFlexEntity(vm, type, intype, count, kind, klass) ((type *) agateEntityNew(sizeof(type), (kind), sizeof(intype), (count), (klass), (vm)))

// String

static inline AgateString *agateStringAllocate(AgateVM *vm, ptrdiff_t size, uint64_t hash) {
  AgateString *string = agateAllocateFlexEntity(vm, AgateString, char, (size + 1), AGATE_ENTITY_STRING, vm->string_class);
  string->size = size;
  string->hash = hash;
  return string;
}

static AgateString *agateStringNew(AgateVM *vm, const char *data, ptrdiff_t size) {
  uint64_t hash = agateStringHash(data, size);
  AgateString *string = agateStringAllocate(vm, size, hash);
  memcpy(string->data, data, size);
  string->data[size] = '\0';
  return string;
}

static AgateString *agateStringNewFormat(AgateVM *vm, const char *format, ...) {
  va_list args;
  va_start(args, format);
  ptrdiff_t size = 0;

  for (const char *ptr = format; *ptr != '\0'; ++ptr) {
    switch (*ptr) {
      case '$':
        size += strlen(va_arg(args, const char *));
        break;

      case '@':
        size += va_arg(args, AgateString *)->size;
        break;

      default:
        ++size;
    }
  }

  va_end(args);

  AgateString *string = agateStringAllocate(vm, size, 0);
  char *out = string->data;

  va_start(args, format);

  for (const char *ptr = format; *ptr != '\0'; ++ptr) {
    switch (*ptr) {
      case '$':
      {
        const char *raw = va_arg(args, const char *);
        size = strlen(raw);
        memcpy(out, raw, size);
        out += size;
        break;
      }

      case '@':
      {
        AgateString *raw = va_arg(args, AgateString *);
        memcpy(out, raw->data, raw->size);
        out += raw->size;
        break;
      }

      default:
        *out++ = *ptr;
    }
  }
  *out++ = '\0';

  va_end(args);

  string->hash = agateStringHash(string->data, string->size);
  return string;
}

static ptrdiff_t agateStringFind(AgateString *haystack, const char *needle, ptrdiff_t needle_size, ptrdiff_t start) {
  const ptrdiff_t haystack_size = haystack->size;

  if (needle_size == 0) {
    return start;
  }

  if (start >= haystack_size) {
    return -1;
  }

  if (start + needle_size > haystack_size) {
    return -1;
  }

  ptrdiff_t skip[UINT8_MAX];

  for (ptrdiff_t i = 0; i < UINT8_MAX; ++i) {
    skip[i] = needle_size;
  }

  for (ptrdiff_t i = 0; i < needle_size - 1; ++i) {
    uint8_t c = needle[i];
    skip[c] = needle_size - i - 1;
  }

  ptrdiff_t index = start;
  ptrdiff_t max = haystack_size - needle_size;
  uint8_t last = needle[needle_size - 1];

  while (index <= max) {
    uint8_t c = haystack->data[index + needle_size - 1];

    if (c == last) {
      if (memcmp(haystack->data + index, needle, needle_size - 1) == 0) {
        return index;
      }
    }

    index += skip[c];
  }

  return -1;
}

static AgateString *agateStringReplace(AgateVM *vm, AgateString *text, AgateString *from, AgateString *to) {
  AgateCharArray buffer;
  agateCharArrayCreate(&buffer);

  ptrdiff_t index = 0;

  do {
    ptrdiff_t next = agateStringFind(text, from->data, from->size, index);

    if (next == -1) {
      break;
    }

    if (next > index) {
      agateCharArrayAppendMultiple(&buffer, text->data + index, next - index, vm);
    }

    agateCharArrayAppendMultiple(&buffer, to->data, to->size, vm);
    index = next + from->size;
  } while (index < text->size);

  // add the end
  if (index < text->size) {
    agateCharArrayAppendMultiple(&buffer, text->data + index, text->size - index, vm);
  }

  AgateString *result = agateStringNew(vm, buffer.data, buffer.size);
  agateCharArrayDestroy(&buffer, vm);
  return result;
}

typedef enum {
  AGATE_TRIM_NONE  = 0,
  AGATE_TRIM_LEFT  = 1,
  AGATE_TRIM_RIGHT = 2,
  AGATE_TRIM_BOTH  = 3,
} AgateTrimKind;

static AgateString *agateStringTrim(AgateVM *vm, AgateString *text, AgateString *chars, AgateTrimKind kind) {
  ptrdiff_t start = 0;
  ptrdiff_t stop = text->size;

  if (kind == AGATE_TRIM_LEFT || kind == AGATE_TRIM_BOTH) {
    ptrdiff_t index = 0;

    while (index < chars->size) {
      ptrdiff_t size = agateUtf8DecodeSize(chars->data + index);

      if (start + size <= text->size && memcmp(text->data + start, chars->data + index, size) == 0) {
        start += size;
        index = 0;
      } else {
        index += size;
      }
    }
  }

  if (kind == AGATE_TRIM_RIGHT || kind == AGATE_TRIM_BOTH) {
    ptrdiff_t index = 0;

    while (index < chars->size) {
      ptrdiff_t size = agateUtf8DecodeSize(chars->data + index);

      if (stop - size >= start && memcmp(text->data + stop - size, chars->data + index, size) == 0) {
        stop -= size;
        index = 0;
      } else {
        index += size;
      }
    }
  }

  return agateStringNew(vm, text->data + start, stop - start);
}


// Array

static AgateArray *agateArrayNew(AgateVM *vm) {
  AgateArray *array = agateAllocateEntity(vm, AgateArray, AGATE_ENTITY_ARRAY, vm->array_class);
  agateValueArrayCreate(&array->elements);
  return array;
}

// Class

static AgateClass *agateClassNewBare(AgateVM *vm, AgateUnit *unit, int field_count, AgateString *name) {
  AgateClass *klass = agateAllocateEntity(vm, AgateClass, AGATE_ENTITY_CLASS, NULL);
  klass->unit = unit;
  klass->supertype = NULL;
  klass->field_count = field_count;
  klass->name = name;
  agateMethodArrayCreate(&klass->methods);
  return klass;
}

static void agateClassBindMethod(AgateVM *vm, AgateClass *klass, ptrdiff_t symbol, AgateMethod method) {
  if (symbol >= klass->methods.size) {
    AgateMethod none;
    none.kind = AGATE_METHOD_NONE;
    agateMethodArrayResize(&klass->methods, symbol + 1, none, vm);
  }

  assert(symbol < klass->methods.size);
  klass->methods.data[symbol] = method;
}

static void agateClassBindSuperclass(AgateVM *vm, AgateClass *subclass, AgateClass *superclass) {
  assert(superclass);
  subclass->supertype = superclass;

  if (subclass->field_count != -1) {
    subclass->field_count += superclass->field_count;
  } else {
    assert(superclass->field_count == 0);
  }

  for (ptrdiff_t i = 0; i < superclass->methods.size; ++i) {
    agateClassBindMethod(vm, subclass, i, superclass->methods.data[i]);
  }
}

static AgateClass *agateClassNew(AgateVM *vm, AgateUnit *unit, AgateClass *superclass, int field_count, AgateString *name) {
  assert(superclass->base.kind == AGATE_ENTITY_CLASS);

  // metaclass creation
  AgateString *metaclass_name = agateStringNewFormat(vm, "@ metaclass", name);
  agatePushRoot(vm, (AgateEntity *) metaclass_name);

  AgateClass *metaclass = agateClassNewBare(vm, unit, 0, metaclass_name);
  metaclass->base.type = vm->class_class;

  agatePopRoot(vm);
  agatePushRoot(vm, (AgateEntity *) metaclass);

  agateClassBindSuperclass(vm, metaclass, vm->class_class);

  // class creation
  AgateClass *klass = agateClassNewBare(vm, unit, field_count, name);
  agatePushRoot(vm, (AgateEntity *) klass);

  klass->base.type = metaclass;

  agateClassBindSuperclass(vm, klass, superclass);

  agatePopRoot(vm);
  agatePopRoot(vm);

  return klass;
}

// Closure

static AgateClosure *agateClosureNew(AgateVM *vm, AgateFunction *function) {
  assert(function);
  AgateClosure *closure = agateAllocateFlexEntity(vm, AgateClosure, AgateUpvalue *, function->upvalue_count, AGATE_ENTITY_CLOSURE, vm->fn_class);
  closure->function = function;
  closure->upvalue_count = function->upvalue_count;

  for (ptrdiff_t i = 0; i < function->upvalue_count; ++i) {
    closure->upvalues[i] = NULL;
  }

  return closure;
}

// Foreign

static ptrdiff_t agateSymbolTableFind(AgateTable *self, const char *name, ptrdiff_t size);

static AgateForeign *agateForeignNew(AgateVM *vm, AgateClass *klass) {
  assert(klass->field_count == -1);

  ptrdiff_t symbol = agateSymbolTableFind(&vm->method_names, "<allocate>", 10);
  assert(symbol != -1);

  assert(symbol < klass->methods.size);
  AgateMethod *method = &klass->methods.data[symbol];
  assert(method->kind == AGATE_METHOD_FOREIN_ALLOCATE);

  ptrdiff_t data_size = method->as.foreign_allocate(vm, klass->unit->name->data, klass->name->data);

  AgateForeign *foreign = agateAllocateFlexEntity(vm, AgateForeign, uint8_t, data_size, AGATE_ENTITY_FOREIGN, klass);
  foreign->data_size = data_size;
  memset(foreign->data, 0, data_size);
  return foreign;
}

static void agateForeignDestroy(AgateVM *vm, AgateForeign *foreign) {
  ptrdiff_t symbol = agateSymbolTableFind(&vm->method_names, "<destroy>", 9);

  if (symbol == -1) {
    return;
  }

  AgateClass *klass = foreign->base.type;

  if (symbol >= klass->methods.size) {
    return;
  }

  AgateMethod *method = &klass->methods.data[symbol];

  if (method->kind == AGATE_METHOD_NONE) {
    return;
  }

  assert(method->kind == AGATE_METHOD_FOREIN_DESTROY);
  method->as.foreign_destroy(vm, klass->unit->name->data, klass->name->data, foreign->data);
}

// Function

static AgateFunction *agateFunctionNew(AgateVM *vm, AgateUnit *unit, ptrdiff_t slot_count) {
  AgateFunction *function = agateAllocateEntity(vm, AgateFunction, AGATE_ENTITY_FUNCTION, vm->fn_class);
  agateBytecodeCreate(&function->bc);
  function->unit = unit;
  function->arity = 0;
  function->slot_count = slot_count;
  function->upvalue_count = 0;
  function->name = NULL;
  return function;
}

static void agateFunctionBindName(AgateVM *vm, AgateFunction *function, const char *name, ptrdiff_t size) {
  function->name = agateStringNew(vm, name, size);
}

// Instance

static AgateInstance *agateInstanceNew(AgateVM *vm, AgateClass *klass) {
  AgateInstance *instance = agateAllocateFlexEntity(vm, AgateInstance, AgateValue, klass->field_count, AGATE_ENTITY_INSTANCE, klass);
  instance->field_count = klass->field_count;

  for (ptrdiff_t i = 0; i < klass->field_count; ++i) {
    instance->fields[i] = agateNilValue();
  }

  return instance;
}

// Map

static AgateMap *agateMapNew(AgateVM *vm) {
  AgateMap *map = agateAllocateEntity(vm, AgateMap, AGATE_ENTITY_MAP, vm->map_class);
  agateTableCreate(&map->members);
  return map;
}

// Unit

static AgateUnit *agateUnitNew(AgateVM *vm, AgateString *name) {
  AgateUnit *unit = agateAllocateEntity(vm, AgateUnit, AGATE_ENTITY_UNIT, NULL);
  agateValueArrayCreate(&unit->object_values);
  agateTableCreate(&unit->object_names);
  unit->name = name;
  return unit;
}

// Range

static AgateRange *agateRangeNew(AgateVM *vm, int64_t from, int64_t to, AgateRangeKind kind) {
  AgateRange *range = agateAllocateEntity(vm, AgateRange, AGATE_ENTITY_RANGE, vm->range_class);
  range->from = from;
  range->to = to;
  range->kind = kind;
  return range;
}

// Upvalue

static AgateUpvalue *agateUpvalueNew(AgateVM *vm, AgateValue *slot) {
  AgateUpvalue *upvalue = agateAllocateEntity(vm, AgateUpvalue, AGATE_ENTITY_UPVALUE, NULL);
  upvalue->location = slot;
  upvalue->closed = agateNilValue();
  upvalue->next = NULL;
  return upvalue;
}


static void agateEntityDelete(AgateEntity *entity, AgateVM *vm) {
#ifdef AGATE_DEBUG_LOG_GC
  printf("%p free for kind %d\n", (void *) entity, entity->kind);
#endif

  switch (entity->kind) {
    case AGATE_ENTITY_ARRAY: {
      AgateArray *array = (AgateArray *) entity;
      agateValueArrayDestroy(&array->elements, vm);
      agateFree(vm, AgateArray, array);
      break;
    }

    case AGATE_ENTITY_CLASS: {
      AgateClass *klass = (AgateClass *) entity;
      agateMethodArrayDestroy(&klass->methods, vm);
      agateFree(vm, AgateClass, klass);
      break;
    }

    case AGATE_ENTITY_CLOSURE: {
      AgateClosure *closure = (AgateClosure *) entity;
      agateFreeFlex(vm, AgateClosure, closure, AgateUpvalue *, closure->upvalue_count);
      break;
    }

    case AGATE_ENTITY_FOREIGN: {
      AgateForeign *foreign = (AgateForeign *) entity;
      agateForeignDestroy(vm, foreign);
      agateFreeFlex(vm, AgateForeign, foreign, uint8_t, foreign->data_size);
      break;
    }

    case AGATE_ENTITY_FUNCTION: {
      AgateFunction *function = (AgateFunction *) entity;
      agateBytecodeDestroy(&function->bc, vm);
      agateFree(vm, AgateFunction, function);
      break;
    }

    case AGATE_ENTITY_INSTANCE: {
      AgateInstance *instance = (AgateInstance *) entity;
      agateFreeFlex(vm, AgateInstance, instance, AgateValue, instance->field_count);
      break;
    }

    case AGATE_ENTITY_MAP: {
      AgateMap *map = (AgateMap *) entity;
      agateTableDestroy(&map->members, vm);
      agateFree(vm, AgateMap, map);
      break;
    }

    case AGATE_ENTITY_UNIT: {
      AgateUnit *unit = (AgateUnit *) entity;
      agateValueArrayDestroy(&unit->object_values, vm);
      agateTableDestroy(&unit->object_names, vm);
      agateFree(vm, AgateUnit, unit);
      break;
    }

    case AGATE_ENTITY_RANGE: {
      agateFree(vm, AgateRange, entity);
      break;
    }

    case AGATE_ENTITY_STRING: {
      AgateString *string = (AgateString *) entity;
      agateFreeFlex(vm, AgateString, string, char, (string->size + 1));
      break;
    }

    case AGATE_ENTITY_UPVALUE: {
      agateFree(vm, AgateUpvalue, entity);
      break;
    }
  }
}


/*
 * symbol table
 */

static ptrdiff_t agateSymbolTableFind(AgateTable *self, const char *name, ptrdiff_t size) {
  if (self->capacity == 0) {
    return -1;
  }

  uint64_t hash = agateStringHash(name, size);

  ptrdiff_t start = hash % self->capacity;
  ptrdiff_t index = start;

  do {
    AgateTableEntry *entry = &self->entries[index];

    if (agateIsUndefined(entry->key)) {
      assert(agateIsBool(entry->value));
      if (!agateAsBool(entry->value)) {
        // not a tombstone
        return -1;
      }
    } else if (agateStringCompare(entry->key, name, size)) {
      assert(agateIsInt(entry->value));
      return agateAsInt(entry->value);
    }

    index = (index + 1) % self->capacity;
  } while (index != start);

  return -1;
}

static ptrdiff_t agateSymbolTableInsert(AgateTable *self, const char *name, ptrdiff_t size, AgateVM *vm) {
  AgateString *string = agateStringNew(vm, name, size);
  ptrdiff_t symbol = self->size;
  agatePushRoot(vm, (AgateEntity *) string);
  bool inserted = agateTableInsert(self, agateEntityValue(string), agateIntValue(symbol), string->hash, vm);
  agatePopRoot(vm);
  assert(inserted);
  return symbol;
}

static ptrdiff_t agateSymbolTableEnsure(AgateTable *self, const char *name, ptrdiff_t size, AgateVM *vm) {
  ptrdiff_t symbol = agateSymbolTableFind(self, name, size);

  if (symbol != -1) {
    return symbol;
  }

  return agateSymbolTableInsert(self, name, size, vm);
}

static AgateString *agateSymbolTableReverseFind(AgateTable *self, ptrdiff_t symbol) {
  for (ptrdiff_t i = 0; i < self->capacity; ++i) {
    AgateTableEntry *entry = &self->entries[i];

    if (agateIsUndefined(entry->key)) {
      continue;
    }

    assert(agateIsString(entry->key));

    if (agateAsInt(entry->value) == symbol) {
      return agateAsString(entry->key);
    }
  }

  assert(false);
  return NULL;
}

/*
 * entities - unit
 */

#define AGATE_DEFINITION_ALREADY_DEFINED (-1)
#define AGATE_DEFINITION_TOO_MANY_DEFINITIONS (-2)

static ptrdiff_t agateUnitAddFutureVariable(AgateVM *vm, AgateUnit *unit, const char *name, ptrdiff_t size, int line) { // ~wrenDeclareVariable
  if (unit->object_values.size == AGATE_MAX_UNIT_OBJECTS) {
    return AGATE_DEFINITION_TOO_MANY_DEFINITIONS;
  }

  agateValueArrayAppend(&unit->object_values, agateIntValue(line), vm);
  return agateSymbolTableInsert(&unit->object_names, name, size, vm);
}

static ptrdiff_t agateUnitAddVariable(AgateVM *vm, AgateUnit *unit, const char *name, ptrdiff_t size, AgateValue value) { // ~wrenDefineVariable
  assert(unit);
  assert(unit->object_names.size == unit->object_values.size);

  if (unit->object_values.size == AGATE_MAX_UNIT_OBJECTS) {
    return AGATE_DEFINITION_TOO_MANY_DEFINITIONS;
  }

  if (agateIsEntity(value)) {
    agatePushRoot(vm, agateAsEntity(value));
  }

  ptrdiff_t symbol = agateSymbolTableFind(&unit->object_names, name, size);

  if (symbol == -1) {
    symbol = agateSymbolTableInsert(&unit->object_names, name, size, vm);
    agateValueArrayAppend(&unit->object_values, value, vm);
  } else if (agateIsInt(unit->object_values.data[symbol])) {
    unit->object_values.data[symbol] = value;
  } else {
    symbol = AGATE_DEFINITION_ALREADY_DEFINED;
  }

  if (agateIsEntity(value)) {
    agatePopRoot(vm);
  }

  return symbol;
}

static AgateValue agateUnitFindVariable(AgateVM *vm, AgateUnit *unit, const char *name) {
  int64_t symbol = agateSymbolTableFind(&unit->object_names, name, strlen(name));
  assert(symbol < unit->object_values.size);
  return unit->object_values.data[symbol];
}

static AgateValue agateUnitGetVariable(AgateVM *vm, AgateUnit *unit, AgateValue name) {
  assert(agateIsString(name));
  AgateString *variable = agateAsString(name);
  ptrdiff_t symbol = agateSymbolTableFind(&unit->object_names, variable->data, variable->size);

  if (symbol != -1) {
    assert(symbol < unit->object_values.size);
    return unit->object_values.data[symbol];
  }

  vm->error = agateEntityValue(agateStringNewFormat(vm, "Could not find an object named '@' in unit '@'.", variable, unit->name));
  return agateNilValue();
}

static bool agateCheckArity(AgateVM *vm, AgateValue value, int argc) {
  assert(agateIsClosure(value));
  AgateFunction *function = agateAsClosure(value)->function;

  if (argc - 1 >= function->arity) {
    return true;
  }

  vm->error = agateEntityValue(agateStringNewFormat(vm, "Function expects more arguments."));
  return false;
}

/* debug */

#if defined(AGATE_DEBUG_PRINT_CODE) || defined(AGATE_DEBUG_TRACE_EXECUTION) || defined(AGATE_DEBUG_LOG_GC)
#include "debug.inc.c"
#endif

/*
 * memory - gc
 */

static void agateMarkEntity(AgateVM *vm, AgateEntity *entity) {
  if (entity == NULL) {
    return;
  }

  if (entity->status == AGATE_ENTITY_STATUS_GRAY) {
    return;
  }

#ifdef AGATE_DEBUG_LOG_GC
  printf("%p mark ", (void *) entity);
  agateValuePrint(agateEntityValue(entity));
  printf("\n");
#endif

  entity->status = AGATE_ENTITY_STATUS_GRAY;

  if (vm->gray_count >= vm->gray_capacity) {
    vm->gray_capacity = agateGrowCapacity(vm->gray_capacity);
    vm->gray_stack = (AgateEntity **) vm->config.reallocate(vm->gray_stack, sizeof(AgateEntity *) * vm->gray_capacity, vm->config.user_data);
    assert(vm->gray_stack != NULL);
  }

  vm->gray_stack[vm->gray_count++] = entity;
}

static void agateMarkValue(AgateVM *vm, AgateValue value) {
  if (agateIsEntity(value)) {
    agateMarkEntity(vm, agateAsEntity(value));
  }
}

static void agateMarkTable(AgateVM *vm, AgateTable *table) {
  for (ptrdiff_t i = 0; i < table->capacity; ++i) {
    AgateTableEntry *entry = &table->entries[i];

    if (agateIsUndefined(entry->key)) {
      continue;
    }

    agateMarkValue(vm, entry->key);
    agateMarkValue(vm, entry->value);
  }
}

static void agateMarkArray(AgateVM *vm, AgateValueArray *array) {
  for (ptrdiff_t i = 0; i < array->size; ++i) {
    agateMarkValue(vm, array->data[i]);
  }
}

static void agateMarkCompilerRoots(AgateVM *vm, AgateCompiler *compiler);

static void agateMarkRoots(AgateVM *vm) {
  agateMarkTable(vm, &vm->units);
  agateMarkTable(vm, &vm->method_names);

  for (AgateValue *slot = vm->stack; slot < vm->stack_top; ++slot) {
    agateMarkValue(vm, *slot);
  }

  for (ptrdiff_t i = 0; i < vm->frames_count; ++i) {
    agateMarkEntity(vm, (AgateEntity *) vm->frames[i].closure);
  }

  for (AgateUpvalue *upvalue = vm->open_upvalues; upvalue != NULL; upvalue = upvalue->next) {
    agateMarkEntity(vm, (AgateEntity *) upvalue);
  }

  for (AgateHandle *handle = vm->handles; handle != NULL; handle = handle->next) {
    agateMarkValue(vm, handle->value);
  }

  if (vm->compiler != NULL) {
    agateMarkCompilerRoots(vm, vm->compiler);
  }

  for (ptrdiff_t i = 0; i < vm->roots_count; ++i) {
    agateMarkEntity(vm, vm->roots[i]);
  }
}

static void agateBlackenObject(AgateVM *vm, AgateEntity *entity) {
#ifdef AGATE_DEBUG_LOG_GC
  printf("%p blacken ", (void *) entity);
  agateValuePrint(agateEntityValue(entity));
  printf("\n");
#endif

  agateMarkEntity(vm, (AgateEntity *) entity->type);

  switch (entity->kind) {
    case AGATE_ENTITY_ARRAY: {
      AgateArray *array = (AgateArray *) entity;
      agateMarkArray(vm, &array->elements);
      break;
    }

    case AGATE_ENTITY_CLASS: {
      AgateClass *klass = (AgateClass *) entity;
      agateMarkEntity(vm, (AgateEntity *) klass->base.type);
      agateMarkEntity(vm, (AgateEntity *) klass->supertype);

      for (ptrdiff_t i = 0; i < klass->methods.size; ++i) {
        if (klass->methods.data[i].kind == AGATE_METHOD_CLOSURE) {
          agateMarkEntity(vm, (AgateEntity *) klass->methods.data[i].as.closure);
        }
      }

      agateMarkEntity(vm, (AgateEntity *) klass->name);
      break;
    }

    case AGATE_ENTITY_CLOSURE: {
      AgateClosure *closure = (AgateClosure *) entity;
      agateMarkEntity(vm, (AgateEntity *) closure->function);

      for (ptrdiff_t i = 0; i < closure->upvalue_count; ++i) {
        agateMarkEntity(vm, (AgateEntity *) closure->upvalues[i]);
      }
      break;
    }

    case AGATE_ENTITY_FOREIGN: {
      break;
    }

    case AGATE_ENTITY_FUNCTION: {
      AgateFunction *function = (AgateFunction *) entity;
      agateMarkEntity(vm, (AgateEntity *) function->name);
      agateMarkArray(vm, &function->bc.constants);
      break;
    }

    case AGATE_ENTITY_INSTANCE: {
      AgateInstance *instance = (AgateInstance *) entity;

      for (ptrdiff_t i = 0; i < instance->field_count; ++i) {
        agateMarkValue(vm, instance->fields[i]);
      }
      break;
    }

    case AGATE_ENTITY_MAP: {
      AgateMap *map = (AgateMap *) entity;
      agateMarkTable(vm, &map->members);
      break;
    }

    case AGATE_ENTITY_UNIT: {
      AgateUnit *unit = (AgateUnit *) entity;
      agateMarkArray(vm, &unit->object_values);
      agateMarkTable(vm, &unit->object_names);
      agateMarkEntity(vm, (AgateEntity *) unit->name);
      break;
    }

    case AGATE_ENTITY_RANGE: {
      break;
    }

    case AGATE_ENTITY_STRING: {
      break;
    }

    case AGATE_ENTITY_UPVALUE: {
      AgateUpvalue *upvalue = (AgateUpvalue *) entity;
      agateMarkValue(vm, upvalue->closed);
      break;
    }
  }
}

static void agateTraceReferences(AgateVM *vm) {
  while (vm->gray_count > 0) {
    --vm->gray_count;
    AgateEntity *entity = vm->gray_stack[vm->gray_count];
    agateBlackenObject(vm, entity);
  }
}

static void agateEntityDelete(AgateEntity *entity, AgateVM *vm);

static void agateSweep(AgateVM *vm) {
  AgateEntity *previous = NULL;
  AgateEntity *entity = vm->entities;

  while (entity != NULL) {
    if (entity->status == AGATE_ENTITY_STATUS_GRAY) {
      entity->status = AGATE_ENTITY_STATUS_WHITE;
      previous = entity;
      entity = entity->next;
    } else {
      AgateEntity *unreached = entity;
      entity = entity->next;

      if (previous != NULL) {
        previous->next = entity;
      } else {
        vm->entities = entity;
      }

      agateEntityDelete(unreached, vm);
    }
  }
}

static void agateCollectGarbage(AgateVM *vm) {
#ifdef AGATE_DEBUG_LOG_GC
  printf("-- gc begin\n");
  ptrdiff_t before = vm->bytes_allocated;
#endif

  agateMarkRoots(vm);
  agateTraceReferences(vm);
  agateSweep(vm);

  vm->bytes_threshold = vm->bytes_allocated * AGATE_GC_HEAP_GROW_FACTOR;

#ifdef AGATE_DEBUG_LOG_GC
  printf("-- gc end\n");
  printf("   collected %td bytes (from %td to %td) next at %td\n", before - vm->bytes_allocated, before, vm->bytes_allocated, vm->bytes_threshold);
#endif
}

static void *agateMemoryHandle(AgateVM *vm, void *previous, ptrdiff_t old_size, ptrdiff_t new_size) {
  vm->bytes_allocated += new_size - old_size;

  if (new_size > old_size) {
#ifdef AGATE_DEBUG_STRESS_GC
    agateCollectGarbage(vm);
#endif

    if (vm->bytes_allocated > vm->bytes_threshold) {
      agateCollectGarbage(vm);
    }
  }

  return vm->config.reallocate(previous, new_size, vm->config.user_data);
}

/*
 * vm - defaults
 */

static void *agateReallocDefault(void *ptr, ptrdiff_t size, void *user_data) {
  if (size == 0) {
    free(ptr);
    return NULL;
  }

  void* next = realloc(ptr, size);
  assert(next);
  return next;
}

#define AGATE_LOCALE_SIZE_MAX 32

typedef struct {
  int category;
  char locale[AGATE_LOCALE_SIZE_MAX + 1];
} AgateLocaleSettings;

static void agateLocaleSettingsSave(AgateLocaleSettings *self, int category) {
  self->category = category;
  const char *locale = setlocale(category, NULL);
  assert(strlen(locale) < AGATE_LOCALE_SIZE_MAX);
  strncpy(self->locale, locale, AGATE_LOCALE_SIZE_MAX);
  self->locale[AGATE_LOCALE_SIZE_MAX] = '\0';
}

static void agateLocaleSettingsRestore(AgateLocaleSettings *self) {
  setlocale(self->category, self->locale);
}

static bool agateParseIntDefault(const char *text, ptrdiff_t size, int base, int64_t *result) {
  AgateLocaleSettings locale;
  agateLocaleSettingsSave(&locale, LC_NUMERIC);
  setlocale(LC_NUMERIC, "C");

  char *end;
  errno = 0;
  int64_t value = strtoll(text, &end, base);
  bool has_error = errno != 0;

  agateLocaleSettingsRestore(&locale);

  if (has_error) {
    *result = INT64_MAX;
    return false;
  }

  if (result) {
    *result = value;
  }

  return end == text + size;
}

static bool AgateParseFloatDefault(const char *text, ptrdiff_t size, double *result) {
  AgateLocaleSettings locale;
  agateLocaleSettingsSave(&locale, LC_NUMERIC);
  setlocale(LC_NUMERIC, "C");

  char *end = NULL;
  errno = 0;
  double value = strtod(text, &end);
  bool has_error = errno != 0;

  agateLocaleSettingsRestore(&locale);

  if (has_error) {
    *result = NAN;
    return false;
  }

  if (result) {
    *result = value;
  }

  return end == text + size;
}


/*
 * vm - run
 */

static void agateResetStack(AgateVM *vm) {
  vm->stack_top = vm->stack;
  vm->frames_count = 0;
  vm->open_upvalues = NULL;
}

static void agateEnsureStack(AgateVM *vm, ptrdiff_t needed) {
  if (vm->stack_capacity >= needed) {
    return;
  }

  ptrdiff_t capacity = vm->stack_capacity;

  while (capacity < needed) {
    capacity *= 2;
  }

  AgateValue *old_stack = vm->stack;
  vm->stack = agateGrowArray(vm, AgateValue, vm->stack, vm->stack_capacity, capacity);
  vm->stack_capacity = capacity;

  if (vm->stack != old_stack) {

    for (ptrdiff_t i = 0; i < vm->frames_count; ++i) {
      AgateCallFrame *frame = &vm->frames[i];
      frame->stack_start = vm->stack + (frame->stack_start - old_stack);
    }

    for (AgateUpvalue *upvalue = vm->open_upvalues; upvalue != NULL; upvalue = upvalue->next) {
      upvalue->location = vm->stack + (upvalue->location - old_stack);
    }

    vm->stack_top = vm->stack + (vm->stack_top - old_stack);
  }
}

static inline bool agateCopy(AgateValue *dest, AgateValue orig) {
  *dest = orig;
  return true;
}

static inline uint8_t agateReadByte(AgateCallFrame *frame) {
  return *frame->ip++;
}

static inline uint16_t agateReadShort(AgateCallFrame *frame) {
  uint16_t value = *frame->ip++;
  value = (value << 8) | *frame->ip++;
  return value;
}

static inline AgateValue agateReadConstant(AgateCallFrame *frame) {
  return frame->closure->function->bc.constants.data[agateReadShort(frame)];
}

static inline void agatePush(AgateVM *vm, AgateValue value) {
  assert(vm->stack_top - vm->stack < vm->stack_capacity);
  agateCopy(vm->stack_top, value);
  ++vm->stack_top;
}

static inline AgateValue agatePop(AgateVM *vm) {
  assert(vm->stack_top - vm->stack > 0);
  --vm->stack_top;
  return *vm->stack_top;
}

static inline AgateValue agatePeek(const AgateVM *vm, ptrdiff_t distance) {
  return vm->stack_top[-1 - distance];
}

static AgateUpvalue *agateCaptureUpvalue(AgateVM *vm, AgateValue *local) {
  AgateUpvalue *prev = NULL;
  AgateUpvalue *upvalue = vm->open_upvalues;

  while (upvalue != NULL && upvalue->location > local) {
    prev = upvalue;
    upvalue = upvalue->next;
  }

  if (upvalue != NULL && upvalue->location == local) {
    return upvalue;
  }

  AgateUpvalue *created = agateUpvalueNew(vm, local);
  created->next = upvalue;

  if (prev == NULL) {
    vm->open_upvalues = created;
  } else {
    prev->next = created;
  }

  return created;
}

static void agateCloseUpvalue(AgateVM *vm, AgateValue *last) {
  AgateUpvalue *current = vm->open_upvalues;

  while (current != NULL && current->location >= last) {
    AgateUpvalue *upvalue = current;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    current = upvalue->next;
  }

  vm->open_upvalues = current;
}

static AgateForeignMethodFunc agateFindForeignMethod(AgateVM *vm, const char *unit_name, const char *class_name, AgateForeignMethodKind kind, const char *signature) {
  AgateForeignMethodFunc method = NULL;

  if (vm->config.foreign_method_handler != NULL) {
    method = vm->config.foreign_method_handler(vm, unit_name, class_name, kind, signature);
  }

  return method;
}

static ptrdiff_t agateByteCountForArguments(AgateBytecode *bc, ptrdiff_t ip) {
  static const ptrdiff_t offsets[] = {
    #define X(name, stack, bytes) bytes,
    AGATE_OPCODE_LIST
    #undef X
  };

  AgateOpCode instruction = bc->code.data[ip];

  if (instruction == AGATE_OP_CLOSURE) {
    uint16_t constant = (bc->code.data[ip + 1] << 8) | bc->code.data[ip + 2];
    AgateFunction *function = agateAsFunction(bc->constants.data[constant]);
    return 2 + function->upvalue_count * 2;
  }

  return offsets[instruction];
}

static void agatePatchMethodCode(AgateClass *klass, AgateFunction *function) { // wrenBindMethodCode
  ptrdiff_t ip = 0;

  for (;;) {
    AgateOpCode instruction = function->bc.code.data[ip];

    switch (instruction) {
      case AGATE_OP_FIELD_LOAD:
      case AGATE_OP_FIELD_STORE:
      case AGATE_OP_FIELD_LOAD_THIS:
      case AGATE_OP_FIELD_STORE_THIS:
        function->bc.code.data[ip + 1] += klass->supertype->field_count;
        break;

      case AGATE_OP_SUPER:
      {
        uint16_t constant = (function->bc.code.data[ip + 4] << 8) | function->bc.code.data[ip + 5];
        function->bc.constants.data[constant] = agateEntityValue(klass->supertype);
        break;
      }

      case AGATE_OP_CLOSURE:
      {
        uint16_t constant = (function->bc.code.data[ip + 1] << 8) | function->bc.code.data[ip + 2];
        agatePatchMethodCode(klass, agateAsFunction(function->bc.constants.data[constant]));
        break;
      }

      case AGATE_OP_END:
        return;

      default:
        break;
    }

    ip += 1 + agateByteCountForArguments(&function->bc, ip);
  }
}

static void agateBindMethod(AgateVM *vm, AgateOpCode op, ptrdiff_t symbol, AgateUnit *unit, AgateClass *klass, AgateValue method_value) {
  const char *class_name = klass->name->data;

  AgateForeignMethodKind kind = op == AGATE_OP_METHOD_INSTANCE ? AGATE_FOREIGN_METHOD_INSTANCE : AGATE_FOREIGN_METHOD_CLASS;

  if (kind == AGATE_FOREIGN_METHOD_CLASS) {
    klass = klass->base.type;
  }

  AgateMethod method;

  if (agateIsString(method_value)) {
    const char *name = agateAsCString(method_value);
    method.kind = AGATE_METHOD_FOREIGN;
    method.as.foreign = agateFindForeignMethod(vm, unit->name->data, class_name, kind, name);

    if (method.as.foreign == NULL) {
      vm->error = agateEntityValue(agateStringNewFormat(vm, "Could not find foreign method '@' for class $ in unit '$'.", agateAsString(method_value), klass->name->data, unit->name->data));
      return;
    }
  } else {
    assert(agateIsClosure(method_value));
    method.kind = AGATE_METHOD_CLOSURE;
    method.as.closure = agateAsClosure(method_value);

    agatePatchMethodCode(klass, method.as.closure->function);
  }

  agateClassBindMethod(vm, klass, symbol, method);
}

static void agateCallForeign(AgateVM *vm, AgateForeignMethodFunc foreign, int argc) {
  assert(vm->api_stack == NULL);
  vm->api_stack = vm->stack_top - argc;
  foreign(vm);
  vm->stack_top = vm->api_stack + 1;
  vm->api_stack = NULL;
}

static void agateRuntimeError(AgateVM *vm) {
  assert(!agateIsNil(vm->error));

  if (vm->config.error == NULL) {
    return;
  }

  if (agateIsString(vm->error)) {
    vm->config.error(vm, AGATE_ERROR_RUNTIME, NULL, -1, agateAsCString(vm->error));
  } else {
    vm->config.error(vm, AGATE_ERROR_RUNTIME, NULL, -1, "[unidentified error]");
  }

  for (ptrdiff_t i = vm->frames_count - 1; i >= 0; --i) {
    AgateCallFrame *frame = &vm->frames[i];
    AgateFunction *function = frame->closure->function;

    if (function->unit == NULL) {
      continue;
    }

    // core unit
    if (function->unit->name == NULL) {
      continue;
    }

    int line = agateBytecodeLineFromOffset(&function->bc, frame->ip - function->bc.code.data - 1);
    vm->config.error(vm, AGATE_ERROR_STACKTRACE, function->unit->name->data, line, function->name->data);
  }

  vm->api_stack = NULL;
}

static AgateUnit *agateGetUnit(AgateVM *vm, AgateValue name) {
  AgateValue value = agateTableHashFind(&vm->units, name);

  if (agateIsUndefined(value)) {
    return NULL;
  }

  return agateAsUnit(value);
}

static AgateFunction *agateRawCompile(AgateVM *vm, AgateUnit *unit, const char *source);

static AgateClosure *agateCompile(AgateVM *vm, AgateValue name, const char *source) { // ~ compileInModule
  AgateUnit *unit = agateGetUnit(vm, name);

  if (unit == NULL) {
    assert(agateIsString(name));
    unit = agateUnitNew(vm, agateAsString(name));
    assert(unit);

    agatePushRoot(vm, (AgateEntity *) unit);
    agateTableHashInsert(&vm->units, name, agateEntityValue(unit), vm);
    agatePopRoot(vm);

    AgateUnit *core = agateGetUnit(vm, agateNilValue());
    assert(core);

    for (ptrdiff_t i = 0; i < core->object_names.capacity; ++i) {
      AgateTableEntry *entry = &core->object_names.entries[i];
      if (agateIsString(entry->key)) {
        AgateString *name = agateAsString(entry->key);
        assert(agateIsInt(entry->value));
        int64_t symbol = agateAsInt(entry->value);
        assert(symbol < core->object_values.size);
        agateUnitAddVariable(vm, unit, name->data, name->size, core->object_values.data[symbol]);
      }
    }
  }

  AgateFunction *function = agateRawCompile(vm, unit, source);

  if (function == NULL) {
    return NULL;
  }

  agatePushRoot(vm, (AgateEntity *) function);
  AgateClosure *closure = agateClosureNew(vm, function);
  agatePopRoot(vm);

  return closure;
}

static AgateValue agateValidateSuperclass(AgateVM *vm, AgateValue name, AgateValue superclass_value, ptrdiff_t field_count) {
  assert(agateIsString(name));

  if (!agateIsClass(superclass_value)) {
    return agateEntityValue(agateStringNewFormat(vm, "Class '@' cannot inherit from a non-class object.", agateAsString(name)));
  }

  AgateClass *superclass = agateAsClass(superclass_value);

  if (superclass == vm->array_class || superclass == vm->bool_class || superclass == vm->char_class || superclass == vm->class_class || superclass == vm->float_class || superclass == vm->fn_class || superclass == vm->int_class || superclass == vm->map_class || superclass == vm->nil_class || superclass == vm->range_class || superclass == vm->string_class) {
    return agateEntityValue(agateStringNewFormat(vm, "Class '@' cannot inherit from built-in class '@'.", agateAsString(name), superclass->name));
  }

  if (superclass->field_count == -1) {
    return agateEntityValue(agateStringNewFormat(vm, "Class '@' cannot inherit from foreign class '@'.", agateAsString(name), superclass->name));
  }

  if (field_count == -1 && superclass->field_count > 0) {
    return agateEntityValue(agateStringNewFormat(vm, "Foreign class '@' cannot inherit from a class with fields.", agateAsString(name)));
  }

  if (superclass->field_count + field_count > AGATE_MAX_FIELDS) {
    return agateEntityValue(agateStringNewFormat(vm, "Class '@' may not have more than 255 fields, including inherited ones.", agateAsString(name)));
  }

  return agateNilValue();
}

static void agateBindForeignClass(AgateVM *vm, AgateClass *klass, AgateUnit *unit) {
  AgateForeignClassHandler handler;
  handler.allocate = NULL;
  handler.destroy = NULL;

  if (vm->config.foreign_class_handler != NULL) {
    handler = vm->config.foreign_class_handler(vm, unit->name->data, klass->name->data);
  }

  ptrdiff_t allocate_symbol = agateSymbolTableEnsure(&vm->method_names, "<allocate>", 10, vm);

  if (handler.allocate != NULL) {
    AgateMethod method;
    method.kind = AGATE_METHOD_FOREIN_ALLOCATE;
    method.as.foreign_allocate = handler.allocate;
    agateClassBindMethod(vm, klass, allocate_symbol, method);
  }

  ptrdiff_t destroy_symbol = agateSymbolTableEnsure(&vm->method_names, "<destroy>", 9, vm);

  if (handler.destroy != NULL) {
    AgateMethod method;
    method.kind = AGATE_METHOD_FOREIN_DESTROY;
    method.as.foreign_destroy = handler.destroy;
    agateClassBindMethod(vm, klass, destroy_symbol, method);
  }
}

static void agateCreateClass(AgateVM *vm, ptrdiff_t field_count, AgateUnit *unit) {
  AgateValue name = vm->stack_top[-2];
  AgateValue superclass = vm->stack_top[-1];
  --vm->stack_top;

  vm->error = agateValidateSuperclass(vm, name, superclass, field_count);

  if (!agateIsNil(vm->error)) {
    return;
  }

  AgateClass *klass = agateClassNew(vm, unit, agateAsClass(superclass), field_count, agateAsString(name));
  vm->stack_top[-1] = agateEntityValue(klass);

  if (field_count == -1) {
    agateBindForeignClass(vm, klass, unit);
  }
}

static AgateValue agateImportUnit(AgateVM *vm, AgateValue name) {
  assert(agateIsString(name));
  AgateValue existing = agateTableHashFind(&vm->units, name);

  if (!agateIsUndefined(existing)) {
    return existing;
  }

  agatePushRoot(vm, agateAsEntity(name));

  AgateUnitHandler handler = { NULL, NULL, NULL };
  const char *source = NULL;

  if (vm->config.unit_handler != NULL) {
    handler = vm->config.unit_handler(vm, agateAsCString(name));
    source = handler.load(agateAsCString(name), handler.user_data);
  }

  if (source == NULL) {
    vm->error = agateEntityValue(agateStringNewFormat(vm, "Could not load unit '@'.", agateAsString(name)));
    agatePopRoot(vm);
    return agateNilValue();
  }

  AgateClosure *closure = agateCompile(vm, name, source);

  if (handler.release) {
    handler.release(source, handler.user_data);
  }

  if (closure == NULL) {
    vm->error = agateEntityValue(agateStringNewFormat(vm, "Could not compile unit '@'.", agateAsString(name)));
    agatePopRoot(vm);
    return agateNilValue();
  }

  agatePopRoot(vm);
  return agateEntityValue(closure);
}

static void agateMethodNotFound(AgateVM *vm, AgateClass *klass, ptrdiff_t symbol) {
  AgateString *method = agateSymbolTableReverseFind(&vm->method_names, symbol);
  vm->error = agateEntityValue(agateStringNewFormat(vm, "@ does not implement '@'.", klass->name, method));
}

static inline void agateClosureCall(AgateVM *vm, AgateClosure *closure, int argc) {
  if (vm->frames_count >= vm->frames_capacity) {
    ptrdiff_t capacity = agateGrowCapacity(vm->frames_capacity);
    vm->frames = agateGrowArray(vm, AgateCallFrame, vm->frames, vm->frames_capacity, capacity);
    vm->frames_capacity = capacity;
  }

  ptrdiff_t stack_size = vm->stack_top - vm->stack;
  ptrdiff_t needed = stack_size + closure->function->slot_count;
  agateEnsureStack(vm, needed);

  AgateCallFrame *frame = &vm->frames[vm->frames_count++];
  frame->stack_start = vm->stack_top - argc;
  frame->closure = closure;
  frame->ip = closure->function->bc.code.data;
}

static inline bool agateMethodCall(AgateVM *vm, AgateClass *klass, ptrdiff_t symbol, int argc, AgateValue *args) {
  if (symbol >= klass->methods.size) {
    agateMethodNotFound(vm, klass, symbol);
    return false;
  }

  AgateMethod *method = &klass->methods.data[symbol];

  if (method->kind == AGATE_METHOD_NONE) {
    agateMethodNotFound(vm, klass, symbol);

//     for (ptrdiff_t i = 0; i < klass->methods.size; ++i) {
//       AgateMethod *method = &klass->methods.data[i];
//
//       if (method->kind != AGATE_METHOD_NONE) {
//         AgateString *name = agateSymbolTableReverseFind(&vm->method_names, i);
//         printf("- %s\n", name->data);
//       }
//     }

    return false;
  }

  switch (method->kind) {
    case AGATE_METHOD_NATIVE:
      if (method->as.native(vm, argc, args)) {
        vm->stack_top -= argc - 1;
        return true;
      }

      return false;

    case AGATE_METHOD_FOREIGN:
      agateCallForeign(vm, method->as.foreign, argc);
      return agateIsNil(vm->error);

    case AGATE_METHOD_CLOSURE:
      agateClosureCall(vm, method->as.closure, argc);
      return true;

    default:
      assert(false);
      break;
  }

  return false;
}

static bool agateFunctionCall(AgateVM *vm, AgateValue value, int argc) {
  if (!agateIsClosure(value)) {
    vm->error = agateEntityValue(agateStringNewFormat(vm, "Can only call functions."));
    return false;
  }

  if (!agateCheckArity(vm, value, argc)) {
    return false;
  }

  agateClosureCall(vm, agateAsClosure(value), argc);
  return true;
}

static AgateStatus agateRun(AgateVM *vm) {
  AgateCallFrame *frame = &vm->frames[vm->frames_count - 1];
  AgateFunction *function = frame->closure->function;

  #define AGATE_LOAD_FRAME()                        \
    do {                                            \
      frame = &vm->frames[vm->frames_count - 1];    \
      function = frame->closure->function;          \
    } while (false)

  #define AGATE_RUNTIME_ERROR()                     \
    do {                                            \
      agateRuntimeError(vm);                        \
      return AGATE_STATUS_RUNTIME_ERROR;            \
    } while (false)

  for (;;) {
#ifdef AGATE_DEBUG_TRACE_EXECUTION
    printf("          ");

    for (AgateValue* slot = vm->stack; slot < vm->stack_top; ++slot) {
      printf("[ ");
      agateValuePrint(*slot);
      printf(" ]");
    }

    printf("\n");

    agateDisassembleInstruction(vm, function, frame->ip - function->bc.code.data);
#endif

    AgateOpCode instruction;

    switch (instruction = agateReadByte(frame)) {
      case AGATE_OP_CONSTANT:
      {
        AgateValue constant = agateReadConstant(frame);
        agatePush(vm, constant);
        break;
      }

      case AGATE_OP_NIL:
        agatePush(vm, agateNilValue());
        break;

      case AGATE_OP_FALSE:
        agatePush(vm, agateBoolValue(false));
        break;

      case AGATE_OP_TRUE:
        agatePush(vm, agateBoolValue(true));
        break;

      case AGATE_OP_GLOBAL_LOAD:
      {
        uint16_t global = agateReadShort(frame);
        AgateValue value = function->unit->object_values.data[global];
        agatePush(vm, value);
        break;
      }

      case AGATE_OP_GLOBAL_STORE:
      {
        uint16_t global = agateReadShort(frame);
        agateCopy(&function->unit->object_values.data[global], agatePeek(vm, 0));
        break;
      }

      case AGATE_OP_LOCAL_LOAD:
      {
        uint8_t slot = agateReadByte(frame);
        agatePush(vm, frame->stack_start[slot]);
        break;
      }

      case AGATE_OP_LOCAL_STORE:
      {
        uint8_t slot = agateReadByte(frame);
        agateCopy(&frame->stack_start[slot], agatePeek(vm, 0));
        break;
      }

      case AGATE_OP_UPVALUE_LOAD:
      {
        uint8_t slot = agateReadByte(frame);
        agatePush(vm, *frame->closure->upvalues[slot]->location);
        break;
      }

      case AGATE_OP_UPVALUE_STORE:
      {
        uint8_t slot = agateReadByte(frame);
        agateCopy(frame->closure->upvalues[slot]->location, agatePeek(vm, 0));
        break;
      }

      case AGATE_OP_FIELD_LOAD:
      {
        ptrdiff_t field = agateReadByte(frame);
        AgateValue receiver = agatePop(vm);
        assert(agateIsInstance(receiver));
        AgateInstance *instance = agateAsInstance(receiver);
        assert(field < instance->field_count);
        agatePush(vm, instance->fields[field]);
        break;
      }

      case AGATE_OP_FIELD_STORE:
      {
        ptrdiff_t field = agateReadByte(frame);
        AgateValue receiver = agatePop(vm);
        assert(agateIsInstance(receiver));
        AgateInstance *instance = agateAsInstance(receiver);
        assert(field < instance->field_count);
        instance->fields[field] = agatePeek(vm, 0);
        break;
      }

      case AGATE_OP_FIELD_LOAD_THIS:
      {
        ptrdiff_t field = agateReadByte(frame);
        AgateValue receiver = frame->stack_start[0];
        assert(agateIsInstance(receiver));
        AgateInstance *instance = agateAsInstance(receiver);
        assert(field < instance->field_count);
        agatePush(vm, instance->fields[field]);
        break;
      }

      case AGATE_OP_FIELD_STORE_THIS:
      {
        ptrdiff_t field = agateReadByte(frame);
        AgateValue receiver = frame->stack_start[0];
        assert(agateIsInstance(receiver));
        AgateInstance *instance = agateAsInstance(receiver);
        assert(field < instance->field_count);
        instance->fields[field] = agatePeek(vm, 0);
        break;
      }

      case AGATE_OP_JUMP_FORWARD:
      {
        uint16_t offset = agateReadShort(frame);
        frame->ip += offset;
        break;
      }

      case AGATE_OP_JUMP_BACKWARD:
      {
        uint16_t offset = agateReadShort(frame);
        frame->ip -= offset;
        break;
      }

      case AGATE_OP_JUMP_IF:
      {
        uint16_t offset = agateReadShort(frame);

        if (agateIsFalsey(agatePop(vm))) {
          frame->ip += offset;
        }

        break;
      }

      case AGATE_OP_AND:
      {
        uint16_t offset = agateReadShort(frame);
        AgateValue condition = agatePeek(vm, 0);

        if (agateIsFalsey(condition)) {
          frame->ip += offset;
        } else {
          --vm->stack_top;
        }

        break;
      }

      case AGATE_OP_OR:
      {
        uint16_t offset = agateReadShort(frame);
        AgateValue condition = agatePeek(vm, 0);

        if (agateIsFalsey(condition)) {
          --vm->stack_top;
        } else {
          frame->ip += offset;
        }

        break;
      }

      case AGATE_OP_CALL:
      {
        int argc = agateReadByte(frame);
        AgateValue *args = vm->stack_top - argc;

        if (!agateFunctionCall(vm, args[0], argc)) {
          AGATE_RUNTIME_ERROR();
        }

        AGATE_LOAD_FRAME();
        break;
      }

      case AGATE_OP_INVOKE:
      {
        int argc = agateReadByte(frame) + 1;
        ptrdiff_t symbol = agateReadShort(frame);
        AgateValue *args = vm->stack_top - argc;
        AgateClass *klass = agateValueGetClass(args[0], vm);

        if (!agateMethodCall(vm, klass, symbol, argc, args)) {
          AGATE_RUNTIME_ERROR();
        }

        AGATE_LOAD_FRAME();
        break;
      }

      case AGATE_OP_SUPER:
      {
        int argc = agateReadByte(frame) + 1;
        ptrdiff_t symbol = agateReadShort(frame);
        AgateValue *args = vm->stack_top - argc;
        AgateClass *klass = agateAsClass(agateReadConstant(frame));

        if (!agateMethodCall(vm, klass, symbol, argc, args)) {
          AGATE_RUNTIME_ERROR();
        }

        AGATE_LOAD_FRAME();
        break;
      }

      case AGATE_OP_CLOSURE:
      {
        AgateFunction *function = agateAsFunction(agateReadConstant(frame));
        AgateClosure *closure = agateClosureNew(vm, function);
        agatePush(vm, agateEntityValue(closure));

        for (ptrdiff_t i = 0; i < closure->upvalue_count; ++i) {
          AgateCapture capture = agateReadByte(frame);
          ptrdiff_t index = agateReadByte(frame);

          if (capture == AGATE_CAPTURE_LOCAL) {
            closure->upvalues[i] = agateCaptureUpvalue(vm, frame->stack_start + index);
          } else {
            closure->upvalues[i] = frame->closure->upvalues[index];
          }
        }

        break;
      }

      case AGATE_OP_CLOSE_UPVALUE:
      {
        agateCloseUpvalue(vm, vm->stack_top - 1);
        agatePop(vm);
        break;
      }

      case AGATE_OP_POP:
        --vm->stack_top;
        break;

      case AGATE_OP_RETURN:
      {
        AgateValue result = agatePop(vm);
        --vm->frames_count;
        agateCloseUpvalue(vm, frame->stack_start);

        if (vm->frames_count == 0) {
          vm->stack[0] = result;
          vm->stack_top = vm->stack + 1;
          return AGATE_STATUS_OK;
        }

        vm->stack_top = frame->stack_start;
        agatePush(vm, result);
        AGATE_LOAD_FRAME();
        break;
      }

      case AGATE_OP_CLASS:
      {
        ptrdiff_t field_count = agateReadByte(frame);
        agateCreateClass(vm, field_count, NULL);

        if (!agateIsNil(vm->error)) {
          AGATE_RUNTIME_ERROR();
        }

        break;
      }

      case AGATE_OP_CLASS_FOREIGN:
      {
        agateCreateClass(vm, -1, function->unit);

        if (!agateIsNil(vm->error)) {
          AGATE_RUNTIME_ERROR();
        }

        break;
      }

      case AGATE_OP_CONSTRUCT:
      {
        assert(agateIsClass(frame->stack_start[0]));
        frame->stack_start[0] = agateEntityValue(agateInstanceNew(vm, agateAsClass(frame->stack_start[0])));
        break;
      }

      case AGATE_OP_CONSTRUCT_FOREIGN:
      {
        assert(agateIsClass(frame->stack_start[0]));
        frame->stack_start[0] = agateEntityValue(agateForeignNew(vm, agateAsClass(frame->stack_start[0])));
        break;
      }

      case AGATE_OP_METHOD_INSTANCE:
      case AGATE_OP_METHOD_CLASS:
      {
        ptrdiff_t symbol = agateReadShort(frame);
        assert(agateIsClass(agatePeek(vm, 0)));
        AgateClass *klass = agateAsClass(agatePeek(vm, 0));
        AgateValue method = agatePeek(vm, 1);
        agateBindMethod(vm, instruction, symbol, function->unit, klass, method);

        if (!agateIsNil(vm->error)) {
          AGATE_RUNTIME_ERROR();
        }

        vm->stack_top -= 2;
        break;
      }

      case AGATE_OP_IMPORT_UNIT:
      {
        agatePush(vm, agateImportUnit(vm, agateReadConstant(frame)));

        if (!agateIsNil(vm->error)) {
          AGATE_RUNTIME_ERROR();
        }

        if (agateIsClosure(agatePeek(vm, 0))) {
          AgateClosure *closure = agateAsClosure(agatePeek(vm, 0));
          agateClosureCall(vm, closure, 1);
          AGATE_LOAD_FRAME();
        } else {
          vm->last_unit = agateAsUnit(agatePeek(vm, 0));
        }

        break;
      }

      case AGATE_OP_IMPORT_OBJECT:
      {
        AgateValue variable = agateReadConstant(frame);
        assert(vm->last_unit != NULL);
        AgateValue object = agateUnitGetVariable(vm, vm->last_unit, variable);

        if (!agateIsNil(vm->error)) {
          AGATE_RUNTIME_ERROR();
        }

        agatePush(vm, object);
        break;
      }

      case AGATE_OP_END_UNIT:
      {
        vm->last_unit = function->unit;
        agatePush(vm, agateNilValue());
        break;
      }

      case AGATE_OP_END:
      {
        assert(false);
        return AGATE_STATUS_RUNTIME_ERROR;
      }

    }
  }

  return AGATE_STATUS_RUNTIME_ERROR;
}

/*
 * vm - validation
 */

static bool agateValidateInt(AgateVM *vm, AgateValue value, const char *arg) {
  if (agateIsInt(value)) {
    return true;
  }

  vm->error = agateEntityValue(agateStringNewFormat(vm, "$ must be an integer.", arg));
  return false;
}

static bool agateValidateFloat(AgateVM *vm, AgateValue value, const char *arg) {
  if (agateIsFloat(value)) {
    return true;
  }

  vm->error = agateEntityValue(agateStringNewFormat(vm, "$ must be a float.", arg));
  return false;
}

static bool agateValidateChar(AgateVM *vm, AgateValue value, const char *arg) {
  if (agateIsChar(value)) {
    return true;
  }

  vm->error = agateEntityValue(agateStringNewFormat(vm, "$ must be a character.", arg));
  return false;
}

static ptrdiff_t agateValidateIndexValue(AgateVM *vm, ptrdiff_t value, ptrdiff_t size, const char *arg) {
  if (value < 0) {
    value = size + value;
  }

  if (0 <= value && value < size) {
    return value;
  }

  vm->error = agateEntityValue(agateStringNewFormat(vm, "$ out of bounds.", arg));
  return AGATE_INDEX_ERROR;
}

static ptrdiff_t agateValidateIndex(AgateVM *vm, AgateValue value, ptrdiff_t size, const char *arg) {
  if (!agateValidateInt(vm, value, arg)) {
    return AGATE_INDEX_ERROR;
  }

  return agateValidateIndexValue(vm, agateAsInt(value), size, arg);
}

static bool agateValidateString(AgateVM *vm, AgateValue value, const char *arg) {
  if (agateIsString(value)) {
    return true;
  }

  vm->error = agateEntityValue(agateStringNewFormat(vm, "$ must be a string.", arg));
  return false;
}

typedef struct {
  ptrdiff_t start;
  ptrdiff_t count;
  ptrdiff_t step;
} AgateExtent;

static bool agateValidateRange(AgateVM *vm, const AgateRange *range, ptrdiff_t size, AgateExtent *extent) {
  extent->step = 0;

  // special case for arrays
  if (range->from == size && range->to == (range->kind == AGATE_RANGE_INCLUSIVE ? -1 : size)) {
    extent->start = 0;
    extent->count = 0;
    return true;
  }

  ptrdiff_t from = agateValidateIndexValue(vm, range->from, size, "Range start");

  if (from == AGATE_INDEX_ERROR) {
    return false;
  }

  ptrdiff_t to = range->to;

  if (to < 0) {
    to += size;
  }

  if (range->kind == AGATE_RANGE_EXCLUSIVE) {
    if (from == to) {
      extent->start = from;
      extent->count = 0;
      return true;
    }

    if (from <= to) {
      to -= 1;
    } else {
      to += 1;
    }
  }

  if (to < 0 || to >= size) {
    vm->error = agateEntityValue(agateStringNewFormat(vm, "Range end out of bounds."));
    return false;
  }

  extent->start = from;

  if (from <= to) {
    extent->count = to - from + 1;
    extent->step = 1;
  } else {
    extent->count = from - to + 1;
    extent->step = -1;
  }

  return true;
}



/*
 * vm - core
 */

#include "core.inc"

#define AGATE_CONST_STRING(vm, text) agateEntityValue(agateStringNew((vm), (text), sizeof(text) - 1))

// Object

static bool agateCoreObjectNot(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateBoolValue(false);
  return true;
}

static bool agateCoreObjectEqual(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateBoolValue(agateValueEquals(args[0], args[1]));
  return true;
}

static bool agateCoreObjectNotEqual(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateBoolValue(!agateValueEquals(args[0], args[1]));
  return true;
}

static bool agateCoreObjectSame(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateBoolValue(agateValueEquals(args[1], args[2]));
  return true;
}

static bool agateCoreObjectIs(AgateVM *vm, int argc, AgateValue *args) {
  if (!agateIsClass(args[1])) {
    vm->error = agateEntityValue(agateStringNewFormat(vm, "Right operand must be a class."));
    return false;
  }

  AgateClass *object_class = agateValueGetClass(args[0], vm);
  AgateClass *base_class = agateAsClass(args[1]);

  do {
    if (base_class == object_class) {
      args[0] = agateBoolValue(true);
      return true;
    }

    object_class = object_class->supertype;
  } while (object_class != NULL);

  args[0] = agateBoolValue(false);
  return true;
}

static bool agateCoreObjectType(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateEntityValue(agateValueGetClass(args[0], vm));
  return true;
}

static bool agateCoreObjectToS(AgateVM *vm, int argc, AgateValue *args) {
  AgateClass *object_class = agateValueGetClass(args[0], vm);
  AgateString *name = agateStringNewFormat(vm, "instance of @", object_class->name);
  args[0] = agateEntityValue(name);
  return true;
}


// Class

static bool agateCoreClassName(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateEntityValue(agateAsClass(args[0])->name);
  return true;
}

static bool agateCoreClassSupertype(AgateVM *vm, int argc, AgateValue *args) {
  AgateClass *klass = agateAsClass(args[0]);

  if (klass->supertype == NULL) {
    args[0] = agateNilValue();
  } else {
    args[0] = agateEntityValue(klass->supertype);
  }

  return true;
}

static bool agateCoreClassToS(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateEntityValue(agateAsClass(args[0])->name);
  return true;
}

static bool agateCoreClassHash(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateIntValue(agateAsClass(args[0])->name->hash);
  return true;
}

// Nil

static bool agateCoreNilNot(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateBoolValue(true);
  return true;
}

static bool agateCoreNilToS(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = AGATE_CONST_STRING(vm, "nil");
  return true;
}

static bool agateCoreNilHash(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateIntValue(agateHashNil());
  return true;
}

// Bool

static bool agateCoreBoolNot(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateBoolValue(!agateAsBool(args[0]));
  return true;
}

static bool agateCoreBoolToI(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateIntValue(agateAsBool(args[0]) ? 1 : 0);
  return true;
}

static bool agateCoreBoolToS(AgateVM *vm, int argc, AgateValue *args) {
  if (agateAsBool(args[0])) {
    args[0] = AGATE_CONST_STRING(vm, "true");
  } else {
    args[0] = AGATE_CONST_STRING(vm, "false");
  }
  return true;
}

static bool agateCoreBoolHash(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateIntValue(agateHashBool(agateAsBool(args[0])));
  return true;
}

// Int

#define AGATE_INT_CONSTANT(name, value)                                     \
static bool agateCoreInt ## name(AgateVM *vm, int argc, AgateValue *args) { \
  args[0] = agateIntValue(value);                                           \
  return true;                                                              \
}

AGATE_INT_CONSTANT(Lowest, INT64_MIN)
AGATE_INT_CONSTANT(Max, INT64_MAX)
AGATE_INT_CONSTANT(Min, INT64_MIN)

#undef AGATE_INT_CONSTANT

#define AGATE_INT_INFIX(name, op)                                           \
static bool agateCoreInt ## name(AgateVM *vm, int argc, AgateValue *args) { \
  if (!agateValidateInt(vm, args[1], "Right operand")) {                    \
    return false;                                                           \
  }                                                                         \
  args[0] = agateIntValue(agateAsInt(args[0]) op agateAsInt(args[1]));      \
  return true;                                                              \
}

AGATE_INT_INFIX(Plus,       +)
AGATE_INT_INFIX(Minus,      -)
AGATE_INT_INFIX(Multiply,   *)
AGATE_INT_INFIX(Divide,     /)
AGATE_INT_INFIX(Modulo,     %)

AGATE_INT_INFIX(And,        &)
AGATE_INT_INFIX(Or,         |)
AGATE_INT_INFIX(Xor,        ^)

#undef AGATE_INT_INFIX

#define AGATE_INT_CMP(name, op)                                             \
static bool agateCoreInt ## name(AgateVM *vm, int argc, AgateValue *args) { \
  if (!agateValidateInt(vm, args[1], "Right operand")) {                    \
    return false;                                                           \
  }                                                                         \
  args[0] = agateBoolValue(agateAsInt(args[0]) op agateAsInt(args[1]));     \
  return true;                                                              \
}

AGATE_INT_CMP(Lt,   <)
AGATE_INT_CMP(LEq,  <=)
AGATE_INT_CMP(Gt,   >)
AGATE_INT_CMP(GEq,  >=)

#undef AGATE_INT_CMP

static bool agateCoreIntPrefixPlus(AgateVM *vm, int argc, AgateValue *args) {
  return true;
}

static bool agateCoreIntPrefixMinus(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateIntValue(-agateAsInt(args[0]));
  return true;
}

static bool agateCoreIntInvert(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateIntValue(~agateAsInt(args[0]));
  return true;
}

static bool agateCoreIntLeftShift(AgateVM *vm, int argc, AgateValue *args) {
  if (!agateValidateInt(vm, args[1], "Right operand")) {
    return false;
  }

  int64_t shift = agateAsInt(args[1]);

  if (shift < 0) {
    vm->error = AGATE_CONST_STRING(vm, "Can not shift from a negative value.");
  }

  args[0] = agateIntValue(agateAsInt(args[0]) << (shift & 0x3F));
  return true;
}

static bool agateCoreIntRightShift(AgateVM *vm, int argc, AgateValue *args) {
  if (!agateValidateInt(vm, args[1], "Right operand")) {
    return false;
  }

  int64_t shift = agateAsInt(args[1]);

  if (shift < 0) {
    vm->error = AGATE_CONST_STRING(vm, "Can not shift from a negative value.");
  }

  args[0] = agateIntValue(agateAsInt(args[0]) >> (shift & 0x3F));
  return true;
}

static bool agateCoreIntLogicalRightShift(AgateVM *vm, int argc, AgateValue *args) {
  if (!agateValidateInt(vm, args[1], "Right operand")) {
    return false;
  }

  int64_t shift = agateAsInt(args[1]);

  if (shift < 0) {
    vm->error = AGATE_CONST_STRING(vm, "Can not shift from a negative value.");
  }

  args[0] = agateIntValue(((uint64_t) agateAsInt(args[0])) >> (shift & 0x3F));
  return true;
}

static bool agateCoreIntDotDot(AgateVM *vm, int argc, AgateValue *args) {
  if (!agateValidateInt(vm, args[1], "Right operand")) {
    return false;
  }

  args[0] = agateEntityValue(agateRangeNew(vm, agateAsInt(args[0]), agateAsInt(args[1]), AGATE_RANGE_INCLUSIVE));
  return true;
}

static bool agateCoreIntDotDotDot(AgateVM *vm, int argc, AgateValue *args) {
  if (!agateValidateInt(vm, args[1], "Right operand")) {
    return false;
  }

  args[0] = agateEntityValue(agateRangeNew(vm, agateAsInt(args[0]), agateAsInt(args[1]), AGATE_RANGE_EXCLUSIVE));
  return true;
}

static bool agateCoreIntToF(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateFloatValue((double) agateAsInt(args[0]));
  return true;
}

static bool agateCoreIntToC(AgateVM *vm, int argc, AgateValue *args) {
  int64_t value = agateAsInt(args[0]);

  if (value < 0 || value > 0x10FFFF) {
    vm->error = AGATE_CONST_STRING(vm, "Integer value is not a valid character.");
    return false;
  }

  args[0] = agateCharValue((uint32_t) value);
  return true;
}

static bool agateCoreIntToS(AgateVM *vm, int argc, AgateValue *args) {
#define AGATE_INT_BUFFER_SIZE 32
  char buffer[AGATE_INT_BUFFER_SIZE];
  ptrdiff_t size = snprintf(buffer, AGATE_INT_BUFFER_SIZE, "%" PRIi64, agateAsInt(args[0]));
  args[0] = agateEntityValue(agateStringNew(vm, buffer, size));
  return true;
}

static bool agateCoreIntHash(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateIntValue(agateHashInt(agateAsInt(args[0])));
  return true;
}

// Float

#define AGATE_FLOAT_CONSTANT(name, value)                                     \
static bool agateCoreFloat ## name(AgateVM *vm, int argc, AgateValue *args) { \
  args[0] = agateFloatValue(value);                                           \
  return true;                                                                \
}

AGATE_FLOAT_CONSTANT(Epsilon, DBL_EPSILON)
AGATE_FLOAT_CONSTANT(Lowest, -DBL_MAX)
AGATE_FLOAT_CONSTANT(Infinity, INFINITY)
AGATE_FLOAT_CONSTANT(Max, DBL_MAX)
AGATE_FLOAT_CONSTANT(Min, DBL_MIN)
AGATE_FLOAT_CONSTANT(Nan, NAN)
AGATE_FLOAT_CONSTANT(TrueMin, 0x1P-1074)

#undef AGATE_FLOAT_CONSTANT

#define AGATE_FLOAT_INFIX(name, op)                                           \
static bool agateCoreFloat ## name(AgateVM *vm, int argc, AgateValue *args) { \
  if (!agateValidateFloat(vm, args[1], "Right operand")) {                    \
    return false;                                                             \
  }                                                                           \
  args[0] = agateFloatValue(agateAsFloat(args[0]) op agateAsFloat(args[1]));  \
  return true;                                                                \
}

AGATE_FLOAT_INFIX(Plus,     +)
AGATE_FLOAT_INFIX(Minus,    -)
AGATE_FLOAT_INFIX(Multiply, *)
AGATE_FLOAT_INFIX(Divide,   /)

#undef AGATE_FLOAT_INFIX

#define AGATE_FLOAT_CMP(name, op)                                             \
static bool agateCoreFloat ## name(AgateVM *vm, int argc, AgateValue *args) { \
  if (!agateValidateFloat(vm, args[1], "Right operand")) {                    \
    return false;                                                             \
  }                                                                           \
  args[0] = agateBoolValue(agateAsFloat(args[0]) op agateAsFloat(args[1]));   \
  return true;                                                                \
}

AGATE_FLOAT_CMP(Lt,     <)
AGATE_FLOAT_CMP(LEq,    <=)
AGATE_FLOAT_CMP(Gt,     >)
AGATE_FLOAT_CMP(GEq,    >=)

#undef AGATE_FLOAT_CMP

static bool agateCoreFloatEqual(AgateVM *vm, int argc, AgateValue *args) {
  if (!agateIsFloat(args[1])) {
    args[0] = agateBoolValue(false);
    return true;
  }

  args[0] = agateBoolValue(agateAsFloat(args[0]) == agateAsFloat(args[1]));
  return true;
}

static bool agateCoreFloatNotEqual(AgateVM *vm, int argc, AgateValue *args) {
  if (!agateIsFloat(args[1])) {
    args[0] = agateBoolValue(true);
    return true;
  }

  args[0] = agateBoolValue(agateAsFloat(args[0]) != agateAsFloat(args[1]));
  return true;
}

static bool agateCoreFloatPrefixPlus(AgateVM *vm, int argc, AgateValue *args) {
  return true;
}

static bool agateCoreFloatPrefixMinus(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateFloatValue(-agateAsFloat(args[0]));
  return true;
}

static inline bool agateAlmostEquals(double a, double b, double abs_error, double rel_error) {
  if (a == b) {
    return true;
  }

  double diff = fabs(a - b);

  if (diff <= abs_error) {
    return true;
  }

  double max = fmin(fmax(fabs(a), fabs(b)), DBL_MAX);
  return diff <= max * rel_error;
}

static bool agateCoreFloatAlmostEquals2(AgateVM *vm, int argc, AgateValue *args) {
  if (!agateValidateFloat(vm, args[1], "First argument")) {
    return false;
  }

  if (!agateValidateFloat(vm, args[2], "Second argument")) {
    return false;
  }

  args[0] = agateBoolValue(agateAlmostEquals(agateAsFloat(args[1]), agateAsFloat(args[2]), DBL_EPSILON, DBL_EPSILON));
  return true;
}

static bool agateCoreFloatAlmostEquals4(AgateVM *vm, int argc, AgateValue *args) {
  if (!agateValidateFloat(vm, args[1], "First argument")) {
    return false;
  }

  if (!agateValidateFloat(vm, args[2], "Second argument")) {
    return false;
  }

  if (!agateValidateFloat(vm, args[3], "Absolute error")) {
    return false;
  }

  if (!agateValidateFloat(vm, args[4], "Relative error")) {
    return false;
  }

  args[0] = agateBoolValue(agateAlmostEquals(agateAsFloat(args[1]), agateAsFloat(args[2]), agateAsFloat(args[3]), agateAsFloat(args[4])));
  return true;
}

static bool agateCoreFloatIntegral(AgateVM *vm, int argc, AgateValue *args) {
  double integral;
  modf(agateAsFloat(args[0]), &integral);
  args[0] = agateFloatValue(integral);
  return true;
}

static bool agateCoreFloatFractional(AgateVM *vm, int argc, AgateValue *args) {
  double integral;
  double fractional = modf(agateAsFloat(args[0]), &integral);
  args[0] = agateFloatValue(fractional);
  return true;
}

static bool agateCoreFloatToI(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateIntValue((int64_t) agateAsFloat(args[0]));
  return true;
}

static bool agateCoreFloatIsInfinity(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateBoolValue(isinf(agateAsFloat(args[0])) != 0);
  return true;
}

static bool agateCoreFloatIsNan(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateBoolValue(isnan(agateAsFloat(args[0])) != 0);
  return true;
}

static bool agateCoreFloatToS(AgateVM *vm, int argc, AgateValue *args) {
#define AGATE_FLOAT_BUFFER_SIZE 32
  double value = agateAsFloat(args[0]);
  char buffer[AGATE_FLOAT_BUFFER_SIZE];
  char *start = NULL;
  ptrdiff_t size = 0;

  if (isnan(value)) {
    start = "nan";
    size = 3;
  } else if (isinf(value)) {
    if (value > 0.0) {
      start = "inf";
      size = 3;
    } else {
      start = "-inf";
      size = 4;
    }
  } else {
    start = buffer;
    size = snprintf(buffer, AGATE_FLOAT_BUFFER_SIZE, "%.14g", value);
  }

  args[0] = agateEntityValue(agateStringNew(vm, start, size));
  return true;
}

static bool agateCoreFloatHash(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateIntValue(agateHashFloat(agateAsFloat(args[0])));
  return true;
}

// Char

#define AGATE_CHAR_CMP(name, op)                                              \
static bool agateCoreChar ## name(AgateVM *vm, int argc, AgateValue *args) {  \
  if (!agateValidateChar(vm, args[1], "Right operand")) {                     \
    return false;                                                             \
  }                                                                           \
  args[0] = agateBoolValue(agateAsChar(args[0]) op agateAsChar(args[1]));     \
  return true;                                                                \
}

AGATE_CHAR_CMP(Lt,  <)
AGATE_CHAR_CMP(LEq, <=)
AGATE_CHAR_CMP(Gt,  >)
AGATE_CHAR_CMP(GEq, >=)

#undef AGATE_CHAR_CMP

static bool agateCoreCharToI(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateIntValue(agateAsChar(args[0]));
  return true;
}

static bool agateCoreCharToS(AgateVM *vm, int argc, AgateValue *args) {
  char buffer[AGATE_CHAR_BUFFER_SIZE];
  ptrdiff_t size = agateUtf8Encode(agateAsChar(args[0]), buffer);
  assert(size < AGATE_CHAR_BUFFER_SIZE);

  if (size == 0) {
    vm->error = agateEntityValue(agateStringNewFormat(vm, "Invalid chararacter."));
    return false;
  }

  args[0] = agateEntityValue(agateStringNew(vm, buffer, size));
  return true;
}

static bool agateCoreCharHash(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateIntValue(agateHashChar(agateAsChar(args[0])));
  return true;
}

// Fn

static bool agateCoreFnNew(AgateVM *vm, int argc, AgateValue *args) {
  if (!agateIsClosure(args[1])) {
    vm->error = AGATE_CONST_STRING(vm, "Argument must be a function.");
    return false;
  }

  args[0] = args[1];
  return true;
}

static bool agateCoreFnArity(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateIntValue(agateAsClosure(args[0])->function->arity);
  return true;
}

static bool agateCoreFnToS(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = AGATE_CONST_STRING(vm, "<fn>");
  return true;
}

// Array

static bool agateCoreArrayNew(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateEntityValue(agateArrayNew(vm));
  return true;
}

static bool agateCoreArrayAppend(AgateVM *vm, int argc, AgateValue *args) {
  agateValueArrayAppend(&agateAsArray(args[0])->elements, args[1], vm);
  args[0] = args[1];
  return true;
}

static bool agateCoreArrayPut(AgateVM *vm, int argc, AgateValue *args) {
  agateValueArrayAppend(&agateAsArray(args[0])->elements, args[1], vm);
  return true;
}

static bool agateCoreArrayClear(AgateVM *vm, int argc, AgateValue *args) {
  agateAsArray(args[0])->elements.size = 0;
  args[0] = agateNilValue();
  return true;
}

static bool agateCoreArraySize(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateIntValue(agateAsArray(args[0])->elements.size);
  return true;
}

static bool agateCoreArrayInsert(AgateVM *vm, int argc, AgateValue *args) {
  AgateArray *array = agateAsArray(args[0]);
  ptrdiff_t index = agateValidateIndex(vm, args[1], array->elements.size + 1, "Index");

  if (index == AGATE_INDEX_ERROR) {
    return false;
  }

  agateValueArrayInsert(&array->elements, index, args[2], vm);
  args[0] = args[2];
  return true;
}

static bool agateCoreArrayFind(AgateVM *vm, int argc, AgateValue *args) {
  AgateArray *array = agateAsArray(args[0]);
  args[0] = agateIntValue(agateValueArrayFind(&array->elements, args[1]));
  return true;
}

static bool agateCoreArrayErase(AgateVM *vm, int argc, AgateValue *args) {
  AgateArray *array = agateAsArray(args[0]);
  ptrdiff_t index = agateValidateIndex(vm, args[1], array->elements.size, "Index");

  if (index == AGATE_INDEX_ERROR) {
    return false;
  }

  args[0] = agateValueArrayErase(&array->elements, index);
  return true;
}

static bool agateCoreArrayRemove(AgateVM *vm, int argc, AgateValue *args) {
  AgateArray *array = agateAsArray(args[0]);
  ptrdiff_t index = agateValueArrayFind(&array->elements, args[1]);

  if (index == -1) {
    args[0] = agateNilValue();
    return true;
  }

  agateValueArrayErase(&array->elements, index);
  args[0] = args[1];
  return true;
}

static bool agateCoreArrayReverse(AgateVM *vm, int argc, AgateValue *args) {
  AgateArray *array = agateAsArray(args[0]);
  ptrdiff_t size = array->elements.size;

  for (ptrdiff_t i = 0; i < size / 2; ++i) {
    ptrdiff_t j = size - 1 - i;
    assert(0 <= j && j < size);
    AgateValue tmp = array->elements.data[i];
    array->elements.data[i] = array->elements.data[j];
    array->elements.data[j] = tmp;
  }

  return true;
}

static bool agateCoreArrayIterate(AgateVM *vm, int argc, AgateValue *args) {
  AgateArray *array = agateAsArray(args[0]);

  if (agateIsNil(args[1])) {
    if (array->elements.size == 0) {
      args[0] = agateNilValue();
    } else {
      args[0] = agateIntValue(0);
    }

    return true;
  }

  if (!agateValidateInt(vm, args[1], "Iterator")) {
    return false;
  }

  int64_t index = agateAsInt(args[1]);

  if (index < 0 || index >= array->elements.size - 1) {
    args[0] = agateNilValue();
  } else {
    args[0] = agateIntValue(index + 1);
  }

  return true;
}

static bool agateCoreArrayIteratorValue(AgateVM *vm, int argc, AgateValue *args) {
  assert(agateIsArray(args[0]));
  AgateArray *array = agateAsArray(args[0]);

  ptrdiff_t index = agateValidateIndex(vm, args[1], array->elements.size, "Iterator");

  if (index == AGATE_INDEX_ERROR) {
    return false;
  }

  args[0] = array->elements.data[index];
  return true;
}

static bool agateCoreArraySwap(AgateVM *vm, int argc, AgateValue *args) {
  AgateArray *array = agateAsArray(args[0]);
  ptrdiff_t index0 = agateValidateIndex(vm, args[1], array->elements.size, "Index 0");

  if (index0 == AGATE_INDEX_ERROR) {
    return false;
  }

  ptrdiff_t index1 = agateValidateIndex(vm, args[2], array->elements.size, "Index 1");

  if (index1 == AGATE_INDEX_ERROR) {
    return false;
  }

  AgateValue tmp = array->elements.data[index0];
  array->elements.data[index0] = array->elements.data[index1];
  array->elements.data[index1] = tmp;
  args[0] = agateNilValue();
  return true;
}

static bool agateCoreArraySubscriptGetter(AgateVM *vm, int argc, AgateValue *args) {
  AgateArray *array = agateAsArray(args[0]);

  if (agateIsInt(args[1])) {
    ptrdiff_t index = agateValidateIndex(vm, args[1], array->elements.size, "Index");

    if (index == AGATE_INDEX_ERROR) {
      return false;
    }

    args[0] = array->elements.data[index];
    return true;
  }

  if (agateIsRange(args[1])) {
    AgateExtent extent;

    if (!agateValidateRange(vm, agateAsRange(args[1]), array->elements.size, &extent)) {
      return false;
    }

    AgateArray *result = agateArrayNew(vm);

    if (extent.count > 0) {
      agatePushRoot(vm, (AgateEntity *) result);
      agateValueArrayResize(&result->elements, extent.count, agateNilValue(), vm);

      for (ptrdiff_t i = 0; i < extent.count; ++i) {
        result->elements.data[i] = array->elements.data[extent.start + i * extent.step];
      }
      agatePopRoot(vm);
    }

    args[0] = agateEntityValue(result);
    return true;
  }

  vm->error = AGATE_CONST_STRING(vm, "Index must be an integer or a range.");
  return false;
}

static bool agateCoreArraySubscriptSetter(AgateVM *vm, int argc, AgateValue *args) {
  AgateArray *array = agateAsArray(args[0]);
  ptrdiff_t index = agateValidateIndex(vm, args[1], array->elements.size, "Index");

  if (index == AGATE_INDEX_ERROR) {
    return false;
  }

  array->elements.data[index] = args[2];
  args[0] = args[2];
  return true;
}

// String

static bool agateCoreStringContains(AgateVM *vm, int argc, AgateValue *args) {
  AgateString *haystack = agateAsString(args[0]);

  const char *needle = NULL;
  ptrdiff_t needle_size = 0;

  char buffer[AGATE_CHAR_BUFFER_SIZE];

  if (agateIsChar(args[1])) {
    needle_size = agateUtf8Encode(agateAsChar(args[1]), buffer);
    assert(needle_size < AGATE_CHAR_BUFFER_SIZE - 1);

    if (needle_size == 0) {
      vm->error = AGATE_CONST_STRING(vm, "Invalid chararacter.");
      return false;
    }

    needle = buffer;
  } else if (agateIsString(args[1])) {
    AgateString *string = agateAsString(args[1]);
    needle = string->data;
    needle_size = string->size;
  } else {
    vm->error = AGATE_CONST_STRING(vm, "Argument must be a character or a string.");
    return false;
  }

  args[0] = agateBoolValue(agateStringFind(haystack, needle, needle_size, 0) != -1);
  return true;
}

static bool agateCoreStringStartsWith(AgateVM *vm, int argc, AgateValue *args) {
  if (!agateValidateString(vm, args[1], "Argument")) {
    return false;
  }

  AgateString *haystack = agateAsString(args[0]);
  AgateString *needle = agateAsString(args[1]);

  if (needle->size > haystack->size) {
    args[0] = agateBoolValue(false);
    return true;
  }

  args[0] = agateBoolValue(memcmp(haystack->data, needle->data, needle->size) == 0);
  return true;
}

static bool agateCoreStringEndsWith(AgateVM *vm, int argc, AgateValue *args) {
  if (!agateValidateString(vm, args[1], "Argument")) {
    return false;
  }

  AgateString *haystack = agateAsString(args[0]);
  AgateString *needle = agateAsString(args[1]);

  if (needle->size > haystack->size) {
    args[0] = agateBoolValue(false);
    return true;
  }

  args[0] = agateBoolValue(memcmp(haystack->data + haystack->size - needle->size, needle->data, needle->size) == 0);
  return true;
}

static bool agateCoreStringFind1(AgateVM *vm, int argc, AgateValue *args) {
  if (!agateValidateString(vm, args[1], "Argument")) {
    return false;
  }

  AgateString *haystack = agateAsString(args[0]);
  AgateString *needle = agateAsString(args[1]);
  ptrdiff_t index = agateStringFind(haystack, needle->data, needle->size, 0);
  args[0] = agateIntValue(index);
  return true;
}

static bool agateCoreStringFind2(AgateVM *vm, int argc, AgateValue *args) {
  if (!agateValidateString(vm, args[1], "Argument")) {
    return false;
  }

  AgateString *haystack = agateAsString(args[0]);
  AgateString *needle = agateAsString(args[1]);
  ptrdiff_t start = agateValidateIndex(vm, args[2], haystack->size, "Start");

  if (start == AGATE_INDEX_ERROR) {
    return false;
  }

  ptrdiff_t index = agateStringFind(haystack, needle->data, needle->size, start);
  args[0] = agateIntValue(index);
  return true;
}

static bool agateCoreStringReplace(AgateVM *vm, int argc, AgateValue *args) {
  if (!agateValidateString(vm, args[1], "From")) {
    return false;
  }

  AgateString *from = agateAsString(args[1]);

  if (from->size == 0) {
    vm->error = AGATE_CONST_STRING(vm, "From must be a non-empty string.");
    return false;
  }

  if (!agateValidateString(vm, args[2], "To")) {
    return false;
  }

  AgateString *to = agateAsString(args[2]);

  args[0] = agateEntityValue(agateStringReplace(vm, agateAsString(args[0]), from, to));
  return true;
}

static bool agateCoreStringTrim(AgateVM *vm, int argc, AgateValue *args) {
  if (!agateValidateString(vm, args[1], "Characters")) {
    return false;
  }

  assert(agateIsBool(args[2]));
  bool left = agateAsBool(args[2]);

  assert(agateIsBool(args[3]));
  bool right = agateAsBool(args[3]);

  AgateTrimKind kind = (AgateTrimKind) ((left ? 1 : 0) + (right ? 2 : 0));
  args[0] = agateEntityValue(agateStringTrim(vm, agateAsString(args[0]), agateAsString(args[1]), kind));
  return true;
}

static bool agateCoreStringSplit(AgateVM *vm, int argc, AgateValue *args) {
  AgateString *string = agateAsString(args[0]);

  const char *separator = NULL;
  ptrdiff_t separator_size = 0;

  char buffer[AGATE_CHAR_BUFFER_SIZE];

  if (agateIsChar(args[1])) {
    separator_size = agateUtf8Encode(agateAsChar(args[1]), buffer);
    assert(separator_size < AGATE_CHAR_BUFFER_SIZE - 1);
    separator = buffer;
  } else if (agateIsString(args[1])) {
    AgateString *raw = agateAsString(args[1]);
    separator = raw->data;
    separator_size = raw->size;
  }

  if (separator == NULL || separator_size == 0) {
    vm->error = AGATE_CONST_STRING(vm, "Separator must be a character or a non-empty string.");
    return false;
  }

  AgateArray *result = agateArrayNew(vm);
  agatePushRoot(vm, (AgateEntity *) result);

  ptrdiff_t start = 0;

  for (;;) {
    ptrdiff_t stop = agateStringFind(string, separator, separator_size, start);

    if (stop == -1) {
      break;
    }

    AgateString *item = agateStringNew(vm, string->data + start, stop - start);
    agatePushRoot(vm, (AgateEntity *) item);
    agateValueArrayAppend(&result->elements, agateEntityValue(item), vm);
    agatePopRoot(vm);

    start = stop + separator_size;
  }

  AgateString *item = agateStringNew(vm, string->data + start, string->size - start);
  agatePushRoot(vm, (AgateEntity *) item);
  agateValueArrayAppend(&result->elements, agateEntityValue(item), vm);
  agatePopRoot(vm);

  agatePopRoot(vm);
  args[0] = agateEntityValue(result);
  return true;
}

static bool agateCoreStringIterate(AgateVM *vm, int argc, AgateValue *args) {
  AgateString *string = agateAsString(args[0]);

  if (agateIsNil(args[1])) {
    if (string->size == 0) {
      args[0] = agateNilValue();
    } else {
      args[0] = agateIntValue(0);
    }

    return true;
  }

  if (!agateValidateInt(vm, args[1], "Iterator")) {
    return false;
  }

  int64_t index = agateAsInt(args[1]);

  if (index < 0) {
    args[0] = agateNilValue();
    return true;
  }

  do {
    ++index;

    if (index >= string->size) {
      args[0] = agateNilValue();
      return true;
    }
  } while ((string->data[index] & 0xC0) == 0x80);

  args[0] = agateIntValue(index);
  return true;
}

static bool agateCoreStringIteratorValue(AgateVM *vm, int argc, AgateValue *args) {
  AgateString *string = agateAsString(args[0]);

  if (!agateValidateInt(vm, args[1], "Iterator")) {
    return false;
  }

  int64_t index = agateAsInt(args[1]);

  if (index < 0 || index >= string->size) {
    vm->error = AGATE_CONST_STRING(vm, "Iterator out of bounds.");
    return false;
  }

  uint32_t c = agateUtf8Decode(string->data + index, string->size - index, NULL);

  if (c == AGATE_INVALID_CHAR) {
    vm->error = AGATE_CONST_STRING(vm, "Iterator does not point to a valid character.");
    return false;
  }

  args[0] = agateCharValue(c);
  return true;
}

static bool agateCoreStringPlus(AgateVM *vm, int argc, AgateValue *args) {
  if (!agateValidateString(vm, args[1], "Right operand")) {
    return false;
  }

  args[0] = agateEntityValue(agateStringNewFormat(vm, "@@", agateAsString(args[0]), agateAsString(args[1])));
  return true;
}

static bool agateCoreStringTimes(AgateVM *vm, int argc, AgateValue *args) {
  if (!agateIsInt(args[1]) || agateAsInt(args[1]) < 0) {
    vm->error = AGATE_CONST_STRING(vm, "Count must be a non-negative integer.");
    return false;
  }

  AgateString *string = agateAsString(args[0]);
  int64_t count = agateAsInt(args[1]);

  AgateCharArray buffer;
  agateCharArrayCreate(&buffer);

  for (int64_t i = 0; i < count; ++i) {
    agateCharArrayAppendMultiple(&buffer, string->data, string->size, vm);
  }

  args[0] = agateEntityValue(agateStringNew(vm, buffer.data, buffer.size));
  agateCharArrayDestroy(&buffer, vm);
  return true;
}

static bool agateCoreStringToS(AgateVM *vm, int argc, AgateValue *args) {
  return true;
}

static bool agateCoreStringToI(AgateVM *vm, int argc, AgateValue *args) {
  AgateString *string = agateAsString(args[0]);
  int64_t result;

  if (!vm->config.parse_int(string->data, string->size, 0, &result)) {
    vm->error = AGATE_CONST_STRING(vm, "Could not parse integer string.");
    return false;
  }

  args[0] = agateIntValue(result);
  return true;
}

static bool agateCoreStringToF(AgateVM *vm, int argc, AgateValue *args) {
  AgateString *string = agateAsString(args[0]);
  double result;

  if (!vm->config.parse_float(string->data, string->size, &result)) {
    vm->error = AGATE_CONST_STRING(vm, "Could not parse float string.");
    return false;
  }

  args[0] = agateFloatValue(result);
  return true;
}

static bool agateCoreStringHash(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateIntValue(agateAsString(args[0])->hash);
  return true;
}

// Map

static bool agateCoreMapNew(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateEntityValue(agateMapNew(vm));
  return true;
}

static bool agateCoreMapClear(AgateVM *vm, int argc, AgateValue *args) {
  agateTableClear(&agateAsMap(args[0])->members);
  args[0] = agateNilValue();
  return true;
}

static bool agateCoreMapSize(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateIntValue(agateAsMap(args[0])->members.size);
  return true;
}

static bool agateCoreMapErase(AgateVM *vm, int argc, AgateValue *args) {
  if (!agateValidateInt(vm, args[2], "hash")) {
    return false;
  }

  args[0] = agateTableErase(&agateAsMap(args[0])->members, args[1], agateAsInt(args[2]));
  return true;
}

static bool agateCoreMapContains(AgateVM *vm, int argc, AgateValue *args) {
  if (!agateValidateInt(vm, args[2], "hash")) {
    return false;
  }

  AgateValue value = agateTableFind(&agateAsMap(args[0])->members, args[1], agateAsInt(args[2]));
  args[0] = agateBoolValue(!agateIsUndefined(value));
  return true;
}

static bool agateCoreMapInsert(AgateVM *vm, int argc, AgateValue *args) {
  if (!agateValidateInt(vm, args[3], "hash")) {
    return false;
  }

  args[0] = agateBoolValue(agateTableInsert(&agateAsMap(args[0])->members, args[1], args[2], agateAsInt(args[3]), vm));
  return true;
}

static bool agateCoreMapPut(AgateVM *vm, int argc, AgateValue *args) {
  if (!agateValidateInt(vm, args[3], "Hash")) {
    return false;
  }

  agateTableInsert(&agateAsMap(args[0])->members, args[1], args[2], agateAsInt(args[3]), vm);
  return true;
}

static bool agateCoreMapSubscriptGetter(AgateVM *vm, int argc, AgateValue *args) {
  if (!agateValidateInt(vm, args[2], "Hash")) {
    return false;
  }

  AgateValue value = agateTableFind(&agateAsMap(args[0])->members, args[1], agateAsInt(args[2]));

  if (agateIsUndefined(value)) {
    args[0] = agateNilValue();
  } else {
    args[0] = value;
  }

  return true;
}

static bool agateCoreMapSubscriptSetter(AgateVM *vm, int argc, AgateValue *args) {
  if (!agateValidateInt(vm, args[3], "Hash")) {
    return false;
  }

  agateTableInsert(&agateAsMap(args[0])->members, args[1], args[2], agateAsInt(args[3]), vm);
  args[0] = args[2];
  return true;
}

static bool agateCoreMapIterate(AgateVM *vm, int argc, AgateValue *args) {
  AgateMap *map = agateAsMap(args[0]);

  if (map->members.size == 0) {
    args[0] = agateNilValue();
    return true;
  }

  int64_t index = 0;

  if (!agateIsNil(args[1])) {
    if (!agateValidateInt(vm, args[1], "Iterator")) {
      return false;
    }

    index = agateAsInt(args[1]);

    if (index < 0 || index >= map->members.capacity) {
      args[0] = agateNilValue();
      return true;
    }

    ++index;
  }

  for (; index < map->members.capacity; ++index) {
    if (!agateIsUndefined(map->members.entries[index].key)) {
      args[0] = agateIntValue(index);
      return true;
    }
  }

  args[0] = agateNilValue();
  return true;
}

static bool agateCoreMapKeyFromIterator(AgateVM *vm, int argc, AgateValue *args) {
  AgateMap *map = agateAsMap(args[0]);
  ptrdiff_t index = agateValidateIndex(vm, args[1], map->members.capacity, "Iterator");

  if (index == AGATE_INDEX_ERROR) {
    return false;
  }

  AgateTableEntry *entry = &map->members.entries[index];

  if (agateIsUndefined(entry->key)) {
    vm->error = AGATE_CONST_STRING(vm, "Invalid map iterator.");
    return false;
  }

  args[0] = entry->key;
  return true;
}

static bool agateCoreMapValueFromIterator(AgateVM *vm, int argc, AgateValue *args) {
  AgateMap *map = agateAsMap(args[0]);
  ptrdiff_t index = agateValidateIndex(vm, args[1], map->members.capacity, "Iterator");

  if (index == AGATE_INDEX_ERROR) {
    return false;
  }

  AgateTableEntry *entry = &map->members.entries[index];

  if (agateIsUndefined(entry->key)) {
    vm->error = AGATE_CONST_STRING(vm, "Invalid map iterator.");
    return false;
  }

  args[0] = entry->value;
  return true;
}

// Range

static bool agateCoreRangeFrom(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateIntValue(agateAsRange(args[0])->from);
  return true;
}

static bool agateCoreRangeTo(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateIntValue(agateAsRange(args[0])->to);
  return true;
}

static bool agateCoreRangeInclusive(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateBoolValue(agateAsRange(args[0])->kind == AGATE_RANGE_INCLUSIVE);
  return true;
}

static inline int64_t agateMin(int64_t lhs, int64_t rhs) {
  return lhs < rhs ? lhs : rhs;
}

static bool agateCoreRangeMin(AgateVM *vm, int argc, AgateValue *args) {
  AgateRange *range = agateAsRange(args[0]);
  args[0] = agateIntValue(agateMin(range->from, range->to));
  return true;
}

static inline int64_t agateMax(int64_t lhs, int64_t rhs) {
  return lhs < rhs ? rhs : lhs;
}

static bool agateCoreRangeMax(AgateVM *vm, int argc, AgateValue *args) {
  AgateRange *range = agateAsRange(args[0]);
  args[0] = agateIntValue(agateMax(range->from, range->to));
  return true;
}

static bool agateCoreRangeIterate(AgateVM *vm, int argc, AgateValue *args) {
  assert(agateIsRange(args[0]));
  AgateRange *range = agateAsRange(args[0]);

  if (range->from == range->to && range->kind == AGATE_RANGE_EXCLUSIVE) {
    args[0] = agateNilValue();
    return true;
  }

  if (agateIsNil(args[1])) {
    args[0] = agateIntValue(range->from);
    return true;
  }

  if (!agateValidateInt(vm, args[1], "Iterator")) {
    return false;
  }

  int64_t iterator = agateAsInt(args[1]);

  if (range->from < range->to) {
    ++iterator;

    if (iterator > range->to) {
      args[0] = agateNilValue();
      return true;
    }
  } else {
    --iterator;

    if (iterator < range->to) {
      args[0] = agateNilValue();
      return true;
    }
  }

  if (range->kind == AGATE_RANGE_EXCLUSIVE && iterator == range->to) {
    args[0] = agateNilValue();
    return true;
  }

  args[0] = agateIntValue(iterator);
  return true;
}

static bool agateCoreRangeIteratorValue(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = args[1];
  return true;
}

static bool agateCoreRangeHash(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateIntValue(agateRangeHash(agateAsRange(args[0])));
  return true;
}


// Math

#define AGATE_MATH_CONSTANT(name, value)                                     \
static bool agateCoreMath ## name(AgateVM *vm, int argc, AgateValue *args) { \
  args[0] = agateFloatValue(value);                                          \
  return true;                                                               \
}

AGATE_MATH_CONSTANT(E,        2.718281828459045235360287471352662498)
AGATE_MATH_CONSTANT(InvPi,    0.318309886183790671537767526745028724)
AGATE_MATH_CONSTANT(InvPi2,   0.636619772367581343075535053490057448)
AGATE_MATH_CONSTANT(InvSqrt2, 0.707106781186547524400844362104849039)
AGATE_MATH_CONSTANT(Ln2,      0.693147180559945309417232121458176568)
AGATE_MATH_CONSTANT(Ln10,     2.302585092994045684017991454684364208)
AGATE_MATH_CONSTANT(Log2E,    1.442695040888963407359924681001892137)
AGATE_MATH_CONSTANT(Log10E,   0.434294481903251827651128918916605082)
AGATE_MATH_CONSTANT(Pi,       3.141592653589793238462643383279502884)
AGATE_MATH_CONSTANT(Pi2,      1.570796326794896619231321691639751442)
AGATE_MATH_CONSTANT(Pi4,      0.785398163397448309615660845819875721)
AGATE_MATH_CONSTANT(Sqrt2,    1.414213562373095048801688724209698079)

#define AGATE_MATH_FLOAT_1(name, fn)                                          \
static bool agateCoreMath ## name(AgateVM *vm, int argc, AgateValue *args) {  \
  if (!agateValidateFloat(vm, args[1], "Value")) {                            \
    return false;                                                             \
  }                                                                           \
  args[0] = agateFloatValue(fn(agateAsFloat(args[1])));                       \
  return true;                                                                \
}

AGATE_MATH_FLOAT_1(Sin,   sin)
AGATE_MATH_FLOAT_1(Cos,   cos)
AGATE_MATH_FLOAT_1(Tan,   tan)
AGATE_MATH_FLOAT_1(ASin,  asin)
AGATE_MATH_FLOAT_1(ACos,  acos)
AGATE_MATH_FLOAT_1(ATan,  atan)
AGATE_MATH_FLOAT_1(Exp,   exp)
AGATE_MATH_FLOAT_1(Exp2,  exp2)
AGATE_MATH_FLOAT_1(Log,   log)
AGATE_MATH_FLOAT_1(Log10, log10)
AGATE_MATH_FLOAT_1(Log2,  log2)
AGATE_MATH_FLOAT_1(Sqrt,  sqrt)
AGATE_MATH_FLOAT_1(Cbrt,  cbrt)
AGATE_MATH_FLOAT_1(Floor, floor)
AGATE_MATH_FLOAT_1(Ceil,  ceil)
AGATE_MATH_FLOAT_1(Trunc, trunc)
AGATE_MATH_FLOAT_1(Round, round)

#undef AGATE_MATH_FLOAT_1

#define AGATE_MATH_FLOAT_2(name, fn)                                          \
static bool agateCoreMath ## name(AgateVM *vm, int argc, AgateValue *args) {  \
  if (!agateValidateFloat(vm, args[1], "Value")) {                            \
    return false;                                                             \
  }                                                                           \
  if (!agateValidateFloat(vm, args[2], "Value")) {                            \
    return false;                                                             \
  }                                                                           \
  args[0] = agateFloatValue(fn(agateAsFloat(args[1]), agateAsFloat(args[2])));  \
  return true;                                                                \
}

AGATE_MATH_FLOAT_2(ATan2, atan2)
AGATE_MATH_FLOAT_2(Hypot, hypot)

static bool agateCoreMathAbs(AgateVM *vm, int argc, AgateValue *args) {
  if (agateIsFloat(args[1])) {
    args[0] = agateFloatValue(fabs(agateAsFloat(args[1])));
    return true;
  }

  if (agateIsInt(args[1])) {
    args[0] = agateIntValue(abs(agateAsInt(args[1])));
    return true;
  }

  vm->error = AGATE_CONST_STRING(vm, "Argument must be an integer or a float.");
  return false;
}

static bool agateCoreMathSign(AgateVM *vm, int argc, AgateValue *args) {
  if (agateIsFloat(args[1])) {
    double val = agateAsFloat(args[1]);
    args[0] = agateFloatValue((val > 0.0) - (val < 0.0));
    return true;
  }

  if (agateIsInt(args[1])) {
    int64_t val = agateAsInt(args[1]);
    args[0] = agateIntValue((val > 0) - (val < 0));
    return true;
  }

  vm->error = AGATE_CONST_STRING(vm, "Argument must be an integer or a float.");
  return false;
}

static bool agateCoreMathMax(AgateVM *vm, int argc, AgateValue *args) {
  if (agateIsFloat(args[1]) && agateIsFloat(args[2])) {
    double lhs = agateAsFloat(args[1]);
    double rhs = agateAsFloat(args[2]);
    args[0] = agateFloatValue(fmax(lhs, rhs));
    return true;
  }

  if (agateIsInt(args[1]) && agateIsInt(args[2])) {
    int64_t lhs = agateAsInt(args[1]);
    int64_t rhs = agateAsInt(args[2]);
    args[0] = agateIntValue(lhs < rhs ? rhs : lhs);
    return true;
  }

  vm->error = AGATE_CONST_STRING(vm, "Arguments must both be integers or floats.");
  return false;
}

static bool agateCoreMathMin(AgateVM *vm, int argc, AgateValue *args) {
  if (agateIsFloat(args[1]) && agateIsFloat(args[2])) {
    double lhs = agateAsFloat(args[1]);
    double rhs = agateAsFloat(args[2]);
    args[0] = agateFloatValue(fmin(lhs, rhs));
    return true;
  }

  if (agateIsInt(args[1]) && agateIsInt(args[2])) {
    int64_t lhs = agateAsInt(args[1]);
    int64_t rhs = agateAsInt(args[2]);
    args[0] = agateIntValue(lhs < rhs ? lhs : rhs);
    return true;
  }

  vm->error = AGATE_CONST_STRING(vm, "Arguments must both be integers or floats.");
  return false;
}

static bool agateCoreMathClamp(AgateVM *vm, int argc, AgateValue *args) {
  if (agateIsFloat(args[1]) && agateIsFloat(args[2]) && agateIsFloat(args[3])) {
    double v = agateAsFloat(args[1]);
    double lo = agateAsFloat(args[2]);
    double hi = agateAsFloat(args[3]);
    args[0] = agateFloatValue(fmax(lo, fmin(hi, v)));
    return true;
  }

  if (agateIsInt(args[1]) && agateIsInt(args[2]) && agateIsInt(args[3])) {
    int64_t v = agateAsInt(args[1]);
    int64_t lo = agateAsInt(args[2]);
    int64_t hi = agateAsInt(args[3]);
    args[0] = agateIntValue( v < lo ? lo : v < hi ? v : hi);
    return true;
  }

  vm->error = AGATE_CONST_STRING(vm, "Arguments must all be integers or floats.");
  return false;
}

static int64_t agateExpBySquaring(int64_t x, int64_t n) {
  if (n < 0) {
    return x == 1 ? 1 : 0;
  }
  if (n == 0) {
    return 1;
  }
  int64_t y = 1;
  while (n > 1) {
    if (n % 2 == 1) {
      y = x * y;
    }
    x = x * x;
    n /= 2;
  }
  return x * y;
}

static bool agateCoreMathPow(AgateVM *vm, int argc, AgateValue *args) {
  if (agateIsFloat(args[1]) && agateIsFloat(args[2])) {
    double x = agateAsFloat(args[1]);
    double y = agateAsFloat(args[2]);
    args[0] = agateFloatValue(pow(x, y));
    return true;
  }

  if (agateIsInt(args[1]) && agateIsInt(args[2])) {
    int64_t x = agateAsInt(args[1]);
    int64_t y = agateAsInt(args[2]);
    args[0] = agateIntValue(agateExpBySquaring(x, y));
    return true;
  }

  vm->error = AGATE_CONST_STRING(vm, "Arguments must both be integers or floats.");
  return false;
}

// IO

static bool agateCoreIoPrint(AgateVM *vm, int argc, AgateValue *args) {
  if (!agateValidateString(vm, args[1], "obj")) {
    return false;
  }

  if (vm->config.print != NULL) {
    vm->config.print(vm, agateAsCString(args[1]));
  }

  args[0] = args[1];
  return true;
}

// System

static bool agateCoreSystemAbort(AgateVM *vm, int argc, AgateValue *args) {
  vm->error = args[1];
  return agateIsNil(args[1]);
}

static bool agateCoreSystemClock(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = agateFloatValue((double) clock() / CLOCKS_PER_SEC);
  return true;
}

static bool agateCoreSystemEnv(AgateVM *vm, int argc, AgateValue *args) {
  if (!agateValidateString(vm, args[1], "Name")) {
    return false;
  }

  const char *val = getenv(agateAsString(args[1])->data);

  if (val == NULL) {
    args[0] = agateNilValue();
  } else {
    args[0] = agateEntityValue(agateStringNew(vm, val, strlen(val)));
  }

  return true;
}

static bool agateCoreSystemGc(AgateVM *vm, int argc, AgateValue *args) {
  agateCollectGarbage(vm);
  args[0] = agateNilValue();
  return true;
}

static bool agateCoreSystemVersion(AgateVM *vm, int argc, AgateValue *args) {
  AgateArray *result = agateArrayNew(vm);
  agatePushRoot(vm, (AgateEntity *) result);

  agateValueArrayAppend(&result->elements, agateIntValue(AGATE_VERSION_MAJOR), vm);
  agateValueArrayAppend(&result->elements, agateIntValue(AGATE_VERSION_MINOR), vm);
  agateValueArrayAppend(&result->elements, agateIntValue(AGATE_VERSION_PATCH), vm);

  agatePopRoot(vm);
  args[0] = agateEntityValue(result);
  return true;
}

static bool agateCoreSystemVersionString(AgateVM *vm, int argc, AgateValue *args) {
  args[0] = AGATE_CONST_STRING(vm, AGATE_VERSION_STRING);
  return true;
}

// utils

static AgateClass *agateClassNewBasic(AgateVM *vm, AgateUnit *unit, const char *name) {
  AgateString *string = agateStringNew(vm, name, strlen(name));
  agatePushRoot(vm, (AgateEntity *) string);

  AgateClass *klass = agateClassNewBare(vm, unit, 0, string);
  agateUnitAddVariable(vm, unit, name, strlen(name), agateEntityValue(klass));

  agatePopRoot(vm);
  return klass;
}

static inline void agateClassBindPrimitive(AgateVM *vm, AgateClass *klass, const char *name, AgateNativeFunc func) {
  ptrdiff_t symbol = agateSymbolTableEnsure(&vm->method_names, name, strlen(name), vm);

  AgateMethod method;
  method.kind = AGATE_METHOD_NATIVE;
  method.as.native = func;

  agateClassBindMethod(vm, klass, symbol, method);
}

static void agateLoadCoreUnit(AgateVM *vm) {
  AgateUnit *core = agateUnitNew(vm, NULL);
  agatePushRoot(vm, (AgateEntity *) core);
  agateTableHashInsert(&vm->units, agateNilValue(), agateEntityValue(core), vm);
  agatePopRoot(vm);

  vm->object_class = agateClassNewBasic(vm, core, "Object");
  agateClassBindPrimitive(vm, vm->object_class, "!", agateCoreObjectNot);
  agateClassBindPrimitive(vm, vm->object_class, "==(_)", agateCoreObjectEqual);
  agateClassBindPrimitive(vm, vm->object_class, "!=(_)", agateCoreObjectNotEqual);
  agateClassBindPrimitive(vm, vm->object_class, "is(_)", agateCoreObjectIs);
  agateClassBindPrimitive(vm, vm->object_class, "to_s", agateCoreObjectToS);
  agateClassBindPrimitive(vm, vm->object_class, "type", agateCoreObjectType);

  vm->class_class = agateClassNewBasic(vm, core, "Class");
  agateClassBindSuperclass(vm, vm->class_class, vm->object_class);
  agateClassBindPrimitive(vm, vm->class_class, "name", agateCoreClassName);
  agateClassBindPrimitive(vm, vm->class_class, "hash", agateCoreClassHash);
  agateClassBindPrimitive(vm, vm->class_class, "supertype", agateCoreClassSupertype);
  agateClassBindPrimitive(vm, vm->class_class, "to_s", agateCoreClassToS);

  AgateClass *object_metaclass = agateClassNewBasic(vm, core, "Object metaclass");
  agateClassBindPrimitive(vm, object_metaclass, "same(_,_)", agateCoreObjectSame);

  vm->object_class->base.type = object_metaclass;
  object_metaclass->base.type = vm->class_class;
  vm->class_class->base.type = vm->class_class;

  agateClassBindSuperclass(vm, object_metaclass, vm->class_class);

  agateInterpret(vm, NULL, AgateCore);

  vm->array_class = agateAsClass(agateUnitFindVariable(vm, core, "Array"));
  agateClassBindPrimitive(vm, vm->array_class->base.type, "new()", agateCoreArrayNew);
  agateClassBindPrimitive(vm, vm->array_class, "__put(_)", agateCoreArrayPut);
  agateClassBindPrimitive(vm, vm->array_class, "[_]", agateCoreArraySubscriptGetter);
  agateClassBindPrimitive(vm, vm->array_class, "[_]=(_)", agateCoreArraySubscriptSetter);
  agateClassBindPrimitive(vm, vm->array_class, "append(_)", agateCoreArrayAppend);
  agateClassBindPrimitive(vm, vm->array_class, "clear()", agateCoreArrayClear);
  agateClassBindPrimitive(vm, vm->array_class, "size", agateCoreArraySize);
  agateClassBindPrimitive(vm, vm->array_class, "erase(_)", agateCoreArrayErase);
  agateClassBindPrimitive(vm, vm->array_class, "find(_)", agateCoreArrayFind);
  agateClassBindPrimitive(vm, vm->array_class, "insert(_,_)", agateCoreArrayInsert);
  agateClassBindPrimitive(vm, vm->array_class, "iterate(_)", agateCoreArrayIterate);
  agateClassBindPrimitive(vm, vm->array_class, "iterator_value(_)", agateCoreArrayIteratorValue);
  agateClassBindPrimitive(vm, vm->array_class, "remove(_)", agateCoreArrayRemove);
  agateClassBindPrimitive(vm, vm->array_class, "reverse()", agateCoreArrayReverse);
  agateClassBindPrimitive(vm, vm->array_class, "swap(_,_)", agateCoreArraySwap);

  vm->bool_class = agateAsClass(agateUnitFindVariable(vm, core, "Bool"));
  agateClassBindPrimitive(vm, vm->bool_class, "!", agateCoreBoolNot);
  agateClassBindPrimitive(vm, vm->bool_class, "hash", agateCoreBoolHash);
  agateClassBindPrimitive(vm, vm->bool_class, "to_i", agateCoreBoolToI);
  agateClassBindPrimitive(vm, vm->bool_class, "to_s", agateCoreBoolToS);

  vm->char_class = agateAsClass(agateUnitFindVariable(vm, core, "Char"));
  agateClassBindPrimitive(vm, vm->char_class, "<(_)", agateCoreCharLt);
  agateClassBindPrimitive(vm, vm->char_class, "<=(_)", agateCoreCharLEq);
  agateClassBindPrimitive(vm, vm->char_class, ">(_)", agateCoreCharGt);
  agateClassBindPrimitive(vm, vm->char_class, ">=(_)", agateCoreCharGEq);
  agateClassBindPrimitive(vm, vm->char_class, "hash", agateCoreCharHash);
  agateClassBindPrimitive(vm, vm->char_class, "to_i", agateCoreCharToI);
  agateClassBindPrimitive(vm, vm->char_class, "to_s", agateCoreCharToS);

  vm->float_class = agateAsClass(agateUnitFindVariable(vm, core, "Float"));
  agateClassBindPrimitive(vm, vm->float_class->base.type, "EPSILON", agateCoreFloatEpsilon);
  agateClassBindPrimitive(vm, vm->float_class->base.type, "LOWEST", agateCoreFloatLowest);
  agateClassBindPrimitive(vm, vm->float_class->base.type, "INFINITY", agateCoreFloatInfinity);
  agateClassBindPrimitive(vm, vm->float_class->base.type, "MAX", agateCoreFloatMax);
  agateClassBindPrimitive(vm, vm->float_class->base.type, "MIN", agateCoreFloatMin);
  agateClassBindPrimitive(vm, vm->float_class->base.type, "NAN", agateCoreFloatNan);
  agateClassBindPrimitive(vm, vm->float_class->base.type, "TRUE_MIN", agateCoreFloatTrueMin);
  agateClassBindPrimitive(vm, vm->float_class->base.type, "almost_equals(_,_)", agateCoreFloatAlmostEquals2);
  agateClassBindPrimitive(vm, vm->float_class->base.type, "almost_equals(_,_,_,_)", agateCoreFloatAlmostEquals4);
  agateClassBindPrimitive(vm, vm->float_class, "+", agateCoreFloatPrefixPlus);
  agateClassBindPrimitive(vm, vm->float_class, "-", agateCoreFloatPrefixMinus);
  agateClassBindPrimitive(vm, vm->float_class, "+(_)", agateCoreFloatPlus);
  agateClassBindPrimitive(vm, vm->float_class, "-(_)", agateCoreFloatMinus);
  agateClassBindPrimitive(vm, vm->float_class, "*(_)", agateCoreFloatMultiply);
  agateClassBindPrimitive(vm, vm->float_class, "/(_)", agateCoreFloatDivide);
  agateClassBindPrimitive(vm, vm->float_class, "==(_)", agateCoreFloatEqual);
  agateClassBindPrimitive(vm, vm->float_class, "!=(_)", agateCoreFloatNotEqual);
  agateClassBindPrimitive(vm, vm->float_class, "<(_)", agateCoreFloatLt);
  agateClassBindPrimitive(vm, vm->float_class, "<=(_)", agateCoreFloatLEq);
  agateClassBindPrimitive(vm, vm->float_class, ">(_)", agateCoreFloatGt);
  agateClassBindPrimitive(vm, vm->float_class, ">=(_)", agateCoreFloatGEq);
  agateClassBindPrimitive(vm, vm->float_class, "fractional", agateCoreFloatFractional);
  agateClassBindPrimitive(vm, vm->float_class, "hash", agateCoreFloatHash);
  agateClassBindPrimitive(vm, vm->float_class, "integral", agateCoreFloatIntegral);
  agateClassBindPrimitive(vm, vm->float_class, "is_infinity", agateCoreFloatIsInfinity);
  agateClassBindPrimitive(vm, vm->float_class, "is_nan", agateCoreFloatIsNan);
  agateClassBindPrimitive(vm, vm->float_class, "to_i", agateCoreFloatToI);
  agateClassBindPrimitive(vm, vm->float_class, "to_s", agateCoreFloatToS);

  vm->fn_class = agateAsClass(agateUnitFindVariable(vm, core, "Fn"));
  agateClassBindPrimitive(vm, vm->fn_class->base.type, "new(_)", agateCoreFnNew);
  agateClassBindPrimitive(vm, vm->fn_class, "arity", agateCoreFnArity);
  agateClassBindPrimitive(vm, vm->fn_class, "to_s", agateCoreFnToS);

  vm->int_class = agateAsClass(agateUnitFindVariable(vm, core, "Int"));
  agateClassBindPrimitive(vm, vm->int_class->base.type, "LOWEST", agateCoreIntLowest);
  agateClassBindPrimitive(vm, vm->int_class->base.type, "MAX", agateCoreIntMax);
  agateClassBindPrimitive(vm, vm->int_class->base.type, "MIN", agateCoreIntMin);
  agateClassBindPrimitive(vm, vm->int_class, "+", agateCoreIntPrefixPlus);
  agateClassBindPrimitive(vm, vm->int_class, "-", agateCoreIntPrefixMinus);
  agateClassBindPrimitive(vm, vm->int_class, "~", agateCoreIntInvert);
  agateClassBindPrimitive(vm, vm->int_class, "+(_)", agateCoreIntPlus);
  agateClassBindPrimitive(vm, vm->int_class, "-(_)", agateCoreIntMinus);
  agateClassBindPrimitive(vm, vm->int_class, "*(_)", agateCoreIntMultiply);
  agateClassBindPrimitive(vm, vm->int_class, "/(_)", agateCoreIntDivide);
  agateClassBindPrimitive(vm, vm->int_class, "%(_)", agateCoreIntModulo);
  agateClassBindPrimitive(vm, vm->int_class, "&(_)", agateCoreIntAnd);
  agateClassBindPrimitive(vm, vm->int_class, "|(_)", agateCoreIntOr);
  agateClassBindPrimitive(vm, vm->int_class, "^(_)", agateCoreIntXor);
  agateClassBindPrimitive(vm, vm->int_class, "<<(_)", agateCoreIntLeftShift);
  agateClassBindPrimitive(vm, vm->int_class, ">>(_)", agateCoreIntRightShift);
  agateClassBindPrimitive(vm, vm->int_class, ">>>(_)", agateCoreIntLogicalRightShift);
  agateClassBindPrimitive(vm, vm->int_class, "<(_)", agateCoreIntLt);
  agateClassBindPrimitive(vm, vm->int_class, "<=(_)", agateCoreIntLEq);
  agateClassBindPrimitive(vm, vm->int_class, ">(_)", agateCoreIntGt);
  agateClassBindPrimitive(vm, vm->int_class, ">=(_)", agateCoreIntGEq);
  agateClassBindPrimitive(vm, vm->int_class, "..(_)", agateCoreIntDotDot);
  agateClassBindPrimitive(vm, vm->int_class, "...(_)", agateCoreIntDotDotDot);
  agateClassBindPrimitive(vm, vm->int_class, "hash", agateCoreIntHash);
  agateClassBindPrimitive(vm, vm->int_class, "to_c", agateCoreIntToC);
  agateClassBindPrimitive(vm, vm->int_class, "to_f", agateCoreIntToF);
  agateClassBindPrimitive(vm, vm->int_class, "to_s", agateCoreIntToS);

  AgateClass *io_class = agateAsClass(agateUnitFindVariable(vm, core, "IO"));
  agateClassBindPrimitive(vm, io_class->base.type, "__print(_)", agateCoreIoPrint);

  vm->map_class = agateAsClass(agateUnitFindVariable(vm, core, "Map"));
  agateClassBindPrimitive(vm, vm->map_class->base.type, "new()", agateCoreMapNew);
  agateClassBindPrimitive(vm, vm->map_class, "__contains(_,_)", agateCoreMapContains);
  agateClassBindPrimitive(vm, vm->map_class, "__erase(_,_)", agateCoreMapErase);
  agateClassBindPrimitive(vm, vm->map_class, "__insert(_,_,_)", agateCoreMapInsert);
  agateClassBindPrimitive(vm, vm->map_class, "__key_from_iterator(_)", agateCoreMapKeyFromIterator);
  agateClassBindPrimitive(vm, vm->map_class, "__put(_,_,_)", agateCoreMapPut);
  agateClassBindPrimitive(vm, vm->map_class, "__subscript_getter(_,_)", agateCoreMapSubscriptGetter);
  agateClassBindPrimitive(vm, vm->map_class, "__subscript_setter(_,_,_)", agateCoreMapSubscriptSetter);
  agateClassBindPrimitive(vm, vm->map_class, "__value_from_iterator(_)", agateCoreMapValueFromIterator);
  agateClassBindPrimitive(vm, vm->map_class, "clear()", agateCoreMapClear);
  agateClassBindPrimitive(vm, vm->map_class, "size", agateCoreMapSize);
  agateClassBindPrimitive(vm, vm->map_class, "iterate(_)", agateCoreMapIterate);

  AgateClass *math_class = agateAsClass(agateUnitFindVariable(vm, core, "Math"));
  agateClassBindPrimitive(vm, math_class->base.type, "E", agateCoreMathE);
  agateClassBindPrimitive(vm, math_class->base.type, "INV_PI", agateCoreMathInvPi);
  agateClassBindPrimitive(vm, math_class->base.type, "INV_PI2", agateCoreMathInvPi2);
  agateClassBindPrimitive(vm, math_class->base.type, "INV_SQRT2", agateCoreMathInvSqrt2);
  agateClassBindPrimitive(vm, math_class->base.type, "LN2", agateCoreMathLn2);
  agateClassBindPrimitive(vm, math_class->base.type, "LN10", agateCoreMathLn10);
  agateClassBindPrimitive(vm, math_class->base.type, "LOG2E", agateCoreMathLog2E);
  agateClassBindPrimitive(vm, math_class->base.type, "LOG10E", agateCoreMathLog10E);
  agateClassBindPrimitive(vm, math_class->base.type, "PI", agateCoreMathPi);
  agateClassBindPrimitive(vm, math_class->base.type, "PI2", agateCoreMathPi2);
  agateClassBindPrimitive(vm, math_class->base.type, "PI4", agateCoreMathPi4);
  agateClassBindPrimitive(vm, math_class->base.type, "SQRT2", agateCoreMathSqrt2);
  agateClassBindPrimitive(vm, math_class->base.type, "abs(_)", agateCoreMathAbs);
  agateClassBindPrimitive(vm, math_class->base.type, "acos(_)", agateCoreMathACos);
  agateClassBindPrimitive(vm, math_class->base.type, "asin(_)", agateCoreMathASin);
  agateClassBindPrimitive(vm, math_class->base.type, "atan(_)", agateCoreMathATan);
  agateClassBindPrimitive(vm, math_class->base.type, "atan2(_,_)", agateCoreMathATan2);
  agateClassBindPrimitive(vm, math_class->base.type, "cbrt(_)", agateCoreMathCbrt);
  agateClassBindPrimitive(vm, math_class->base.type, "ceil(_)", agateCoreMathCeil);
  agateClassBindPrimitive(vm, math_class->base.type, "clamp(_,_,_)", agateCoreMathClamp);
  agateClassBindPrimitive(vm, math_class->base.type, "cos(_)", agateCoreMathCos);
  agateClassBindPrimitive(vm, math_class->base.type, "exp(_)", agateCoreMathExp);
  agateClassBindPrimitive(vm, math_class->base.type, "exp2(_)", agateCoreMathExp2);
  agateClassBindPrimitive(vm, math_class->base.type, "floor(_)", agateCoreMathFloor);
  agateClassBindPrimitive(vm, math_class->base.type, "hypot(_,_)", agateCoreMathHypot);
  agateClassBindPrimitive(vm, math_class->base.type, "log(_)", agateCoreMathLog);
  agateClassBindPrimitive(vm, math_class->base.type, "log10(_)", agateCoreMathLog10);
  agateClassBindPrimitive(vm, math_class->base.type, "log2(_)", agateCoreMathLog2);
  agateClassBindPrimitive(vm, math_class->base.type, "max(_,_)", agateCoreMathMax);
  agateClassBindPrimitive(vm, math_class->base.type, "min(_,_)", agateCoreMathMin);
  agateClassBindPrimitive(vm, math_class->base.type, "pow(_,_)", agateCoreMathPow);
  agateClassBindPrimitive(vm, math_class->base.type, "round(_)", agateCoreMathRound);
  agateClassBindPrimitive(vm, math_class->base.type, "sign(_)", agateCoreMathSign);
  agateClassBindPrimitive(vm, math_class->base.type, "sin(_)", agateCoreMathSin);
  agateClassBindPrimitive(vm, math_class->base.type, "sqrt(_)", agateCoreMathSqrt);
  agateClassBindPrimitive(vm, math_class->base.type, "tan(_)", agateCoreMathTan);
  agateClassBindPrimitive(vm, math_class->base.type, "trunc(_)", agateCoreMathTrunc);

  vm->nil_class = agateAsClass(agateUnitFindVariable(vm, core, "Nil"));
  agateClassBindPrimitive(vm, vm->nil_class, "!", agateCoreNilNot);
  agateClassBindPrimitive(vm, vm->nil_class, "hash", agateCoreNilHash);
  agateClassBindPrimitive(vm, vm->nil_class, "to_s", agateCoreNilToS);

  vm->range_class = agateAsClass(agateUnitFindVariable(vm, core, "Range"));
  agateClassBindPrimitive(vm, vm->range_class, "from", agateCoreRangeFrom);
  agateClassBindPrimitive(vm, vm->range_class, "hash", agateCoreRangeHash);
  agateClassBindPrimitive(vm, vm->range_class, "inclusive", agateCoreRangeInclusive);
  agateClassBindPrimitive(vm, vm->range_class, "iterate(_)", agateCoreRangeIterate);
  agateClassBindPrimitive(vm, vm->range_class, "iterator_value(_)", agateCoreRangeIteratorValue);
  agateClassBindPrimitive(vm, vm->range_class, "max", agateCoreRangeMax);
  agateClassBindPrimitive(vm, vm->range_class, "min", agateCoreRangeMin);
  agateClassBindPrimitive(vm, vm->range_class, "to", agateCoreRangeTo);

  vm->string_class = agateAsClass(agateUnitFindVariable(vm, core, "String"));
  agateClassBindPrimitive(vm, vm->string_class, "__trim(_,_,_)", agateCoreStringTrim);
  agateClassBindPrimitive(vm, vm->string_class, "+(_)", agateCoreStringPlus);
  agateClassBindPrimitive(vm, vm->string_class, "*(_)", agateCoreStringTimes);
  agateClassBindPrimitive(vm, vm->string_class, "contains(_)", agateCoreStringContains);
  agateClassBindPrimitive(vm, vm->string_class, "ends_with(_)", agateCoreStringEndsWith);
  agateClassBindPrimitive(vm, vm->string_class, "find(_)", agateCoreStringFind1);
  agateClassBindPrimitive(vm, vm->string_class, "find(_,_)", agateCoreStringFind2);
  agateClassBindPrimitive(vm, vm->string_class, "hash", agateCoreStringHash);
  agateClassBindPrimitive(vm, vm->string_class, "iterate(_)", agateCoreStringIterate);
  agateClassBindPrimitive(vm, vm->string_class, "iterator_value(_)", agateCoreStringIteratorValue);
  agateClassBindPrimitive(vm, vm->string_class, "replace(_,_)", agateCoreStringReplace);
  agateClassBindPrimitive(vm, vm->string_class, "split(_)", agateCoreStringSplit);
  agateClassBindPrimitive(vm, vm->string_class, "starts_with(_)", agateCoreStringStartsWith);
  agateClassBindPrimitive(vm, vm->string_class, "to_f", agateCoreStringToF);
  agateClassBindPrimitive(vm, vm->string_class, "to_i", agateCoreStringToI);
  agateClassBindPrimitive(vm, vm->string_class, "to_s", agateCoreStringToS);

  AgateClass *system_class = agateAsClass(agateUnitFindVariable(vm, core, "System"));
  agateClassBindPrimitive(vm, system_class->base.type, "abort(_)", agateCoreSystemAbort);
  agateClassBindPrimitive(vm, system_class->base.type, "clock", agateCoreSystemClock);
  agateClassBindPrimitive(vm, system_class->base.type, "env(_)", agateCoreSystemEnv);
  agateClassBindPrimitive(vm, system_class->base.type, "gc()", agateCoreSystemGc);
  agateClassBindPrimitive(vm, system_class->base.type, "version", agateCoreSystemVersion);
  agateClassBindPrimitive(vm, system_class->base.type, "version_string", agateCoreSystemVersionString);

  for (AgateEntity *entity = vm->entities; entity != NULL; entity = entity->next) {
    if (entity->kind == AGATE_ENTITY_STRING) {
      entity->type = vm->string_class;
    }
  }

}

static AgateHandle *agateHandleNew(AgateVM *vm, AgateValue value) {
  if (agateIsEntity(value)) {
    agatePushRoot(vm, agateAsEntity(value));
  }

  AgateHandle *handle = agateAllocate(vm, AgateHandle, 1);
  handle->value = value;

  if (agateIsEntity(value)) {
    agatePopRoot(vm);
  }

  if (vm->handles != NULL) {
    vm->handles->prev = handle;
  }

  handle->prev = NULL;
  handle->next = vm->handles;
  vm->handles = handle;
  return handle;
}

static void agateHandleDelete(AgateVM *vm, AgateHandle *handle) {
  assert(handle != NULL);

  if (vm->handles == handle) {
    vm->handles = handle->next;
  }

  if (handle->prev != NULL) {
    handle->prev->next = handle->next;
  }

  if (handle->next != NULL) {
    handle->next->prev = handle->prev;
  }

  handle->next = handle->prev = NULL;
  handle->value = agateNilValue();
  agateFree(vm, AgateHandle, handle);
}

/*
 * api
 */

AgateStatus agateInterpret(AgateVM *vm, const char *unit, const char *source) { // ~ wrenInterpret + wrenCompileSource
  AgateValue name = agateNilValue();

  if (unit != NULL) {
    name = agateEntityValue(agateStringNew(vm, unit, strlen(unit)));
    agatePushRoot(vm, agateAsEntity(name));
  }

  AgateClosure *closure = agateCompile(vm, name, source);

  if (unit != NULL) {
    agatePopRoot(vm);
  }

  if (closure == NULL) {
    return vm->status;
  }

  agatePushRoot(vm, (AgateEntity *) closure);
  agateClosureCall(vm, closure, 1);
  agatePopRoot(vm);

  AgateStatus status = agateRun(vm);

  if (status == AGATE_STATUS_OK) {
    agatePop(vm);
  }

  return status;
}


void agateConfigInitialize(AgateConfig *config) {
  config->reallocate = NULL;
  config->unit_handler = NULL;
  config->foreign_class_handler = NULL;
  config->foreign_method_handler = NULL;
  config->parse_int = NULL;
  config->parse_float = NULL;
  config->assert_handling = AGATE_ASSERT_ABORT;
  config->print = NULL;
  config->error = NULL;
  config->user_data = NULL;
}

AgateVM *agateNewVM(const AgateConfig *config) {
  // config

  AgateConfig normalized;

  if (config == NULL) {
    agateConfigInitialize(&normalized);
  } else {
    normalized = *config;
  }

  if (normalized.reallocate == NULL) {
    normalized.reallocate = agateReallocDefault;
  }

  if (normalized.parse_int == NULL) {
    normalized.parse_int = agateParseIntDefault;
  }

  if (normalized.parse_float == NULL) {
    normalized.parse_float = AgateParseFloatDefault;
  }

  AgateVM *vm = normalized.reallocate(NULL, sizeof(AgateVM), normalized.user_data);
  memset(vm, 0, sizeof(AgateVM));

  vm->config = normalized;

  // initialize memory related fields first

  vm->roots_count = 0;

  vm->bytes_allocated = 0;
  vm->bytes_threshold = 1024 * 1024;

  vm->entities = NULL;
  vm->handles = NULL;

  vm->gray_capacity = 0;
  vm->gray_count = 0;
  vm->gray_stack = NULL;

  // now we can use memory

  vm->status = AGATE_STATUS_OK;

  agateTableCreate(&vm->units);

  vm->array_class = NULL;
  vm->bool_class = NULL;
  vm->char_class = NULL;
  vm->class_class = NULL;
  vm->float_class = NULL;
  vm->fn_class = NULL;
  vm->int_class = NULL;
  vm->map_class = NULL;
  vm->nil_class = NULL;
  vm->object_class = NULL;
  vm->range_class = NULL;
  vm->string_class = NULL;

  agateTableCreate(&vm->method_names);

  vm->stack_capacity = AGATE_INITIAL_STACK_CAPACITY;
  vm->stack = agateAllocate(vm, AgateValue, vm->stack_capacity);

  vm->api_stack = NULL;

  vm->frames_capacity = AGATE_INITIAL_FRAMES_CAPACITY;
  vm->frames = agateAllocate(vm, AgateCallFrame, vm->frames_capacity);

  agateResetStack(vm);

  vm->error = agateNilValue();

  vm->compiler = NULL;
  vm->last_unit = NULL;

  // load core

  agateLoadCoreUnit(vm);

  return vm;
}

void agateDeleteVM(AgateVM *vm) {
  agateFreeArray(vm, AgateCallFrame, vm->frames, vm->frames_capacity);
  agateFreeArray(vm, AgateValue, vm->stack, vm->stack_capacity);
  agateTableDestroy(&vm->method_names, vm);
  agateTableDestroy(&vm->units, vm);

  // destroy entities

  AgateEntity *entity = vm->entities;

  while (entity != NULL) {
    AgateEntity *next = entity->next;
    agateEntityDelete(entity, vm);
    entity = next;
  }

  assert(vm->handles == NULL);
  assert(vm->bytes_allocated == 0);

  AgateReallocFunc reallocate = vm->config.reallocate;
  void *user_data = vm->config.user_data;

  reallocate(vm->gray_stack, 0, user_data);

  // delete the vm

  reallocate(vm, 0, user_data);
}


AgateHandle *agateMakeCallHandle(AgateVM *vm, const char *signature) {
  assert(signature != NULL);

  ptrdiff_t signature_size = strlen(signature);
  assert(signature_size > 0);

  int argc = 0;

  if (signature[signature_size - 1] == ')') {
    for (ptrdiff_t i = signature_size - 1; i > 0 && signature[i] != '('; --i) {
      if (signature[i] == '_') {
        ++argc;
      }
    }
  }

  if (signature[0] == '[') {
    for (ptrdiff_t i = 0; i < signature_size && signature[i] != ']'; ++i) {
      if (signature[i] == '_') {
        ++argc;
      }
    }
  }

  ptrdiff_t symbol = agateSymbolTableEnsure(&vm->method_names, signature, signature_size, vm);

  AgateFunction *function = agateFunctionNew(vm, NULL, argc + 1);
  AgateHandle *handle = agateHandleNew(vm, agateEntityValue(function));
  handle->value = agateEntityValue(agateClosureNew(vm, function));

  agateBytecodeWrite(&function->bc, AGATE_OP_INVOKE,      0, vm);
  agateBytecodeWrite(&function->bc, argc,                 0, vm);
  agateBytecodeWrite(&function->bc, (symbol >> 8) & 0xFF, 0, vm);
  agateBytecodeWrite(&function->bc,  symbol       & 0xFF, 0, vm);
  agateBytecodeWrite(&function->bc, AGATE_OP_RETURN,      0, vm);
  agateBytecodeWrite(&function->bc, AGATE_OP_END,         0, vm);

  agateFunctionBindName(vm, function, signature, signature_size);

  return handle;
}

AgateStatus agateCall(AgateVM *vm, AgateHandle *method) {
  assert(method != NULL);
  assert(agateIsClosure(method->value));
  assert(vm->api_stack != NULL);
  assert(vm->frames_count == 0); // TODO

  AgateClosure *closure = agateAsClosure(method->value);
  assert(vm->stack_top - vm->stack >= closure->function->arity);

  vm->api_stack = NULL; // XXX
  vm->stack_top = &vm->stack[closure->function->slot_count]; // XXX

  agateClosureCall(vm, closure, 0);
  AgateStatus status = agateRun(vm);

  vm->api_stack = vm->stack;
  return status;
}

ptrdiff_t agateSlotCount(AgateVM *vm) {
  if (vm->api_stack == NULL) {
    return 0;
  }

  return vm->stack_top - vm->api_stack;
}

void agateEnsureSlots(AgateVM *vm, ptrdiff_t slots_count) {
  if (vm->api_stack == NULL) {
    vm->api_stack = vm->stack_top;
  }

  ptrdiff_t current_size = vm->stack_top - vm->api_stack;

  if (current_size >= slots_count) {
    return;
  }

  ptrdiff_t needed = (vm->api_stack - vm->stack) + slots_count;
  agateEnsureStack(vm, needed);
  vm->stack_top = vm->api_stack + slots_count;
}

static inline bool agateIsSlotValid(AgateVM *vm, ptrdiff_t slot) {
  return 0 <= slot && slot < agateSlotCount(vm);
}

AgateType agateSlotType(AgateVM *vm, ptrdiff_t slot) {
  assert(agateIsSlotValid(vm, slot));
  switch (vm->api_stack[slot].kind) {
    case AGATE_VALUE_NIL:
      return AGATE_TYPE_NIL;
    case AGATE_VALUE_BOOL:
      return AGATE_TYPE_BOOL;
    case AGATE_VALUE_CHAR:
      return AGATE_TYPE_CHAR;
    case AGATE_VALUE_INT:
      return AGATE_TYPE_INT;
    case AGATE_VALUE_FLOAT:
      return AGATE_TYPE_FLOAT;
    case AGATE_VALUE_ENTITY:
      switch (agateEntityKind(vm->api_stack[slot])) {
        case AGATE_ENTITY_ARRAY:
          return AGATE_TYPE_ARRAY;
        case AGATE_ENTITY_FOREIGN:
          return AGATE_TYPE_FOREIGN;
        case AGATE_ENTITY_MAP:
          return AGATE_TYPE_MAP;
        case AGATE_ENTITY_STRING:
          return AGATE_TYPE_STRING;
        default:
          return AGATE_TYPE_UNKNOWN;
      }
    default:
      return AGATE_TYPE_UNKNOWN;
  }

  return AGATE_TYPE_UNKNOWN;
}

bool agateSlotGetBool(AgateVM *vm, ptrdiff_t slot) {
  assert(agateIsSlotValid(vm, slot));
  assert(agateIsBool(vm->api_stack[slot]));
  return agateAsBool(vm->api_stack[slot]);
}

uint32_t agateSlotGetChar(AgateVM *vm, ptrdiff_t slot) {
  assert(agateIsSlotValid(vm, slot));
  assert(agateIsChar(vm->api_stack[slot]));
  return agateAsChar(vm->api_stack[slot]);
}

int64_t agateSlotGetInt(AgateVM *vm, ptrdiff_t slot) {
  assert(agateIsSlotValid(vm, slot));
  assert(agateIsInt(vm->api_stack[slot]));
  return agateAsInt(vm->api_stack[slot]);
}

double agateSlotGetFloat(AgateVM *vm, ptrdiff_t slot) {
  assert(agateIsSlotValid(vm, slot));
  assert(agateIsFloat(vm->api_stack[slot]));
  return agateAsFloat(vm->api_stack[slot]);
}

void *agateSlotGetForeign(AgateVM *vm, ptrdiff_t slot) {
  assert(agateIsSlotValid(vm, slot));
  assert(agateIsForeign(vm->api_stack[slot]));
  return agateAsForeign(vm->api_stack[slot])->data;
}

const char *agateSlotGetString(AgateVM *vm, ptrdiff_t slot) {
  assert(agateIsSlotValid(vm, slot));
  assert(agateIsString(vm->api_stack[slot]));
  return agateAsCString(vm->api_stack[slot]);
}

const char *agateSlotGetStringSize(AgateVM *vm, ptrdiff_t slot, ptrdiff_t *size) {
  assert(agateIsSlotValid(vm, slot));
  assert(agateIsString(vm->api_stack[slot]));
  AgateString *string = agateAsString(vm->api_stack[slot]);

  if (size != NULL) {
    *size = string->size;
  }

  return string->data;
}

AgateHandle *agateSlotGetHandle(AgateVM *vm, ptrdiff_t slot) {
  assert(agateIsSlotValid(vm, slot));
  return agateHandleNew(vm, vm->api_stack[slot]);
}

void agateReleaseHandle(AgateVM *vm, AgateHandle *handle) {
  agateHandleDelete(vm, handle);
}

static inline void agateSlotSetValue(AgateVM *vm, ptrdiff_t slot, AgateValue value) {
  assert(agateIsSlotValid(vm, slot));
  vm->api_stack[slot] = value;
}

void agateSlotSetNil(AgateVM *vm, ptrdiff_t slot) {
  agateSlotSetValue(vm, slot, agateNilValue());
}

void agateSlotSetBool(AgateVM *vm, ptrdiff_t slot, bool value) {
  agateSlotSetValue(vm, slot, agateBoolValue(value));
}

void agateSlotSetChar(AgateVM *vm, ptrdiff_t slot, uint32_t value) {
  agateSlotSetValue(vm, slot, agateCharValue(value));
}

void agateSlotSetInt(AgateVM *vm, ptrdiff_t slot, int64_t value) {
  agateSlotSetValue(vm, slot, agateIntValue(value));
}

void agateSlotSetFloat(AgateVM *vm, ptrdiff_t slot, double value) {
  agateSlotSetValue(vm, slot, agateFloatValue(value));
}

void agateSlotSetString(AgateVM *vm, ptrdiff_t slot, const char *text) {
  agateSlotSetValue(vm, slot, agateEntityValue(agateStringNew(vm, text, strlen(text))));
}

void agateSlotSetStringSize(AgateVM *vm, ptrdiff_t slot, const char *text, ptrdiff_t size) {
  agateSlotSetValue(vm, slot, agateEntityValue(agateStringNew(vm, text, size)));
}

void agateSlotSetHandle(AgateVM *vm, ptrdiff_t slot, AgateHandle *handle) {
  assert(handle != NULL);
  agateSlotSetValue(vm, slot, handle->value);
}

void agateSlotArrayNew(AgateVM *vm, ptrdiff_t slot) {
  agateSlotSetValue(vm, slot, agateEntityValue(agateArrayNew(vm)));
}

ptrdiff_t agateSlotArraySize(AgateVM *vm, ptrdiff_t slot) {
  assert(agateIsSlotValid(vm, slot));
  agateIsArray(vm->api_stack[slot]);
  AgateArray *array = agateAsArray(vm->api_stack[slot]);
  return array->elements.size;
}

void agateSlotArrayGet(AgateVM *vm, ptrdiff_t array_slot, ptrdiff_t index, ptrdiff_t element_slot) {
  assert(agateIsSlotValid(vm, array_slot));
  assert(agateIsSlotValid(vm, element_slot));
  agateIsArray(vm->api_stack[array_slot]);
  AgateArray *array = agateAsArray(vm->api_stack[array_slot]);

  index = agateValidateIndexValue(vm, index, array->elements.size, "Index");
  assert(index != AGATE_INDEX_ERROR);

  vm->api_stack[element_slot] = array->elements.data[index];
}

void agateSlotArraySet(AgateVM *vm, ptrdiff_t array_slot, ptrdiff_t index, ptrdiff_t element_slot) {
  assert(agateIsSlotValid(vm, array_slot));
  assert(agateIsSlotValid(vm, element_slot));
  agateIsArray(vm->api_stack[array_slot]);
  AgateArray *array = agateAsArray(vm->api_stack[array_slot]);

  index = agateValidateIndexValue(vm, index, array->elements.size, "Index");
  assert(index != AGATE_INDEX_ERROR);

  array->elements.data[index] = vm->api_stack[element_slot];
}

void agateSlotArrayInsert(AgateVM *vm, ptrdiff_t array_slot, ptrdiff_t index, ptrdiff_t element_slot) {
  assert(agateIsSlotValid(vm, array_slot));
  assert(agateIsSlotValid(vm, element_slot));
  agateIsArray(vm->api_stack[array_slot]);
  AgateArray *array = agateAsArray(vm->api_stack[array_slot]);

  index = agateValidateIndexValue(vm, index, array->elements.size + 1, "Index");
  assert(index != AGATE_INDEX_ERROR);

  agateValueArrayInsert(&array->elements, index, vm->api_stack[element_slot], vm);
}

void agateSlotArrayErase(AgateVM *vm, ptrdiff_t array_slot, ptrdiff_t index, ptrdiff_t element_slot) {
  assert(agateIsSlotValid(vm, array_slot));
  assert(agateIsSlotValid(vm, element_slot));
  agateIsArray(vm->api_stack[array_slot]);
  AgateArray *array = agateAsArray(vm->api_stack[array_slot]);

  index = agateValidateIndexValue(vm, index, array->elements.size, "Index");
  assert(index != AGATE_INDEX_ERROR);

  AgateValue erased = agateValueArrayErase(&array->elements, index);
  vm->api_stack[element_slot] = erased;
}

void agateSlotMapNew(AgateVM *vm, ptrdiff_t slot) {
  agateSlotSetValue(vm, slot, agateEntityValue(agateMapNew(vm)));
}

ptrdiff_t agateSlotMapSize(AgateVM *vm, ptrdiff_t slot) {
  assert(agateIsSlotValid(vm, slot));
  assert(agateIsMap(vm->api_stack[slot]));
  AgateMap *map = agateAsMap(vm->api_stack[slot]);
  return map->members.size;
}

bool agateSlotMapContains(AgateVM *vm, ptrdiff_t map_slot, ptrdiff_t key_slot) {
  assert(agateIsSlotValid(vm, map_slot));
  assert(agateIsSlotValid(vm, key_slot));
  assert(agateIsMap(vm->api_stack[map_slot]));
  AgateMap *map = agateAsMap(vm->api_stack[map_slot]);

  if (!agateHasNativeHash(vm->api_stack[key_slot])) {
    vm->error = AGATE_CONST_STRING(vm, "Key must be a value type.");
    return false;
  }

  AgateValue value = agateTableFind(&map->members, vm->api_stack[key_slot], agateValueHash(vm->api_stack[key_slot]));
  return !agateIsUndefined(value);
}

void agateSlotMapGet(AgateVM *vm, ptrdiff_t map_slot, ptrdiff_t key_slot, ptrdiff_t value_slot) {
  assert(agateIsSlotValid(vm, map_slot));
  assert(agateIsSlotValid(vm, key_slot));
  assert(agateIsSlotValid(vm, value_slot));
  assert(agateIsMap(vm->api_stack[map_slot]));
  AgateMap *map = agateAsMap(vm->api_stack[map_slot]);
  AgateValue key = vm->api_stack[key_slot];

  if (!agateHasNativeHash(key)) {
    vm->error = AGATE_CONST_STRING(vm, "Key must be a value type.");
    return;
  }

  AgateValue value = agateTableHashFind(&map->members, key);

  if (agateIsUndefined(value)) {
    value = agateNilValue();
  }

  vm->api_stack[value_slot] = value;
}

void agateSlotMapSet(AgateVM *vm, ptrdiff_t map_slot, ptrdiff_t key_slot, ptrdiff_t value_slot) {
  assert(agateIsSlotValid(vm, map_slot));
  assert(agateIsSlotValid(vm, key_slot));
  assert(agateIsSlotValid(vm, value_slot));
  assert(agateIsMap(vm->api_stack[map_slot]));
  AgateMap *map = agateAsMap(vm->api_stack[map_slot]);
  AgateValue key = vm->api_stack[key_slot];

  if (!agateHasNativeHash(key)) {
    vm->error = AGATE_CONST_STRING(vm, "Key must be a value type.");
    return;
  }

  AgateValue value = vm->api_stack[value_slot];
  agateTableHashInsert(&map->members, key, value, vm);
}

void agateSlotMapErase(AgateVM *vm, ptrdiff_t map_slot, ptrdiff_t key_slot, ptrdiff_t value_slot) {
  assert(agateIsSlotValid(vm, map_slot));
  assert(agateIsSlotValid(vm, key_slot));
  assert(agateIsSlotValid(vm, value_slot));
  assert(agateIsMap(vm->api_stack[map_slot]));
  AgateMap *map = agateAsMap(vm->api_stack[map_slot]);
  AgateValue key = vm->api_stack[key_slot];

  if (!agateHasNativeHash(key)) {
    vm->error = AGATE_CONST_STRING(vm, "Key must be a value type.");
    return;
  }

  AgateValue erased = agateTableHashErase(&map->members, key);
  vm->api_stack[value_slot] = erased;
}

bool agateHasUnit(AgateVM *vm, const char *unit_name) {
  assert(unit_name != NULL);

  AgateValue name = agateEntityValue(agateStringNew(vm, unit_name, strlen(unit_name)));
  agatePushRoot(vm, agateAsEntity(name));

  AgateUnit *unit = agateGetUnit(vm, name);

  agatePopRoot(vm);
  return unit != NULL;
}

bool agateHasVariable(AgateVM *vm, const char *unit_name, const char *variable_name) {
  assert(unit_name != NULL);
  assert(variable_name != NULL);

  AgateValue name = agateEntityValue(agateStringNew(vm, unit_name, strlen(unit_name)));
  agatePushRoot(vm, agateAsEntity(name));

  AgateUnit *unit = agateGetUnit(vm, name);
  assert(unit != NULL);

  agatePopRoot(vm);

  ptrdiff_t symbol = agateSymbolTableFind(&unit->object_names, variable_name, strlen(variable_name));
  return symbol != -1;
}

void agateGetVariable(AgateVM *vm, const char *unit_name, const char *variable_name, ptrdiff_t slot) {
  assert(unit_name != NULL);
  assert(variable_name != NULL);

  AgateValue name = agateEntityValue(agateStringNew(vm, unit_name, strlen(unit_name)));
  agatePushRoot(vm, agateAsEntity(name));

  AgateUnit *unit = agateGetUnit(vm, name);
  assert(unit != NULL);

  agatePopRoot(vm);

  ptrdiff_t symbol = agateSymbolTableFind(&unit->object_names, variable_name, strlen(variable_name));
  assert(symbol != -1);

  agateSlotSetValue(vm, slot, unit->object_values.data[symbol]);
}

void agateAbort(AgateVM *vm, ptrdiff_t slot) {
  assert(agateIsSlotValid(vm, slot));
  vm->error = vm->api_stack[slot];
}

void *agateGetUserData(AgateVM *vm) {
  return vm->config.user_data;
}

void agateSetUserData(AgateVM *vm, void *user_data) {
  vm->config.user_data = user_data;
}

/*
 * types - parser
 */

typedef enum {
  AGATE_TOKEN_AS,
  AGATE_TOKEN_AMP,
  AGATE_TOKEN_AMP_AMP,
  AGATE_TOKEN_AND,
  AGATE_TOKEN_ASSERT,
  AGATE_TOKEN_BANG,
  AGATE_TOKEN_BANG_EQUAL,
  AGATE_TOKEN_BAR,
  AGATE_TOKEN_BAR_BAR,
  AGATE_TOKEN_BREAK,
  AGATE_TOKEN_CARET,
  AGATE_TOKEN_CHAR,
  AGATE_TOKEN_CLASS,
  AGATE_TOKEN_COLON,
  AGATE_TOKEN_COMMA,
  AGATE_TOKEN_CONSTRUCT,
  AGATE_TOKEN_CONTINUE,
  AGATE_TOKEN_DEF,
  AGATE_TOKEN_DOT,
  AGATE_TOKEN_DOT_DOT,
  AGATE_TOKEN_DOT_DOT_DOT,
  AGATE_TOKEN_ELSE,
  AGATE_TOKEN_EQUAL,
  AGATE_TOKEN_EQUAL_EQUAL,
  AGATE_TOKEN_FALSE,
  AGATE_TOKEN_FIELD_CLASS,
  AGATE_TOKEN_FIELD_INSTANCE,
  AGATE_TOKEN_FLOAT,
  AGATE_TOKEN_FOR,
  AGATE_TOKEN_FOREIGN,
  AGATE_TOKEN_GREATER,
  AGATE_TOKEN_GREATER_EQUAL,
  AGATE_TOKEN_GREATER_GREATER,
  AGATE_TOKEN_GREATER_GREATER_GREATER,
  AGATE_TOKEN_IDENTIFIER,
  AGATE_TOKEN_IF,
  AGATE_TOKEN_IMPORT,
  AGATE_TOKEN_IN,
  AGATE_TOKEN_INTEGER,
  AGATE_TOKEN_INTERPOLATION,
  AGATE_TOKEN_IS,
  AGATE_TOKEN_LEFT_BRACE,
  AGATE_TOKEN_LEFT_BRACKET,
  AGATE_TOKEN_LEFT_PAREN,
  AGATE_TOKEN_LESS,
  AGATE_TOKEN_LESS_EQUAL,
  AGATE_TOKEN_LESS_LESS,
  AGATE_TOKEN_LOOP,
  AGATE_TOKEN_MINUS,
  AGATE_TOKEN_NIL,
  AGATE_TOKEN_ONCE,
  AGATE_TOKEN_OR,
  AGATE_TOKEN_PERCENT,
  AGATE_TOKEN_PLUS,
  AGATE_TOKEN_QUESTION,
  AGATE_TOKEN_RETURN,
  AGATE_TOKEN_RIGHT_BRACE,
  AGATE_TOKEN_RIGHT_BRACKET,
  AGATE_TOKEN_RIGHT_PAREN,
  AGATE_TOKEN_SLASH,
  AGATE_TOKEN_STAR,
  AGATE_TOKEN_STATIC,
  AGATE_TOKEN_STRING,
  AGATE_TOKEN_SUPER,
  AGATE_TOKEN_THIS,
  AGATE_TOKEN_TILDE,
  AGATE_TOKEN_TRUE,
  AGATE_TOKEN_WHILE,

  AGATE_TOKEN_EOL,
  AGATE_TOKEN_EOF,
  AGATE_TOKEN_ERROR,
} AgateTokenKind;

typedef struct {
  AgateTokenKind kind;
  const char *start;
  ptrdiff_t size;
  int line;
  AgateValue value;
} AgateToken;

typedef struct {
  AgateVM *vm;
  AgateUnit *unit;

  const char *source_start;
  const char *source_end;
  const char *token_start;
  const char *token_current;
  int line;

  AgateToken current;
  AgateToken previous;

  int interpolation[AGATE_MAX_INTERPOLATION_NESTING];
  int interpolation_count;

  bool has_error;
} AgateParser;


typedef struct {
  const char *name;
  ptrdiff_t size;
  int depth;
  bool is_captured;
} AgateLocal;

typedef struct {
  ptrdiff_t index;
  AgateCapture capture;
} AgateCompilerUpvalue;

typedef struct AgateLoopContext {
  ptrdiff_t start;
  ptrdiff_t exit_jump;

  ptrdiff_t breaks[AGATE_MAX_BREAKS];
  ptrdiff_t breaks_count;

  int scope_depth;
  struct AgateLoopContext *enclosing;
} AgateLoopContext;


typedef enum {
  AGATE_SIG_METHOD,
  AGATE_SIG_GETTER,
  AGATE_SIG_SETTER,
  AGATE_SIG_SUBSCRIPT_GETTER,
  AGATE_SIG_SUBSCRIPT_SETTER,
  AGATE_SIG_CONSTRUCTOR
} AgateSignatureKind;

typedef struct {
  char name[AGATE_MAX_METHOD_SIGNATURE_SIZE];
  ptrdiff_t size;
} AgateSignatureBuilder;

typedef struct {
  const char *name;
  ptrdiff_t size;
  AgateSignatureKind kind;
  int arity;
} AgateSignature;

AGATE_ARRAY_DECLARE(SymbolArray, ptrdiff_t)

typedef struct {
  AgateString *name;
  AgateTable fields;
  AgateSymbolArray instance_methods;
  AgateSymbolArray class_methods;
  bool is_foreign;
  bool is_method_static;
  AgateSignature *signature;
} AgateClassContext;

struct AgateCompiler {
  AgateParser *parser;
  struct AgateCompiler *parent;

  AgateLocal locals[AGATE_MAX_LOCALS];
  ptrdiff_t locals_count;

  AgateCompilerUpvalue upvalues[AGATE_MAX_UPVALUES];

  int scope_depth;
  ptrdiff_t slot_count;

  AgateLoopContext *loop;
  AgateClassContext *enclosing_class;

  AgateFunction *function;
  AgateTable constants;

  bool is_initializer;
};

typedef enum {
  AGATE_SCOPE_LOCAL,
  AGATE_SCOPE_UPVALUE,
  AGATE_SCOPE_GLOBAL,
} AgateScope;

typedef struct {
  AgateScope scope;
  ptrdiff_t index;
} AgateVariable;

typedef enum {
  AGATE_BLOCK_SINGLE_LINE,
  AGATE_BLOCK_MULTIPLE_LINE,
} AgateBlockKind;

AGATE_ARRAY_DEFINE(SymbolArray, ptrdiff_t)


/*
 * parser - error
 */

static void agateErrorPrint(AgateParser *parser, int line, const char *label, const char *format, va_list args) {
  if (parser->has_error) {
    return;
  }

  parser->has_error = true;

  if (parser->vm->config.error == NULL) {
    return;
  }

  #define AGATE_ERROR_MESSAGE_SIZE 512
  char message[AGATE_ERROR_MESSAGE_SIZE];
  int size = snprintf(message, AGATE_ERROR_MESSAGE_SIZE, "%s: ", label);
  size += vsnprintf(message + size, AGATE_ERROR_MESSAGE_SIZE - size, format, args);
  assert(size < AGATE_ERROR_MESSAGE_SIZE);

  AgateString *unit_string = parser->unit->name;
  const char *unit_name = unit_string ? unit_string->data : "<unknown>";

  parser->vm->config.error(parser->vm, AGATE_ERROR_COMPILE, unit_name, line, message);
  parser->vm->status = AGATE_STATUS_COMPILE_ERROR;
}

static void agateLexicalError(AgateParser *parser, const char *format, ...) {
  va_list args;
  va_start(args, format);
  agateErrorPrint(parser, parser->line, "Error", format, args);
  va_end(args);
}

static void agateError(AgateCompiler *compiler, const char *format, ...) {
  AgateToken *token = &compiler->parser->previous;

  if (token->kind == AGATE_TOKEN_ERROR) {
    return;
  }

  va_list args;
  va_start(args, format);

  if (token->kind == AGATE_TOKEN_EOL) {
    agateErrorPrint(compiler->parser, token->line, "Error at newline", format, args);
  } else if (token->kind == AGATE_TOKEN_EOF) {
    agateErrorPrint(compiler->parser, token->line, "Error at end of file", format, args);
  } else {
    #define AGATE_ERROR_LABEL_SIZE 256
    char label[AGATE_ERROR_LABEL_SIZE];

    if (token->size < AGATE_ERROR_LABEL_SIZE) {
      snprintf(label, AGATE_ERROR_LABEL_SIZE, "Error at '%.*s'", (int) token->size, token->start);
    } else {
      snprintf(label, AGATE_ERROR_LABEL_SIZE, "Error at '%.*s...'", AGATE_ERROR_LABEL_SIZE - 15, token->start);
    }

    agateErrorPrint(compiler->parser, token->line, label, format, args);
  }

  va_end(args);
}

/*
 * parser - constants
 */

static ptrdiff_t agateCompilerAddConstant(AgateCompiler *compiler, AgateValue value) {
  AgateValue existing = agateTableHashFind(&compiler->constants, value);

  if (!agateIsUndefined(existing)) {
    assert(agateIsInt(existing));
    return agateAsInt(existing);
  }

  ptrdiff_t symbol = compiler->function->bc.constants.size;

  if (symbol < AGATE_MAX_CONSTANTS) {
    if (agateIsEntity(value)) {
      agatePushRoot(compiler->parser->vm, agateAsEntity(value));
    }

    agateBytecodeAddConstant(&compiler->function->bc, value, compiler->parser->vm);
    agateTableHashInsert(&compiler->constants, value, agateIntValue(symbol), compiler->parser->vm);

    if (agateIsEntity(value)) {
      agatePopRoot(compiler->parser->vm);
    }
  } else {
    agateError(compiler, "A function may only contain %d unique constants.", AGATE_MAX_CONSTANTS);
  }

  return symbol;
}

static void agateCompilerCreate(AgateCompiler *compiler, AgateParser *parser, AgateCompiler *parent, bool is_method) {
  compiler->parser = parser;
  compiler->parent = parent;

  compiler->loop = NULL;
  compiler->enclosing_class = NULL;
  compiler->is_initializer = false;

  compiler->function = NULL;
  agateTableCreate(&compiler->constants);

  parser->vm->compiler = compiler;

  compiler->locals_count = 1;
  compiler->slot_count = compiler->locals_count;

  AgateLocal *local = &compiler->locals[0];

  if (is_method) {
    local->name = "this";
    local->size = 4;
  } else {
    local->name = NULL;
    local->size = 0;
  }

  local->depth = -1;
  local->is_captured = false;

  if (parent == NULL) {
    compiler->scope_depth = -1;
  } else {
    compiler->scope_depth = 0;
  }

  compiler->function = agateFunctionNew(parser->vm, parser->unit, compiler->locals_count);
}


/*
 * parser - scanner
 */

static inline bool agateParserAtEnd(const AgateParser *parser) {
  return *parser->token_current == '\0';
}

static inline char agateParserAdvanceChar(AgateParser *parser) {
  return *parser->token_current++;
}

static inline bool agateCompilerMatchChar(AgateParser *parser, char expected) {
  if (agateParserAtEnd(parser)) {
    return false;
  }

  if (*parser->token_current != expected) {
    return false;
  }

  ++parser->token_current;
  return true;
}

static inline char agateParserPeekChar(const AgateParser *parser) {
  return *parser->token_current;
}

static inline char agateParserPeekNextChar(const AgateParser *parser) {
  if (agateParserAtEnd(parser)) {
    return '\0';
  }

  return parser->token_current[1];
}

static void agateParserSkipWhitespace(AgateParser *parser) {
  for (;;) {
    char c = agateParserPeekChar(parser);

    switch (c) {
      case ' ':
      case '\r':
      case '\t':
        agateParserAdvanceChar(parser);
        break;
      case '#': // comment
        do {
          agateParserAdvanceChar(parser);
        } while (agateParserPeekChar(parser) != '\0'  && agateParserPeekChar(parser) != '\n');
        return;
      default:
        return;
    }
  }
}

/*
 * parser - textutils
 */

static inline bool agateIsDigit(char c) {
  return '0' <= c && c <= '9';
}

static inline bool agateIsBinDigit(char c) {
  return c == '0' || c == '1';
}

static inline bool agateIsOctDigit(char c) {
  return '0' <= c && c <= '7';
}

static inline bool agateIsHexDigit(char c) {
  return ('0' <= c && c <= '9') || ('A' <= c && c <= 'F') || ('a' <= c && c <= 'f');
}

static inline bool agateIsUpperAlpha(char c) {
  return 'A' <= c && c <= 'Z';
}

static inline bool agateIsLowerAlpha(char c) {
  return 'a' <= c && c <= 'z';
}

static inline bool agateIsAlpha(char c) {
  return agateIsLowerAlpha(c) || agateIsUpperAlpha(c) || c == '_';
}

static inline uint32_t agateComputeHex(char c) {
  if ('0' <= c && c <= '9') { return c - '0'; }
  if ('a' <= c && c <= 'f') { return 10 + (c - 'a'); }
  if ('A' <= c && c <= 'F') { return 10 + (c - 'A'); }
  return AGATE_INVALID_CHAR;
}

static inline uint32_t agateParserHexEscapeDecode(AgateParser *parser, int size) {
  uint32_t res = 0;

  for (int i = 0; i < size; ++i) {
    if (agateParserAtEnd(parser)) { return AGATE_INVALID_CHAR; }
    uint32_t val = agateComputeHex(agateParserAdvanceChar(parser));
    if (val == AGATE_INVALID_CHAR) { return AGATE_INVALID_CHAR; }
    res = (res << 4) + val;
  }

  return res;
}

static uint32_t agateParserEscapeDecode(AgateParser *parser) {
  agateParserAdvanceChar(parser); // eat '\'

  if (agateParserAtEnd(parser)) { return AGATE_INVALID_CHAR; }

  uint32_t res = AGATE_INVALID_CHAR;
  char c = agateParserAdvanceChar(parser);

  switch (c) {
    case '0': res = 0x00; break;
    case 'a': res = 0x07; break;
    case 'b': res = 0x08; break;
    case 't': res = 0x09; break;
    case 'n': res = 0x0A; break;
    case 'v': res = 0x0B; break;
    case 'f': res = 0x0C; break;
    case 'r': res = 0x0D; break;
    case 'e': res = 0x1B; break;
    case '"': res = 0x22; break;
    case '%': res = 0x25; break;
    case '\'': res = 0x27; break;
    case '\\': res = 0x5C; break;

    case 'u':
      res = agateParserHexEscapeDecode(parser, 4);
      break;

    case 'U':
      res = agateParserHexEscapeDecode(parser, 8);
      break;

    default:
      agateLexicalError(parser, "Invalid escape character '%c' (0x%.2X).", c, c);
      break;
  }

  return res;
}

uint32_t agateParserUtf8Decode(AgateParser *parser) {
  return agateUtf8Decode(parser->token_current, parser->source_end - parser->token_current, &parser->token_current);
}

bool agateParserUtf8Encode(AgateParser *parser, uint32_t c, AgateCharArray *array) {
  char buffer[AGATE_CHAR_BUFFER_SIZE];
  ptrdiff_t size = agateUtf8Encode(c, buffer);
  assert(size < AGATE_CHAR_BUFFER_SIZE);

  if (size == 0) {
    return false;
  }

  for (ptrdiff_t i = 0; i < size; ++i) {
    agateCharArrayAppend(array, buffer[i], parser->vm);
  }

  return true;
}



/*
 * scanner - tokens
 */

static void agateParserMakeToken(AgateParser *parser, AgateTokenKind kind) {
  parser->current.kind = kind;
  parser->current.start = parser->token_start;
  parser->current.size = parser->token_current - parser->token_start;
  parser->current.line = parser->line;
  parser->current.value = agateNilValue();

  if (kind == AGATE_TOKEN_EOL) {
    --parser->current.line;
  }
}

static void agateParserReadString(AgateParser *parser) {
  AgateCharArray buffer;
  agateCharArrayCreate(&buffer);

  AgateTokenKind kind = AGATE_TOKEN_STRING;

  for (;;) {
    if (agateParserAtEnd(parser)) {
      agateLexicalError(parser, "Unterminated string.");
    }

    char c = agateParserPeekChar(parser);

    if (c == '"') {
      agateParserAdvanceChar(parser); // eat "
      break;
    }

    if (c == '%') {
      agateParserAdvanceChar(parser); // eat %

      if (parser->interpolation_count == AGATE_MAX_INTERPOLATION_NESTING) {
        agateLexicalError(parser, "Interpolation may only nest %d levels deep.", AGATE_MAX_INTERPOLATION_NESTING);
        agateCharArrayDestroy(&buffer, parser->vm);
        return;
      }

      if (agateParserAdvanceChar(parser) != '(') {
        agateLexicalError(parser, "Expect '(' after '%%'.");
        agateCharArrayDestroy(&buffer, parser->vm);
        return;
      }

      parser->interpolation[parser->interpolation_count++] = 1;
      kind = AGATE_TOKEN_INTERPOLATION;
      break;
    }

    uint32_t codepoint = AGATE_INVALID_CHAR;

    if (c == '\\') {
      codepoint = agateParserEscapeDecode(parser);
    } else {
      codepoint = agateParserUtf8Decode(parser);
    }

    if (codepoint == AGATE_INVALID_CHAR) {
      agateLexicalError(parser, "Malformed string.");
      agateCharArrayDestroy(&buffer, parser->vm);
      return;
    }

    if (!agateParserUtf8Encode(parser, codepoint, &buffer)) {
      agateLexicalError(parser, "Malformed string.");
      agateCharArrayDestroy(&buffer, parser->vm);
      return;
    }
  }

  agateParserMakeToken(parser, kind);
  parser->current.value = agateEntityValue(agateStringNew(parser->vm, buffer.data, buffer.size));

  agateCharArrayDestroy(&buffer, parser->vm);
}

static void agateParserReadChar(AgateParser *parser) {
  if (agateParserAtEnd(parser)) {
    agateLexicalError(parser, "Unterminated char.");
  }

  char c = agateParserPeekChar(parser);

  if (c == '\'') {
    agateLexicalError(parser, "Empty char.");
  }

  uint32_t codepoint = AGATE_INVALID_CHAR;

  if (c == '\\') {
    codepoint = agateParserEscapeDecode(parser);
  } else {
    codepoint = agateParserUtf8Decode(parser);
  }

  if (codepoint == AGATE_INVALID_CHAR) {
    agateLexicalError(parser, "Malformed char.");
    return;
  }

  if (agateParserAdvanceChar(parser) != '\'') {
    agateLexicalError(parser, "Unterminated char.");
    return;
  }

  agateParserMakeToken(parser, AGATE_TOKEN_CHAR);
  parser->current.value = agateCharValue(codepoint);
}


static inline int64_t agateParseInt(AgateParser *parser, const char *text, ptrdiff_t size, int base) {
  int64_t value;

  if (parser->vm->config.parse_int(text, size, base, &value)) {
    return value;
  }

  agateLexicalError(parser, "Could not parse integer '%.*s'.", (int) size, text);
  return 0;
}

static inline double agateParseFloat(AgateParser *parser, const char *text, ptrdiff_t size) {
  double value;

  if (parser->vm->config.parse_float(text, size, &value)) {
    return value;
  }

  agateLexicalError(parser, "Could not parse float '%.*s'.", (int) size, text);
  return 0.0;
}

static void agateParserReadFractional(AgateParser *parser) {
  if (agateCompilerMatchChar(parser, '.')) {
    assert(agateIsDigit(agateParserPeekChar(parser)));

    while (agateIsDigit(agateParserPeekChar(parser))) {
      agateParserAdvanceChar(parser);
    }
  }

  if (agateCompilerMatchChar(parser, 'e')) {
    (void) (agateCompilerMatchChar(parser, '+') || agateCompilerMatchChar(parser, '-'));

    while (agateIsDigit(agateParserPeekChar(parser))) {
      agateParserAdvanceChar(parser);
    }
  }

  double value = agateParseFloat(parser, parser->token_start, parser->token_current - parser->token_start);
  agateParserMakeToken(parser, AGATE_TOKEN_FLOAT);
  parser->current.value = agateFloatValue(value);
}

static void agateParserReadNumber(AgateParser *parser) {
  while (agateIsDigit(agateParserPeekChar(parser))) {
    agateParserAdvanceChar(parser);
  }

  if ((agateParserPeekChar(parser) == '.' && agateIsDigit(agateParserPeekNextChar(parser))) || agateParserPeekChar(parser) == 'e') {
    agateParserReadFractional(parser);
    return;
  }

  int64_t value = agateParseInt(parser, parser->token_start, parser->token_current - parser->token_start, 10);
  agateParserMakeToken(parser, AGATE_TOKEN_INTEGER);
  parser->current.value = agateIntValue(value);
}

static void agateParserReadNumberWithZero(AgateParser *parser) {
  char c = agateParserPeekChar(parser);
  switch (c) {
    case 'x':
    {
      agateParserAdvanceChar(parser); // eat 'x'

      while (agateIsHexDigit(agateParserPeekChar(parser))) {
        agateParserAdvanceChar(parser);
      }

      int64_t value = agateParseInt(parser, parser->token_start + 2, parser->token_current - parser->token_start - 2, 16);
      agateParserMakeToken(parser, AGATE_TOKEN_INTEGER);
      parser->current.value = agateIntValue(value);
      return;
    }
    case 'o':
    {
      agateParserAdvanceChar(parser); // eat 'o'

      while (agateIsOctDigit(agateParserPeekChar(parser))) {
        agateParserAdvanceChar(parser);
      }

      int64_t value = agateParseInt(parser, parser->token_start + 2, parser->token_current - parser->token_start - 2, 8);
      agateParserMakeToken(parser, AGATE_TOKEN_INTEGER);
      parser->current.value = agateIntValue(value);
      return;
    }
    case 'b':
    {
      agateParserAdvanceChar(parser); // eat 'b'

      while (agateIsBinDigit(agateParserPeekChar(parser))) {
        agateParserAdvanceChar(parser);
      }

      int64_t value = agateParseInt(parser, parser->token_start + 2, parser->token_current - parser->token_start - 2, 2);
      agateParserMakeToken(parser, AGATE_TOKEN_INTEGER);
      parser->current.value = agateIntValue(value);
      return;
    }
    case '.':
      if (agateIsDigit(agateParserPeekNextChar(parser))) {
        agateParserReadFractional(parser);
        return;
      }
      break;
    case 'e':
      agateParserReadFractional(parser);
      return;
    default:
      break;
  }

  // just zero
  agateParserMakeToken(parser, AGATE_TOKEN_INTEGER);
  parser->current.value = agateIntValue(0);
}

typedef struct {
  const char *name;
  ptrdiff_t size;
  AgateTokenKind kind;
} AgateKeyword;

static const AgateKeyword AgateKeywordArray[] = {
  { "as",         2, AGATE_TOKEN_AS },
  { "assert",     6, AGATE_TOKEN_ASSERT },
  { "break",      5, AGATE_TOKEN_BREAK },
  { "class",      5, AGATE_TOKEN_CLASS },
  { "construct",  9, AGATE_TOKEN_CONSTRUCT },
  { "continue",   8, AGATE_TOKEN_CONTINUE },
  { "def",        3, AGATE_TOKEN_DEF },
  { "else",       4, AGATE_TOKEN_ELSE },
  { "false",      5, AGATE_TOKEN_FALSE },
  { "for",        3, AGATE_TOKEN_FOR },
  { "foreign",    7, AGATE_TOKEN_FOREIGN },
  { "if",         2, AGATE_TOKEN_IF },
  { "import",     6, AGATE_TOKEN_IMPORT },
  { "in",         2, AGATE_TOKEN_IN },
  { "is",         2, AGATE_TOKEN_IS },
  { "loop",       4, AGATE_TOKEN_LOOP },
  { "nil",        3, AGATE_TOKEN_NIL },
  { "once",       4, AGATE_TOKEN_ONCE },
  { "return",     6, AGATE_TOKEN_RETURN },
  { "static",     6, AGATE_TOKEN_STATIC },
  { "super",      5, AGATE_TOKEN_SUPER },
  { "this",       4, AGATE_TOKEN_THIS },
  { "true",       4, AGATE_TOKEN_TRUE },
  { "while",      5, AGATE_TOKEN_WHILE },
};

static inline bool agateKeywordEquals(const AgateKeyword *keyword, const char *name, ptrdiff_t size) {
  if (keyword->size != size) {
    return false;
  }

  return strncmp(keyword->name, name, size) == 0;
}

static void agateParserReadIdentifier(AgateParser *parser, AgateTokenKind kind) {
  while (agateIsAlpha(agateParserPeekChar(parser)) || agateIsDigit(agateParserPeekChar(parser))) {
    agateParserAdvanceChar(parser);
  }

  const ptrdiff_t count = AGATE_ARRAY_SIZE(AgateKeywordArray);

  const char *name = parser->token_start;
  ptrdiff_t size = parser->token_current - parser->token_start;

  for (ptrdiff_t i = 0; i < count; ++i) {
    if (agateKeywordEquals(AgateKeywordArray + i, name, size)) {
      kind = AgateKeywordArray[i].kind;
    }
  }

  agateParserMakeToken(parser, kind);
  parser->current.value = agateEntityValue(agateStringNew(parser->vm, parser->current.start, parser->current.size));
}

static inline void agateParserTwoCharToken(AgateParser *parser, AgateTokenKind t1, char c2, AgateTokenKind t2) {
  if (agateCompilerMatchChar(parser, c2)) {
    agateParserMakeToken(parser, t2);
  } else {
    agateParserMakeToken(parser, t1);
  }
}

static inline void agateParserTwoCharTokenAlt(AgateParser *parser, AgateTokenKind t1, char c2, AgateTokenKind t2, char c3, AgateTokenKind t3) {
  if (agateCompilerMatchChar(parser, c2)) {
    agateParserMakeToken(parser, t2);
  } else if (agateCompilerMatchChar(parser, c3)) {
    agateParserMakeToken(parser, t3);
  } else {
    agateParserMakeToken(parser, t1);
  }
}

static void agateParserAdvance(AgateParser *parser) {
  agateParserSkipWhitespace(parser);

  parser->previous = parser->current;
  parser->token_start = parser->token_current;

  if (agateParserAtEnd(parser)) {
    agateParserMakeToken(parser, AGATE_TOKEN_EOF);
    return;
  }

  char c = agateParserAdvanceChar(parser);

  if (c == '\n') {
    ++parser->line;
    agateParserMakeToken(parser, AGATE_TOKEN_EOL);
    return;
  }

  if (agateIsAlpha(c)) {
    agateParserReadIdentifier(parser, AGATE_TOKEN_IDENTIFIER);
    return;
  }

  if (c == '0') {
    agateParserReadNumberWithZero(parser);
    return;
  }

  if (agateIsDigit(c)) {
    agateParserReadNumber(parser);
    return;
  }

  switch (c) {
    case '(':
      if (parser->interpolation_count > 0) {
        ++parser->interpolation[parser->interpolation_count - 1];
      }

      agateParserMakeToken(parser, AGATE_TOKEN_LEFT_PAREN);
      return;

    case ')':
      if (parser->interpolation_count > 0 && --parser->interpolation[parser->interpolation_count - 1] == 0) {
        --parser->interpolation_count;
        agateParserReadString(parser);
        return;
      }

      agateParserMakeToken(parser, AGATE_TOKEN_RIGHT_PAREN);
      return;

    case '{': agateParserMakeToken(parser, AGATE_TOKEN_LEFT_BRACE); return;
    case '}': agateParserMakeToken(parser, AGATE_TOKEN_RIGHT_BRACE); return;
    case '[': agateParserMakeToken(parser, AGATE_TOKEN_LEFT_BRACKET); return;
    case ']': agateParserMakeToken(parser, AGATE_TOKEN_RIGHT_BRACKET); return;
    case ':': agateParserMakeToken(parser, AGATE_TOKEN_COLON); return;
    case '?': agateParserMakeToken(parser, AGATE_TOKEN_QUESTION); return;
    case ',': agateParserMakeToken(parser, AGATE_TOKEN_COMMA); return;
    case '-': agateParserMakeToken(parser, AGATE_TOKEN_MINUS); return;
    case '+': agateParserMakeToken(parser, AGATE_TOKEN_PLUS); return;
    case '/': agateParserMakeToken(parser, AGATE_TOKEN_SLASH); return;
    case '*': agateParserMakeToken(parser, AGATE_TOKEN_STAR); return;
    case '%': agateParserMakeToken(parser, AGATE_TOKEN_PERCENT); return;
    case '^': agateParserMakeToken(parser, AGATE_TOKEN_CARET); return;
    case '~': agateParserMakeToken(parser, AGATE_TOKEN_TILDE); return;

    case '&': agateParserTwoCharToken(parser, AGATE_TOKEN_AMP, '&', AGATE_TOKEN_AMP_AMP); return;
    case '|': agateParserTwoCharToken(parser, AGATE_TOKEN_BAR, '|', AGATE_TOKEN_BAR_BAR); return;
    case '!': agateParserTwoCharToken(parser, AGATE_TOKEN_BANG, '=', AGATE_TOKEN_BANG_EQUAL); return;
    case '=': agateParserTwoCharToken(parser, AGATE_TOKEN_EQUAL, '=', AGATE_TOKEN_EQUAL_EQUAL); return;
    case '<':
      if (agateCompilerMatchChar(parser, '=')) {
        agateParserMakeToken(parser, AGATE_TOKEN_LESS_EQUAL);
      } else {
        agateParserTwoCharToken(parser, AGATE_TOKEN_LESS, '<', AGATE_TOKEN_LESS_LESS);
      }
      return;
    case '>':
      if (agateCompilerMatchChar(parser, '=')) {
        agateParserMakeToken(parser, AGATE_TOKEN_GREATER_EQUAL);
      } else if (agateCompilerMatchChar(parser, '>')) {
        agateParserTwoCharToken(parser, AGATE_TOKEN_GREATER_GREATER, '>', AGATE_TOKEN_GREATER_GREATER_GREATER);
      } else {
        agateParserMakeToken(parser, AGATE_TOKEN_GREATER);
      }
      return;
    case '@':
      if (agateCompilerMatchChar(parser, '@')) {
        agateParserReadIdentifier(parser, AGATE_TOKEN_FIELD_CLASS);
      } else {
        agateParserReadIdentifier(parser, AGATE_TOKEN_FIELD_INSTANCE);
      }
      return;

    case '.':
      if (agateCompilerMatchChar(parser, '.')) {
        if (agateCompilerMatchChar(parser, '.')) {
          agateParserMakeToken(parser, AGATE_TOKEN_DOT_DOT_DOT);
        } else {
          agateParserMakeToken(parser, AGATE_TOKEN_DOT_DOT);
        }
      } else {
        agateParserMakeToken(parser, AGATE_TOKEN_DOT);
      }
      return;

    case '\'': agateParserReadChar(parser); return;
    case '"': agateParserReadString(parser); return;
  }

  if (' ' <= c && c <= '~') { // printable ASCII characters
    agateLexicalError(parser, "Unexpected character: '%c' (0X%.2X).", c, c);
  } else {
    agateLexicalError(parser, "Unexpected byte: 0X%.2X.", c);
  }

  agateParserMakeToken(parser, AGATE_TOKEN_ERROR);
}

/*
 * parser - utils
 */

static inline AgateTokenKind agateCompilerPeek(AgateCompiler *compiler) {
  return compiler->parser->current.kind;
}

static inline bool agateCompilerCheck(AgateCompiler *compiler, AgateTokenKind kind) {
  return agateCompilerPeek(compiler) == kind;
}

static inline bool agateCompilerMatch(AgateCompiler *compiler, AgateTokenKind kind) {
  if (!agateCompilerCheck(compiler, kind)) {
    return false;
  }

  agateParserAdvance(compiler->parser);
  return true;
}

static inline void agateCompilerConsume(AgateCompiler *compiler, AgateTokenKind expected, const char *message) {
  agateParserAdvance(compiler->parser);

  if (compiler->parser->previous.kind != expected) {
    agateError(compiler, "%s", message);

    // maybe the current one is the good one
    if (compiler->parser->current.kind == expected) {
      agateParserAdvance(compiler->parser);
    }
  }
}

static inline void agateCompilerMaybeNewline(AgateCompiler *compiler) {
  while (agateCompilerMatch(compiler, AGATE_TOKEN_EOL)) {
    // nothing to do
  }
}

static inline bool agateCompilerMatchNewline(AgateCompiler *compiler) {
  bool result = agateCompilerMatch(compiler, AGATE_TOKEN_EOL);
  agateCompilerMaybeNewline(compiler);
  return result;
}

static inline void agateCompilerConsumeNewline(AgateCompiler *compiler, const char *message) {
  agateCompilerConsume(compiler, AGATE_TOKEN_EOL, message);
  agateCompilerMaybeNewline(compiler);
}


static inline bool agateCompilerMatchAny(AgateCompiler *compiler, const AgateTokenKind *tokens, ptrdiff_t count) {
  for (ptrdiff_t i = 0; i < count; ++i) {
    if (agateCompilerMatch(compiler, tokens[i])) {
      return true;
    }
  }

  return false;
}

/*
 * parser - bytecode
 */

static inline AgateBytecode *agateCurrentBytecode(AgateCompiler *compiler) {
  return &compiler->function->bc;
}

static inline ptrdiff_t agateCurrentBytecodeOffset(AgateCompiler *compiler) {
  return compiler->function->bc.code.size;
}

static inline void agateWriteByte(AgateCompiler *compiler, uint8_t byte) {
  AgateBytecode *bc = agateCurrentBytecode(compiler);
  agateBytecodeWrite(bc, byte, compiler->parser->previous.line, compiler->parser->vm);
}

static inline ptrdiff_t agateEmitByte(AgateCompiler *compiler, uint8_t byte) {
  agateWriteByte(compiler, byte);
  return agateCurrentBytecodeOffset(compiler) - 1;
}

static inline void agateEmitOpcode(AgateCompiler *compiler, AgateOpCode instruction) {
  static const ptrdiff_t stack_effects[] = {
    #define X(name, stack, bytes) stack,
    AGATE_OPCODE_LIST
    #undef X
  };

  agateEmitByte(compiler, instruction);

  compiler->slot_count += stack_effects[instruction];

  if (compiler->slot_count > compiler->function->slot_count) {
    compiler->function->slot_count = compiler->slot_count;
  }
}

static inline ptrdiff_t agateEmitShort(AgateCompiler *compiler, uint16_t bytes) {
  agateWriteByte(compiler, (uint8_t) ((bytes >> 8) & 0xFF));
  agateWriteByte(compiler, (uint8_t) ( bytes       & 0xFF));
  return agateCurrentBytecodeOffset(compiler) - 2;
}

static inline ptrdiff_t agateEmitByteArg(AgateCompiler *compiler, AgateOpCode instruction, uint8_t arg) {
  agateEmitOpcode(compiler, instruction);
  return agateEmitByte(compiler, arg);
}

static inline ptrdiff_t agateEmitShortArg(AgateCompiler *compiler, AgateOpCode instruction, uint16_t arg) {
  agateEmitOpcode(compiler, instruction);
  return agateEmitShort(compiler, arg);
}

static inline ptrdiff_t agateEmitJumpForward(AgateCompiler *compiler, AgateOpCode instruction) {
  return agateEmitShortArg(compiler, instruction, 0xFFFF);
}

static inline void agatePatchJump(AgateCompiler *compiler, ptrdiff_t offset) {
  AgateBytecode *bc = agateCurrentBytecode(compiler);
  ptrdiff_t jump = bc->code.size - offset - 2;
  assert(jump >= 0);

  if (jump > UINT16_MAX) {
    agateError(compiler, "Too much code to jump over.");
  }

  bc->code.data[offset] = (uint8_t) ((jump >> 8) & 0xFF);
  bc->code.data[offset + 1] = (uint8_t) (jump & 0xFF);
}

static inline void agateEmitJumpBackward(AgateCompiler *compiler, ptrdiff_t start) {
  AgateBytecode *bc = agateCurrentBytecode(compiler);
  ptrdiff_t offset = bc->code.size - start + 2;
  assert(offset > 0);

  if (offset > UINT16_MAX) {
    agateError(compiler, "Loop body too large.");
  }

  agateEmitShortArg(compiler, AGATE_OP_JUMP_BACKWARD, (uint16_t) offset);
}

static inline void agateEmitConstant(AgateCompiler *compiler, AgateValue value) {
  ptrdiff_t symbol = agateCompilerAddConstant(compiler, value);
  agateEmitShortArg(compiler, AGATE_OP_CONSTANT, symbol);
}

static inline void agateEmitLiteral(AgateCompiler *compiler) {
  agateEmitConstant(compiler, compiler->parser->previous.value);
}

static inline void agateEmitLoopJump(AgateCompiler *compiler) {
  assert(compiler->loop);
  compiler->loop->exit_jump = agateEmitJumpForward(compiler, AGATE_OP_JUMP_IF);
}

static inline void agateEmitLoopRestart(AgateCompiler *compiler) {
  assert(compiler->loop);
  agateEmitJumpBackward(compiler, compiler->loop->start);
}

/*
 * parser - scopes
 */

static ptrdiff_t agateCompilerAddLocal(AgateCompiler *compiler, const char *name, ptrdiff_t size) {
  assert(compiler->locals_count < AGATE_MAX_LOCALS);
  AgateLocal *local = &compiler->locals[compiler->locals_count];
  local->name = name;
  local->size = size;
  local->depth = compiler->scope_depth;
  local->is_captured = false;
  return compiler->locals_count++;
}

static ptrdiff_t agateCompilerDeclareVariable(AgateCompiler *compiler, AgateToken *token) {
  if (token == NULL) {
    token = &compiler->parser->previous;
  }

  if (token->size > AGATE_MAX_VARIABLE_NAME_SIZE) {
    agateError(compiler, "Variable name cannot be longer than %d characters.", AGATE_MAX_VARIABLE_NAME_SIZE);
  }

  if (compiler->scope_depth == -1) {
    ptrdiff_t symbol = agateUnitAddVariable(compiler->parser->vm, compiler->parser->unit, token->start, token->size, agateNilValue());

    switch (symbol) {
      case AGATE_DEFINITION_ALREADY_DEFINED:
        agateError(compiler, "Unit variable is already defined.");
        break;
      case AGATE_DEFINITION_TOO_MANY_DEFINITIONS:
        agateError(compiler, "Too many unit variables defined.");
        break;
      default:
        break;
    }

    return symbol;
  }

  for (ptrdiff_t i = compiler->locals_count - 1; i >= 0; --i) {
    AgateLocal *local = &compiler->locals[i];

    if (local->depth < compiler->scope_depth) {
      break;
    }

    if (local->size == token->size && memcmp(local->name, token->start, token->size) == 0) {
      agateError(compiler, "Variable is already declared in this scope.");
      return i;
    }
  }

  if (compiler->locals_count == AGATE_MAX_LOCALS) {
    agateError(compiler, "Cannot declare more than %d variables in one scope.", AGATE_MAX_LOCALS);
    return -1;
  }

  return agateCompilerAddLocal(compiler, token->start, token->size);
}

static ptrdiff_t agateCompilerDeclareNamedVariable(AgateCompiler *compiler) {
  agateCompilerConsume(compiler, AGATE_TOKEN_IDENTIFIER, "Expect variable name.");
  return agateCompilerDeclareVariable(compiler, NULL);
}

static void agateCompilerDefineVariable(AgateCompiler *compiler, ptrdiff_t symbol) {
  if (compiler->scope_depth >= 0) {
    return;
  }

  agateEmitShortArg(compiler, AGATE_OP_GLOBAL_STORE, symbol);
  agateEmitOpcode(compiler, AGATE_OP_POP);
}

static void agateCompilerPushScope(AgateCompiler *compiler) {
  ++compiler->scope_depth;
}

static ptrdiff_t agateCompilerDiscardLocals(AgateCompiler *compiler, ptrdiff_t depth) {
  assert(compiler->scope_depth > -1);

  ptrdiff_t local = compiler->locals_count - 1;

  while (local >= 0 && compiler->locals[local].depth >= depth) {
    if (compiler->locals[local].is_captured) {
      agateEmitOpcode(compiler, AGATE_OP_CLOSE_UPVALUE);
    } else {
      agateEmitOpcode(compiler, AGATE_OP_POP);
    }

    --local;
  }

  return compiler->locals_count - local - 1;
}

static void agateCompilerPopScope(AgateCompiler *compiler) {
  ptrdiff_t popped = agateCompilerDiscardLocals(compiler, compiler->scope_depth);
  compiler->locals_count -= popped;
  compiler->slot_count -= popped;
  --compiler->scope_depth;
}

static ptrdiff_t agateCompilerResolveLocal(AgateCompiler *compiler, const char *name, ptrdiff_t size) {
  for (ptrdiff_t i = compiler->locals_count - 1; i >= 0; --i) {
    if (compiler->locals[i].size == size && memcmp(compiler->locals[i].name, name, size) == 0) {
      return i;
    }
  }

  return -1;
}

static ptrdiff_t agateCompilerAddUpvalue(AgateCompiler *compiler, AgateCapture capture, ptrdiff_t index) {
  ptrdiff_t count = compiler->function->upvalue_count;

  for (ptrdiff_t i = 0; i < count; ++i) {
    AgateCompilerUpvalue *upvalue = &compiler->upvalues[i];

    if (upvalue->capture == capture && upvalue->index == index) {
      return i;
    }
  }

  assert(count < AGATE_MAX_UPVALUES);
  compiler->upvalues[count].capture = capture;
  compiler->upvalues[count].index = index;
  return compiler->function->upvalue_count++;
}

static ptrdiff_t agateCompilerFindUpvalue(AgateCompiler *compiler, const char *name, ptrdiff_t size) {
  if (compiler->parent == NULL) {
    return -1;
  }

  ptrdiff_t local = agateCompilerResolveLocal(compiler->parent, name, size);

  if (local != -1) {
    compiler->parent->locals[local].is_captured = true;
    return agateCompilerAddUpvalue(compiler, AGATE_CAPTURE_LOCAL, local);
  }

  ptrdiff_t upvalue = agateCompilerFindUpvalue(compiler->parent, name, size);

  if (upvalue != -1) {
    return agateCompilerAddUpvalue(compiler, AGATE_CAPTURE_UPVALUE, upvalue);
  }

  return -1;
}

static AgateVariable agateCompilerResolveNonGlobalName(AgateCompiler *compiler, const char *name, ptrdiff_t size) {
  AgateVariable variable;

  variable.scope = AGATE_SCOPE_LOCAL;
  variable.index = agateCompilerResolveLocal(compiler, name, size);

  if (variable.index != -1) {
    return variable;
  }

  variable.scope = AGATE_SCOPE_UPVALUE;
  variable.index = agateCompilerFindUpvalue(compiler, name, size);
  return variable;
}

static AgateVariable agateCompilerResolveName(AgateCompiler *compiler, const char *name, ptrdiff_t size) {
  AgateVariable variable = agateCompilerResolveNonGlobalName(compiler, name, size);

  if (variable.index != -1) {
    return variable;
  }

  variable.scope = AGATE_SCOPE_GLOBAL;
  variable.index = agateSymbolTableFind(&compiler->parser->unit->object_names, name, size);
  return variable;
}

static AgateFunction *agateCompilerEnd(AgateCompiler *compiler, const char *name, ptrdiff_t size) {
  agateTableDestroy(&compiler->constants, compiler->parser->vm);

  if (compiler->parser->has_error) {
    compiler->parser->vm->compiler = compiler->parent;
    return NULL;
  }

  agateEmitOpcode(compiler, AGATE_OP_END);
  AgateFunction *function = compiler->function;
  agateFunctionBindName(compiler->parser->vm, function, name, size);


#ifdef AGATE_DEBUG_PRINT_CODE
  agateDisassemble(compiler->parser->vm, function);
#endif // AGATE_DEBUG_PRINT_CODE

  if (compiler->parent != NULL) {
    ptrdiff_t symbol = agateCompilerAddConstant(compiler->parent, agateEntityValue(function));
    agateEmitShortArg(compiler->parent, AGATE_OP_CLOSURE, symbol);

    for (int i = 0; i < function->upvalue_count; ++i) {
      agateEmitByte(compiler->parent, compiler->upvalues[i].capture);
      agateEmitByte(compiler->parent, compiler->upvalues[i].index);
    }
  }

  compiler->parser->vm->compiler = compiler->parent;
  return function;
}

static AgateCompiler *agateGetEnclosingClassCompiler(AgateCompiler *compiler) {
  while (compiler != NULL) {
    if (compiler->enclosing_class != NULL) {
      return compiler;
    }

    compiler = compiler->parent;
  }

  return NULL;
}

static AgateClassContext *agateGetEnclosingClass(AgateCompiler *compiler) {
  compiler = agateGetEnclosingClassCompiler(compiler);
  return compiler == NULL ? NULL : compiler->enclosing_class;
}


static void agateExpression(AgateCompiler *compiler);
static void agateDeclaration(AgateCompiler *compiler);

/*
 * parser - signature
 */

static AgateBlockKind agateFinishBlock(AgateCompiler *compiler) {
  if (agateCompilerMatch(compiler, AGATE_TOKEN_RIGHT_BRACE)) {
    return AGATE_BLOCK_MULTIPLE_LINE;
  }

  if (!agateCompilerMatchNewline(compiler)) {
    agateExpression(compiler);
    agateCompilerConsume(compiler, AGATE_TOKEN_RIGHT_BRACE, "Expect '}' after single line block.");
    return AGATE_BLOCK_SINGLE_LINE;
  }

  if (agateCompilerMatch(compiler, AGATE_TOKEN_RIGHT_BRACE)) {
    return AGATE_BLOCK_MULTIPLE_LINE;
  }

  do {
    agateDeclaration(compiler);
  } while (!agateCompilerCheck(compiler, AGATE_TOKEN_RIGHT_BRACE) && !agateCompilerCheck(compiler, AGATE_TOKEN_EOF));

  agateCompilerConsume(compiler, AGATE_TOKEN_RIGHT_BRACE, "Expect '}' after multiple line block.");
  return AGATE_BLOCK_MULTIPLE_LINE;
}

static void agateFinishScopedBlock(AgateCompiler *compiler) {
  agateCompilerPushScope(compiler);
  AgateBlockKind block = agateFinishBlock(compiler);

  if (block == AGATE_BLOCK_SINGLE_LINE) {
    agateEmitOpcode(compiler, AGATE_OP_POP);
  }

  agateCompilerPopScope(compiler);
}

static void agateFinishBody(AgateCompiler *compiler) {
  AgateBlockKind block = agateFinishBlock(compiler);

  if (compiler->is_initializer) {
    if (block == AGATE_BLOCK_SINGLE_LINE) {
      agateEmitOpcode(compiler, AGATE_OP_POP);
    }

    agateEmitByteArg(compiler, AGATE_OP_LOCAL_LOAD, 0);
  } else if (block == AGATE_BLOCK_MULTIPLE_LINE) {
    agateEmitOpcode(compiler, AGATE_OP_NIL);
  }

  agateEmitOpcode(compiler, AGATE_OP_RETURN);
}

static inline void agateValidateParametersCount(AgateCompiler *compiler, int argc) {
  if (argc == AGATE_MAX_PARAMETERS + 1) {
    agateError(compiler, "Functions and methods can not have more than %d parameters.", AGATE_MAX_PARAMETERS);
  }
}

static inline void agateFinishParameterList(AgateCompiler *compiler, int *arity) {
  int local_arity = 0;

  do {
    agateValidateParametersCount(compiler, ++local_arity);
    agateCompilerDeclareNamedVariable(compiler);
  } while (agateCompilerMatch(compiler, AGATE_TOKEN_COMMA));

  *arity = local_arity;
}

static inline ptrdiff_t agateCompilerMethodSymbol(AgateCompiler *compiler, const char *name, ptrdiff_t size) {
  return agateSymbolTableEnsure(&compiler->parser->vm->method_names, name, size, compiler->parser->vm);
}

static inline void agateSignatureAppend(AgateSignatureBuilder *builder, const char *name, ptrdiff_t size) {
  assert(builder->size + size < AGATE_MAX_METHOD_SIGNATURE_SIZE);
  memcpy(builder->name + builder->size, name, size);
  builder->size += size;
}

static inline void agateSignatureAppendChar(AgateSignatureBuilder *builder, char c) {
  assert(builder->size + 1 < AGATE_MAX_METHOD_SIGNATURE_SIZE);
  builder->name[builder->size++] = c;
}

static void agateSignatureParameterList(AgateSignatureBuilder *builder, int argc, char left_bracket, char right_bracket) {
  agateSignatureAppendChar(builder, left_bracket);

  for (int i = 0; i < argc && i < AGATE_MAX_PARAMETERS; ++i) {
    if (i > 0) {
      agateSignatureAppendChar(builder, ',');
    }

    agateSignatureAppendChar(builder, '_');
  }

  agateSignatureAppendChar(builder, right_bracket);
}

static void agateSignatureToString(AgateSignature *signature, AgateSignatureBuilder *builder) {
  builder->size = 0;
  agateSignatureAppend(builder, signature->name, signature->size);

  switch (signature->kind) {
    case AGATE_SIG_METHOD:
      agateSignatureParameterList(builder, signature->arity, '(', ')');
      break;

    case AGATE_SIG_GETTER:
      break;

    case AGATE_SIG_SETTER:
      agateSignatureAppendChar(builder, '=');
      agateSignatureParameterList(builder, 1, '(', ')');
      break;

    case AGATE_SIG_SUBSCRIPT_GETTER:
      agateSignatureParameterList(builder, signature->arity, '[', ']');
      break;

    case AGATE_SIG_SUBSCRIPT_SETTER:
      agateSignatureParameterList(builder, signature->arity - 1, '[', ']');
      agateSignatureAppendChar(builder, '=');
      agateSignatureParameterList(builder, 1, '(', ')');
      break;

    case AGATE_SIG_CONSTRUCTOR:
      builder->size = 0;
      agateSignatureAppend(builder, "init ", 5);
      agateSignatureAppend(builder, signature->name, signature->size);
      agateSignatureParameterList(builder, signature->arity, '(', ')');
      break;
  }

  builder->name[builder->size] = '\0';
}

static ptrdiff_t agateSignatureSymbol(AgateCompiler *compiler, AgateSignature *signature) {
  AgateSignatureBuilder builder;
  agateSignatureToString(signature, &builder);
  return agateCompilerMethodSymbol(compiler, builder.name, builder.size);
}

static AgateSignature agateSignatureFromToken(AgateCompiler *compiler, AgateSignatureKind kind) {
  AgateToken *token = &compiler->parser->previous;

  AgateSignature signature;
  signature.name = token->start;
  signature.size = token->size;
  signature.kind = kind;
  signature.arity = 0;

  if (signature.size > AGATE_MAX_METHOD_NAME_SIZE) {
    agateError(compiler, "Method names cannot be longer than %d characters.", AGATE_MAX_METHOD_NAME_SIZE);
    signature.size = AGATE_MAX_METHOD_NAME_SIZE;
  }

  return signature;
}

static inline void agateFinishArgumentList(AgateCompiler *compiler, int *argc) {
  int local_argc = *argc;

  do {
    agateValidateParametersCount(compiler, ++local_argc);
    agateExpression(compiler);
  } while (agateCompilerMatch(compiler, AGATE_TOKEN_COMMA));

  *argc = local_argc;
}

static void agateCompilerCallSignature(AgateCompiler *compiler, AgateOpCode instruction, AgateSignature *signature) {
  ptrdiff_t symbol = agateSignatureSymbol(compiler, signature);
  agateEmitByteArg(compiler, instruction, signature->arity);
  agateEmitShort(compiler, symbol);

  if (instruction == AGATE_OP_SUPER) {
    agateEmitShort(compiler, agateCompilerAddConstant(compiler, agateNilValue()));
  }
}

static void agateEmitInvoke(AgateCompiler *compiler, int argc, const char *name, ptrdiff_t size) {
  ptrdiff_t symbol = agateCompilerMethodSymbol(compiler, name, size);
  agateEmitByteArg(compiler, AGATE_OP_INVOKE, argc);
  agateEmitShort(compiler, symbol);
}

static void agateCompilerMethodCall(AgateCompiler *compiler, AgateOpCode instruction, AgateSignature *signature) {
  AgateSignature called = { signature->name, signature->size, AGATE_SIG_GETTER, 0 };

  if (agateCompilerMatch(compiler, AGATE_TOKEN_LEFT_PAREN)) {
    called.kind = AGATE_SIG_METHOD;

    if (!agateCompilerCheck(compiler, AGATE_TOKEN_RIGHT_PAREN)) {
      agateFinishArgumentList(compiler, &called.arity);
    }

    agateCompilerConsume(compiler, AGATE_TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
  }

  if (agateCompilerMatch(compiler, AGATE_TOKEN_LEFT_BRACE)) {
    called.kind = AGATE_SIG_METHOD;
    ++called.arity;

    AgateCompiler lambda_compiler;
    agateCompilerCreate(&lambda_compiler, compiler->parser, compiler, false);

    AgateSignature lambda_signature = { "", 0, AGATE_SIG_METHOD, 0 };

    if (agateCompilerMatch(compiler, AGATE_TOKEN_BAR)) {
      agateFinishParameterList(&lambda_compiler, &lambda_signature.arity);
      agateCompilerConsume(compiler, AGATE_TOKEN_BAR, "Expect '|' after lambda parameters.");
    }

    lambda_compiler.function->arity = lambda_signature.arity;
    agateFinishBody(&lambda_compiler);

    AgateSignatureBuilder builder;
    agateSignatureToString(&called, &builder);
    agateSignatureAppend(&builder, " block argument", 16);
    agateCompilerEnd(&lambda_compiler, builder.name, builder.size);
  }

  if (signature->kind == AGATE_SIG_CONSTRUCTOR) {
    if (called.kind != AGATE_SIG_METHOD) {
      agateError(compiler, "A superclass constructor must have an argument list.");
    }

    called.kind = AGATE_SIG_CONSTRUCTOR;
  }

  agateCompilerCallSignature(compiler, instruction, &called);
}

static void agateCompilerNamedCall(AgateCompiler *compiler, bool can_assign, AgateOpCode instruction) {
  AgateSignature signature = agateSignatureFromToken(compiler, AGATE_SIG_GETTER);

  if (can_assign && agateCompilerMatch(compiler, AGATE_TOKEN_EQUAL)) {
    signature.kind = AGATE_SIG_SETTER;
    signature.arity = 1;

    agateExpression(compiler);
    agateCompilerCallSignature(compiler, instruction, &signature);
  } else {
    agateCompilerMethodCall(compiler, instruction, &signature);
  }
}

static void agateEmitLoadVariable(AgateCompiler *compiler, AgateVariable variable) {
  switch (variable.scope) {
    case AGATE_SCOPE_LOCAL:
      agateEmitByteArg(compiler, AGATE_OP_LOCAL_LOAD, variable.index);
      break;
    case AGATE_SCOPE_UPVALUE:
      agateEmitByteArg(compiler, AGATE_OP_UPVALUE_LOAD, variable.index);
      break;
    case AGATE_SCOPE_GLOBAL:
      agateEmitShortArg(compiler, AGATE_OP_GLOBAL_LOAD, variable.index);
      break;
  }
}

static void agateInfixSignature(AgateCompiler *compiler, AgateSignature *signature) {
  signature->kind = AGATE_SIG_METHOD;
  signature->arity = 1;

  agateCompilerConsume(compiler, AGATE_TOKEN_LEFT_PAREN, "Expect '(' after operator name.");
  agateCompilerDeclareNamedVariable(compiler);
  agateCompilerConsume(compiler, AGATE_TOKEN_RIGHT_PAREN, "Expect ')' after parameter name.");
}

static void agateUnarySignature(AgateCompiler *compiler, AgateSignature *signature) {
  signature->kind = AGATE_SIG_GETTER;
}

static void agateMixedSignature(AgateCompiler *compiler, AgateSignature *signature) {
  signature->kind = AGATE_SIG_GETTER;

  if (agateCompilerMatch(compiler, AGATE_TOKEN_LEFT_PAREN)) {
    signature->kind = AGATE_SIG_METHOD;
    signature->arity = 1;

    agateCompilerDeclareNamedVariable(compiler);
    agateCompilerConsume(compiler, AGATE_TOKEN_RIGHT_PAREN, "Expect ')' after parameter name.");
  }
}

static bool agateMaybeSetter(AgateCompiler *compiler, AgateSignature *signature) {
  if (!agateCompilerMatch(compiler, AGATE_TOKEN_EQUAL)) {
    return false;
  }

  switch (signature->kind) {
    case AGATE_SIG_GETTER:
      signature->kind = AGATE_SIG_SETTER;
      break;
    case AGATE_SIG_SUBSCRIPT_GETTER:
      signature->kind = AGATE_SIG_SUBSCRIPT_SETTER;
      break;
    default:
      assert(false);
      break;
  }

  agateCompilerConsume(compiler, AGATE_TOKEN_LEFT_PAREN, "Expect '(' after '='.");
  agateCompilerDeclareNamedVariable(compiler);
  agateCompilerConsume(compiler, AGATE_TOKEN_RIGHT_PAREN, "Expect ')' after parameter name.");

  ++signature->arity;
  return true;
}

static void agateSubscriptSignature(AgateCompiler *compiler, AgateSignature *signature) {
  signature->kind = AGATE_SIG_SUBSCRIPT_GETTER;
  signature->size = 0;
  agateFinishParameterList(compiler, &signature->arity);
  agateCompilerConsume(compiler, AGATE_TOKEN_RIGHT_BRACKET, "Expect ']' after parameters.");
  agateMaybeSetter(compiler, signature);
}

static void agateParameterList(AgateCompiler *compiler, AgateSignature *signature) {
  if (!agateCompilerMatch(compiler, AGATE_TOKEN_LEFT_PAREN)) {
    return;
  }

  signature->kind = AGATE_SIG_METHOD;

  if (agateCompilerMatch(compiler, AGATE_TOKEN_RIGHT_PAREN)) {
    return;
  }

  agateFinishParameterList(compiler, &signature->arity);
  agateCompilerConsume(compiler, AGATE_TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
}

static void agateNameSignature(AgateCompiler *compiler, AgateSignature *signature) {
  signature->kind = AGATE_SIG_GETTER;

  if (agateMaybeSetter(compiler, signature)) {
    return;
  }

  agateParameterList(compiler, signature);
}

static void agateConstructorSignature(AgateCompiler *compiler, AgateSignature *signature) {
  agateCompilerConsume(compiler, AGATE_TOKEN_IDENTIFIER, "Expect constructor name after 'construct'.");
  *signature = agateSignatureFromToken(compiler, AGATE_SIG_CONSTRUCTOR);

  if (agateCompilerMatch(compiler, AGATE_TOKEN_EQUAL)) {
    agateError(compiler, "A constructor cannot be a setter.");
  }

  if (!agateCompilerMatch(compiler, AGATE_TOKEN_LEFT_PAREN)) {
    agateError(compiler, "A constructor cannot be a getter.");
    return;
  }

  if (agateCompilerMatch(compiler, AGATE_TOKEN_RIGHT_PAREN)) {
    return;
  }

  agateFinishParameterList(compiler, &signature->arity);
  agateCompilerConsume(compiler, AGATE_TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
}

static void agateEmitLoadThis(AgateCompiler *compiler) {
  agateEmitLoadVariable(compiler, agateCompilerResolveNonGlobalName(compiler, "this", 4));
}

static void agateEmitLoadCoreVariable(AgateCompiler *compiler, const char *name) {
  ptrdiff_t symbol = agateSymbolTableFind(&compiler->parser->unit->object_names, name, strlen(name));
  assert(symbol != -1);
  agateEmitShortArg(compiler, AGATE_OP_GLOBAL_LOAD, symbol);
}


/*
 * parser - loop
 */

static void agateLoopStart(AgateCompiler *compiler, AgateLoopContext *loop) {
  loop->start = agateCurrentBytecodeOffset(compiler) - 1;
  loop->exit_jump = -1;
  loop->breaks_count = 0;
  loop->scope_depth = compiler->scope_depth;
  loop->enclosing = compiler->loop;

  compiler->loop = loop;
}

static void agateLoopBody(AgateCompiler *compiler) {
  agateCompilerConsume(compiler, AGATE_TOKEN_LEFT_BRACE, "Expect '{' before loop body.");
  agateFinishScopedBlock(compiler);
}

static void agateLoopEnd(AgateCompiler *compiler) {
  if (compiler->loop->exit_jump != -1) {
    agatePatchJump(compiler, compiler->loop->exit_jump);
  }

  for (ptrdiff_t i = 0; i < compiler->loop->breaks_count; ++i) {
    agatePatchJump(compiler, compiler->loop->breaks[i]);
  }

  compiler->loop = compiler->loop->enclosing;
}



/*
 * parser - expression
 */

static void agateUnaryExpression(AgateCompiler *compiler, bool can_assign);

static void agateArrayExpression(AgateCompiler *compiler) {
  agateEmitLoadCoreVariable(compiler, "Array");
  agateEmitInvoke(compiler, 0, "new()", 5);

  do {
    agateCompilerMaybeNewline(compiler);

    if (agateCompilerCheck(compiler, AGATE_TOKEN_RIGHT_BRACKET)) {
      break;
    }

    agateExpression(compiler);
    agateEmitInvoke(compiler, 1, "__put(_)", 8);
  } while (agateCompilerMatch(compiler, AGATE_TOKEN_COMMA));

  agateCompilerMaybeNewline(compiler);
  agateCompilerConsume(compiler, AGATE_TOKEN_RIGHT_BRACKET, "Expect ']' after array elements.");
}

static void agateMapExpression(AgateCompiler *compiler) {
  agateEmitLoadCoreVariable(compiler, "Map");
  agateEmitInvoke(compiler, 0, "new()", 5);

  do {
    agateCompilerMaybeNewline(compiler);

    if (agateCompilerCheck(compiler, AGATE_TOKEN_RIGHT_BRACE)) {
      break;
    }

    agateUnaryExpression(compiler, false);
    agateCompilerConsume(compiler, AGATE_TOKEN_COLON, "Expect ':' after map key.");
    agateCompilerMaybeNewline(compiler);
    agateExpression(compiler);
    agateEmitInvoke(compiler, 2, "__put(_,_)", 10);
  } while (agateCompilerMatch(compiler, AGATE_TOKEN_COMMA));

  agateCompilerMaybeNewline(compiler);
  agateCompilerConsume(compiler, AGATE_TOKEN_RIGHT_BRACE, "Expect '}' after map members.");
}

static void agateInterpolationExpression(AgateCompiler *compiler) {
  agateEmitLoadCoreVariable(compiler, "Array");
  agateEmitInvoke(compiler, 0, "new()", 5);

  do {
    // add the string part
    agateEmitLiteral(compiler);
    agateEmitInvoke(compiler, 1, "__put(_)", 8);

    // add the expression part
    agateExpression(compiler);
    agateEmitInvoke(compiler, 1, "__put(_)", 8);

  } while (agateCompilerMatch(compiler, AGATE_TOKEN_INTERPOLATION));

  agateCompilerConsume(compiler, AGATE_TOKEN_STRING, "Expect end of string interpolation.");
  agateEmitLiteral(compiler);
  agateEmitInvoke(compiler, 1, "__put(_)", 8);

  agateEmitInvoke(compiler, 0, "join()", 6);
}

static void agateFieldExpression(AgateCompiler *compiler, bool can_assign) {
  ptrdiff_t field = AGATE_MAX_FIELDS;
  AgateClassContext *enclosing_class = agateGetEnclosingClass(compiler);

  if (enclosing_class == NULL) {
    agateError(compiler, "Cannot reference a field outside of a class definition.");
  } else if (enclosing_class->is_foreign) {
    agateError(compiler, "Cannot define fields in a foreign class.");
  } else if (enclosing_class->is_method_static) {
    agateError(compiler, "Cannot use an instance field in a static method.");
  } else {
    field = agateSymbolTableEnsure(&enclosing_class->fields, compiler->parser->previous.start, compiler->parser->previous.size, compiler->parser->vm);

    if (field >= AGATE_MAX_FIELDS) {
      agateError(compiler, "A class can only have %d fields.", AGATE_MAX_FIELDS);
    }
  }

  bool is_load = true;

  if (can_assign && agateCompilerMatch(compiler, AGATE_TOKEN_EQUAL)) {
    agateExpression(compiler);
    is_load = false;
  }

  if (compiler->parent != NULL && compiler->parent->enclosing_class == enclosing_class) {
    agateEmitByteArg(compiler, is_load ? AGATE_OP_FIELD_LOAD_THIS : AGATE_OP_FIELD_STORE_THIS, field);
  } else {
    agateEmitLoadThis(compiler);
    agateEmitByteArg(compiler, is_load ? AGATE_OP_FIELD_LOAD : AGATE_OP_FIELD_STORE, field);
  }
}

static void agateBareName(AgateCompiler *compiler, bool can_assign, AgateVariable variable) {
  if (can_assign && agateCompilerMatch(compiler, AGATE_TOKEN_EQUAL)) {
    agateExpression(compiler);

    switch (variable.scope) {
      case AGATE_SCOPE_LOCAL:
        agateEmitByteArg(compiler, AGATE_OP_LOCAL_STORE, variable.index);
        break;
      case AGATE_SCOPE_UPVALUE:
        agateEmitByteArg(compiler, AGATE_OP_UPVALUE_STORE, variable.index);
        break;
      case AGATE_SCOPE_GLOBAL:
        agateEmitShortArg(compiler, AGATE_OP_GLOBAL_STORE, variable.index);
        break;
    }
  } else {
    agateEmitLoadVariable(compiler, variable);
  }
}

static void agateStaticFieldExpression(AgateCompiler *compiler, bool can_assign) {
  AgateCompiler *class_compiler = agateGetEnclosingClassCompiler(compiler);

  if (class_compiler == NULL) {
    agateError(compiler, "Cannot use a static field outside of a class definition.");
    return;
  }

  AgateToken *token = &compiler->parser->previous;

  if (agateCompilerResolveLocal(class_compiler, token->start, token->size) == -1) {
    ptrdiff_t symbol = agateCompilerDeclareVariable(class_compiler, NULL);
    agateEmitOpcode(class_compiler, AGATE_OP_NIL);
    agateCompilerDefineVariable(class_compiler, symbol);
  }

  AgateVariable variable = agateCompilerResolveName(compiler, token->start, token->size);
  agateBareName(compiler, can_assign, variable);
}

static void agateIdentifierExpression(AgateCompiler *compiler, bool can_assign) {
  AgateToken *token = &compiler->parser->previous;
  AgateVariable variable = agateCompilerResolveName(compiler, token->start, token->size);

  if (variable.index == -1) {
    variable.scope = AGATE_SCOPE_GLOBAL;
    variable.index = agateUnitAddFutureVariable(compiler->parser->vm, compiler->parser->unit, token->start, token->size, token->line);

    if (variable.index == AGATE_DEFINITION_TOO_MANY_DEFINITIONS) {
      agateError(compiler, "Too many unit variables defined.");
    }
  }

  agateBareName(compiler, can_assign, variable);
}

static void agateThisMethodExpression(AgateCompiler *compiler, bool can_assign) {
  if (agateGetEnclosingClass(compiler) == NULL) {
    agateError(compiler, "Cannot call a direct method outside of a class.");
    return;
  }

  agateCompilerConsume(compiler, AGATE_TOKEN_IDENTIFIER, "Expect method name after '.'.");
  agateEmitLoadThis(compiler);
  agateCompilerNamedCall(compiler, can_assign, AGATE_OP_INVOKE);
}

static void agateThisExpression(AgateCompiler *compiler) {
  if (agateGetEnclosingClass(compiler) == NULL) {
    agateError(compiler, "Cannot use 'this' outside of a method.");
    return;
  }

  agateEmitLoadThis(compiler);
}

static void agateSuperExpression(AgateCompiler *compiler, bool can_assign) {
  AgateClassContext *enclosing_class = agateGetEnclosingClass(compiler);

  if (enclosing_class == NULL) {
    agateError(compiler, "Cannot use 'super' outside a method.");
  }

  agateEmitLoadThis(compiler);

  if (agateCompilerMatch(compiler, AGATE_TOKEN_DOT)) {
    agateCompilerConsume(compiler, AGATE_TOKEN_IDENTIFIER, "Expect method name after 'super.'.");
    agateCompilerNamedCall(compiler, can_assign, AGATE_OP_SUPER);
  } else if (enclosing_class != NULL) {
    agateCompilerMethodCall(compiler, AGATE_OP_SUPER, enclosing_class->signature);
  }
}

static void agatePrimaryExpression(AgateCompiler *compiler, bool can_assign) {
  if (agateCompilerMatch(compiler, AGATE_TOKEN_INTEGER)) {
    agateEmitLiteral(compiler);
  } else if (agateCompilerMatch(compiler, AGATE_TOKEN_FLOAT)) {
    agateEmitLiteral(compiler);
  } else if (agateCompilerMatch(compiler, AGATE_TOKEN_INTERPOLATION)) {
    agateInterpolationExpression(compiler);
  } else if (agateCompilerMatch(compiler, AGATE_TOKEN_STRING)) {
    agateEmitLiteral(compiler);
  } else if (agateCompilerMatch(compiler, AGATE_TOKEN_CHAR)) {
    agateEmitLiteral(compiler);
  } else if (agateCompilerMatch(compiler, AGATE_TOKEN_IDENTIFIER)) {
    agateIdentifierExpression(compiler, can_assign);
  } else if (agateCompilerMatch(compiler, AGATE_TOKEN_LEFT_PAREN)) {
    agateExpression(compiler);
    agateCompilerConsume(compiler, AGATE_TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
  } else if (agateCompilerMatch(compiler, AGATE_TOKEN_LEFT_BRACE)) {
    agateMapExpression(compiler);
  } else if (agateCompilerMatch(compiler, AGATE_TOKEN_LEFT_BRACKET)) {
    agateArrayExpression(compiler);
  } else if (agateCompilerMatch(compiler, AGATE_TOKEN_FIELD_INSTANCE)) {
    agateFieldExpression(compiler, can_assign);
  } else if (agateCompilerMatch(compiler, AGATE_TOKEN_FIELD_CLASS)) {
    agateStaticFieldExpression(compiler, can_assign);
  } else if (agateCompilerMatch(compiler, AGATE_TOKEN_DOT)) {
    agateThisMethodExpression(compiler, can_assign);
  } else if (agateCompilerMatch(compiler, AGATE_TOKEN_NIL)) {
    agateEmitOpcode(compiler, AGATE_OP_NIL);
  } else if (agateCompilerMatch(compiler, AGATE_TOKEN_TRUE)) {
    agateEmitOpcode(compiler, AGATE_OP_TRUE);
  } else if (agateCompilerMatch(compiler, AGATE_TOKEN_FALSE)) {
    agateEmitOpcode(compiler, AGATE_OP_FALSE);
  } else if (agateCompilerMatch(compiler, AGATE_TOKEN_THIS)) {
    agateThisExpression(compiler);
  } else if (agateCompilerMatch(compiler, AGATE_TOKEN_SUPER)) {
    agateSuperExpression(compiler, can_assign);
  } else {
    agateError(compiler, "Expected expression.");
  }
}

static void agateInvokeExpression(AgateCompiler *compiler, bool can_assign) {
  agateCompilerConsume(compiler, AGATE_TOKEN_IDENTIFIER, "Expect method name after '.'.");
  agateCompilerNamedCall(compiler, can_assign, AGATE_OP_INVOKE);
}

static void agateCallExpression(AgateCompiler *compiler, bool can_assign) {
  int argc = 0;

  if (!agateCompilerMatch(compiler, AGATE_TOKEN_RIGHT_PAREN)) {
    agateFinishArgumentList(compiler, &argc);
    agateCompilerConsume(compiler, AGATE_TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
  }

  if (agateCompilerMatch(compiler, AGATE_TOKEN_LEFT_BRACE)) {
    ++argc;

    AgateCompiler lambda_compiler;
    agateCompilerCreate(&lambda_compiler, compiler->parser, compiler, false);
    int arity = 0;

    if (agateCompilerMatch(&lambda_compiler, AGATE_TOKEN_BAR)) {
      agateFinishParameterList(&lambda_compiler, &arity);
      agateCompilerConsume(&lambda_compiler, AGATE_TOKEN_BAR, "Expect '|' after lambda parameters.");
    }

    lambda_compiler.function->arity = arity;
    agateFinishBody(&lambda_compiler);
    agateCompilerEnd(&lambda_compiler, "<lambda>", 8);
  }

  agateEmitByteArg(compiler, AGATE_OP_CALL, argc + 1);
}

static void agateSubscriptExpression(AgateCompiler *compiler, bool can_assign) {
  AgateSignature signature = { "", 0, AGATE_SIG_SUBSCRIPT_GETTER, 0 };
  agateFinishArgumentList(compiler, &signature.arity);
  agateCompilerConsume(compiler, AGATE_TOKEN_RIGHT_BRACKET, "Expect ']' after arguments.");

  if (can_assign && agateCompilerMatch(compiler, AGATE_TOKEN_EQUAL)) {
    signature.kind = AGATE_SIG_SUBSCRIPT_SETTER;
    agateValidateParametersCount(compiler, ++signature.arity);
    agateExpression(compiler);
  }

  agateCompilerCallSignature(compiler, AGATE_OP_INVOKE, &signature);
}

static void agatePostfixExpression(AgateCompiler *compiler, bool can_assign) {
  agatePrimaryExpression(compiler, can_assign);

  for (;;) {
    if (agateCompilerMatch(compiler, AGATE_TOKEN_DOT)) {
      agateInvokeExpression(compiler, can_assign);
    } else if (agateCompilerMatch(compiler, AGATE_TOKEN_LEFT_PAREN)) {
      agateCallExpression(compiler, can_assign);
    } else if (agateCompilerMatch(compiler, AGATE_TOKEN_LEFT_BRACKET)) {
      agateSubscriptExpression(compiler, can_assign);
    } else {
      break;
    }
  }
}

static void agateUnaryExpression(AgateCompiler *compiler, bool can_assign) {
  static const AgateTokenKind UnaryTokens[] = { AGATE_TOKEN_BANG, AGATE_TOKEN_MINUS, AGATE_TOKEN_PLUS, AGATE_TOKEN_TILDE };

  if (agateCompilerMatchAny(compiler, UnaryTokens, AGATE_ARRAY_SIZE(UnaryTokens))) {
    AgateTokenKind op = compiler->parser->previous.kind;
    agateUnaryExpression(compiler, false);

    switch (op) {
      case AGATE_TOKEN_BANG:
        agateEmitInvoke(compiler, 0, "!", 1);
        break;
      case AGATE_TOKEN_MINUS:
        agateEmitInvoke(compiler, 0, "-", 1);
        break;
      case AGATE_TOKEN_PLUS:
        agateEmitInvoke(compiler, 0, "+", 1);
        break;
      case AGATE_TOKEN_TILDE:
        agateEmitInvoke(compiler, 0, "~", 1);
        break;
      default:
        assert(false);
        break;
    }
  } else {
    agatePostfixExpression(compiler, can_assign);
  }
}

static void agateFactorExpression(AgateCompiler *compiler, bool can_assign) {
  static const AgateTokenKind FactorTokens[] = { AGATE_TOKEN_STAR, AGATE_TOKEN_SLASH, AGATE_TOKEN_PERCENT };

  agateUnaryExpression(compiler, can_assign);

  while (agateCompilerMatchAny(compiler, FactorTokens, AGATE_ARRAY_SIZE(FactorTokens))) {
    AgateTokenKind op = compiler->parser->previous.kind;
    agateUnaryExpression(compiler, false);

    switch (op) {
      case AGATE_TOKEN_STAR:
        agateEmitInvoke(compiler, 1, "*(_)", 4);
        break;
      case AGATE_TOKEN_SLASH:
        agateEmitInvoke(compiler, 1, "/(_)", 4);
        break;
      case AGATE_TOKEN_PERCENT:
        agateEmitInvoke(compiler, 1, "%(_)", 4);
        break;
      default:
        assert(false);
        break;
    }
  }
}

static void agateTermExpression(AgateCompiler *compiler, bool can_assign) {
  static const AgateTokenKind TermTokens[] = { AGATE_TOKEN_PLUS, AGATE_TOKEN_MINUS };

  agateFactorExpression(compiler, can_assign);

  while (agateCompilerMatchAny(compiler, TermTokens, AGATE_ARRAY_SIZE(TermTokens))) {
    AgateTokenKind op = compiler->parser->previous.kind;
    agateFactorExpression(compiler, false);

    switch (op) {
      case AGATE_TOKEN_PLUS:
        agateEmitInvoke(compiler, 1, "+(_)", 4);
        break;
      case AGATE_TOKEN_MINUS:
        agateEmitInvoke(compiler, 1, "-(_)", 4);
        break;
      default:
        assert(false);
        break;
    }
  }
}

static void agateRangeExpression(AgateCompiler *compiler, bool can_assign) {
  static const AgateTokenKind RangeTokens[] = { AGATE_TOKEN_DOT_DOT, AGATE_TOKEN_DOT_DOT_DOT };

  agateTermExpression(compiler, can_assign);

  while (agateCompilerMatchAny(compiler, RangeTokens, AGATE_ARRAY_SIZE(RangeTokens))) {
    AgateTokenKind op = compiler->parser->previous.kind;
    agateTermExpression(compiler, false);

    switch (op) {
      case AGATE_TOKEN_DOT_DOT:
        agateEmitInvoke(compiler, 1, "..(_)", 5);
        break;
      case AGATE_TOKEN_DOT_DOT_DOT:
        agateEmitInvoke(compiler, 1, "...(_)", 6);
        break;
      default:
        assert(false);
        break;
    }
  }
}

static void agateShiftExpression(AgateCompiler *compiler, bool can_assign) {
  static const AgateTokenKind ShiftTokens[] = { AGATE_TOKEN_LESS_LESS, AGATE_TOKEN_GREATER_GREATER, AGATE_TOKEN_GREATER_GREATER_GREATER };

  agateRangeExpression(compiler, can_assign);

  while (agateCompilerMatchAny(compiler, ShiftTokens, AGATE_ARRAY_SIZE(ShiftTokens))) {
    AgateTokenKind op = compiler->parser->previous.kind;
    agateRangeExpression(compiler, false);

    switch (op) {
      case AGATE_TOKEN_LESS_LESS:
        agateEmitInvoke(compiler, 1, "<<(_)", 5);
        break;
      case AGATE_TOKEN_GREATER_GREATER:
        agateEmitInvoke(compiler, 1, ">>(_)", 5);
        break;
      case AGATE_TOKEN_GREATER_GREATER_GREATER:
        agateEmitInvoke(compiler, 1, ">>>(_)", 6);
        break;
      default:
        assert(false);
        break;
    }
  }
}

static void agateBitwiseAndExpression(AgateCompiler *compiler, bool can_assign) {
  agateShiftExpression(compiler, can_assign);

  while (agateCompilerMatch(compiler, AGATE_TOKEN_AMP)) {
    agateShiftExpression(compiler, false);
    agateEmitInvoke(compiler, 1, "&(_)", 4);
  }
}

static void agateBitwiseXorExpression(AgateCompiler *compiler, bool can_assign) {
  agateBitwiseAndExpression(compiler, can_assign);

  while (agateCompilerMatch(compiler, AGATE_TOKEN_CARET)) {
    agateBitwiseAndExpression(compiler, false);
    agateEmitInvoke(compiler, 1, "^(_)", 4);
  }
}

static void agateBitwiseOrExpression(AgateCompiler *compiler, bool can_assign) {
  agateBitwiseXorExpression(compiler, can_assign);

  while (agateCompilerMatch(compiler, AGATE_TOKEN_BAR)) {
    agateBitwiseXorExpression(compiler, false);
    agateEmitInvoke(compiler, 1, "|(_)", 4);
  }
}

static void agateComparisonExpression(AgateCompiler *compiler, bool can_assign) {
  static const AgateTokenKind ComparisonTokens[] = { AGATE_TOKEN_LESS, AGATE_TOKEN_GREATER, AGATE_TOKEN_LESS_EQUAL, AGATE_TOKEN_GREATER_EQUAL };

  agateBitwiseOrExpression(compiler, can_assign);

  while (agateCompilerMatchAny(compiler, ComparisonTokens, AGATE_ARRAY_SIZE(ComparisonTokens))) {
    AgateTokenKind op = compiler->parser->previous.kind;
    agateBitwiseOrExpression(compiler, false);

    switch (op) {
      case AGATE_TOKEN_LESS:
        agateEmitInvoke(compiler, 1, "<(_)", 4);
        break;
      case AGATE_TOKEN_GREATER:
        agateEmitInvoke(compiler, 1, ">(_)", 4);
        break;
      case AGATE_TOKEN_LESS_EQUAL:
        agateEmitInvoke(compiler, 1, "<=(_)", 5);
        break;
      case AGATE_TOKEN_GREATER_EQUAL:
        agateEmitInvoke(compiler, 1, ">=(_)", 5);
        break;
      default:
        assert(false);
        break;
    }
  }
}

static void agateIsExpression(AgateCompiler *compiler, bool can_assign) {
  agateComparisonExpression(compiler, can_assign);

  while (agateCompilerMatch(compiler, AGATE_TOKEN_IS)) {
    agateComparisonExpression(compiler, false);
    agateEmitInvoke(compiler, 1, "is(_)", 5);
  }
}

static void agateEqualityExpression(AgateCompiler *compiler, bool can_assign) {
  static const AgateTokenKind EqualityTokens[] = { AGATE_TOKEN_EQUAL_EQUAL, AGATE_TOKEN_BANG_EQUAL };

  agateIsExpression(compiler, can_assign);

  while (agateCompilerMatchAny(compiler, EqualityTokens, AGATE_ARRAY_SIZE(EqualityTokens))) {
    AgateTokenKind op = compiler->parser->previous.kind;
    agateIsExpression(compiler, false);

    switch (op) {
      case AGATE_TOKEN_EQUAL_EQUAL:
        agateEmitInvoke(compiler, 1, "==(_)", 5);
        break;
      case AGATE_TOKEN_BANG_EQUAL:
        agateEmitInvoke(compiler, 1, "!=(_)", 5);
        break;
      default:
        assert(false);
        break;
    }
  }
}

static void agateLogicalAndExpression(AgateCompiler *compiler, bool can_assign) {
  agateEqualityExpression(compiler, can_assign);

  while (agateCompilerMatch(compiler, AGATE_TOKEN_AMP_AMP)) {
    ptrdiff_t jump = agateEmitJumpForward(compiler, AGATE_OP_AND);
    agateEqualityExpression(compiler, false);
    agatePatchJump(compiler, jump);
  }
}

static void agateLogicalOrExpression(AgateCompiler *compiler, bool can_assign) {
  agateLogicalAndExpression(compiler, can_assign);

  while (agateCompilerMatch(compiler, AGATE_TOKEN_BAR_BAR)) {
    ptrdiff_t jump = agateEmitJumpForward(compiler, AGATE_OP_OR);
    agateLogicalAndExpression(compiler, false);
    agatePatchJump(compiler, jump);
  }
}

static void agateConditionalExpression(AgateCompiler *compiler, bool can_assign) {
  agateLogicalOrExpression(compiler, can_assign);

  if (agateCompilerMatch(compiler, AGATE_TOKEN_QUESTION)) {
    ptrdiff_t if_jump = agateEmitJumpForward(compiler, AGATE_OP_JUMP_IF);
    agateExpression(compiler);
    agateCompilerConsume(compiler, AGATE_TOKEN_COLON, "Expect ':' in conditional expression.");

    ptrdiff_t else_jump = agateEmitJumpForward(compiler, AGATE_OP_JUMP_FORWARD);
    agatePatchJump(compiler, if_jump);
    agateConditionalExpression(compiler, false);
    agatePatchJump(compiler, else_jump);
  }
}

static void agateAssignmentExpression(AgateCompiler *compiler, bool can_assign) {
  agateConditionalExpression(compiler, can_assign);

  if (agateCompilerMatch(compiler, AGATE_TOKEN_EQUAL)) {
    agateError(compiler, "Unexpected assignment.");
  }
}

static void agateExpression(AgateCompiler *compiler) {
  agateAssignmentExpression(compiler, true);
}

/*
 * parser - statement
 */

static void agateAssertStatement(AgateCompiler *compiler) {
  AgateAssertHandling assert_handling = compiler->parser->vm->config.assert_handling;

  AgateBytecode *bc = agateCurrentBytecode(compiler);
  ptrdiff_t current_code = bc->code.size;
  ptrdiff_t current_lines = bc->lines.size;
  ptrdiff_t current_constant = bc->constants.size;

  agateCompilerConsume(compiler, AGATE_TOKEN_LEFT_PAREN, "Expect '(' after 'assert'.");
  agateExpression(compiler);
  agateCompilerConsume(compiler, AGATE_TOKEN_COMMA, "Expect ',' after condition.");

  ptrdiff_t assert_jump = -1;

  switch (assert_handling) {
    case AGATE_ASSERT_ABORT:
      agateEmitInvoke(compiler, 0, "!", 1);
      assert_jump = agateEmitJumpForward(compiler, AGATE_OP_JUMP_IF);
      agateEmitLoadCoreVariable(compiler, "System");
      break;
    case AGATE_ASSERT_NIL:
      agateEmitInvoke(compiler, 0, "!", 1);
      assert_jump = agateEmitJumpForward(compiler, AGATE_OP_JUMP_IF);
      break;
    case AGATE_ASSERT_NONE:
      break;
  }

  agateExpression(compiler);
  agateCompilerConsume(compiler, AGATE_TOKEN_RIGHT_PAREN, "Expect ')' after reason.");

  switch (assert_handling) {
    case AGATE_ASSERT_ABORT:
      agateEmitInvoke(compiler, 1, "abort(_)", 8);
      agatePatchJump(compiler, assert_jump);
      break;
    case AGATE_ASSERT_NIL:
      agateEmitOpcode(compiler, AGATE_OP_POP);
      agateEmitOpcode(compiler, AGATE_OP_NIL);
      agateEmitOpcode(compiler, AGATE_OP_RETURN);
      agatePatchJump(compiler, assert_jump);
      break;
    case AGATE_ASSERT_NONE:
      bc->code.size = current_code;
      bc->lines.size = current_lines;
      bc->constants.size = current_constant;
      break;
  }
}

static void agateIfStatement(AgateCompiler *compiler) {
  agateCompilerConsume(compiler, AGATE_TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
  agateExpression(compiler);
  agateCompilerConsume(compiler, AGATE_TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  ptrdiff_t then_jump = agateEmitJumpForward(compiler, AGATE_OP_JUMP_IF);
  agateCompilerConsume(compiler, AGATE_TOKEN_LEFT_BRACE, "Expect '{' before 'if' body.");
  agateFinishScopedBlock(compiler);

  if (agateCompilerMatch(compiler, AGATE_TOKEN_ELSE)) {
    ptrdiff_t else_jump = agateEmitJumpForward(compiler, AGATE_OP_JUMP_FORWARD);
    agatePatchJump(compiler, then_jump);

    if (agateCompilerMatch(compiler, AGATE_TOKEN_IF)) {
      agateIfStatement(compiler);
    } else if (agateCompilerMatch(compiler, AGATE_TOKEN_LEFT_BRACE)) {
      agateFinishScopedBlock(compiler);
    } else {
      agateError(compiler, "Expect 'if' or '{' after 'else'.");
    }

    agatePatchJump(compiler, else_jump);
  } else {
    agatePatchJump(compiler, then_jump);
  }
}

static void agateForStatement(AgateCompiler *compiler) {
  agateCompilerPushScope(compiler);

  agateCompilerConsume(compiler, AGATE_TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
  agateCompilerConsume(compiler, AGATE_TOKEN_IDENTIFIER, "Expect for loop variable name.");

  const char *name = compiler->parser->previous.start;
  ptrdiff_t size = compiler->parser->previous.size;

  agateCompilerConsume(compiler, AGATE_TOKEN_IN, "Expect 'in' after loop variable.");
  agateExpression(compiler);

  if (compiler->locals_count + 2 > AGATE_MAX_LOCALS) {
    agateError(compiler, "Cannot declare more than %d variables in one scope. (Not enough space for for-loops internal variables)");
    return;
  }

  ptrdiff_t sequence_symbol = agateCompilerAddLocal(compiler, "$sequence", 9);

  agateEmitOpcode(compiler, AGATE_OP_NIL);
  ptrdiff_t iterator_symbol = agateCompilerAddLocal(compiler, "$iterator", 9);

  agateCompilerConsume(compiler, AGATE_TOKEN_RIGHT_PAREN,"Expect ')' after loop expression.");

  AgateLoopContext loop;
  agateLoopStart(compiler, &loop);

  agateEmitByteArg(compiler, AGATE_OP_LOCAL_LOAD, sequence_symbol);
  agateEmitByteArg(compiler, AGATE_OP_LOCAL_LOAD, iterator_symbol);
  agateEmitInvoke(compiler, 1, "iterate(_)", 10);
  agateEmitByteArg(compiler, AGATE_OP_LOCAL_STORE, iterator_symbol);
  agateEmitLoopJump(compiler);

  agateEmitByteArg(compiler, AGATE_OP_LOCAL_LOAD, sequence_symbol);
  agateEmitByteArg(compiler, AGATE_OP_LOCAL_LOAD, iterator_symbol);
  agateEmitInvoke(compiler, 1, "iterator_value(_)", 17);

  agateCompilerPushScope(compiler);
  agateCompilerAddLocal(compiler, name, size);

  agateLoopBody(compiler);

  agateCompilerPopScope(compiler);

  agateEmitLoopRestart(compiler);
  agateLoopEnd(compiler);

  agateCompilerPopScope(compiler);
}

static void agateWhileStatement(AgateCompiler *compiler) {
  AgateLoopContext loop;
  agateLoopStart(compiler, &loop);

  agateCompilerConsume(compiler, AGATE_TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
  agateExpression(compiler);
  agateCompilerConsume(compiler, AGATE_TOKEN_RIGHT_PAREN, "Expect ')' after while condition.");

  agateEmitLoopJump(compiler);
  agateLoopBody(compiler);
  agateEmitLoopRestart(compiler);
  agateLoopEnd(compiler);
}

static void agateLoopStatement(AgateCompiler *compiler) {
  AgateLoopContext loop;
  agateLoopStart(compiler, &loop);
  agateLoopBody(compiler);
  agateEmitLoopRestart(compiler);
  agateLoopEnd(compiler);
}

static void agateContinueStatement(AgateCompiler *compiler) {
  if (compiler->loop == NULL) {
    agateError(compiler, "Cannot use 'continue' outside of a loop.");
  } else {
    agateCompilerDiscardLocals(compiler, compiler->loop->scope_depth + 1);
    agateEmitLoopRestart(compiler);
  }
}

static void agateBreakStatement(AgateCompiler *compiler) {
  if (compiler->loop == NULL) {
    agateError(compiler, "Cannot use 'break' outside of a loop.");
  } else {
    agateCompilerDiscardLocals(compiler, compiler->loop->scope_depth + 1);
    ptrdiff_t jump = agateEmitJumpForward(compiler, AGATE_OP_JUMP_FORWARD);

    if (compiler->loop->breaks_count == AGATE_MAX_BREAKS) {
      agateError(compiler, "Too many 'break'.");
    } else {
      compiler->loop->breaks[compiler->loop->breaks_count++] = jump;
    }
  }
}

static void agateReturnStatement(AgateCompiler *compiler) {
  if (agateCompilerCheck(compiler, AGATE_TOKEN_EOL)) {
    if (compiler->is_initializer) {
      agateEmitByteArg(compiler, AGATE_OP_LOCAL_LOAD, 0);
    } else {
      agateEmitOpcode(compiler, AGATE_OP_NIL);
    }
  } else {
    if (compiler->is_initializer) {
      agateError(compiler, "A constructor cannot return a value.");
    }

    agateExpression(compiler);
  }

  agateEmitOpcode(compiler, AGATE_OP_RETURN);
}

static void agateOnceStatement(AgateCompiler *compiler) {
  AgateLoopContext loop;
  agateLoopStart(compiler, &loop);
  agateLoopBody(compiler);
  agateLoopEnd(compiler);
}

static void agateExpressionStatement(AgateCompiler *compiler) {
  agateExpression(compiler);
  agateEmitOpcode(compiler, AGATE_OP_POP);
}

static void agateStatement(AgateCompiler *compiler) {
  if (agateCompilerMatch(compiler, AGATE_TOKEN_ASSERT)) {
    agateAssertStatement(compiler);
  } else if (agateCompilerMatch(compiler, AGATE_TOKEN_IF)) {
    agateIfStatement(compiler);
  } else if (agateCompilerMatch(compiler, AGATE_TOKEN_FOR)) {
    agateForStatement(compiler);
  } else if (agateCompilerMatch(compiler, AGATE_TOKEN_WHILE)) {
    agateWhileStatement(compiler);
  } else if (agateCompilerMatch(compiler, AGATE_TOKEN_LOOP)) {
    agateLoopStatement(compiler);
  } else if (agateCompilerMatch(compiler, AGATE_TOKEN_CONTINUE)) {
    agateContinueStatement(compiler);
  } else if (agateCompilerMatch(compiler, AGATE_TOKEN_BREAK)) {
    agateBreakStatement(compiler);
  } else if (agateCompilerMatch(compiler, AGATE_TOKEN_RETURN)) {
    agateReturnStatement(compiler);
  } else if (agateCompilerMatch(compiler, AGATE_TOKEN_ONCE)) {
    agateOnceStatement(compiler);
  } else {
    agateExpressionStatement(compiler);
  }
}

/*
 * parser - declaration
 */

static void agateCreateConstructor(AgateCompiler *compiler, AgateSignature *signature, ptrdiff_t constructor_symbol) {
  AgateCompiler method_compiler;
  agateCompilerCreate(&method_compiler, compiler->parser, compiler, true);

  agateEmitOpcode(&method_compiler, compiler->enclosing_class->is_foreign ? AGATE_OP_CONSTRUCT_FOREIGN : AGATE_OP_CONSTRUCT);

  agateEmitByteArg(&method_compiler, AGATE_OP_INVOKE, signature->arity);
  agateEmitShort(&method_compiler, constructor_symbol);

  agateEmitOpcode(&method_compiler, AGATE_OP_RETURN);
  agateCompilerEnd(&method_compiler, "", 0);
}

static void agateDefineMethod(AgateCompiler *compiler, AgateVariable class_variable, bool is_static, ptrdiff_t method_symbol) {
  agateEmitLoadVariable(compiler, class_variable);
  AgateOpCode instruction = is_static ? AGATE_OP_METHOD_CLASS : AGATE_OP_METHOD_INSTANCE;
  agateEmitShortArg(compiler, instruction, method_symbol);
}

static ptrdiff_t agateDeclareMethod(AgateCompiler *compiler, AgateSignature *signature, const char *name, ptrdiff_t size) {
  ptrdiff_t symbol = agateSignatureSymbol(compiler, signature);

  AgateClassContext *context = compiler->enclosing_class;
  AgateSymbolArray *methods = context->is_method_static ? &context->class_methods : &context->instance_methods;

  for (ptrdiff_t i = 0; i < methods->size; ++i) {
    if (methods->data[i] == symbol) {
      if (context->is_method_static) {
        agateError(compiler, "Class %s already defines a static method '%.*s'.", &compiler->enclosing_class->name->data, (int) size, name);
      } else {
        agateError(compiler, "Class %s already defines a method '%.*s'.", &compiler->enclosing_class->name->data, (int) size, name);
      }

      return symbol;
    }
  }

  agateSymbolArrayAppend(methods, symbol, compiler->parser->vm);
  return symbol;
}

static bool agateMethod(AgateCompiler *compiler, AgateVariable class_variable) {
  bool is_foreign = agateCompilerMatch(compiler, AGATE_TOKEN_FOREIGN);
  bool is_static = agateCompilerMatch(compiler, AGATE_TOKEN_STATIC);

  compiler->enclosing_class->is_method_static = is_static;

  AgateTokenKind method_token = compiler->parser->current.kind;
  agateParserAdvance(compiler->parser);

  AgateSignature signature = agateSignatureFromToken(compiler, AGATE_SIG_GETTER);
  compiler->enclosing_class->signature = &signature;

  AgateCompiler method_compiler;
  agateCompilerCreate(&method_compiler, compiler->parser, compiler, true);

  switch (method_token) {
    case AGATE_TOKEN_AMP:
    case AGATE_TOKEN_BAR:
    case AGATE_TOKEN_BANG_EQUAL:
    case AGATE_TOKEN_CARET:
    case AGATE_TOKEN_DOT_DOT:
    case AGATE_TOKEN_DOT_DOT_DOT:
    case AGATE_TOKEN_EQUAL_EQUAL:
    case AGATE_TOKEN_GREATER:
    case AGATE_TOKEN_GREATER_EQUAL:
    case AGATE_TOKEN_GREATER_GREATER:
    case AGATE_TOKEN_GREATER_GREATER_GREATER:
    case AGATE_TOKEN_IS:
    case AGATE_TOKEN_LESS:
    case AGATE_TOKEN_LESS_EQUAL:
    case AGATE_TOKEN_LESS_LESS:
    case AGATE_TOKEN_PERCENT:
    case AGATE_TOKEN_SLASH:
    case AGATE_TOKEN_STAR:
      agateInfixSignature(&method_compiler, &signature);
      break;
    case AGATE_TOKEN_BANG:
    case AGATE_TOKEN_TILDE:
      agateUnarySignature(&method_compiler, &signature);
      break;
    case AGATE_TOKEN_MINUS:
    case AGATE_TOKEN_PLUS:
      agateMixedSignature(&method_compiler, &signature);
      break;
    case AGATE_TOKEN_IDENTIFIER:
      agateNameSignature(&method_compiler, &signature);
      break;
    case AGATE_TOKEN_CONSTRUCT:
      agateConstructorSignature(&method_compiler, &signature);
      break;
    case AGATE_TOKEN_LEFT_BRACKET:
      agateSubscriptSignature(&method_compiler, &signature);
      break;
    default:
      agateError(compiler, "Expect method definition.");
      method_compiler.parser->vm->compiler = method_compiler.parent;
      return false;
  }

  method_compiler.is_initializer = (signature.kind == AGATE_SIG_CONSTRUCTOR);

  if (is_static && signature.kind == AGATE_SIG_CONSTRUCTOR) {
    agateError(compiler, "A constructor cannot be static.");
  }

  AgateSignatureBuilder builder;
  agateSignatureToString(&signature, &builder);

  ptrdiff_t method_symbol = agateDeclareMethod(compiler, &signature, builder.name, builder.size);

  if (is_foreign) {
    agateEmitConstant(compiler, agateEntityValue(agateStringNew(compiler->parser->vm, builder.name, builder.size)));
    method_compiler.parser->vm->compiler = method_compiler.parent;
  } else {
    agateCompilerConsume(compiler, AGATE_TOKEN_LEFT_BRACE, "Expect '{' to begin method body.");
    agateFinishBody(&method_compiler);
    agateCompilerEnd(&method_compiler, builder.name, builder.size);
  }

  agateDefineMethod(compiler, class_variable, is_static, method_symbol);

  if (signature.kind == AGATE_SIG_CONSTRUCTOR) {
    signature.kind = AGATE_SIG_METHOD;
    ptrdiff_t constructor_symbol = agateSignatureSymbol(compiler, &signature);

    agateCreateConstructor(compiler, &signature, method_symbol);
    agateDefineMethod(compiler, class_variable, true, constructor_symbol);
  }

  return true;
}

static void agateClassDeclaration(AgateCompiler *compiler, bool is_foreign) {
  AgateVariable variable;
  variable.scope = compiler->scope_depth == -1 ? AGATE_SCOPE_GLOBAL : AGATE_SCOPE_LOCAL;
  variable.index = agateCompilerDeclareNamedVariable(compiler);

  AgateString *class_name = agateStringNew(compiler->parser->vm, compiler->parser->previous.start, compiler->parser->previous.size);
  AgateValue class_name_value = agateEntityValue(class_name);
  agateEmitConstant(compiler, class_name_value);

  if (agateCompilerMatch(compiler, AGATE_TOKEN_IS)) {
    agatePrimaryExpression(compiler, false);
  } else {
    agateEmitLoadCoreVariable(compiler, "Object");
  }

  ptrdiff_t field_count_instruction = -1;

  if (is_foreign) {
    agateEmitOpcode(compiler, AGATE_OP_CLASS_FOREIGN);
  } else {
    field_count_instruction = agateEmitByteArg(compiler, AGATE_OP_CLASS, 0xFF);
  }

  agateCompilerDefineVariable(compiler, variable.index);

  agateCompilerPushScope(compiler);

  AgateClassContext enclosing_class;
  enclosing_class.is_foreign = is_foreign;
  enclosing_class.name = class_name;
  agateTableCreate(&enclosing_class.fields);
  agateSymbolArrayCreate(&enclosing_class.instance_methods);
  agateSymbolArrayCreate(&enclosing_class.class_methods);
  compiler->enclosing_class = &enclosing_class;

  agateCompilerConsume(compiler, AGATE_TOKEN_LEFT_BRACE,  "Expect '{' before class body.");

  if (!agateCompilerMatch(compiler, AGATE_TOKEN_RIGHT_BRACE)) {
    agateCompilerConsumeNewline(compiler, "Expect newline after '{'.");

    while (!agateCompilerCheck(compiler, AGATE_TOKEN_RIGHT_BRACE) && !agateCompilerCheck(compiler, AGATE_TOKEN_EOF)) {
      agateMethod(compiler, variable);
      agateCompilerConsumeNewline(compiler, "Expect newline after method definition.");
    }

    agateCompilerConsume(compiler, AGATE_TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
  }

  if (!is_foreign) {
    compiler->function->bc.code.data[field_count_instruction] = (uint8_t) enclosing_class.fields.size;
  }

  agateSymbolArrayDestroy(&enclosing_class.class_methods, compiler->parser->vm);
  agateSymbolArrayDestroy(&enclosing_class.instance_methods, compiler->parser->vm);
  agateTableDestroy(&enclosing_class.fields, compiler->parser->vm);
  compiler->enclosing_class = NULL;

  agateCompilerPopScope(compiler);
}

static void agateVariableOrFunctionDeclaration(AgateCompiler *compiler) {
  agateCompilerConsume(compiler, AGATE_TOKEN_IDENTIFIER, "Expect function or variable name.");
  AgateToken name = compiler->parser->previous;
  ptrdiff_t symbol = -1;

  if (agateCompilerMatch(compiler, AGATE_TOKEN_LEFT_PAREN)) {
    // function
    symbol = agateCompilerDeclareVariable(compiler, &name);

    AgateCompiler function_compiler;
    agateCompilerCreate(&function_compiler, compiler->parser, compiler, false);
    int argc = 0;

    if (!agateCompilerMatch(&function_compiler, AGATE_TOKEN_RIGHT_PAREN)) {
      agateFinishParameterList(&function_compiler, &argc);
      agateCompilerConsume(&function_compiler, AGATE_TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    }

    function_compiler.function->arity = argc;

    agateCompilerConsume(&function_compiler, AGATE_TOKEN_LEFT_BRACE, "Expect '{' to begin function body.");
    agateFinishBody(&function_compiler);
    agateCompilerEnd(&function_compiler, name.start, name.size);
  } else {
    // variable
    if (agateCompilerMatch(compiler, AGATE_TOKEN_EQUAL)) {
      agateExpression(compiler);
    } else {
      agateEmitOpcode(compiler, AGATE_OP_NIL);
    }

    symbol = agateCompilerDeclareVariable(compiler, &name);
  }

  agateCompilerDefineVariable(compiler, symbol);
}

static void agateImport(AgateCompiler *compiler) {
  agateCompilerConsume(compiler, AGATE_TOKEN_STRING, "Expect a string after 'import'.");
  ptrdiff_t unit_constant = agateCompilerAddConstant(compiler, compiler->parser->previous.value);

  agateEmitShortArg(compiler, AGATE_OP_IMPORT_UNIT, unit_constant);
  agateEmitOpcode(compiler, AGATE_OP_POP);

  if (!agateCompilerMatch(compiler, AGATE_TOKEN_FOR)) {
    return;
  }

  do {
    agateCompilerConsume(compiler, AGATE_TOKEN_IDENTIFIER, "Expect variable name.");

    AgateToken source = compiler->parser->previous;
    ptrdiff_t source_constant = agateCompilerAddConstant(compiler, agateEntityValue(agateStringNew(compiler->parser->vm, source.start, source.size)));

    ptrdiff_t symbol;

    if (agateCompilerMatch(compiler, AGATE_TOKEN_AS)) {
      symbol = agateCompilerDeclareNamedVariable(compiler);
    } else {
      symbol = agateCompilerDeclareVariable(compiler, &source);
    }

    agateEmitShortArg(compiler, AGATE_OP_IMPORT_OBJECT, source_constant);
    agateCompilerDefineVariable(compiler, symbol);
  } while (agateCompilerMatch(compiler, AGATE_TOKEN_COMMA));
}

static void agateParserSynchronize(AgateParser *parser) {
  while (parser->current.kind != AGATE_TOKEN_EOF) {
    switch (parser->current.kind) {
      // start of declaration
      case AGATE_TOKEN_CLASS:
      case AGATE_TOKEN_DEF:
      case AGATE_TOKEN_IMPORT:
      // start of statement
      case AGATE_TOKEN_FOR:
      case AGATE_TOKEN_IF:
      case AGATE_TOKEN_LOOP:
      case AGATE_TOKEN_ONCE:
      case AGATE_TOKEN_RETURN:
      case AGATE_TOKEN_WHILE:
        return;

      default:
        break;
    }

    agateParserAdvance(parser);
  }
}

static void agateDeclaration(AgateCompiler *compiler) {
  if (agateCompilerMatch(compiler, AGATE_TOKEN_CLASS)) {
    agateClassDeclaration(compiler, false);
  } else if (agateCompilerMatch(compiler, AGATE_TOKEN_DEF)) {
    agateVariableOrFunctionDeclaration(compiler);
  } else if (agateCompilerMatch(compiler, AGATE_TOKEN_IMPORT)) {
    agateImport(compiler);
  } else if (agateCompilerMatch(compiler, AGATE_TOKEN_FOREIGN)) {
    agateCompilerConsume(compiler, AGATE_TOKEN_CLASS, "Expect 'class' after 'foreign'.");
    agateClassDeclaration(compiler, true);
  } else {
    agateStatement(compiler);
  }

  if (compiler->parser->has_error) {
    agateParserSynchronize(compiler->parser);
  } else {
    agateCompilerConsumeNewline(compiler, "Expect newline after declaration.");
  }
}

static void agateUnit(AgateCompiler *compiler) {
  agateCompilerMaybeNewline(compiler);

  while (!agateCompilerMatch(compiler, AGATE_TOKEN_EOF)) {
    agateDeclaration(compiler);
  }

  agateEmitOpcode(compiler, AGATE_OP_END_UNIT);
  agateEmitOpcode(compiler, AGATE_OP_RETURN);
}

static AgateFunction *agateRawCompile(AgateVM *vm, AgateUnit *unit, const char *source) { // # ~wrenCompile
  AgateParser parser;

  parser.vm = vm;
  parser.unit = unit;

  parser.source_start = source;
  parser.source_end = source + strlen(source);

  parser.token_start = source;
  parser.token_current = source;
  parser.line = 1;
  parser.interpolation_count = 0;

  AgateToken init = { AGATE_TOKEN_EOF, source, 0, -1, agateNilValue() };
  parser.current = init;
  parser.previous = init;

  parser.has_error = false;

  ptrdiff_t existing_object_count = unit->object_values.size;

  AgateCompiler compiler;
  agateCompilerCreate(&compiler, &parser, NULL, false);
  agateParserAdvance(&parser);

  agateUnit(&compiler);

  for (ptrdiff_t i = existing_object_count; i < unit->object_values.size; ++i) {
    if (agateIsInt(unit->object_values.data[i])) {
      AgateString *name = agateSymbolTableReverseFind(&unit->object_names, i);

      parser.previous.kind = AGATE_TOKEN_IDENTIFIER;
      parser.previous.start = name->data;
      parser.previous.size = name->size;
      parser.previous.line = agateAsInt(unit->object_values.data[i]);
      agateError(&compiler, "Variable '%s' is used but not defined.", name->data);
    }
  }


  return agateCompilerEnd(&compiler, "(script)", 8);
}

// parser - gc

static void agateMarkCompilerRoots(AgateVM *vm, AgateCompiler *compiler) {
  agateMarkValue(vm, compiler->parser->current.value);
  agateMarkValue(vm, compiler->parser->previous.value);

  do {
    agateMarkEntity(vm, (AgateEntity *) compiler->function);
    agateMarkTable(vm, & compiler->constants);

    if (compiler->enclosing_class != NULL) {
      agateMarkTable(vm, &compiler->enclosing_class->fields);
    }

    compiler = compiler->parent;

  } while (compiler != NULL);
}
