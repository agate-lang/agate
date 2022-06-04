#include <stdio.h>
#include <stdlib.h>

extern FILE *yyin;
int yyparse(void);
int yylex_destroy(void);

int main(int argc, const char *argv[]) {
  FILE *in = stdin;

  if (argc == 2) {
    in = fopen(argv[1], "rb");

    if (in == NULL) {
      fprintf(stderr, "No file: '%s'\n", argv[1]);
      return EXIT_FAILURE;
    }
  }

  yyin = in;
  int result = yyparse();
  yylex_destroy();

  if (result == 0) {
    printf("No syntax error!\n");
  }

  if (argc == 2) {
    fclose(in);
  }

  return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
