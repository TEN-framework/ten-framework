/**
** @file mruby/gc.h - garbage collector for mruby
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_GC_H
#define MRUBY_GC_H

#include "common.h"

/**
 * Uncommon memory management stuffs.
 */
MRB_BEGIN_DECL


struct mrb_state;

#define MRB_EACH_OBJ_OK 0
#define MRB_EACH_OBJ_BREAK 1
typedef int (mrb_each_object_callback)(struct mrb_state *mrb, struct RBasic *obj, void *data);
void mrb_objspace_each_objects(struct mrb_state *mrb, mrb_each_object_callback *callback, void *data);
size_t mrb_objspace_page_slot_size(void);
MRB_API void mrb_free_context(struct mrb_state *mrb, struct mrb_context *c);

#ifndef MRB_GC_ARENA_SIZE
#define MRB_GC_ARENA_SIZE 100
#endif

typedef enum {
  MRB_GC_STATE_ROOT = 0,
  MRB_GC_STATE_MARK,
  MRB_GC_STATE_SWEEP
} mrb_gc_state;

/* Disable MSVC warning "C4200: nonstandard extension used: zero-sized array
 * in struct/union" when in C++ mode */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4200)
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

typedef struct mrb_gc {
  struct mrb_heap_page *heaps;     /* all heaps pages */
  struct mrb_heap_page *free_heaps;/* heaps for allocation */
  struct mrb_heap_page *sweeps;    /* page where sweep starts */
  struct RBasic *gray_list;        /* list of gray objects to be traversed incrementally */
  struct RBasic *atomic_gray_list; /* list of objects to be traversed atomically */
  size_t live;                     /* count of live objects */
  size_t live_after_mark;          /* old generation objects */
  size_t threshold;                /* threshold to start GC */
  size_t oldgen_threshold;         /* threshold to kick major GC */
  mrb_gc_state state;              /* current state of gc */
  int interval_ratio;
  int step_ratio;
  int current_white_part :2;       /* make white object by white_part */
  mrb_bool iterating     :1;       /* currently iterating over objects */
  mrb_bool disabled      :1;       /* GC disabled */
  mrb_bool generational  :1;       /* generational GC mode */
  mrb_bool full          :1;       /* major GC mode */
  mrb_bool out_of_memory :1;       /* out-of-memory error occurred */

#ifdef MRB_GC_FIXED_ARENA
  struct RBasic *arena[MRB_GC_ARENA_SIZE]; /* GC protection array */
#else
  struct RBasic **arena;                   /* GC protection array */
  int arena_capa;                          /* size of protection array */
#endif
  int arena_idx;
} mrb_gc;

MRB_API mrb_bool mrb_object_dead_p(struct mrb_state *mrb, struct RBasic *object);

#define MRB_GC_RED 7

MRB_END_DECL

#endif  /* MRUBY_GC_H */
