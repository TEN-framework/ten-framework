//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten

import "sync"

// We want to provide a recycle pool for bytes to reduce memory allocation, as
// the []byte (the requests are always structured data, which will be marshalled
// into []byte as the property of a ten msg) will be used frequently in the
// backend services.
//
// However, the size of each request is not fixed, so we provide more than one
// pool, and each pool has a boundary size. If someone acquires a []byte with an
// expected size:
//
// * We will try to find a pool with a boundary size that is larger than the
// expected size and closest to the expected size.
//
// * The []byte will be retrieved from the pool if found.
//
// * Otherwise, a new []byte with the expected size will be allocated directly.
// So every time a memory space is taken from the bytes pool, its size is one of
// the fixed boundary sizes. Therefore, when returning the memory space to the
// bytes pool, a simple verification is performed. It checks if the size of the
// memory space being returned is one of those boundary sizes.
//
// In general, there are no statistics on the distribution trend of the request
// size. We provide the following five boundary sizes based on the test cases in
// bytes_test.go. We define a simple struct as follows.
//
//	type payload struct {
//		MsgId     string `json:"msg_id"`
//		OpenId    string `json:"openid"`
//		Content   string `json:"content"`
//		AvatarUrl string `json:"avatar_url"`
//		Nickname  string `json:"nickname"`
//		Timestamp int64  `json:"timestamp"`
//	}
//
//	type liveMessage struct {
//		RoomId   string    `json:"room_id"`
//		AppId    string    `json:"app_id"`
//		Payloads []payload `json:"payloads"`
//	}
//
// And we calculate the size of the marshalled liveMessage with different number
// of payloads. The result is as follows:
//
//   - If the number of payloads is 1, the size is 267.
//   - If the number of payloads is 2, the size is 455.
//   - If the number of payloads is 10, the size is 1974.
//   - If the number of payloads is 20, the size is 3861.
//
// TODO(Liu): add a observation of the distribution trend of the request size.
// And we can adjust the boundary size according to the observation.
// Furthermore, we can provide a mechanism similar to the PGO optimization.
const (
	defaultBoundaryTiny   = 128
	defaultBoundarySmall  = 512
	defaultBoundaryMiddle = 1024
	defaultBoundaryLarge  = 2048
	defaultBoundaryHuge   = 4096
)

var (
	bytesPoolTiny = sync.Pool{
		New: func() any {
			return make([]byte, 0, defaultBoundaryTiny)
		},
	}

	bytesPoolSmall = sync.Pool{
		New: func() any {
			return make([]byte, 0, defaultBoundarySmall)
		},
	}

	bytesPoolMiddle = sync.Pool{
		New: func() any {
			return make([]byte, 0, defaultBoundaryMiddle)
		},
	}

	bytesPoolLarge = sync.Pool{
		New: func() any {
			return make([]byte, 0, defaultBoundaryLarge)
		},
	}

	bytesPoolHuge = sync.Pool{
		New: func() any {
			return make([]byte, 0, defaultBoundaryHuge)
		},
	}
)

func findPoolInClosestBoundary(size int) *sync.Pool {
	if size <= defaultBoundaryTiny {
		return &bytesPoolTiny
	} else if size <= defaultBoundarySmall {
		return &bytesPoolSmall
	} else if size <= defaultBoundaryMiddle {
		return &bytesPoolMiddle
	} else if size <= defaultBoundaryLarge {
		return &bytesPoolLarge
	} else if size <= defaultBoundaryHuge {
		return &bytesPoolHuge
	} else {
		return nil
	}
}

func findPoolEqualBoundary(size int) *sync.Pool {
	if size == defaultBoundaryTiny {
		return &bytesPoolTiny
	} else if size == defaultBoundarySmall {
		return &bytesPoolSmall
	} else if size == defaultBoundaryMiddle {
		return &bytesPoolMiddle
	} else if size == defaultBoundaryLarge {
		return &bytesPoolLarge
	} else if size == defaultBoundaryHuge {
		return &bytesPoolHuge
	} else {
		return nil
	}
}

// acquireBytes tries to allocate a []byte from the pool.
func acquireBytes(size int) []byte {
	p := findPoolInClosestBoundary(size)
	if p == nil {
		return make([]byte, 0, size)
	}

	return p.Get().([]byte)
}

// This function is willing to be called by users after the []byte is useless,
// so it is exported.

// ReleaseBytes tries to recycle the []byte to reduce memory allocation.
func ReleaseBytes(b []byte) {
	p := findPoolEqualBoundary(cap(b))
	if p == nil {
		return
	}

	b = b[:0]

	// TODO(Liu): there is a memory allocation due to `runtime.convTslice`, fix.
	// This matter is unrelated to cgo; it purely involves the use of the `Any`
	// type in Golang, where the Go compiler might perform actions involving
	// memory allocation (convT).
	p.Put(b)
}
