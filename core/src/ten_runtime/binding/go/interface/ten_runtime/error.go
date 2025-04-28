//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten

// #include "common.h"
import "C"

import (
	"fmt"
	"unsafe"
)

// TenError is the standard error returned to user from the golang binding. It
// shall be exported, as the callers will determine whether an error is
// TenError.
type TenError struct {
	errorCode    uint32
	errorMessage string
}

// newTenError constructor of ApiError. Note that the ApiError is always created
// from the golang binding, so it's unexported.
func newTenError(errorCode uint32, errorMessage string) error {
	return &TenError{
		errorCode:    errorCode,
		errorMessage: errorMessage,
	}
}

// withCGoError creates an TenError based on the api status from C. Note that
// the `error_message` in `cgoError` will be freed after this function, do not
// access
// it again.
func withCGoError(cgoError *C.ten_go_error_t) error {
	if cgoError.error_code == 0 {
		// No error.
		return nil
	}

	if cgoError.error_message_size == 0 {
		// An error occurred, but no error message.
		return &TenError{
			errorCode: uint32(cgoError.error_code),
		}
	}

	// It's crucial to free the memory allocated in the C environment to prevent
	// memory leaks. Since C.GoString creates a copy of the memory content, it
	// is safe to release the 'error_message' memory in C after its use.
	defer C.free(unsafe.Pointer(cgoError.error_message))

	return &TenError{
		errorCode:    uint32(cgoError.error_code),
		errorMessage: C.GoString(cgoError.error_message),
	}
}

func (e *TenError) Error() string {
	return fmt.Sprintf(
		"error_code: %d, error_message: %s",
		e.errorCode,
		e.errorMessage,
	)
}

// ErrorCode returns the inner error number.
func (e *TenError) ErrorCode() uint32 {
	return e.errorCode
}

// ErrorMessage returns the inner error message.
func (e *TenError) ErrorMessage() string {
	return e.errorMessage
}

// These definitions need to be the same as the TEN_ERROR_CODE enum in C.
//
// Note: To achieve the best compatibility, any new enum item, should be added
// to the end to avoid changing the value of previous enum items.
const (
	// ErrorCodeGeneric is the default errno, for those users only care error
	// msgs.
	ErrorCodeGeneric = 1

	// ErrorCodeInvalidJSON means the json data is invalid.
	ErrorCodeInvalidJSON = 2

	// ErrorCodeInvalidArgument means invalid parameter.
	ErrorCodeInvalidArgument = 3

	// ErrorCodeInvalidType means invalid type.
	ErrorCodeInvalidType = 4

	// ErrorCodeInvalidGraph means invalid graph.
	ErrorCodeInvalidGraph = 5

	// ErrorCodeTenIsClosed means the TEN world is closed.
	ErrorCodeTenIsClosed = 6

	// ErrorCodeMsgNotConnected means the msg is not connected in the graph.
	ErrorCodeMsgNotConnected = 7
)
