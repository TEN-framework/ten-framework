//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/ten_config.h"

#include <assert.h>
#include <string.h>

#include "include_internal/ten_utils/backtrace/backtrace.h"
#include "include_internal/ten_utils/backtrace/platform/posix/dwarf.h"
#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/buf.h"

/**
 * @brief Read one zero-terminated string from buffer and advance past it.
 *
 * This function reads a null-terminated string from the current position in the
 * buffer and advances the buffer position past the string (including the null
 * terminator).
 *
 * If the string is not properly null-terminated within the buffer's remaining
 * space, an error will be generated when trying to advance past the end of
 * the buffer.
 *
 * @param self The backtrace context.
 * @param buf The buffer to read from.
 * @return A pointer to the string in the buffer, or NULL if an error occurred.
 *         Note that the returned pointer is only valid until the buffer is
 *         modified or freed.
 */
const char *read_string(ten_backtrace_t *self, dwarf_buf *buf) {
  assert(buf && "Invalid buffer argument.");

  const char *p = (const char *)buf->buf;
  size_t len = strnlen(p, buf->left);

  // Handle two cases:
  // - If len == buf->left, no null terminator was found within the buffer's
  //   remaining space. We'll try to advance len + 1 bytes, which will trigger
  //   an error in advance().
  // - If len < buf->left, we found a null terminator, so advance past the
  //   string and its terminator (len + 1 bytes).
  if (!advance(self, buf, len + 1)) {
    return NULL;
  }

  return p;
}

/**
 * @brief Read one byte from buffer and advance the position by 1 byte.
 *
 * This function reads a single byte from the current position in the buffer
 * and advances the buffer position by 1 byte. If the buffer doesn't have
 * enough remaining data, an error will be reported through the advance()
 * function.
 *
 * @param self The backtrace context.
 * @param buf The buffer to read from.
 * @return The byte value read from the buffer, or 0 if an error occurred
 *         (such as buffer underflow).
 */
unsigned char read_byte(ten_backtrace_t *self, dwarf_buf *buf) {
  assert(buf && "Invalid buffer argument.");

  const unsigned char *p = buf->buf;

  if (!advance(self, buf, 1)) {
    return 0;
  }
  return p[0];
}

/**
 * @brief Read a signed byte from buffer and advance the position by 1 byte.
 *
 * This function reads a single signed byte from the current position in the
 * buffer and advances the buffer position by 1 byte. If the buffer doesn't have
 * enough remaining data, an error will be reported through the advance()
 * function.
 *
 * The implementation uses a bit manipulation technique to convert an unsigned
 * byte to a signed byte by XORing with 0x80 and then subtracting 0x80, which
 * effectively reinterprets the value as a signed byte.
 *
 * @param self The backtrace context.
 * @param buf The buffer to read from.
 * @return The signed byte value read from the buffer, or 0 if an error occurred
 *         (such as buffer underflow).
 */
signed char read_sbyte(ten_backtrace_t *self, dwarf_buf *buf) {
  assert(buf && "Invalid buffer argument.");

  const unsigned char *p = buf->buf;

  if (!advance(self, buf, 1)) {
    return 0;
  }
  return (signed char)((*p ^ 0x80) - 0x80);
}

/**
 * @brief Read a 16-bit unsigned integer from buffer and advance the position by
 * 2 bytes.
 *
 * This function reads a 16-bit unsigned integer from the current position in
 * the buffer and advances the buffer position by 2 bytes. If the buffer doesn't
 * have enough remaining data, an error will be reported through the advance()
 * function.
 *
 * The function handles both big-endian and little-endian byte ordering based on
 * the buffer's is_bigendian flag.
 *
 * @param self The backtrace context.
 * @param buf The buffer to read from.
 * @return The 16-bit unsigned integer value read from the buffer, or 0 if an
 * error occurred (such as buffer underflow).
 */
uint16_t read_uint16(ten_backtrace_t *self, dwarf_buf *buf) {
  assert(buf && "Invalid buffer argument.");

  const unsigned char *p = buf->buf;

  if (!advance(self, buf, 2)) {
    return 0;
  }

  if (buf->is_bigendian) {
    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
  } else {
    return ((uint16_t)p[1] << 8) | (uint16_t)p[0];
  }
}

/**
 * @brief Read a 24-bit unsigned integer from buffer and advance the position by
 * 3 bytes.
 *
 * This function reads a 24-bit unsigned integer from the current position in
 * the buffer and advances the buffer position by 3 bytes. If the buffer doesn't
 * have enough remaining data, an error will be reported through the advance()
 * function.
 *
 * The function handles both big-endian and little-endian byte ordering based on
 * the buffer's is_bigendian flag.
 *
 * @param self The backtrace context.
 * @param buf The buffer to read from.
 * @return The 24-bit unsigned integer value read from the buffer (as a
 * uint32_t), or 0 if an error occurred (such as buffer underflow).
 */
uint32_t read_uint24(ten_backtrace_t *self, dwarf_buf *buf) {
  assert(buf && "Invalid buffer argument.");

  const unsigned char *p = buf->buf;

  if (!advance(self, buf, 3)) {
    return 0;
  }

  if (buf->is_bigendian) {
    return (((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2]);
  } else {
    return (((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[0]);
  }
}

/**
 * @brief Read a 32-bit unsigned integer from buffer and advance the position by
 * 4 bytes.
 *
 * This function reads a 32-bit unsigned integer from the current position in
 * the buffer and advances the buffer position by 4 bytes. If the buffer doesn't
 * have enough remaining data, an error will be reported through the advance()
 * function.
 *
 * The function handles both big-endian and little-endian byte ordering based on
 * the buffer's is_bigendian flag.
 *
 * @param self The backtrace context.
 * @param buf The buffer to read from.
 * @return The 32-bit unsigned integer value read from the buffer, or 0 if an
 * error occurred (such as buffer underflow).
 */
uint32_t read_uint32(ten_backtrace_t *self, dwarf_buf *buf) {
  assert(buf && "Invalid buffer argument.");

  const unsigned char *p = buf->buf;

  if (!advance(self, buf, 4)) {
    return 0;
  }

  if (buf->is_bigendian) {
    return (((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
            ((uint32_t)p[2] << 8) | (uint32_t)p[3]);
  } else {
    return (((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) |
            ((uint32_t)p[1] << 8) | (uint32_t)p[0]);
  }
}

/**
 * @brief Read a 64-bit unsigned integer from buffer and advance the position by
 * 8 bytes.
 *
 * This function reads a 64-bit unsigned integer from the current position in
 * the buffer and advances the buffer position by 8 bytes. If the buffer doesn't
 * have enough remaining data, an error will be reported through the advance()
 * function.
 *
 * The function handles both big-endian and little-endian byte ordering based on
 * the buffer's is_bigendian flag.
 *
 * @param self The backtrace context.
 * @param buf The buffer to read from.
 * @return The 64-bit unsigned integer value read from the buffer, or 0 if an
 * error occurred (such as buffer underflow).
 */
uint64_t read_uint64(ten_backtrace_t *self, dwarf_buf *buf) {
  assert(buf && "Invalid buffer argument.");

  const unsigned char *p = buf->buf;

  if (!advance(self, buf, 8)) {
    return 0;
  }

  if (buf->is_bigendian) {
    return (((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48) |
            ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32) |
            ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16) |
            ((uint64_t)p[6] << 8) | (uint64_t)p[7]);
  } else {
    return (((uint64_t)p[7] << 56) | ((uint64_t)p[6] << 48) |
            ((uint64_t)p[5] << 40) | ((uint64_t)p[4] << 32) |
            ((uint64_t)p[3] << 24) | ((uint64_t)p[2] << 16) |
            ((uint64_t)p[1] << 8) | (uint64_t)p[0]);
  }
}

/**
 * @brief Read a DWARF offset from buffer and advance the position accordingly.
 *
 * This function reads an offset value from the current position in the buffer.
 * The size of the offset depends on whether DWARF64 format is being used:
 * - For DWARF64 format (is_dwarf64 = 1): Reads an 8-byte offset
 * - For standard DWARF format (is_dwarf64 = 0): Reads a 4-byte offset
 *
 * The function handles both big-endian and little-endian byte ordering based on
 * the buffer's is_bigendian flag.
 *
 * @param self The backtrace context.
 * @param buf The buffer to read from.
 * @param is_dwarf64 Flag indicating whether DWARF64 format is being used.
 * @return The offset value read from the buffer, or 0 if an error occurred
 *         (such as buffer underflow).
 */
uint64_t read_offset(ten_backtrace_t *self, dwarf_buf *buf, int is_dwarf64) {
  assert(buf && "Invalid buffer argument.");

  if (is_dwarf64) {
    return read_uint64(self, buf);
  } else {
    return read_uint32(self, buf);
  }
}

/**
 * @brief Read an address from buffer and advance the position accordingly.
 *
 * This function reads an address value from the current position in the buffer.
 * The size of the address depends on the addrsize parameter, which is typically
 * determined by the compilation unit's address_size field in DWARF.
 *
 * The function handles addresses of different sizes:
 * - 1-byte addresses (rare)
 * - 2-byte addresses (uncommon)
 * - 4-byte addresses (32-bit systems)
 * - 8-byte addresses (64-bit systems)
 *
 * If an unrecognized address size is encountered, an error is reported and 0 is
 * returned.
 *
 * @param self The backtrace context.
 * @param buf The buffer to read from.
 * @param addrsize The size of the address in bytes (typically 4 or 8).
 * @return The address value read from the buffer, or 0 if an error occurred.
 */
uint64_t read_address(ten_backtrace_t *self, dwarf_buf *buf, int addrsize) {
  assert(buf && "Invalid buffer argument.");

  switch (addrsize) {
  case 1:
    return read_byte(self, buf);
  case 2:
    return read_uint16(self, buf);
  case 4:
    return read_uint32(self, buf);
  case 8:
    return read_uint64(self, buf);
  default:
    dwarf_buf_error(self, buf, "unrecognized address size", 0);
    return 0;
  }
}

/**
 * @brief Read an unsigned LEB128 (Little Endian Base 128) number from buffer.
 *
 * This function reads a variable-length encoded unsigned integer from the
 * current position in the buffer. LEB128 encoding is used in DWARF to
 * efficiently represent integers of arbitrary size.
 *
 * The encoding works as follows:
 * - Each byte uses 7 bits for the value and 1 bit (MSB) as a continuation flag
 * - If the MSB is set (1), more bytes follow
 * - If the MSB is clear (0), this is the last byte
 * - The value is constructed by concatenating the 7-bit chunks, starting with
 *   the least significant bits
 *
 * The function handles overflow by reporting an error if the value exceeds
 * 64 bits, but continues processing to maintain the correct buffer position.
 *
 * @param self The backtrace context.
 * @param buf The buffer to read from.
 * @return The decoded unsigned integer value, or 0 if an error occurred.
 */
uint64_t read_uleb128(ten_backtrace_t *self, dwarf_buf *buf) {
  assert(buf && "Invalid buffer argument.");

  unsigned char b = 0;
  uint64_t ret = 0;
  unsigned int shift = 0;
  int overflow = 0;

  do {
    const unsigned char *p = buf->buf;
    if (!advance(self, buf, 1)) {
      return 0;
    }
    b = *p;
    if (shift < 64) {
      ret |= ((uint64_t)(b & 0x7f)) << shift;
    } else if (!overflow) {
      dwarf_buf_error(self, buf, "LEB128 overflows uint64_t", 0);
      overflow = 1;
    }
    shift += 7;
  } while ((b & 0x80) != 0);

  return ret;
}

/**
 * @brief Read a signed LEB128 (Little Endian Base 128) number from buffer.
 *
 * This function reads a variable-length encoded signed integer from the
 * current position in the buffer. LEB128 encoding is used in DWARF to
 * efficiently represent integers of arbitrary size.
 *
 * The encoding works as follows:
 * - Each byte uses 7 bits for the value and 1 bit (MSB) as a continuation flag
 * - If the MSB is set (1), more bytes follow
 * - If the MSB is clear (0), this is the last byte
 * - The value is constructed by concatenating the 7-bit chunks, starting with
 *   the least significant bits
 * - For signed numbers, the sign bit is the most significant bit of the last
 * byte (bit 6)
 * - If the sign bit is set, the value is sign-extended
 *
 * The function handles overflow by reporting an error if the value exceeds
 * 64 bits, but continues processing to maintain the correct buffer position.
 *
 * @param self The backtrace context.
 * @param buf The buffer to read from.
 * @return The decoded signed integer value, or 0 if an error occurred.
 */
int64_t read_sleb128(ten_backtrace_t *self, dwarf_buf *buf) {
  assert(buf && "Invalid buffer argument.");

  unsigned char b = 0;
  uint64_t val = 0;
  unsigned int shift = 0;
  int overflow = 0;

  do {
    const unsigned char *p = buf->buf;
    if (!advance(self, buf, 1)) {
      return 0;
    }

    b = *p;
    if (shift < 64) {
      val |= ((uint64_t)(b & 0x7f)) << shift;
    } else if (!overflow) {
      dwarf_buf_error(self, buf, "signed LEB128 overflows int64_t", 0);
      overflow = 1;
    }
    shift += 7;
  } while ((b & 0x80) != 0);

  // Sign extend if necessary (when the sign bit is set).
  if ((b & 0x40) != 0 && shift < 64) {
    val |= ((uint64_t)-1) << shift;
  }

  return (int64_t)val;
}

/**
 * @brief Reads the initial length field from a DWARF section.
 *
 * In DWARF, the initial length field can be either 4 or 12 bytes:
 * - If the first 4 bytes are not 0xffffffff, it's a 32-bit length field.
 * - If the first 4 bytes are 0xffffffff, it's followed by an 8-byte length
 *   field (DWARF64 format).
 *
 * This function reads the appropriate length value and sets is_dwarf64 to
 * indicate which format was detected. The buffer position is advanced past the
 * length field.
 *
 * @param self The backtrace context.
 * @param buf The buffer to read from.
 * @param is_dwarf64 Pointer to store whether DWARF64 format was detected (1) or
 * not (0).
 * @return The length value read from the buffer, or 0 if an error occurred.
 */
uint64_t read_initial_length(ten_backtrace_t *self, dwarf_buf *buf,
                             int *is_dwarf64) {
  assert(buf && "Invalid buffer argument.");
  assert(is_dwarf64 && "Invalid is_dwarf64 argument.");

  uint64_t len = read_uint32(self, buf);
  if (len == 0xffffffff) {
    // This is DWARF64 format - the real length follows as an 8-byte value.
    len = read_uint64(self, buf);
    *is_dwarf64 = 1;
  } else {
    *is_dwarf64 = 0;
  }

  return len;
}
