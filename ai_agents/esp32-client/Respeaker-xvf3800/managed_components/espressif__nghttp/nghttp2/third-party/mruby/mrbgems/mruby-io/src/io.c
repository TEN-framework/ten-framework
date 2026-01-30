/*
** io.c - IO class
*/

#include "mruby.h"
#include "mruby/array.h"
#include "mruby/class.h"
#include "mruby/data.h"
#include "mruby/hash.h"
#include "mruby/string.h"
#include "mruby/variable.h"
#include "mruby/ext/io.h"
#include "mruby/error.h"
#include "mruby/internal.h"
#include "mruby/presym.h"

#include <sys/types.h>
#include <sys/stat.h>

#if defined(_WIN32) || defined(_WIN64)
  #include <winsock.h>
  #include <io.h>
  #include <basetsd.h>
  #define open  _open
  #define close _close
  #define dup _dup
  #define dup2 _dup2
  #define read  _read
  #define write _write
  #define lseek _lseek
  #define isatty _isatty
  #define WEXITSTATUS(x) (x)
  typedef int fsize_t;
  typedef long ftime_t;
  typedef long fsuseconds_t;
  typedef int fmode_t;
  typedef int fssize_t;

  #ifndef O_TMPFILE
    #define O_TMPFILE O_TEMPORARY
  #endif

#else
  #include <sys/wait.h>
  #include <sys/time.h>
  #include <unistd.h>
  typedef size_t fsize_t;
  typedef time_t ftime_t;
#ifdef __DJGPP__
  typedef long fsuseconds_t;
#else
  typedef suseconds_t fsuseconds_t;
#endif
  typedef mode_t fmode_t;
  typedef ssize_t fssize_t;
#endif

#ifdef _MSC_VER
typedef mrb_int pid_t;
#endif

#include <fcntl.h>

#include <errno.h>
#include <string.h>

#define OPEN_ACCESS_MODE_FLAGS (O_RDONLY | O_WRONLY | O_RDWR)
#define OPEN_RDONLY_P(f)       ((mrb_bool)(((f) & OPEN_ACCESS_MODE_FLAGS) == O_RDONLY))
#define OPEN_WRONLY_P(f)       ((mrb_bool)(((f) & OPEN_ACCESS_MODE_FLAGS) == O_WRONLY))
#define OPEN_RDWR_P(f)         ((mrb_bool)(((f) & OPEN_ACCESS_MODE_FLAGS) == O_RDWR))
#define OPEN_READABLE_P(f)     ((mrb_bool)(OPEN_RDONLY_P(f) || OPEN_RDWR_P(f)))
#define OPEN_WRITABLE_P(f)     ((mrb_bool)(OPEN_WRONLY_P(f) || OPEN_RDWR_P(f)))

static void io_free(mrb_state *mrb, void *ptr);
struct mrb_data_type mrb_io_type = { "IO", io_free };


static int io_modestr_to_flags(mrb_state *mrb, const char *modestr);
static int io_mode_to_flags(mrb_state *mrb, mrb_value mode);
static void fptr_finalize(mrb_state *mrb, struct mrb_io *fptr, int quiet);

static struct mrb_io*
io_get_open_fptr(mrb_state *mrb, mrb_value io)
{
  struct mrb_io *fptr;

  fptr = (struct mrb_io*)mrb_data_get_ptr(mrb, io, &mrb_io_type);
  if (fptr == NULL) {
    mrb_raise(mrb, E_IO_ERROR, "uninitialized stream");
  }
  if (fptr->fd < 0) {
    mrb_raise(mrb, E_IO_ERROR, "closed stream");
  }
  return fptr;
}

#if !defined(MRB_NO_IO_POPEN) && defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
# define MRB_NO_IO_POPEN 1
#endif

#ifndef MRB_NO_IO_POPEN
static void
io_set_process_status(mrb_state *mrb, pid_t pid, int status)
{
  struct RClass *c_process, *c_status;
  mrb_value v;

  c_status = NULL;
  if (mrb_class_defined_id(mrb, MRB_SYM(Process))) {
    c_process = mrb_module_get_id(mrb, MRB_SYM(Process));
    if (mrb_const_defined(mrb, mrb_obj_value(c_process), MRB_SYM(Status))) {
      c_status = mrb_class_get_under_id(mrb, c_process, MRB_SYM(Status));
    }
  }
  if (c_status != NULL) {
    v = mrb_funcall_id(mrb, mrb_obj_value(c_status), MRB_SYM(new), 2, mrb_fixnum_value(pid), mrb_fixnum_value(status));
  }
  else {
    v = mrb_fixnum_value(WEXITSTATUS(status));
  }
  mrb_gv_set(mrb, mrb_intern_lit(mrb, "$?"), v);
}
#endif

static int
io_modestr_to_flags(mrb_state *mrb, const char *mode)
{
  int flags;
  const char *m = mode;

  switch (*m++) {
    case 'r':
      flags = O_RDONLY;
      break;
    case 'w':
      flags = O_WRONLY | O_CREAT | O_TRUNC;
      break;
    case 'a':
      flags = O_WRONLY | O_CREAT | O_APPEND;
      break;
    default:
      goto modeerr;
  }

  while (*m) {
    switch (*m++) {
      case 'b':
#ifdef O_BINARY
        flags |= O_BINARY;
#endif
        break;
      case 'x':
        if (mode[0] != 'w') goto modeerr;
        flags |= O_EXCL;
        break;
      case '+':
        flags = (flags & ~OPEN_ACCESS_MODE_FLAGS) | O_RDWR;
        break;
      case ':':
        /* XXX: PASSTHROUGH*/
      default:
        goto modeerr;
    }
  }

  return flags;

 modeerr:
  mrb_raisef(mrb, E_ARGUMENT_ERROR, "illegal access mode %s", mode);
  return 0; /* not reached */
}

static int
io_mode_to_flags(mrb_state *mrb, mrb_value mode)
{
  if (mrb_nil_p(mode)) {
    return O_RDONLY;
  }
  else if (mrb_string_p(mode)) {
    return io_modestr_to_flags(mrb, RSTRING_CSTR(mrb, mode));
  }
  else {
    int flags = 0;
    mrb_int flags0 = mrb_as_int(mrb, mode);

    switch (flags0 & MRB_O_ACCMODE) {
      case MRB_O_RDONLY:
        flags |= O_RDONLY;
        break;
      case MRB_O_WRONLY:
        flags |= O_WRONLY;
        break;
      case MRB_O_RDWR:
        flags |= O_RDWR;
        break;
      default:
        mrb_raisef(mrb, E_ARGUMENT_ERROR, "illegal access mode %v", mode);
    }

    if (flags0 & MRB_O_APPEND)        flags |= O_APPEND;
    if (flags0 & MRB_O_CREAT)         flags |= O_CREAT;
    if (flags0 & MRB_O_EXCL)          flags |= O_EXCL;
    if (flags0 & MRB_O_TRUNC)         flags |= O_TRUNC;
#ifdef O_NONBLOCK
    if (flags0 & MRB_O_NONBLOCK)      flags |= O_NONBLOCK;
#endif
#ifdef O_NOCTTY
    if (flags0 & MRB_O_NOCTTY)        flags |= O_NOCTTY;
#endif
#ifdef O_BINARY
    if (flags0 & MRB_O_BINARY)        flags |= O_BINARY;
#endif
#ifdef O_SHARE_DELETE
    if (flags0 & MRB_O_SHARE_DELETE)  flags |= O_SHARE_DELETE;
#endif
#ifdef O_SYNC
    if (flags0 & MRB_O_SYNC)          flags |= O_SYNC;
#endif
#ifdef O_DSYNC
    if (flags0 & MRB_O_DSYNC)         flags |= O_DSYNC;
#endif
#ifdef O_RSYNC
    if (flags0 & MRB_O_RSYNC)         flags |= O_RSYNC;
#endif
#ifdef O_NOFOLLOW
    if (flags0 & MRB_O_NOFOLLOW)      flags |= O_NOFOLLOW;
#endif
#ifdef O_NOATIME
    if (flags0 & MRB_O_NOATIME)       flags |= O_NOATIME;
#endif
#ifdef O_DIRECT
    if (flags0 & MRB_O_DIRECT)        flags |= O_DIRECT;
#endif
#ifdef O_TMPFILE
    if (flags0 & MRB_O_TMPFILE)       flags |= O_TMPFILE;
#endif

    return flags;
  }
}

static void
io_fd_cloexec(mrb_state *mrb, int fd)
{
#if defined(F_GETFD) && defined(F_SETFD) && defined(FD_CLOEXEC)
  int flags, flags2;

  flags = fcntl(fd, F_GETFD);
  if (flags < 0) {
    mrb_sys_fail(mrb, "cloexec GETFD");
  }
  if (fd <= 2) {
    flags2 = flags & ~FD_CLOEXEC; /* Clear CLOEXEC for standard file descriptors: 0, 1, 2. */
  }
  else {
    flags2 = flags | FD_CLOEXEC; /* Set CLOEXEC for non-standard file descriptors: 3, 4, 5, ... */
  }
  if (flags != flags2) {
    if (fcntl(fd, F_SETFD, flags2) < 0) {
      mrb_sys_fail(mrb, "cloexec SETFD");
    }
  }
#endif
}

#if !defined(_WIN32) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
static int
io_cloexec_pipe(mrb_state *mrb, int fildes[2])
{
  int ret;
  ret = pipe(fildes);
  if (ret == -1)
    return -1;
  io_fd_cloexec(mrb, fildes[0]);
  io_fd_cloexec(mrb, fildes[1]);
  return ret;
}

static int
io_pipe(mrb_state *mrb, int pipes[2])
{
  int ret;
  ret = io_cloexec_pipe(mrb, pipes);
  if (ret == -1) {
    if (errno == EMFILE || errno == ENFILE) {
      mrb_garbage_collect(mrb);
      ret = io_cloexec_pipe(mrb, pipes);
    }
  }
  return ret;
}

static int
io_process_exec(const char *pname)
{
  const char *s;
  s = pname;

  while (*s == ' ' || *s == '\t' || *s == '\n')
    s++;

  if (!*s) {
    errno = ENOENT;
    return -1;
  }

  execl("/bin/sh", "sh", "-c", pname, (char*)NULL);
  return -1;
}
#endif

static void
io_free(mrb_state *mrb, void *ptr)
{
  struct mrb_io *io = (struct mrb_io*)ptr;
  if (io != NULL) {
    fptr_finalize(mrb, io, TRUE);
    mrb_free(mrb, io);
  }
}

static void
io_init_buf(mrb_state *mrb, struct mrb_io *fptr)
{
  if (fptr->readable) {
    fptr->buf = (struct mrb_io_buf*)mrb_malloc(mrb, sizeof(struct mrb_io_buf));
    fptr->buf->start = 0;
    fptr->buf->len = 0;
  }
}

static struct mrb_io *
io_alloc(mrb_state *mrb)
{
  struct mrb_io *fptr;

  fptr = (struct mrb_io*)mrb_malloc(mrb, sizeof(struct mrb_io));
  fptr->fd = -1;
  fptr->fd2 = -1;
  fptr->pid = 0;
  fptr->buf = 0;
  fptr->readable = 0;
  fptr->writable = 0;
  fptr->sync = 0;
  fptr->eof = 0;
  fptr->is_socket = 0;
  return fptr;
}

#ifndef NOFILE
#define NOFILE 64
#endif

#ifdef MRB_NO_IO_POPEN
# define io_s_popen mrb_notimplement_m
#else
static int
option_to_fd(mrb_state *mrb, mrb_value v)
{
  if (mrb_undef_p(v)) return -1;
  if (mrb_nil_p(v)) return -1;

  switch (mrb_type(v)) {
    case MRB_TT_CDATA: /* IO */
      return mrb_io_fileno(mrb, v);
    case MRB_TT_INTEGER:
      return (int)mrb_integer(v);
    default:
      mrb_raise(mrb, E_ARGUMENT_ERROR, "wrong exec redirect action");
      break;
  }
  return -1; /* never reached */
}

static mrb_value
io_s_popen_args(mrb_state *mrb, mrb_value klass,
                    const char **cmd, int *flags, int *doexec,
                    int *opt_in, int *opt_out, int *opt_err)
{
  mrb_value mode = mrb_nil_value();
  struct { mrb_value opt_in, opt_out, opt_err; } kv;
  mrb_sym knames[3] = {MRB_SYM(in), MRB_SYM(out), MRB_SYM(err)};
  const mrb_kwargs kw = {
    3, 0,
    knames,
    &kv.opt_in,
    NULL,
  };

  mrb_get_args(mrb, "zo:", cmd, &mode, &kw);

  *flags = io_mode_to_flags(mrb, mode);
  *doexec = (strcmp("-", *cmd) != 0);
  *opt_in = option_to_fd(mrb, kv.opt_in);
  *opt_out = option_to_fd(mrb, kv.opt_out);
  *opt_err = option_to_fd(mrb, kv.opt_err);

  return mrb_obj_value(mrb_data_object_alloc(mrb, mrb_class_ptr(klass), NULL, &mrb_io_type));
}

#ifdef _WIN32
static mrb_value
io_s_popen(mrb_state *mrb, mrb_value klass)
{
  mrb_value io;
  int doexec;
  int opt_in, opt_out, opt_err;
  const char *cmd;

  struct mrb_io *fptr;
  int pid = 0, flags;
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  SECURITY_ATTRIBUTES saAttr;

  HANDLE ifd[2];
  HANDLE ofd[2];

  ifd[0] = INVALID_HANDLE_VALUE;
  ifd[1] = INVALID_HANDLE_VALUE;
  ofd[0] = INVALID_HANDLE_VALUE;
  ofd[1] = INVALID_HANDLE_VALUE;

  mrb->c->ci->mid = 0;
  io = io_s_popen_args(mrb, klass, &cmd, &flags, &doexec,
                       &opt_in, &opt_out, &opt_err);

  saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
  saAttr.bInheritHandle = TRUE;
  saAttr.lpSecurityDescriptor = NULL;

  if (OPEN_READABLE_P(flags)) {
    if (!CreatePipe(&ofd[0], &ofd[1], &saAttr, 0)
        || !SetHandleInformation(ofd[0], HANDLE_FLAG_INHERIT, 0)) {
      mrb_sys_fail(mrb, "pipe");
    }
  }

  if (OPEN_WRITABLE_P(flags)) {
    if (!CreatePipe(&ifd[0], &ifd[1], &saAttr, 0)
        || !SetHandleInformation(ifd[1], HANDLE_FLAG_INHERIT, 0)) {
      mrb_sys_fail(mrb, "pipe");
    }
  }

  if (doexec) {
    ZeroMemory(&pi, sizeof(pi));
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.dwFlags |= STARTF_USESTDHANDLES;
    if (OPEN_READABLE_P(flags)) {
      si.hStdOutput = ofd[1];
      si.hStdError = ofd[1];
    }
    if (OPEN_WRITABLE_P(flags)) {
      si.hStdInput = ifd[0];
    }
    if (!CreateProcess(
        NULL, (char*)cmd, NULL, NULL,
        TRUE, CREATE_NEW_PROCESS_GROUP, NULL, NULL, &si, &pi)) {
      CloseHandle(ifd[0]);
      CloseHandle(ifd[1]);
      CloseHandle(ofd[0]);
      CloseHandle(ofd[1]);
      mrb_raisef(mrb, E_IO_ERROR, "command not found: %s", cmd);
    }
    CloseHandle(pi.hThread);
    CloseHandle(ifd[0]);
    CloseHandle(ofd[1]);
    pid = pi.dwProcessId;
  }

  fptr = io_alloc(mrb);
  fptr->fd = _open_osfhandle((intptr_t)ofd[0], 0);
  fptr->fd2 = _open_osfhandle((intptr_t)ifd[1], 0);
  fptr->pid = pid;
  fptr->readable = OPEN_READABLE_P(flags);
  fptr->writable = OPEN_WRITABLE_P(flags);
  io_init_buf(mrb, fptr);

  DATA_TYPE(io) = &mrb_io_type;
  DATA_PTR(io)  = fptr;
  return io;
}
#else
static mrb_value
io_s_popen(mrb_state *mrb, mrb_value klass)
{
  mrb_value io, result;
  int doexec;
  int opt_in, opt_out, opt_err;
  const char *cmd;

  struct mrb_io *fptr;
  int pid, flags, fd, write_fd = -1;
  int pr[2] = { -1, -1 };
  int pw[2] = { -1, -1 };
  int saved_errno;

  mrb->c->ci->mid = 0;
  io = io_s_popen_args(mrb, klass, &cmd, &flags, &doexec,
                       &opt_in, &opt_out, &opt_err);

  if (OPEN_READABLE_P(flags)) {
    if (pipe(pr) == -1) {
      mrb_sys_fail(mrb, "pipe");
    }
    io_fd_cloexec(mrb, pr[0]);
    io_fd_cloexec(mrb, pr[1]);
  }

  if (OPEN_WRITABLE_P(flags)) {
    if (pipe(pw) == -1) {
      if (pr[0] != -1) close(pr[0]);
      if (pr[1] != -1) close(pr[1]);
      mrb_sys_fail(mrb, "pipe");
    }
    io_fd_cloexec(mrb, pw[0]);
    io_fd_cloexec(mrb, pw[1]);
  }

  if (!doexec) {
    // XXX
    fflush(stdin);
    fflush(stdout);
    fflush(stderr);
  }

  result = mrb_nil_value();
  switch (pid = fork()) {
    case 0: /* child */
      if (opt_in != -1) {
        dup2(opt_in, 0);
      }
      if (opt_out != -1) {
        dup2(opt_out, 1);
      }
      if (opt_err != -1) {
        dup2(opt_err, 2);
      }
      if (OPEN_READABLE_P(flags)) {
        close(pr[0]);
        if (pr[1] != 1) {
          dup2(pr[1], 1);
          close(pr[1]);
        }
      }
      if (OPEN_WRITABLE_P(flags)) {
        close(pw[1]);
        if (pw[0] != 0) {
          dup2(pw[0], 0);
          close(pw[0]);
        }
      }
      if (doexec) {
        for (fd = 3; fd < NOFILE; fd++) {
          close(fd);
        }
        io_process_exec(cmd);
        mrb_raisef(mrb, E_IO_ERROR, "command not found: %s", cmd);
        _exit(127);
      }
      result = mrb_nil_value();
      break;

    default: /* parent */
      if (OPEN_RDWR_P(flags)) {
        close(pr[1]);
        fd = pr[0];
        close(pw[0]);
        write_fd = pw[1];
      }
      else if (OPEN_RDONLY_P(flags)) {
        close(pr[1]);
        fd = pr[0];
      }
      else {
        close(pw[0]);
        fd = pw[1];
      }

      fptr = io_alloc(mrb);
      fptr->fd = fd;
      fptr->fd2 = write_fd;
      fptr->pid = pid;
      fptr->readable = OPEN_READABLE_P(flags);
      fptr->writable = OPEN_WRITABLE_P(flags);
      io_init_buf(mrb, fptr);

      DATA_TYPE(io) = &mrb_io_type;
      DATA_PTR(io)  = fptr;
      result = io;
      break;

    case -1: /* error */
      saved_errno = errno;
      if (OPEN_READABLE_P(flags)) {
        close(pr[0]);
        close(pr[1]);
      }
      if (OPEN_WRITABLE_P(flags)) {
        close(pw[0]);
        close(pw[1]);
      }
      errno = saved_errno;
      mrb_sys_fail(mrb, "pipe_open failed");
      break;
  }
  return result;
}
#endif /* _WIN32 */
#endif /* TARGET_OS_IPHONE */

static int
symdup(mrb_state *mrb, int fd, mrb_bool *failed)
{
  int new_fd;

  *failed = TRUE;
  if (fd < 0)
    return fd;

  new_fd = dup(fd);
  if (new_fd > 0) *failed = FALSE;
  return new_fd;
}

static mrb_value
io_init_copy(mrb_state *mrb, mrb_value copy)
{
  mrb_value orig = mrb_get_arg1(mrb);
  struct mrb_io *fptr_copy;
  struct mrb_io *fptr_orig;
  mrb_bool failed = TRUE;

  fptr_orig = io_get_open_fptr(mrb, orig);
  fptr_copy = (struct mrb_io*)DATA_PTR(copy);
  if (fptr_orig == fptr_copy) return copy;
  if (fptr_copy != NULL) {
    fptr_finalize(mrb, fptr_copy, FALSE);
    mrb_free(mrb, fptr_copy);
  }
  fptr_copy = (struct mrb_io*)io_alloc(mrb);
  fptr_copy->pid = fptr_orig->pid;
  fptr_copy->readable = fptr_orig->readable;
  fptr_copy->writable = fptr_orig->writable;
  fptr_copy->sync = fptr_orig->sync;
  fptr_copy->is_socket = fptr_orig->is_socket;

  io_init_buf(mrb, fptr_copy);

  DATA_TYPE(copy) = &mrb_io_type;
  DATA_PTR(copy) = fptr_copy;

  fptr_copy->fd = symdup(mrb, fptr_orig->fd, &failed);
  if (failed) {
    mrb_sys_fail(mrb, 0);
  }
  io_fd_cloexec(mrb, fptr_copy->fd);

  if (fptr_orig->fd2 != -1) {
    fptr_copy->fd2 = symdup(mrb, fptr_orig->fd2, &failed);
    if (failed) {
      close(fptr_copy->fd);
      mrb_sys_fail(mrb, 0);
    }
    io_fd_cloexec(mrb, fptr_copy->fd2);
  }

  return copy;
}

static void
check_file_descriptor(mrb_state *mrb, mrb_int fd)
{
  struct stat sb;
  int fdi = (int)fd;

#if MRB_INT_MIN < INT_MIN || MRB_INT_MAX > INT_MAX
  if (fdi != fd) {
    errno = EBADF;
    goto badfd;
  }
#endif

#ifdef _WIN32
  {
    DWORD err;
    int len = sizeof(err);

    if (getsockopt(fdi, SOL_SOCKET, SO_ERROR, (char*)&err, &len) == 0) {
      return;
    }
  }

  if (fdi < 0 || fdi > _getmaxstdio()) {
    errno = EBADF;
    goto badfd;
  }
#endif /* _WIN32 */

  if (fstat(fdi, &sb) == 0) return;
  if (errno == EBADF) goto badfd;
  return;

badfd:
  mrb_sys_fail(mrb, "bad file descriptor");
}

static mrb_value
io_init(mrb_state *mrb, mrb_value io)
{
  struct mrb_io *fptr;
  mrb_int fd;
  mrb_value mode, opt;          /* opt (Hash) will be ignored */
  int flags;

  mode = opt = mrb_nil_value();

  if (mrb_block_given_p(mrb)) {
    mrb_warn(mrb, "File.new() does not take block; use File.open() instead");
  }
  mrb_get_args(mrb, "i|oH", &fd, &mode, &opt);
  switch (fd) {
    case 0: /* STDIN_FILENO */
    case 1: /* STDOUT_FILENO */
    case 2: /* STDERR_FILENO */
      break;
    default:
      check_file_descriptor(mrb, fd);
      break;
  }
  flags = io_mode_to_flags(mrb, mode);

  fptr = (struct mrb_io*)DATA_PTR(io);
  if (fptr != NULL) {
    fptr_finalize(mrb, fptr, TRUE);
    mrb_free(mrb, fptr);
  }
  fptr = io_alloc(mrb);

  DATA_TYPE(io) = &mrb_io_type;
  DATA_PTR(io) = fptr;

  fptr->fd = (int)fd;
  fptr->readable = OPEN_READABLE_P(flags);
  fptr->writable = OPEN_WRITABLE_P(flags);
  io_init_buf(mrb, fptr);
  return io;
}

static void
fptr_finalize(mrb_state *mrb, struct mrb_io *fptr, int quiet)
{
  int saved_errno = 0;
  int limit = quiet ? 3 : 0;

  if (fptr == NULL) {
    return;
  }

  if (fptr->fd >= limit) {
#ifdef _WIN32
    if (fptr->is_socket) {
      if (closesocket(fptr->fd) != 0) {
        saved_errno = WSAGetLastError();
      }
      fptr->fd = -1;
    }
#endif
    if (fptr->fd != -1) {
      if (close(fptr->fd) == -1) {
        saved_errno = errno;
      }
    }
    fptr->fd = -1;
  }

  if (fptr->fd2 >= limit) {
    if (close(fptr->fd2) == -1) {
      if (saved_errno == 0) {
        saved_errno = errno;
      }
    }
    fptr->fd2 = -1;
  }

#ifndef MRB_NO_IO_POPEN
  if (fptr->pid != 0) {
#if !defined(_WIN32) && !defined(_WIN64)
    pid_t pid;
    int status;
    do {
      pid = waitpid(fptr->pid, &status, 0);
    } while (pid == -1 && errno == EINTR);
    if (!quiet && pid == fptr->pid) {
      io_set_process_status(mrb, pid, status);
    }
#else
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, fptr->pid);
    DWORD status;
    if (WaitForSingleObject(h, INFINITE) && GetExitCodeProcess(h, &status))
      if (!quiet)
        io_set_process_status(mrb, fptr->pid, (int)status);
    CloseHandle(h);
#endif
    fptr->pid = 0;
    /* Note: we don't raise an exception when waitpid(3) fails */
  }
#endif

  if (fptr->buf) {
    mrb_free(mrb, fptr->buf);
    fptr->buf = NULL;
  }

  if (!quiet && saved_errno != 0) {
    errno = saved_errno;
    mrb_sys_fail(mrb, "fptr_finalize failed");
  }
}

static struct mrb_io*
io_get_read_fptr(mrb_state *mrb, mrb_value io)
{
  struct mrb_io *fptr = io_get_open_fptr(mrb, io);
  if (!fptr->readable) {
    mrb_raise(mrb, E_IO_ERROR, "not opened for reading");
  }
  return fptr;
}

static struct mrb_io*
io_get_write_fptr(mrb_state *mrb, mrb_value io)
{
  struct mrb_io *fptr = io_get_open_fptr(mrb, io);
  if (!fptr->writable) {
    mrb_raise(mrb, E_IO_ERROR, "not opened for writing");
  }
  return fptr;
}

static int
io_get_write_fd(struct mrb_io *fptr)
{
  if (fptr->fd2 == -1) {
    return fptr->fd;
  }
  else {
    return fptr->fd2;
  }
}

static mrb_value
io_isatty(mrb_state *mrb, mrb_value io)
{
  struct mrb_io *fptr;

  fptr = io_get_open_fptr(mrb, io);
  if (isatty(fptr->fd) == 0)
    return mrb_false_value();
  return mrb_true_value();
}

static mrb_value
io_s_for_fd(mrb_state *mrb, mrb_value klass)
{
  struct RClass *c = mrb_class_ptr(klass);
  enum mrb_vtype ttype = MRB_INSTANCE_TT(c);
  mrb_value obj;

  /* copied from mrb_instance_alloc() */
  if (ttype == 0) ttype = MRB_TT_OBJECT;
  obj = mrb_obj_value((struct RObject*)mrb_obj_alloc(mrb, ttype, c));
  return io_init(mrb, obj);
}

static mrb_value
io_s_sysclose(mrb_state *mrb, mrb_value klass)
{
  mrb_int fd;
  mrb->c->ci->mid = 0;
  mrb_get_args(mrb, "i", &fd);
  if (close((int)fd) == -1) {
    mrb_sys_fail(mrb, "close");
  }
  return mrb_fixnum_value(0);
}

static int
io_cloexec_open(mrb_state *mrb, const char *pathname, int flags, fmode_t mode)
{
  int fd, retry = FALSE;
  char* fname = mrb_locale_from_utf8(pathname, -1);

#ifdef O_CLOEXEC
  /* O_CLOEXEC is available since Linux 2.6.23.  Linux 2.6.18 silently ignore it. */
  flags |= O_CLOEXEC;
#elif defined O_NOINHERIT
  flags |= O_NOINHERIT;
#endif
reopen:
  fd = open(fname, flags, mode);
  if (fd == -1) {
    if (!retry) {
      switch (errno) {
        case ENFILE:
        case EMFILE:
        mrb_garbage_collect(mrb);
        retry = TRUE;
        goto reopen;
      }
    }

    mrb_sys_fail(mrb, RSTRING_CSTR(mrb, mrb_format(mrb, "open %s", pathname)));
  }
  mrb_locale_free(fname);

  if (fd <= 2) {
    io_fd_cloexec(mrb, fd);
  }
  return fd;
}

static mrb_value
io_s_sysopen(mrb_state *mrb, mrb_value klass)
{
  mrb_value path = mrb_nil_value();
  mrb_value mode = mrb_nil_value();
  mrb_int fd, perm = -1;
  const char *pat;
  int flags;

  mrb_get_args(mrb, "S|oi", &path, &mode, &perm);
  if (perm < 0) {
    perm = 0666;
  }

  pat = RSTRING_CSTR(mrb, path);
  flags = io_mode_to_flags(mrb, mode);
  fd = io_cloexec_open(mrb, pat, flags, (fmode_t)perm);
  return mrb_fixnum_value(fd);
}

static void
eof_error(mrb_state *mrb)
{
  mrb_raise(mrb, E_EOF_ERROR, "end of file reached");
}

static mrb_value
io_read_common(mrb_state *mrb,
    fssize_t (*readfunc)(int, void*, fsize_t, off_t),
    mrb_value io, mrb_value buf, mrb_int maxlen, off_t offset)
{
  int ret;

  if (maxlen < 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "negative expanding string size");
  }
  else if (maxlen == 0) {
    return mrb_str_new(mrb, NULL, maxlen);
  }

  if (mrb_nil_p(buf)) {
    buf = mrb_str_new(mrb, NULL, maxlen);
  }

  if (RSTRING_LEN(buf) != maxlen) {
    buf = mrb_str_resize(mrb, buf, maxlen);
  }
  else {
    mrb_str_modify(mrb, RSTRING(buf));
  }

  struct mrb_io *fptr = io_get_read_fptr(mrb, io);
  ret = readfunc(fptr->fd, RSTRING_PTR(buf), (fsize_t)maxlen, offset);
  if (ret < 0) {
    mrb_sys_fail(mrb, "sysread failed");
  }
  if (RSTRING_LEN(buf) != ret) {
    buf = mrb_str_resize(mrb, buf, ret);
  }
  if (ret == 0 && maxlen > 0) {
    fptr->eof = 1;
    eof_error(mrb);
  }
  return buf;
}

static fssize_t
sysread(int fd, void *buf, fsize_t nbytes, off_t offset)
{
  return (fssize_t)read(fd, buf, nbytes);
}

static mrb_value
io_sysread(mrb_state *mrb, mrb_value io)
{
  mrb_value buf = mrb_nil_value();
  mrb_int maxlen;

  mrb_get_args(mrb, "i|S", &maxlen, &buf);

  return io_read_common(mrb, sysread, io, buf, maxlen, 0);
}

static mrb_value
io_sysseek(mrb_state *mrb, mrb_value io)
{
  struct mrb_io *fptr;
  off_t pos;
  mrb_int offset, whence = -1;

  mrb_get_args(mrb, "i|i", &offset, &whence);
  if (whence < 0) {
    whence = 0;
  }

  fptr = io_get_open_fptr(mrb, io);
  pos = lseek(fptr->fd, (off_t)offset, (int)whence);
  if (pos == -1) {
    mrb_sys_fail(mrb, "sysseek");
  }
  fptr->eof = 0;
  if (sizeof(off_t) > sizeof(mrb_int) && pos > (off_t)MRB_INT_MAX) {
    mrb_raise(mrb, E_IO_ERROR, "sysseek reached too far for mrb_int");
  }
  return mrb_int_value(mrb, (mrb_int)pos);
}

static mrb_value
io_seek(mrb_state *mrb, mrb_value io)
{
  mrb_value pos = io_sysseek(mrb, io);
  struct mrb_io *fptr = io_get_open_fptr(mrb, io);
  if (fptr->buf) {
    fptr->buf->start = 0;
    fptr->buf->len = 0;
  }
  return pos;
}

static mrb_value
io_write_common(mrb_state *mrb,
    fssize_t (*writefunc)(int, const void*, fsize_t, off_t),
    struct mrb_io *fptr, const void *buf, mrb_ssize blen, off_t offset)
{
  int fd;
  fssize_t length;

  fd = io_get_write_fd(fptr);
  length = writefunc(fd, buf, (fsize_t)blen, offset);
  if (length == -1) {
    mrb_sys_fail(mrb, "syswrite");
  }
  return mrb_int_value(mrb, (mrb_int)length);
}

static fssize_t
syswrite(int fd, const void *buf, fsize_t nbytes, off_t offset)
{
  return (fssize_t)write(fd, buf, nbytes);
}

static mrb_value
io_syswrite(mrb_state *mrb, mrb_value io)
{
  mrb_value buf;

  mrb_get_args(mrb, "S", &buf);

  return io_write_common(mrb, syswrite, io_get_write_fptr(mrb, io), RSTRING_PTR(buf), RSTRING_LEN(buf), 0);
}

  /* def write(string) */
  /*   str = string.is_a?(String) ? string : string.to_s */
  /*   return 0 if str.empty? */
  /*   len = syswrite(str) */
  /*   len */
  /* end */

static mrb_int
fd_write(mrb_state *mrb, int fd, mrb_value str)
{
  fssize_t len, sum, n;

  str = mrb_obj_as_string(mrb, str);
  len = (fssize_t)RSTRING_LEN(str);
  if (len == 0)return 0;

  for (sum=0; sum<len; sum+=n) {
    n = write(fd, RSTRING_PTR(str), (fsize_t)len);
    if (n == -1) {
      mrb_sys_fail(mrb, "syswrite");
    }
  }
  return len;
}

static mrb_value
io_write(mrb_state *mrb, mrb_value io)
{
  struct mrb_io *fptr = io_get_write_fptr(mrb, io);
  int fd = io_get_write_fd(fptr);
  mrb_int len = 0;

  if (fptr->buf && fptr->buf->len > 0) {
    off_t n;

    /* get current position */
    n = lseek(fd, 0, SEEK_CUR);
    if (n == -1) mrb_sys_fail(mrb, "lseek");
    /* move cursor */
    n = lseek(fd, n - fptr->buf->len, SEEK_SET);
    if (n == -1) mrb_sys_fail(mrb, "lseek(2)");
    fptr->buf->start = fptr->buf->len = 0;
  }

  if (mrb_get_argc(mrb) == 1) {
    len = fd_write(mrb, fd, mrb_get_arg1(mrb));
  }
  else {
    mrb_value *argv;
    mrb_int argc;

    mrb_get_args(mrb, "*", &argv, &argc);
    while (argc--) {
      len += fd_write(mrb, fd, *argv++);
    }
  }
  return mrb_int_value(mrb, len);
}

static mrb_value
io_close(mrb_state *mrb, mrb_value io)
{
  struct mrb_io *fptr;
  fptr = io_get_open_fptr(mrb, io);
  fptr_finalize(mrb, fptr, FALSE);
  return mrb_nil_value();
}

static mrb_value
io_close_write(mrb_state *mrb, mrb_value io)
{
  struct mrb_io *fptr;
  fptr = io_get_open_fptr(mrb, io);
  if (close((int)fptr->fd2) == -1) {
    mrb_sys_fail(mrb, "close");
  }
  return mrb_nil_value();
}

static mrb_value
io_closed(mrb_state *mrb, mrb_value io)
{
  struct mrb_io *fptr;
  fptr = (struct mrb_io*)mrb_data_get_ptr(mrb, io, &mrb_io_type);
  if (fptr == NULL || fptr->fd >= 0) {
    return mrb_false_value();
  }

  return mrb_true_value();
}

static mrb_value
io_pos(mrb_state *mrb, mrb_value io)
{
  struct mrb_io *fptr = io_get_open_fptr(mrb, io);
  off_t pos = lseek(fptr->fd, 0, SEEK_CUR);
  if (pos == -1) mrb_sys_fail(mrb, 0);

  if (fptr->buf) {
    return mrb_int_value(mrb, pos - fptr->buf->len);
  }
  else {
    return mrb_int_value(mrb, pos);
  }
}

static mrb_value
io_pid(mrb_state *mrb, mrb_value io)
{
  struct mrb_io *fptr;
  fptr = io_get_open_fptr(mrb, io);

  if (fptr->pid > 0) {
    return mrb_fixnum_value(fptr->pid);
  }

  return mrb_nil_value();
}

static struct timeval
time2timeval(mrb_state *mrb, mrb_value time)
{
  struct timeval t = { 0, 0 };

  switch (mrb_type(time)) {
    case MRB_TT_INTEGER:
      t.tv_sec = (ftime_t)mrb_integer(time);
      t.tv_usec = 0;
      break;

#ifndef MRB_NO_FLOAT
    case MRB_TT_FLOAT:
      t.tv_sec = (ftime_t)mrb_float(time);
      t.tv_usec = (fsuseconds_t)((mrb_float(time) - t.tv_sec) * 1000000.0);
      break;
#endif

    default:
      mrb_raise(mrb, E_TYPE_ERROR, "wrong argument class");
  }

  return t;
}

#if !defined(_WIN32) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
static mrb_value
io_s_pipe(mrb_state *mrb, mrb_value klass)
{
  mrb_value r = mrb_nil_value();
  mrb_value w = mrb_nil_value();
  struct mrb_io *fptr_r;
  struct mrb_io *fptr_w;
  int pipes[2];

  if (io_pipe(mrb, pipes) == -1) {
    mrb_sys_fail(mrb, "pipe");
  }

  r = mrb_obj_value(mrb_data_object_alloc(mrb, mrb_class_ptr(klass), NULL, &mrb_io_type));
  fptr_r = io_alloc(mrb);
  fptr_r->fd = pipes[0];
  fptr_r->readable = 1;
  DATA_TYPE(r) = &mrb_io_type;
  DATA_PTR(r)  = fptr_r;
  io_init_buf(mrb, fptr_r);

  w = mrb_obj_value(mrb_data_object_alloc(mrb, mrb_class_ptr(klass), NULL, &mrb_io_type));
  fptr_w = io_alloc(mrb);
  fptr_w->fd = pipes[1];
  fptr_w->writable = 1;
  fptr_w->sync = 1;
  DATA_TYPE(w) = &mrb_io_type;
  DATA_PTR(w)  = fptr_w;

  return mrb_assoc_new(mrb, r, w);
}
#endif

static int
mrb_io_read_data_pending(mrb_state *mrb, struct mrb_io *fptr)
{
  if (fptr->buf && fptr->buf->len > 0) return 1;
  return 0;
}

static mrb_value
io_s_select(mrb_state *mrb, mrb_value klass)
{
  const mrb_value *argv;
  mrb_int argc;
  mrb_value read, read_io, write, except, timeout, list;
  struct timeval *tp, timerec;
  fd_set pset, rset, wset, eset;
  fd_set *rp, *wp, *ep;
  struct mrb_io *fptr;
  int pending = 0;
  mrb_value result;
  int max = 0;
  int interrupt_flag = 0;
  int i, n;

  mrb_get_args(mrb, "*", &argv, &argc);

  if (argc < 1 || argc > 4) {
    mrb_argnum_error(mrb, argc, 1, 4);
  }

  timeout = mrb_nil_value();
  except = mrb_nil_value();
  write = mrb_nil_value();
  if (argc > 3)
    timeout = argv[3];
  if (argc > 2)
    except = argv[2];
  if (argc > 1)
    write = argv[1];
  read = argv[0];

  if (mrb_nil_p(timeout)) {
    tp = NULL;
  }
  else {
    timerec = time2timeval(mrb, timeout);
    tp = &timerec;
  }

  FD_ZERO(&pset);
  if (!mrb_nil_p(read)) {
    mrb_check_type(mrb, read, MRB_TT_ARRAY);
    rp = &rset;
    FD_ZERO(rp);
    for (i = 0; i < RARRAY_LEN(read); i++) {
      read_io = RARRAY_PTR(read)[i];
      fptr = io_get_open_fptr(mrb, read_io);
      if (fptr->fd >= FD_SETSIZE) continue;
      FD_SET(fptr->fd, rp);
      if (mrb_io_read_data_pending(mrb, fptr)) {
        pending++;
        FD_SET(fptr->fd, &pset);
      }
      if (max < fptr->fd)
        max = fptr->fd;
    }
    if (pending) {
      timerec.tv_sec = timerec.tv_usec = 0;
      tp = &timerec;
    }
  }
  else {
    rp = NULL;
  }

  if (!mrb_nil_p(write)) {
    mrb_check_type(mrb, write, MRB_TT_ARRAY);
    wp = &wset;
    FD_ZERO(wp);
    for (i = 0; i < RARRAY_LEN(write); i++) {
      fptr = io_get_open_fptr(mrb, RARRAY_PTR(write)[i]);
      if (fptr->fd >= FD_SETSIZE) continue;
      FD_SET(fptr->fd, wp);
      if (max < fptr->fd)
        max = fptr->fd;
      if (fptr->fd2 >= 0) {
        FD_SET(fptr->fd2, wp);
        if (max < fptr->fd2)
          max = fptr->fd2;
      }
    }
  }
  else {
    wp = NULL;
  }

  if (!mrb_nil_p(except)) {
    mrb_check_type(mrb, except, MRB_TT_ARRAY);
    ep = &eset;
    FD_ZERO(ep);
    for (i = 0; i < RARRAY_LEN(except); i++) {
      fptr = io_get_open_fptr(mrb, RARRAY_PTR(except)[i]);
      if (fptr->fd >= FD_SETSIZE) continue;
      FD_SET(fptr->fd, ep);
      if (max < fptr->fd)
        max = fptr->fd;
      if (fptr->fd2 >= 0) {
        FD_SET(fptr->fd2, ep);
        if (max < fptr->fd2)
          max = fptr->fd2;
      }
    }
  }
  else {
    ep = NULL;
  }

  max++;

retry:
  n = select(max, rp, wp, ep, tp);
  if (n < 0) {
#ifdef _WIN32
    errno = WSAGetLastError();
    if (errno != WSAEINTR)
      mrb_sys_fail(mrb, "select failed");
#else
    if (errno != EINTR)
      mrb_sys_fail(mrb, "select failed");
#endif
    if (tp == NULL)
      goto retry;
    interrupt_flag = 1;
  }

  if (!pending && n == 0)
    return mrb_nil_value();

  result = mrb_ary_new_capa(mrb, 3);
  mrb_ary_push(mrb, result, rp? mrb_ary_new(mrb) : mrb_ary_new_capa(mrb, 0));
  mrb_ary_push(mrb, result, wp? mrb_ary_new(mrb) : mrb_ary_new_capa(mrb, 0));
  mrb_ary_push(mrb, result, ep? mrb_ary_new(mrb) : mrb_ary_new_capa(mrb, 0));

  if (interrupt_flag == 0) {
    if (rp) {
      list = RARRAY_PTR(result)[0];
      for (i = 0; i < RARRAY_LEN(read); i++) {
        fptr = io_get_open_fptr(mrb, RARRAY_PTR(read)[i]);
        if (FD_ISSET(fptr->fd, rp) ||
            FD_ISSET(fptr->fd, &pset)) {
          mrb_ary_push(mrb, list, RARRAY_PTR(read)[i]);
        }
      }
    }

    if (wp) {
      list = RARRAY_PTR(result)[1];
      for (i = 0; i < RARRAY_LEN(write); i++) {
        fptr = io_get_open_fptr(mrb, RARRAY_PTR(write)[i]);
        if (FD_ISSET(fptr->fd, wp)) {
          mrb_ary_push(mrb, list, RARRAY_PTR(write)[i]);
        }
        else if (fptr->fd2 >= 0 && FD_ISSET(fptr->fd2, wp)) {
          mrb_ary_push(mrb, list, RARRAY_PTR(write)[i]);
        }
      }
    }

    if (ep) {
      list = RARRAY_PTR(result)[2];
      for (i = 0; i < RARRAY_LEN(except); i++) {
        fptr = io_get_open_fptr(mrb, RARRAY_PTR(except)[i]);
        if (FD_ISSET(fptr->fd, ep)) {
          mrb_ary_push(mrb, list, RARRAY_PTR(except)[i]);
        }
        else if (fptr->fd2 >= 0 && FD_ISSET(fptr->fd2, ep)) {
          mrb_ary_push(mrb, list, RARRAY_PTR(except)[i]);
        }
      }
    }
  }

  return result;
}

int
mrb_io_fileno(mrb_state *mrb, mrb_value io)
{
  struct mrb_io *fptr;
  fptr = io_get_open_fptr(mrb, io);
  return fptr->fd;
}

static mrb_value
io_fileno(mrb_state *mrb, mrb_value io)
{
  int fd = mrb_io_fileno(mrb, io);
  return mrb_fixnum_value(fd);
}

#if defined(F_GETFD) && defined(F_SETFD) && defined(FD_CLOEXEC)
static mrb_value
io_close_on_exec_p(mrb_state *mrb, mrb_value io)
{
  struct mrb_io *fptr;
  int ret;

  fptr = io_get_open_fptr(mrb, io);

  if (fptr->fd2 >= 0) {
    if ((ret = fcntl(fptr->fd2, F_GETFD)) == -1) mrb_sys_fail(mrb, "F_GETFD failed");
    if (!(ret & FD_CLOEXEC)) return mrb_false_value();
  }

  if ((ret = fcntl(fptr->fd, F_GETFD)) == -1) mrb_sys_fail(mrb, "F_GETFD failed");
  if (!(ret & FD_CLOEXEC)) return mrb_false_value();
  return mrb_true_value();
}
#else
# define io_close_on_exec_p mrb_notimplement_m
#endif

#if defined(F_GETFD) && defined(F_SETFD) && defined(FD_CLOEXEC)
static mrb_value
io_set_close_on_exec(mrb_state *mrb, mrb_value io)
{
  struct mrb_io *fptr;
  int flag, ret;
  mrb_bool b;

  fptr = io_get_open_fptr(mrb, io);
  mrb_get_args(mrb, "b", &b);
  flag = b ? FD_CLOEXEC : 0;

  if (fptr->fd2 >= 0) {
    if ((ret = fcntl(fptr->fd2, F_GETFD)) == -1) mrb_sys_fail(mrb, "F_GETFD failed");
    if ((ret & FD_CLOEXEC) != flag) {
      ret = (ret & ~FD_CLOEXEC) | flag;
      ret = fcntl(fptr->fd2, F_SETFD, ret);

      if (ret == -1) mrb_sys_fail(mrb, "F_SETFD failed");
    }
  }

  if ((ret = fcntl(fptr->fd, F_GETFD)) == -1) mrb_sys_fail(mrb, "F_GETFD failed");
  if ((ret & FD_CLOEXEC) != flag) {
    ret = (ret & ~FD_CLOEXEC) | flag;
    ret = fcntl(fptr->fd, F_SETFD, ret);
    if (ret == -1) mrb_sys_fail(mrb, "F_SETFD failed");
  }

  return mrb_bool_value(b);
}
#else
# define io_set_close_on_exec mrb_notimplement_m
#endif

static mrb_value
io_set_sync(mrb_state *mrb, mrb_value io)
{
  struct mrb_io *fptr;
  mrb_bool b;

  fptr = io_get_open_fptr(mrb, io);
  mrb_get_args(mrb, "b", &b);
  fptr->sync = b;
  return mrb_bool_value(b);
}

static mrb_value
io_sync(mrb_state *mrb, mrb_value io)
{
  struct mrb_io *fptr;
  fptr = io_get_open_fptr(mrb, io);
  return mrb_bool_value(fptr->sync);
}

#ifndef MRB_WITH_IO_PREAD_PWRITE
# define io_pread   mrb_notimplement_m
# define io_pwrite  mrb_notimplement_m
#else
static off_t
value2off(mrb_state *mrb, mrb_value offv)
{
  return (off_t)mrb_as_int(mrb, offv);
}

/*
 * call-seq:
 *  pread(maxlen, offset, outbuf = "") -> outbuf
 */
static mrb_value
io_pread(mrb_state *mrb, mrb_value io)
{
  mrb_value buf = mrb_nil_value();
  mrb_value off;
  mrb_int maxlen;

  mrb_get_args(mrb, "io|S!", &maxlen, &off, &buf);

  return io_read_common(mrb, pread, io, buf, maxlen, value2off(mrb, off));
}

/*
 * call-seq:
 *  pwrite(buffer, offset) -> wrote_bytes
 */
static mrb_value
io_pwrite(mrb_state *mrb, mrb_value io)
{
  mrb_value buf, off;

  mrb_get_args(mrb, "So", &buf, &off);

  return io_write_common(mrb, pwrite, io_get_write_fptr(mrb, io), RSTRING_PTR(buf), RSTRING_LEN(buf), value2off(mrb, off));
}
#endif /* MRB_WITH_IO_PREAD_PWRITE */

static mrb_value
io_ungetc(mrb_state *mrb, mrb_value io)
{
  struct mrb_io *fptr = io_get_read_fptr(mrb, io);
  struct mrb_io_buf *buf = fptr->buf;
  mrb_value str;
  mrb_int len;

  mrb_get_args(mrb, "S", &str);
  len = RSTRING_LEN(str);
  if (len > SHRT_MAX) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "string too long to ungetc");
  }
  if (len > MRB_IO_BUF_SIZE - buf->len) {
    fptr->buf = (struct mrb_io_buf*)mrb_realloc(mrb, buf, sizeof(struct mrb_io_buf)+buf->len+len-MRB_IO_BUF_SIZE);
    buf = fptr->buf;
  }
  memmove(buf->mem+len, buf->mem+buf->start, buf->len);
  memcpy(buf->mem, RSTRING_PTR(str), len);
  buf->start = 0;
  buf->len += (short)len;
  return mrb_nil_value();
}

static void
io_buf_reset(struct mrb_io_buf *buf)
{
  buf->start = 0;
  buf->len = 0;
}

static void
io_buf_shift(struct mrb_io_buf *buf, mrb_int n)
{
  mrb_assert(n <= SHRT_MAX);
  buf->start += (short)n;
  buf->len -= (short)n;
}

#ifdef MRB_UTF8_STRING
static void
io_fill_buf_comp(mrb_state *mrb, struct mrb_io *fptr)
{
  struct mrb_io_buf *buf = fptr->buf;
  int keep = buf->len;

  memmove(buf->mem, buf->mem+buf->start, keep);
  int n = read(fptr->fd, buf->mem+keep, MRB_IO_BUF_SIZE-keep);
  if (n < 0) mrb_sys_fail(mrb, 0);
  if (n == 0) fptr->eof = 1;
  buf->start = 0;
  buf->len += (short)n;
}
#endif

static void
io_fill_buf(mrb_state *mrb, struct mrb_io *fptr)
{
  struct mrb_io_buf *buf = fptr->buf;

  if (buf->len > 0) return;

  int n = read(fptr->fd, buf->mem, MRB_IO_BUF_SIZE);
  if (n < 0) mrb_sys_fail(mrb, 0);
  if (n == 0) fptr->eof = 1;
  buf->start = 0;
  buf->len = (short)n;
}

static mrb_value
io_eof(mrb_state *mrb, mrb_value io)
{
  struct mrb_io *fptr = io_get_read_fptr(mrb, io);

  if (fptr->eof) return mrb_true_value();
  if (fptr->buf->len > 0) return mrb_false_value();
  io_fill_buf(mrb, fptr);
  return mrb_bool_value(fptr->eof);
}

static void
io_buf_cat(mrb_state *mrb, mrb_value outbuf, struct mrb_io_buf *buf, mrb_int n)
{
  mrb_assert(n <= buf->len);
  mrb_str_cat(mrb, outbuf, buf->mem+buf->start, n);
  io_buf_shift(buf, n);
}

static void
io_buf_cat_all(mrb_state *mrb, mrb_value outbuf, struct mrb_io_buf *buf)
{
  mrb_str_cat(mrb, outbuf, buf->mem+buf->start, buf->len);
  io_buf_reset(buf);
}

static mrb_value
io_read_all(mrb_state *mrb, struct mrb_io *fptr, mrb_value outbuf)
{
  for (;;) {
    io_fill_buf(mrb, fptr);
    if (fptr->eof) {
      return outbuf;
    }
    io_buf_cat_all(mrb, outbuf, fptr->buf);
  }
}

static mrb_value
io_reset_outbuf(mrb_state *mrb, mrb_value outbuf, mrb_int len)
{
  if (mrb_nil_p(outbuf)) {
    outbuf = mrb_str_new(mrb, NULL, 0);
  }
  else {
    mrb_str_modify(mrb, mrb_str_ptr(outbuf));
    RSTR_SET_LEN(mrb_str_ptr(outbuf), 0);
  }
  return outbuf;
}

static mrb_value
io_read(mrb_state *mrb, mrb_value io)
{
  mrb_value outbuf = mrb_nil_value();
  mrb_value len;
  mrb_int length = 0;
  mrb_bool length_given;
  struct mrb_io *fptr = io_get_read_fptr(mrb, io);

  mrb_get_args(mrb, "|o?S", &len, &length_given, &outbuf);
  if (length_given) {
    if (mrb_nil_p(len)) {
      length_given = FALSE;
    }
    else {
      length = mrb_as_int(mrb, len);
      if (length < 0) {
        mrb_raisef(mrb, E_ARGUMENT_ERROR, "negative length %d given", length);
      }
      if (length == 0) {
        return io_reset_outbuf(mrb, outbuf, 0);
      }
    }
  }

  outbuf = io_reset_outbuf(mrb, outbuf, MRB_IO_BUF_SIZE);
  if (!length_given) {          /* read as much as possible */
    return io_read_all(mrb, fptr, outbuf);
  }

  struct mrb_io_buf *buf = fptr->buf;

  for (;;) {
    io_fill_buf(mrb, fptr);
    if (fptr->eof || length == 0) {
      if (RSTRING_LEN(outbuf) == 0)
        return mrb_nil_value();
      return outbuf;
    }
    if (buf->len < length) {
      length -= buf->len;
      io_buf_cat_all(mrb, outbuf, buf);
    }
    else {
      io_buf_cat(mrb, outbuf, buf, length);
      return outbuf;
    }
  }
}

static mrb_int
io_find_index(struct mrb_io *fptr, const char *rs, mrb_int rslen)
{
  struct mrb_io_buf *buf = fptr->buf;

  mrb_assert(rslen > 0);
  const char c = rs[0];
  const mrb_int limit = buf->len - rslen + 1;
  const char *p = buf->mem+buf->start;
  for (mrb_int i=0; i<limit; i++) {
    if (p[i] == c && (rslen == 1 || memcmp(p+i, rs, rslen) == 0)) {
      return i;
    }
  }
  return -1;
}

static mrb_value
io_gets(mrb_state *mrb, mrb_value io)
{
  mrb_value rs = mrb_nil_value();
  mrb_int limit;
  mrb_bool rs_given = FALSE;    /* newline break */
  mrb_bool limit_given = FALSE; /* no limit */
  mrb_value outbuf;
  struct mrb_io *fptr = io_get_read_fptr(mrb, io);
  struct mrb_io_buf *buf = fptr->buf;

  mrb_get_args(mrb, "|o?i?", &rs, &rs_given, &limit, &limit_given);

  if (limit_given == FALSE) {
    if (rs_given) {
      if (mrb_nil_p(rs)) {
        rs_given = FALSE;
      }
      else if (mrb_integer_p(rs)) {
        limit = mrb_integer(rs);
        limit_given = TRUE;
        rs = mrb_nil_value();
      }
      else if (!mrb_string_p(rs)) {
        mrb_ensure_int_type(mrb, rs);
      }
    }
  }
  if (rs_given) {
    if (mrb_nil_p(rs)) {
      rs_given = FALSE;
    }
    else {
      mrb_ensure_string_type(mrb, rs);
      if (RSTRING_LEN(rs) == 0) { /* paragraph mode */
        rs = mrb_str_new_lit(mrb, "\n\n");
      }
    }
  }
  else {
    rs = mrb_str_new_lit(mrb, "\n");
    rs_given = TRUE;
  }

  /* from now on rs_given==FALSE means no RS */
  if (mrb_nil_p(rs) && !limit_given) {
    return io_read_all(mrb, fptr, mrb_str_new_capa(mrb, MRB_IO_BUF_SIZE));
  }

  io_fill_buf(mrb, fptr);
  if (fptr->eof) return mrb_nil_value();

  if (limit_given) {
    if (limit == 0) return mrb_str_new(mrb, NULL, 0);
    outbuf = mrb_str_new_capa(mrb, limit);
  }
  else {
    outbuf = mrb_str_new(mrb, NULL, 0);
  }

  for (;;) {
    if (rs_given) {                /* with RS */
      int rslen = RSTRING_LEN(rs);
      mrb_int idx = io_find_index(fptr, RSTRING_PTR(rs), rslen);
      if (idx >= 0) {              /* found */
        mrb_int n = idx+rslen;
        if (limit_given && limit < n) {
          n = limit;
        }
        io_buf_cat(mrb, outbuf, buf, n);
        return outbuf;
      }
    }
    if (limit_given) {
      if (limit <= buf->len) {
        io_buf_cat(mrb, outbuf, buf, limit);
        return outbuf;
      }
      limit -= buf->len;
    }
    io_buf_cat_all(mrb, outbuf, buf);
    io_fill_buf(mrb, fptr);
    if (fptr->eof) {
      if (RSTRING_LEN(outbuf) == 0) return mrb_nil_value();
      return outbuf;
    }
  }
}

static mrb_value
io_readline(mrb_state *mrb, mrb_value io)
{
  mrb_value result = io_gets(mrb, io);
  if (mrb_nil_p(result)) {
    eof_error(mrb);
  }
  return result;
}

static mrb_value
io_readlines(mrb_state *mrb, mrb_value io)
{
  mrb_value ary = mrb_ary_new(mrb);
  for (;;) {
    mrb_value line = io_gets(mrb, io);

    if (mrb_nil_p(line)) return ary;
    mrb_ary_push(mrb, ary, line);
  }
}

static mrb_value
io_getc(mrb_state *mrb, mrb_value io)
{
  mrb_int len = 1;
  struct mrb_io *fptr = io_get_read_fptr(mrb, io);
  struct mrb_io_buf *buf = fptr->buf;

  io_fill_buf(mrb, fptr);
  if (fptr->eof) return mrb_nil_value();
#ifdef MRB_UTF8_STRING
  const char *p = &buf->mem[buf->start];
  if ((*p) & 0x80) {
    len = mrb_utf8len(p, p+buf->len);
    if (len == 1 && buf->len < 4) { /* partial UTF-8 */
      io_fill_buf_comp(mrb, fptr);
      p = &buf->mem[buf->start];
      len = mrb_utf8len(p, p+buf->len);
    }
  }
#endif
  mrb_value str = mrb_str_new(mrb, buf->mem+buf->start, len);
  io_buf_shift(buf, len);
  return str;
}

static mrb_value
io_readchar(mrb_state *mrb, mrb_value io)
{
  mrb_value result = io_getc(mrb, io);
  if (mrb_nil_p(result)) {
    eof_error(mrb);
  }
  return result;
}

static mrb_value
io_getbyte(mrb_state *mrb, mrb_value io)
{
  struct mrb_io *fptr = io_get_read_fptr(mrb, io);
  struct mrb_io_buf *buf = fptr->buf;

  io_fill_buf(mrb, fptr);
  if (fptr->eof) return mrb_nil_value();

  unsigned char c = buf->mem[buf->start];
  io_buf_shift(buf, 1);
  return mrb_int_value(mrb, (mrb_int)c);
}

static mrb_value
io_readbyte(mrb_state *mrb, mrb_value io)
{
  mrb_value result = io_getbyte(mrb, io);
  if (mrb_nil_p(result)) {
    eof_error(mrb);
  }
  return result;
}

static mrb_value
io_flush(mrb_state *mrb, mrb_value io)
{
  io_get_open_fptr(mrb, io);
  return io;
}

void
mrb_init_io(mrb_state *mrb)
{
  struct RClass *io;

  io      = mrb_define_class(mrb, "IO", mrb->object_class);
  MRB_SET_INSTANCE_TT(io, MRB_TT_CDATA);

  mrb_include_module(mrb, io, mrb_module_get(mrb, "Enumerable")); /* 15.2.20.3 */
  mrb_define_class_method(mrb, io, "_popen",  io_s_popen,   MRB_ARGS_ARG(1,2));
  mrb_define_class_method(mrb, io, "_sysclose",  io_s_sysclose, MRB_ARGS_REQ(1));
  mrb_define_class_method(mrb, io, "for_fd",  io_s_for_fd,   MRB_ARGS_ARG(1,2));
  mrb_define_class_method(mrb, io, "select",  io_s_select,  MRB_ARGS_ARG(1,3));
  mrb_define_class_method(mrb, io, "sysopen", io_s_sysopen, MRB_ARGS_ARG(1,2));
#if !defined(_WIN32) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
  mrb_define_class_method(mrb, io, "_pipe", io_s_pipe, MRB_ARGS_NONE());
#endif

  mrb_define_method(mrb, io, "initialize",      io_init, MRB_ARGS_ARG(1,2));
  mrb_define_method(mrb, io, "initialize_copy", io_init_copy, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, io, "isatty",     io_isatty,     MRB_ARGS_NONE());
  mrb_define_method(mrb, io, "eof?",       io_eof,        MRB_ARGS_NONE());   /* 15.2.20.5.6 */
  mrb_define_method(mrb, io, "getc",       io_getc,       MRB_ARGS_NONE());   /* 15.2.20.5.8 */
  mrb_define_method(mrb, io, "gets",       io_gets,       MRB_ARGS_OPT(2));   /* 15.2.20.5.9 */
  mrb_define_method(mrb, io, "read",       io_read,       MRB_ARGS_OPT(2));   /* 15.2.20.5.14 */
  mrb_define_method(mrb, io, "readchar",   io_readchar,   MRB_ARGS_NONE());   /* 15.2.20.5.15 */
  mrb_define_method(mrb, io, "readline",   io_readline,   MRB_ARGS_OPT(2));   /* 15.2.20.5.16 */
  mrb_define_method(mrb, io, "readlines",  io_readlines,  MRB_ARGS_OPT(2));   /* 15.2.20.5.17 */
  mrb_define_method(mrb, io, "sync",       io_sync,       MRB_ARGS_NONE());   /* 15.2.20.5.18 */
  mrb_define_method(mrb, io, "sync=",      io_set_sync,   MRB_ARGS_REQ(1));   /* 15.2.20.5.19 */
  mrb_define_method(mrb, io, "sysread",    io_sysread,    MRB_ARGS_ARG(1,1));
  mrb_define_method(mrb, io, "sysseek",    io_sysseek,    MRB_ARGS_ARG(1,1));
  mrb_define_method(mrb, io, "syswrite",   io_syswrite,   MRB_ARGS_REQ(1));
  mrb_define_method(mrb, io, "seek",       io_seek,       MRB_ARGS_ARG(1,1));
  mrb_define_method(mrb, io, "close",      io_close,      MRB_ARGS_NONE());   /* 15.2.20.5.1 */
  mrb_define_method(mrb, io, "close_write",    io_close_write,       MRB_ARGS_NONE());
  mrb_define_method(mrb, io, "close_on_exec=", io_set_close_on_exec, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, io, "close_on_exec?", io_close_on_exec_p,   MRB_ARGS_NONE());
  mrb_define_method(mrb, io, "closed?",    io_closed,     MRB_ARGS_NONE());   /* 15.2.20.5.2 */
  mrb_define_method(mrb, io, "flush",      io_flush,      MRB_ARGS_NONE());   /* 15.2.20.5.7 */
  mrb_define_method(mrb, io, "ungetc",     io_ungetc,     MRB_ARGS_REQ(1));
  mrb_define_method(mrb, io, "pos",        io_pos,        MRB_ARGS_NONE());
  mrb_define_method(mrb, io, "pid",        io_pid,        MRB_ARGS_NONE());
  mrb_define_method(mrb, io, "fileno",     io_fileno,     MRB_ARGS_NONE());
  mrb_define_method(mrb, io, "write",      io_write,      MRB_ARGS_ANY());    /* 15.2.20.5.20 */
  mrb_define_method(mrb, io, "pread",      io_pread,      MRB_ARGS_ANY());    /* ruby 2.5 feature */
  mrb_define_method(mrb, io, "pwrite",     io_pwrite,     MRB_ARGS_ANY());    /* ruby 2.5 feature */
  mrb_define_method(mrb, io, "getbyte",    io_getbyte,    MRB_ARGS_NONE());
  mrb_define_method(mrb, io, "readbyte",   io_readbyte,   MRB_ARGS_NONE());

  mrb_define_const_id(mrb, io, MRB_SYM(SEEK_SET), mrb_fixnum_value(SEEK_SET));
  mrb_define_const_id(mrb, io, MRB_SYM(SEEK_CUR), mrb_fixnum_value(SEEK_CUR));
  mrb_define_const_id(mrb, io, MRB_SYM(SEEK_END), mrb_fixnum_value(SEEK_END));
}
