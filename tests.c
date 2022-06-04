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

// useful declaration from generated files
extern FILE *yyin;
int yyparse(void);
int yylex_destroy(void);

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
}

static void agateTestBufferReset(AgateTestBuffer *self) {
  self->size = 0;
  memset(self->data, 0, self->capacity);
}

static void agateTestBufferAppend(AgateTestBuffer *self, const char *buffer, size_t length) {
  if (length == 0) {
    return;
  }

  ptrdiff_t needed = self->size + length + 1;

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

  memcpy(self->data + self->size, buffer, length);

  self->size += length;
  self->data[self->size] = '\0';
}

/*
 * AgateTest
 */

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
    size_t length = strlen(buffer);

    agateTestBufferAppend(&self->content, buffer, length);

    char *where = strstr(buffer, EXPECT_STRING);

    if (where != NULL) {
      where += sizeof(EXPECT_STRING) - 1; // -1 for '\0'
      agateTestBufferAppend(&self->expected.print, where, length - (where - buffer)); // this includes the final '\n'
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

static bool agateTestExecFile(AgateTest *self, AgateTestFunc func, AgateOutcome *outcome) {
  int ret;

  int out[2];
  ret = pipe(out);
  assert(ret != -1);

  pid_t pid = fork();
  assert(pid != -1);

  if (pid == 0) {
    // child

    // redirect stdout in the pipre
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
    ssize_t length = read(out[0], buffer, BUFFER_SIZE);
    assert(length != -1);

    if (length == 0) {
      ret = close(out[0]);
      assert(ret != -1);
      break;
    }

    agateTestBufferAppend(&outcome->print, buffer, length);
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

static bool agateTestCheckSyntax(AgateTest *self) {
  if (!agateTestExecFile(self, agateTestRunSyntax, &self->syntax)) {
    return false;
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
  printf("%s", text);
}

static void agateTestError(AgateVM *vm, AgateErrorKind kind, const char *module_name, int line, const char *message) {
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

static const char *agateTestModuleLoadFile(const char *path) {
  FILE *file = fopen(path, "rb");

  if (file == NULL) {
    return NULL;
  }

  AgateTestBuffer content;
  agateTestBufferCreate(&content);

  char buffer[BUFFER_SIZE];

  while (!feof(file)) {
    size_t count = fread(buffer, sizeof(char), BUFFER_SIZE, file);
    agateTestBufferAppend(&content, buffer, count);
  }

  fclose(file);

  return content.data;
}

static const char *agateTestModuleLoad(const char *name, void *user_data) {
  AgateTest *test = user_data;

  AgateTestBuffer buffer;
  agateTestBufferCreate(&buffer);

  const char *slash = strrchr(test->path, '/');

  if (slash != NULL) {
    agateTestBufferAppend(&buffer, test->path, slash - test->path + 1);
  }

  agateTestBufferAppend(&buffer, name, strlen(name));
  agateTestBufferAppend(&buffer, ".agate", 6);

  const char *source = agateTestModuleLoadFile(buffer.data);

  agateTestBufferDestroy(&buffer);

  return source;
}

static void agateTestModuleRelease(const char *source, void *user_data) {
  free((void *) source);
}

static AgateModuleHandler agateTestModuleHandler(AgateVM *vm, const char *name) {
  AgateModuleHandler module_handler;
  module_handler.load = agateTestModuleLoad;
  module_handler.release = agateTestModuleRelease;
  module_handler.user_data = agateGetUserData(vm);
  return module_handler;
}

static AgateStatus agateTestRunInterpreter(AgateTest *self) {
  // run the file with the interpreter
  AgateConfig config;
  agateConfigInitialize(&config);

  config.module_handler = agateTestModuleHandler;

  config.assert_handling = AGATE_ASSERT_ABORT;

  config.print = agateTestPrint;
  config.error = agateTestError;

  config.user_data = self;

  AgateVM *vm = agateNewVM(&config);
  AgateStatus status = agateInterpret(vm, self->path, self->content.data);
  agateDeleteVM(vm);

  return status;
}

static bool agateTestCheckRuntime(AgateTest *self) {
  if (!agateTestExecFile(self, agateTestRunInterpreter, &self->actual)) {
    return false;
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

static bool agateTestRunFile(AgateTest *self, const char *path) {
  agateTestReset(self);

  self->path = path;
  self->file = fopen(path, "rb");

  if (self->file == NULL) {
    printf("\tMissing file: %s\n", path);
    return false;
  }

  bool result = true;

  result = result && agateTestReadFile(self);
  result = result && agateTestCheckSyntax(self);
  result = result && agateTestCheckRuntime(self);

  fclose(self->file);
  return result;
}

/*
 *
 */

int main(int argc, char *argv[]) {
  const char **test_files = AgateTestFiles;

  if (argc > 1) {
    test_files = (const char **) (argv + 1);
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

    if (agateTestRunFile(&test, path)) {
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
