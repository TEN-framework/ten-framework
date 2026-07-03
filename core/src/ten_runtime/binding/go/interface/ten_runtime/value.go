//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten_runtime

import "unsafe"

// ValueType represents the type of a Value.
type ValueType uint8

const (
	// In theory, users should not see this value, so it is not exported
	valueTypeInvalid ValueType = iota

	// ValueTypeNull - Null
	ValueTypeNull

	// ValueTypeBool - Boolean
	ValueTypeBool

	// ValueTypeInt8 - 8-bit integer
	ValueTypeInt8

	// ValueTypeInt16 - 16-bit integer
	ValueTypeInt16

	// ValueTypeInt32 - 32-bit integer
	ValueTypeInt32

	// ValueTypeInt64 - 64-bit integer
	ValueTypeInt64

	// ValueTypeUint8 - 8-bit unsigned integer
	ValueTypeUint8

	// ValueTypeUint16 - 16-bit unsigned integer
	ValueTypeUint16

	// ValueTypeUint32 - 32-bit unsigned integer
	ValueTypeUint32

	// ValueTypeUint64 - 64-bit unsigned integer
	ValueTypeUint64

	// ValueTypeFloat32 - 32-bit floating point number
	ValueTypeFloat32

	// ValueTypeFloat64 - 64-bit floating point number
	ValueTypeFloat64

	// ValueTypeString - String
	ValueTypeString

	// ValueTypeBytes - Buffer
	ValueTypeBytes

	// ValueTypeArray - Array
	ValueTypeArray

	// ValueTypeObject - Object
	ValueTypeObject

	// ValueTypePtr - Pointer
	ValueTypePtr

	// ValueTypeJSONString - JSON string
	ValueTypeJSONString
)

// Value represents a value that can hold different types of data.
type Value struct {
	typ  ValueType
	data any
}

// NewBoolValue creates a new boolean Value.
func NewBoolValue(b bool) Value {
	return Value{typ: ValueTypeBool, data: b}
}

// NewInt8Value creates a new int8 Value.
func NewInt8Value(i int8) Value {
	return Value{typ: ValueTypeInt8, data: i}
}

// NewInt16Value creates a new int16 Value.
func NewInt16Value(i int16) Value {
	return Value{typ: ValueTypeInt16, data: i}
}

// NewInt32Value creates a new int32 Value.
func NewInt32Value(i int32) Value {
	return Value{typ: ValueTypeInt32, data: i}
}

// NewInt64Value creates a new int64 Value.
func NewInt64Value(i int64) Value {
	return Value{typ: ValueTypeInt64, data: i}
}

// NewUint8Value creates a new uint8 Value.
func NewUint8Value(i uint8) Value {
	return Value{typ: ValueTypeUint8, data: i}
}

// NewUint16Value creates a new uint16 Value.
func NewUint16Value(i uint16) Value {
	return Value{typ: ValueTypeUint16, data: i}
}

// NewUint32Value creates a new uint32 Value.
func NewUint32Value(i uint32) Value {
	return Value{typ: ValueTypeUint32, data: i}
}

// NewUint64Value creates a new uint64 Value.
func NewUint64Value(i uint64) Value {
	return Value{typ: ValueTypeUint64, data: i}
}

// NewFloat32Value creates a new float32 Value.
func NewFloat32Value(f float32) Value {
	return Value{typ: ValueTypeFloat32, data: f}
}

// NewFloat64Value creates a new float64 Value.
func NewFloat64Value(f float64) Value {
	return Value{typ: ValueTypeFloat64, data: f}
}

// NewStringValue creates a new string Value.
func NewStringValue(s string) Value {
	return Value{typ: ValueTypeString, data: s}
}

// NewBufValue creates a new []byte Value.
func NewBufValue(b []byte) Value {
	return Value{typ: ValueTypeBytes, data: b}
}

// NewArrayValue creates a new array Value.
func NewArrayValue(arr []Value) Value {
	return Value{typ: ValueTypeArray, data: arr}
}

// NewObjectValue creates a new object Value.
func NewObjectValue(m map[string]Value) Value {
	return Value{typ: ValueTypeObject, data: m}
}

// NewPtrValue creates a new pointer Value.
func NewPtrValue(p unsafe.Pointer) Value {
	return Value{typ: ValueTypePtr, data: p}
}

// NewJSONStringValue creates a new JSON string Value.
func NewJSONStringValue(s string) Value {
	return Value{typ: ValueTypeJSONString, data: s}
}

// NewIntValue creates a new int Value.
func NewIntValue(i int) Value {
	return Value{typ: ValueTypeInt64, data: int64(i)}
}

// NewUintValue creates a new uint Value.
func NewUintValue(i uint) Value {
	return Value{typ: ValueTypeUint64, data: uint64(i)}
}

// GetType returns the ValueType of the Value.
func (v *Value) GetType() ValueType {
	return v.typ
}

// GetBool returns the boolean value if the type matches, otherwise returns an
// error.
func (v *Value) GetBool() (bool, error) {
	if v.typ != ValueTypeBool {
		return false, NewTenError(ErrorCodeInvalidType, "value is not a bool")
	}
	if val, ok := v.data.(bool); ok {
		return val, nil
	}
	return false, NewTenError(ErrorCodeInvalidType, "value is not a bool")
}

// GetInt8 returns the int8 value if the type matches, otherwise returns an
// error.
func (v *Value) GetInt8() (int8, error) {
	if v.typ != ValueTypeInt8 {
		return 0, NewTenError(ErrorCodeInvalidType, "value is not an int8")
	}
	if val, ok := v.data.(int8); ok {
		return val, nil
	}
	return 0, NewTenError(ErrorCodeInvalidType, "value is not an int8")
}

// GetInt16 returns the int16 value if the type matches, otherwise returns an
// error.
func (v *Value) GetInt16() (int16, error) {
	if v.typ != ValueTypeInt16 {
		return 0, NewTenError(ErrorCodeInvalidType, "value is not an int16")
	}
	if val, ok := v.data.(int16); ok {
		return val, nil
	}
	return 0, NewTenError(ErrorCodeInvalidType, "value is not an int16")
}

// GetInt32 returns the int32 value if the type matches, otherwise returns an
// error.
func (v *Value) GetInt32() (int32, error) {
	if v.typ != ValueTypeInt32 {
		return 0, NewTenError(ErrorCodeInvalidType, "value is not an int32")
	}
	if val, ok := v.data.(int32); ok {
		return val, nil
	}
	return 0, NewTenError(ErrorCodeInvalidType, "value is not an int32")
}

// GetInt64 returns the int64 value if the type matches, otherwise returns an
// error.
func (v *Value) GetInt64() (int64, error) {
	if v.typ != ValueTypeInt64 {
		return 0, NewTenError(ErrorCodeInvalidType, "value is not an int64")
	}
	if val, ok := v.data.(int64); ok {
		return val, nil
	}
	return 0, NewTenError(ErrorCodeInvalidType, "value is not an int64")
}

// GetUint8 returns the uint8 value if the type matches, otherwise returns an
// error.
func (v *Value) GetUint8() (uint8, error) {
	if v.typ != ValueTypeUint8 {
		return 0, NewTenError(ErrorCodeInvalidType, "value is not a uint8")
	}
	if val, ok := v.data.(uint8); ok {
		return val, nil
	}
	return 0, NewTenError(ErrorCodeInvalidType, "value is not a uint8")
}

// GetUint16 returns the uint16 value if the type matches, otherwise returns an
// error.
func (v *Value) GetUint16() (uint16, error) {
	if v.typ != ValueTypeUint16 {
		return 0, NewTenError(ErrorCodeInvalidType, "value is not a uint16")
	}
	if val, ok := v.data.(uint16); ok {
		return val, nil
	}
	return 0, NewTenError(ErrorCodeInvalidType, "value is not a uint16")
}

// GetUint32 returns the uint32 value if the type matches, otherwise returns an
// error.
func (v *Value) GetUint32() (uint32, error) {
	if v.typ != ValueTypeUint32 {
		return 0, NewTenError(ErrorCodeInvalidType, "value is not a uint32")
	}
	if val, ok := v.data.(uint32); ok {
		return val, nil
	}
	return 0, NewTenError(ErrorCodeInvalidType, "value is not a uint32")
}

// GetUint64 returns the uint64 value if the type matches, otherwise returns an
// error.
func (v *Value) GetUint64() (uint64, error) {
	if v.typ != ValueTypeUint64 {
		return 0, NewTenError(ErrorCodeInvalidType, "value is not a uint64")
	}
	if val, ok := v.data.(uint64); ok {
		return val, nil
	}
	return 0, NewTenError(ErrorCodeInvalidType, "value is not a uint64")
}

// GetFloat32 returns the float32 value if the type matches, otherwise returns
// an error.
func (v *Value) GetFloat32() (float32, error) {
	if v.typ != ValueTypeFloat32 {
		return 0, NewTenError(ErrorCodeInvalidType, "value is not a float32")
	}
	if val, ok := v.data.(float32); ok {
		return val, nil
	}
	return 0, NewTenError(ErrorCodeInvalidType, "value is not a float32")
}

// GetFloat64 returns the float64 value if the type matches, otherwise returns
// an error.
func (v *Value) GetFloat64() (float64, error) {
	if v.typ != ValueTypeFloat64 {
		return 0, NewTenError(ErrorCodeInvalidType, "value is not a float64")
	}
	if val, ok := v.data.(float64); ok {
		return val, nil
	}
	return 0, NewTenError(ErrorCodeInvalidType, "value is not a float64")
}

// GetString returns the string value if the type matches, otherwise returns an
// error.
func (v *Value) GetString() (string, error) {
	if v.typ != ValueTypeString {
		return "", NewTenError(ErrorCodeInvalidType, "value is not a string")
	}
	if val, ok := v.data.(string); ok {
		return val, nil
	}
	return "", NewTenError(ErrorCodeInvalidType, "value is not a string")
}

// GetBuf returns the []byte value if the type matches, otherwise returns an
// error.
func (v *Value) GetBuf() ([]byte, error) {
	if v.typ != ValueTypeBytes {
		return nil, NewTenError(ErrorCodeInvalidType, "value is not bytes")
	}
	if val, ok := v.data.([]byte); ok {
		return val, nil
	}
	return nil, NewTenError(ErrorCodeInvalidType, "value is not bytes")
}

// GetArray returns the []Value value if the type matches, otherwise returns an
// error.
func (v *Value) GetArray() ([]Value, error) {
	if v.typ != ValueTypeArray {
		return nil, NewTenError(ErrorCodeInvalidType, "value is not an array")
	}
	if val, ok := v.data.([]Value); ok {
		return val, nil
	}
	return nil, NewTenError(ErrorCodeInvalidType, "value is not an array")
}

// GetObject returns the map[string]Value value if the type matches, otherwise
// returns an error.
func (v *Value) GetObject() (map[string]Value, error) {
	if v.typ != ValueTypeObject {
		return nil, NewTenError(ErrorCodeInvalidType, "value is not an object")
	}
	if val, ok := v.data.(map[string]Value); ok {
		return val, nil
	}
	return nil, NewTenError(ErrorCodeInvalidType, "value is not an object")
}

// GetPtr returns the unsafe.Pointer value if the type matches, otherwise
// returns an error.
func (v *Value) GetPtr() (unsafe.Pointer, error) {
	if v.typ != ValueTypePtr {
		return nil, NewTenError(ErrorCodeInvalidType, "value is not a pointer")
	}
	if val, ok := v.data.(unsafe.Pointer); ok {
		return val, nil
	}
	return nil, NewTenError(ErrorCodeInvalidType, "value is not a pointer")
}

// GetJSONString returns the JSON string value if the type matches, otherwise
// returns an error.
func (v *Value) GetJSONString() (string, error) {
	if v.typ != ValueTypeJSONString {
		return "", NewTenError(
			ErrorCodeInvalidType,
			"value is not a JSON string",
		)
	}
	if val, ok := v.data.(string); ok {
		return val, nil
	}
	return "", NewTenError(ErrorCodeInvalidType, "value is not a JSON string")
}

// GetInt returns the int value if the type matches, otherwise returns an error.
func (v *Value) GetInt() (int, error) {
	if v.typ != ValueTypeInt64 {
		return 0, NewTenError(ErrorCodeInvalidType, "value is not an int")
	}
	if val, ok := v.data.(int64); ok {
		return int(val), nil
	}
	return 0, NewTenError(ErrorCodeInvalidType, "value is not an int")
}

// GetUint returns the uint value if the type matches, otherwise returns an
// error.
func (v *Value) GetUint() (uint, error) {
	if v.typ != ValueTypeUint64 {
		return 0, NewTenError(ErrorCodeInvalidType, "value is not a uint")
	}
	if val, ok := v.data.(uint64); ok {
		return uint(val), nil
	}
	return 0, NewTenError(ErrorCodeInvalidType, "value is not a uint")
}
