//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten

// #include "cmd.h"
import "C"

import (
	"unsafe"
)

// CmdBase is the base interface for the command and the result command.
type CmdBase interface {
	Msg
}

// Cmd is the interface for the command.
type Cmd interface {
	CmdBase

	Clone() (Cmd, error)
}

// NewCmd creates a custom cmd which is intended to be sent to another
// extension using tenEnv.SendCmd().
func NewCmd(cmdName string) (Cmd, error) {
	if len(cmdName) == 0 {
		return nil, newTenError(
			ErrorCodeInvalidArgument,
			"cmd name is required.",
		)
	}

	var bridge C.uintptr_t
	err := withCGOLimiter(func() error {
		// We do not pass the GO string (i.e., cmdName) to the C world in the
		// normal way -- using the following codes:
		//
		//  cString := C.CString(cmdName)
		//  defer C.free(unsafe.Pointer(cString))
		//
		// The above codes create the C string in the GO world with two cgo
		// calls.
		//
		// We prefer to pass the address of the underlying buffer of the GO
		// string to the C world instead, to improve performance. In this way,
		// no memory is allocated and two cgo calls are reduced.
		//
		// In Go, as per the language specification, a string is effectively a
		// read-only slice of bytes. Once a string is created, its value cannot
		// be modified, ensuring immutability. Refer to:
		//
		//  - https://go.dev/ref/spec#String_types
		//  - https://go.dev/blog/strings
		//
		// The function `unsafe.StringData()` returns the underlying slice of
		// the GO string, and a slice is a piece of an array (with fixed size).
		// As the GO string is readonly, the return value of
		// `unsafe.StringData()` equals an array. So it's safe to pass the
		// address of the underlying slice to the C world, as it's immutable.
		// Please keep in mind that, since Go strings are immutable, the bytes
		// returned by StringData must not be modified. And do not access it in
		// an async way in the C world.
		//
		// And also, as `unsafe.StringData()` returns a slice which memory is
		// sequential, so it's safe to read the memory based on the address and
		// length (i.e., the first and second parameter of
		// ten_go_cmd_create_cmd) in the C world.
		//
		// And golang uses `unsafe.StringData()` in its codes too. Ex, in
		// `src/runtime/heapdump.go`:
		//
		//  func dumpstr(s string) {
		// 	  dumpmemrange(
		//	    unsafe.Pointer(unsafe.StringData(s)),
		//      uintptr(len(s)),
		//    )
		//  }
		//
		// which dumpmemrange makes a syscall.
		//
		// Note:
		//
		// - Do not call `unsafe.StringData` on an empty string, the return
		//   value is unspecified.
		//
		// - Only read operation is permitted in the C world.
		cStatus := C.ten_go_cmd_create_cmd(
			unsafe.Pointer(unsafe.StringData(cmdName)),
			C.int(len(cmdName)),
			&bridge,
		)
		e := withCGoError(&cStatus)

		return e
	})
	if err != nil {
		return nil, err
	}

	return newCmd(bridge), nil
}

type cmd struct {
	*msg
}

func newCmd(bridge C.uintptr_t) *cmd {
	return &cmd{
		msg: newMsg(bridge),
	}
}

func (p *cmd) Clone() (Cmd, error) {
	var bridge C.uintptr_t
	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_cmd_clone(
			p.getCPtr(),
			&bridge,
		)
		return withCGoError(&apiStatus)
	})

	if err != nil {
		return nil, err
	}

	if bridge == 0 {
		// Should not happen.
		return nil, newTenError(
			ErrorCodeInvalidArgument,
			"bridge is nil",
		)
	}

	return newCmd(bridge), nil
}

var _ Cmd = new(cmd)
