//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/unit.h"

#include <assert.h>
#include <stdlib.h>

/**
 * @brief Compare a unit offset against a unit for bsearch.
 *
 * This function is used as a comparison callback for the bsearch standard
 * library function to locate a DWARF compilation unit containing a specific
 * offset.
 *
 * @param vkey Pointer to the offset value to search for (cast to void*)
 * @param ventry Pointer to the unit pointer being compared against (cast to
 * void*)
 * @return -1 if offset is less than unit's low_offset,
 *          1 if offset is greater than or equal to unit's high_offset,
 *          0 if offset is within the unit's range (indicating a match)
 */
static int units_search(const void *vkey, const void *ventry) {
  const size_t *key = (const size_t *)vkey;
  const unit *entry = *((const unit *const *)ventry);
  size_t offset = *key;

  if (offset < entry->low_offset) {
    return -1;
  } else if (offset >= entry->high_offset) {
    return 1;
  } else {
    return 0;
  }
}

/**
 * @brief Find a DWARF compilation unit containing a specific offset.
 *
 * This function searches through an array of compilation units to find the one
 * that contains the specified offset. It uses binary search via the bsearch
 * standard library function for efficient lookup.
 *
 * @param pu Array of pointers to unit structures
 * @param units_count Number of elements in the pu array
 * @param offset The offset value to search for within the units
 * @return Pointer to the unit containing the offset, or NULL if not found
 */
unit *find_unit(unit **pu, size_t units_count, size_t offset) {
  if (pu == NULL || units_count == 0) {
    assert(0 && "Invalid arguments.");
    return NULL;
  }

  unit **u = (unit **)bsearch(&offset, (const void *)pu, units_count,
                              sizeof(unit *), units_search);
  return u == NULL ? NULL : *u;
}
