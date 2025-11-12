//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
// This file is modified from
// https://github.com/ianlancetaylor/libbacktrace [BSD license]
//
#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/backtrace/platform/posix/dwarf.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "include_internal/ten_utils/backtrace/backtrace.h"
#include "include_internal/ten_utils/backtrace/platform/posix/internal.h"
#include "include_internal/ten_utils/backtrace/sort.h"
#include "include_internal/ten_utils/backtrace/vector.h"
#include "ten_utils/lib/atomic.h"
#include "ten_utils/lib/atomic_ptr.h"

// Report an error for a DWARF buffer.
void dwarf_buf_error(ten_backtrace_t *self, dwarf_buf *buf, const char *msg,
                     int errnum) {
  char b[200];

  int written = snprintf(b, sizeof b, "%s in %s at %d", msg, buf->name,
                         (int)(buf->buf - buf->start));
  assert(written > 0);

  buf->on_error(self, b, errnum, NULL);
}

// Require at least COUNT bytes in BUF.
// Return true if all is well, false on error.
static bool require(ten_backtrace_t *self, dwarf_buf *buf, size_t count) {
  assert(self);
  assert(buf);

  if (buf->left >= count) {
    return true;
  }

  if (!buf->reported_underflow) {
    dwarf_buf_error(self, buf, "DWARF underflow", 0);
    buf->reported_underflow = 1;
  }

  return false;
}

// Advance COUNT bytes in BUF.
// Return true if all is well, false on error.
bool advance(ten_backtrace_t *self, dwarf_buf *buf, size_t count) {
  assert(self);
  assert(buf);

  if (!require(self, buf, count)) {
    return false;
  }

  buf->buf += count;
  buf->left -= count;

  return true;
}

// Compare unit_addrs for qsort. When ranges are nested, make the smallest one
// sort last.
static int unit_addrs_compare(const void *v1, const void *v2) {
  const unit_addrs *a1 = (const unit_addrs *)v1;
  const unit_addrs *a2 = (const unit_addrs *)v2;

  if (a1->low < a2->low) {
    return -1;
  }
  if (a1->low > a2->low) {
    return 1;
  }
  if (a1->high < a2->high) {
    return 1;
  }
  if (a1->high > a2->high) {
    return -1;
  }
  if (a1->u->lineoff < a2->u->lineoff) {
    return -1;
  }
  if (a1->u->lineoff > a2->u->lineoff) {
    return 1;
  }
  return 0;
}

// Compare a PC against a unit_addrs for bsearch.  We always allocate
// an entry entry at the end of the vector, so that this routine can
// safely look at the next entry.  Note that if there are multiple
// ranges containing PC, which one will be returned is unpredictable.
// We compensate for that in dwarf_fileline.
static int unit_addrs_search(const void *vkey, const void *ventry) {
  const uintptr_t *key = (const uintptr_t *)vkey;
  const unit_addrs *entry = (const unit_addrs *)ventry;
  uintptr_t pc = *key;
  if (pc < entry->low) {
    return -1;
  } else if (pc > (entry + 1)->low) {
    return 1;
  } else {
    return 0;
  }
}

// Find a PC in a line vector.  We always allocate an extra entry at
// the end of the lines vector, so that this routine can safely look
// at the next entry.  Note that when there are multiple mappings for
// the same PC value, this will return the last one.
static int line_search(const void *vkey, const void *ventry) {
  const uintptr_t *key = (const uintptr_t *)vkey;
  const line *entry = (const line *)ventry;
  uintptr_t pc = *key;
  if (pc < entry->pc) {
    return -1;
  } else if (pc >= (entry + 1)->pc) {
    return 1;
  } else {
    return 0;
  }
}

/**
 * @brief Look for a @a pc in the DWARF mapping for one module. On success, call
 * @a callback and return whatever it returns. On error, call @a on_error
 * and return 0. Sets @a *found to 1 if the @a pc is found, 0 if not.
 */
static int dwarf_lookup_pc(
    ten_backtrace_t *self, dwarf_data *ddata, uintptr_t pc,
    ten_backtrace_on_dump_file_line_func_t on_dump_file_line,
    ten_backtrace_on_error_func_t on_error, void *data, int *found) {
  unit_addrs *entry = NULL;
  int found_entry = 0;
  unit *u = NULL;
  int new_data = 0;
  line *lines = NULL;
  line *ln = NULL;
  function_addrs *p = NULL;
  function_addrs *fmatch = NULL;
  function *function = NULL;
  const char *filename = NULL;
  int lineno = 0;
  int ret = 0;

  *found = 1;

  // Find an address range that includes PC.  Our search isn't safe if
  // PC == -1, as we use that as a sentinel value, so skip the search
  // in that case.
  entry = (ddata->addrs_count == 0 || pc + 1 == 0
               ? NULL
               : bsearch(&pc, ddata->addrs, ddata->addrs_count,
                         sizeof(unit_addrs), unit_addrs_search));

  if (entry == NULL) {
    *found = 0;
    return 0;
  }

  // Here pc >= entry->low && pc < (entry + 1)->low.  The unit_addrs
  // are sorted by low, so if pc > p->low we are at the end of a range
  // of unit_addrs with the same low value.  If pc == p->low walk
  // forward to the end of the range with that low value.  Then walk
  // backward and use the first range that includes pc.
  while (pc == (entry + 1)->low) {
    ++entry;
  }

  found_entry = 0;

  while (1) {
    if (pc < entry->high) {
      found_entry = 1;
      break;
    }
    if (entry == ddata->addrs) {
      break;
    }

    if ((entry - 1)->low < entry->low) {
      break;
    }
    --entry;
  }
  if (!found_entry) {
    *found = 0;
    return 0;
  }

  // We need the lines, lines_count, function_addrs,
  // function_addrs_count fields of u.  If they are not set, we need
  // to set them.  When running in threaded mode, we need to allow for
  // the possibility that some other thread is setting them
  // simultaneously.

  u = entry->u;
  lines = u->lines;

  // Skip units with no useful line number information by walking
  // backward.  Useless line number information is marked by setting
  // lines == -1.
  while (entry > ddata->addrs && pc >= (entry - 1)->low &&
         pc < (entry - 1)->high) {
    lines = (line *)ten_atomic_ptr_load((ten_atomic_ptr_t *)&u->lines);
    if (lines != (line *)(uintptr_t)-1) {
      break;
    }

    --entry;

    u = entry->u;
    lines = u->lines;
  }

  lines = ten_atomic_ptr_load((ten_atomic_ptr_t *)&u->lines);

  new_data = 0;
  if (lines == NULL) {
    function_addrs *function_addrs = NULL;
    size_t function_addrs_count = 0;
    size_t count = 0;
    line_header lhdr;

    // We have never read the line information for this unit. Read it now.

    function_addrs = NULL;
    function_addrs_count = 0;
    if (read_line_info(self, ddata, on_error, data, entry->u, &lhdr, &lines,
                       &count)) {
      function_vector *pfvec = NULL;

      read_function_info(self, ddata, &lhdr, on_error, data, entry->u, pfvec,
                         &function_addrs, &function_addrs_count);
      free_line_header(self, &lhdr, on_error, data);
      new_data = 1;
    }

    // Atomically store the information we just read into the unit.
    // If another thread is simultaneously writing, it presumably
    // read the same information, and we don't care which one we
    // wind up with; we just leak the other one.  We do have to
    // write the lines field last, so that the acquire-loads above
    // ensure that the other fields are set.

    ten_atomic_store(&u->lines_count, count);
    ten_atomic_store(&u->function_addrs_count, function_addrs_count);
    ten_atomic_ptr_store((ten_atomic_ptr_t *)&u->function_addrs,
                         function_addrs);
    ten_atomic_ptr_store((ten_atomic_ptr_t *)&u->lines, lines);
  }

  // Now all fields of U have been initialized.

  if (lines == (line *)(uintptr_t)-1) {
    // If reading the line number information failed in some way,
    // try again to see if there is a better compilation unit for
    // this PC.
    if (new_data) {
      return dwarf_lookup_pc(self, ddata, pc, on_dump_file_line, on_error, data,
                             found);
    }

    return on_dump_file_line(self, pc, NULL, 0, NULL, data);
  }

  // Search for PC within this unit.

  ln = (line *)bsearch(&pc, lines, entry->u->lines_count, sizeof(line),
                       line_search);
  if (ln == NULL) {
    // The PC is between the low_pc and high_pc attributes of the
    // compilation unit, but no entry in the line table covers it.
    // This implies that the start of the compilation unit has no
    // line number information.

    if (entry->u->abs_filename == NULL) {
      const char *filename = entry->u->filename;
      if (filename != NULL && !IS_ABSOLUTE_PATH(filename) &&
          entry->u->comp_dir != NULL) {
        size_t filename_len = 0;
        const char *dir = NULL;
        size_t dir_len = 0;
        char *s = NULL;

        filename_len = strlen(filename);
        dir = entry->u->comp_dir;
        dir_len = strlen(dir);
        s = (char *)malloc(dir_len + filename_len + 2);
        if (s == NULL) {
          *found = 0;
          return 0;
        }

        memcpy(s, dir, dir_len);
        // TODO(Wei): Should use backslash if DOS file system.
        s[dir_len] = '/';
        memcpy(s + dir_len + 1, filename, filename_len + 1);
        filename = s;
      }
      entry->u->abs_filename = filename;
    }

    return on_dump_file_line(self, pc, entry->u->abs_filename, 0, NULL, data);
  }

  // Search for function name within this unit.

  if (entry->u->function_addrs_count == 0) {
    return on_dump_file_line(self, pc, ln->filename, ln->lineno, NULL, data);
  }

  p = ((function_addrs *)bsearch(
      &pc, entry->u->function_addrs, entry->u->function_addrs_count,
      sizeof(function_addrs), function_addrs_search));
  if (p == NULL) {
    return on_dump_file_line(self, pc, ln->filename, ln->lineno, NULL, data);
  }

  // Here pc >= p->low && pc < (p + 1)->low.  The function_addrs are
  // sorted by low, so if pc > p->low we are at the end of a range of
  // function_addrs with the same low value.  If pc == p->low walk
  // forward to the end of the range with that low value.  Then walk
  // backward and use the first range that includes pc.
  while (pc == (p + 1)->low) {
    ++p;
  }

  fmatch = NULL;
  while (1) {
    if (pc < p->high) {
      fmatch = p;
      break;
    }

    if (p == entry->u->function_addrs) {
      break;
    }

    if ((p - 1)->low < p->low) {
      break;
    }

    --p;
  }

  if (fmatch == NULL) {
    return on_dump_file_line(self, pc, ln->filename, ln->lineno, NULL, data);
  }

  function = fmatch->func;

  filename = ln->filename;
  lineno = ln->lineno;

  ret = report_inlined_functions(self, pc, function, on_dump_file_line, data,
                                 &filename, &lineno);
  if (ret != 0) {
    return ret;
  }

  return on_dump_file_line(self, pc, filename, lineno, function->name, data);
}

/**
 * @brief Return the file/line information for a PC using the DWARF mapping we
 * built earlier.
 */
static int dwarf_fileline(
    ten_backtrace_t *self, uintptr_t pc,
    ten_backtrace_on_dump_file_line_func_t on_dump_file_line,
    ten_backtrace_on_error_func_t on_error, void *data) {
  assert(self);

  ten_backtrace_posix_t *self_posix = (ten_backtrace_posix_t *)self;

  dwarf_data **pp = (dwarf_data **)&self_posix->on_get_file_line_data;
  while (1) {
    dwarf_data *ddata = ten_atomic_ptr_load((ten_atomic_ptr_t *)pp);
    if (ddata == NULL) {
      break;
    }

    int found = 0;
    int ret = dwarf_lookup_pc(self, ddata, pc, on_dump_file_line, on_error,
                              data, &found);
    if (ret != 0 || found) {
      return ret;
    }

    pp = &ddata->next;
  }

  // TODO(Wei): See if any libraries have been dlopen'ed.

  return on_dump_file_line(self, pc, NULL, 0, NULL, data);
}

// Initialize our data structures from the DWARF debug info for a
// file.  Return NULL on failure.
static dwarf_data *build_dwarf_data(ten_backtrace_t *self,
                                    uintptr_t base_address,
                                    const dwarf_sections *dwarf_sections,
                                    int is_bigendian, dwarf_data *altlink,
                                    ten_backtrace_on_error_func_t on_error,
                                    void *data) {
  unit_addrs_vector addrs_vec;
  unit_addrs *addrs = NULL;
  size_t addrs_count = 0;
  unit_vector units_vec;
  unit **units = NULL;
  size_t units_count = 0;
  dwarf_data *fdata = NULL;

  if (!build_address_map(self, base_address, dwarf_sections, is_bigendian,
                         altlink, on_error, data, &addrs_vec, &units_vec)) {
    return NULL;
  }

  if (!ten_vector_release_remaining_space(&addrs_vec.vec)) {
    return NULL;
  }

  if (!ten_vector_release_remaining_space(&units_vec.vec)) {
    return NULL;
  }

  addrs = (unit_addrs *)addrs_vec.vec.data;
  units = (unit **)units_vec.vec.data;
  addrs_count = addrs_vec.count;
  units_count = units_vec.count;
  backtrace_sort(addrs, addrs_count, sizeof(unit_addrs), unit_addrs_compare);
  // No qsort for units required, already sorted.

  fdata = (dwarf_data *)malloc(sizeof(dwarf_data));
  if (fdata == NULL) {
    return NULL;
  }

  fdata->next = NULL;
  fdata->altlink = altlink;
  fdata->base_address = base_address;
  fdata->addrs = addrs;
  fdata->addrs_count = addrs_count;
  fdata->units = units;
  fdata->units_count = units_count;
  fdata->dwarf_sections = *dwarf_sections;
  fdata->is_bigendian = is_bigendian;
  memset(&fdata->fvec, 0, sizeof fdata->fvec);

  return fdata;
}

/**
 * @brief Build our data structures from the DWARF sections for a module. Set
 * @a fileline_fn and @a state->file_line_data.
 *
 * @return 1 on success, 0 on failure.
 */
int backtrace_dwarf_add(ten_backtrace_t *self, uintptr_t base_address,
                        const dwarf_sections *dwarf_sections, int is_bigendian,
                        dwarf_data *fileline_altlink,
                        ten_backtrace_on_error_func_t on_error, void *data,
                        ten_backtrace_on_get_file_line_func_t *on_get_file_line,
                        dwarf_data **fileline_entry) {
  ten_backtrace_posix_t *self_posix = (ten_backtrace_posix_t *)self;
  assert(self_posix && "Invalid argument.");

  dwarf_data *fdata =
      build_dwarf_data(self, base_address, dwarf_sections, is_bigendian,
                       fileline_altlink, on_error, data);
  if (fdata == NULL) {
    return 0;
  }

  if (fileline_entry != NULL) {
    *fileline_entry = fdata;
  }

  while (1) {
    dwarf_data **pp = (dwarf_data **)&self_posix->on_get_file_line_data;

    // Find the last element in the 'dwarf_data' list.
    while (1) {
      dwarf_data *p = ten_atomic_ptr_load((ten_atomic_ptr_t *)pp);
      if (p == NULL) {
        break;
      }

      pp = &p->next;
    }

    if (__sync_bool_compare_and_swap(pp, NULL, fdata)) {
      break;
    }
  }

  *on_get_file_line = dwarf_fileline;

  return 1;
}
