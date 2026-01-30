/*
** dir.c - Dir
**
** See Copyright Notice in mruby.h
*/

#include "mruby.h"
#include "mruby/class.h"
#include "mruby/data.h"
#include "mruby/error.h"
#include "mruby/string.h"
#include "mruby/presym.h"
#include <sys/types.h>
#if defined(_WIN32) || defined(_WIN64)
  #define MAXPATHLEN 1024
 #if !defined(PATH_MAX)
  #define PATH_MAX MAX_PATH
 #endif
  #define S_ISDIR(B) ((B)&_S_IFDIR)
  #include "Win/dirent.c"
  #include <direct.h>
  #define rmdir(path) _rmdir(path)
  #define getcwd(path,len) _getcwd(path,len)
  #define mkdir(path,mode) _mkdir(path)
  #define chdir(path) _chdir(path)
#else
  #include <sys/param.h>
  #include <dirent.h>
  #include <unistd.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#define E_IO_ERROR mrb_exc_get_id(mrb, MRB_SYM(IOError))

struct mrb_dir {
  DIR *dir;
};

static void
mrb_dir_free(mrb_state *mrb, void *ptr)
{
  struct mrb_dir *mdir = (struct mrb_dir*)ptr;

  if (mdir->dir) {
    closedir(mdir->dir);
    mdir->dir = NULL;
  }
  mrb_free(mrb, mdir);
}

static struct mrb_data_type mrb_dir_type = { "DIR", mrb_dir_free };

static mrb_value
mrb_dir_close(mrb_state *mrb, mrb_value self)
{
  struct mrb_dir *mdir;
  mdir = (struct mrb_dir*)mrb_get_datatype(mrb, self, &mrb_dir_type);
  if (!mdir) return mrb_nil_value();
  if (!mdir->dir) {
    mrb_raise(mrb, E_IO_ERROR, "closed directory");
  }
  if (closedir(mdir->dir) == -1) {
    mrb_sys_fail(mrb, "closedir");
  }
  mdir->dir = NULL;
  return mrb_nil_value();
}

static mrb_value
mrb_dir_init(mrb_state *mrb, mrb_value self)
{
  DIR *dir;
  struct mrb_dir *mdir;
  const char *path;

  mdir = (struct mrb_dir*)DATA_PTR(self);
  if (mdir) {
    mrb_dir_free(mrb, mdir);
  }
  DATA_TYPE(self) = &mrb_dir_type;
  DATA_PTR(self) = NULL;

  mdir = (struct mrb_dir*)mrb_malloc(mrb, sizeof(*mdir));
  mdir->dir = NULL;
  DATA_PTR(self) = mdir;

  mrb_get_args(mrb, "z", &path);
  if ((dir = opendir(path)) == NULL) {
    mrb_sys_fail(mrb, path);
  }
  mdir->dir = dir;
  return self;
}

static mrb_value
mrb_dir_delete(mrb_state *mrb, mrb_value klass)
{
  const char *path;

  mrb_get_args(mrb, "z", &path);
  if (rmdir(path) == -1) {
    mrb_sys_fail(mrb, path);
  }
  return mrb_fixnum_value(0);
}

static mrb_value
mrb_dir_existp(mrb_state *mrb, mrb_value klass)
{
  struct stat sb;
  const char *path;

  mrb_get_args(mrb, "z", &path);
  if (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode)) {
    return mrb_true_value();
  }
  else {
    return mrb_false_value();
  }
}

static mrb_value
mrb_dir_getwd(mrb_state *mrb, mrb_value klass)
{
  mrb_value path;
  mrb_int size = 64;

  path = mrb_str_buf_new(mrb, size);
  while (getcwd(RSTRING_PTR(path), size) == NULL) {
    int e = errno;
    if (e != ERANGE) {
      mrb_sys_fail(mrb, "getcwd(2)");
    }
    size *= 2;
    mrb_str_resize(mrb, path, size);
  }
  mrb_str_resize(mrb, path, strlen(RSTRING_PTR(path)));
  return path;
}

static mrb_value
mrb_dir_mkdir(mrb_state *mrb, mrb_value klass)
{
  mrb_int mode;
  const char *path;

  mode = 0777;
  mrb_get_args(mrb, "z|i", &path, &mode);
  if (mkdir(path, mode) == -1) {
    mrb_sys_fail(mrb, path);
  }
  return mrb_fixnum_value(0);
}

static mrb_value
mrb_dir_chdir(mrb_state *mrb, mrb_value klass)
{
  const char *path;

  mrb_get_args(mrb, "z", &path);
  if (chdir(path) == -1) {
    mrb_sys_fail(mrb, path);
  }
  return mrb_fixnum_value(0);
}

static mrb_value
mrb_dir_chroot(mrb_state *mrb, mrb_value self)
{
#if defined(_WIN32) || defined(_WIN64) || defined(__ANDROID__) || defined(__MSDOS__)
  mrb_raise(mrb, E_NOTIMP_ERROR, "chroot() unreliable on your system");
  return mrb_fixnum_value(0);
#else
  const char *path;
  int res;

  mrb_get_args(mrb, "z", &path);
  res = chroot(path);
  if (res == -1) {
    mrb_sys_fail(mrb, path);
  }

  return mrb_fixnum_value(res);
#endif
}

static mrb_bool
skip_name_p(const char *name)
{
  if (name[0] != '.') return FALSE;
  if (name[1] == '\0') return TRUE;
  if (name[1] != '.') return FALSE;
  if (name[2] == '\0') return TRUE;
  return FALSE;
}

static mrb_value
mrb_dir_empty(mrb_state *mrb, mrb_value self)
{
  DIR *dir;
  struct dirent *dp;
  const char *path;
  mrb_value result = mrb_true_value();

  mrb_get_args(mrb, "z", &path);
  if ((dir = opendir(path)) == NULL) {
    mrb_sys_fail(mrb, path);
  }
  while ((dp = readdir(dir))) {
    if (!skip_name_p(dp->d_name)) {
      result = mrb_false_value();
      break;
    }
  }
  closedir(dir);
  return result;
}

static mrb_value
mrb_dir_read(mrb_state *mrb, mrb_value self)
{
  struct mrb_dir *mdir;
  struct dirent *dp;

  mdir = (struct mrb_dir*)mrb_get_datatype(mrb, self, &mrb_dir_type);
  if (!mdir) return mrb_nil_value();
  if (!mdir->dir) {
    mrb_raise(mrb, E_IO_ERROR, "closed directory");
  }
  dp = readdir(mdir->dir);
  if (dp != NULL) {
    return mrb_str_new_cstr(mrb, dp->d_name);
  }
  else {
    return mrb_nil_value();
  }
}

static mrb_value
mrb_dir_rewind(mrb_state *mrb, mrb_value self)
{
  struct mrb_dir *mdir;

  mdir = (struct mrb_dir*)mrb_get_datatype(mrb, self, &mrb_dir_type);
  if (!mdir) return mrb_nil_value();
  if (!mdir->dir) {
    mrb_raise(mrb, E_IO_ERROR, "closed directory");
  }
  rewinddir(mdir->dir);
  return self;
}

static mrb_value
mrb_dir_seek(mrb_state *mrb, mrb_value self)
{
  #if defined(_WIN32) || defined(_WIN64) || defined(__ANDROID__)
  mrb_raise(mrb, E_NOTIMP_ERROR, "dirseek() unreliable on your system");
  return self;
  #else
  struct mrb_dir *mdir;
  mrb_int pos;

  mdir = (struct mrb_dir*)mrb_get_datatype(mrb, self, &mrb_dir_type);
  if (!mdir) return mrb_nil_value();
  if (!mdir->dir) {
    mrb_raise(mrb, E_IO_ERROR, "closed directory");
  }
  mrb_get_args(mrb, "i", &pos);
  seekdir(mdir->dir, (long)pos);
  return self;
  #endif
}

static mrb_value
mrb_dir_tell(mrb_state *mrb, mrb_value self)
{
#if defined(_WIN32) || defined(_WIN64) || defined(__ANDROID__)
  mrb_raise(mrb, E_NOTIMP_ERROR, "dirtell() unreliable on your system");
  return mrb_fixnum_value(0);
#else
  struct mrb_dir *mdir;
  mrb_int pos;

  mdir = (struct mrb_dir*)mrb_get_datatype(mrb, self, &mrb_dir_type);
  if (!mdir) return mrb_nil_value();
  if (!mdir->dir) {
    mrb_raise(mrb, E_IO_ERROR, "closed directory");
  }
  pos = (mrb_int)telldir(mdir->dir);
  return mrb_fixnum_value(pos);
#endif
}

void
mrb_mruby_dir_gem_init(mrb_state *mrb)
{
  struct RClass *d;

  d = mrb_define_class(mrb, "Dir", mrb->object_class);
  MRB_SET_INSTANCE_TT(d, MRB_TT_DATA);
  mrb_define_class_method(mrb, d, "delete", mrb_dir_delete, MRB_ARGS_REQ(1));
  mrb_define_class_method(mrb, d, "exist?", mrb_dir_existp, MRB_ARGS_REQ(1));
  mrb_define_class_method(mrb, d, "getwd",  mrb_dir_getwd,  MRB_ARGS_NONE());
  mrb_define_class_method(mrb, d, "mkdir",  mrb_dir_mkdir,  MRB_ARGS_REQ(1)|MRB_ARGS_OPT(1));
  mrb_define_class_method(mrb, d, "_chdir", mrb_dir_chdir,  MRB_ARGS_REQ(1));
  mrb_define_class_method(mrb, d, "chroot", mrb_dir_chroot, MRB_ARGS_REQ(1));
  mrb_define_class_method(mrb, d, "empty?", mrb_dir_empty, MRB_ARGS_REQ(1));

  mrb_define_method(mrb, d, "close",      mrb_dir_close,  MRB_ARGS_NONE());
  mrb_define_method(mrb, d, "initialize", mrb_dir_init,   MRB_ARGS_REQ(1));
  mrb_define_method(mrb, d, "read",       mrb_dir_read,   MRB_ARGS_NONE());
  mrb_define_method(mrb, d, "rewind",     mrb_dir_rewind, MRB_ARGS_NONE());
  mrb_define_method(mrb, d, "seek",       mrb_dir_seek,   MRB_ARGS_REQ(1));
  mrb_define_method(mrb, d, "tell",       mrb_dir_tell,   MRB_ARGS_NONE());

  mrb_define_class(mrb, "IOError", E_STANDARD_ERROR);
}

void
mrb_mruby_dir_gem_final(mrb_state *mrb)
{
}
