//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/pcrange.h"

#include "include_internal/ten_utils/backtrace/platform/posix/dwarf.h"
#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/attribute.h"

// Update PCRANGE from an attribute value.
void update_pcrange(const attr *attr, const attr_val *val, pcrange *pcrange) {
  switch (attr->name) {
  case DW_AT_low_pc:
    if (val->encoding == ATTR_VAL_ADDRESS) {
      pcrange->lowpc = (uintptr_t)val->u.uint;
      pcrange->have_lowpc = 1;
    } else if (val->encoding == ATTR_VAL_ADDRESS_INDEX) {
      pcrange->lowpc = (uintptr_t)val->u.uint;
      pcrange->have_lowpc = 1;
      pcrange->lowpc_is_addr_index = 1;
    }
    break;

  case DW_AT_high_pc:
    if (val->encoding == ATTR_VAL_ADDRESS) {
      pcrange->highpc = (uintptr_t)val->u.uint;
      pcrange->have_highpc = 1;
    } else if (val->encoding == ATTR_VAL_UINT) {
      pcrange->highpc = (uintptr_t)val->u.uint;
      pcrange->have_highpc = 1;
      pcrange->highpc_is_relative = 1;
    } else if (val->encoding == ATTR_VAL_ADDRESS_INDEX) {
      pcrange->highpc = (uintptr_t)val->u.uint;
      pcrange->have_highpc = 1;
      pcrange->highpc_is_addr_index = 1;
    }
    break;

  case DW_AT_ranges:
    if (val->encoding == ATTR_VAL_UINT ||
        val->encoding == ATTR_VAL_REF_SECTION) {
      pcrange->ranges = val->u.uint;
      pcrange->have_ranges = 1;
    } else if (val->encoding == ATTR_VAL_RNGLISTS_INDEX) {
      pcrange->ranges = val->u.uint;
      pcrange->have_ranges = 1;
      pcrange->ranges_is_index = 1;
    }
    break;

  default:
    break;
  }
}

// Call ADD_RANGE for a low/high PC pair.  Returns 1 on success, 0 on
// error.
static int add_low_high_range(
    ten_backtrace_t *self, const dwarf_sections *dwarf_sections,
    uintptr_t base_address, int is_bigendian, unit *u, const pcrange *pcrange,
    int (*add_range)(ten_backtrace_t *self, void *rdata, uintptr_t lowpc,
                     uintptr_t highpc, ten_backtrace_on_error_func_t on_error,
                     void *data, void *vec),
    void *rdata, ten_backtrace_on_error_func_t on_error, void *data,
    void *vec) {
  uintptr_t lowpc = pcrange->lowpc;
  if (pcrange->lowpc_is_addr_index) {
    if (!resolve_addr_index(self, dwarf_sections, u->addr_base, u->addrsize,
                            is_bigendian, lowpc, on_error, data, &lowpc)) {
      return 0;
    }
  }

  uintptr_t highpc = pcrange->highpc;
  if (pcrange->highpc_is_addr_index) {
    if (!resolve_addr_index(self, dwarf_sections, u->addr_base, u->addrsize,
                            is_bigendian, highpc, on_error, data, &highpc)) {
      return 0;
    }
  }

  if (pcrange->highpc_is_relative) {
    highpc += lowpc;
  }

  // Add in the base address of the module when recording PC values,
  // so that we can look up the PC directly.
  lowpc += base_address;
  highpc += base_address;

  return add_range(self, rdata, lowpc, highpc, on_error, data, vec);
}

// Return whether a value is the highest possible address, given the
// address size.
static int is_highest_address(uint64_t address, int addrsize) {
  switch (addrsize) {
  case 1:
    return address == (unsigned char)-1;
  case 2:
    return address == (uint16_t)-1;
  case 4:
    return address == (uint32_t)-1;
  case 8:
    return address == (uint64_t)-1;
  default:
    return 0;
  }
}

// Call ADD_RANGE for each range read from .debug_ranges, as used in
// DWARF versions 2 through 4.
static int add_ranges_from_ranges(
    ten_backtrace_t *self, const dwarf_sections *dwarf_sections,
    uintptr_t base_address, int is_bigendian, unit *u, uintptr_t base,
    const pcrange *pcrange,
    int (*add_range)(ten_backtrace_t *self, void *rdata, uintptr_t lowpc,
                     uintptr_t highpc, ten_backtrace_on_error_func_t on_error,
                     void *data, void *vec),
    void *rdata, ten_backtrace_on_error_func_t on_error, void *data,
    void *vec) {
  dwarf_buf ranges_buf;

  if (pcrange->ranges >= dwarf_sections->size[DEBUG_RANGES]) {
    on_error(self, "ranges offset out of range", 0, data);
    return 0;
  }

  ranges_buf.name = ".debug_ranges";
  ranges_buf.start = dwarf_sections->data[DEBUG_RANGES];
  ranges_buf.buf = dwarf_sections->data[DEBUG_RANGES] + pcrange->ranges;
  ranges_buf.left = dwarf_sections->size[DEBUG_RANGES] - pcrange->ranges;
  ranges_buf.is_bigendian = is_bigendian;
  ranges_buf.on_error = on_error;
  ranges_buf.data = data;
  ranges_buf.reported_underflow = 0;

  while (1) {
    if (ranges_buf.reported_underflow) {
      return 0;
    }

    uint64_t low = read_address(self, &ranges_buf, u->addrsize);
    uint64_t high = read_address(self, &ranges_buf, u->addrsize);

    if (low == 0 && high == 0) {
      break;
    }

    if (is_highest_address(low, u->addrsize)) {
      base = (uintptr_t)high;
    } else {
      if (!add_range(self, rdata, (uintptr_t)low + base + base_address,
                     (uintptr_t)high + base + base_address, on_error, data,
                     vec)) {
        return 0;
      }
    }
  }

  if (ranges_buf.reported_underflow) {
    return 0;
  }
  return 1;
}

// Call ADD_RANGE for each range read from .debug_rnglists, as used in
// DWARF version 5.
static int add_ranges_from_rnglists(
    ten_backtrace_t *self, const dwarf_sections *dwarf_sections,
    uintptr_t base_address, int is_bigendian, unit *u, uintptr_t base,
    const pcrange *pcrange,
    int (*add_range)(ten_backtrace_t *self, void *rdata, uintptr_t lowpc,
                     uintptr_t highpc, ten_backtrace_on_error_func_t on_error,
                     void *data, void *vec),
    void *rdata, ten_backtrace_on_error_func_t on_error, void *data,
    void *vec) {
  uint64_t offset = 0;
  dwarf_buf rnglists_buf;

  if (!pcrange->ranges_is_index) {
    offset = pcrange->ranges;
  } else {
    offset = u->rnglists_base + pcrange->ranges * (u->is_dwarf64 ? 8 : 4);
  }

  if (offset >= dwarf_sections->size[DEBUG_RNGLISTS]) {
    on_error(self, "rnglists offset out of range", 0, data);
    return 0;
  }

  rnglists_buf.name = ".debug_rnglists";
  rnglists_buf.start = dwarf_sections->data[DEBUG_RNGLISTS];
  rnglists_buf.buf = dwarf_sections->data[DEBUG_RNGLISTS] + offset;
  rnglists_buf.left = dwarf_sections->size[DEBUG_RNGLISTS] - offset;
  rnglists_buf.is_bigendian = is_bigendian;
  rnglists_buf.on_error = on_error;
  rnglists_buf.data = data;
  rnglists_buf.reported_underflow = 0;

  if (pcrange->ranges_is_index) {
    offset = read_offset(self, &rnglists_buf, u->is_dwarf64);
    offset += u->rnglists_base;
    if (offset >= dwarf_sections->size[DEBUG_RNGLISTS]) {
      on_error(self, "rnglists index offset out of range", 0, data);
      return 0;
    }
    rnglists_buf.buf = dwarf_sections->data[DEBUG_RNGLISTS] + offset;
    rnglists_buf.left = dwarf_sections->size[DEBUG_RNGLISTS] - offset;
  }

  while (1) {
    unsigned char rle = read_byte(self, &rnglists_buf);
    if (rle == DW_RLE_end_of_list) {
      break;
    }

    switch (rle) {
    case DW_RLE_base_addressx: {
      uint64_t index = read_uleb128(self, &rnglists_buf);
      if (!resolve_addr_index(self, dwarf_sections, u->addr_base, u->addrsize,
                              is_bigendian, index, on_error, data, &base)) {
        return 0;
      }
    } break;

    case DW_RLE_startx_endx: {
      uintptr_t low = 0;
      uintptr_t high = 0;

      uint64_t index = read_uleb128(self, &rnglists_buf);
      if (!resolve_addr_index(self, dwarf_sections, u->addr_base, u->addrsize,
                              is_bigendian, index, on_error, data, &low)) {
        return 0;
      }

      index = read_uleb128(self, &rnglists_buf);
      if (!resolve_addr_index(self, dwarf_sections, u->addr_base, u->addrsize,
                              is_bigendian, index, on_error, data, &high)) {
        return 0;
      }

      if (!add_range(self, rdata, low + base_address, high + base_address,
                     on_error, data, vec)) {
        return 0;
      }
    } break;

    case DW_RLE_startx_length: {
      uintptr_t low = 0;
      uintptr_t length = 0;

      uint64_t index = read_uleb128(self, &rnglists_buf);
      if (!resolve_addr_index(self, dwarf_sections, u->addr_base, u->addrsize,
                              is_bigendian, index, on_error, data, &low)) {
        return 0;
      }
      length = read_uleb128(self, &rnglists_buf);
      low += base_address;
      if (!add_range(self, rdata, low, low + length, on_error, data, vec)) {
        return 0;
      }
    } break;

    case DW_RLE_offset_pair: {
      uint64_t low = read_uleb128(self, &rnglists_buf);
      uint64_t high = read_uleb128(self, &rnglists_buf);
      if (!add_range(self, rdata, low + base + base_address,
                     high + base + base_address, on_error, data, vec)) {
        return 0;
      }
    } break;

    case DW_RLE_base_address:
      base = (uintptr_t)read_address(self, &rnglists_buf, u->addrsize);
      break;

    case DW_RLE_start_end: {
      uintptr_t low = (uintptr_t)read_address(self, &rnglists_buf, u->addrsize);
      uintptr_t high =
          (uintptr_t)read_address(self, &rnglists_buf, u->addrsize);
      if (!add_range(self, rdata, low + base_address, high + base_address,
                     on_error, data, vec)) {
        return 0;
      }
    } break;

    case DW_RLE_start_length: {
      uintptr_t low = (uintptr_t)read_address(self, &rnglists_buf, u->addrsize);
      uintptr_t length = (uintptr_t)read_uleb128(self, &rnglists_buf);
      low += base_address;
      if (!add_range(self, rdata, low, low + length, on_error, data, vec)) {
        return 0;
      }
    } break;

    default:
      dwarf_buf_error(self, &rnglists_buf, "unrecognized DW_RLE value", -1);
      return 0;
    }
  }

  if (rnglists_buf.reported_underflow) {
    return 0;
  }

  return 1;
}

// Call ADD_RANGE for each lowpc/highpc pair in PCRANGE.  RDATA is
// passed to ADD_RANGE, and is either a unit * or a struct
// function *.  VEC is the vector we are adding ranges to, and is
// either a struct unit_addrs_vector * or a function_vector *.
// Returns 1 on success, 0 on error.
int add_ranges(ten_backtrace_t *self, const dwarf_sections *dwarf_sections,
               uintptr_t base_address, int is_bigendian, unit *u,
               uintptr_t base, const pcrange *pcrange,
               int (*add_range)(ten_backtrace_t *self, void *rdata,
                                uintptr_t lowpc, uintptr_t highpc,
                                ten_backtrace_on_error_func_t on_error,
                                void *data, void *vec),
               void *rdata, ten_backtrace_on_error_func_t on_error, void *data,
               void *vec) {
  if (pcrange->have_lowpc && pcrange->have_highpc) {
    return add_low_high_range(self, dwarf_sections, base_address, is_bigendian,
                              u, pcrange, add_range, rdata, on_error, data,
                              vec);
  }

  if (!pcrange->have_ranges) {
    // Did not find any address ranges to add.
    return 1;
  }

  if (u->version < 5) {
    return add_ranges_from_ranges(self, dwarf_sections, base_address,
                                  is_bigendian, u, base, pcrange, add_range,
                                  rdata, on_error, data, vec);
  } else {
    return add_ranges_from_rnglists(self, dwarf_sections, base_address,
                                    is_bigendian, u, base, pcrange, add_range,
                                    rdata, on_error, data, vec);
  }
}
