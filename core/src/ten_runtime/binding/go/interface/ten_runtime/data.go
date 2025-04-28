//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten

// #include "msg.h"
// #include "data.h"
import "C"

import (
	"unsafe"
)

// Data is the interface for the TEN data message.
type Data interface {
	Msg

	Clone() (Data, error)
	AllocBuf(size int) error
	LockBuf() ([]byte, error)
	UnlockBuf(buf *[]byte) error
	GetBuf() ([]byte, error)
}

type data struct {
	*msg
}

func newData(bridge C.uintptr_t) *data {
	return &data{msg: newMsg(bridge)}
}

var _ Data = new(data)

// NewData creates a new data message.
func NewData(dataName string) (Data, error) {
	if len(dataName) == 0 {
		return nil, newTenError(
			ErrorCodeInvalidArgument,
			"data name is required.",
		)
	}

	var bridge C.uintptr_t
	err := withCGOLimiter(func() error {
		cStatus := C.ten_go_data_create(
			unsafe.Pointer(unsafe.StringData(dataName)),
			C.int(len(dataName)),
			&bridge,
		)
		e := withCGoError(&cStatus)

		return e
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

	return newData(bridge), nil
}

func (p *data) AllocBuf(size int) error {
	if size <= 0 {
		return newTenError(ErrorCodeInvalidArgument, "the size should be > 0")
	}

	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_data_alloc_buf(p.getCPtr(), C.int(size))
		return withCGoError(&apiStatus)
	})

	return err
}

func (p *data) LockBuf() ([]byte, error) {
	var bufAddr *C.uint8_t
	var bufSize C.uint64_t

	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_data_lock_buf(
			p.getCPtr(),
			&bufAddr,
			&bufSize,
		)

		return withCGoError(&apiStatus)
	})

	if err != nil {
		return nil, err
	}

	// This means the len and cap of this `buf` are equal.
	buf := unsafe.Slice((*byte)(unsafe.Pointer(bufAddr)), int(bufSize))

	return buf, nil
}

func (p *data) UnlockBuf(buf *[]byte) error {
	if buf == nil {
		return newTenError(ErrorCodeInvalidArgument, "buf is nil")
	}

	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_data_unlock_buf(
			p.getCPtr(),
			unsafe.Pointer(unsafe.SliceData(*buf)),
		)

		return withCGoError(&apiStatus)
	})

	if err == nil {
		// The buf can not be used anymore after giving back.
		*buf = nil
	}

	return err
}

func (p *data) GetBuf() ([]byte, error) {
	var bufSize C.uint64_t

	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_data_get_buf_size(p.getCPtr(), &bufSize)
		return withCGoError(&apiStatus)
	})

	if err != nil {
		return nil, err
	}

	if bufSize == 0 {
		return make([]byte, 0), nil
	}

	buf := make([]byte, bufSize)
	err = withCGOLimiter(func() error {
		apiStatus := C.ten_go_data_get_buf(
			p.getCPtr(),
			unsafe.Pointer(&buf[0]),
			bufSize,
		)

		return withCGoError(&apiStatus)
	})

	if err != nil {
		return nil, err
	}

	return buf, nil
}

func (p *data) Clone() (Data, error) {
	var bridge C.uintptr_t
	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_data_clone(p.getCPtr(), &bridge)
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

	return newData(bridge), nil
}
