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
	"fmt"
	"unsafe"
)

func (p *msg) getPropertyTypeAndSize(
	path string,
	size *propSizeInC,
) (propType, error) {
	defer p.keepAlive()

	var ptInC propTypeInC

	// There is memory allocation in passing `cPtr` (the first parameter) to C.
	// The asm code is as follows.
	//
	//	 CALL runtime.convT64(SB)
	//	 MOVQ AX, BX
	//	 XORL CX, CX
	//	 XORL DI, DI
	//	 LEAQ type:eYXH5Xgq(SB), AX
	//	 CALL local.runtime.cgoCheckPointer(SB)
	//
	// The first line converts the `cPtr` to an unsafe.Pointer using
	// `runtime.convT64`, there may be memory allocation if the address bits of
	// `cPtr` is bigger than `0xff`. Refer to the chapter `Incomplete type` in
	// README.md.
	apiStatus := C.ten_go_msg_property_get_type_and_size(
		p.cPtr,
		unsafe.Pointer(unsafe.StringData(path)),
		C.int(len(path)),
		&ptInC,
		size,
	)
	err := withCGoError(&apiStatus)
	if err != nil {
		return propTypeInvalid, err
	}

	return propType(ptInC), nil
}

func (p *msg) getPropertyString(path string) (string, error) {
	var pSize propSizeInC = 0
	realPt, err := p.getPropertyTypeAndSize(path, &pSize)
	if err != nil {
		return "", err
	}

	if realPt != propTypeStr {
		return "", newTenError(
			ErrorCodeInvalidType,
			fmt.Sprintf("expected: string, actual: %s", realPt),
		)
	}

	if pSize == 0 {
		// We can not allocate a []byte with size 0, so just return "".
		return "", nil
	}

	return getPropStr(
		uintptr(pSize),
		func(v unsafe.Pointer) C.ten_go_error_t {
			return C.ten_go_msg_property_get_string(
				p.cPtr,
				unsafe.Pointer(unsafe.StringData(path)),
				C.int(len(path)),
				v,
			)
		},
	)
}

// GetPropertyString retrieves the string property stored in a msg. This
// function has one less memory allocation than calling GetProperty.
//
// Any type conversions to or from the `any` type may lead the Go compiler to
// generate functions like runtime.convT64, runtime.convTslice, or similar
// functions to perform the conversions. These runtime.convTxxx functions might
// lead to memory allocations. The return type of GetProperty is any, so this
// function with a return type that is not any is created to avoid the memory
// allocations associated with type conversions involving the `any` type.
func (p *msg) GetPropertyString(path string) (string, error) {
	if len(path) == 0 {
		return "", newTenError(
			ErrorCodeInvalidArgument,
			"property path is required.",
		)
	}

	defer p.keepAlive()

	return withCGOLimiterHasReturnValue[string](func() (string, error) {
		return p.getPropertyString(path)
	})
}

func (p *msg) getPropertyBytes(path string) ([]byte, error) {
	var pSize propSizeInC = 0
	realPt, err := p.getPropertyTypeAndSize(path, &pSize)
	if err != nil {
		return nil, err
	}

	if realPt != propTypeBuf {
		return nil, newTenError(
			ErrorCodeInvalidType,
			fmt.Sprintf("expected: []byte, actual: %s", realPt),
		)
	}

	if pSize == 0 {
		// We can not allocate a []byte with size 0, so just return nil.
		return nil, nil
	}

	return getPropBytes(
		uintptr(pSize),
		func(v unsafe.Pointer) C.ten_go_error_t {
			return C.ten_go_msg_property_get_buf(
				p.cPtr,
				unsafe.Pointer(unsafe.StringData(path)),
				C.int(len(path)),
				v,
			)
		},
	)
}

// GetPropertyBytes retrieves the []byte property stored in a msg. This function
// has one less memory allocation than calling GetProperty.
//
// Any type conversions to or from the `any` type may lead the Go compiler to
// generate functions like runtime.convT64, runtime.convTslice, or similar
// functions to perform the conversions. These runtime.convTxxx functions might
// lead to memory allocations. The return type of GetProperty is any, so this
// function with a return type that is not any is created to avoid the memory
// allocations associated with type conversions involving the `any` type.
func (p *msg) GetPropertyBytes(path string) ([]byte, error) {
	if len(path) == 0 {
		return nil, newTenError(
			ErrorCodeInvalidArgument,
			"property path is required.",
		)
	}

	defer p.keepAlive()

	return withCGOLimiterHasReturnValue[[]byte](func() ([]byte, error) {
		return p.getPropertyBytes(path)
	})
}

func (p *msg) GetPropertyInt8(path string) (int8, error) {
	if len(path) == 0 {
		return 0, newTenError(
			ErrorCodeInvalidArgument,
			"property path is required",
		)
	}

	return withCGOLimiterHasReturnValue[int8](func() (int8, error) {
		return getPropInt8(func(v *C.int8_t) C.ten_go_error_t {
			defer p.keepAlive()
			return C.ten_go_msg_property_get_int8(
				p.cPtr,
				unsafe.Pointer(unsafe.StringData(path)),
				C.int(len(path)),
				v,
			)
		})
	})
}

func (p *msg) GetPropertyInt16(path string) (int16, error) {
	if len(path) == 0 {
		return 0, newTenError(
			ErrorCodeInvalidArgument,
			"property path is required",
		)
	}

	return withCGOLimiterHasReturnValue[int16](func() (int16, error) {
		return getPropInt16(func(v *C.int16_t) C.ten_go_error_t {
			defer p.keepAlive()
			return C.ten_go_msg_property_get_int16(
				p.cPtr,
				unsafe.Pointer(unsafe.StringData(path)),
				C.int(len(path)),
				v,
			)
		})
	})
}

func (p *msg) GetPropertyInt32(path string) (int32, error) {
	if len(path) == 0 {
		return 0, newTenError(
			ErrorCodeInvalidArgument,
			"property path is required",
		)
	}

	return withCGOLimiterHasReturnValue[int32](func() (int32, error) {
		return getPropInt32(func(v *C.int32_t) C.ten_go_error_t {
			defer p.keepAlive()
			return C.ten_go_msg_property_get_int32(
				p.cPtr,
				unsafe.Pointer(unsafe.StringData(path)),
				C.int(len(path)),
				v,
			)
		})
	})
}

func (p *msg) GetPropertyInt64(path string) (int64, error) {
	if len(path) == 0 {
		return 0, newTenError(
			ErrorCodeInvalidArgument,
			"property path is required",
		)
	}

	return withCGOLimiterHasReturnValue[int64](func() (int64, error) {
		return getPropInt64(func(v *C.int64_t) C.ten_go_error_t {
			defer p.keepAlive()
			return C.ten_go_msg_property_get_int64(
				p.cPtr,
				unsafe.Pointer(unsafe.StringData(path)),
				C.int(len(path)),
				v,
			)
		})
	})
}

func (p *msg) GetPropertyUint8(path string) (uint8, error) {
	if len(path) == 0 {
		return 0, newTenError(
			ErrorCodeInvalidArgument,
			"property path is required",
		)
	}

	return withCGOLimiterHasReturnValue[uint8](func() (uint8, error) {
		return getPropUint8(func(v *C.uint8_t) C.ten_go_error_t {
			defer p.keepAlive()
			return C.ten_go_msg_property_get_uint8(
				p.cPtr,
				unsafe.Pointer(unsafe.StringData(path)),
				C.int(len(path)),
				v,
			)
		})
	})

}

func (p *msg) GetPropertyUint16(path string) (uint16, error) {
	if len(path) == 0 {
		return 0, newTenError(
			ErrorCodeInvalidArgument,
			"property path is required",
		)
	}

	return withCGOLimiterHasReturnValue[uint16](func() (uint16, error) {
		return getPropUint16(func(v *C.uint16_t) C.ten_go_error_t {
			defer p.keepAlive()
			return C.ten_go_msg_property_get_uint16(
				p.cPtr,
				unsafe.Pointer(unsafe.StringData(path)),
				C.int(len(path)),
				v,
			)
		})
	})
}

func (p *msg) GetPropertyUint32(path string) (uint32, error) {
	if len(path) == 0 {
		return 0, newTenError(
			ErrorCodeInvalidArgument,
			"property path is required",
		)
	}

	return withCGOLimiterHasReturnValue[uint32](func() (uint32, error) {
		return getPropUint32(func(v *C.uint32_t) C.ten_go_error_t {
			defer p.keepAlive()
			return C.ten_go_msg_property_get_uint32(
				p.cPtr,
				unsafe.Pointer(unsafe.StringData(path)),
				C.int(len(path)),
				v,
			)
		})
	})
}

func (p *msg) GetPropertyUint64(path string) (uint64, error) {
	if len(path) == 0 {
		return 0, newTenError(
			ErrorCodeInvalidArgument,
			"property path is required",
		)
	}

	return withCGOLimiterHasReturnValue[uint64](func() (uint64, error) {
		return getPropUint64(func(v *C.uint64_t) C.ten_go_error_t {
			defer p.keepAlive()
			return C.ten_go_msg_property_get_uint64(
				p.cPtr,
				unsafe.Pointer(unsafe.StringData(path)),
				C.int(len(path)),
				v,
			)
		})
	})

}

func (p *msg) GetPropertyFloat32(path string) (float32, error) {
	if len(path) == 0 {
		return 0, newTenError(
			ErrorCodeInvalidArgument,
			"property path is required",
		)
	}

	return withCGOLimiterHasReturnValue[float32](func() (float32, error) {
		return getPropFloat32(func(v *C.float) C.ten_go_error_t {
			defer p.keepAlive()
			return C.ten_go_msg_property_get_float32(
				p.cPtr,
				unsafe.Pointer(unsafe.StringData(path)),
				C.int(len(path)),
				v,
			)
		})
	})
}

func (p *msg) GetPropertyFloat64(path string) (float64, error) {
	if len(path) == 0 {
		return 0, newTenError(
			ErrorCodeInvalidArgument,
			"property path is required",
		)
	}

	return withCGOLimiterHasReturnValue[float64](func() (float64, error) {
		return getPropFloat64(func(v *C.double) C.ten_go_error_t {
			defer p.keepAlive()
			return C.ten_go_msg_property_get_float64(
				p.cPtr,
				unsafe.Pointer(unsafe.StringData(path)),
				C.int(len(path)),
				v,
			)
		})
	})
}

func (p *msg) GetPropertyBool(path string) (bool, error) {
	if len(path) == 0 {
		return false, newTenError(
			ErrorCodeInvalidArgument,
			"property path is required",
		)
	}

	return withCGOLimiterHasReturnValue[bool](func() (bool, error) {
		return getPropBool(func(v *C.bool) C.ten_go_error_t {
			defer p.keepAlive()
			return C.ten_go_msg_property_get_bool(
				p.cPtr,
				unsafe.Pointer(unsafe.StringData(path)),
				C.int(len(path)),
				v,
			)
		})
	})
}

func (p *msg) GetPropertyPtr(path string) (any, error) {
	if len(path) == 0 {
		return nil, newTenError(
			ErrorCodeInvalidArgument,
			"property path is required",
		)
	}

	return withCGOLimiterHasReturnValue[any](func() (any, error) {
		return getPropPtr(func(v *cHandle) C.ten_go_error_t {
			defer p.keepAlive()
			return C.ten_go_msg_property_get_ptr(
				p.cPtr,
				unsafe.Pointer(unsafe.StringData(path)),
				C.int(len(path)),
				v,
			)
		})
	})

}

func (p *msg) setPropertyString(path string, value string) error {
	apiStatus := C.ten_go_msg_property_set_string(
		p.cPtr,
		unsafe.Pointer(unsafe.StringData(path)),
		C.int(len(path)),
		unsafe.Pointer(unsafe.StringData(value)),
		C.int(len(value)),
	)
	err := withCGoError(&apiStatus)
	return err
}

// SetPropertyString sets a string as property in the msg. This function has one
// less memory allocation than calling SetProperty.
func (p *msg) SetPropertyString(path string, value string) error {
	if len(path) == 0 {
		return newTenError(
			ErrorCodeInvalidArgument,
			"property path is required.",
		)
	}

	defer p.keepAlive()

	return withCGOLimiter(func() error {
		return p.setPropertyString(path, value)
	})
}

func (p *msg) setPropertyBytes(path string, value []byte) error {
	// TEN runtime can not malloc memory with size 0.
	if len(value) == 0 {
		return newTenError(
			ErrorCodeInvalidArgument,
			"property value is required",
		)
	}

	apiStatus := C.ten_go_msg_property_set_buf(
		p.cPtr,
		unsafe.Pointer(unsafe.StringData(path)),
		C.int(len(path)),
		// Using either `unsafe.SliceData()` or `&value[0]` works fine. And
		// `unsafe.SliceData()` reduces one memory allocation, while
		// `runtime.convTslice` will be called if using `&value[0]`.
		unsafe.Pointer(unsafe.SliceData(value)),
		C.int(len(value)),
	)
	err := withCGoError(&apiStatus)
	return err
}

// SetPropertyBytes sets a []byte as property in the msg. This function has one
// less memory allocation than calling SetProperty.
func (p *msg) SetPropertyBytes(path string, value []byte) error {
	if len(path) == 0 || len(value) == 0 {
		return newTenError(
			ErrorCodeInvalidArgument,
			"property path and value are required.",
		)
	}

	defer p.keepAlive()

	return withCGOLimiter(func() error {
		return p.setPropertyBytes(path, value)
	})
}

func (p *msg) setProperty(path string, value any) error {
	pt := getPropType(value)
	if err := pt.isTypeSupported(); err != nil {
		return err
	}

	var err error
	switch pt {
	case propTypeBool:
		apiStatus := C.ten_go_msg_property_set_bool(
			p.cPtr,
			unsafe.Pointer(unsafe.StringData(path)),
			C.int(len(path)),
			C.bool(value.(bool)),
		)
		err = withCGoError(&apiStatus)

	case propTypeInt8:
		apiStatus := C.ten_go_msg_property_set_int8(
			p.cPtr,
			unsafe.Pointer(unsafe.StringData(path)),
			C.int(len(path)),
			C.int8_t(value.(int8)),
		)
		err = withCGoError(&apiStatus)

	case propTypeInt16:
		apiStatus := C.ten_go_msg_property_set_int16(
			p.cPtr,
			unsafe.Pointer(unsafe.StringData(path)),
			C.int(len(path)),
			C.int16_t(value.(int16)),
		)
		err = withCGoError(&apiStatus)

	case propTypeInt32:
		apiStatus := C.ten_go_msg_property_set_int32(
			p.cPtr,
			unsafe.Pointer(unsafe.StringData(path)),
			C.int(len(path)),
			C.int32_t(value.(int32)),
		)
		err = withCGoError(&apiStatus)

	case propTypeInt64:
		apiStatus := C.ten_go_msg_property_set_int64(
			p.cPtr,
			unsafe.Pointer(unsafe.StringData(path)),
			C.int(len(path)),
			C.int64_t(value.(int64)),
		)
		err = withCGoError(&apiStatus)

	case propTypeInt:
		if is64bit {
			apiStatus := C.ten_go_msg_property_set_int64(
				p.cPtr,
				unsafe.Pointer(unsafe.StringData(path)),
				C.int(len(path)),
				C.int64_t(value.(int)),
			)
			err = withCGoError(&apiStatus)
		} else {
			apiStatus := C.ten_go_msg_property_set_int32(
				p.cPtr,
				unsafe.Pointer(unsafe.StringData(path)),
				C.int(len(path)),
				C.int32_t(value.(int)),
			)
			err = withCGoError(&apiStatus)
		}

	case propTypeUint8:
		apiStatus := C.ten_go_msg_property_set_uint8(
			p.cPtr,
			unsafe.Pointer(unsafe.StringData(path)),
			C.int(len(path)),
			C.uint8_t(value.(uint8)),
		)
		err = withCGoError(&apiStatus)

	case propTypeUint16:
		apiStatus := C.ten_go_msg_property_set_uint16(
			p.cPtr,
			unsafe.Pointer(unsafe.StringData(path)),
			C.int(len(path)),
			C.uint16_t(value.(uint16)),
		)
		err = withCGoError(&apiStatus)

	case propTypeUint32:
		apiStatus := C.ten_go_msg_property_set_uint32(
			p.cPtr,
			unsafe.Pointer(unsafe.StringData(path)),
			C.int(len(path)),
			C.uint32_t(value.(uint32)),
		)
		err = withCGoError(&apiStatus)

	case propTypeUint64:
		apiStatus := C.ten_go_msg_property_set_uint64(
			p.cPtr,
			unsafe.Pointer(unsafe.StringData(path)),
			C.int(len(path)),
			C.uint64_t(value.(uint64)),
		)
		err = withCGoError(&apiStatus)

	case propTypeUint:
		if is64bit {
			apiStatus := C.ten_go_msg_property_set_uint64(
				p.cPtr,
				unsafe.Pointer(unsafe.StringData(path)),
				C.int(len(path)),
				C.uint64_t(value.(uint)),
			)
			err = withCGoError(&apiStatus)
		} else {
			apiStatus := C.ten_go_msg_property_set_uint32(
				p.cPtr,
				unsafe.Pointer(unsafe.StringData(path)),
				C.int(len(path)),
				C.uint32_t(value.(uint)),
			)
			err = withCGoError(&apiStatus)
		}

	case propTypeFloat32:
		apiStatus := C.ten_go_msg_property_set_float32(
			p.cPtr,
			unsafe.Pointer(unsafe.StringData(path)),
			C.int(len(path)),
			C.float(value.(float32)),
		)
		err = withCGoError(&apiStatus)

	case propTypeFloat64:
		apiStatus := C.ten_go_msg_property_set_float64(
			p.cPtr,
			unsafe.Pointer(unsafe.StringData(path)),
			C.int(len(path)),
			C.double(value.(float64)),
		)
		err = withCGoError(&apiStatus)

	case propTypeStr:
		vs := value.(string)
		err = p.setPropertyString(path, vs)

	case propTypeBuf:
		vb := value.([]byte)
		err = p.setPropertyBytes(path, vb)

	case propTypePtr:
		vh := newGoHandle(value)
		apiStatus := C.ten_go_msg_property_set_ptr(
			p.cPtr,
			unsafe.Pointer(unsafe.StringData(path)),
			C.int(len(path)),
			cHandle(vh),
		)
		err = withCGoError(&apiStatus)

	default:
		// Should not happen.
		panic("Should not happen.")
	}

	return err
}

// SetProperty sets the value as a property in the msg. Note that there will be
// a type conversion which causes memory allocation if the type of value is
// string or []byte. If the performance is critical, calling the concrete method
// SetPropertyString or SetPropertyBytes is more appropriate.
func (p *msg) SetProperty(path string, value any) error {
	if len(path) == 0 {
		return newTenError(
			ErrorCodeInvalidArgument,
			"the property path is required.",
		)
	}

	defer p.keepAlive()

	return withCGOLimiter(func() error {
		return p.setProperty(path, value)
	})
}

func (p *msg) setPropertyFromJSONBytes(path string, value []byte) error {
	defer p.keepAlive()

	apiStatus := C.ten_go_msg_property_set_json_bytes(
		p.cPtr,
		unsafe.Pointer(unsafe.StringData(path)),
		C.int(len(path)),
		unsafe.Pointer(unsafe.SliceData(value)),
		C.int(len(value)),
	)
	err := withCGoError(&apiStatus)
	return err
}

// SetPropertyFromJSONBytes sets a json data as a property in the msg. The
// `value` must be a valid json data. The json data will be treated as an object
// or array in TEN runtime, but not a slice. The usual practice is to use
// GetPropertyToJSONBytes to extract everything at once. However, if the
// structure is already known beforehand through certain methods, GetProperty
// can be used to retrieve individual fields.
func (p *msg) SetPropertyFromJSONBytes(path string, value []byte) error {
	return withCGOLimiter(func() error {
		return p.setPropertyFromJSONBytes(path, value)
	})
}

func (p *msg) getPropertyToJSONBytes(path string) ([]byte, error) {
	defer p.keepAlive()

	var cJSONStr *C.char
	var cJSONStrLen C.uintptr_t
	apiStatus := C.ten_go_msg_property_get_json_and_size(
		p.cPtr,
		unsafe.Pointer(unsafe.StringData(path)),
		C.int(len(path)),
		&cJSONStrLen,
		&cJSONStr,
	)
	err := withCGoError(&apiStatus)
	if err != nil {
		return nil, err
	}

	size := uintptr(cJSONStrLen)
	if size == 0 {
		// The size of any valid json could not be 0.
		return nil, newTenError(
			ErrorCodeInvalidJSON,
			"the size of json data is 0",
		)
	}

	// First, the slice can not be created by the following code, as the
	// `cJSONStr` is allocated in C world, which is not managed by the GO GC.
	// The underlying buffer of the slice must be pointed to the GO heap.
	//
	// unsafe.Slice(cJSONStr, uintptr(cJSONStrLen))
	//
	// Second, the cgo provides the following function to convert a C pointer to
	// a GO slice.
	//
	// C.GoBytes(unsafe.Pointer(cJSONStr), C.int(cJSONStrLen))
	//
	// Refer to runtime/string.go in golang source code for the implementation
	// of `C.GoBytes`.
	//
	// However, we do not want to use this function, the reason is as follows.
	//
	// - A new slice is always allocated in the `C.GOBytes`, it means we can not
	//   use the bytes pool to reduce memory allocation.
	//
	// - The syscall `runtime.memmove` will be called to copy the C memory to
	//   the slice. However, the `runtime.memmove` may be slower than glibc on
	//   some hardware, refer to
	//   [issue](https://github.com/golang/go/issues/38512).
	//
	// And we have to free the `cJSONStr` (ex: using C.free) after calling
	// `C.GOBytes`.
	//
	// So we prefer to provide a function to copy and destroy the `jsonStr`
	// ourselves, rather than using `C.GoBytes`.

	return getPropBytes(size, func(v unsafe.Pointer) C.ten_go_error_t {
		return C.ten_go_copy_c_str_to_slice_and_free(cJSONStr, v)
	})
}

// GetPropertyToJSONBytes retrieves the property and parses it as a json data.
// This function uses a bytes pool to improve the performance. ReleaseBytes is
// recommended to be called after the []byte is no longer used.
func (p *msg) GetPropertyToJSONBytes(path string) ([]byte, error) {
	return withCGOLimiterHasReturnValue[[]byte](func() ([]byte, error) {
		return p.getPropertyToJSONBytes(path)
	})
}
