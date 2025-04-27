#include <clingo.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// struct to store parsed command line arguments
typedef struct options {
  char const *program;
} options_t;

char const *name(void *data) {
  (void)data;
  // the name of the program printed in its help output
  return "example";
}

char const *version(void *data) {
  (void)data;
  // the version of the program printed in its help output
  return "1.0.0";
}

bool solve(clingo_control_t *ctl) {
  bool ret = true;
  clingo_solve_handle_t *handle;
  clingo_model_t const *model;
  clingo_solve_result_bitset_t result;

  // get a solve handle
  if (!clingo_control_solve(ctl, clingo_solve_mode_yield, NULL, 0, NULL, NULL, &handle)) { goto error; }
  // loop over all models
  while (true) {
    if (!clingo_solve_handle_resume(handle)) { goto error; }
    if (!clingo_solve_handle_model(handle, &model)) { goto error; }

    if (!model) { break; }
  }
  // close the solve handle
  if (!clingo_solve_handle_get(handle, &result)) { goto error; }

  goto out;

error:
  ret = false;

out:
  // free the solve handle
  return clingo_solve_handle_close(handle) && ret;
}

bool parse_option(char const *value, void *data) {
  char **program = (char **)data;
  // allocate memory for program name
  // note that we forgo freeing memory for the example
  // (it could be done early in the main loop or after clingo_main finished)
  if (!(*program = (char *)malloc(strlen(value) + 1))) {
    return false;
  }
  strcpy(*program, value);
  return true;
}

bool register_options(clingo_options_t *options, void *data) {
  options_t *options_ = (options_t*)data;
  // register an option to overwrite which program part to ground
  return clingo_options_add(options, "Example", "program", "Override the default program part to ground", parse_option, &options_->program, false, "<prog>");
}

bool main_loop(clingo_control_t *ctl, char const *const *files, size_t size, void *data) {
  options_t *options = (options_t*)data;
  bool ret = true;
  clingo_part_t parts[] = {{ options->program ? options->program : "base", NULL, 0 }};
  char const *const *file;

  // load files into the control object
  for (file = files; file != files + size; ++file) {
    if (!clingo_control_load(ctl, *file)) { goto error; }
  }
  // if no files are given read from stdin
  if (size == 0) {
    if (!clingo_control_load(ctl, "-")) { goto error; }
  }

  // ground
  if (!clingo_control_ground(ctl, parts, 1, NULL, NULL)) { goto error; }

  // solve
  if (!solve(ctl)) { goto error; }

  goto out;

error:
  ret = false;

out:
  return ret;
}

int main(int argc, char const **argv) {
  options_t options = { NULL };
  clingo_application_t app = { name, version, NULL, main_loop, NULL, NULL, register_options, NULL };
  return clingo_main(&app, argv+1, argc-1, &options);
}
