//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten

import (
	"testing"
)

// The benchmark is:
// goos: linux
// goarch: amd64
// cpu: Intel(R) Core(TM) i7-10510U CPU @ 1.80GHz
// -1     2710520     543.0 ns/op      141 B/op        1 allocs/op
// -2     3605497     530.7 ns/op      142 B/op        1 allocs/op
// -4     3225613     402.1 ns/op       20 B/op        1 allocs/op
// -8     1000000     1374 ns/op       934 B/op        1 allocs/op
func BenchmarkGoHandleNewAndLoad(b *testing.B) {
	cb := func(msg any) {
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		handle := newGoHandle(cb)
		_ = loadGoHandle(handle)
	}
	b.ReportAllocs()
}

// The benchmark is:
// goos: linux
// goarch: amd64
// cpu: Intel(R) Core(TM) i7-10510U CPU @ 1.80GHz
// -1    10968644     91.83 ns/op        0 B/op        0 allocs/op
// -2    13182796     88.11 ns/op        0 B/op        0 allocs/op
// -4    13787869     90.62 ns/op        0 B/op        0 allocs/op
// -8    14344474     87.11 ns/op        0 B/op        0 allocs/op
func BenchmarkGoHandleNewAndDelete(b *testing.B) {
	cb := func(msg any) {
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		handle := newGoHandle(cb)
		v := loadAndDeleteGoHandle(handle)

		if v == nil {
			b.FailNow()
		}
	}
	b.ReportAllocs()
}

func TestGoHandleNewAndLoad(t *testing.T) {
	cb := func() {}

	for i := 0; i < 10; i++ {
		handle := newGoHandle(cb)
		v := loadGoHandle(handle)

		if v == nil {
			t.FailNow()
		}
	}
}
