// SPDX-License-Identifier: MIT
// Copyright (c) 2022 Julien Bernard
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <poll.h>

#include "tests.inc"

#include "agate.h"
#include "agate-support.h"

#include "tests/api/api_tests.h"

#include "agate-lexer.h"
#include "agate-parser.h"

#define C_OK "\033[32m"
#define C_KO "\033[31m"
#define C_OUT "\033[34m"
#define C_STOP "\033[m"

static const char *agateStatusString(AgateStatus status) {
  switch (status) {
    case AGATE_STATUS_OK:
      return "AGATE_STATUS_OK";
    case AGATE_STATUS_COMPILE_ERROR:
      return "AGATE_STATUS_COMPILE_ERROR";
    case AGATE_STATUS_RUNTIME_ERROR:
      return "AGATE_STATUS_RUNTIME_ERROR";
    default:
      break;
  }

  return "AGATE_STATUS_???";
}


/*
 * AgateTestBuffer
 */

typedef struct {
  char *data;
  size_t capacity;
  size_t size;
} AgateTestBuffer;

static void agateTestBufferCreate(AgateTestBuffer *self) {
  self->data = NULL;
  self->capacity = 0;
  self->size = 0;
}

static void agateTestBufferDestroy(AgateTestBuffer *self) {
  free(self->data);
  self->data = NULL;
  self->capacity = 0;
  self->size = 0;
}

static void agateTestBufferReset(AgateTestBuffer *self) {
  self->size = 0;
  memset(self->data, 0, self->capacity);
}

static void agateTestBufferAppend(AgateTestBuffer *self, const char *buffer, size_t size) {
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

    char *data = realloc(self->data, self->capacity);
    assert(data);
    self->data = data;
  }

  memcpy(self->data + self->size, buffer, size);

  self->size += size;
  self->data[self->size] = '\0';
}

/*
 * AgateTest
 */

typedef enum {
  AGATE_MODE_TEST,
  AGATE_MODE_SINGLE,
} AgateTestMode;

typedef struct {
  AgateTestBuffer print;
  AgateStatus status;
} AgateOutcome;

typedef struct {
  const char *path;
  FILE *file;
  AgateTestBuffer content;

  AgateOutcome syntax;

  AgateOutcome expected;
  AgateOutcome actual;
} AgateTest;

static void agateTestCreate(AgateTest *self) {
  agateTestBufferCreate(&self->content);
  agateTestBufferCreate(&self->syntax.print);
  agateTestBufferCreate(&self->expected.print);
  agateTestBufferCreate(&self->actual.print);
}

static void agateTestDestroy(AgateTest *self) {
  agateTestBufferDestroy(&self->actual.print);
  agateTestBufferDestroy(&self->expected.print);
  agateTestBufferDestroy(&self->syntax.print);
  agateTestBufferDestroy(&self->content);
}

static void agateTestReset(AgateTest *self) {
  self->path = NULL;
  self->file = NULL;
  agateTestBufferReset(&self->content);
  agateTestBufferReset(&self->syntax.print);
  agateTestBufferReset(&self->expected.print);
  agateTestBufferReset(&self->actual.print);
}

#define BUFFER_SIZE 1024
#define STATUS_OK_STRING              "# result: ok"
#define STATUS_COMPILE_ERROR_STRING   "# result: compile error"
#define STATUS_RUNTIME_ERROR_STRING   "# result: runtime error"
#define EXPECT_STRING                 "# expect: "

static bool agateTestReadFile(AgateTest *self) {
  self->expected.status = AGATE_STATUS_OK;

  char buffer[BUFFER_SIZE];

  while (fgets(buffer, BUFFER_SIZE, self->file) != NULL) {
    size_t size = strlen(buffer);

    agateTestBufferAppend(&self->content, buffer, size);

    char *where = strstr(buffer, EXPECT_STRING);

    if (where != NULL) {
      where += sizeof(EXPECT_STRING) - 1; // -1 for '\0'
      agateTestBufferAppend(&self->expected.print, where, size - (where - buffer)); // this includes the final '\n'
    }

    if (strstr(buffer, STATUS_OK_STRING) != NULL) {
      self->expected.status = AGATE_STATUS_OK;
    } else if (strstr(buffer, STATUS_COMPILE_ERROR_STRING) != NULL) {
      self->expected.status = AGATE_STATUS_COMPILE_ERROR;
    } else if (strstr(buffer, STATUS_RUNTIME_ERROR_STRING) != NULL) {
      self->expected.status = AGATE_STATUS_RUNTIME_ERROR;
    }

  }

  rewind(self->file);
  return true;
}

typedef AgateStatus (*AgateTestFunc)(AgateTest *self);

static bool agateTestExecFile(AgateTest *self, AgateTestFunc func, AgateOutcome *outcome, AgateTestMode mode) {
  if (mode == AGATE_MODE_SINGLE) {
    outcome->status = func(self);
    return true;
  }

  int ret;

  int out[2];
  ret = pipe(out);
  assert(ret != -1);

  pid_t pid = fork();
  assert(pid != -1);

  if (pid == 0) {
    // child

    // redirect stdout in the pipe
    ret = dup2(out[1], 1);
    assert(ret != -1);

    // close the useless side of the pipe
    ret = close(out[0]);
    assert(ret != -1);

    // redirect stderr on stdout
    ret = dup2(1, 2);
    assert(ret != -1);

    // remove the buffer for stdout
    setbuf(stdout, NULL);

    // call function
    AgateStatus status = func(self);

    // do not leak memory in this process
    fclose(self->file);
    agateTestDestroy(self);

    // close the pipe and exit
    close(out[1]);
    exit(status);
  }

  ret = close(out[1]);
  assert(ret != -1);

  char buffer[BUFFER_SIZE];

  for (;;) {
    ssize_t size = read(out[0], buffer, BUFFER_SIZE);
    assert(size != -1);

    if (size == 0) {
      ret = close(out[0]);
      assert(ret != -1);
      break;
    }

    agateTestBufferAppend(&outcome->print, buffer, size);
  }

  int status;
  pid_t proc = waitpid(pid, &status, 0);
  assert(proc != -1);
  assert(proc == pid);

  if (WIFEXITED(status)) {
    outcome->status = (AgateStatus) WEXITSTATUS(status);
  } else {
    outcome->status = -1;
  }

  return true;
}


static AgateStatus agateTestRunSyntax(AgateTest *self) {
  yyin = self->file;
  int result = yyparse();
  yylex_destroy();
  return result == 0 ? AGATE_STATUS_OK : AGATE_STATUS_COMPILE_ERROR;
}

static bool agateTestCheckSyntax(AgateTest *self, AgateTestMode mode) {
  if (!agateTestExecFile(self, agateTestRunSyntax, &self->syntax, mode)) {
    return false;
  }

  if (mode == AGATE_MODE_SINGLE) {
    return true;
  }

  const char *syntax_string = self->syntax.print.data ? self->syntax.print.data : "";
  assert(syntax_string);

  if (self->syntax.status == AGATE_STATUS_COMPILE_ERROR) {
    if (self->expected.status != AGATE_STATUS_COMPILE_ERROR) {
      printf("\tExpected no compile time error and got one\n");
      printf(C_OUT "%s" C_STOP "\n", syntax_string);
      return false;
    }
  }

  return true;
}

static void agateTestPrint(AgateVM *vm, const char* text) {
  fputs(text, stdout);
}

static void agateTestWrite(AgateVM *vm, uint8_t byte) {
  fputc(byte, stdout);
}

static void agateTestError(AgateVM *vm, AgateErrorKind kind, const char *unit_name, int line, const char *message) {
  switch (kind) {
    case AGATE_ERROR_COMPILE:
      printf("test:%d: %s\n", line, message);
      break;
    case AGATE_ERROR_RUNTIME:
      printf("%s\n", message);
      break;
    case AGATE_ERROR_STACKTRACE:
      break;
  }
}

static AgateStatus agateTestRunInterpreter(AgateTest *self) {
  // run the file with the interpreter
  AgateConfig config;
  agateConfigInitialize(&config);

  config.unit_handler = agateExUnitHandler;

  config.assert_handling = AGATE_ASSERT_ABORT;

  config.print = agateTestPrint;
  config.write = agateTestWrite;
  config.error = agateTestError;

  config.user_data = self;

  bool use_foreign = agateTestUseForeign(self->path);

  if (use_foreign) {
    config.foreign_class_handler = agateTestForeignClassHandler;
    config.foreign_method_handler = agateTestForeignMethodHandler;
  }

  AgateVM *vm = agateExNewVM(&config);
  agateExUnitAddIncludePath(vm, "tests/language/unit");
  agateExUnitAddIncludePath(vm, "tests/docs/units");
  AgateStatus status = agateCallString(vm, self->path, self->content.data);

  if (use_foreign) {
    agateTestRunNative(vm, self->path);
  }

  agateExDeleteVM(vm);

  return status;
}

static bool agateTestCheckRuntime(AgateTest *self, AgateTestMode mode) {
  if (!agateTestExecFile(self, agateTestRunInterpreter, &self->actual, mode)) {
    return false;
  }

  if (mode == AGATE_MODE_SINGLE) {
    return true;
  }

  const char *expected_string = self->expected.print.data ? self->expected.print.data : "";
  const char *actual_string = self->actual.print.data ? self->actual.print.data : "";

  if (self->expected.status != self->actual.status) {
    printf("\tResult mismatch. Expected: %s. Actual: %s.\n", agateStatusString(self->expected.status), agateStatusString(self->actual.status));
    printf(C_OUT "%s" C_STOP "\n", actual_string);
    return false;
  }

  if (strcmp(expected_string, actual_string) != 0) {
    printf("\tExpected:\n" C_OUT "%s" C_STOP "\n\tActual:\n" C_OUT "%s" C_STOP "\n", expected_string, actual_string);
    return false;
  }

  return true;
}

static bool agateTestRunFile(AgateTest *self, const char *path, AgateTestMode mode) {
  agateTestReset(self);

  self->path = path;
  self->file = fopen(path, "rb");

  if (self->file == NULL) {
    printf("\tMissing file: %s\n", path);
    return false;
  }

  bool result = true;

  result = result && agateTestReadFile(self);
  result = result && agateTestCheckSyntax(self, mode);
  result = result && agateTestCheckRuntime(self, mode);

  fclose(self->file);
  return result;
}

/*
 *
 */

int main(int argc, char *argv[]) {
  const char **test_files = AgateTestFiles;
  AgateTestMode mode = AGATE_MODE_TEST;
  bool force_test= false;

  if (argc > 1) {
    if (strcmp(argv[1], "--single") == 0) {
      --argc;
      ++argv;
      mode = AGATE_MODE_SINGLE;
    } else if (strcmp(argv[1], "--test") == 0) {
      --argc;
      ++argv;
      mode = AGATE_MODE_TEST;
      force_test = true;
    }
  }

  if (argc > 1) {
    test_files = (const char **) (argv + 1);

    if (argc == 2 && !force_test) {
      mode = AGATE_MODE_SINGLE;
    }
  }

  size_t i = 0;
  int success_count = 0;
  int failure_count = 0;

  AgateTest test;
  agateTestCreate(&test);

  while (test_files[i] != NULL) {
    const char *path = test_files[i];
    size_t no = i + 1;

    printf("[%3zu] %s\n", no, path);
    fflush(stdout);

    if (agateTestRunFile(&test, path, mode)) {
      printf(C_OK "[%3zu] %s OK" C_STOP "\n", no, path);
      ++success_count;
    } else {
      printf(C_KO "[%3zu] %s KO" C_STOP "\n", no, path);
      ++failure_count;
    }

    ++i;
  }

  agateTestDestroy(&test);

  printf("======\n");

  if (failure_count == 0) {
    printf("All " C_OK "%i" C_STOP " tests passed.\n", success_count);
  } else {
    printf(C_OK "%i" C_STOP " tests passed. " C_KO "%i" C_STOP " tests failed.\n", success_count, failure_count);
  }

  return EXIT_SUCCESS;
}
