//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/line.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "include_internal/ten_utils/backtrace/backtrace.h"
#include "include_internal/ten_utils/backtrace/platform/posix/dwarf.h"
#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/buf.h"
#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/data.h"
#include "include_internal/ten_utils/backtrace/sort.h"

// Read a single version 5 LNCT entry for a directory or file name in a
// line header.  Sets *STRING to the resulting name, ignoring other
// data.  Return 1 on success, 0 on failure.
static int read_lnct(ten_backtrace_t *self, dwarf_data *ddata, unit *u,
                     dwarf_buf *hdr_buf, const line_header *hdr,
                     size_t formats_count, const line_header_format *formats,
                     const char **string) {
  size_t i = 0;
  const char *dir = NULL;
  const char *path = NULL;

  for (i = 0; i < formats_count; i++) {
    attr_val val;

    if (!read_attribute(self, formats[i].form, 0, hdr_buf, u->is_dwarf64,
                        u->version, hdr->addrsize, &ddata->dwarf_sections,
                        ddata->altlink, &val)) {
      return 0;
    }

    switch (formats[i].lnct) {
    case DW_LNCT_path:
      if (!resolve_string(self, &ddata->dwarf_sections, u->is_dwarf64,
                          ddata->is_bigendian, u->str_offsets_base, &val,
                          hdr_buf->on_error, hdr_buf->data, &path)) {
        return 0;
      }
      break;
    case DW_LNCT_directory_index:
      if (val.encoding == ATTR_VAL_UINT) {
        if (val.u.uint >= hdr->dirs_count) {
          dwarf_buf_error(self, hdr_buf,
                          ("Invalid directory index in "
                           "line number program header"),
                          0);
          return 0;
        }
        dir = hdr->dirs[val.u.uint];
      }
      break;
    default:
      // We don't care about timestamps or sizes or hashes.
      break;
    }
  }

  if (path == NULL) {
    dwarf_buf_error(self, hdr_buf,
                    "missing file name in line number program header", 0);
    return 0;
  }

  if (dir == NULL) {
    *string = path;
  } else {
    size_t dir_len = strlen(dir);
    size_t path_len = strlen(path);
    char *s = (char *)malloc(dir_len + path_len + 2);
    if (s == NULL) {
      assert(0 && "Failed to allocate memory.");
      return 0;
    }

    memcpy(s, dir, dir_len);

    // FIXME: If we are on a DOS-based file system, and the
    // directory or the path name use backslashes, then we should
    // use a backslash here.
    s[dir_len] = '/';
    memcpy(s + dir_len + 1, path, path_len + 1);
    *string = s;
  }

  return 1;
}

// Read a set of DWARF 5 line header format entries, setting *PCOUNT
// and *PPATHS.  Return 1 on success, 0 on failure.
static int read_line_header_format_entries(ten_backtrace_t *self,
                                           dwarf_data *ddata, unit *u,
                                           dwarf_buf *hdr_buf, line_header *hdr,
                                           size_t *pcount,
                                           const char ***ppaths) {
  size_t formats_count = 0;
  line_header_format *formats = NULL;
  size_t paths_count = 0;
  size_t i = 0;
  int ret = 0;

  formats_count = read_byte(self, hdr_buf);
  if (formats_count == 0) {
    formats = NULL;
  } else {
    formats = (line_header_format *)malloc(
        (formats_count * sizeof(line_header_format)));
    if (formats == NULL) {
      return 0;
    }

    for (i = 0; i < formats_count; i++) {
      formats[i].lnct = (int)read_uleb128(self, hdr_buf);
      formats[i].form = (dwarf_form)read_uleb128(self, hdr_buf);
    }
  }

  paths_count = read_uleb128(self, hdr_buf);
  if (paths_count == 0) {
    *pcount = 0;
    *ppaths = NULL;
    ret = 1;
    goto exit;
  }

  const char **paths =
      (const char **)malloc(paths_count * sizeof(const char *));
  if (paths == NULL) {
    assert(0 && "Failed to allocate memory.");
    ret = 0;
    goto exit;
  }

  for (i = 0; i < paths_count; i++) {
    if (!read_lnct(self, ddata, u, hdr_buf, hdr, formats_count, formats,
                   &paths[i])) {
      free((void *)paths);
      ret = 0;
      goto exit;
    }
  }

  *pcount = paths_count;
  *ppaths = paths;

  ret = 1;

exit:
  if (formats != NULL) {
    free(formats);
  }

  return ret;
}

// Return the length of an LEB128 number.
static size_t leb128_len(const unsigned char *p) {
  size_t ret = 1;
  while ((*p & 0x80) != 0) {
    ++p;
    ++ret;
  }
  return ret;
}

// Read the directories and file names for a line header for version
// 2, setting fields in HDR.  Return 1 on success, 0 on failure.
static int read_v2_paths(ten_backtrace_t *self, unit *u, dwarf_buf *hdr_buf,
                         line_header *hdr) {
  const unsigned char *p = NULL;
  const unsigned char *pend = NULL;
  size_t i = 0;

  // Count the number of directory entries.
  hdr->dirs_count = 0;
  p = hdr_buf->buf;
  pend = p + hdr_buf->left;
  while (p < pend && *p != '\0') {
    p += strnlen((const char *)p, pend - p) + 1;
    ++hdr->dirs_count;
  }

  // The index of the first entry in the list of directories is 1.  Index 0 is
  // used for the current directory of the compilation.  To simplify index
  // handling, we set entry 0 to the compilation unit directory.
  ++hdr->dirs_count;
  hdr->dirs = (const char **)malloc(hdr->dirs_count * sizeof(const char *));
  if (hdr->dirs == NULL) {
    assert(0 && "Failed to allocate memory.");
    return 0;
  }

  hdr->dirs[0] = u->comp_dir;
  i = 1;
  while (*hdr_buf->buf != '\0') {
    if (hdr_buf->reported_underflow) {
      return 0;
    }

    hdr->dirs[i] = read_string(self, hdr_buf);
    if (hdr->dirs[i] == NULL) {
      return 0;
    }
    ++i;
  }
  if (!advance(self, hdr_buf, 1)) {
    return 0;
  }

  // Count the number of file entries.
  hdr->filenames_count = 0;
  p = hdr_buf->buf;
  pend = p + hdr_buf->left;
  while (p < pend && *p != '\0') {
    p += strnlen((const char *)p, pend - p) + 1;
    p += leb128_len(p);
    p += leb128_len(p);
    p += leb128_len(p);
    ++hdr->filenames_count;
  }

  // The index of the first entry in the list of file names is 1.  Index 0 is
  // used for the DW_AT_name of the compilation unit.  To simplify index
  // handling, we set entry 0 to the compilation unit file name.
  ++hdr->filenames_count;
  hdr->filenames = (const char **)malloc(hdr->filenames_count * sizeof(char *));
  if (hdr->filenames == NULL) {
    return 0;
  }

  hdr->filenames[0] = u->filename;
  i = 1;
  while (*hdr_buf->buf != '\0') {
    const char *filename = NULL;
    uint64_t dir_index = 0;

    if (hdr_buf->reported_underflow) {
      return 0;
    }

    filename = read_string(self, hdr_buf);
    if (filename == NULL) {
      return 0;
    }
    dir_index = read_uleb128(self, hdr_buf);
    if (IS_ABSOLUTE_PATH(filename) ||
        (dir_index < hdr->dirs_count && hdr->dirs[dir_index] == NULL)) {
      hdr->filenames[i] = filename;
    } else {
      const char *dir = NULL;
      size_t dir_len = 0;
      size_t filename_len = 0;
      char *s = NULL;

      if (dir_index < hdr->dirs_count) {
        dir = hdr->dirs[dir_index];
      } else {
        dwarf_buf_error(self, hdr_buf,
                        ("Invalid directory index in "
                         "line number program header"),
                        0);
        return 0;
      }

      dir_len = strlen(dir);
      filename_len = strlen(filename);
      s = (char *)malloc(dir_len + filename_len + 2);
      if (s == NULL) {
        return 0;
      }

      memcpy(s, dir, dir_len);

      // FIXME: If we are on a DOS-based file system, and the
      // directory or the file name use backslashes, then we
      // should use a backslash here.
      s[dir_len] = '/';
      memcpy(s + dir_len + 1, filename, filename_len + 1);
      hdr->filenames[i] = s;
    }

    // Ignore the modification time and size.
    read_uleb128(self, hdr_buf);
    read_uleb128(self, hdr_buf);

    ++i;
  }

  return 1;
}

// Read the line header.  Return 1 on success, 0 on failure.
static int read_line_header(ten_backtrace_t *self, dwarf_data *ddata, unit *u,
                            int is_dwarf64, dwarf_buf *line_buf,
                            line_header *hdr) {
  uint64_t hdrlen = 0;
  dwarf_buf hdr_buf;

  hdr->version = read_uint16(self, line_buf);
  if (hdr->version < 2 || hdr->version > 5) {
    dwarf_buf_error(self, line_buf, "unsupported line number version", -1);
    return 0;
  }

  if (hdr->version < 5) {
    hdr->addrsize = u->addrsize;
  } else {
    hdr->addrsize = read_byte(self, line_buf);
    // We could support a non-zero segment_selector_size but I doubt
    // we'll ever see it.
    if (read_byte(self, line_buf) != 0) {
      dwarf_buf_error(self, line_buf,
                      "non-zero segment_selector_size not supported", -1);
      return 0;
    }
  }

  hdrlen = read_offset(self, line_buf, is_dwarf64);

  hdr_buf = *line_buf;
  hdr_buf.left = hdrlen;

  if (!advance(self, line_buf, hdrlen)) {
    return 0;
  }

  hdr->min_insn_len = read_byte(self, &hdr_buf);
  if (hdr->version < 4) {
    hdr->max_ops_per_insn = 1;
  } else {
    hdr->max_ops_per_insn = read_byte(self, &hdr_buf);
  }

  // We don't care about default_is_stmt.
  read_byte(self, &hdr_buf);

  hdr->line_base = read_sbyte(self, &hdr_buf);
  hdr->line_range = read_byte(self, &hdr_buf);

  hdr->opcode_base = read_byte(self, &hdr_buf);
  hdr->opcode_lengths = hdr_buf.buf;
  if (!advance(self, &hdr_buf, hdr->opcode_base - 1)) {
    return 0;
  }

  if (hdr->version < 5) {
    if (!read_v2_paths(self, u, &hdr_buf, hdr)) {
      return 0;
    }
  } else {
    if (!read_line_header_format_entries(self, ddata, u, &hdr_buf, hdr,
                                         &hdr->dirs_count, &hdr->dirs)) {
      return 0;
    }

    if (!read_line_header_format_entries(self, ddata, u, &hdr_buf, hdr,
                                         &hdr->filenames_count,
                                         &hdr->filenames)) {
      return 0;
    }
  }

  if (hdr_buf.reported_underflow) {
    return 0;
  }

  return 1;
}

// Add a new mapping to the vector of line mappings that we are
// building.  Returns 1 on success, 0 on failure.
static int add_line(ten_backtrace_t *self, dwarf_data *ddata, uintptr_t pc,
                    const char *filename, int lineno,
                    ten_backtrace_on_error_func_t on_error, void *data,
                    line_vector *vec) {
  line *ln = NULL;

  // If we are adding the same mapping, ignore it.  This can happen
  // when using discriminators.
  if (vec->count > 0) {
    ln = (line *)vec->vec.data + (vec->count - 1);
    if (pc == ln->pc && filename == ln->filename && lineno == ln->lineno) {
      return 1;
    }
  }

  ln = (line *)ten_vector_grow(&vec->vec, sizeof(line));
  if (ln == NULL) {
    return 0;
  }

  // Add in the base address here, so that we can look up the PC
  // directly.
  ln->pc = pc + ddata->base_address;

  ln->filename = filename;
  ln->lineno = lineno;
  ln->idx = vec->count;

  ++vec->count;

  return 1;
}

// Read the line program, adding line mappings to VEC.  Return 1 on
// success, 0 on failure.
static int read_line_program(ten_backtrace_t *self, dwarf_data *ddata,
                             const line_header *hdr, dwarf_buf *line_buf,
                             line_vector *vec) {
  uint64_t address = 0;
  unsigned int op_index = 0;
  const char *reset_filename = NULL;
  const char *filename = NULL;
  int lineno = 1;

  if (hdr->filenames_count > 1) {
    reset_filename = hdr->filenames[1];
  } else {
    reset_filename = "";
  }

  filename = reset_filename;
  lineno = 1;
  while (line_buf->left > 0) {
    unsigned int op = read_byte(self, line_buf);
    if (op >= hdr->opcode_base) {
      unsigned int advance = 0;

      // Special opcode.
      op -= hdr->opcode_base;
      advance = op / hdr->line_range;
      address +=
          (hdr->min_insn_len * (op_index + advance) / hdr->max_ops_per_insn);
      op_index = (op_index + advance) % hdr->max_ops_per_insn;
      lineno += hdr->line_base + (int)(op % hdr->line_range);
      add_line(self, ddata, address, filename, lineno, line_buf->on_error,
               line_buf->data, vec);
    } else if (op == DW_LNS_extended_op) {
      uint64_t len = read_uleb128(self, line_buf);
      op = read_byte(self, line_buf);
      switch (op) {
      case DW_LNE_end_sequence:
        // FIXME: Should we mark the high PC here?  It seems
        // that we already have that information from the
        // compilation unit.
        address = 0;
        op_index = 0;
        filename = reset_filename;
        lineno = 1;
        break;
      case DW_LNE_set_address:
        address = read_address(self, line_buf, hdr->addrsize);
        break;
      case DW_LNE_define_file: {
        const char *f;
        unsigned int dir_index;

        f = read_string(self, line_buf);
        if (f == NULL) {
          return 0;
        }
        dir_index = read_uleb128(self, line_buf);
        // Ignore that time and length.
        read_uleb128(self, line_buf);
        read_uleb128(self, line_buf);
        if (IS_ABSOLUTE_PATH(f)) {
          filename = f;
        } else {
          const char *dir = NULL;
          size_t dir_len = 0;
          size_t f_len = 0;
          char *p = NULL;

          if (dir_index < hdr->dirs_count) {
            dir = hdr->dirs[dir_index];
          } else {
            dwarf_buf_error(self, line_buf,
                            ("Invalid directory index "
                             "in line number program"),
                            0);
            return 0;
          }
          dir_len = strlen(dir);
          f_len = strlen(f);
          p = (char *)malloc(dir_len + f_len + 2);
          if (p == NULL) {
            return 0;
          }

          memcpy(p, dir, dir_len);

          // FIXME: If we are on a DOS-based file system,
          // and the directory or the file name use
          // backslashes, then we should use a backslash
          // here.
          p[dir_len] = '/';
          memcpy(p + dir_len + 1, f, f_len + 1);
          filename = p;
        }
      } break;
      case DW_LNE_set_discriminator:
        // We don't care about discriminators.
        read_uleb128(self, line_buf);
        break;
      default:
        if (!advance(self, line_buf, len - 1)) {
          return 0;
        }

        break;
      }
    } else {
      switch (op) {
      case DW_LNS_copy:
        add_line(self, ddata, address, filename, lineno, line_buf->on_error,
                 line_buf->data, vec);
        break;
      case DW_LNS_advance_pc: {
        uint64_t advance = 0;

        advance = read_uleb128(self, line_buf);
        address +=
            (hdr->min_insn_len * (op_index + advance) / hdr->max_ops_per_insn);
        op_index = (op_index + advance) % hdr->max_ops_per_insn;
      } break;
      case DW_LNS_advance_line:
        lineno += (int)read_sleb128(self, line_buf);
        break;
      case DW_LNS_set_file: {
        uint64_t fileno = 0;

        fileno = read_uleb128(self, line_buf);
        if (fileno >= hdr->filenames_count) {
          dwarf_buf_error(self, line_buf,
                          ("Invalid file number in "
                           "line number program"),
                          0);
          return 0;
        }
        filename = hdr->filenames[fileno];
      } break;
      case DW_LNS_set_column:
        read_uleb128(self, line_buf);
        break;
      case DW_LNS_negate_stmt:
      case DW_LNS_set_basic_block:
        break;
      case DW_LNS_const_add_pc: {
        unsigned int advance = 0;

        op = 255 - hdr->opcode_base;
        advance = op / hdr->line_range;
        address +=
            (hdr->min_insn_len * (op_index + advance) / hdr->max_ops_per_insn);
        op_index = (op_index + advance) % hdr->max_ops_per_insn;
      } break;
      case DW_LNS_fixed_advance_pc:
        address += read_uint16(self, line_buf);
        op_index = 0;
        break;
      case DW_LNS_set_prologue_end:
      case DW_LNS_set_epilogue_begin:
        break;
      case DW_LNS_set_isa:
        read_uleb128(self, line_buf);
        break;
      default: {
        unsigned int i;

        for (i = hdr->opcode_lengths[op - 1]; i > 0; --i) {
          read_uleb128(self, line_buf);
        }
      } break;
      }
    }
  }

  return 1;
}

// Free the line header information.
void free_line_header(ten_backtrace_t *self, line_header *hdr,
                      ten_backtrace_on_error_func_t on_error, void *data) {
  if ((hdr->dirs_count != 0) && (hdr->dirs != NULL)) {
    free(hdr->dirs);
  }
  if ((hdr->filenames_count != 0) && (hdr->filenames != NULL)) {
    free(hdr->filenames);
  }
}

// Sort the line vector by PC.  We want a stable sort here to maintain
// the order of lines for the same PC values.  Since the sequence is
// being sorted in place, their addresses cannot be relied on to
// maintain stability.  That is the purpose of the index member.
static int line_compare(const void *v1, const void *v2) {
  const line *ln1 = (const line *)v1;
  const line *ln2 = (const line *)v2;

  if (ln1->pc < ln2->pc) {
    return -1;
  } else if (ln1->pc > ln2->pc) {
    return 1;
  } else if (ln1->idx < ln2->idx) {
    return -1;
  } else if (ln1->idx > ln2->idx) {
    return 1;
  } else {
    return 0;
  }
}

// Read the line number information for a compilation unit.  Returns 1
// on success, 0 on failure.
int read_line_info(ten_backtrace_t *self, dwarf_data *ddata,
                   ten_backtrace_on_error_func_t on_error, void *data, unit *u,
                   line_header *hdr, line **lines, size_t *lines_count) {
  line_vector vec;
  dwarf_buf line_buf;
  uint64_t len = 0;
  int is_dwarf64 = 0;
  line *ln = NULL;

  memset(&vec.vec, 0, sizeof vec.vec);
  vec.count = 0;

  memset(hdr, 0, sizeof *hdr);

  if (u->lineoff != (off_t)(size_t)u->lineoff ||
      (size_t)u->lineoff >= ddata->dwarf_sections.size[DEBUG_LINE]) {
    on_error(self, "unit line offset out of range", 0, data);
    goto fail;
  }

  line_buf.name = ".debug_line";
  line_buf.start = ddata->dwarf_sections.data[DEBUG_LINE];
  line_buf.buf = ddata->dwarf_sections.data[DEBUG_LINE] + u->lineoff;
  line_buf.left = ddata->dwarf_sections.size[DEBUG_LINE] - u->lineoff;
  line_buf.is_bigendian = ddata->is_bigendian;
  line_buf.on_error = on_error;
  line_buf.data = data;
  line_buf.reported_underflow = 0;

  len = read_initial_length(self, &line_buf, &is_dwarf64);
  line_buf.left = len;

  if (!read_line_header(self, ddata, u, is_dwarf64, &line_buf, hdr)) {
    goto fail;
  }

  if (!read_line_program(self, ddata, hdr, &line_buf, &vec)) {
    goto fail;
  }

  if (line_buf.reported_underflow) {
    goto fail;
  }

  if (vec.count == 0) {
    // This is not a failure in the sense of a generating an error,
    // but it is a failure in that sense that we have no useful
    // information.
    goto fail;
  }

  // Allocate one extra entry at the end.
  ln = (line *)ten_vector_grow(&vec.vec, sizeof(line));
  if (ln == NULL) {
    goto fail;
  }

  ln->pc = (uintptr_t)-1;
  ln->filename = NULL;
  ln->lineno = 0;
  ln->idx = 0;

  if (!ten_vector_release_remaining_space(&vec.vec)) {
    goto fail;
  }

  ln = (line *)vec.vec.data;
  backtrace_sort(ln, vec.count, sizeof(line), line_compare);

  *lines = ln;
  *lines_count = vec.count;

  return 1;

fail:
  ten_vector_deinit(&vec.vec);

  free_line_header(self, hdr, on_error, data);
  *lines = (line *)(uintptr_t)-1;
  *lines_count = 0;
  return 0;
}
