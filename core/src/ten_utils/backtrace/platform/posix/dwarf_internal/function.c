//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/ten_config.h"

#include <stdlib.h>
#include <string.h>

#include "include_internal/ten_utils/backtrace/backtrace.h"
#include "include_internal/ten_utils/backtrace/platform/posix/dwarf.h"
#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/pcrange.h"
#include "include_internal/ten_utils/backtrace/sort.h"
#include "ten_utils/macro/mark.h"

// Add a range to a unit that maps to a function.  This is called via
// add_ranges.  Returns 1 on success, 0 on error.
static int add_function_range(TEN_UNUSED ten_backtrace_t *self, void *rdata,
                              uintptr_t lowpc, uintptr_t highpc,
                              TEN_UNUSED ten_backtrace_on_error_func_t on_error,
                              TEN_UNUSED void *data, void *pvec) {
  function *func = (function *)rdata;
  function_vector *vec = (function_vector *)pvec;
  function_addrs *p = NULL;

  if (vec->count > 0) {
    p = (function_addrs *)vec->vec.data + (vec->count - 1);
    if ((lowpc == p->high || lowpc == p->high + 1) && func == p->func) {
      if (highpc > p->high) {
        p->high = highpc;
      }
      return 1;
    }
  }

  p = (function_addrs *)ten_vector_grow(&vec->vec, sizeof(function_addrs));
  if (p == NULL) {
    return 0;
  }

  p->low = lowpc;
  p->high = highpc;
  p->func = func;

  ++vec->count;

  return 1;
}

// Compare function_addrs for qsort.  When ranges are nested, make the
// smallest one sort last.
static int function_addrs_compare(const void *v1, const void *v2) {
  const function_addrs *a1 = (const function_addrs *)v1;
  const function_addrs *a2 = (const function_addrs *)v2;

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

  return strcmp(a1->func->name, a2->func->name);
}

// Read one entry plus all its children.  Add function addresses to
// VEC.  Returns 1 on success, 0 on error.
static int read_function_entry(ten_backtrace_t *self, dwarf_data *ddata,
                               unit *u, uintptr_t base, dwarf_buf *unit_buf,
                               const line_header *lhdr,
                               ten_backtrace_on_error_func_t on_error,
                               void *data, function_vector *vec_function,
                               function_vector *vec_inlined) {
  while (unit_buf->left > 0) {
    const abbrev *abbrev = NULL;
    int is_function = 0;
    function *func = NULL;
    function_vector *vec = NULL;
    size_t i = 0;
    pcrange pcrange;
    int have_linkage_name = 0;

    uint64_t code = read_uleb128(self, unit_buf);
    if (code == 0) {
      return 1;
    }

    abbrev = lookup_abbrev(self, &u->abbrevs, code, on_error, data);
    if (abbrev == NULL) {
      return 0;
    }

    is_function = (abbrev->tag == DW_TAG_subprogram ||
                   abbrev->tag == DW_TAG_entry_point ||
                   abbrev->tag == DW_TAG_inlined_subroutine);

    if (abbrev->tag == DW_TAG_inlined_subroutine) {
      vec = vec_inlined;
    } else {
      vec = vec_function;
    }

    func = NULL;
    if (is_function) {
      func = (function *)malloc(sizeof *func);
      if (func == NULL) {
        return 0;
      }

      memset(func, 0, sizeof *func);
    }

    memset(&pcrange, 0, sizeof pcrange);
    have_linkage_name = 0;
    for (i = 0; i < abbrev->num_attrs; ++i) {
      attr_val val;

      if (!read_attribute(self, abbrev->attrs[i].form, abbrev->attrs[i].val,
                          unit_buf, u->is_dwarf64, u->version, u->addrsize,
                          &ddata->dwarf_sections, ddata->altlink, &val)) {
        return 0;
      }

      // The compile unit sets the base address for any address
      // ranges in the function entries.
      if ((abbrev->tag == DW_TAG_compile_unit ||
           abbrev->tag == DW_TAG_skeleton_unit) &&
          abbrev->attrs[i].name == DW_AT_low_pc) {
        if (val.encoding == ATTR_VAL_ADDRESS) {
          base = (uintptr_t)val.u.uint;
        } else if (val.encoding == ATTR_VAL_ADDRESS_INDEX) {
          if (!resolve_addr_index(self, &ddata->dwarf_sections, u->addr_base,
                                  u->addrsize, ddata->is_bigendian, val.u.uint,
                                  on_error, data, &base)) {
            return 0;
          }
        }
      }

      if (is_function) {
        switch (abbrev->attrs[i].name) {
        case DW_AT_call_file:
          if (val.encoding == ATTR_VAL_UINT) {
            if (val.u.uint >= lhdr->filenames_count) {
              dwarf_buf_error(self, unit_buf,
                              ("Invalid file number in "
                               "DW_AT_call_file attribute"),
                              0);
              return 0;
            }
            func->caller_filename = lhdr->filenames[val.u.uint];
          }
          break;

        case DW_AT_call_line:
          if (val.encoding == ATTR_VAL_UINT) {
            func->caller_lineno = val.u.uint;
          }
          break;

        case DW_AT_abstract_origin:
        case DW_AT_specification:
          // Second name preference: override DW_AT_name, don't override
          // DW_AT_linkage_name.
          if (have_linkage_name) {
            break;
          }

          {
            const char *name = read_referenced_name_from_attr(
                self, ddata, u, &abbrev->attrs[i], &val, on_error, data);
            if (name != NULL) {
              func->name = name;
            }
          }
          break;

        case DW_AT_name:
          // Third name preference: don't override.
          if (func->name != NULL) {
            break;
          }
          if (!resolve_string(self, &ddata->dwarf_sections, u->is_dwarf64,
                              ddata->is_bigendian, u->str_offsets_base, &val,
                              on_error, data, &func->name)) {
            return 0;
          }
          break;

        case DW_AT_linkage_name:
        case DW_AT_MIPS_linkage_name:
          // First name preference: override all.
          {
            const char *s = NULL;
            if (!resolve_string(self, &ddata->dwarf_sections, u->is_dwarf64,
                                ddata->is_bigendian, u->str_offsets_base, &val,
                                on_error, data, &s)) {
              return 0;
            }
            if (s != NULL) {
              func->name = s;
              have_linkage_name = 1;
            }
          }
          break;

        case DW_AT_low_pc:
        case DW_AT_high_pc:
        case DW_AT_ranges:
          update_pcrange(&abbrev->attrs[i], &val, &pcrange);
          break;

        default:
          break;
        }
      }
    }

    // If we couldn't find a name for the function, we have no use
    // for it.
    if (is_function && func->name == NULL) {
      free(func);
      is_function = 0;
    }

    if (is_function) {
      if (pcrange.have_ranges || (pcrange.have_lowpc && pcrange.have_highpc)) {
        if (!add_ranges(self, &ddata->dwarf_sections, ddata->base_address,
                        ddata->is_bigendian, u, base, &pcrange,
                        add_function_range, (void *)func, on_error, data,
                        (void *)vec)) {
          return 0;
        }
      } else {
        free(func);
        is_function = 0;
      }
    }

    if (abbrev->has_children) {
      if (!is_function) {
        if (!read_function_entry(self, ddata, u, base, unit_buf, lhdr, on_error,
                                 data, vec_function, vec_inlined)) {
          return 0;
        }
      } else {
        function_vector fvec;

        // Gather any information for inlined functions in
        // FVEC.

        memset(&fvec, 0, sizeof fvec);

        if (!read_function_entry(self, ddata, u, base, unit_buf, lhdr, on_error,
                                 data, vec_function, &fvec)) {
          return 0;
        }

        if (fvec.count > 0) {
          // Allocate a trailing entry, but don't include it
          // in fvec.count.
          function_addrs *p = (function_addrs *)ten_vector_grow(
              &fvec.vec, sizeof(function_addrs));
          if (p == NULL) {
            return 0;
          }

          p->low = 0;
          --p->low;
          p->high = p->low;
          p->func = NULL;

          if (!ten_vector_release_remaining_space(&fvec.vec)) {
            return 0;
          }

          function_addrs *faddrs = (function_addrs *)fvec.vec.data;
          backtrace_sort(faddrs, fvec.count, sizeof(function_addrs),
                         function_addrs_compare);

          func->function_addrs = faddrs;
          func->function_addrs_count = fvec.count;
        }
      }
    }
  }

  return 1;
}

/**
 * @brief Read function name information for a compilation unit. We look through
 * the whole unit looking for function tags.
 */
void read_function_info(ten_backtrace_t *self, dwarf_data *ddata,
                        const line_header *lhdr,
                        ten_backtrace_on_error_func_t on_error, void *data,
                        unit *u, function_vector *fvec,
                        function_addrs **ret_addrs, size_t *ret_addrs_count) {
  function_vector lvec;
  function_vector *pfvec = NULL;
  dwarf_buf unit_buf;
  function_addrs *p = NULL;
  function_addrs *addrs = NULL;
  size_t addrs_count = 0;

  // Use FVEC if it is not NULL. Otherwise use our own vector.
  if (fvec != NULL) {
    pfvec = fvec;
  } else {
    memset(&lvec, 0, sizeof lvec);
    pfvec = &lvec;
  }

  unit_buf.name = ".debug_info";
  unit_buf.start = ddata->dwarf_sections.data[DEBUG_INFO];
  unit_buf.buf = u->unit_data;
  unit_buf.left = u->unit_data_len;
  unit_buf.is_bigendian = ddata->is_bigendian;
  unit_buf.on_error = on_error;
  unit_buf.data = data;
  unit_buf.reported_underflow = 0;

  while (unit_buf.left > 0) {
    if (!read_function_entry(self, ddata, u, 0, &unit_buf, lhdr, on_error, data,
                             pfvec, pfvec)) {
      return;
    }
  }

  if (pfvec->count == 0) {
    return;
  }

  // Allocate a trailing entry, but don't include it in pfvec->count.
  p = (function_addrs *)ten_vector_grow(&pfvec->vec, sizeof(function_addrs));
  if (p == NULL) {
    return;
  }

  p->low = 0;
  --p->low;
  p->high = p->low;
  p->func = NULL;

  addrs_count = pfvec->count;

  if (fvec == NULL) {
    if (!ten_vector_release_remaining_space(&lvec.vec)) {
      return;
    }

    addrs = (function_addrs *)pfvec->vec.data;
  } else {
    // Finish this list of addresses, but leave the remaining space in the
    // vector available for the next function unit.
    addrs = (function_addrs *)ten_vector_take_out(&fvec->vec);
    if (addrs == NULL) {
      return;
    }

    fvec->count = 0;
  }

  backtrace_sort(addrs, addrs_count, sizeof(function_addrs),
                 function_addrs_compare);

  *ret_addrs = addrs;
  *ret_addrs_count = addrs_count;
}

// Compare a PC against a function_addrs for bsearch.  We always
// allocate an entra entry at the end of the vector, so that this
// routine can safely look at the next entry.  Note that if there are
// multiple ranges containing PC, which one will be returned is
// unpredictable.  We compensate for that in dwarf_fileline.
int function_addrs_search(const void *vkey, const void *ventry) {
  const uintptr_t *key = (const uintptr_t *)vkey;
  const function_addrs *entry = (const function_addrs *)ventry;
  uintptr_t pc = *key;
  if (pc < entry->low) {
    return -1;
  } else if (pc > (entry + 1)->low) {
    return 1;
  } else {
    return 0;
  }
}

// See if PC is inlined in FUNCTION.  If it is, print out the inlined
// information, and update FILENAME and LINENO for the caller.
// Returns whatever CALLBACK returns, or 0 to keep going.
int report_inlined_functions(
    ten_backtrace_t *self, uintptr_t pc, function *func,
    ten_backtrace_on_dump_file_line_func_t on_dump_file_line, void *data,
    const char **filename, int *lineno) {
  function_addrs *p = NULL;
  function_addrs *match = NULL;
  function *inlined = NULL;
  int ret = 0;

  if (func->function_addrs_count == 0) {
    return 0;
  }

  // Our search isn't safe if pc == -1, as that is the sentinel
  // value.
  if (pc + 1 == 0) {
    return 0;
  }

  p = ((function_addrs *)bsearch(
      &pc, func->function_addrs, func->function_addrs_count,
      sizeof(function_addrs), function_addrs_search));
  if (p == NULL) {
    return 0;
  }

  // Here pc >= p->low && pc < (p + 1)->low.  The function_addrs are
  // sorted by low, so if pc > p->low we are at the end of a range of
  // function_addrs with the same low value.  If pc == p->low walk
  // forward to the end of the range with that low value.  Then walk
  // backward and use the first range that includes pc.
  while (pc == (p + 1)->low) {
    ++p;
  }

  match = NULL;
  while (1) {
    if (pc < p->high) {
      match = p;
      break;
    }
    if (p == func->function_addrs) {
      break;
    }
    if ((p - 1)->low < p->low) {
      break;
    }
    --p;
  }
  if (match == NULL) {
    return 0;
  }

  // We found an inlined call.

  inlined = match->func;

  // Report any calls inlined into this one.
  ret = report_inlined_functions(self, pc, inlined, on_dump_file_line, data,
                                 filename, lineno);
  if (ret != 0) {
    return ret;
  }

  // Report this inlined call.
  ret = on_dump_file_line(self, pc, *filename, *lineno, inlined->name, data);
  if (ret != 0) {
    return ret;
  }

  // Our caller will report the caller of the inlined function; tell
  // it the appropriate filename and line number.
  *filename = inlined->caller_filename;
  *lineno = inlined->caller_lineno;

  return 0;
}
