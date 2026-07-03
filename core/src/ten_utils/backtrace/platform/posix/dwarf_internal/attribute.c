//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/attribute.h"

#include <assert.h>
#include <string.h>

#include "include_internal/ten_utils/backtrace/backtrace.h"
#include "include_internal/ten_utils/backtrace/platform/posix/dwarf.h"

// Read an attribute value.  Returns 1 on success, 0 on failure.  If
// the value can be represented as a uint64_t, sets *VAL and sets
// *IS_VALID to 1.  We don't try to store the value of other attribute
// forms, because we don't care about them.
int read_attribute(ten_backtrace_t *self, enum dwarf_form form,
                   uint64_t implicit_val, dwarf_buf *buf, int is_dwarf64,
                   int version, int addrsize,
                   const dwarf_sections *dwarf_sections, dwarf_data *altlink,
                   attr_val *val) {
  // Avoid warnings about val.u.FIELD may be used uninitialized if
  // this function is inlined.  The warnings aren't valid but can
  // occur because the different fields are set and used
  // conditionally.
  memset(val, 0, sizeof *val);

  switch (form) {
  case DW_FORM_addr:
    val->encoding = ATTR_VAL_ADDRESS;
    val->u.uint = read_address(self, buf, addrsize);
    return 1;
  case DW_FORM_block2:
    val->encoding = ATTR_VAL_BLOCK;
    return advance(self, buf, read_uint16(self, buf));
  case DW_FORM_block4:
    val->encoding = ATTR_VAL_BLOCK;
    return advance(self, buf, read_uint32(self, buf));
  case DW_FORM_data2:
    val->encoding = ATTR_VAL_UINT;
    val->u.uint = read_uint16(self, buf);
    return 1;
  case DW_FORM_data4:
    val->encoding = ATTR_VAL_UINT;
    val->u.uint = read_uint32(self, buf);
    return 1;
  case DW_FORM_data8:
    val->encoding = ATTR_VAL_UINT;
    val->u.uint = read_uint64(self, buf);
    return 1;
  case DW_FORM_data16:
    val->encoding = ATTR_VAL_BLOCK;
    return advance(self, buf, 16);
  case DW_FORM_string:
    val->encoding = ATTR_VAL_STRING;
    val->u.string = read_string(self, buf);
    return val->u.string == NULL ? 0 : 1;
  case DW_FORM_block:
    val->encoding = ATTR_VAL_BLOCK;
    return advance(self, buf, read_uleb128(self, buf));
  case DW_FORM_block1:
    val->encoding = ATTR_VAL_BLOCK;
    return advance(self, buf, read_byte(self, buf));
  case DW_FORM_data1:
  case DW_FORM_flag:
    val->encoding = ATTR_VAL_UINT;
    val->u.uint = read_byte(self, buf);
    return 1;
  case DW_FORM_sdata:
    val->encoding = ATTR_VAL_SINT;
    val->u.sint = read_sleb128(self, buf);
    return 1;
  case DW_FORM_strp: {
    uint64_t offset = 0;

    offset = read_offset(self, buf, is_dwarf64);
    if (offset >= dwarf_sections->size[DEBUG_STR]) {
      dwarf_buf_error(self, buf, "DW_FORM_strp out of range", 0);
      return 0;
    }
    val->encoding = ATTR_VAL_STRING;
    val->u.string = (const char *)dwarf_sections->data[DEBUG_STR] + offset;
    return 1;
  }
  case DW_FORM_line_strp: {
    uint64_t offset = 0;

    offset = read_offset(self, buf, is_dwarf64);
    if (offset >= dwarf_sections->size[DEBUG_LINE_STR]) {
      dwarf_buf_error(self, buf, "DW_FORM_line_strp out of range", 0);
      return 0;
    }
    val->encoding = ATTR_VAL_STRING;
    val->u.string = (const char *)dwarf_sections->data[DEBUG_LINE_STR] + offset;
    return 1;
  }
  case DW_FORM_udata:
    val->encoding = ATTR_VAL_UINT;
    val->u.uint = read_uleb128(self, buf);
    return 1;
  case DW_FORM_ref_addr:
    val->encoding = ATTR_VAL_REF_INFO;
    if (version == 2) {
      val->u.uint = read_address(self, buf, addrsize);
    } else {
      val->u.uint = read_offset(self, buf, is_dwarf64);
    }
    return 1;
  case DW_FORM_ref1:
    val->encoding = ATTR_VAL_REF_UNIT;
    val->u.uint = read_byte(self, buf);
    return 1;
  case DW_FORM_ref2:
    val->encoding = ATTR_VAL_REF_UNIT;
    val->u.uint = read_uint16(self, buf);
    return 1;
  case DW_FORM_ref4:
    val->encoding = ATTR_VAL_REF_UNIT;
    val->u.uint = read_uint32(self, buf);
    return 1;
  case DW_FORM_ref8:
    val->encoding = ATTR_VAL_REF_UNIT;
    val->u.uint = read_uint64(self, buf);
    return 1;
  case DW_FORM_ref_udata:
    val->encoding = ATTR_VAL_REF_UNIT;
    val->u.uint = read_uleb128(self, buf);
    return 1;
  case DW_FORM_indirect: {
    uint64_t form = 0;

    form = read_uleb128(self, buf);
    if (form == DW_FORM_implicit_const) {
      dwarf_buf_error(self, buf, "DW_FORM_indirect to DW_FORM_implicit_const",
                      0);
      return 0;
    }
    return read_attribute(self, (enum dwarf_form)form, 0, buf, is_dwarf64,
                          version, addrsize, dwarf_sections, altlink, val);
  }
  case DW_FORM_sec_offset:
    val->encoding = ATTR_VAL_REF_SECTION;
    val->u.uint = read_offset(self, buf, is_dwarf64);
    return 1;
  case DW_FORM_exprloc:
    val->encoding = ATTR_VAL_EXPR;
    return advance(self, buf, read_uleb128(self, buf));
  case DW_FORM_flag_present:
    val->encoding = ATTR_VAL_UINT;
    val->u.uint = 1;
    return 1;
  case DW_FORM_ref_sig8:
    val->encoding = ATTR_VAL_REF_TYPE;
    val->u.uint = read_uint64(self, buf);
    return 1;
  case DW_FORM_strx:
  case DW_FORM_strx1:
  case DW_FORM_strx2:
  case DW_FORM_strx3:
  case DW_FORM_strx4: {
    uint64_t offset = 0;

    switch (form) {
    case DW_FORM_strx:
      offset = read_uleb128(self, buf);
      break;
    case DW_FORM_strx1:
      offset = read_byte(self, buf);
      break;
    case DW_FORM_strx2:
      offset = read_uint16(self, buf);
      break;
    case DW_FORM_strx3:
      offset = read_uint24(self, buf);
      break;
    case DW_FORM_strx4:
      offset = read_uint32(self, buf);
      break;
    default:
      // This case can't happen.
      assert(0 && "unreachable");
      return 0;
    }
    val->encoding = ATTR_VAL_STRING_INDEX;
    val->u.uint = offset;
    return 1;
  }
  case DW_FORM_addrx:
  case DW_FORM_addrx1:
  case DW_FORM_addrx2:
  case DW_FORM_addrx3:
  case DW_FORM_addrx4: {
    uint64_t offset = 0;

    switch (form) {
    case DW_FORM_addrx:
      offset = read_uleb128(self, buf);
      break;
    case DW_FORM_addrx1:
      offset = read_byte(self, buf);
      break;
    case DW_FORM_addrx2:
      offset = read_uint16(self, buf);
      break;
    case DW_FORM_addrx3:
      offset = read_uint24(self, buf);
      break;
    case DW_FORM_addrx4:
      offset = read_uint32(self, buf);
      break;
    default:
      // This case can't happen.
      assert(0 && "unreachable");
      return 0;
    }
    val->encoding = ATTR_VAL_ADDRESS_INDEX;
    val->u.uint = offset;
    return 1;
  }
  case DW_FORM_ref_sup4:
    val->encoding = ATTR_VAL_REF_SECTION;
    val->u.uint = read_uint32(self, buf);
    return 1;
  case DW_FORM_ref_sup8:
    val->encoding = ATTR_VAL_REF_SECTION;
    val->u.uint = read_uint64(self, buf);
    return 1;
  case DW_FORM_implicit_const:
    val->encoding = ATTR_VAL_UINT;
    val->u.uint = implicit_val;
    return 1;
  case DW_FORM_loclistx:
    // We don't distinguish this from DW_FORM_sec_offset.  It
    // shouldn't matter since we don't care about loclists.
    val->encoding = ATTR_VAL_REF_SECTION;
    val->u.uint = read_uleb128(self, buf);
    return 1;
  case DW_FORM_rnglistx:
    val->encoding = ATTR_VAL_RNGLISTS_INDEX;
    val->u.uint = read_uleb128(self, buf);
    return 1;
  case DW_FORM_GNU_addr_index:
  case DW_FORM_GNU_str_index:
    val->encoding = ATTR_VAL_REF_SECTION;
    val->u.uint = read_uleb128(self, buf);
    return 1;
  case DW_FORM_GNU_ref_alt:
    val->u.uint = read_offset(self, buf, is_dwarf64);
    if (altlink == NULL) {
      val->encoding = ATTR_VAL_NONE;
      return 1;
    }
    val->encoding = ATTR_VAL_REF_ALT_INFO;
    return 1;
  case DW_FORM_strp_sup:
  case DW_FORM_GNU_strp_alt: {
    uint64_t offset = 0;

    offset = read_offset(self, buf, is_dwarf64);
    if (altlink == NULL) {
      val->encoding = ATTR_VAL_NONE;
      return 1;
    }
    if (offset >= altlink->dwarf_sections.size[DEBUG_STR]) {
      dwarf_buf_error(self, buf, "DW_FORM_strp_sup out of range", 0);
      return 0;
    }
    val->encoding = ATTR_VAL_STRING;
    val->u.string =
        (const char *)altlink->dwarf_sections.data[DEBUG_STR] + offset;
    return 1;
  }
  default:
    dwarf_buf_error(self, buf, "unrecognized DWARF form", -1);
    return 0;
  }
}
