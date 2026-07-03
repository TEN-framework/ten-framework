//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/backtrace/platform/posix/dwarf.h"
#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/buf.h"

// Read the name of a function from a DIE referenced by a DW_AT_abstract_origin
// or DW_AT_specification tag. OFFSET is within the same compilation unit.
static const char *read_referenced_name(ten_backtrace_t *self,
                                        dwarf_data *ddata, unit *u,
                                        uint64_t offset,
                                        ten_backtrace_on_error_func_t on_error,
                                        void *data) {
  dwarf_buf unit_buf;
  uint64_t code = 0;
  const abbrev *abbrev = NULL;
  const char *ret = NULL;
  size_t i = 0;

  // OFFSET is from the start of the data for this compilation unit.
  // U->unit_data is the data, but it starts U->unit_data_offset bytes
  // from the beginning.

  if (offset < u->unit_data_offset ||
      offset - u->unit_data_offset >= u->unit_data_len) {
    on_error(self, "abstract origin or specification out of range", 0, data);
    return NULL;
  }

  offset -= u->unit_data_offset;

  unit_buf.name = ".debug_info";
  unit_buf.start = ddata->dwarf_sections.data[DEBUG_INFO];
  unit_buf.buf = u->unit_data + offset;
  unit_buf.left = u->unit_data_len - offset;
  unit_buf.is_bigendian = ddata->is_bigendian;
  unit_buf.on_error = on_error;
  unit_buf.data = data;
  unit_buf.reported_underflow = 0;

  code = read_uleb128(self, &unit_buf);
  if (code == 0) {
    dwarf_buf_error(self, &unit_buf, "Invalid abstract origin or specification",
                    0);
    return NULL;
  }

  abbrev = lookup_abbrev(self, &u->abbrevs, code, on_error, data);
  if (abbrev == NULL) {
    return NULL;
  }

  ret = NULL;
  for (i = 0; i < abbrev->num_attrs; ++i) {
    attr_val val;

    if (!read_attribute(self, abbrev->attrs[i].form, abbrev->attrs[i].val,
                        &unit_buf, u->is_dwarf64, u->version, u->addrsize,
                        &ddata->dwarf_sections, ddata->altlink, &val)) {
      return NULL;
    }

    switch (abbrev->attrs[i].name) {
    case DW_AT_name:
      // Third name preference: don't override.  A name we found in some
      // other way, will normally be more useful -- e.g., this name is
      // normally not mangled.
      if (ret != NULL) {
        break;
      }

      if (!resolve_string(self, &ddata->dwarf_sections, u->is_dwarf64,
                          ddata->is_bigendian, u->str_offsets_base, &val,
                          on_error, data, &ret)) {
        return NULL;
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
          return NULL;
        }

        if (s != NULL) {
          return s;
        }
      }
      break;

    case DW_AT_specification:
      // Second name preference: override DW_AT_name, don't override
      // DW_AT_linkage_name.
      {
        const char *name = read_referenced_name_from_attr(
            self, ddata, u, &abbrev->attrs[i], &val, on_error, data);
        if (name != NULL) {
          ret = name;
        }
      }
      break;

    default:
      break;
    }
  }

  return ret;
}

// Read the name of a function from a DIE referenced by ATTR with VAL.
const char *read_referenced_name_from_attr(
    ten_backtrace_t *self, dwarf_data *ddata, unit *u, attr *attr,
    attr_val *val, ten_backtrace_on_error_func_t on_error, void *data) {
  switch (attr->name) {
  case DW_AT_abstract_origin:
  case DW_AT_specification:
    break;
  default:
    return NULL;
  }

  if (attr->form == DW_FORM_ref_sig8) {
    return NULL;
  }

  if (val->encoding == ATTR_VAL_REF_INFO) {
    unit *unit = find_unit(ddata->units, ddata->units_count, val->u.uint);
    if (unit == NULL) {
      return NULL;
    }

    uint64_t offset = val->u.uint - unit->low_offset;
    return read_referenced_name(self, ddata, unit, offset, on_error, data);
  }

  if (val->encoding == ATTR_VAL_UINT || val->encoding == ATTR_VAL_REF_UNIT) {
    return read_referenced_name(self, ddata, u, val->u.uint, on_error, data);
  }

  if (val->encoding == ATTR_VAL_REF_ALT_INFO) {
    unit *alt_unit = find_unit(ddata->altlink->units,
                               ddata->altlink->units_count, val->u.uint);
    if (alt_unit == NULL) {
      return NULL;
    }

    uint64_t offset = val->u.uint - alt_unit->low_offset;
    return read_referenced_name(self, ddata->altlink, alt_unit, offset,
                                on_error, data);
  }

  return NULL;
}
