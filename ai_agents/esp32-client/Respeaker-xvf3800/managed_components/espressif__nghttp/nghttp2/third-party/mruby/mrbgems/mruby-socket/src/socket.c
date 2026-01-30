/*
** socket.c - Socket module
**
** See Copyright Notice in mruby.h
*/

#ifdef _WIN32
  #define _WIN32_WINNT 0x0501

  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  #include <winerror.h>

  #define SHUT_RDWR SD_BOTH
  typedef int fsize_t;
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <sys/param.h>
  #include <sys/un.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <arpa/inet.h>
  #include <fcntl.h>
  #include <netdb.h>
  #include <unistd.h>
  typedef size_t fsize_t;
#endif

#include <string.h>

#include "mruby.h"
#include "mruby/array.h"
#include "mruby/class.h"
#include "mruby/data.h"
#include "mruby/numeric.h"
#include "mruby/string.h"
#include "mruby/variable.h"
#include "mruby/error.h"
#include "mruby/internal.h"
#include "mruby/presym.h"

#include "mruby/ext/io.h"

#if !defined(HAVE_SA_LEN)
#if (defined(BSD) && (BSD >= 199006))
#define HAVE_SA_LEN  1
#else
#define HAVE_SA_LEN  0
#endif
#endif

#define E_SOCKET_ERROR             mrb_class_get_id(mrb, MRB_SYM(SocketError))

#ifdef _WIN32
static const char *inet_ntop(int af, const void *src, char *dst, socklen_t cnt)
{
  if (af == AF_INET) {
    struct sockaddr_in in = {0};

    in.sin_family = AF_INET;
    memcpy(&in.sin_addr, src, sizeof(struct in_addr));
    getnameinfo((struct sockaddr*)&in, sizeof(struct sockaddr_in),
                dst, cnt, NULL, 0, NI_NUMERICHOST);
    return dst;
  }
  else if (af == AF_INET6) {
    struct sockaddr_in6 in = {0};

    in.sin6_family = AF_INET6;
    memcpy(&in.sin6_addr, src, sizeof(struct in_addr6));
    getnameinfo((struct sockaddr*)&in, sizeof(struct sockaddr_in6),
                dst, cnt, NULL, 0, NI_NUMERICHOST);
    return dst;
  }
  return NULL;
}

static int inet_pton(int af, const char *src, void *dst)
{
  struct addrinfo hints = {0};
  struct addrinfo *res, *ressave;

  hints.ai_family = af;

  if (getaddrinfo(src, NULL, &hints, &res) != 0) {
    printf("Couldn't resolve host %s\n", src);
    return -1;
  }

  ressave = res;

  while (res) {
    memcpy(dst, res->ai_addr, res->ai_addrlen);
    res = res->ai_next;
  }

  freeaddrinfo(ressave);
  return 0;
}

#endif

struct gen_addrinfo_args {
  struct RClass *klass;
  struct addrinfo *addrinfo;
};

static mrb_value
gen_addrinfo(mrb_state *mrb, mrb_value args)
{
  mrb_value ary = mrb_ary_new(mrb);
  int arena_idx = mrb_gc_arena_save(mrb);  /* ary must be on arena! */
  struct gen_addrinfo_args *a = (struct gen_addrinfo_args*)mrb_cptr(args);

  for (struct addrinfo *res = a->addrinfo; res != NULL; res = res->ai_next) {
    mrb_value sa = mrb_str_new(mrb, (char*)res->ai_addr, res->ai_addrlen);
    mrb_value args[4] = {sa, mrb_fixnum_value(res->ai_family), mrb_fixnum_value(res->ai_socktype), mrb_fixnum_value(res->ai_protocol)};
    mrb_value ai = mrb_obj_new(mrb, a->klass, 4, args);
    mrb_ary_push(mrb, ary, ai);
    mrb_gc_arena_restore(mrb, arena_idx);
  }
  return ary;
}

static mrb_value
free_addrinfo(mrb_state *mrb, mrb_value addrinfo)
{
  freeaddrinfo((struct addrinfo*)mrb_cptr(addrinfo));
  return mrb_nil_value();
}

static mrb_value
mrb_addrinfo_getaddrinfo(mrb_state *mrb, mrb_value klass)
{
  struct addrinfo hints = {0}, *addr;
  mrb_value family, protocol, service, socktype;
  mrb_int flags;
  const char *hostname, *servname = NULL;

  family = socktype = protocol = mrb_nil_value();
  flags = 0;
  mrb_get_args(mrb, "z!o|oooi", &hostname, &service, &family, &socktype, &protocol, &flags);

  if (mrb_string_p(service)) {
    servname = RSTRING_CSTR(mrb, service);
  }
  else if (mrb_integer_p(service)) {
    servname = RSTRING_PTR(mrb_integer_to_str(mrb, service, 10));
  }
  else if (mrb_nil_p(service)) {
    servname = NULL;
  }
  else {
    mrb_raise(mrb, E_TYPE_ERROR, "service must be String, Integer, or nil");
  }

  hints.ai_flags = (int)flags;

  if (mrb_integer_p(family)) {
    hints.ai_family = (int)mrb_integer(family);
  }

  if (mrb_integer_p(socktype)) {
    hints.ai_socktype = (int)mrb_integer(socktype);
  }

  if (mrb_integer_p(protocol)) {
    hints.ai_protocol = (int)mrb_integer(protocol);
  }

  int error = getaddrinfo(hostname, servname, &hints, &addr);
  if (error) {
    mrb_raisef(mrb, E_SOCKET_ERROR, "getaddrinfo: %s", gai_strerror(error));
  }

  struct gen_addrinfo_args args = {mrb_class_ptr(klass), addr};
  return mrb_ensure(mrb, gen_addrinfo, mrb_cptr_value(mrb, &args), free_addrinfo, mrb_cptr_value(mrb, addr));
}

static mrb_value
mrb_addrinfo_getnameinfo(mrb_state *mrb, mrb_value self)
{
  mrb_int flags;
  mrb_value ary, host, sastr, serv;
  int error;

  flags = 0;
  mrb_get_args(mrb, "|i", &flags);
  host = mrb_str_new_capa(mrb, NI_MAXHOST);
  serv = mrb_str_new_capa(mrb, NI_MAXSERV);

  sastr = mrb_iv_get(mrb, self, MRB_IVSYM(sockaddr));
  if (!mrb_string_p(sastr)) {
    mrb_raise(mrb, E_SOCKET_ERROR, "invalid sockaddr");
  }
  error = getnameinfo((struct sockaddr*)RSTRING_PTR(sastr), (socklen_t)RSTRING_LEN(sastr), RSTRING_PTR(host), NI_MAXHOST, RSTRING_PTR(serv), NI_MAXSERV, (int)flags);
  if (error) {
    mrb_raisef(mrb, E_SOCKET_ERROR, "getnameinfo: %s", gai_strerror(error));
  }
  ary = mrb_ary_new_capa(mrb, 2);
  mrb_str_resize(mrb, host, strlen(RSTRING_PTR(host)));
  mrb_ary_push(mrb, ary, host);
  mrb_str_resize(mrb, serv, strlen(RSTRING_PTR(serv)));
  mrb_ary_push(mrb, ary, serv);
  return ary;
}

#ifndef _WIN32
static mrb_value
mrb_addrinfo_unix_path(mrb_state *mrb, mrb_value self)
{
  mrb_value sastr;

  sastr = mrb_iv_get(mrb, self, MRB_IVSYM(sockaddr));
  if (!mrb_string_p(sastr) || ((struct sockaddr*)RSTRING_PTR(sastr))->sa_family != AF_UNIX)
    mrb_raise(mrb, E_SOCKET_ERROR, "need AF_UNIX address");
  if (RSTRING_LEN(sastr) < (mrb_int)offsetof(struct sockaddr_un, sun_path) + 1) {
    return mrb_str_new(mrb, "", 0);
  }
  else {
    return mrb_str_new_cstr(mrb, ((struct sockaddr_un*)RSTRING_PTR(sastr))->sun_path);
  }
}
#endif

static mrb_value
sa2addrlist(mrb_state *mrb, const struct sockaddr *sa, socklen_t salen)
{
  mrb_value ary, host;
  unsigned short port;
  const char *afstr;

  switch (sa->sa_family) {
  case AF_INET:
    afstr = "AF_INET";
    port = ((struct sockaddr_in*)sa)->sin_port;
    break;
  case AF_INET6:
    afstr = "AF_INET6";
    port = ((struct sockaddr_in6*)sa)->sin6_port;
    break;
  default:
    mrb_raise(mrb, E_ARGUMENT_ERROR, "bad af");
    return mrb_nil_value();
  }
  port = ntohs(port);
  host = mrb_str_new_capa(mrb, NI_MAXHOST);
  if (getnameinfo(sa, salen, RSTRING_PTR(host), NI_MAXHOST, NULL, 0, NI_NUMERICHOST) == -1)
    mrb_sys_fail(mrb, "getnameinfo");
  mrb_str_resize(mrb, host, strlen(RSTRING_PTR(host)));
  ary = mrb_ary_new_capa(mrb, 4);
  mrb_ary_push(mrb, ary, mrb_str_new_cstr(mrb, afstr));
  mrb_ary_push(mrb, ary, mrb_fixnum_value(port));
  mrb_ary_push(mrb, ary, host);
  mrb_ary_push(mrb, ary, host);
  return ary;
}

int mrb_io_fileno(mrb_state *mrb, mrb_value io);

static int
socket_fd(mrb_state *mrb, mrb_value sock)
{
  return mrb_io_fileno(mrb, sock);
}

static int
socket_family(int s)
{
  struct sockaddr_storage ss;
  socklen_t salen;

  salen = sizeof(ss);
  if (getsockname(s, (struct sockaddr*)&ss, &salen) == -1)
    return AF_UNSPEC;
  return ss.ss_family;
}

static mrb_value
mrb_basicsocket_getpeereid(mrb_state *mrb, mrb_value self)
{
#ifdef HAVE_GETPEEREID
  mrb_value ary;
  gid_t egid;
  uid_t euid;
  int s;

  s = socket_fd(mrb, self);
  if (getpeereid(s, &euid, &egid) != 0)
    mrb_sys_fail(mrb, "getpeereid");

  ary = mrb_ary_new_capa(mrb, 2);
  mrb_ary_push(mrb, ary, mrb_fixnum_value((mrb_int)euid));
  mrb_ary_push(mrb, ary, mrb_fixnum_value((mrb_int)egid));
  return ary;
#else
  mrb_raise(mrb, E_RUNTIME_ERROR, "getpeereid is not available on this system");
  return mrb_nil_value();
#endif
}

static mrb_value
mrb_basicsocket_getpeername(mrb_state *mrb, mrb_value self)
{
  struct sockaddr_storage ss;
  socklen_t salen;

  salen = sizeof(ss);
  if (getpeername(socket_fd(mrb, self), (struct sockaddr*)&ss, &salen) != 0)
    mrb_sys_fail(mrb, "getpeername");

  return mrb_str_new(mrb, (char*)&ss, salen);
}

static mrb_value
mrb_basicsocket_getsockname(mrb_state *mrb, mrb_value self)
{
  struct sockaddr_storage ss;
  socklen_t salen;

  salen = sizeof(ss);
  if (getsockname(socket_fd(mrb, self), (struct sockaddr*)&ss, &salen) != 0)
    mrb_sys_fail(mrb, "getsockname");

  return mrb_str_new(mrb, (char*)&ss, salen);
}

static struct RClass *
socket_option_class(mrb_state *mrb)
{
  return mrb_class_get_under_id(mrb, mrb_class_get_id(mrb, MRB_SYM(Socket)), MRB_SYM(Option));
}

static mrb_value
socket_option_init(mrb_state *mrb, mrb_value self)
{
  mrb_int family, level, optname;
  mrb_value data;

  mrb_get_args(mrb, "iiio", &family, &level, &optname, &data);
  mrb_iv_set(mrb, self, MRB_SYM(family), mrb_int_value(mrb, family));
  mrb_iv_set(mrb, self, MRB_SYM(level), mrb_int_value(mrb, level));
  mrb_iv_set(mrb, self, MRB_SYM(optname), mrb_int_value(mrb, optname));
  mrb_iv_set(mrb, self, MRB_SYM(data), data);

  return self;
}

static mrb_value
socket_option_s_bool(mrb_state *mrb, mrb_value klass)
{
  mrb_value args[4];
  mrb_bool data;
  int tmp;

  mrb_get_args(mrb, "ooob", &args[0], &args[1], &args[2], &data);
  tmp = (int)data;
  args[3] = mrb_str_new(mrb, (char*)&tmp, sizeof(int));
  return mrb_obj_new(mrb, mrb_class_ptr(klass), 4, args);
}

static mrb_value
socket_option_s_int(mrb_state *mrb, mrb_value klass)
{
  mrb_value args[4];
  mrb_int data;
  int tmp;

  mrb_get_args(mrb, "oooi", &args[0], &args[1], &args[2], &data);
  tmp = (int)data;
  args[3] = mrb_str_new(mrb, (char*)&tmp, sizeof(int));
  return mrb_obj_new(mrb, mrb_class_ptr(klass), 4, args);
}

static mrb_value
socket_option_family(mrb_state *mrb, mrb_value self)
{
  return mrb_iv_get(mrb, self, MRB_SYM(family));
}

static mrb_value
socket_option_level(mrb_state *mrb, mrb_value self)
{
  return mrb_iv_get(mrb, self, MRB_SYM(level));
}

static mrb_value
socket_option_optname(mrb_state *mrb, mrb_value self)
{
  return mrb_iv_get(mrb, self, MRB_SYM(optname));
}

static mrb_value
socket_option_data(mrb_state *mrb, mrb_value self)
{
  return mrb_iv_get(mrb, self, MRB_SYM(data));
}

static int
option_int(mrb_state *mrb, mrb_value self)
{
  mrb_value data = mrb_obj_as_string(mrb, mrb_iv_get(mrb, self, MRB_SYM(data)));
  int tmp;

  if (RSTRING_LEN(data) != sizeof(int)) {
    mrb_raisef(mrb, E_TYPE_ERROR, "size differ; expected as sizeof(int)=%i but %i", (mrb_int)sizeof(int), RSTRING_LEN(data));
  }
  memcpy((char*)&tmp, RSTRING_PTR(data), sizeof(int));
  return tmp;
}

static mrb_value
socket_option_int(mrb_state *mrb, mrb_value self)
{
  int i = option_int(mrb, self);
  return mrb_int_value(mrb, (mrb_int)i);
}

static mrb_value
socket_option_bool(mrb_state *mrb, mrb_value self)
{
  int i = option_int(mrb, self);
  return mrb_bool_value((mrb_bool)i);
}

static mrb_value
socket_option_notimp(mrb_state *mrb, mrb_value self)
{
  mrb_notimplement(mrb);
  return mrb_nil_value();
}

static mrb_value
socket_option_inspect(mrb_state *mrb, mrb_value self)
{
  mrb_value str = mrb_str_new_cstr(mrb, "#<Socket::Option: ");

  mrb_value family = mrb_iv_get(mrb, self, MRB_SYM(family));
  const char *pf = NULL;

  if (mrb_integer_p(family)) {
    mrb_int fm = mrb_integer(family);
    switch (fm) {
    case PF_INET:
      pf = "INET"; break;
#ifdef PF_INET6
    case PF_INET6:
      pf = "INET6"; break;
#endif
#ifdef PF_IPX
    case PF_IPX:
      pf = "IPX"; break;
#endif
#ifdef PF_AX25
    case PF_AX25:
      pf = "AX25"; break;
#endif
#ifdef PF_APPLETALK
    case PF_APPLETALK:
      pf = "APPLETALK"; break;
#endif
#ifdef PF_UNIX
    case PF_UNIX:
      pf = "UNIX"; break;
#endif
    default:
      break;
    }
  }

  if (pf) {
    mrb_str_cat_cstr(mrb, str, pf);
  }
  else {
    mrb_str_cat_cstr(mrb, str, "family:");
    mrb_str_cat_str(mrb, str, mrb_inspect(mrb, family));
  }
  mrb_str_cat_cstr(mrb, str, " level:");
  mrb_str_cat_str(mrb, str, mrb_inspect(mrb, mrb_iv_get(mrb, self, MRB_SYM(level))));
  mrb_str_cat_cstr(mrb, str, " optname:");
  mrb_str_cat_str(mrb, str, mrb_inspect(mrb, mrb_iv_get(mrb, self, MRB_SYM(optname))));
  mrb_str_cat_cstr(mrb, str, " ");
  mrb_str_cat_str(mrb, str, mrb_inspect(mrb, mrb_iv_get(mrb, self, MRB_SYM(data))));
  mrb_str_cat_cstr(mrb, str, ">");

  return str;
}

static mrb_value
mrb_basicsocket_getsockopt(mrb_state *mrb, mrb_value self)
{
  char opt[8];
  int s;
  mrb_int family, level, optname;
  mrb_value data;
  socklen_t optlen;

  mrb_get_args(mrb, "ii", &level, &optname);
  s = socket_fd(mrb, self);
  optlen = sizeof(opt);
  if (getsockopt(s, (int)level, (int)optname, opt, &optlen) == -1)
    mrb_sys_fail(mrb, "getsockopt");
  family = socket_family(s);
  data = mrb_str_new(mrb, opt, optlen);
  mrb_value args[4] = {mrb_fixnum_value(family), mrb_fixnum_value(level), mrb_fixnum_value(optname), data};
  return mrb_obj_new(mrb, socket_option_class(mrb), 4, args);
}

static mrb_value
mrb_basicsocket_recv(mrb_state *mrb, mrb_value self)
{
  ssize_t n;
  mrb_int maxlen, flags = 0;
  mrb_value buf;

  mrb_get_args(mrb, "i|i", &maxlen, &flags);
  buf = mrb_str_new_capa(mrb, maxlen);
  n = recv(socket_fd(mrb, self), RSTRING_PTR(buf), (fsize_t)maxlen, (int)flags);
  if (n == -1)
    mrb_sys_fail(mrb, "recv");
  mrb_str_resize(mrb, buf, (mrb_int)n);
  return buf;
}

static mrb_value
mrb_basicsocket_recvfrom(mrb_state *mrb, mrb_value self)
{
  ssize_t n;
  mrb_int maxlen, flags = 0;
  mrb_value ary, buf, sa;
  socklen_t socklen;

  mrb_get_args(mrb, "i|i", &maxlen, &flags);
  buf = mrb_str_new_capa(mrb, maxlen);
  socklen = sizeof(struct sockaddr_storage);
  sa = mrb_str_new_capa(mrb, socklen);
  n = recvfrom(socket_fd(mrb, self), RSTRING_PTR(buf), (fsize_t)maxlen, (int)flags, (struct sockaddr*)RSTRING_PTR(sa), &socklen);
  if (n == -1)
    mrb_sys_fail(mrb, "recvfrom");
  mrb_str_resize(mrb, buf, (mrb_int)n);
  mrb_str_resize(mrb, sa, (mrb_int)socklen);
  ary = mrb_ary_new_capa(mrb, 2);
  mrb_ary_push(mrb, ary, buf);
  mrb_ary_push(mrb, ary, sa);
  return ary;
}

static mrb_value
mrb_basicsocket_send(mrb_state *mrb, mrb_value self)
{
  ssize_t n;
  mrb_int flags;
  mrb_value dest, mesg;

  dest = mrb_nil_value();
  mrb_get_args(mrb, "Si|S", &mesg, &flags, &dest);
  if (mrb_nil_p(dest)) {
    n = send(socket_fd(mrb, self), RSTRING_PTR(mesg), (fsize_t)RSTRING_LEN(mesg), (int)flags);
  }
  else {
    n = sendto(socket_fd(mrb, self), RSTRING_PTR(mesg), (fsize_t)RSTRING_LEN(mesg), (int)flags, (const struct sockaddr*)RSTRING_PTR(dest), (fsize_t)RSTRING_LEN(dest));
  }
  if (n == -1)
    mrb_sys_fail(mrb, "send");
  return mrb_fixnum_value((mrb_int)n);
}

static mrb_value
mrb_basicsocket_setnonblock(mrb_state *mrb, mrb_value self)
{
  int fd, flags;
  mrb_bool nonblocking;
#ifdef _WIN32
  u_long mode = 1;
#endif

  mrb_get_args(mrb, "b", &nonblocking);
  fd = socket_fd(mrb, self);
#ifdef _WIN32
  flags = ioctlsocket(fd, FIONBIO, &mode);
  if (flags != NO_ERROR)
    mrb_sys_fail(mrb, "ioctlsocket");
#else
  flags = fcntl(fd, F_GETFL, 0);
  if (flags == 1)
    mrb_sys_fail(mrb, "fcntl");
  if (nonblocking)
    flags |= O_NONBLOCK;
  else
    flags &= ~O_NONBLOCK;
  if (fcntl(fd, F_SETFL, flags) == -1)
    mrb_sys_fail(mrb, "fcntl");
#endif
  return mrb_nil_value();
}

static mrb_value
mrb_basicsocket_setsockopt(mrb_state *mrb, mrb_value self)
{
  int s;
  mrb_int argc, level = 0, optname;
  mrb_value optval, so;

  argc = mrb_get_args(mrb, "o|io", &so, &optname, &optval);
  if (argc == 3) {
    mrb_ensure_int_type(mrb, so);
    level = mrb_integer(so);
    if (mrb_string_p(optval)) {
      /* that's good */
    }
    else if (mrb_true_p(optval) || mrb_false_p(optval)) {
      mrb_int i = mrb_test(optval) ? 1 : 0;
      optval = mrb_str_new(mrb, (char*)&i, sizeof(i));
    }
    else if (mrb_integer_p(optval)) {
      if (optname == IP_MULTICAST_TTL || optname == IP_MULTICAST_LOOP) {
        char uc = (char)mrb_integer(optval);
        optval = mrb_str_new(mrb, &uc, sizeof(uc));
      }
      else {
        mrb_int i = mrb_integer(optval);
        optval = mrb_str_new(mrb, (char*)&i, sizeof(i));
      }
    }
    else {
      mrb_raise(mrb, E_ARGUMENT_ERROR, "optval should be true, false, an integer, or a string");
    }
  }
  else if (argc == 1) {
    if (!mrb_obj_is_instance_of(mrb, so, socket_option_class(mrb)))
      mrb_raise(mrb, E_ARGUMENT_ERROR, "not an instance of Socket::Option");
    level = mrb_as_int(mrb, mrb_iv_get(mrb, so, MRB_SYM(level)));
    optname = mrb_as_int(mrb, mrb_iv_get(mrb, so, MRB_SYM(optname)));
    optval = mrb_iv_get(mrb, so, MRB_SYM(data));
    mrb_ensure_string_type(mrb, optval);
  }
  else {
    mrb_argnum_error(mrb, argc, 3, 3);
  }

  s = socket_fd(mrb, self);
  if (setsockopt(s, (int)level, (int)optname, RSTRING_PTR(optval), (socklen_t)RSTRING_LEN(optval)) == -1)
    mrb_sys_fail(mrb, "setsockopt");
  return mrb_fixnum_value(0);
}

static mrb_value
mrb_basicsocket_shutdown(mrb_state *mrb, mrb_value self)
{
  mrb_int how = SHUT_RDWR;

  mrb_get_args(mrb, "|i", &how);
  if (shutdown(socket_fd(mrb, self), (int)how) != 0)
    mrb_sys_fail(mrb, "shutdown");
  return mrb_fixnum_value(0);
}

static mrb_value
mrb_basicsocket_set_is_socket(mrb_state *mrb, mrb_value self)
{
  mrb_bool b;
  struct mrb_io *io_p;
  mrb_get_args(mrb, "b", &b);

  io_p = (struct mrb_io*)DATA_PTR(self);
  if (io_p) {
    io_p->is_socket = b;
  }

  return mrb_bool_value(b);
}

static mrb_value
mrb_ipsocket_ntop(mrb_state *mrb, mrb_value klass)
{
  mrb_int af, n;
  const char *addr;
  char buf[50];

  mrb_get_args(mrb, "is", &af, &addr, &n);
  if ((af == AF_INET && n != 4) || (af == AF_INET6 && n != 16) ||
      inet_ntop((int)af, addr, buf, sizeof(buf)) == NULL)
    mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid address");
  return mrb_str_new_cstr(mrb, buf);
}

static mrb_value
mrb_ipsocket_pton(mrb_state *mrb, mrb_value klass)
{
  mrb_int af, n;
  const char *bp;
  char buf[50];

  mrb_get_args(mrb, "is", &af, &bp, &n);
  if ((size_t)n > sizeof(buf) - 1) goto invalid;
  memcpy(buf, bp, n);
  buf[n] = '\0';

  if (af == AF_INET) {
    struct in_addr in;
    if (inet_pton(AF_INET, buf, (void*)&in.s_addr) != 1)
      goto invalid;
    return mrb_str_new(mrb, (char*)&in.s_addr, 4);
  }
  else if (af == AF_INET6) {
    struct in6_addr in6;
    if (inet_pton(AF_INET6, buf, (void*)&in6.s6_addr) != 1)
      goto invalid;
    return mrb_str_new(mrb, (char*)&in6.s6_addr, 16);
  }
  else {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "unsupported address family");
  }

invalid:
  mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid address");
  return mrb_nil_value(); /* dummy */
}

static mrb_value
mrb_ipsocket_recvfrom(mrb_state *mrb, mrb_value self)
{
  struct sockaddr_storage ss;
  socklen_t socklen;
  mrb_value a, buf, pair;
  mrb_int flags, maxlen;
  ssize_t n;
  int fd;

  fd = socket_fd(mrb, self);
  flags = 0;
  mrb_get_args(mrb, "i|i", &maxlen, &flags);
  buf = mrb_str_new_capa(mrb, maxlen);
  socklen = sizeof(ss);
  n = recvfrom(fd, RSTRING_PTR(buf), (fsize_t)maxlen, (int)flags,
               (struct sockaddr*)&ss, &socklen);
  if (n == -1) {
    mrb_sys_fail(mrb, "recvfrom");
  }
  mrb_str_resize(mrb, buf, (mrb_int)n);
  a = sa2addrlist(mrb, (struct sockaddr*)&ss, socklen);
  pair = mrb_ary_new_capa(mrb, 2);
  mrb_ary_push(mrb, pair, buf);
  mrb_ary_push(mrb, pair, a);
  return pair;
}

static mrb_value
mrb_socket_gethostname(mrb_state *mrb, mrb_value cls)
{
  mrb_value buf;
  size_t bufsize;

#ifdef HOST_NAME_MAX
  bufsize = HOST_NAME_MAX + 1;
#else
  bufsize = 256;
#endif
  buf = mrb_str_new_capa(mrb, (mrb_int)bufsize);
  if (gethostname(RSTRING_PTR(buf), (fsize_t)bufsize) != 0)
    mrb_sys_fail(mrb, "gethostname");
  mrb_str_resize(mrb, buf, (mrb_int)strlen(RSTRING_PTR(buf)));
  return buf;
}

static mrb_value
mrb_socket_accept(mrb_state *mrb, mrb_value klass)
{
  int s1;
  mrb_int s0;

  mrb_get_args(mrb, "i", &s0);
  s1 = (int)accept(s0, NULL, NULL);
  if (s1 == -1) {
    mrb_sys_fail(mrb, "accept");
  }
  return mrb_fixnum_value(s1);
}

static mrb_value
mrb_socket_accept2(mrb_state *mrb, mrb_value klass)
{
  mrb_value ary, sastr;
  int s1;
  mrb_int s0;
  socklen_t socklen;

  mrb_get_args(mrb, "i", &s0);
  socklen = sizeof(struct sockaddr_storage);
  sastr = mrb_str_new_capa(mrb, (mrb_int)socklen);
  s1 = (int)accept(s0, (struct sockaddr*)RSTRING_PTR(sastr), &socklen);
  if (s1 == -1) {
    mrb_sys_fail(mrb, "accept");
  }
  // XXX: possible descriptor leakage here!
  mrb_str_resize(mrb, sastr, socklen);
  ary = mrb_ary_new_capa(mrb, 2);
  mrb_ary_push(mrb, ary, mrb_fixnum_value(s1));
  mrb_ary_push(mrb, ary, sastr);
  return ary;
}

static mrb_value
mrb_socket_bind(mrb_state *mrb, mrb_value klass)
{
  mrb_value sastr;
  mrb_int s;

  mrb_get_args(mrb, "iS", &s, &sastr);
  if (bind((int)s, (struct sockaddr*)RSTRING_PTR(sastr), (socklen_t)RSTRING_LEN(sastr)) == -1) {
    mrb_sys_fail(mrb, "bind");
  }
  return mrb_nil_value();
}

static mrb_value
mrb_socket_connect(mrb_state *mrb, mrb_value klass)
{
  mrb_value sastr;
  mrb_int s;

  mrb_get_args(mrb, "iS", &s, &sastr);
  if (connect((int)s, (struct sockaddr*)RSTRING_PTR(sastr), (socklen_t)RSTRING_LEN(sastr)) == -1) {
    mrb_sys_fail(mrb, "connect");
  }
  return mrb_nil_value();
}

static mrb_value
mrb_socket_listen(mrb_state *mrb, mrb_value klass)
{
  mrb_int backlog, s;

  mrb_get_args(mrb, "ii", &s, &backlog);
  if (listen((int)s, (int)backlog) == -1) {
    mrb_sys_fail(mrb, "listen");
  }
  return mrb_nil_value();
}

static mrb_value
mrb_socket_sockaddr_family(mrb_state *mrb, mrb_value klass)
{
  const struct sockaddr *sa;
  mrb_value str;

  mrb_get_args(mrb, "S", &str);
  if ((size_t)RSTRING_LEN(str) < offsetof(struct sockaddr, sa_family) + sizeof(sa->sa_family)) {
    mrb_raise(mrb, E_SOCKET_ERROR, "invalid sockaddr (too short)");
  }
  sa = (const struct sockaddr*)RSTRING_PTR(str);
  return mrb_fixnum_value(sa->sa_family);
}

static mrb_value
mrb_socket_sockaddr_un(mrb_state *mrb, mrb_value klass)
{
#ifdef _WIN32
  mrb_raise(mrb, E_NOTIMP_ERROR, "sockaddr_un unsupported on Windows");
  return mrb_nil_value();
#else
  struct sockaddr_un *sunp;
  mrb_value path, s;

  mrb_get_args(mrb, "S", &path);
  if ((size_t)RSTRING_LEN(path) > sizeof(sunp->sun_path) - 1) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "too long unix socket path (max: %d bytes)", (int)sizeof(sunp->sun_path) - 1);
  }
  s = mrb_str_new_capa(mrb, sizeof(struct sockaddr_un));
  sunp = (struct sockaddr_un*)RSTRING_PTR(s);
#if HAVE_SA_LEN
  sunp->sun_len = sizeof(struct sockaddr_un);
#endif
  sunp->sun_family = AF_UNIX;
  memcpy(sunp->sun_path, RSTRING_PTR(path), RSTRING_LEN(path));
  sunp->sun_path[RSTRING_LEN(path)] = '\0';
  mrb_str_resize(mrb, s, sizeof(struct sockaddr_un));
  return s;
#endif
}

static mrb_value
mrb_socket_socketpair(mrb_state *mrb, mrb_value klass)
{
#ifdef _WIN32
  mrb_raise(mrb, E_NOTIMP_ERROR, "socketpair unsupported on Windows");
  return mrb_nil_value();
#else
  mrb_value ary;
  mrb_int domain, type, protocol;
  int sv[2];

  mrb_get_args(mrb, "iii", &domain, &type, &protocol);
  if (socketpair(domain, type, protocol, sv) == -1) {
    mrb_sys_fail(mrb, "socketpair");
  }
  // XXX: possible descriptor leakage here!
  ary = mrb_ary_new_capa(mrb, 2);
  mrb_ary_push(mrb, ary, mrb_fixnum_value(sv[0]));
  mrb_ary_push(mrb, ary, mrb_fixnum_value(sv[1]));
  return ary;
#endif
}

static mrb_value
mrb_socket_socket(mrb_state *mrb, mrb_value klass)
{
  mrb_int domain, type, protocol;
  int s;

  mrb_get_args(mrb, "iii", &domain, &type, &protocol);
  s = (int)socket((int)domain, (int)type, (int)protocol);
  if (s == -1)
    mrb_sys_fail(mrb, "socket");
  return mrb_fixnum_value(s);
}

static mrb_value
mrb_tcpsocket_allocate(mrb_state *mrb, mrb_value klass)
{
  struct RClass *c = mrb_class_ptr(klass);
  enum mrb_vtype ttype = MRB_INSTANCE_TT(c);

  /* copied from mrb_instance_alloc() */
  if (ttype == 0) ttype = MRB_TT_OBJECT;
  return mrb_obj_value((struct RObject*)mrb_obj_alloc(mrb, ttype, c));
}

/* Windows overrides for IO methods on BasicSocket objects.
 * This is because sockets on Windows are not the same as file
 * descriptors, and thus functions which operate on file descriptors
 * will break on socket descriptors.
 */
#ifdef _WIN32
static mrb_value
mrb_win32_basicsocket_close(mrb_state *mrb, mrb_value self)
{
  if (closesocket(socket_fd(mrb, self)) != NO_ERROR)
    mrb_raise(mrb, E_SOCKET_ERROR, "closesocket unsuccessful");
  return mrb_nil_value();
}

static mrb_value
mrb_win32_basicsocket_sysread(mrb_state *mrb, mrb_value self)
{
  int sd, ret;
  mrb_value buf = mrb_nil_value();
  mrb_int maxlen;

  mrb_get_args(mrb, "i|S", &maxlen, &buf);
  if (maxlen < 0) {
    return mrb_nil_value();
  }

  if (mrb_nil_p(buf)) {
    buf = mrb_str_new(mrb, NULL, maxlen);
  }
  if (RSTRING_LEN(buf) != maxlen) {
    buf = mrb_str_resize(mrb, buf, maxlen);
  }

  sd = socket_fd(mrb, self);
  ret = recv(sd, RSTRING_PTR(buf), (int)maxlen, 0);

  switch (ret) {
    case 0: /* EOF */
      if (maxlen == 0) {
        buf = mrb_str_new_cstr(mrb, "");
      }
      else {
        mrb_raise(mrb, E_EOF_ERROR, "sysread failed: End of File");
      }
      break;
    case SOCKET_ERROR: /* Error */
      mrb_sys_fail(mrb, "recv");
      break;
    default:
      if (RSTRING_LEN(buf) != ret) {
        buf = mrb_str_resize(mrb, buf, ret);
      }
      break;
  }

  return buf;
}

static mrb_value
mrb_win32_basicsocket_sysseek(mrb_state *mrb, mrb_value self)
{
  mrb_raise(mrb, E_NOTIMP_ERROR, "sysseek not implemented for windows sockets");
  return mrb_nil_value();
}

static mrb_value
mrb_win32_basicsocket_syswrite(mrb_state *mrb, mrb_value self)
{
  int n;
  SOCKET sd;
  mrb_value str;

  sd = socket_fd(mrb, self);
  mrb_get_args(mrb, "S", &str);
  n = send(sd, RSTRING_PTR(str), (int)RSTRING_LEN(str), 0);
  if (n == SOCKET_ERROR)
    mrb_sys_fail(mrb, "send");
  return mrb_int_value(mrb, n);
}

#endif

void
mrb_mruby_socket_gem_init(mrb_state* mrb)
{
  struct RClass *io, *ai, *sock, *bsock, *ipsock, *tcpsock;
  struct RClass *constants, *option;

#ifdef _WIN32
  WSADATA wsaData;
  int result;
  result = WSAStartup(MAKEWORD(2,2), &wsaData);
  if (result != NO_ERROR)
    mrb_raise(mrb, E_RUNTIME_ERROR, "WSAStartup failed");
#endif

  ai = mrb_define_class(mrb, "Addrinfo", mrb->object_class);
  mrb_define_class_method(mrb, ai, "getaddrinfo", mrb_addrinfo_getaddrinfo, MRB_ARGS_REQ(2)|MRB_ARGS_OPT(4));
  mrb_define_method(mrb, ai, "getnameinfo", mrb_addrinfo_getnameinfo, MRB_ARGS_OPT(1));
#ifndef _WIN32
  mrb_define_method(mrb, ai, "unix_path", mrb_addrinfo_unix_path, MRB_ARGS_NONE());
#endif

  io = mrb_class_get_id(mrb, MRB_SYM(IO));

  bsock = mrb_define_class(mrb, "BasicSocket", io);
  mrb_define_method(mrb, bsock, "_recvfrom", mrb_basicsocket_recvfrom, MRB_ARGS_REQ(1)|MRB_ARGS_OPT(1));
  mrb_define_method(mrb, bsock, "_setnonblock", mrb_basicsocket_setnonblock, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, bsock, "getpeereid", mrb_basicsocket_getpeereid, MRB_ARGS_NONE());
  mrb_define_method(mrb, bsock, "getpeername", mrb_basicsocket_getpeername, MRB_ARGS_NONE());
  mrb_define_method(mrb, bsock, "getsockname", mrb_basicsocket_getsockname, MRB_ARGS_NONE());
  mrb_define_method(mrb, bsock, "getsockopt", mrb_basicsocket_getsockopt, MRB_ARGS_REQ(2));
  mrb_define_method(mrb, bsock, "recv", mrb_basicsocket_recv, MRB_ARGS_REQ(1)|MRB_ARGS_OPT(1));
  // #recvmsg(maxlen, flags=0)
  mrb_define_method(mrb, bsock, "send", mrb_basicsocket_send, MRB_ARGS_REQ(2)|MRB_ARGS_OPT(1));
  // #sendmsg
  // #sendmsg_nonblock
  mrb_define_method(mrb, bsock, "setsockopt", mrb_basicsocket_setsockopt, MRB_ARGS_REQ(1)|MRB_ARGS_OPT(2));
  mrb_define_method(mrb, bsock, "shutdown", mrb_basicsocket_shutdown, MRB_ARGS_OPT(1));
  mrb_define_method(mrb, bsock, "_is_socket=", mrb_basicsocket_set_is_socket, MRB_ARGS_REQ(1));

  ipsock = mrb_define_class(mrb, "IPSocket", bsock);
  mrb_define_class_method(mrb, ipsock, "ntop", mrb_ipsocket_ntop, MRB_ARGS_REQ(1));
  mrb_define_class_method(mrb, ipsock, "pton", mrb_ipsocket_pton, MRB_ARGS_REQ(2));
  mrb_define_method(mrb, ipsock, "recvfrom", mrb_ipsocket_recvfrom, MRB_ARGS_REQ(1)|MRB_ARGS_OPT(1));

  tcpsock = mrb_define_class(mrb, "TCPSocket", ipsock);
  mrb_define_class_method(mrb, tcpsock, "_allocate", mrb_tcpsocket_allocate, MRB_ARGS_NONE());
  mrb_define_class(mrb, "TCPServer", tcpsock);

  mrb_define_class(mrb, "UDPSocket", ipsock);
  //#recvfrom_nonblock

  sock = mrb_define_class(mrb, "Socket", bsock);
  mrb_define_class_method(mrb, sock, "_accept", mrb_socket_accept, MRB_ARGS_REQ(1));
  mrb_define_class_method(mrb, sock, "_accept2", mrb_socket_accept2, MRB_ARGS_REQ(1));
  mrb_define_class_method(mrb, sock, "_bind", mrb_socket_bind, MRB_ARGS_REQ(3));
  mrb_define_class_method(mrb, sock, "_connect", mrb_socket_connect, MRB_ARGS_REQ(3));
  mrb_define_class_method(mrb, sock, "_listen", mrb_socket_listen, MRB_ARGS_REQ(2));
  mrb_define_class_method(mrb, sock, "_sockaddr_family", mrb_socket_sockaddr_family, MRB_ARGS_REQ(1));
  mrb_define_class_method(mrb, sock, "_socket", mrb_socket_socket, MRB_ARGS_REQ(3));
  //mrb_define_class_method(mrb, sock, "gethostbyaddr", mrb_socket_gethostbyaddr, MRB_ARGS_REQ(1)|MRB_ARGS_OPT(1));
  //mrb_define_class_method(mrb, sock, "gethostbyname", mrb_socket_gethostbyname, MRB_ARGS_REQ(1)|MRB_ARGS_OPT(1));
  mrb_define_class_method(mrb, sock, "gethostname", mrb_socket_gethostname, MRB_ARGS_NONE());
  //mrb_define_class_method(mrb, sock, "getservbyname", mrb_socket_getservbyname, MRB_ARGS_REQ(1)|MRB_ARGS_OPT(1));
  //mrb_define_class_method(mrb, sock, "getservbyport", mrb_socket_getservbyport, MRB_ARGS_REQ(1)|MRB_ARGS_OPT(1));
  mrb_define_class_method(mrb, sock, "sockaddr_un", mrb_socket_sockaddr_un, MRB_ARGS_REQ(1));
  mrb_define_class_method(mrb, sock, "socketpair", mrb_socket_socketpair, MRB_ARGS_REQ(3));
  //mrb_define_method(mrb, sock, "sysaccept", mrb_socket_accept, MRB_ARGS_NONE());

#ifndef _WIN32
  mrb_define_class(mrb, "UNIXSocket", bsock);
  //mrb_define_class_method(mrb, usock, "pair", mrb_unixsocket_open, MRB_ARGS_OPT(2));
  //mrb_define_class_method(mrb, usock, "socketpair", mrb_unixsocket_open, MRB_ARGS_OPT(2));

  //mrb_define_method(mrb, usock, "recv_io", mrb_unixsocket_peeraddr, MRB_ARGS_NONE());
  //mrb_define_method(mrb, usock, "recvfrom", mrb_unixsocket_peeraddr, MRB_ARGS_NONE());
  //mrb_define_method(mrb, usock, "send_io", mrb_unixsocket_peeraddr, MRB_ARGS_NONE());
#endif

  /* Windows IO Method Overrides on BasicSocket */
#ifdef _WIN32
  mrb_define_method(mrb, bsock, "close", mrb_win32_basicsocket_close, MRB_ARGS_NONE());
  mrb_define_method(mrb, bsock, "sysread", mrb_win32_basicsocket_sysread, MRB_ARGS_REQ(1)|MRB_ARGS_OPT(1));
  mrb_define_method(mrb, bsock, "sysseek", mrb_win32_basicsocket_sysseek, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, bsock, "syswrite", mrb_win32_basicsocket_syswrite, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, bsock, "read", mrb_win32_basicsocket_sysread, MRB_ARGS_REQ(1)|MRB_ARGS_OPT(1));
  mrb_define_method(mrb, bsock, "write", mrb_win32_basicsocket_syswrite, MRB_ARGS_REQ(1));
#endif

  option = mrb_define_class_under(mrb, sock, "Option", mrb->object_class);
  mrb_define_class_method(mrb, option, "bool", socket_option_s_bool, MRB_ARGS_REQ(4));
  mrb_define_class_method(mrb, option, "int", socket_option_s_int, MRB_ARGS_REQ(4));
  mrb_define_method(mrb, option, "initialize", socket_option_init, MRB_ARGS_REQ(4));
  mrb_define_method(mrb, option, "inspect", socket_option_inspect, MRB_ARGS_REQ(0));
  mrb_define_method(mrb, option, "family", socket_option_family, MRB_ARGS_REQ(0));
  mrb_define_method(mrb, option, "level", socket_option_level, MRB_ARGS_REQ(0));
  mrb_define_method(mrb, option, "optname", socket_option_optname, MRB_ARGS_REQ(0));
  mrb_define_method(mrb, option, "data", socket_option_data, MRB_ARGS_REQ(0));
  mrb_define_method(mrb, option, "bool", socket_option_bool, MRB_ARGS_REQ(0));
  mrb_define_method(mrb, option, "int", socket_option_int, MRB_ARGS_REQ(0));

  mrb_define_method(mrb, option, "linger", socket_option_notimp, MRB_ARGS_REQ(0));
  mrb_define_method(mrb, option, "unpack", socket_option_notimp, MRB_ARGS_REQ(1));

  constants = mrb_define_module_under(mrb, sock, "Constants");

#define define_const(SYM) \
  do {                                                                \
    mrb_define_const(mrb, constants, #SYM, mrb_int_value(mrb, SYM));  \
  } while (0)

#include "const.cstub"

  mrb_include_module(mrb, sock, constants);
}

void
mrb_mruby_socket_gem_final(mrb_state* mrb)
{
#ifdef _WIN32
  WSACleanup();
#endif
}
