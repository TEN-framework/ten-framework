//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten

import (
	"fmt"
	"runtime"
	"sync"
)

type baseTenObject[T any] struct {
	// The object is short-lived, it becomes invalid after sending or returning
	// to TEN runtime. Ex: there's nothing can do on a cmd after someone calls
	// SendCmd(). It's the user's responsibility to make sure the calling
	// sequences on one baseTenObject are correct. And we will do some check in
	// the "dev" mode (i.e., build with flags: -tags "dev"), this lock is only
	// used to implement the check, you can NOT use it in anywhere else to
	// prevent deadlock.
	//
	// All methods of baseTenObject shall be surrounded with `escapeCheck()`,
	// refer to base_dev.go and base_release.go.
	//
	// There is another behavior in golang when some struct has an embedded
	// sync.RWMutex, the struct shall be non-copied. The structs implement
	// Lock() and Unlock() interface will be non-copied. Ex:
	//
	// b1 := baseTenObject[*C.X] {}
	// b2 := b1  // Incorrect!
	//
	// However, golang only provides a command line tool to check those invalid
	// operations -- go vet. But the tool is offline, there is no way to check
	// during runtime.
	//
	// In golang's codes, there is another way to check if some struct has been
	// copied. Refer to `copyCheck` in Builder:
	//
	//	func (b *Builder) copyCheck() {
	//	  if b.addr == nil {
	//	    b.addr = (*Builder)(noescape(unsafe.Pointer(b)))
	//    } else if b.addr != b {
	//	    panic("strings: illegal use of non-zero Builder copied by value")
	//    }
	//  }
	//
	// For now, all user cases of baseTenObject are pointer, and we only provide
	// interfaces to the users. So we do not need to add such `copyCheck` now.
	sync.RWMutex

	// The pointer of the C bridge object used to communicate with the TEN
	// runtime.
	cPtr T

	// The handle of self, used to callback from the C part.
	goObjID handle

	// Make cgo functions always called on limited goroutines.
	*pool
}

func (p *baseTenObject[T]) free() {
	p.goObjID.free()
}

// In a GO function, keepAlive ensures that the `p` is alive (i.e., is not being
// finalized by the garbage collector) before the location of call site of
// keepAlive. This function is always used to mark the last point where the
// object must be reached.
//
// The baseTenObject is used as a bridge between Go and C, and its field
// `cPtr` will always be passed as a parameter in cgo calls. Ex, the code in
// `msg::getProperty`:
//
//	func (p *msg) getProperty(path string) (v, error) {
//	  C.ten_go_msg_get_property(p.cPtr, path)
//	}
//
// From the GC's (i.e., garbage collector) perspective, the `p` is unreachable
// before the `C.ten_go_msg_get_property` even through its field `p.cPtr` stays
// alive as a parameter in the cgo call. As the GC only cares about the
// reference in the GO world, it can not see insight into the underlying layer
// (any syscall). The `p.cPtr` in the parameter is not a reference to `p` from
// the GC's view. So the `p` might be marked as "dead" before calling
// `C.ten_go_msg_get_property`.
//
// The `p` might or might not be recycled by the garbage collector, it depends
// on the behavior of the GC workers. The GC workers recycle the unreachable
// heap memory based on the GOGC, a ratio of incremental heap memory in each GC
// cycle. As the cgo call will invoke `gopark` which will trigger one round of
// scheduler, a new GC cycle might be started. The GC workers run in the
// different goroutines which have a higher scheduling priority. It means the
// following cases might be happened when calling cgo:
//
//   - The finalizer has been executed, which causes the `p` becomes to nil.
//     Then "invalid memory address or nil pointer dereference" happens in
//     accessing `p.cPtr`.
//
//   - The finalizer and the cgo call run in parallel, the native memory of
//     `p.cPtr` might be invalid when the underlying C codes execute. It's an
//     undefined behavior.
//
// To prevent the above cases, golang provides the following method to keep a
// reference alive:
//
//	runtime.KeepAlive()
//
// The simplified usage is calling `runtime.KeepAlive()` in the end(ex: using
// defer).
//
// Refer to:
//
//   - https://github.com/golang/go/issues/21402#issuecomment-321860922
//
//   - https://groups.google.com/g/golang-nuts/c/PbeVtMpkjVk?pli=1&from_wecom=1
func (p *baseTenObject[T]) keepAlive() {
	runtime.KeepAlive(p)
}

// String dumps the baseTenObject. We prefer to use `String` as the method name,
// which will be treated as implementing the Stringer interface. It's not a
// standard, but that's what golang runtime does in its codes.
func (p *baseTenObject[T]) String() string {
	return fmt.Sprintf("rteBridge handle: %d", p.goObjID)
}
