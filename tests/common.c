#include "common.h"
#include <tap.h>
#include <string.h>



static int
mystrcmp(const char *a, const char *b) {
  return a == b ? 0 : !a ? -1 : !b ? 1 : strcmp(a, b);
}

#define eq(a, b) (!mystrcmp(a, b))
#define ne(a, b) (mystrcmp(a, b))


void
run_test(int argc, char **argv, char *test_name, void (*test)(void)) {
  int i, should_run;

  should_run = 0;

  if (argc == 1) {
    /* Always run if no arguments present */
    should_run = 1;

  } else {
    /* Otherwise, check if the test name matches any arguments given */

    for (i = 1; i < argc; ++i) {
      if (strncmp(test_name, argv[i], strlen(argv[i])) == 0) {
        should_run = 1;
      }
    }
  }

  if (should_run) {
    diag("---[ %s ]---", test_name);
    test();
  }
}


int
fcmp_ok_at_loc(const char *file, int line, float range, float a,
               const char *op, float b, const char *fmt, ...) {
  int test = eq(op, "==") ? fabs(a - b) <= range : eq(op,
                                                      "!=") ? fabs(a - b) >
    range : eq(op, "<") ? a < b && fabs(a - b) > range : eq(op, ">") ? a > b
    && fabs(a - b) > range : eq(op, "<=") ? a <= b
    || fabs(a - b) <= range : eq(op, ">=") ? a >= b
    || fabs(a - b) <= range : diag("unrecognized operator '%s'", op);
  va_list args;

  va_start(args, fmt);
  vok_at_loc(file, line, test, fmt, args);
  va_end(args);
  if (!test) {
    diag("    %g", a);
    diag("        %s", op);
    diag("    %g", b);
  }
  return test;
}
