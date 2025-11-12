//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/buf.h"
#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/data.h"
#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/form.h"
#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/section.h"

typedef struct ten_backtrace_t ten_backtrace_t;

/**
 * @brief DWARF attribute tags as defined in the DWARF debugging format
 * standard.
 *
 * These attributes describe various properties of debugging information entries
 * (DIEs) in the DWARF format. They provide information about variables, types,
 * functions, and other program elements to support debugging and stack
 * unwinding.
 *
 * The values are from the DWARF standard versions 2 through 5, plus
 * vendor-specific extensions (prefixed with vendor names like DW_AT_GNU_*,
 * DW_AT_MIPS_*, etc.).
 */
typedef enum dwarf_attribute {
  DW_AT_sibling = 0x01,           // Reference to sibling
  DW_AT_location = 0x02,          // Location description
  DW_AT_name = 0x03,              // Name of the object
  DW_AT_ordering = 0x09,          // Array ordering
  DW_AT_subscr_data = 0x0a,       // Subscript data
  DW_AT_byte_size = 0x0b,         // Size in bytes
  DW_AT_bit_offset = 0x0c,        // Bit offset
  DW_AT_bit_size = 0x0d,          // Size in bits
  DW_AT_element_list = 0x0f,      // Element list
  DW_AT_stmt_list = 0x10,         // Line number information
  DW_AT_low_pc = 0x11,            // Lowest program counter value
  DW_AT_high_pc = 0x12,           // Highest program counter value
  DW_AT_language = 0x13,          // Source language
  DW_AT_member = 0x14,            // Member of type
  DW_AT_discr = 0x15,             // Discriminant of variant
  DW_AT_discr_value = 0x16,       // Discriminant value
  DW_AT_visibility = 0x17,        // Visibility attribute
  DW_AT_import = 0x18,            // Imported declaration
  DW_AT_string_length = 0x19,     // String length
  DW_AT_common_reference = 0x1a,  // Common block reference
  DW_AT_comp_dir = 0x1b,          // Compilation directory
  DW_AT_const_value = 0x1c,       // Constant value
  DW_AT_containing_type = 0x1d,   // Containing type
  DW_AT_default_value = 0x1e,     // Default value
  DW_AT_inline = 0x20,            // Inline attribute
  DW_AT_is_optional = 0x21,       // Optional parameter
  DW_AT_lower_bound = 0x22,       // Lower bound of subrange
  DW_AT_producer = 0x25,          // Producer of the compilation
  DW_AT_prototyped = 0x27,        // Function is prototyped
  DW_AT_return_addr = 0x2a,       // Return address location
  DW_AT_start_scope = 0x2c,       // Start scope
  DW_AT_bit_stride = 0x2e,        // Stride size in bits (was DW_AT_stride_size)
  DW_AT_upper_bound = 0x2f,       // Upper bound of subrange
  DW_AT_abstract_origin = 0x31,   // Abstract origin
  DW_AT_accessibility = 0x32,     // Accessibility (public, private, etc.)
  DW_AT_address_class = 0x33,     // Address class
  DW_AT_artificial = 0x34,        // Artificial (compiler generated)
  DW_AT_base_types = 0x35,        // Base types for type
  DW_AT_calling_convention = 0x36,       // Calling convention
  DW_AT_count = 0x37,                    // Count of members
  DW_AT_data_member_location = 0x38,     // Data member location
  DW_AT_decl_column = 0x39,              // Declaration column
  DW_AT_decl_file = 0x3a,                // Declaration file
  DW_AT_decl_line = 0x3b,                // Declaration line
  DW_AT_declaration = 0x3c,              // Declaration, not definition
  DW_AT_discr_list = 0x3d,               // Discriminant list
  DW_AT_encoding = 0x3e,                 // Encoding attribute
  DW_AT_external = 0x3f,                 // External symbol
  DW_AT_frame_base = 0x40,               // Frame base location
  DW_AT_friend = 0x41,                   // Friend relationship
  DW_AT_identifier_case = 0x42,          // Identifier case sensitivity
  DW_AT_macro_info = 0x43,               // Macro information
  DW_AT_namelist_items = 0x44,           // Namelist items
  DW_AT_priority = 0x45,                 // Priority
  DW_AT_segment = 0x46,                  // Segment
  DW_AT_specification = 0x47,            // Specification
  DW_AT_static_link = 0x48,              // Static link
  DW_AT_type = 0x49,                     // Type
  DW_AT_use_location = 0x4a,             // Use location
  DW_AT_variable_parameter = 0x4b,       // Variable parameter
  DW_AT_virtuality = 0x4c,               // Virtuality
  DW_AT_vtable_elem_location = 0x4d,     // Location of vtable element
  DW_AT_allocated = 0x4e,                // Allocated
  DW_AT_associated = 0x4f,               // Associated
  DW_AT_data_location = 0x50,            // Data location
  DW_AT_byte_stride = 0x51,              // Stride size in bytes
  DW_AT_entry_pc = 0x52,                 // Entry point
  DW_AT_use_UTF8 = 0x53,                 // Use UTF-8 encoding
  DW_AT_extension = 0x54,                // Extension
  DW_AT_ranges = 0x55,                   // Ranges
  DW_AT_trampoline = 0x56,               // Trampoline
  DW_AT_call_column = 0x57,              // Call column
  DW_AT_call_file = 0x58,                // Call file
  DW_AT_call_line = 0x59,                // Call line
  DW_AT_description = 0x5a,              // Description
  DW_AT_binary_scale = 0x5b,             // Binary scale
  DW_AT_decimal_scale = 0x5c,            // Decimal scale
  DW_AT_small = 0x5d,                    // Small
  DW_AT_decimal_sign = 0x5e,             // Decimal sign
  DW_AT_digit_count = 0x5f,              // Digit count
  DW_AT_picture_string = 0x60,           // Picture string
  DW_AT_mutable = 0x61,                  // Mutable
  DW_AT_threads_scaled = 0x62,           // Threads scaled
  DW_AT_explicit = 0x63,                 // Explicit
  DW_AT_object_pointer = 0x64,           // Object pointer
  DW_AT_endianity = 0x65,                // Endianity
  DW_AT_elemental = 0x66,                // Elemental
  DW_AT_pure = 0x67,                     // Pure
  DW_AT_recursive = 0x68,                // Recursive
  DW_AT_signature = 0x69,                // Type signature
  DW_AT_main_subprogram = 0x6a,          // Main subprogram
  DW_AT_data_bit_offset = 0x6b,          // Data bit offset
  DW_AT_const_expr = 0x6c,               // Constant expression
  DW_AT_enum_class = 0x6d,               // Enum class
  DW_AT_linkage_name = 0x6e,             // Linkage name (mangled name)
  DW_AT_string_length_bit_size = 0x6f,   // String length bit size
  DW_AT_string_length_byte_size = 0x70,  // String length byte size
  DW_AT_rank = 0x71,                     // Rank
  DW_AT_str_offsets_base = 0x72,         // String offsets base
  DW_AT_addr_base = 0x73,                // Address base
  DW_AT_rnglists_base = 0x74,            // Range lists base
  DW_AT_dwo_name = 0x76,                 // DWO name
  DW_AT_reference = 0x77,                // Reference
  DW_AT_rvalue_reference = 0x78,         // R-value reference
  DW_AT_macros = 0x79,                   // Macros
  DW_AT_call_all_calls = 0x7a,           // Call all calls
  DW_AT_call_all_source_calls = 0x7b,    // Call all source calls
  DW_AT_call_all_tail_calls = 0x7c,      // Call all tail calls
  DW_AT_call_return_pc = 0x7d,           // Call return PC
  DW_AT_call_value = 0x7e,               // Call value
  DW_AT_call_origin = 0x7f,              // Call origin
  DW_AT_call_parameter = 0x80,           // Call parameter
  DW_AT_call_pc = 0x81,                  // Call PC
  DW_AT_call_tail_call = 0x82,           // Call tail call
  DW_AT_call_target = 0x83,              // Call target
  DW_AT_call_target_clobbered = 0x84,    // Call target clobbered
  DW_AT_call_data_location = 0x85,       // Call data location
  DW_AT_call_data_value = 0x86,          // Call data value
  DW_AT_noreturn = 0x87,                 // No return
  DW_AT_alignment = 0x88,                // Alignment
  DW_AT_export_symbols = 0x89,           // Export symbols
  DW_AT_deleted = 0x8a,                  // Deleted
  DW_AT_defaulted = 0x8b,                // Defaulted
  DW_AT_loclists_base = 0x8c,            // Location lists base

  // User-defined attributes
  DW_AT_lo_user = 0x2000,  // Beginning of user-defined attributes
  DW_AT_hi_user = 0x3fff,  // End of user-defined attributes

  // MIPS extensions
  DW_AT_MIPS_fde = 0x2001,                      // MIPS FDE
  DW_AT_MIPS_loop_begin = 0x2002,               // MIPS loop begin
  DW_AT_MIPS_tail_loop_begin = 0x2003,          // MIPS tail loop begin
  DW_AT_MIPS_epilog_begin = 0x2004,             // MIPS epilog begin
  DW_AT_MIPS_loop_unroll_factor = 0x2005,       // MIPS loop unroll factor
  DW_AT_MIPS_software_pipeline_depth = 0x2006,  // MIPS software pipeline depth
  DW_AT_MIPS_linkage_name = 0x2007,             // MIPS linkage name
  DW_AT_MIPS_stride = 0x2008,                   // MIPS stride
  DW_AT_MIPS_abstract_name = 0x2009,            // MIPS abstract name
  DW_AT_MIPS_clone_origin = 0x200a,             // MIPS clone origin
  DW_AT_MIPS_has_inlines = 0x200b,              // MIPS has inlines

  // HP extensions
  DW_AT_HP_block_index = 0x2000,               // HP block index
  DW_AT_HP_unmodifiable = 0x2001,              // HP unmodifiable
  DW_AT_HP_prologue = 0x2005,                  // HP prologue
  DW_AT_HP_epilogue = 0x2008,                  // HP epilogue
  DW_AT_HP_actuals_stmt_list = 0x2010,         // HP actuals statement list
  DW_AT_HP_proc_per_section = 0x2011,          // HP proc per section
  DW_AT_HP_raw_data_ptr = 0x2012,              // HP raw data pointer
  DW_AT_HP_pass_by_reference = 0x2013,         // HP pass by reference
  DW_AT_HP_opt_level = 0x2014,                 // HP optimization level
  DW_AT_HP_prof_version_id = 0x2015,           // HP profile version ID
  DW_AT_HP_opt_flags = 0x2016,                 // HP optimization flags
  DW_AT_HP_cold_region_low_pc = 0x2017,        // HP cold region low PC
  DW_AT_HP_cold_region_high_pc = 0x2018,       // HP cold region high PC
  DW_AT_HP_all_variables_modifiable = 0x2019,  // HP all variables modifiable
  DW_AT_HP_linkage_name = 0x201a,              // HP linkage name
  DW_AT_HP_prof_flags = 0x201b,                // HP profile flags
  DW_AT_HP_unit_name = 0x201f,                 // HP unit name
  DW_AT_HP_unit_size = 0x2020,                 // HP unit size
  DW_AT_HP_widened_byte_size = 0x2021,         // HP widened byte size
  DW_AT_HP_definition_points = 0x2022,         // HP definition points
  DW_AT_HP_default_location = 0x2023,          // HP default location
  DW_AT_HP_is_result_param = 0x2029,           // HP is result parameter

  // GNU extensions
  DW_AT_sf_names = 0x2101,                      // Names of source files
  DW_AT_src_info = 0x2102,                      // Source information
  DW_AT_mac_info = 0x2103,                      // Macro information
  DW_AT_src_coords = 0x2104,                    // Source coordinates
  DW_AT_body_begin = 0x2105,                    // Beginning of body
  DW_AT_body_end = 0x2106,                      // End of body
  DW_AT_GNU_vector = 0x2107,                    // GNU vector
  DW_AT_GNU_guarded_by = 0x2108,                // GNU guarded by
  DW_AT_GNU_pt_guarded_by = 0x2109,             // GNU pt guarded by
  DW_AT_GNU_guarded = 0x210a,                   // GNU guarded
  DW_AT_GNU_pt_guarded = 0x210b,                // GNU pt guarded
  DW_AT_GNU_locks_excluded = 0x210c,            // GNU locks excluded
  DW_AT_GNU_exclusive_locks_required = 0x210d,  // GNU exclusive locks required
  DW_AT_GNU_shared_locks_required = 0x210e,     // GNU shared locks required
  DW_AT_GNU_odr_signature = 0x210f,             // GNU ODR signature
  DW_AT_GNU_template_name = 0x2110,             // GNU template name
  DW_AT_GNU_call_site_value = 0x2111,           // GNU call site value
  DW_AT_GNU_call_site_data_value = 0x2112,      // GNU call site data value
  DW_AT_GNU_call_site_target = 0x2113,          // GNU call site target
  DW_AT_GNU_call_site_target_clobbered =
      0x2114,                                // GNU call site target clobbered
  DW_AT_GNU_tail_call = 0x2115,              // GNU tail call
  DW_AT_GNU_all_tail_call_sites = 0x2116,    // GNU all tail call sites
  DW_AT_GNU_all_call_sites = 0x2117,         // GNU all call sites
  DW_AT_GNU_all_source_call_sites = 0x2118,  // GNU all source call sites
  DW_AT_GNU_macros = 0x2119,                 // GNU macros
  DW_AT_GNU_deleted = 0x211a,                // GNU deleted
  DW_AT_GNU_dwo_name = 0x2130,               // GNU DWO name
  DW_AT_GNU_dwo_id = 0x2131,                 // GNU DWO ID
  DW_AT_GNU_ranges_base = 0x2132,            // GNU ranges base
  DW_AT_GNU_addr_base = 0x2133,              // GNU address base
  DW_AT_GNU_pubnames = 0x2134,               // GNU public names
  DW_AT_GNU_pubtypes = 0x2135,               // GNU public types
  DW_AT_GNU_discriminator = 0x2136,          // GNU discriminator
  DW_AT_GNU_locviews = 0x2137,               // GNU location views
  DW_AT_GNU_entry_view = 0x2138,             // GNU entry view

  // VMS extensions
  DW_AT_VMS_rtnbeg_pd_address = 0x2201,  // VMS routine begin address

  // GNAT extensions
  DW_AT_use_GNAT_descriptive_type = 0x2301,  // Use GNAT descriptive type
  DW_AT_GNAT_descriptive_type = 0x2302,      // GNAT descriptive type
  DW_AT_GNU_numerator = 0x2303,              // GNU numerator
  DW_AT_GNU_denominator = 0x2304,            // GNU denominator
  DW_AT_GNU_bias = 0x2305,                   // GNU bias

  // UPC extensions
  DW_AT_upc_threads_scaled = 0x3210,  // UPC threads scaled

  // PGI extensions
  DW_AT_PGI_lbase = 0x3a00,    // PGI lbase
  DW_AT_PGI_soffset = 0x3a01,  // PGI soffset
  DW_AT_PGI_lstride = 0x3a02,  // PGI lstride

  // Apple extensions
  DW_AT_APPLE_optimized = 0x3fe1,           // Apple optimized
  DW_AT_APPLE_flags = 0x3fe2,               // Apple flags
  DW_AT_APPLE_isa = 0x3fe3,                 // Apple ISA
  DW_AT_APPLE_block = 0x3fe4,               // Apple block
  DW_AT_APPLE_major_runtime_vers = 0x3fe5,  // Apple major runtime version
  DW_AT_APPLE_runtime_class = 0x3fe6,       // Apple runtime class
  DW_AT_APPLE_omit_frame_ptr = 0x3fe7,      // Apple omit frame pointer
  DW_AT_APPLE_property_name = 0x3fe8,       // Apple property name
  DW_AT_APPLE_property_getter = 0x3fe9,     // Apple property getter
  DW_AT_APPLE_property_setter = 0x3fea,     // Apple property setter
  DW_AT_APPLE_property_attribute = 0x3feb,  // Apple property attribute
  DW_AT_APPLE_objc_complete_type = 0x3fec,  // Apple Objective-C complete type
  DW_AT_APPLE_property = 0x3fed             // Apple property
} dwarf_attribute;

/**
 * @brief Enumeration of possible DWARF attribute value encodings.
 *
 * This enumeration defines the different ways DWARF attribute values can be
 * encoded in the debugging information. Each encoding corresponds to a specific
 * data type or reference mechanism used in the DWARF format.
 */
typedef enum attr_val_encoding {
  // No attribute value present.
  ATTR_VAL_NONE,
  // A memory address value.
  ATTR_VAL_ADDRESS,
  // An index into the .debug_addr section, relative to the DW_AT_addr_base
  // attribute of the compilation unit.
  ATTR_VAL_ADDRESS_INDEX,
  // An unsigned integer value.
  ATTR_VAL_UINT,
  // A signed integer value.
  ATTR_VAL_SINT,
  // A null-terminated string value.
  ATTR_VAL_STRING,
  // An index into the .debug_str_offsets section.
  ATTR_VAL_STRING_INDEX,
  // An offset to other data within the same compilation unit.
  ATTR_VAL_REF_UNIT,
  // An offset to other data within the .debug_info section.
  ATTR_VAL_REF_INFO,
  // An offset to other data within the alternate .debug_info section.
  ATTR_VAL_REF_ALT_INFO,
  // An offset to data in some other DWARF section.
  ATTR_VAL_REF_SECTION,
  // A type signature (8-byte unique identifier for a type).
  ATTR_VAL_REF_TYPE,
  // An index into the .debug_rnglists section for DWARF 5 range information.
  ATTR_VAL_RNGLISTS_INDEX,
  // A block of data (not directly represented in this structure).
  ATTR_VAL_BLOCK,
  // A DWARF expression (not directly represented in this structure).
  ATTR_VAL_EXPR,
} attr_val_encoding;

/**
 * @brief Structure representing a DWARF attribute value.
 *
 * This structure stores the value of a DWARF attribute along with its encoding
 * type. The actual value is stored in a union, with the appropriate field
 * determined by the encoding type.
 */
typedef struct attr_val {
  // Indicates how the value is encoded and which union field to use.
  enum attr_val_encoding encoding;

  // Union containing the attribute value in the appropriate format.
  union {
    // Used for: ATTR_VAL_ADDRESS, ATTR_VAL_ADDRESS_INDEX, ATTR_VAL_UINT,
    // ATTR_VAL_REF_UNIT, ATTR_VAL_REF_INFO, ATTR_VAL_REF_ALT_INFO,
    // ATTR_VAL_REF_SECTION, ATTR_VAL_REF_TYPE, ATTR_VAL_RNGLISTS_INDEX
    uint64_t uint;

    // Used for: ATTR_VAL_SINT
    int64_t sint;

    // Used for: ATTR_VAL_STRING, ATTR_VAL_STRING_INDEX
    const char *string;

    // Note: ATTR_VAL_BLOCK and ATTR_VAL_EXPR values are not stored directly
    // in this structure and require special handling elsewhere.
  } u;
} attr_val;

/**
 * @brief Represents a single attribute in a DWARF abbreviation.
 *
 * DWARF debugging information uses attributes to describe properties of
 * debugging entries. Each attribute has a name, a form that determines
 * how its value is encoded, and potentially a value.
 */
typedef struct attr {
  // The attribute name (e.g., DW_AT_name, DW_AT_location).
  // Identifies what kind of information this attribute provides.
  enum dwarf_attribute name;

  // The attribute form (e.g., DW_FORM_string, DW_FORM_data4).
  // Determines how the attribute's value is encoded in the DWARF data.
  enum dwarf_form form;

  // The attribute value, used only when form is DW_FORM_implicit_const.
  // For this form, the value is stored directly in the abbreviation
  // rather than in the DIE (Debugging Information Entry).
  int64_t val;
} attr;

TEN_UTILS_PRIVATE_API int read_attribute(ten_backtrace_t *self, dwarf_form form,
                                         uint64_t implicit_val, dwarf_buf *buf,
                                         int is_dwarf64, int version,
                                         int addrsize,
                                         const dwarf_sections *dwarf_sections,
                                         dwarf_data *altlink, attr_val *val);
