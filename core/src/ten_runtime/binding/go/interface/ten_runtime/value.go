//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten_runtime

// ValueType represents the type of a Value.
type ValueType uint8

const (
	valueTypeInvalid ValueType = iota

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

	// TODO: Ptr type is not supported for now as it seems unnecessary
	// ValueTypePtr

	// ValueTypeInt - Go int, converted to int64 at runtime
	ValueTypeInt

	// ValueTypeUint - Go uint, converted to uint64 at runtime
	ValueTypeUint

	// ValueTypeJSONString - JSON string
	ValueTypeJSONString
)

// Value represents a value that can hold different types of data.
type Value struct {
	Type ValueType

	boolVal bool

	int8Val  int8
	int16Val int16
	int32Val int32
	int64Val int64

	uint8Val  uint8
	uint16Val uint16
	uint32Val uint32
	uint64Val uint64

	float32Val float32
	float64Val float64

	stringVal string
	bytesVal  []byte
	arrayVal  []Value
	objectVal map[string]Value

	// TODO: Ptr type is not supported for now as it seems unnecessary
	// ptrVal unsafe.Pointer
}

// NewBool creates a new boolean Value.
func NewBool(b bool) Value {
	return Value{Type: ValueTypeBool, boolVal: b}
}

// NewInt8 creates a new int8 Value.
func NewInt8(i int8) Value {
	return Value{Type: ValueTypeInt8, int8Val: i}
}

// NewInt16 creates a new int16 Value.
func NewInt16(i int16) Value {
	return Value{Type: ValueTypeInt16, int16Val: i}
}

// NewInt32 creates a new int32 Value.
func NewInt32(i int32) Value {
	return Value{Type: ValueTypeInt32, int32Val: i}
}

// NewInt64 creates a new int64 Value.
func NewInt64(i int64) Value {
	return Value{Type: ValueTypeInt64, int64Val: i}
}

// NewUint8 creates a new uint8 Value.
func NewUint8(i uint8) Value {
	return Value{Type: ValueTypeUint8, uint8Val: i}
}

// NewUint16 creates a new uint16 Value.
func NewUint16(i uint16) Value {
	return Value{Type: ValueTypeUint16, uint16Val: i}
}

// NewUint32 creates a new uint32 Value.
func NewUint32(i uint32) Value {
	return Value{Type: ValueTypeUint32, uint32Val: i}
}

// NewUint64 creates a new uint64 Value.
func NewUint64(i uint64) Value {
	return Value{Type: ValueTypeUint64, uint64Val: i}
}

// NewFloat32 creates a new float32 Value.
func NewFloat32(f float32) Value {
	return Value{Type: ValueTypeFloat32, float32Val: f}
}

// NewFloat64 creates a new float64 Value.
func NewFloat64(f float64) Value {
	return Value{Type: ValueTypeFloat64, float64Val: f}
}

// NewString creates a new string Value.
func NewString(s string) Value {
	return Value{Type: ValueTypeString, stringVal: s}
}

// NewBytes creates a new []byte Value.
func NewBytes(b []byte) Value {
	return Value{Type: ValueTypeBytes, bytesVal: b}
}

// NewArray creates a new array Value.
func NewArray(arr []Value) Value {
	return Value{Type: ValueTypeArray, arrayVal: arr}
}

// NewObject creates a new object Value.
func NewObject(m map[string]Value) Value {
	return Value{Type: ValueTypeObject, objectVal: m}
}

// TODO: Ptr type is not supported for now as it seems unnecessary
// NewPtr creates a new pointer Value.
//
// func NewPtr(p unsafe.Pointer) Value {
//    return Value{Type: ValueTypePtr, ptrVal: p}
// }

// NewInt creates a new int Value.
func NewInt(i int) Value {
	return Value{Type: ValueTypeInt, int64Val: int64(i)}
}

// NewUint creates a new uint Value.
func NewUint(i uint) Value {
	return Value{Type: ValueTypeUint, uint64Val: uint64(i)}
}

// NewJSONString creates a new JSON string Value.
func NewJSONString(s string) Value {
	return Value{Type: ValueTypeJSONString, stringVal: s}
}

// IsBool returns true if the Value is a boolean.
func (v *Value) IsBool() bool {
	return v.Type == ValueTypeBool
}

// IsInt8 returns true if the Value is an int8.
func (v *Value) IsInt8() bool {
	return v.Type == ValueTypeInt8
}

// IsInt16 returns true if the Value is an int16.
func (v *Value) IsInt16() bool {
	return v.Type == ValueTypeInt16
}

// IsInt32 returns true if the Value is an int32.
func (v *Value) IsInt32() bool {
	return v.Type == ValueTypeInt32
}

// IsInt64 returns true if the Value is an int64.
func (v *Value) IsInt64() bool {
	return v.Type == ValueTypeInt64
}

// IsUint8 returns true if the Value is an uint8.
func (v *Value) IsUint8() bool {
	return v.Type == ValueTypeUint8
}

// IsUint16 returns true if the Value is an uint16.
func (v *Value) IsUint16() bool {
	return v.Type == ValueTypeUint16
}

// IsUint32 returns true if the Value is an uint32.
func (v *Value) IsUint32() bool {
	return v.Type == ValueTypeUint32
}

// IsUint64 returns true if the Value is an uint64.
func (v *Value) IsUint64() bool {
	return v.Type == ValueTypeUint64
}

// IsFloat32 returns true if the Value is a float32.
func (v *Value) IsFloat32() bool {
	return v.Type == ValueTypeFloat32
}

// IsFloat64 returns true if the Value is a float64.
func (v *Value) IsFloat64() bool {
	return v.Type == ValueTypeFloat64
}

// IsString returns true if the Value is a string.
func (v *Value) IsString() bool {
	return v.Type == ValueTypeString
}

// IsBytes returns true if the Value is a []byte.
func (v *Value) IsBytes() bool {
	return v.Type == ValueTypeBytes
}

// IsArray returns true if the Value is an array.
func (v *Value) IsArray() bool {
	return v.Type == ValueTypeArray
}

// IsObject returns true if the Value is an object.
func (v *Value) IsObject() bool {
	return v.Type == ValueTypeObject
}

// IsInt returns true if the Value is an int.
func (v *Value) IsInt() bool {
	return v.Type == ValueTypeInt
}

// IsUint returns true if the Value is an uint.
func (v *Value) IsUint() bool {
	return v.Type == ValueTypeUint
}

// IsJSONString returns true if the Value is a JSON string.
func (v *Value) IsJSONString() bool {
	return v.Type == ValueTypeJSONString
}
