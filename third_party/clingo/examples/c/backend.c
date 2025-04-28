#include <clingo.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

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

bool solve(clingo_control_t *ctl, clingo_solve_result_bitset_t *result) {
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
  size_t offset;
  clingo_solve_result_bitset_t solve_ret;
  clingo_control_t *ctl;
  clingo_symbolic_atoms_t const *atoms;
  clingo_backend_t *backend;
  clingo_atom_t atom_ids[4];
  char const *atom_strings[] = {"a", "b", "c"};
  clingo_literal_t body[2];
  clingo_part_t parts[] = {{ "base", NULL, 0 }};

  // create a control object and pass command line arguments
  if (!clingo_control_new(argv+1, argc-1, NULL, NULL, 20, &ctl) != 0) { goto error; }

  // add a logic program to the base part
  if (!clingo_control_add(ctl, "base", NULL, 0, "{a; b; c}.")) { goto error; }

  // ground the base part
  if (!clingo_control_ground(ctl, parts, 1, NULL, NULL)) { goto error; }

  // get the container for symbolic atoms
  if (!clingo_control_symbolic_atoms(ctl, &atoms)) { goto error; }
  // get the ids of atoms a, b, and c
  offset = 0;
  for (char const **it = atom_strings, **ie = it + sizeof(atom_strings) / sizeof(*atom_strings); it != ie; ++it) {
    clingo_symbol_t sym;
    clingo_symbolic_atom_iterator_t atom_it, atom_ie;
    clingo_literal_t lit;
    bool equal;

    // lookup the atom
    if (!clingo_symbol_create_id(*it, true, &sym)) { goto error; }
    if (!clingo_symbolic_atoms_find(atoms, sym, &atom_it)) { goto error; }
    if (!clingo_symbolic_atoms_end(atoms, &atom_ie)) { goto error; }
    if (!clingo_symbolic_atoms_iterator_is_equal_to(atoms, atom_it, atom_ie, &equal)) { goto error; }
    assert(!equal); (void)equal;

    // get the atom's id
    if (!clingo_symbolic_atoms_literal(atoms, atom_it, &lit)) { goto error; }
    atom_ids[offset++] = lit;
  }

  // get the backend
  if (!clingo_control_backend(ctl, &backend)) { goto error; }

  // prepare the backend for adding rules
  if (!clingo_backend_begin(backend)) { goto error; }

  // add an additional atom (called d below)
  if (!clingo_backend_add_atom(backend, NULL, &atom_ids[3])) { goto error; }

  // add rule: d :- a, b.
  body[0] = atom_ids[0];
  body[1] = atom_ids[1];
  if (!clingo_backend_rule(backend, false, &atom_ids[3], 1, body, sizeof(body)/sizeof(*body))) { goto error; }

  // add rule: :- not d, c.
  body[0] = -(clingo_literal_t)atom_ids[3];
  body[1] = atom_ids[2];
  if (!clingo_backend_rule(backend, false, NULL, 0, body, sizeof(body)/sizeof(*body))) { goto error; }

  // finalize the backend
  if (!clingo_backend_end(backend)) { goto error; }

  // solve
  if (!solve(ctl, &solve_ret)) { goto error; }

  goto out;

error:
  if (!(error_message = clingo_error_message())) { error_message = "error"; }

  printf("%s\n", error_message);
  ret = clingo_error_code();

out:
  if (ctl) { clingo_control_free(ctl); }

  return ret;
}

