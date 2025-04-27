#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <clingo.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct model_buffer {
  clingo_symbol_t *symbols;
  size_t           symbols_n;
  char            *string;
  size_t           string_n;
} model_buffer_t;

void free_model_buffer(model_buffer_t *buf) {
  if (buf->symbols) {
    free(buf->symbols);
    buf->symbols   = NULL;
    buf->symbols_n = 0;
  }

  if (buf->string) {
    free(buf->string);
    buf->string   = NULL;
    buf->string_n = 0;
  }
}

bool print_symbol(clingo_symbol_t symbol, model_buffer_t *buf) {
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

bool print_model(clingo_model_t const *model, model_buffer_t *buf, char const *label, clingo_show_type_bitset_t show) {
  bool ret = true;
  clingo_symbol_t *symbols;
  size_t n;
  clingo_symbol_t const *it, *ie;

  // determine the number of (shown) symbols in the model
  if (!clingo_model_symbols_size(model, show, &n)) { goto error; }

  // allocate required memory to hold all the symbols
  if (buf->symbols_n < n) {
    if (!(symbols = (clingo_symbol_t*)malloc(sizeof(*buf->symbols) * n))) {
      clingo_set_error(clingo_error_bad_alloc, "could not allocate memory for atoms");
      goto error;
    }

    buf->symbols   = symbols;
    buf->symbols_n = n;
  }

  // retrieve the symbols in the model
  if (!clingo_model_symbols(model, show, buf->symbols, n)) { goto error; }

  printf("%s:", label);

  for (it = buf->symbols, ie = buf->symbols + n; it != ie; ++it) {
    printf(" ");
    if (!print_symbol(*it, buf)) { goto error; }
  }

  printf("\n");
  goto out;

error:
  ret = false;

out:
  return ret;
}

bool print_solution(clingo_model_t const *model, model_buffer_t *data) {
  bool ret = true;
  uint64_t number;
  clingo_model_type_t type;
  char const *type_string = "";

  // get model type
  if (!clingo_model_type(model, &type)) { goto error; }

  switch ((enum clingo_model_type_e)type) {
    case clingo_model_type_stable_model:          { type_string = "Stable model"; break; }
    case clingo_model_type_brave_consequences:    { type_string = "Brave consequences"; break; }
    case clingo_model_type_cautious_consequences: { type_string = "Cautious consequences"; break; }
  }

  // get running number of model
  if (!clingo_model_number(model, &number)) { goto error; }

  printf("%s %" PRIu64 ":\n", type_string, number);

  if (!print_model(model, data, "  shown", clingo_show_type_shown)) { goto error; }
  if (!print_model(model, data, "  atoms", clingo_show_type_atoms)) { goto error; }
  if (!print_model(model, data, "  terms", clingo_show_type_terms)) { goto error; }
  if (!print_model(model, data, " ~atoms", clingo_show_type_complement
                                         | clingo_show_type_atoms)) { goto error; }

  goto out;

error:
  ret = false;

out:
  return ret;
}

bool solve(clingo_control_t *ctl, model_buffer_t *data, clingo_solve_result_bitset_t *result) {
  bool ret = true;
  clingo_solve_handle_t *handle;
  clingo_model_t const *model;

  // get a solve handle
  if (!clingo_control_solve(ctl, clingo_solve_mode_yield, NULL, 0, NULL, NULL, &handle)) { goto error; }
  // loop over all models
  while (true) {
    if (!clingo_solve_handle_resume(handle)) { goto error; }
    if (!clingo_solve_handle_model(handle, &model)) { goto error; }
    // print the model
    if (model) { print_solution(model, data); }
    // stop if there are no more models
    else       { break; }
  }
  // close the solve handle
  if (!clingo_solve_handle_get(handle, result)) { goto error; }

  goto out;

error:
  ret = false;

out:
  // free the solve handle
  return clingo_solve_handle_close(handle) && ret;
}

int main(int argc, char const **argv) {
  char const *error_message;
  int ret = 0;
  clingo_solve_result_bitset_t solve_ret;
  clingo_control_t *ctl = NULL;
  clingo_part_t parts[] = {{ "base", NULL, 0 }};
  model_buffer_t buf = {NULL, 0, NULL, 0};

  // create a control object and pass command line arguments
  if (!clingo_control_new(argv+1, argc-1, NULL, NULL, 20, &ctl) != 0) { goto error; }

  // add a logic program to the base part
  if (!clingo_control_add(ctl, "base", NULL, 0, "1 {a; b} 1. #show c : b. #show a/0.")) { goto error; }

  // ground the base part
  if (!clingo_control_ground(ctl, parts, 1, NULL, NULL)) { goto error; }

  // solve
  if (!solve(ctl, &buf, &solve_ret)) { goto error; }

  goto out;

error:
  if (!(error_message = clingo_error_message())) { error_message = "error"; }

  printf("%s\n", error_message);
  ret = clingo_error_code();

out:
  free_model_buffer(&buf);
  if (ctl) { clingo_control_free(ctl); }

  return ret;
}

