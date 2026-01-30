/*
** backtrace.c -
**
** See Copyright Notice in mruby.h
*/

#include <mruby.h>
#include <mruby/variable.h>
#include <mruby/proc.h>
#include <mruby/array.h>
#include <mruby/string.h>
#include <mruby/class.h>
#include <mruby/debug.h>
#include <mruby/error.h>
#include <mruby/numeric.h>
#include <mruby/data.h>
#include <mruby/internal.h>
#include <mruby/presym.h>

struct backtrace_location {
  mrb_sym method_id;
  int32_t idx;
  const mrb_irep *irep;
};

typedef void (*each_backtrace_func)(mrb_state*, const struct backtrace_location*, void*);

static const mrb_data_type bt_type = { "Backtrace", mrb_free };

static uint32_t
each_backtrace(mrb_state *mrb, ptrdiff_t ciidx, each_backtrace_func func, void *data)
{
  uint32_t n = 0;

  for (ptrdiff_t i=ciidx; i >= 0; i--) {
    struct backtrace_location loc;
    mrb_callinfo *ci;
    const mrb_code *pc;

    ci = &mrb->c->cibase[i];

    if (!ci->proc || MRB_PROC_CFUNC_P(ci->proc)) {
      if (!ci->mid) continue;
      loc.irep = NULL;
    }
    else {
      loc.irep = ci->proc->body.irep;
      if (!loc.irep) continue;
      if (!loc.irep->debug_info) continue;
      if (mrb->c->cibase[i].pc) {
        pc = &mrb->c->cibase[i].pc[-1];
      }
      else {
        continue;
      }
      loc.idx = (uint32_t)(pc - loc.irep->iseq);
    }
    loc.method_id = ci->mid;
    if (loc.irep == NULL) {
      for (ptrdiff_t j=i-1; j >= 0; j--) {
        ci = &mrb->c->cibase[j];

        if (!ci->proc) continue;
        if (MRB_PROC_CFUNC_P(ci->proc)) continue;

        const mrb_irep *irep = ci->proc->body.irep;
        if (!irep) continue;
        if (!irep->debug_info) continue;

        if (mrb->c->cibase[j].pc) {
          pc = &mrb->c->cibase[j].pc[-1];
        }
        else {
          continue;
        }

        loc.irep = irep;
        loc.idx = (uint32_t)(pc - loc.irep->iseq);
        break;
      }
    }
    if (func) func(mrb, &loc, data);
    n++;
  }
  return n;
}

static void
pack_backtrace_i(mrb_state *mrb,
                 const struct backtrace_location *loc,
                 void *data)
{
  struct backtrace_location **pptr = (struct backtrace_location**)data;
  struct backtrace_location *ptr = *pptr;

  *ptr = *loc;
  *pptr = ptr+1;
}

static struct RObject*
packed_backtrace(mrb_state *mrb)
{
  struct RData *backtrace;
  ptrdiff_t ciidx = mrb->c->ci - mrb->c->cibase;

  if (ciidx >= mrb->c->ciend - mrb->c->cibase)
    ciidx = mrb->c->ciend - mrb->c->cibase; /* ciidx is broken... */

  /* count the number of backtraces */
  int len = each_backtrace(mrb, ciidx, NULL, NULL);
  backtrace = mrb_data_object_alloc(mrb, NULL, NULL, &bt_type);
  if (len > 0) {
    void *ptr = mrb_malloc(mrb, len * sizeof(struct backtrace_location));
    backtrace->data = ptr;
    backtrace->flags = len;
    each_backtrace(mrb, ciidx, pack_backtrace_i, &ptr);
  }
  else {
    backtrace->data = NULL;
    backtrace->flags = 0;
  }
  return (struct RObject*)backtrace;
}

static void
store_backtrace(mrb_state *mrb, mrb_value exc, struct RObject *backtrace)
{
  struct RException *e = mrb_exc_ptr(exc);
  e->backtrace = backtrace;
  mrb_field_write_barrier(mrb, (struct RBasic*)e, (struct RBasic*)backtrace);
}

void
mrb_keep_backtrace(mrb_state *mrb, mrb_value exc)
{
  int ai;

  if (mrb->c->ci == NULL) return;
  if (mrb_exc_ptr(exc)->backtrace) return;
  ai = mrb_gc_arena_save(mrb);
  struct RObject *backtrace = packed_backtrace(mrb);
  store_backtrace(mrb, exc, backtrace);
  mrb_gc_arena_restore(mrb, ai);
}

static struct RObject*
mrb_unpack_backtrace(mrb_state *mrb, struct RObject *backtrace)
{
  const struct backtrace_location *bt;
  mrb_int n, i;
  int ai;

  if (backtrace == NULL) {
  empty_backtrace:
    return mrb_obj_ptr(mrb_ary_new_capa(mrb, 0));
  }
  if (backtrace->tt == MRB_TT_ARRAY) return backtrace;
  bt = (struct backtrace_location*)mrb_data_check_get_ptr(mrb, mrb_obj_value(backtrace), &bt_type);
  if (bt == NULL) goto empty_backtrace;
  n = (mrb_int)backtrace->flags;
  if (n == 0) goto empty_backtrace;
  backtrace = mrb_obj_ptr(mrb_ary_new_capa(mrb, n));
  ai = mrb_gc_arena_save(mrb);
  for (i = 0; i < n; i++) {
    const struct backtrace_location *entry = &bt[i];
    mrb_value btline;
    int32_t lineno;
    const char *filename;

    if (!mrb_debug_get_position(mrb, entry->irep, entry->idx, &lineno, &filename)) {
      btline = mrb_str_new_lit(mrb, "(unknown):0");
    }
    else if (lineno != -1) {//debug info was available
      btline = mrb_format(mrb, "%s:%d", filename, (int)lineno);
    }
    else { //all that was left was the stack frame
      btline = mrb_format(mrb, "%s:0", filename);
    }
    if (entry->method_id != 0) {
      mrb_str_cat_lit(mrb, btline, ":in ");
      mrb_str_cat_cstr(mrb, btline, mrb_sym_name(mrb, entry->method_id));
    }
    mrb_ary_push(mrb, mrb_obj_value(backtrace), btline);
    mrb_gc_arena_restore(mrb, ai);
  }

  return backtrace;
}

mrb_value
mrb_exc_backtrace(mrb_state *mrb, mrb_value exc)
{
  struct RObject *backtrace = mrb_exc_ptr(exc)->backtrace;
  if (backtrace == NULL) {
    return mrb_nil_value();
  }
  if (backtrace->tt == MRB_TT_ARRAY) {
    return mrb_obj_value(backtrace);
  }
  /* unpack packed-backtrace */
  backtrace = mrb_unpack_backtrace(mrb, backtrace);
  store_backtrace(mrb, exc, backtrace);
  return mrb_obj_value(backtrace);
}

mrb_value
mrb_get_backtrace(mrb_state *mrb)
{
  return mrb_obj_value(mrb_unpack_backtrace(mrb, packed_backtrace(mrb)));
}

#ifndef MRB_NO_STDIO

static void
print_backtrace(mrb_state *mrb, struct RObject *exc, struct RArray *backtrace)
{
  mrb_int i;
  mrb_int n = (backtrace ? ARY_LEN(backtrace) : 0);
  mrb_value *loc, mesg;

  if (n != 0) {
    if (n > 1) {
      fputs("trace (most recent call last):\n", stderr);
    }
    for (i=n-1,loc=&ARY_PTR(backtrace)[i]; i>0; i--,loc--) {
      if (mrb_string_p(*loc)) {
        fprintf(stderr, "\t[%d] ", (int)i);
        fwrite(RSTRING_PTR(*loc), (int)RSTRING_LEN(*loc), 1, stderr);
        fputc('\n', stderr);
      }
    }
    if (mrb_string_p(*loc)) {
      fwrite(RSTRING_PTR(*loc), (int)RSTRING_LEN(*loc), 1, stderr);
      fputs(": ", stderr);
    }
  }
  else {
    fputs("(unknown):0: ", stderr);
  }

  if (exc == mrb->nomem_err) {
    static const char nomem[] = "Out of memory (NoMemoryError)\n";
    fwrite(nomem, sizeof(nomem)-1, 1, stderr);
  }
  else {
    mesg = mrb_exc_inspect(mrb, mrb_obj_value(exc));
    fwrite(RSTRING_PTR(mesg), RSTRING_LEN(mesg), 1, stderr);
    fputc('\n', stderr);
  }
}

/* mrb_print_backtrace

   function to retrieve backtrace information from the last exception.
*/

MRB_API void
mrb_print_backtrace(mrb_state *mrb)
{
  if (!mrb->exc || mrb->exc->tt != MRB_TT_EXCEPTION) {
    return;
  }

  struct RObject *backtrace = ((struct RException*)mrb->exc)->backtrace;
  if (backtrace && backtrace->tt != MRB_TT_ARRAY) backtrace = mrb_unpack_backtrace(mrb, backtrace);
  print_backtrace(mrb, mrb->exc, (struct RArray*)backtrace);
}
#else
MRB_API void
mrb_print_backtrace(mrb_state *mrb)
{
}
#endif
