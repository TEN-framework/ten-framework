#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <clingo.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

typedef struct {
  char            *string;
  size_t           string_n;
} string_buffer_t;

void free_string_buffer(string_buffer_t *buf) {
  if (buf->string) {
    free(buf->string);
    buf->string   = NULL;
    buf->string_n = 0;
  }
}

bool print_symbol(clingo_symbol_t symbol, string_buffer_t *buf) {
  bool ret = true;
  char *string;
  size_t n;

  // determine size of the string representation of the next symbol in the model
  if (!clingo_symbol_to_string_size(symbol, &n)) { goto error; }

  if (buf->string_n < n) {
    // allocate required memory to hold the symbol's string
    if (!(string = (char*)realloc(buf->string, sizeof(*buf->string) * n))) {
      clingo_set_error(clingo_error_bad_alloc, "could not allocate memory for symbol's string");
      goto error;
    }

    buf->string   = string;
    buf->string_n = n;
  }

  // retrieve the symbol's string
  if (!clingo_symbol_to_string(symbol, buf->string, n)) { goto error; }
  printf("%s", buf->string);
  goto out;

error:
  ret = false;

out:
  return ret;
}

int main() {
  char const *error_message;
  int ret = 0;
  clingo_symbol_t symbols[3];
  string_buffer_t buf = { NULL, 0 };
  clingo_symbol_t const *args;
  size_t size;

  // create a number, identifier (function without arguments), and a function symbol
  clingo_symbol_create_number(42, &symbols[0]);
  if (!clingo_symbol_create_id("x", true, &symbols[1])) { goto error; }
  if (!clingo_symbol_create_function("x", symbols, 2, true, &symbols[2])) { goto error; }

  // print the symbols along with their hash values
  for (size_t i = 0; i < sizeof(symbols) / sizeof(*symbols); ++i) {
    printf("the hash of ");
    if (!print_symbol(symbols[i], &buf)) { goto error; }
    printf(" is %zu\n", clingo_symbol_hash(symbols[i]));
  }

  // compare symbols
  if (!clingo_symbol_arguments(symbols[2], &args, &size)) { goto error; }
  assert(size == 2);
  // equal to comparison
  for (size_t i = 0; i < size; ++i) {
    if (!print_symbol(symbols[0], &buf)) { goto error; }
    printf(" %s ", clingo_symbol_is_equal_to(symbols[0], args[i]) ? "is equal to" : "is not equal to");
    if (!print_symbol(args[i], &buf)) { goto error; }
    printf("\n");
  }
  // less than comparison
  if (!print_symbol(symbols[0], &buf)) { goto error; }
  printf(" %s ", clingo_symbol_is_less_than(symbols[0], symbols[1]) ? "is less than" : "is not less than");
  if (!print_symbol(symbols[1], &buf)) { goto error; }
  printf("\n");

  goto out;

error:
  if (!(error_message = clingo_error_message())) { error_message = "error"; }

  printf("%s\n", error_message);
  ret = clingo_error_code();

out:
  free_string_buffer(&buf);

  return ret;
}

