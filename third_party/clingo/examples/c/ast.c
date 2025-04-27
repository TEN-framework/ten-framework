#include <clingo.h>
#include <stdlib.h>
#include <stdio.h>

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

typedef struct {
  clingo_location_t *loc;
  clingo_ast_t *atom;
  clingo_program_builder_t *builder;
} on_statement_data;

// adds atom enable to all rule bodies
bool on_statement (clingo_ast_t *stm, on_statement_data *data) {
  bool ret = true;
  clingo_ast_t *lit = NULL;
  clingo_ast_type_t type;
  size_t size;

  if (!clingo_ast_get_type(stm, &type)) { goto error; }

  // pass through all statements that are not rules
  if (type != clingo_ast_type_rule) {
    if (!clingo_program_builder_add(data->builder, stm)) { goto error; }
    goto out;
  }

  // create literal "enable"
  if (!clingo_ast_build(clingo_ast_type_literal, &lit, data->loc, clingo_ast_sign_no_sign, data->atom)) {
    goto error;
  }

  if (!clingo_ast_attribute_size_ast_array(stm, clingo_ast_attribute_body, &size)) {
    goto error;
  }

  // append the literal to the rule body
  if (!clingo_ast_attribute_insert_ast_at(stm, clingo_ast_attribute_body, size, lit)) {
    goto error;
  }

  // add the rewritten statement to the program
  if (!clingo_program_builder_add(data->builder, stm)) { goto error; }

  goto out;

error:
  ret = false;

out:
  if (lit != NULL) {
    clingo_ast_release(lit);
  }
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
  clingo_solve_result_bitset_t solve_ret;
  clingo_control_t *ctl = NULL;
  clingo_symbolic_atoms_t const *atoms = NULL;
  clingo_solve_handle_t *handle = NULL;
  clingo_symbol_t sym;
  clingo_symbolic_atom_iterator_t atm_it;
  clingo_literal_t atm;
  clingo_location_t location;
  clingo_ast_t *term = NULL;
  on_statement_data data = {NULL, NULL, NULL};
  clingo_part_t parts[] = {{ "base", NULL, 0 }};

  // create a control object and pass command line arguments
  if (!clingo_control_new(argv+1, argc-1, NULL, NULL, 20, &ctl) != 0) { goto error; }

  // get the program builder
  if (!clingo_program_builder_init(ctl, &data.builder)) { goto error; }

  // initialize the location
  location.begin_line   = location.end_line   = 0;
  location.begin_column = location.end_column = 0;
  location.begin_file   = location.end_file   = "<rewrite>";
  data.loc = &location;

  // initilize atom to add
  if (!clingo_symbol_create_id("enable", true, &sym)) { goto error; }

  if (!clingo_ast_build(clingo_ast_type_symbolic_term, &term, data.loc, sym)) {
    goto error;
  }
  if (!clingo_ast_build(clingo_ast_type_symbolic_atom, &data.atom, term)) {
    goto error;
  }

  // begin building a program
  if (!clingo_program_builder_begin(data.builder)) { goto error; }

  // get the AST of the program
  if (!clingo_ast_parse_string("a :- not b. b :- not a.", (clingo_ast_callback_t)on_statement, &data, NULL, NULL, NULL, 20)) { goto error; }

  // finish building a program
  if (!clingo_program_builder_end(data.builder)) { goto error; }

  // add the external statement: #external enable.
  if (!clingo_control_add(ctl, "base", NULL, 0, "#external enable.")) {
    goto error;
  }

  // ground the base part
  if (!clingo_control_ground(ctl, parts, 1, NULL, NULL)) { goto error; }

  // get the program literal coresponding to the external atom
  if (!clingo_control_symbolic_atoms(ctl, &atoms)) { goto error; }
  if (!clingo_symbolic_atoms_find(atoms, sym, &atm_it)) { goto error; }
  if (!clingo_symbolic_atoms_literal(atoms, atm_it, &atm)) { goto error; }

  // solve with external enable = false
  printf("Solving with enable = false...\n");
  if (!solve(ctl, &solve_ret)) { goto error; }
  // solve with external enable = true
  printf("Solving with enable = true...\n");
  if (!clingo_control_assign_external(ctl, atm, clingo_truth_value_true)) { goto error; }
  if (!solve(ctl, &solve_ret)) { goto error; }
  // solve with external enable = false
  printf("Solving with enable = false...\n");
  if (!clingo_control_assign_external(ctl, atm, clingo_truth_value_false)) { goto error; }
  if (!solve(ctl, &solve_ret)) { goto error; }

  goto out;

error:
  if (!(error_message = clingo_error_message())) { error_message = "error"; }

  printf("%s\n", error_message);
  ret = clingo_error_code();

out:
  if (term) { clingo_ast_release(term); }
  if (data.atom) { clingo_ast_release(data.atom); }
  if (handle) { clingo_solve_handle_close(handle); }
  if (ctl) { clingo_control_free(ctl); }

  return ret;
}
