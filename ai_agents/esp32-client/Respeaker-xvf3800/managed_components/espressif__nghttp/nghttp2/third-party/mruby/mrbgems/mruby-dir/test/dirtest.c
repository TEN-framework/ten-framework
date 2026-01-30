#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#ifdef __cplusplus
extern "C" {
#endif
typedef struct DIR DIR;
struct dirent
{
  char *d_name;
};
DIR *opendir(const char *name);
struct dirent *readdir(DIR *dir);
int closedir(DIR *dir);
#ifdef __cplusplus
}
#endif
#include <io.h>
#include <direct.h>
#define rmdir(path) _rmdir(path)
#define getcwd(path,len) _getcwd(path,len)
#define mkdir(path,mode) _mkdir(path)
#define chdir(path) _chdir(path)
#else
#include <unistd.h>
#include <dirent.h>
#endif

#include "mruby.h"
#include "mruby/string.h"
#include "mruby/variable.h"

static void
make_dir(mrb_state *mrb, const char *name, const char *up)
{
  if (mkdir(name, 0) == -1) {
    if (chdir("..") == 0) {
      rmdir(up);
    }
    mrb_raisef(mrb, E_RUNTIME_ERROR, "mkdir(%s) failed", mrb_str_new_cstr(mrb, name));
  }
}

mrb_value
mrb_dirtest_setup(mrb_state *mrb, mrb_value klass)
{
  char buf[1024];
  const char *aname = "a";
  const char *bname = "b";

  /* save current working directory */
  if (getcwd(buf, sizeof(buf)) == NULL) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "getcwd() failed");
  }
  mrb_cv_set(mrb, klass, mrb_intern_cstr(mrb, "pwd"), mrb_str_new_cstr(mrb, buf));

  /* create sandbox */
#if defined(_WIN32) || defined(_WIN64)
  snprintf(buf, sizeof(buf), "%s\\mruby-dir-test.XXXXXX", _getcwd(NULL,0));
  if ((_mktemp(buf) == NULL) || mkdir(buf,0) != 0) {
    mrb_raisef(mrb, E_RUNTIME_ERROR, "mkdtemp(%s) failed", buf);
  }
#else
  snprintf(buf, sizeof(buf), "%s/mruby-dir-test.XXXXXX", P_tmpdir);
  if (mkdtemp(buf) == NULL) {
    mrb_raisef(mrb, E_RUNTIME_ERROR, "mkdtemp(%s) failed", buf);
  }
#endif
  mrb_cv_set(mrb, klass, mrb_intern_cstr(mrb, "sandbox"), mrb_str_new_cstr(mrb, buf));

  /* go to sandbox */
  if (chdir(buf) == -1) {
    rmdir(buf);
    mrb_raisef(mrb, E_RUNTIME_ERROR, "chdir(%s) failed", buf);
  }

  /* make some directories in the sandbox */
  make_dir(mrb, aname, buf);
  make_dir(mrb, bname, buf);

  return mrb_true_value();
}

mrb_value
mrb_dirtest_teardown(mrb_state *mrb, mrb_value klass)
{
  mrb_value d, sandbox;
  DIR *dirp;
  struct dirent *dp;
  const char *path;

  /* cleanup sandbox */
  sandbox = mrb_cv_get(mrb, klass, mrb_intern_cstr(mrb, "sandbox"));
  path = mrb_str_to_cstr(mrb, sandbox);

  dirp = opendir(path);
  while ((dp = readdir(dirp)) != NULL) {
    if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
      continue;
    if (rmdir(dp->d_name) == -1) {
      mrb_raisef(mrb, E_RUNTIME_ERROR, "rmdir(%s) failed", dp->d_name);
    }
  }
  closedir(dirp);

  /* back to original pwd */
  d = mrb_cv_get(mrb, klass, mrb_intern_cstr(mrb, "pwd"));
  path = mrb_str_to_cstr(mrb, d);
  if (chdir(path) == -1) {
    mrb_raisef(mrb, E_RUNTIME_ERROR, "chdir(%s) failed", path);
  }

  /* remove sandbox directory */
  sandbox = mrb_cv_get(mrb, klass, mrb_intern_cstr(mrb, "sandbox"));
  path = mrb_str_to_cstr(mrb, sandbox);
  if (rmdir(path) == -1) {
    mrb_raisef(mrb, E_RUNTIME_ERROR, "rmdir(%s) failed", path);
  }

  return mrb_true_value();
}

mrb_value
mrb_dirtest_sandbox(mrb_state *mrb, mrb_value klass)
{
  return mrb_cv_get(mrb, klass, mrb_intern_cstr(mrb, "sandbox"));
}

void
mrb_mruby_dir_gem_test(mrb_state *mrb)
{
  struct RClass *c = mrb_define_module(mrb, "DirTest");

  mrb_define_class_method(mrb, c, "sandbox", mrb_dirtest_sandbox, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, c, "setup", mrb_dirtest_setup, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, c, "teardown", mrb_dirtest_teardown, MRB_ARGS_NONE());
}
