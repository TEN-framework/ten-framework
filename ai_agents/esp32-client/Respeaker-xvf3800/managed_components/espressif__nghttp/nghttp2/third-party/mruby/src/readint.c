#include <mruby.h>
#include <mruby/numeric.h>

/* mrb_read_int(): read mrb_int from a string (base 10 only) */
/* const char *p - string to read                            */
/* const char *e - end of string                             */
/* char **endp   - end of parsed integer                     */
/* mrb_int *np   - variable to save the result               */
/* returns TRUE if read succeeded                            */
/* if integer overflows, returns FALSE                       */
MRB_API mrb_bool
mrb_read_int(const char *p, const char *e, char **endp, mrb_int *np)
{
  mrb_int n = 0;
  int ch;

  while ((e == NULL || p < e) && ISDIGIT(*p)) {
    ch = *p - '0';
    if (mrb_int_mul_overflow(n, 10, &n) ||
        mrb_int_add_overflow(n, ch, &n)) {
      return FALSE;
    }
    p++;
  }
  if (endp) *endp = (char*)p;
  *np = n;
  return TRUE;
}
