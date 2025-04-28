#include <clingo.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

bool print_model(clingo_model_t const *model) {
  bool ret = true;
  clingo_symbol_t *atoms = NULL;
  size_t atoms_n;
  clingo_symbol_t const *it, *ie;
  char *str = NULL;
  size_t str_n = 0;

  // determine the number of (shown) symbols in the model
  if (!clingo_model_symbols_size(model, clingo_show_type_shown, &atoms_n)) { goto error; }

  // allocate required memory to hold all the symbols
  if (!(atoms = (clingo_symbol_t*)malloc(sizeof(*atoms) * atoms_n))) {
    clingo_set_error(clingo_error_bad_alloc, "could not allocate memory for atoms");
    goto error;
  }

  // retrieve the symbols in the model
  if (!clingo_model_symbols(model, clingo_show_type_shown, atoms, atoms_n)) { goto error; }

  printf("Model:");

  for (it = atoms, ie = atoms + atoms_n; it != ie; ++it) {
    size_t n;
    char *str_new;

    // determine size of the string representation of the next symbol in the model
    if (!clingo_symbol_to_string_size(*it, &n)) { goto error; }

    if (str_n < n) {
      // allocate required memory to hold the symbol's string
      if (!(str_new = (char*)realloc(str, sizeof(*str) * n))) {
        clingo_set_error(clingo_error_bad_alloc, "could not allocate memory for symbol's string");
        goto error;
      }

      str = str_new;
      str_n = n;
    }

    // retrieve the symbol's string
    if (!clingo_symbol_to_string(*it, str, n)) { goto error; }

    printf(" %s", str);
  }

  printf("\n");
  goto out;

error:
  ret = false;

out:
  if (atoms) { free(atoms); }
  if (str)   { free(str); }

  return ret;
}

bool solve(clingo_control_t *ctl, clingo_solve_result_bitset_t *result, clingo_literal_t assumption) {
  bool ret = true;
  clingo_solve_handle_t *handle;
  clingo_model_t const *model;

  // get a solve handle
  if (!clingo_control_solve(ctl, clingo_solve_mode_yield, &assumption, assumption != 0 ? 1 : 0, NULL, NULL, &handle)) { goto error; }
  // loop over all models
  while (true) {
    if (!clingo_solve_handle_resume(handle)) { goto error; }
    if (!clingo_solve_handle_model(handle, &model)) { goto error; }
    // print the model
    if (model) { print_model(model); }
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
  size_t size;
  clingo_solve_result_bitset_t solve_ret;
  clingo_control_t *ctl = NULL;
  clingo_part_t parts[] = {{ "base", NULL, 0 }};
  clingo_theory_atoms_t const *atoms;
  clingo_literal_t lit = 0;

  // create a control object and pass command line arguments
  if (!clingo_control_new(argv+1, argc-1, NULL, NULL, 20, &ctl) != 0) { goto error; }

  // add a logic program to the base part
  if (!clingo_control_add(ctl, "base", NULL, 0,
    "#theory t {"
    "  term   { + : 1, binary, left };"
    "  &a/0 : term, any;"
    "  &b/1 : term, {=}, term, any"
    "}."
    "x :- &a { 1+2 }."
    "y :- &b(3) { } = 17."
    )) { goto error; }

  // ground the base part
  if (!clingo_control_ground(ctl, parts, 1, NULL, NULL)) { goto error; }

  // get the theory atoms container
  if (!clingo_control_theory_atoms(ctl, &atoms)) { goto error; }

  // print the number of grounded theory atoms
  if (!clingo_theory_atoms_size(atoms, &size)) { goto error; }
  printf("number of grounded theory atoms: %zu\n", size);

  // verify that theory atom b has a guard
  for (clingo_id_t atom = 0; atom < size; ++atom) {
    clingo_id_t term;
    char const *name;

    // get the term associated with the theory atom
    if (!clingo_theory_atoms_atom_term(atoms, atom, &term)) { goto error; }
    // get the name associated with the theory atom
    if (!clingo_theory_atoms_term_name(atoms, term, &name)) { goto error; }
    // we got theory atom b/1 here
    if (strcmp(name, "b") == 0) {
      bool guard;
      if (!clingo_theory_atoms_atom_has_guard(atoms, atom, &guard)) { goto error; }
      printf("theory atom b/1 has a guard: %s\n", guard ? "true" : "false");
      // get the literal associated with the theory atom
      if (!clingo_theory_atoms_atom_literal(atoms, atom, &lit)) { goto error; }
      break;
    }
  }

  // solve using a model callback
  if (!solve(ctl, &solve_ret, lit)) { goto error; }

  goto out;

error:
  if (!(error_message = clingo_error_message())) { error_message = "error"; }

  printf("%s\n", error_message);
  ret = clingo_error_code();

out:
  if (ctl) { clingo_control_free(ctl); }

  return ret;
}

