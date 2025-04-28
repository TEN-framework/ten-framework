//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include <stdint.h>
#include <stdlib.h>

typedef struct ten_smart_ptr_t ten_shared_ptr_t;

// The ten_go_handle_t is the C type corresponding to the `goHandle` in the GO
// world. Note that it is not the replacement of the definition of `uintptr_t`.
// It represents a GO object, which is passed from GO to C. It can _not_ be
// interpreted as an address in C.
typedef uintptr_t ten_go_handle_t;
typedef struct ten_error_t ten_error_t;

typedef struct ten_go_handle_array_t {
  ten_go_handle_t *array;
  size_t size;
} ten_go_handle_array_t;

typedef struct ten_go_bridge_t {
  // The following two fields are used to prevent the bridge instance from being
  // finalized. The bridge instance is finalized when both of the following two
  // fields are destroyed.
  ten_shared_ptr_t *sp_ref_by_c;
  ten_shared_ptr_t *sp_ref_by_go;

  // This is a handle to the Go instance.
  ten_go_handle_t go_instance;
} ten_go_bridge_t;

// Return type for functions invoked from GO. The `ten_go_error_t` should
// always be instantiated on the stack. This approach eliminates the need for
// freeing it from the GO side, thereby reducing one cgo call.
typedef struct ten_go_error_t {
  // The errno is always 0 if no error.
  // The type of this field must equal to the errno of ten_error_t.
  int64_t error_code;

  // The actually size of error_message, not including the null terminator. It
  // can be used directly to determine if error_message is empty in GO, without
  // any cgo call.
  //
  // Note that the max size of error_message is TEN_GO_STATUS_ERR_MSG_BUF_SIZE -
  // 1, so the type is uint8_t here.
  uint8_t error_message_size;

  // The error_message is always NULL if no error.
  //
  // All functions invoked from GO return a `ten_go_error_t` instance, not a
  // pointer. The `error_message` field is defined as a pointer, but not an
  // array with fixed size such as `char error_message[256]`.
  //
  // The advantages and disadvantages of the two approaches are as follows:
  //
  // - If the declaration is `char error_message[256]`.
  //
  //   Advantages: The `error_message` is always allocated on the stack, so no
  //   cgo call is needed to free it.
  //
  //   Disadvantages: The size of `ten_go_error_t` is 264, which is too large.
  //   And as the `error_message` is always allocated on the stack and the
  //   `ten_go_error_t` is returned by value not reference, CGO will allocate
  //   a piece of memory with the same size of `ten_go_error_t`, no matter the
  //   `error_message` is empty or not. In other words, 264 bytes will be
  //   allocated in each cgo call.
  //
  //   Note that we can retrieve the size of `ten_go_error_t` using
  //   `unsafe.Sizeof(C.ten_go_error_t{})` in GO.
  //
  // - If the declaration is `char* error_message`.
  //
  //   Advantages: The size of `ten_go_error_t` is 16. And as the
  //   `error_message` is always allocated on the heap, no memory is allocated
  //   in returning `ten_go_error_t` from C to GO through CGO (after compilation
  //   optimization).
  //
  //   Disadvantages: The `error_message` is always allocated on the heap, so a
  //   cgo call is needed if it is not NULL.
  //
  // In most cases, the error_code is 0, and the error_message is empty. So the
  // second way is better.
  //
  // BTW, if the declaration is `char error_message[256]`, the asm code of
  // copying `ten_go_error_t` in cgo functions is as follows.
  //
  //   LEAQ type:lPIqNLDo(SB), AX
  //   CALL runtime.newobject(SB)
  //   MOVQ AX, 0x148(SP)
  //           ⋮
  //   CMPL runtime.writeBarrier(SB), $0x0
  //   JE 0x6eed12
  //   LEAQ type:lPIqNLDo(SB), AX
  //   MOVQ 0x148(SP), BX
  //   MOVQ SP, CX
  //   CALL local.runtime.wbMove(SB)
  //   MOVQ 0x148(SP), DI
  //   MOVQ SP, SI
  //   NOPL 0(AX)
  //   MOVQ BP, -0x10(SP)
  //   LEAQ -0x10(SP), BP
  //   CALL 0x5793b2
  //   MOVQ 0(BP), BP
  //
  // The first line loads the type of `ten_go_error_t` into AX, and the second
  // line allocates memory using `runtime.newobject` which definition is as
  // follows.
  //
  //  func newobject(typ *_type) unsafe.Pointer {
  //    return mallocgc(typ.size, typ, true)
  //  }
  //
  // The first parameter `typ.size` is the size of `ten_go_error_t`.
  //
  // Why is there memory allocation (i.e., CALL runtime.newobject) during cgo
  // call if the declaration is `char error_message[256]`?
  //
  // * Cgo will generate a GO type corresponding to `ten_go_error_t` in C, ex:
  //
  //   type _Ctype_struct_ten_go_status_t struct {
  //     error_code          _Ctype_int
  //     error_message_size  _Ctype_uint8_t
  //     error_message       [256]_Ctype_char
  //     _                   [3]byte
  //   }
  //
  //   From the GO side, any C function returns `ten_go_error_t`, there will be
  //   a corresponding GO function returns `_Ctype_struct_ten_go_status_t`.
  //   Refer to chapter `cgo` in README.md for more details.
  //
  // * The C functions return `ten_go_error_t` by value, not reference. So the
  //   GO stack frame shall be prepared before calling C functions, and the
  //   memory for returned value will be allocated, i.e.,
  //   sizeof(_Ctype_struct_ten_go_status_t).
  char *error_message;
} ten_go_error_t;

/**
 * @brief Copy the C string to the GO slice and free the C string. It's a
 * replacement of `C.GoBytes` provided by cgo.
 *
 * @param src The C string to be copied.
 *
 * @param dest The pointer to the underlying array of the GO slice.
 *
 * The reason why we need this function is that the `C.GoBytes` provided by cgo
 * might have performance issue. The main logic of `C.GoBytes` is as follows.
 *
 *  func gobytes(p *byte, n int) (b []byte) {
 *    bp := mallocgc(uintptr(n), nil, false)
 *	  memmove(bp, unsafe.Pointer(p), uintptr(n))
 *  }
 *
 * The disadvantages are as follows.
 *
 * - A new slice is always allocated in each call of `C.GoBytes`, which means we
 *   can not use a slice pool to reduce memory allocation.
 *
 * - The `memmove` implemented in GO is slower than glibc, refer to
 *   [issue](https://github.com/golang/go/issues/38512).
 */
ten_go_error_t ten_go_copy_c_str_to_slice_and_free(const char *src, void *dest);
