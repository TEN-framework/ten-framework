#include <clingo.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

// state information for individual solving threads
typedef struct {
  // assignment of pigeons to holes
  // (hole number -> pigeon placement literal or zero)
  clingo_literal_t *holes;
  size_t size;
} state_t;

// state information for the propagator
typedef struct {
  // mapping from solver literals capturing pigeon placements to hole numbers
  // (solver literal -> hole number or zero)
  int *pigeons;
  size_t pigeons_size;
  // array of states
  state_t *states;
  size_t states_size;
} propagator_t;

// returns the offset'th numeric argument of the function symbol sym
bool get_arg(clingo_symbol_t sym, int offset, int *num) {
  clingo_symbol_t const *args;
  size_t args_size;

  // get the arguments of the function symbol
  if (!clingo_symbol_arguments(sym, &args, &args_size)) { return false; }
  // get the requested numeric argument
  if (!clingo_symbol_number(args[offset], num)) { return false; }

  return true;
}

bool init(clingo_propagate_init_t *init, propagator_t *data) {
  // the total number of holes pigeons can be assigned too
  int holes = 0;
  size_t threads = clingo_propagate_init_number_of_threads(init);
  // stores the (numeric) maximum of the solver literals capturing pigeon placements
  // note that the code below assumes that this literal is not negative
  // which holds for the pigeon problem but not in general
  clingo_literal_t max = 0;
  clingo_symbolic_atoms_t const *atoms;
  clingo_signature_t sig;
  clingo_symbolic_atom_iterator_t atoms_it, atoms_ie;
  // ensure that solve can be called multiple times
  // for simplicity, the case that additional holes or pigeons to assign are grounded is not handled here
  if (data->states != NULL) {
    // in principle the number of threads can increase between solve calls by changing the configuration
    // this case is not handled (elegantly) here
    if (threads > data->states_size) {
      clingo_set_error(clingo_error_runtime, "more threads than states");
    }
    return true;
  }
  // allocate memory for exactly one state per thread
  if (!(data->states = (state_t*)malloc(sizeof(*data->states) * threads))) {
    clingo_set_error(clingo_error_bad_alloc, "allocation failed");
    return false;
  }
  memset(data->states, 0, sizeof(*data->states) * threads);
  data->states_size = threads;

  // the propagator monitors place/2 atoms and dectects conflicting assignments
  // first get the symbolic atoms handle
  if (!clingo_propagate_init_symbolic_atoms(init, &atoms)) { return false; }
  // create place/2 signature to filter symbolic atoms with
  if (!clingo_signature_create("place", 2, true, &sig)) { return false; }
  // get an iterator after the last place/2 atom
  // (atom order corresponds to grounding order (and is unpredictable))
  if (!clingo_symbolic_atoms_end(atoms, &atoms_ie)) { return false; }

  // loop over the place/2 atoms in two passes
  // the first pass determines the maximum placement literal
  // the second pass allocates memory for data structures based on the first pass
  for (int pass = 0; pass < 2; ++pass) {
    // get an iterator to the first place/2 atom
    if (!clingo_symbolic_atoms_begin(atoms, &sig, &atoms_it)) { return false; }
    if (pass == 1) {
      // allocate memory for the assignemnt literal -> hole mapping
      if (!(data->pigeons = (int*)malloc(sizeof(*data->pigeons) * (max + 1)))) {
        clingo_set_error(clingo_error_bad_alloc, "allocation failed");
        return false;
      }
      data->pigeons_size = max + 1;
    }
    while (true) {
      int h;
      bool equal;
      clingo_literal_t lit;
      clingo_symbol_t sym;

      // stop iteration if the end is reached
      if (!clingo_symbolic_atoms_iterator_is_equal_to(atoms, atoms_it, atoms_ie, &equal)) { return false; }
      if (equal) { break; }

      // get the solver literal for the placement atom
      if (!clingo_symbolic_atoms_literal(atoms, atoms_it, &lit)) { return false; }
      if (!clingo_propagate_init_solver_literal(init, lit, &lit)) { return false; }

      if (pass == 0) {
        // determine the maximum literal
        assert(lit > 0);
        if (lit > max) { max = lit; }
      }
      else {
        // extract the hole number from the atom
        if (!clingo_symbolic_atoms_symbol(atoms, atoms_it, &sym)) { return false; }
        if (!get_arg(sym, 1, &h)) { return false; }

        // initialize the assignemnt literal -> hole mapping
        data->pigeons[lit] = h;

        // watch the assignment literal
        if (!clingo_propagate_init_add_watch(init, lit)) { return false; }

        // update the total number of holes
        if (h + 1 > holes)   { holes = h + 1; }
      }

      // advance to the next placement atom
      if (!clingo_symbolic_atoms_next(atoms, atoms_it, &atoms_it)) { return false; }
    }
  }

  // initialize the per solver thread state information
  for (size_t i = 0; i < threads; ++i) {
    if (!(data->states[i].holes = (clingo_literal_t*)malloc(sizeof(*data->states[i].holes) * holes))) {
      clingo_set_error(clingo_error_bad_alloc, "allocation failed");
      return false;
    }
    // initially no pigeons are assigned to any holes
    // so the hole -> literal mapping is initialized with zero
    // which is not a valid literal
    memset(data->states[i].holes, 0, sizeof(*data->states[i].holes) * holes);
    data->states[i].size = holes;
  }

  return true;
}

bool propagate(clingo_propagate_control_t *control, const clingo_literal_t *changes, size_t size, propagator_t *data) {
  // get the thread specific state
  state_t state = data->states[clingo_propagate_control_thread_id(control)];

  // apply and check the pigeon assignments done by the solver
  for (size_t i = 0; i < size; ++i) {
    // the freshly assigned literal
    clingo_literal_t lit = changes[i];
    // a pointer to the previously assigned literal
    clingo_literal_t *prev = state.holes + data->pigeons[lit];

    // update the placement if no literal was assigned previously
    if (*prev == 0) { *prev = lit; }
    // create a conflicting clause and propagate it
    else {
      // current and previous literal must not hold together
      clingo_literal_t clause[] = { -lit, -*prev };
      // stores the result when adding a clause or propagationg
      // if result is false propagation must stop for the solver to backtrack
      bool result;

      // add the clause
      if (!clingo_propagate_control_add_clause(control, clause, sizeof(clause)/sizeof(*clause), clingo_clause_type_learnt, &result)) { return false; }
      if (!result) { return true; }

      // propagate it
      if (!clingo_propagate_control_propagate(control, &result)) { return false; }
      if (!result) { return true; }

      // must not happen because the clause above is conflicting by construction
      assert(false);
    }
  }

  return true;
}

void undo(clingo_propagate_control_t *control, const clingo_literal_t *changes, size_t size, propagator_t *data) {
  // get the thread specific state
  state_t state = data->states[clingo_propagate_control_thread_id(control)];

  // undo the assignments made in propagate
  for (size_t i = 0; i < size; ++i) {
    clingo_literal_t lit = changes[i];
    int hole = data->pigeons[lit];

    if (state.holes[hole] == lit) {
      // undo the assignment
      state.holes[hole] = 0;
    }
  }
}

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
  clingo_solve_result_bitset_t solve_ret;
  clingo_control_t *ctl = NULL;
  // arguments to the pigeon program part
  clingo_symbol_t args[2];
  // the pigeon program part having the number of holes and pigeons as parameters
  clingo_part_t parts[] = {{ "pigeon", args, sizeof(args)/sizeof(*args) }};
  // parameters for the pigeon part
  char const *params[] = {"h", "p"};
  // create a propagator with the functions above
  // using the default implementation for the model check
  clingo_propagator_t prop = {
    (clingo_propagator_init_callback_t)init,
    (clingo_propagator_propagate_callback_t)propagate,
    (clingo_propagator_undo_callback_t)undo,
    NULL,
    NULL,
  };
  // user data for the propagator
  propagator_t prop_data = { NULL, 0, NULL, 0 };

  // set the number of holes
  clingo_symbol_create_number(8, &args[0]);
  // set the number of pigeons
  clingo_symbol_create_number(9, &args[1]);

  // create a control object and pass command line arguments
  if (!clingo_control_new(argv+1, argc-1, NULL, NULL, 20, &ctl) != 0) { goto error; }

  // register the propagator
  if (!clingo_control_register_propagator(ctl, &prop, &prop_data, false)) { goto error; }

  // add a logic program to the pigeon part
  if (!clingo_control_add(ctl, "pigeon", params, sizeof(params)/sizeof(*params),
                          "1 { place(P,H) : H = 1..h } 1 :- P = 1..p.")) { goto error; }

  // ground the pigeon part
  if (!clingo_control_ground(ctl, parts, 1, NULL, NULL)) { goto error; }

  // solve using a model callback
  if (!solve(ctl, &solve_ret)) { goto error; }

  goto out;

error:
  if (!(error_message = clingo_error_message())) { error_message = "error"; }

  printf("%s\n", error_message);
  ret = clingo_error_code();

out:
  // free the propagator state
  if (prop_data.pigeons) { free(prop_data.pigeons); }
  if (prop_data.states_size > 0) {
    for (size_t i = 0; i < prop_data.states_size; ++i) {
      if (prop_data.states[i].holes) {
        free(prop_data.states[i].holes);
      }
    }
    free(prop_data.states);
  }

  if (ctl) { clingo_control_free(ctl); }

  return ret;
}

