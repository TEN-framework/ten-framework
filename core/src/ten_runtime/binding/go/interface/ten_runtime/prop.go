//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten

// #include <stdbool.h>
// #include "common.h"
import "C"

import (
	"fmt"
	"reflect"
	"unsafe"
)

// propType is an alias of TEN_TYPE from TEN runtime.
type propType uint8

const (
	propTypeInvalid propType = iota

	propTypeNull

	propTypeBool

	propTypeInt8
	propTypeInt16
	propTypeInt32
	propTypeInt64

	propTypeUint8
	propTypeUint16
	propTypeUint32
	propTypeUint64

	propTypeFloat32
	propTypeFloat64

	propTypeStr
	propTypeBuf

	// propTypeArray and propTypeObject are not supported in golang binding.
	propTypeArray
	propTypeObject

	propTypePtr

	// Some special types which are not in TEN runtime.

	// propTypeInt represents a GO int, which is at least 32 bits in size. It
	// will be treated as propTypeInt64 when communicating with TEN runtime. The
	// reason we do not use propTypeInt64 directly is that types must match
	// exactly in the interface conversion. Ex:
	//
	//   var a any = 1   // the type of 'a' is interface{}|int.
	//   b := a.(int64)  // panic: interface conversion
	//   b := a.(int)    // correct.
	propTypeInt

	// propTypeUint represents a GO uint, which is at least 32 bits in size. It
	// will be treated as propTypeUint64 when communicating with TEN runtime.
	propTypeUint
)

func getPropType(pt any) propType {
	if pt == nil {
		return propTypeNull
	}

	switch pt.(type) {
	case bool:
		return propTypeBool
	case int8:
		return propTypeInt8
	case int16:
		return propTypeInt16
	case int32:
		return propTypeInt32
	case int64:
		return propTypeInt64
	case int:
		return propTypeInt
	case uint8:
		return propTypeUint8
	case uint16:
		return propTypeUint16
	case uint32:
		return propTypeUint32
	case uint64:
		return propTypeUint64
	case uint:
		return propTypeUint
	case float32:
		return propTypeFloat32
	case float64:
		return propTypeFloat64
	case string:
		return propTypeStr
	case []byte:
		return propTypeBuf
	default:
		k := reflect.TypeOf(pt).Kind()
		if k == reflect.Struct || k == reflect.Array || k == reflect.Map ||
			k == reflect.Slice {
			// The struct/array/slice/map is not supported, it should be
			// marshalled first.
			return propTypeInvalid
		}

		if k == reflect.Chan {
			// The chan is a special type in golang which is always used for
			// safe communication between goroutines, it's only used in some
			// specific cases in golang. So we do not use it as a property.
			return propTypeInvalid
		}

		if k == reflect.Pointer || k == reflect.Func || k == reflect.UnsafePointer {
			// We do not need to check the generic type (using
			// reflect.TypeOf(pt).Elem()) if the type is Pointer, as we only
			// need to know the property stored in TEN runtime is a pointer, no
			// matter what type of the pointer it is.
			return propTypePtr
		}

		return propTypeInvalid
	}
}

func (pt propType) isTypeSupported() error {
	if pt == propTypeNull || pt == propTypeInvalid || pt == propTypeArray ||
		pt == propTypeObject {
		return newTenError(
			ErrorCodeInvalidType,
			fmt.Sprintf(
				"the Struct/Array/Slice/Map/Chan are not supported, actually is [%d]",
				pt,
			),
		)
	}

	return nil
}

func (pt propType) String() string {
	switch pt {
	case propTypeInvalid:
		return "invalid"
	case propTypeBool:
		return "bool"
	case propTypeInt8:
		return "int8"
	case propTypeInt16:
		return "int16"
	case propTypeInt32:
		return "int32"
	case propTypeInt64:
		return "int64"
	case propTypeInt:
		return "int"
	case propTypeUint8:
		return "uint8"
	case propTypeUint16:
		return "uint16"
	case propTypeUint32:
		return "uint32"
	case propTypeUint64:
		return "uint64"
	case propTypeUint:
		return "uint"
	case propTypeFloat32:
		return "float32"
	case propTypeFloat64:
		return "float64"
	case propTypePtr:
		return "Pointer"
	case propTypeStr:
		return "string"
	case propTypeArray:
		return "array"
	case propTypeObject:
		return "struct"
	case propTypeBuf:
		return "[]byte"
	case propTypeNull:
		return "nil"
	default:
		return "unsupported"
	}
}

// iProperty defines the property getting and setting functions. Those functions
// are exported to users.
type iProperty interface {
	// SetProperty sets a property with the given path and value. The type of
	// value is inferred automatically.
	SetProperty(path string, value any) error

	// SetPropertyString sets a string as a property. The reason we define a
	// concrete method for string is that the value type of SetProperty is
	// `any`, there is a type conversion in passing a string variable to
	// SetProperty. The conversion calls `runtime.convTstring` that allocates a
	// new unsafe.Pointer.
	// Ex:
	//
	//   s := "ss"
	//   msg.SetProperty("a", s)  // runtime.convTstring will be called.
	SetPropertyString(path string, value string) error

	// SetPropertyBytes sets a []byte as a property. The reason we define a
	// concrete method for []byte is that the value type of SetProperty is
	// `any`, there is a type conversion in passing a []byte variable to
	// SetProperty. The conversion calls `runtime.convTslice` that allocates a
	// new unsafe.Pointer.
	SetPropertyBytes(path string, value []byte) error

	// SetPropertyFromJSONBytes treats the given []byte as a json string, and
	// the type of the newly created properties is json object but not []byte.
	// Which means the property can only be retrieved by GetPropertyToJSONBytes.
	SetPropertyFromJSONBytes(path string, value []byte) error

	GetPropertyInt8(path string) (int8, error)

	GetPropertyInt16(path string) (int16, error)

	GetPropertyInt32(path string) (int32, error)

	GetPropertyInt64(path string) (int64, error)

	GetPropertyUint8(path string) (uint8, error)

	GetPropertyUint16(path string) (uint16, error)

	GetPropertyUint32(path string) (uint32, error)

	GetPropertyUint64(path string) (uint64, error)

	GetPropertyFloat32(path string) (float32, error)

	GetPropertyFloat64(path string) (float64, error)

	GetPropertyBool(path string) (bool, error)

	GetPropertyPtr(path string) (any, error)

	// GetPropertyString gets a string property by the given path. The reason we
	// define a concrete method for string is that the returned type of
	// GetProperty is `any`, there is a type conversion in returning an `any`
	// object as string. The conversion calls `runtime.convTstring` that
	// allocates a new unsafe.Pointer.
	GetPropertyString(path string) (string, error)

	// GetPropertyBytes gets a []byte property by the given path. The reason we
	// define a concrete method for []byte is that the returned type of
	// GetProperty is `any`, there is a type conversion in returning an `any`
	// object as []byte. The conversion calls `runtime.convTslice` that
	// allocates a new unsafe.Pointer.
	GetPropertyBytes(path string) ([]byte, error)

	// GetPropertyToJSONBytes gets a property which is a json data store in TEN
	// runtime. If the property exists, the json data will be marshalled to a
	// json bytes.
	GetPropertyToJSONBytes(path string) ([]byte, error)
}

// The purpose of having this function is because there are two types of
// getProperty:
//
// 1. ten.getProperty
// 2. msg.getProperty
//
// Therefore, a closure is used to abstract the way of getting values, sharing
// the logic of this function.
func getPropInt8(retrieve func(*C.int8_t) C.ten_go_error_t) (int8, error) {
	var cv C.int8_t
	apiStatus := retrieve(&cv)
	if err := withCGoError(&apiStatus); err != nil {
		return 0, err
	}

	return int8(cv), nil
}

func getPropInt16(retrieve func(*C.int16_t) C.ten_go_error_t) (int16, error) {
	var cv C.int16_t
	apiStatus := retrieve(&cv)
	if err := withCGoError(&apiStatus); err != nil {
		return 0, err
	}

	return int16(cv), nil
}

func getPropInt32(retrieve func(*C.int32_t) C.ten_go_error_t) (int32, error) {
	var cv C.int32_t
	apiStatus := retrieve(&cv)
	if err := withCGoError(&apiStatus); err != nil {
		return 0, err
	}

	return int32(cv), nil
}

func getPropInt64(retrieve func(*C.int64_t) C.ten_go_error_t) (int64, error) {
	var cv C.int64_t
	apiStatus := retrieve(&cv)
	if err := withCGoError(&apiStatus); err != nil {
		return 0, err
	}

	return int64(cv), nil
}

func getPropUint8(retrieve func(*C.uint8_t) C.ten_go_error_t) (uint8, error) {
	var cv C.uint8_t
	apiStatus := retrieve(&cv)
	if err := withCGoError(&apiStatus); err != nil {
		return 0, err
	}

	return uint8(cv), nil
}

func getPropUint16(
	retrieve func(*C.uint16_t) C.ten_go_error_t,
) (uint16, error) {
	var cv C.uint16_t
	apiStatus := retrieve(&cv)
	if err := withCGoError(&apiStatus); err != nil {
		return 0, err
	}

	return uint16(cv), nil
}

func getPropUint32(
	retrieve func(*C.uint32_t) C.ten_go_error_t,
) (uint32, error) {
	var cv C.uint32_t
	apiStatus := retrieve(&cv)
	if err := withCGoError(&apiStatus); err != nil {
		return 0, err
	}

	return uint32(cv), nil
}

func getPropUint64(
	retrieve func(*C.uint64_t) C.ten_go_error_t,
) (uint64, error) {
	var cv C.uint64_t
	apiStatus := retrieve(&cv)
	if err := withCGoError(&apiStatus); err != nil {
		return 0, err
	}

	return uint64(cv), nil
}

func getPropBool(retrieve func(*C.bool) C.ten_go_error_t) (bool, error) {
	var cv C.bool
	apiStatus := retrieve(&cv)
	if err := withCGoError(&apiStatus); err != nil {
		return false, err
	}

	return bool(cv), nil
}

func getPropFloat32(
	retrieve func(*C.float) C.ten_go_error_t,
) (float32, error) {
	var cv C.float
	apiStatus := retrieve(&cv)
	if err := withCGoError(&apiStatus); err != nil {
		return 0, err
	}

	return float32(cv), nil
}

func getPropFloat64(
	retrieve func(*C.double) C.ten_go_error_t,
) (float64, error) {
	var cv C.double
	apiStatus := retrieve(&cv)
	if err := withCGoError(&apiStatus); err != nil {
		return 0, err
	}

	return float64(cv), nil
}

func getPropStr(
	size uintptr,
	retrieve func(unsafe.Pointer) C.ten_go_error_t,
) (string, error) {
	if size == 0 {
		panic("Should not happen.")
	}

	// About the size of the slice:
	//
	// The size passed to this function is the result of `strlen` in C, which is
	// the length of the string _without_ the null terminator.
	//
	// The definition of the string in GO is:
	//
	//  type stringStruct struct {
	//  	str unsafe.Pointer
	//  	len int
	//  }
	//
	// The generic type of the `str` field is `*byte`. Unlike C strings, GO
	// strings have no space for a null terminator. And a GO string can be
	// constructed using a []byte, in two ways as follows. Imagine that there is
	// a field named `bytes` which type is []byte.
	//
	//   - string(bytes)
	//   - unsafe.String(bytes, len(bytes))
	//
	// And there is no memory allocation if a GO string is constructed using the
	// second way, in other words, the `bytes` will be used as the underlying
	// buffer of the newly created string. That's also what we will use in this
	// function. Therefore, the length of the []byte which will be used to store
	// the C string, must be `size`, but not be `size + 1`.

	// We have to make a slice whose capacity is equal to its length, in other
	// words, do not pass a third parameter to the make function. Ex:
	//
	//   Expected:
	//     buf := make([]byte, size)     // The length and cap are equal.
	//
	//   Not expected:
	//     buf := make([]byte, 0, size)  // It means the length is 0.
	//
	// As the slice will be written in the C world, using `strcpy`, and the
	// length of the slice won't be increased after `strcpy`, which is
	// inconsistent with the behavior of the `append` function in GO. However,
	// for most functions in GO, only the [0, length] of the slice can be read.
	// So if the slice is created with length 0, nothing can be read from it in
	// GO after `strcpy`.
	//
	// We do not use acquireBytes to retrieve a slice from the bytes pool here,
	// as the slice is used as the underlying buffer of the returned string
	// directly. Golang does not allow developers to customize the memory
	// reclamation of go strings. The garbage collector (GC) of the go runtime
	// automatically reclaims the memory of go strings that are no longer in
	// use. As a result, the memory allocated this time is essentially given to
	// the go string, and developers do not need to manually call ReleaseBytes
	// to reclaim memory. Therefore, there's no need to use acquireBytes, and we
	// can simply use make.
	buf := make([]byte, size)
	apiStatus := retrieve(unsafe.Pointer(&buf[0]))
	if err := withCGoError(&apiStatus); err != nil {
		return "", err
	}

	// The `buf` is always not empty, it's safe to get the address using
	// `&buf[0]`. A more general way is to call unsafe.SliceData.
	return unsafe.String(&buf[0], len(buf)), nil
}

func getPropBytes(
	size uintptr,
	retrieve func(unsafe.Pointer) C.ten_go_error_t,
) ([]byte, error) {
	if size == 0 {
		panic("Should not happen.")
	}

	buf := acquireBytes(int(size))

	// Refer to the comments in getPropStr.
	buf = buf[:size]
	apiStatus := retrieve(unsafe.Pointer(&buf[0]))
	if err := withCGoError(&apiStatus); err != nil {
		ReleaseBytes(buf)
		return nil, err
	}

	return buf, nil
}

func getPropPtr(
	retrieve func(*cHandle) C.ten_go_error_t,
) (any, error) {
	var cv cHandle
	apiStatus := retrieve(&cv)
	if err := withCGoError(&apiStatus); err != nil {
		return nil, err
	}

	return loadGoHandle(goHandle(cv)), nil
}
