#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  if (argc != 4) {
    fprintf(stderr, "Usage: xembed <input> <output> <name>\n");
    return EXIT_FAILURE;
  }

  FILE *in = fopen(argv[1], "rb");

  if (in == NULL) {
    fprintf(stderr, "Could not open input file '%s'\n", argv[1]);
    return EXIT_FAILURE;
  }

  FILE *out = fopen(argv[2], "wb");

  if (out == NULL) {
    fprintf(stderr, "Could not open output file '%s'\n", argv[2]);
    fclose(in);
    return EXIT_FAILURE;
  }

  fprintf(out, "static char %s[] = {\n\t", argv[3]);
  size_t total_size = 0;

#define BUFFER_SIZE 1024

  char buffer[BUFFER_SIZE];

  while (!feof(in)) {
    size_t read = fread(buffer, sizeof(char), BUFFER_SIZE, in);
    total_size += read;

    for (size_t i = 0; i < read; ++i) {
      char c = buffer[i];
      fprintf(out, "0x%.2X,", c);

      if (c == '\n') {
        fputs("\n\t", out);
      } else {
        fputs(" ", out);
      }
    }
  }

  fclose(in);

  fputs("0x00\n};\n", out);
  fprintf(out, "// size: %zu\n", total_size);
  fclose(out);
  return EXIT_SUCCESS;
}
