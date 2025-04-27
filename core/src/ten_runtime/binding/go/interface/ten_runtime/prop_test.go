//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten

import (
	"errors"
	"testing"
)

type testProp struct{}

func TestGetPropType(t *testing.T) {
	t1 := getPropType(nil)
	if t1 != propTypeNull {
		t.FailNow()
	}

	var v2 uint8
	t2 := getPropType(v2)
	if t2 != propTypeUint8 {
		t.FailNow()
	}

	var v3 testProp
	t3 := getPropType(v3)
	if t3 != propTypeInvalid {
		t.FailNow()
	}

	t4 := getPropType(&v3)
	if t4 != propTypePtr {
		t.FailNow()
	}

	var v5 any = v3
	t5 := getPropType(v5)
	if t5 != propTypeInvalid {
		t.FailNow()
	}

	var v6 any = &v3
	t6 := getPropType(v6)
	if t6 != propTypePtr {
		t.FailNow()
	}
}

func TestPropertyTypeMismatch(t *testing.T) {
	c, err := NewCmd("test")
	if err != nil {
		t.FailNow()
	}

	var v1 uint64 = 1
	if err = c.SetProperty("k", v1); err != nil {
		t.FailNow()
	}

	if v, err := c.GetPropertyPtr("k"); err == nil || v != nil {
		t.FailNow()
	} else {
		var e *TenError
		if errors.As(err, &e) {
			// Error from runtime.
			if e.ErrorCode() != 1 {
				t.FailNow()
			}
		} else {
			t.FailNow()
		}
	}
}

func TestPropertyTypeMismatch2(t *testing.T) {
	c, err := NewCmd("test")
	if err != nil {
		t.FailNow()
	}

	if err = c.SetProperty("k", 1); err != nil {
		t.FailNow()
	}

	if v, err := c.GetPropertyString("k"); err == nil || len(v) > 0 {
		t.FailNow()
	} else {
		var e *TenError
		if errors.As(err, &e) {
			if e.ErrorCode() != ErrorCodeInvalidType {
				t.FailNow()
			}
		} else {
			t.FailNow()
		}
	}
}

// The benchmark is:
// goos: linux
// goarch: amd64
// cpu: Intel(R) Core(TM) i7-10510U CPU @ 1.80GHz
//
// -8   	  256129	      4308 ns/op	      16 B/op	       3 allocs/op
func BenchmarkGetPropertyString(b *testing.B) {
	c, err := NewCmd("test")
	if err != nil {
		b.FailNow()
	}

	var v string = "v"
	if err = c.SetProperty("k", v); err != nil {
		b.FailNow()
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		if _, err = c.GetPropertyString("k"); err != nil {
			b.FailNow()
		}
	}
	b.ReportAllocs()
}

// The benchmark is:
// goos: linux
// goarch: amd64
// cpu: Intel(R) Core(TM) i7-10510U CPU @ 1.80GHz
//
// -8   	  254371	      4677 ns/op	      40 B/op	       3 allocs/op
func BenchmarkGetPropertyBytes(b *testing.B) {
	c, err := NewCmd("test")
	if err != nil {
		b.FailNow()
	}

	var v []byte = []byte("v")
	if err = c.SetProperty("k", v); err != nil {
		b.FailNow()
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		if prop, err := c.GetPropertyBytes("k"); err != nil {
			b.FailNow()
		} else {
			ReleaseBytes(prop)
		}
	}
	b.ReportAllocs()
}
