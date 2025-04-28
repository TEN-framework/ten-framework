#include <clingo.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct string_buffer {
  char   *string;
  size_t  string_n;
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

int main(int argc, char const **argv) {
  char const *error_message;
  string_buffer_t buf = {NULL, 0};
  int ret = 0;
  clingo_control_t *ctl = NULL;
  clingo_part_t parts[] = {{ "base", NULL, 0 }};
  clingo_symbolic_atoms_t const *atoms;
  clingo_symbolic_atom_iterator_t it_atoms, ie_atoms;

  // create a control object and pass command line arguments
  if (!clingo_control_new(argv+1, argc-1, NULL, NULL, 20, &ctl) != 0) { goto error; }

  // add a logic program to the base part
  if (!clingo_control_add(ctl, "base", NULL, 0, "a. {b}. #external c.")) { goto error; }

  // ground the base part
  if (!clingo_control_ground(ctl, parts, 1, NULL, NULL)) { goto error; }

  // get symbolic atoms
  if (!clingo_control_symbolic_atoms(ctl, &atoms)) { goto error; }

  // get begin and end iterator
  if (!clingo_symbolic_atoms_begin(atoms, NULL, &it_atoms)) { goto error; }

  if (!clingo_symbolic_atoms_end(atoms, &ie_atoms)) { goto error; }

  printf("Symbolic atoms:\n");

  for (;;) {
    bool equal, fact, external;
    clingo_symbol_t symbol;

    // check if we are at the end of the sequence
    if (!clingo_symbolic_atoms_iterator_is_equal_to(atoms, it_atoms, ie_atoms, &equal)) { goto error; }

    if (equal) { break; }

    // get the associated symbol
    if (!clingo_symbolic_atoms_symbol(atoms, it_atoms, &symbol)) { goto error; }

    // determine if the atom is fact or external
    if (!clingo_symbolic_atoms_is_fact(atoms, it_atoms, &fact)) { goto error; }
    if (!clingo_symbolic_atoms_is_external(atoms, it_atoms, &external)) { goto error; }

    printf("  ");

    if (!print_symbol(symbol, &buf)) { goto error; }
    if (fact) { printf(", fact"); }
    if (external) { printf(", external"); }

    printf("\n");

    // advance the next element in the sequence
    if (!clingo_symbolic_atoms_next(atoms, it_atoms, &it_atoms)) { goto error; }
  }

  goto out;

error:
  if (!(error_message = clingo_error_message())) { error_message = "error"; }

  printf("%s\n", error_message);
  ret = clingo_error_code();

out:
  free_string_buffer(&buf);
  if (ctl) { clingo_control_free(ctl); }

  return ret;
}

