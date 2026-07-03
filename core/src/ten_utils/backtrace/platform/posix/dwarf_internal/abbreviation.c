//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/abbreviation.h"

#include <stdlib.h>
#include <string.h>

#include "include_internal/ten_utils/backtrace/backtrace.h"
#include "include_internal/ten_utils/backtrace/platform/posix/dwarf.h"
#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/buf.h"
#include "include_internal/ten_utils/backtrace/sort.h"

// Sort the abbrevs by the abbrev code.  This function is passed to
// both qsort and bsearch.
static int abbrev_compare(const void *v1, const void *v2) {
  const abbrev *a1 = (const abbrev *)v1;
  const abbrev *a2 = (const abbrev *)v2;

  if (a1->code < a2->code) {
    return -1;
  } else if (a1->code > a2->code) {
    return 1;
  } else {
    // This really shouldn't happen.  It means there are two
    // different abbrevs with the same code, and that means we don't
    // know which one lookup_abbrev should return.
    return 0;
  }
}

// Free an abbreviations structure.
void free_abbrevs(ten_backtrace_t *self, abbrevs *abbrevs,
                  ten_backtrace_on_error_func_t on_error, void *data) {
  size_t i = 0;

  for (i = 0; i < abbrevs->num_abbrevs; ++i) {
    free(abbrevs->abbrevs[i].attrs);
  }
  free(abbrevs->abbrevs);
  abbrevs->num_abbrevs = 0;
  abbrevs->abbrevs = NULL;
}

// Read the abbreviation table for a compilation unit.  Returns 1 on
// success, 0 on failure.
int read_abbrevs(ten_backtrace_t *self, uint64_t abbrev_offset,
                 const unsigned char *dwarf_abbrev, size_t dwarf_abbrev_size,
                 int is_bigendian, ten_backtrace_on_error_func_t on_error,
                 void *data, abbrevs *abbrevs) {
  dwarf_buf abbrev_buf;
  dwarf_buf count_buf;
  size_t num_abbrevs = 0;

  abbrevs->num_abbrevs = 0;
  abbrevs->abbrevs = NULL;

  if (abbrev_offset >= dwarf_abbrev_size) {
    on_error(self, "abbrev offset out of range", 0, data);
    return 0;
  }

  abbrev_buf.name = ".debug_abbrev";
  abbrev_buf.start = dwarf_abbrev;
  abbrev_buf.buf = dwarf_abbrev + abbrev_offset;
  abbrev_buf.left = dwarf_abbrev_size - abbrev_offset;
  abbrev_buf.is_bigendian = is_bigendian;
  abbrev_buf.on_error = on_error;
  abbrev_buf.data = data;
  abbrev_buf.reported_underflow = 0;

  // Count the number of abbrevs in this list.

  count_buf = abbrev_buf;
  num_abbrevs = 0;
  while (read_uleb128(self, &count_buf) != 0) {
    if (count_buf.reported_underflow) {
      return 0;
    }
    ++num_abbrevs;
    // Skip tag.
    read_uleb128(self, &count_buf);
    // Skip has_children.
    read_byte(self, &count_buf);
    // Skip attributes.
    while (read_uleb128(self, &count_buf) != 0) {
      uint64_t form = read_uleb128(self, &count_buf);
      if ((enum dwarf_form)form == DW_FORM_implicit_const) {
        read_sleb128(self, &count_buf);
      }
    }
    // Skip form of last attribute.
    read_uleb128(self, &count_buf);
  }

  if (count_buf.reported_underflow) {
    return 0;
  }

  if (num_abbrevs == 0) {
    return 1;
  }

  abbrevs->abbrevs = (abbrev *)malloc(num_abbrevs * sizeof(abbrev));
  if (abbrevs->abbrevs == NULL) {
    return 0;
  }

  abbrevs->num_abbrevs = num_abbrevs;
  memset(abbrevs->abbrevs, 0, num_abbrevs * sizeof(abbrev));

  num_abbrevs = 0;
  while (1) {
    uint64_t code = 0;
    abbrev a;
    size_t num_attrs = 0;
    attr *attrs = NULL;

    if (abbrev_buf.reported_underflow) {
      goto fail;
    }

    code = read_uleb128(self, &abbrev_buf);
    if (code == 0) {
      break;
    }

    a.code = code;
    a.tag = (enum dwarf_tag)read_uleb128(self, &abbrev_buf);
    a.has_children = read_byte(self, &abbrev_buf);

    count_buf = abbrev_buf;
    num_attrs = 0;
    while (read_uleb128(self, &count_buf) != 0) {
      ++num_attrs;

      uint64_t form = read_uleb128(self, &count_buf);
      if ((enum dwarf_form)form == DW_FORM_implicit_const) {
        read_sleb128(self, &count_buf);
      }
    }

    if (num_attrs == 0) {
      attrs = NULL;
      read_uleb128(self, &abbrev_buf);
      read_uleb128(self, &abbrev_buf);
    } else {
      attrs = (attr *)malloc(num_attrs * sizeof *attrs);
      if (attrs == NULL) {
        goto fail;
      }

      num_attrs = 0;
      while (1) {
        uint64_t name = read_uleb128(self, &abbrev_buf);
        uint64_t form = read_uleb128(self, &abbrev_buf);
        if (name == 0) {
          break;
        }

        attrs[num_attrs].name = (enum dwarf_attribute)name;
        attrs[num_attrs].form = (enum dwarf_form)form;
        if ((enum dwarf_form)form == DW_FORM_implicit_const) {
          attrs[num_attrs].val = read_sleb128(self, &abbrev_buf);
        } else {
          attrs[num_attrs].val = 0;
        }

        ++num_attrs;
      }
    }

    a.num_attrs = num_attrs;
    a.attrs = attrs;

    abbrevs->abbrevs[num_abbrevs] = a;
    ++num_abbrevs;
  }

  backtrace_sort(abbrevs->abbrevs, abbrevs->num_abbrevs, sizeof(abbrev),
                 abbrev_compare);

  return 1;

fail:
  free_abbrevs(self, abbrevs, on_error, data);
  return 0;
}

// Return the abbrev information for an abbrev code.
const abbrev *lookup_abbrev(ten_backtrace_t *self, abbrevs *abbrevs,
                            uint64_t code,
                            ten_backtrace_on_error_func_t on_error,
                            void *data) {
  abbrev key;
  void *p = NULL;

  // With GCC, where abbrevs are simply numbered in order, we should
  // be able to just look up the entry.
  if (code - 1 < abbrevs->num_abbrevs &&
      abbrevs->abbrevs[code - 1].code == code) {
    return &abbrevs->abbrevs[code - 1];
  }

  // Otherwise we have to search.
  memset(&key, 0, sizeof key);
  key.code = code;
  p = bsearch(&key, abbrevs->abbrevs, abbrevs->num_abbrevs, sizeof(abbrev),
              abbrev_compare);
  if (p == NULL) {
    on_error(self, "Invalid abbreviation code", 0, data);
    return NULL;
  }
  return (const abbrev *)p;
}
