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
#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/abbreviation.h"
#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/buf.h"
#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/data.h"
#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/pcrange.h"
#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/section.h"
#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/tag.h"
#include "ten_utils/macro/mark.h"

/* Add a new compilation unit address range to a vector.  This is
   called via add_ranges.  Returns 1 on success, 0 on failure.  */
static int add_unit_addr(TEN_UNUSED ten_backtrace_t *self, void *rdata,
                         uintptr_t lowpc, uintptr_t highpc,
                         TEN_UNUSED ten_backtrace_on_error_func_t on_error,
                         TEN_UNUSED void *data, void *pvec) {
  unit *u = (unit *)rdata;
  unit_addrs_vector *vec = (unit_addrs_vector *)pvec;
  unit_addrs *p = NULL;

  /* Try to merge with the last entry.  */
  if (vec->count > 0) {
    p = (unit_addrs *)vec->vec.data + (vec->count - 1);
    if ((lowpc == p->high || lowpc == p->high + 1) && u == p->u) {
      if (highpc > p->high) {
        p->high = highpc;
      }
      return 1;
    }
  }

  p = (unit_addrs *)ten_vector_grow(&vec->vec, sizeof(unit_addrs));
  if (p == NULL) {
    return 0;
  }

  p->low = lowpc;
  p->high = highpc;
  p->u = u;

  ++vec->count;

  return 1;
}

/* Find the address range covered by a compilation unit, reading from
   UNIT_BUF and adding values to U.  Returns 1 if all data could be
   read, 0 if there is some error.  */
static int find_address_ranges(ten_backtrace_t *self, uintptr_t base_address,
                               dwarf_buf *unit_buf,
                               const dwarf_sections *dwarf_sections,
                               int is_bigendian, dwarf_data *altlink,
                               ten_backtrace_on_error_func_t on_error,
                               void *data, unit *u, unit_addrs_vector *addrs,
                               dwarf_tag *unit_tag) {
  while (unit_buf->left > 0) {
    uint64_t code = 0;
    const abbrev *abbrev = NULL;
    pcrange pcrange;
    attr_val name_val;
    int have_name_val = 0;
    attr_val comp_dir_val;
    int have_comp_dir_val = 0;
    size_t i = 0;

    code = read_uleb128(self, unit_buf);
    if (code == 0) {
      return 1;
    }

    abbrev = lookup_abbrev(self, &u->abbrevs, code, on_error, data);
    if (abbrev == NULL) {
      return 0;
    }

    if (unit_tag != NULL) {
      *unit_tag = abbrev->tag;
    }

    memset(&pcrange, 0, sizeof pcrange);
    memset(&name_val, 0, sizeof name_val);
    have_name_val = 0;
    memset(&comp_dir_val, 0, sizeof comp_dir_val);
    have_comp_dir_val = 0;
    for (i = 0; i < abbrev->num_attrs; ++i) {
      attr_val val;

      if (!read_attribute(self, abbrev->attrs[i].form, abbrev->attrs[i].val,
                          unit_buf, u->is_dwarf64, u->version, u->addrsize,
                          dwarf_sections, altlink, &val)) {
        return 0;
      }

      switch (abbrev->attrs[i].name) {
      case DW_AT_low_pc:
      case DW_AT_high_pc:
      case DW_AT_ranges:
        update_pcrange(&abbrev->attrs[i], &val, &pcrange);
        break;

      case DW_AT_stmt_list:
        if ((abbrev->tag == DW_TAG_compile_unit ||
             abbrev->tag == DW_TAG_skeleton_unit) &&
            (val.encoding == ATTR_VAL_UINT ||
             val.encoding == ATTR_VAL_REF_SECTION)) {
          u->lineoff = val.u.uint;
        }
        break;

      case DW_AT_name:
        if (abbrev->tag == DW_TAG_compile_unit ||
            abbrev->tag == DW_TAG_skeleton_unit) {
          name_val = val;
          have_name_val = 1;
        }
        break;

      case DW_AT_comp_dir:
        if (abbrev->tag == DW_TAG_compile_unit ||
            abbrev->tag == DW_TAG_skeleton_unit) {
          comp_dir_val = val;
          have_comp_dir_val = 1;
        }
        break;

      case DW_AT_str_offsets_base:
        if ((abbrev->tag == DW_TAG_compile_unit ||
             abbrev->tag == DW_TAG_skeleton_unit) &&
            val.encoding == ATTR_VAL_REF_SECTION) {
          u->str_offsets_base = val.u.uint;
        }
        break;

      case DW_AT_addr_base:
        if ((abbrev->tag == DW_TAG_compile_unit ||
             abbrev->tag == DW_TAG_skeleton_unit) &&
            val.encoding == ATTR_VAL_REF_SECTION) {
          u->addr_base = val.u.uint;
        }
        break;

      case DW_AT_rnglists_base:
        if ((abbrev->tag == DW_TAG_compile_unit ||
             abbrev->tag == DW_TAG_skeleton_unit) &&
            val.encoding == ATTR_VAL_REF_SECTION) {
          u->rnglists_base = val.u.uint;
        }
        break;

      default:
        break;
      }
    }

    // Resolve strings after we're sure that we have seen
    // DW_AT_str_offsets_base.
    if (have_name_val) {
      if (!resolve_string(self, dwarf_sections, u->is_dwarf64, is_bigendian,
                          u->str_offsets_base, &name_val, on_error, data,
                          &u->filename)) {
        return 0;
      }
    }
    if (have_comp_dir_val) {
      if (!resolve_string(self, dwarf_sections, u->is_dwarf64, is_bigendian,
                          u->str_offsets_base, &comp_dir_val, on_error, data,
                          &u->comp_dir)) {
        return 0;
      }
    }

    if (abbrev->tag == DW_TAG_compile_unit ||
        abbrev->tag == DW_TAG_subprogram ||
        abbrev->tag == DW_TAG_skeleton_unit) {
      if (!add_ranges(self, dwarf_sections, base_address, is_bigendian, u,
                      pcrange.lowpc, &pcrange, add_unit_addr, (void *)u,
                      on_error, data, (void *)addrs)) {
        return 0;
      }

      /* If we found the PC range in the DW_TAG_compile_unit or
         DW_TAG_skeleton_unit, we can stop now.  */
      if ((abbrev->tag == DW_TAG_compile_unit ||
           abbrev->tag == DW_TAG_skeleton_unit) &&
          (pcrange.have_ranges ||
           (pcrange.have_lowpc && pcrange.have_highpc))) {
        return 1;
      }
    }

    if (abbrev->has_children) {
      if (!find_address_ranges(self, base_address, unit_buf, dwarf_sections,
                               is_bigendian, altlink, on_error, data, u, addrs,
                               NULL)) {
        return 0;
      }
    }
  }

  return 1;
}

/* Build a mapping from address ranges to the compilation units where
   the line number information for that range can be found.  Returns 1
   on success, 0 on failure.  */
int build_address_map(ten_backtrace_t *self, uintptr_t base_address,
                      const dwarf_sections *dwarf_sections, int is_bigendian,
                      dwarf_data *altlink,
                      ten_backtrace_on_error_func_t on_error, void *data,
                      unit_addrs_vector *addrs, unit_vector *unit_vec) {
  dwarf_buf info;
  ten_vector_t units;
  size_t units_count = 0;
  size_t i = 0;
  unit **pu = NULL;
  size_t unit_offset = 0;
  unit_addrs *pa = NULL;

  memset(&addrs->vec, 0, sizeof addrs->vec);
  memset(&unit_vec->vec, 0, sizeof unit_vec->vec);
  addrs->count = 0;
  unit_vec->count = 0;

  /* Read through the .debug_info section.  FIXME: Should we use the
     .debug_aranges section?  gdb and addr2line don't use it, but I'm
     not sure why.  */

  info.name = ".debug_info";
  info.start = dwarf_sections->data[DEBUG_INFO];
  info.buf = info.start;
  info.left = dwarf_sections->size[DEBUG_INFO];
  info.is_bigendian = is_bigendian;
  info.on_error = on_error;
  info.data = data;
  info.reported_underflow = 0;

  memset(&units, 0, sizeof units);
  units_count = 0;

  while (info.left > 0) {
    const unsigned char *unit_data_start = NULL;
    uint64_t len = 0;
    int is_dwarf64 = 0;
    dwarf_buf unit_buf;
    int version = 0;
    int unit_type = 0;
    uint64_t abbrev_offset = 0;
    int addrsize = 0;
    unit *u = NULL;
    dwarf_tag unit_tag = 0;

    if (info.reported_underflow) {
      goto fail;
    }

    unit_data_start = info.buf;

    len = read_initial_length(self, &info, &is_dwarf64);
    unit_buf = info;
    unit_buf.left = len;

    if (!advance(self, &info, len)) {
      goto fail;
    }

    version = read_uint16(self, &unit_buf);
    if (version < 2 || version > 5) {
      dwarf_buf_error(self, &unit_buf, "unrecognized DWARF version", -1);
      goto fail;
    }

    if (version < 5) {
      unit_type = 0;
    } else {
      unit_type = read_byte(self, &unit_buf);
      if (unit_type == DW_UT_type || unit_type == DW_UT_split_type) {
        /* This unit doesn't have anything we need.  */
        continue;
      }
    }

    pu = (unit **)ten_vector_grow(&units, sizeof(unit *));
    if (pu == NULL) {
      goto fail;
    }

    u = (unit *)malloc(sizeof *u);
    if (u == NULL) {
      goto fail;
    }

    *pu = u;
    ++units_count;

    if (version < 5) {
      addrsize = 0; /* Set below.  */
    } else {
      addrsize = read_byte(self, &unit_buf);
    }

    memset(&u->abbrevs, 0, sizeof u->abbrevs);
    abbrev_offset = read_offset(self, &unit_buf, is_dwarf64);
    if (!read_abbrevs(self, abbrev_offset, dwarf_sections->data[DEBUG_ABBREV],
                      dwarf_sections->size[DEBUG_ABBREV], is_bigendian,
                      on_error, data, &u->abbrevs)) {
      goto fail;
    }

    if (version < 5) {
      addrsize = read_byte(self, &unit_buf);
    }

    switch (unit_type) {
    case 0:
    case DW_UT_compile:
    case DW_UT_partial:
      break;
    case DW_UT_skeleton:
    case DW_UT_split_compile:
      read_uint64(self, &unit_buf); /* dwo_id */
      break;
    default:
      break;
    }

    u->low_offset = unit_offset;
    unit_offset += len + (is_dwarf64 ? 12 : 4);
    u->high_offset = unit_offset;
    u->unit_data = unit_buf.buf;
    u->unit_data_len = unit_buf.left;
    u->unit_data_offset = unit_buf.buf - unit_data_start;
    u->version = version;
    u->is_dwarf64 = is_dwarf64;
    u->addrsize = addrsize;
    u->filename = NULL;
    u->comp_dir = NULL;
    u->abs_filename = NULL;
    u->lineoff = 0;
    u->str_offsets_base = 0;
    u->addr_base = 0;
    u->rnglists_base = 0;

    /* The actual line number mappings will be read as needed.  */
    u->lines = NULL;
    u->lines_count = 0;
    u->function_addrs = NULL;
    u->function_addrs_count = 0;

    if (!find_address_ranges(self, base_address, &unit_buf, dwarf_sections,
                             is_bigendian, altlink, on_error, data, u, addrs,
                             &unit_tag)) {
      goto fail;
    }

    if (unit_buf.reported_underflow) {
      goto fail;
    }
  }

  if (info.reported_underflow) {
    goto fail;
  }

  /* Add a trailing addrs entry, but don't include it in addrs->count.  */
  pa = (unit_addrs *)ten_vector_grow(&addrs->vec, sizeof(unit_addrs));
  if (pa == NULL) {
    goto fail;
  }

  pa->low = 0;
  --pa->low;
  pa->high = pa->low;
  pa->u = NULL;

  unit_vec->vec = units;
  unit_vec->count = units_count;

  return 1;

fail:
  if (units_count > 0) {
    pu = (unit **)units.data;
    for (i = 0; i < units_count; i++) {
      free_abbrevs(self, &pu[i]->abbrevs, on_error, data);
      free(pu[i]);
    }
    ten_vector_deinit(&units);
  }
  if (addrs->count > 0) {
    ten_vector_deinit(&addrs->vec);
    addrs->count = 0;
  }
  return 0;
}
