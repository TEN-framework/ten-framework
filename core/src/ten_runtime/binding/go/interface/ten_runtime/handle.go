//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten

// #include <stdint.h>
import "C"

import (
	"sync"
	"sync/atomic"
)

// {@
// TODO(Liu):
// 1. Using goHandleType instead of handle to improve performance.
// 2. Remove those codes after the handle is useless.

type handle uintptr

var (
	goHandles = NewConcurrentMap[any]()
	handleIdx uintptr
)

func newhandle(obj any) handle {
	id := atomic.AddUintptr(&handleIdx, 1)

	goHandles.Set(id, obj)
	return handle(id)
}

func (id handle) get() any {
	v, ok := goHandles.Get(uintptr(id))
	if !ok {
		return nil
	}

	return v
}

func (id handle) free() any {
	v, ok := goHandles.Pop(uintptr(id))
	if !ok {
		return nil
	}

	return v
}

// @}

// The logic surrounding goHandle is largely a mimicry of the
// runtime/cgo/handle.go found in the golang source code.
//
// The reasons we do not use cgo.Handle directly are as follows.
//
// 1. We do not want to have a race condition with Golang runtime.
//
// 2. Golang uses `sync.Map` as the storage for cgo.Handle(s), which does not
// have a good performance on deletion operations.

// The mechanism of go handle essentially involves storing the go object in an
// array within GO, and then simply passing the array index to the C world.
// Therefore, goHandle is an integer value that can represent any object in GO
// world. When passing an object from GO to C, if no special handling is done
// (such as pinning that GO object), the cgo runtime check will fail and the
// application will panic. Using goHandle to pass those objects between GO and C
// is a safe way, without breaking the cgo check rules.
//
// In TEN world, a goHandle may be one of the following.
//
//   - ten, extension.
//   - a GO pointer which is set into a msg or ten as a property.
//   - a callback function, ex: the result handler of SendCmd.
//
// A goHandle can _NOT_ be one of the following.
//
//   - The replacement of the uintptr type in function definition.
type goHandle = uintptr

// cHandle is the C type corresponding to the goHandle when passing a goHandle
// from GO to C. goHandle and cHandle can be converted into each other.
type cHandle = C.uintptr_t

// concurrentMap is a goroutine-safety map.
//
// 1. Use a sync.RWMutex in conjunction with a standard Go map, rather than
// employing a sync.Map. This is primarily driven by our specific operational
// requirements, which involve a high frequency of concurrent store operations
// and load-and-delete actions, and sync.Map does not have a good performance on
// deletion operations.
//
// 2. We do not use buckets (each bucket has a RWMutex and map) to reduce
// the competitions of acquiring R/W lock concurrently, as the major
// contributors to performance degradation are not the contention for read/write
// locks. Instead, the performance bottlenecks are located within the map
// operations and the handling of goHandleType.
//
// Note: the golang map does not provide a behavior to reuse its capacity, when
// we delete an item from the map (calling its delete function), the
// corresponding capacity will be shrink. This will lead to re-expansion when we
// add a new item. However, the expansion is slow.
//
// The newGoHandle will create a new uintptr based on the value of
// currentGoHandle, the assignment will be slow if there are too many
// concurrent store operations, so we use a sync.Pool to recycle the uintptr.
type concurrentMap struct {
	sync.RWMutex
	items map[goHandle]any
}

func newConcurrentMap() *concurrentMap {
	m := &concurrentMap{
		items: make(map[goHandle]any),
	}

	return m
}

func (m *concurrentMap) store(k goHandle, v any) {
	defer m.Unlock()
	m.Lock()

	m.items[k] = v
}

func (m *concurrentMap) load(k goHandle) (any, bool) {
	defer m.RUnlock()
	m.RLock()

	v, ok := m.items[k]
	return v, ok
}

func (m *concurrentMap) loadAndDelete(k goHandle) (any, bool) {
	defer m.Unlock()
	m.Lock()

	v, ok := m.items[k]
	if ok {
		delete(m.items, k)
		return v, true
	}

	return nil, false
}

// immutableHandles store those objects which are immutable once they are
// created, ex: ten, extension. And the number of those objects should be very
// small.
//
// For each item in the map, the key type is goHandleType, and the value type is
// baseTenObject.
var immutableHandles sync.Map

// handles stores those objects which will be passed from Go to C, the reason we
// do not use sync.Map is that sync.Map does not have a good performance in
// deletion operations.
var handles = newConcurrentMap()

var (
	// goHandleNil represents the golang pointer passed to C is nil, it's a
	// default key. Do not call newGoHandle here, as we do not want to store it.
	goHandleNil = goHandle(0)

	currentGoHandle        atomic.Uintptr
	currentImmutableHandle atomic.Uintptr

	// The frequency of add and delete operations in 'handles' map is almost the
	// same, in other words, the items in handles map are often removed out. So
	// we can recycle the goHandleType (i.e., the keys in 'handles') to reduce
	// the competitions on 'currentGoHandle'.
	goHandleCache = sync.Pool{
		New: func() any {
			id := currentGoHandle.Add(1)
			return id
		},
	}
)

func newGoHandle(obj any) goHandle {
	id := goHandleCache.Get().(goHandle)
	handles.store(id, obj)

	return id
}

func loadGoHandle(handle goHandle) any {
	v, ok := handles.load(handle)
	if ok {
		return v
	}

	return nil
}

func loadAndDeleteGoHandle(handle goHandle) any {
	v, ok := handles.loadAndDelete(handle)
	if ok {
		goHandleCache.Put(handle)

		return v
	}

	return nil
}

func newImmutableHandle(obj any) goHandle {
	id := currentImmutableHandle.Add(1)
	immutableHandles.Store(id, obj)
	return id
}

func loadImmutableHandle(handle goHandle) any {
	v, ok := immutableHandles.Load(handle)
	if ok {
		return v
	}

	return nil
}

func loadAndDeleteImmutableHandle(handle goHandle) any {
	v, ok := immutableHandles.LoadAndDelete(handle)
	if ok {
		return v
	}

	return nil
}

func clearImmutableHandles(preAction func(value any)) {
	immutableHandles.Range(func(key, value any) bool {
		if preAction != nil {
			preAction(value)
		}

		immutableHandles.Delete(key)
		return true
	})
}

//export tenUnpinGoPointer
func tenUnpinGoPointer(id C.uintptr_t) {
	gid := goHandle(id)
	if gid <= 0 {
		return
	}

	_ = loadAndDeleteGoHandle(gid)
}
