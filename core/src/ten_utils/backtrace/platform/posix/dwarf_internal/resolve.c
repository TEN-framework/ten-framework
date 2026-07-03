//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/backtrace/platform/posix/dwarf.h"
#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/buf.h"

// If we can determine the value of a string attribute, set *STRING to
// point to the string.  Return 1 on success, 0 on error.  If we don't
// know the value, we consider that a success, and we don't change
// *STRING.  An error is only reported for some sort of out of range
// offset.
int resolve_string(ten_backtrace_t *self, const dwarf_sections *dwarf_sections,
                   int is_dwarf64, int is_bigendian, uint64_t str_offsets_base,
                   const attr_val *val, ten_backtrace_on_error_func_t on_error,
                   void *data, const char **string) {
  switch (val->encoding) {
  case ATTR_VAL_STRING:
    *string = val->u.string;
    return 1;

  case ATTR_VAL_STRING_INDEX: {
    dwarf_buf offset_buf;

    uint64_t offset = (val->u.uint * (is_dwarf64 ? 8 : 4)) + str_offsets_base;
    if (offset + (is_dwarf64 ? 8 : 4) >
        dwarf_sections->size[DEBUG_STR_OFFSETS]) {
      on_error(self, "DW_FORM_strx value out of range", 0, data);
      return 0;
    }

    offset_buf.name = ".debug_str_offsets";
    offset_buf.start = dwarf_sections->data[DEBUG_STR_OFFSETS];
    offset_buf.buf = dwarf_sections->data[DEBUG_STR_OFFSETS] + offset;
    offset_buf.left = dwarf_sections->size[DEBUG_STR_OFFSETS] - offset;
    offset_buf.is_bigendian = is_bigendian;
    offset_buf.on_error = on_error;
    offset_buf.data = data;
    offset_buf.reported_underflow = 0;

    offset = read_offset(self, &offset_buf, is_dwarf64);
    if (offset >= dwarf_sections->size[DEBUG_STR]) {
      dwarf_buf_error(self, &offset_buf, "DW_FORM_strx offset out of range", 0);
      return 0;
    }
    *string = (const char *)dwarf_sections->data[DEBUG_STR] + offset;
    return 1;
  }

  default:
    return 1;
  }
}

// Set *ADDRESS to the real address for a ATTR_VAL_ADDRESS_INDEX.
// Return 1 on success, 0 on error.
int resolve_addr_index(ten_backtrace_t *self,
                       const dwarf_sections *dwarf_sections, uint64_t addr_base,
                       int addrsize, int is_bigendian, uint64_t addr_index,
                       ten_backtrace_on_error_func_t on_error, void *data,
                       uintptr_t *address) {
  dwarf_buf addr_buf;

  uint64_t offset = (addr_index * addrsize) + addr_base;
  if (offset + addrsize > dwarf_sections->size[DEBUG_ADDR]) {
    on_error(self, "DW_FORM_addrx value out of range", 0, data);
    return 0;
  }

  addr_buf.name = ".debug_addr";
  addr_buf.start = dwarf_sections->data[DEBUG_ADDR];
  addr_buf.buf = dwarf_sections->data[DEBUG_ADDR] + offset;
  addr_buf.left = dwarf_sections->size[DEBUG_ADDR] - offset;
  addr_buf.is_bigendian = is_bigendian;
  addr_buf.on_error = on_error;
  addr_buf.data = data;
  addr_buf.reported_underflow = 0;

  *address = (uintptr_t)read_address(self, &addr_buf, addrsize);
  return 1;
}
