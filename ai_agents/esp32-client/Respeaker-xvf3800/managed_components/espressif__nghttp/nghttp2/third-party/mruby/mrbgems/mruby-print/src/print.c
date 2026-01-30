#include <mruby.h>

#ifdef MRB_NO_STDIO
# error print conflicts 'MRB_NO_STDIO' in your build configuration
#endif

#include <mruby/string.h>
#include <string.h>
#if defined(_WIN32)
# include <windows.h>
# include <io.h>
#ifdef _MSC_VER
# define isatty(x) _isatty(x)
# define fileno(x) _fileno(x)
#endif
#else
# include <unistd.h>
#endif

static void
printstr(mrb_state *mrb, mrb_value s)
{
  if (mrb_string_p(s)) {
    const char *p = RSTRING_PTR(s);
    mrb_int len = RSTRING_LEN(s);

#if defined(_WIN32)
    if (isatty(fileno(stdout))) {
      DWORD written;
      int wlen = MultiByteToWideChar(CP_UTF8, 0, p, (int)len, NULL, 0);
      wchar_t* utf16 = (wchar_t*)mrb_malloc(mrb, (wlen+1) * sizeof(wchar_t));
      if (MultiByteToWideChar(CP_UTF8, 0, p, (int)len, utf16, wlen) > 0) {
        utf16[wlen] = 0;
        WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE),
                      utf16, (DWORD)wlen, &written, NULL);
      }
      mrb_free(mrb, utf16);
      return;
    }
#endif
    fwrite(p, (size_t)len, 1, stdout);
  }
}

// ISO 15.3.1.2.10 Kernel.print
// ISO 15.3.1.3.35 Kernel#print
static mrb_value
mrb_print(mrb_state *mrb, mrb_value self)
{
  mrb_int argc = mrb_get_argc(mrb);
  const mrb_value *argv = mrb_get_argv(mrb);

  for (mrb_int i=0; i<argc; i++) {
    mrb_value str = mrb_obj_as_string(mrb, argv[i]);
    printstr(mrb, str);
  }
  if (isatty(fileno(stdout))) fflush(stdout);
  return mrb_nil_value();
}

void
mrb_mruby_print_gem_init(mrb_state* mrb)
{
  mrb_define_method(mrb, mrb->kernel_module, "print", mrb_print, MRB_ARGS_ANY()); /* 15.3.1.3.35 */
}

void
mrb_mruby_print_gem_final(mrb_state* mrb)
{
}
