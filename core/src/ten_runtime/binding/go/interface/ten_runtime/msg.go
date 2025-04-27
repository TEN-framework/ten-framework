//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten

// #include "msg.h"
import "C"

import (
	"runtime"
	"unsafe"
)

// MsgType is an alias of `TEN_MSG_TYPE` from TEN runtime.
type MsgType uint8

type (
	propTypeInC = C.uint8_t
	propSizeInC = C.uintptr_t
)

// Use const variables to represent C enum values.
const (
	MsgTypeCmdInvalid MsgType = iota
	MsgTypeCmdCloseApp
	MsgTypeCmdCloseEngine
	MsgTypeCmdConnect
	MsgTypeCmdTimer
	MsgTypeCmdTimeout
	MsgTypeCmdCustom
	MsgTypeCmdResult
	MsgTypeCmdData
	MsgTypeCmdVideoFrame
	MsgTypeCmdAudioFrame
)

// Msg is the base type from which all other message types are derived.
type Msg interface {
	getCPtr() C.uintptr_t
	free()
	keepAlive()

	GetName() (string, error)
	SetDest(
		appUri string,
		graphId string,
		extensionGroup string,
		extension string,
	) error

	iProperty
}

type msg struct {
	// The cPtr in msg is actually a pointer to the C struct ten_go_msg_t.
	//
	// We prefer to keep the cPtr as C.uintptr_t which is an integer holds the
	// bit pattern of the pointer to the C struct ten_go_msg_t. The reason is as
	// follows.
	//
	// - The C.uintptr_t is big enough to hold the bit pattern of a pointer. So
	//   it's safe to convert between an uintptr_t and *ten_go_msg_t in the C
	//   world.
	//
	// - The C.uintptr_t from C and the uintptr from GO are completely equal.
	//   They can be converted to each other without any loss or memory
	//   allocation.
	//
	// - The C.uintptr_t is an integer, not a pointer, it's safe (without
	//   breaking any cgo rules) and efficient (without any memory allocation)
	//   to pass it between C and GO using cgo.
	//
	// 	 - It's safe because the ten_go_msg_t pointer is allocated in the C
	//     world, and the address won't be changed during the whole lifetime of
	//     the msg.
	//     So it's safe to only keep the bit pattern of the pointer in the GO
	//     world, and reinterpret it as a pointer in the C world.
	//
	//     The cgo compiler only checks pointers passed from GO to C, and the
	//     C.uintptr_t is an integer, so passing it between GO and C won't break
	//     any cgo rules.
	//
	//   - It's efficient because there is memory allocation if passing
	//     *C.ten_go_msg_t between GO and C. Refer to the chapter
	//     "Incomplete type" in README.md. There is no memory allocation if
	//     passing a C.uintptr_t between GO and C.
	//
	// - All accesses to the ten_go_msg_t pointer are always happened in the C
	//   world. No arithmetic will be performed on cPtr in the GO world even
	//   though it is an integer. It will not be converted to an unsafe.Pointer
	//   in the GO world. The cPtr is only kept as C.uintptr_t in the GO world,
	//   and passed to the C world. No more operations will be performed on it.
	baseTenObject[C.uintptr_t]
}

// newMsg constructs a msg.
//
// We have to pass the cPtr as the constructor parameter, as we call
// runtime.SetFinalizer which will access the cPtr, and the Finalizer might be
// called after the constructor if we do not use the created msg.
func newMsg(bridge C.uintptr_t) *msg {
	if bridge == 0 {
		// Should not happen.
		panic("The bridge in the msg constructor is nil.")
	}

	m := &msg{}

	m.cPtr = bridge
	m.pool = &globalPool

	// According to the comments on SetFinalizer, the behavior of Finalizer is
	// as follows.
	//
	// - When GC finds the `msg` is unreachable, it runs the Finalizer in a
	//   separate goroutine. Do _NOT_ make any reference to the `msg` in the
	//   Finalizer, the Finalizer shall be the last access point to the `msg`
	//   object.
	//
	// - The `msg` object is not freed once the Finalizer is completed, but in
	//   the next GC cycle.
	//
	// - All Finalizer runs in one single goroutine. Therefore, firstly, do not
	//   block the finalizer goroutine, start a new goroutine if needed.
	//   Secondly, if there's any access to the fields in `msg`, it's unsafe
	//   unless the fields are immutable or using mutex. In Go Memory Model,
	//   there is no guarantee that the Finalizer happens after all other
	//   operations (ex: runtime.KeepAlive(msg)), and there is only guaranteed
	//   that the `SetFinalizer(msg, f)` always happens before the Finalizer
	//   function.
	//
	// - Calling SetFinalizer is expensive.
	//
	// The Finalizer associated with the `msg` is used to free the cPtr when the
	// `msg` is recycled from the GO side. The `msg` in the GO side comes from
	// the following two ways.
	//
	// - Created by GO extensions (this extension acts as a producer), which is
	//   intended to be sent to another extension with SendXXX(). The `msg`
	//   shall be out-of-scope (any function call to the `msg` is invalid) after
	//   it is sent out.
	//
	//   + In the normal case, one `msg` created in the extension shall be sent
	//     out. As the SendXXX already makes a cgo call, then we can free the
	//     `cPtr` from the c-bridge to reduce one SetFinalizer and one cgo call.
	//
	//   + However, someone might not call SendXXX() after the `msg` is created.
	//     Thus in this case, the SetFinalizer must be called to recycle the
	//     `cPtr`, otherwise the memory will be leaked.
	//
	// - Retrieve from some other extension with onXXX(), and the extension acts
	//   as a consumer. The msg maybe sent out with SendXXX(), or returned back
	//   with ReturnXXX(). And same as the above case, the msg shall be
	//   out-of-scope after calling SendXXX() or ReturnXXX().
	//
	//   + In this case, the SetFinalizer shall always be called, as someone
	//     might do nothing in onXXX(), we have to use the Finalizer to free
	//     `cPtr`.
	//
	// That's why we have to call SetFinalizer on msg here.
	//
	// TODO(Liu): add a msg pool using sync.Pool to reduce calling SetFinalizer
	//  as it's expensive, that's also what golang Pinner does. Refer to Pin in
	//  pinner.go.
	runtime.SetFinalizer(m, func(p *msg) {
		// Please keep in mind that it's unsafe to convert an uintptr (which is
		// equal to C.uintptr_t) to an unsafe.Pointer. Reinterpret the
		// C.uintptr_t to the C pointer in the C world.
		C.ten_go_msg_finalize(p.cPtr)
	})

	return m
}

// This is used to check if the Msg struct implements the msg interface. Note
// that no object is created.
var (
	_ Msg = new(msg)
)

// TODO(Liu): remove. It's not recommended to provide setter/getter for the
// private field, just access the field directly.
func (p *msg) getCPtr() C.uintptr_t {
	return p.cPtr
}

func (p *msg) GetName() (string, error) {
	defer p.keepAlive()

	var msgName *C.char
	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_msg_get_name(p.cPtr, &msgName)
		return withCGoError(&apiStatus)
	})

	if err != nil {
		return "", err
	}

	return C.GoString(msgName), nil
}

func (p *msg) SetDest(
	appUri string,
	graphId string,
	extensionGroup string,
	extension string,
) error {
	defer p.keepAlive()

	err := withCGOLimiter(func() error {
		apiStatus := C.ten_go_msg_set_dest(
			p.cPtr,
			unsafe.Pointer(unsafe.StringData(appUri)),
			C.int(len(appUri)),
			unsafe.Pointer(unsafe.StringData(graphId)),
			C.int(len(graphId)),
			unsafe.Pointer(unsafe.StringData(extensionGroup)),
			C.int(len(extensionGroup)),
			unsafe.Pointer(unsafe.StringData(extension)),
			C.int(len(extension)),
		)

		return withCGoError(&apiStatus)
	})

	return err
}
