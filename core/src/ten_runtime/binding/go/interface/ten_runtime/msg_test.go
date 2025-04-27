//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten

import (
	"encoding/json"
	"testing"
	"unsafe"
)

type testDemo struct {
	A string `json:"a"`
}

func TestMsgGetSetPropAllType(t *testing.T) {
	c, err := NewCmd("test")
	if err != nil {
		t.FailNow()
	}

	var v1 int8 = 1
	err = c.SetProperty("k1", v1)
	if err != nil {
		t.FailNow()
	}

	if vv, err := c.GetPropertyInt8("k1"); err != nil {
		t.FailNow()
	} else {
		if vv != 1 {
			t.FailNow()
		}
	}

	var v2 int16 = 2
	err = c.SetProperty("k2", v2)
	if err != nil {
		t.FailNow()
	}

	if vv, err := c.GetPropertyInt16("k2"); err != nil {
		t.FailNow()
	} else {
		if vv != 2 {
			t.FailNow()
		}
	}

	var v3 int32 = 3
	err = c.SetProperty("k3", v3)
	if err != nil {
		t.FailNow()
	}

	if vv, err := c.GetPropertyInt32("k3"); err != nil {
		t.FailNow()
	} else {
		if vv != 3 {
			t.FailNow()
		}
	}

	var v4 int64 = 4
	err = c.SetProperty("k4", v4)
	if err != nil {
		t.FailNow()
	}

	if vv, err := c.GetPropertyInt64("k4"); err != nil {
		t.FailNow()
	} else {
		if vv != 4 {
			t.FailNow()
		}
	}

	var v5 uint8 = 5
	err = c.SetProperty("k5", v5)
	if err != nil {
		t.FailNow()
	}

	if vv, err := c.GetPropertyUint8("k5"); err != nil {
		t.FailNow()
	} else {
		if vv != 5 {
			t.FailNow()
		}
	}

	var v6 uint16 = 6
	err = c.SetProperty("k6", v6)
	if err != nil {
		t.FailNow()
	}

	if vv, err := c.GetPropertyUint16("k6"); err != nil {
		t.FailNow()
	} else {
		if vv != 6 {
			t.FailNow()
		}
	}

	var v7 uint32 = 7
	err = c.SetProperty("k7", v7)
	if err != nil {
		t.FailNow()
	}

	if vv, err := c.GetPropertyUint32("k7"); err != nil {
		t.FailNow()
	} else {
		if vv != 7 {
			t.FailNow()
		}
	}

	var v8 uint64 = 8
	err = c.SetProperty("k8", v8)
	if err != nil {
		t.FailNow()
	}

	if vv, err := c.GetPropertyUint64("k8"); err != nil {
		t.FailNow()
	} else {
		if vv != 8 {
			t.FailNow()
		}
	}

	var v9 float32 = 9.0
	err = c.SetProperty("k9", v9)
	if err != nil {
		t.FailNow()
	}

	if vv, err := c.GetPropertyFloat32("k9"); err != nil {
		t.FailNow()
	} else {
		if vv != 9.0 {
			t.FailNow()
		}
	}

	var v10 float64 = 10.0
	err = c.SetProperty("k10", v10)
	if err != nil {
		t.FailNow()
	}

	if vv, err := c.GetPropertyFloat64("k10"); err != nil {
		t.FailNow()
	} else {
		if vv != 10.0 {
			t.FailNow()
		}
	}

	var v11 string = "11"
	err = c.SetProperty("k11", v11)
	if err != nil {
		t.FailNow()
	}

	if vv, err := c.GetPropertyString("k11"); err != nil {
		t.FailNow()
	} else {
		if vv != "11" {
			t.FailNow()
		}
	}

	err = c.SetProperty("k12", true)
	if err != nil {
		t.FailNow()
	}

	if vv, err := c.GetPropertyBool("k12"); err != nil {
		t.FailNow()
	} else {
		if !vv {
			t.FailNow()
		}
	}

	v13 := []byte{1, 2, 3}
	err = c.SetProperty("k13", v13)
	if err != nil {
		t.FailNow()
	}

	if vv, err := c.GetPropertyBytes("k13"); err != nil {
		t.FailNow()
	} else {
		if len(vv) != 3 {
			t.FailNow()
		}
	}

	v14 := &testDemo{A: "14"}
	err = c.SetProperty("k14", v14)
	if err != nil {
		t.FailNow()
	}

	if vv, err := c.GetPropertyPtr("k14"); err != nil {
		t.FailNow()
	} else {
		vi, ok := vv.(*testDemo)
		if !ok {
			t.FailNow()
		}

		if vi.A != "14" {
			t.FailNow()
		}
	}

	// int
	err = c.SetProperty("k15", 15)
	if err != nil {
		t.FailNow()
	}

	// The type shall be int64.
	if vv, err := c.GetPropertyInt64("k15"); err != nil {
		t.FailNow()
	} else {
		if vv != 15 {
			t.FailNow()
		}
	}
}

func TestMsgSetAndGetPropJSON(t *testing.T) {
	c, _ := NewCmd("test")

	demo := &testDemo{
		A: "aa",
	}
	bytes, _ := json.Marshal(demo)

	err := c.SetPropertyFromJSONBytes("k1", bytes)
	if err != nil {
		t.FailNow()
	}

	if vv, err := c.GetPropertyToJSONBytes("k1"); err != nil {
		t.FailNow()
	} else {
		var d2 testDemo
		err = json.Unmarshal(vv, &d2)
		if err != nil {
			t.FailNow()
		}

		if d2.A != "aa" {
			t.FailNow()
		}

		vs := unsafe.String(unsafe.SliceData(vv), len(vv))
		if vs != "{\"a\": \"aa\"}" {
			t.FailNow()
		}
	}
}

// The benchmark is:
//
// goos: linux
// goarch: amd64
// cpu: Intel(R) Core(TM) i7-10510U CPU @ 1.80GHz
//
// -8  221768	      5327 ns/op	      40 B/op	       5 allocs/op
func BenchmarkMsgGetPropInt8(b *testing.B) {
	c, err := NewCmd("test")
	if err != nil {
		b.FailNow()
	}

	var v int8 = 1
	err = c.SetProperty("k1", v)
	if err != nil {
		b.FailNow()
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		if vv, err := c.GetPropertyInt8("k1"); err != nil {
			b.FailNow()
		} else {
			if vv != 1 {
				b.FailNow()
			}
		}
	}
	b.ReportAllocs()
}

// The benchmark is:
//
// goos: linux
// goarch: amd64
// cpu: Intel(R) Core(TM) i7-10510U CPU @ 1.80GHz
//
// -8   	  509209	      2332 ns/op	       0 B/op	       0 allocs/op
func BenchmarkMsgSetPropBytes(b *testing.B) {
	c, err := NewCmd("test")
	if err != nil {
		b.FailNow()
	}

	prop := []byte{1, 2, 3}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		err := c.SetPropertyBytes("k", prop)
		if err != nil {
			b.FailNow()
		}
	}
	b.ReportAllocs()
}

// The benchmark is:
//
// goos: linux
// goarch: amd64
// cpu: Intel(R) Core(TM) i7-10510U CPU @ 1.80GHz
//
// -8   	  438103	      2293 ns/op	       0 B/op	       0 allocs/op
func BenchmarkMsgSetPropString(b *testing.B) {
	c, err := NewCmd("test")
	if err != nil {
		b.FailNow()
	}

	prop := "hello"

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		err := c.SetPropertyString("k", prop)
		if err != nil {
			b.FailNow()
		}
	}
	b.ReportAllocs()
}

// The benchmark is:
//
// goos: linux
// goarch: amd64
// cpu: Intel(R) Core(TM) i7-10510U CPU @ 1.80GHz
//
// -8   	  302902	      3897 ns/op	      40 B/op	       3 allocs/op
func BenchmarkMsgGetPropJSON(b *testing.B) {
	c, _ := NewCmd("test")

	demo := &testDemo{
		A: "aa",
	}
	bytes, _ := json.Marshal(demo)

	err := c.SetPropertyFromJSONBytes("k1", bytes)
	if err != nil {
		b.FailNow()
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		if vv, err := c.GetPropertyToJSONBytes("k1"); err != nil {
			b.FailNow()
		} else {
			ReleaseBytes(vv)
		}
	}
	b.ReportAllocs()
}
