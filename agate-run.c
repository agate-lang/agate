// SPDX-License-Identifier: MIT
// Copyright (c) 2022 Julien Bernard
#include <stdio.h>
#include <stdlib.h>

#include "agate.h"

static void repl(AgateVM *vm) {
  char line[1024];

  for (;;) {
    printf("> ");

    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");
      break;
    }

    agateCallString(vm, "script", line);
  }
}

static char *dump(const char *path) {
  FILE *file = fopen(path, "rb");

  if (file == NULL) {
    fprintf(stderr, "Could not open file \"%s\".\n", path);
    exit(EXIT_FAILURE);
  }

  fseek(file, 0L, SEEK_END);
  long ret = ftell(file);
  size_t size = ret > 0 ? ret : 0;
  rewind(file);

  char *buffer = (char *) malloc(size + 1);

  if (buffer == NULL) {
    fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
    exit(EXIT_FAILURE);
  }

  size_t count = fread(buffer, sizeof(char), size, file);

  if (count < size) {
    fprintf(stderr, "Could not read file \"%s\".\n", path);
    exit(EXIT_FAILURE);
  }

  buffer[count] = '\0';

  fclose(file);
  return buffer;
}

static void run(AgateVM *vm, const char *path) {
  char *source = dump(path);
  AgateStatus status = agateCallString(vm, "script", source);
  free(source);

  if (status != AGATE_STATUS_OK) {
    exit(EXIT_FAILURE);
  }
}

static void print(AgateVM *vm, const char* text) {
  fputs(text, stdout);
}

static void write(AgateVM *vm, uint8_t byte) {
  fputc(byte, stdout);
}

static void error(AgateVM *vm, AgateErrorKind kind, const char *unit_name, int line, const char *message) {
  switch (kind) {
    case AGATE_ERROR_COMPILE:
      printf("%s:%d: error: %s\n", unit_name, line, message);
      break;
    case AGATE_ERROR_RUNTIME:
      printf("error: %s\n", message);
      break;
    case AGATE_ERROR_STACKTRACE:
      printf("%s:%d: in %s\n", unit_name, line, message);
      break;
  }
}

static bool input(AgateVM *vm, char *buffer, size_t size) {
  return fgets(buffer, size, stdin) != NULL;
}

int main(int argc, const char *argv[]) {
  AgateConfig config;
  agateConfigInitialize(&config);
  config.print = print;
  config.write = write;
  config.error = error;
  config.input = input;

  AgateVM *vm = agateNewVM(&config);

  if (argc == 1) {
    repl(vm);
  } else if (argc >= 2) {
    agateSetArgs(vm, argc - 1, argv + 1);
    run(vm, argv[1]);
  } else {
    fprintf(stderr, "Usage: agate-run [path]\n");
    return EXIT_FAILURE;
  }

  agateDeleteVM(vm);
  return EXIT_SUCCESS;
}
