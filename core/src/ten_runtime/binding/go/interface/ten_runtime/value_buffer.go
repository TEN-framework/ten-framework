//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten_runtime

import (
	"encoding/binary"
	"fmt"
	"math"
)

// Buffer protocol constants - must match C layer
const (
	valueBufferMagic      uint16 = 0x10E
	valueBufferVersion    uint8  = 1
	valueBufferHeaderSize        = 8 // magic(2) + version(1) + type(1) + size(4)
)

// Buffer type constants - must match TEN_VALUE_BUFFER_TYPE in C
const (
	bufferTypeInvalid    uint8 = 0
	bufferTypeBool       uint8 = 1
	bufferTypeInt8       uint8 = 2
	bufferTypeInt16      uint8 = 3
	bufferTypeInt32      uint8 = 4
	bufferTypeInt64      uint8 = 5
	bufferTypeUint8      uint8 = 6
	bufferTypeUint16     uint8 = 7
	bufferTypeUint32     uint8 = 8
	bufferTypeUint64     uint8 = 9
	bufferTypeFloat32    uint8 = 10
	bufferTypeFloat64    uint8 = 11
	bufferTypeString     uint8 = 12
	bufferTypeBytes      uint8 = 13
	bufferTypeArray      uint8 = 14
	bufferTypeObject     uint8 = 15
	bufferTypePtr        uint8 = 16
	bufferTypeJSONString uint8 = 17
)

// valueBufferHeader represents the buffer header structure
type valueBufferHeader struct {
	magic    uint16
	version  uint8
	typeName uint8
	size     uint32
}

// newBufferProtocolError creates a new TenError for buffer protocol errors
func newBufferProtocolError(code TenErrorCode, message string) *TenError {
	return NewTenError(code, message)
}

// valueTypeToBufferType converts ValueType to buffer type
func valueTypeToBufferType(vt ValueType) uint8 {
	switch vt {
	case ValueTypeBool:
		return bufferTypeBool
	case ValueTypeInt8:
		return bufferTypeInt8
	case ValueTypeInt16:
		return bufferTypeInt16
	case ValueTypeInt32:
		return bufferTypeInt32
	case ValueTypeInt64:
		return bufferTypeInt64
	case ValueTypeUint8:
		return bufferTypeUint8
	case ValueTypeUint16:
		return bufferTypeUint16
	case ValueTypeUint32:
		return bufferTypeUint32
	case ValueTypeUint64:
		return bufferTypeUint64
	case ValueTypeFloat32:
		return bufferTypeFloat32
	case ValueTypeFloat64:
		return bufferTypeFloat64
	case ValueTypeString:
		return bufferTypeString
	case ValueTypeBytes:
		return bufferTypeBytes
	case ValueTypeArray:
		return bufferTypeArray
	case ValueTypeObject:
		return bufferTypeObject
	case ValueTypePtr:
		return bufferTypePtr
	case ValueTypeJSONString:
		return bufferTypeJSONString
	default:
		return bufferTypeInvalid
	}
}

// bufferTypeToValueType converts buffer type to ValueType
func bufferTypeToValueType(bt uint8) ValueType {
	switch bt {
	case bufferTypeBool:
		return ValueTypeBool
	case bufferTypeInt8:
		return ValueTypeInt8
	case bufferTypeInt16:
		return ValueTypeInt16
	case bufferTypeInt32:
		return ValueTypeInt32
	case bufferTypeInt64:
		return ValueTypeInt64
	case bufferTypeUint8:
		return ValueTypeUint8
	case bufferTypeUint16:
		return ValueTypeUint16
	case bufferTypeUint32:
		return ValueTypeUint32
	case bufferTypeUint64:
		return ValueTypeUint64
	case bufferTypeFloat32:
		return ValueTypeFloat32
	case bufferTypeFloat64:
		return ValueTypeFloat64
	case bufferTypeString:
		return ValueTypeString
	case bufferTypeBytes:
		return ValueTypeBytes
	case bufferTypeArray:
		return ValueTypeArray
	case bufferTypeObject:
		return ValueTypeObject
	case bufferTypePtr:
		return ValueTypePtr
	case bufferTypeJSONString:
		return ValueTypeJSONString
	default:
		return valueTypeInvalid
	}
}

// calculateSerializeSize calculates the total size needed for serialization
func (v *Value) calculateSerializeSize() (int, error) {
	contentSize, err := v.calculateContentSize()
	if err != nil {
		return 0, err
	}
	return valueBufferHeaderSize + contentSize, nil
}

// calculateContentSize calculates the size of the value content
func (v *Value) calculateContentSize() (int, error) {
	switch v.typ {
	case valueTypeInvalid:
		return 0, NewTenError(
			ErrorCodeInvalidType,
			"unsupported value type for size calculation",
		)
	case ValueTypeBool, ValueTypeInt8, ValueTypeUint8:
		return 1, nil
	case ValueTypeInt16, ValueTypeUint16:
		return 2, nil
	case ValueTypeInt32, ValueTypeUint32:
		return 4, nil
	case ValueTypeInt64, ValueTypeUint64:
		return 8, nil
	case ValueTypeFloat32:
		return 4, nil
	case ValueTypeFloat64:
		return 8, nil
	case ValueTypeString:
		str, err := v.GetString()
		if err != nil {
			return 0, fmt.Errorf("failed to get string value: %w", err)
		}
		return 4 + len(str), nil // length(4) + data
	case ValueTypeJSONString:
		str, err := v.GetJSONString()
		if err != nil {
			return 0, fmt.Errorf("failed to get string value: %w", err)
		}
		return 4 + len(str), nil // length(4) + data
	case ValueTypeBytes:
		bytes, err := v.GetBuf()
		if err != nil {
			return 0, fmt.Errorf("failed to get bytes value: %w", err)
		}
		return 4 + len(bytes), nil // length(4) + data

	case ValueTypeArray:
		arr, err := v.GetArray()
		if err != nil {
			return 0, fmt.Errorf("failed to get array value: %w", err)
		}
		size := 4 // array length
		for _, item := range arr {
			size++ // item type
			itemSize, err := item.calculateContentSize()
			if err != nil {
				return 0, fmt.Errorf(
					"failed to calculate array item size: %w",
					err,
				)
			}
			size += itemSize
		}
		return size, nil

	case ValueTypeObject:
		obj, err := v.GetObject()
		if err != nil {
			return 0, fmt.Errorf("failed to get object value: %w", err)
		}
		size := 4 // object size
		for key, val := range obj {
			size += 4 + len(key) // key length + key data
			size++               // value type
			valSize, err := val.calculateContentSize()
			if err != nil {
				return 0, fmt.Errorf(
					"failed to calculate object value size: %w",
					err,
				)
			}
			size += valSize
		}
		return size, nil

	default:
		return 0, NewTenError(
			ErrorCodeInvalidType,
			"unknown value type for size calculation",
		)
	}
}

// serializeToBuffer serializes the Value to a buffer using only Go operations
func (v *Value) serializeToBuffer() ([]byte, error) {
	totalSize, sizeErr := v.calculateSerializeSize()
	if sizeErr != nil {
		return nil, fmt.Errorf(
			"failed to calculate serialize size: %w",
			sizeErr,
		)
	}
	buffer := make([]byte, totalSize)

	pos := 0

	// Write header
	header := valueBufferHeader{
		magic:    valueBufferMagic,
		version:  valueBufferVersion,
		typeName: valueTypeToBufferType(v.typ),
		size:     uint32(totalSize - valueBufferHeaderSize),
	}

	binary.LittleEndian.PutUint16(buffer[pos:], header.magic)
	pos += 2
	buffer[pos] = header.version
	pos++
	buffer[pos] = header.typeName
	pos++
	binary.LittleEndian.PutUint32(buffer[pos:], header.size)
	pos += 4

	// Write content
	err := v.serializeContent(buffer, &pos)
	if err != nil {
		return nil, err
	}

	return buffer, nil
}

// serializeContent serializes the value content to buffer
func (v *Value) serializeContent(buffer []byte, pos *int) error {
	switch v.typ {
	case valueTypeInvalid:
		panic("unsupported value type for serialization")

	case ValueTypeBool:
		boolVal, err := v.GetBool()
		if err != nil {
			return fmt.Errorf("failed to get bool value: %w", err)
		}
		val := uint8(0)
		if boolVal {
			val = 1
		}
		buffer[*pos] = val
		*pos++

	case ValueTypeInt8:
		int8Val, err := v.GetInt8()
		if err != nil {
			return fmt.Errorf("failed to get int8 value: %w", err)
		}
		buffer[*pos] = uint8(int8Val)
		*pos++

	case ValueTypeInt16:
		int16Val, err := v.GetInt16()
		if err != nil {
			return fmt.Errorf("failed to get int16 value: %w", err)
		}
		binary.LittleEndian.PutUint16(buffer[*pos:], uint16(int16Val))
		*pos += 2

	case ValueTypeInt32:
		int32Val, err := v.GetInt32()
		if err != nil {
			return fmt.Errorf("failed to get int32 value: %w", err)
		}
		binary.LittleEndian.PutUint32(buffer[*pos:], uint32(int32Val))
		*pos += 4

	case ValueTypeInt64:
		int64Val, err := v.GetInt64()
		if err != nil {
			return fmt.Errorf("failed to get int64 value: %w", err)
		}
		binary.LittleEndian.PutUint64(buffer[*pos:], uint64(int64Val))
		*pos += 8

	case ValueTypeUint8:
		uint8Val, err := v.GetUint8()
		if err != nil {
			return fmt.Errorf("failed to get uint8 value: %w", err)
		}
		buffer[*pos] = uint8Val
		*pos++

	case ValueTypeUint16:
		uint16Val, err := v.GetUint16()
		if err != nil {
			return fmt.Errorf("failed to get uint16 value: %w", err)
		}
		binary.LittleEndian.PutUint16(buffer[*pos:], uint16Val)
		*pos += 2

	case ValueTypeUint32:
		uint32Val, err := v.GetUint32()
		if err != nil {
			return fmt.Errorf("failed to get uint32 value: %w", err)
		}
		binary.LittleEndian.PutUint32(buffer[*pos:], uint32Val)
		*pos += 4

	case ValueTypeUint64:
		uint64Val, err := v.GetUint64()
		if err != nil {
			return fmt.Errorf("failed to get uint64 value: %w", err)
		}
		binary.LittleEndian.PutUint64(buffer[*pos:], uint64Val)
		*pos += 8

	case ValueTypeFloat32:
		float32Val, err := v.GetFloat32()
		if err != nil {
			return fmt.Errorf("failed to get float32 value: %w", err)
		}
		binary.LittleEndian.PutUint32(
			buffer[*pos:],
			math.Float32bits(float32Val),
		)
		*pos += 4

	case ValueTypeFloat64:
		float64Val, err := v.GetFloat64()
		if err != nil {
			return fmt.Errorf("failed to get float64 value: %w", err)
		}
		binary.LittleEndian.PutUint64(
			buffer[*pos:],
			math.Float64bits(float64Val),
		)
		*pos += 8

	case ValueTypeString, ValueTypeJSONString:
		var stringVal string
		var err error
		if v.typ == ValueTypeString {
			stringVal, err = v.GetString()
		} else {
			stringVal, err = v.GetJSONString()
		}
		if err != nil {
			return fmt.Errorf("failed to get string value: %w", err)
		}

		strBytes := []byte(stringVal)
		strLen := uint32(len(strBytes))

		binary.LittleEndian.PutUint32(buffer[*pos:], strLen)
		*pos += 4

		if strLen > 0 {
			copy(buffer[*pos:], strBytes)
			*pos += int(strLen)
		}

	case ValueTypeBytes:
		bytesVal, err := v.GetBuf()
		if err != nil {
			return fmt.Errorf("failed to get bytes value: %w", err)
		}

		bufLen := uint32(len(bytesVal))

		binary.LittleEndian.PutUint32(buffer[*pos:], bufLen)
		*pos += 4

		if bufLen > 0 {
			copy(buffer[*pos:], bytesVal)
			*pos += int(bufLen)
		}

	case ValueTypeArray:
		arrayVal, err := v.GetArray()
		if err != nil {
			return fmt.Errorf("failed to get array value: %w", err)
		}

		arrayLen := uint32(len(arrayVal))
		binary.LittleEndian.PutUint32(buffer[*pos:], arrayLen)
		*pos += 4

		for _, item := range arrayVal {
			buffer[*pos] = valueTypeToBufferType(item.typ)
			*pos++

			err := item.serializeContent(buffer, pos)
			if err != nil {
				return err
			}
		}

	case ValueTypeObject:
		objectVal, err := v.GetObject()
		if err != nil {
			return fmt.Errorf("failed to get object value: %w", err)
		}

		objSize := uint32(len(objectVal))
		binary.LittleEndian.PutUint32(buffer[*pos:], objSize)
		*pos += 4

		for key, val := range objectVal {
			// Write key
			keyBytes := []byte(key)
			keyLen := uint32(len(keyBytes))

			binary.LittleEndian.PutUint32(buffer[*pos:], keyLen)
			*pos += 4
			copy(buffer[*pos:], keyBytes)
			*pos += int(keyLen)

			// Write value type and content
			buffer[*pos] = valueTypeToBufferType(val.typ)
			*pos++

			err := val.serializeContent(buffer, pos)
			if err != nil {
				return err
			}
		}

	default:
		panic("unsupported value type for serialization")
	}

	return nil
}

// validateBufferHeader validates the buffer header and returns header info
func validateBufferHeader(buffer []byte) (*valueBufferHeader, error) {
	if len(buffer) < valueBufferHeaderSize {
		panic("buffer too small to contain header")
	}

	header := &valueBufferHeader{}
	pos := 0

	header.magic = binary.LittleEndian.Uint16(buffer[pos:])
	pos += 2
	header.version = buffer[pos]
	pos++
	header.typeName = buffer[pos]
	pos++
	header.size = binary.LittleEndian.Uint32(buffer[pos:])

	if header.magic != valueBufferMagic {
		return nil, newBufferProtocolError(ErrorCodeGeneric,
			"invalid buffer magic number")
	}

	if header.version != valueBufferVersion {
		return nil, newBufferProtocolError(ErrorCodeGeneric,
			"unsupported buffer protocol version")
	}

	if bufferTypeToValueType(header.typeName) == valueTypeInvalid {
		return nil, newBufferProtocolError(ErrorCodeGeneric,
			"invalid or unknown buffer type")
	}

	if uint32(len(buffer)) < uint32(valueBufferHeaderSize)+header.size {
		return nil, newBufferProtocolError(ErrorCodeGeneric,
			"buffer size doesn't match header specification")
	}

	return header, nil
}

// deserializeFromBuffer deserializes a Value from buffer
func deserializeFromBuffer(buffer []byte) (*Value, error) {
	header, err := validateBufferHeader(buffer)
	if err != nil {
		return nil, err
	}

	valueType := bufferTypeToValueType(header.typeName)
	pos := valueBufferHeaderSize

	value := &Value{typ: valueType}

	err = value.deserializeContent(buffer, &pos)
	if err != nil {
		return nil, err
	}

	return value, nil
}

// deserializeContent deserializes the value content from buffer
func (v *Value) deserializeContent(buffer []byte, pos *int) error {
	switch v.typ {
	case valueTypeInvalid:
		panic("unsupported value type for deserialization")

	case ValueTypeBool:
		boolVal := buffer[*pos] != 0
		v.data = boolVal
		*pos++

	case ValueTypeInt8:
		int8Val := int8(buffer[*pos])
		v.data = int8Val
		*pos++

	case ValueTypeInt16:
		int16Val := int16(binary.LittleEndian.Uint16(buffer[*pos:]))
		v.data = int16Val
		*pos += 2

	case ValueTypeInt32:
		int32Val := int32(binary.LittleEndian.Uint32(buffer[*pos:]))
		v.data = int32Val
		*pos += 4

	case ValueTypeInt64:
		int64Val := int64(binary.LittleEndian.Uint64(buffer[*pos:]))
		v.data = int64Val
		*pos += 8

	case ValueTypeUint8:
		uint8Val := buffer[*pos]
		v.data = uint8Val
		*pos++

	case ValueTypeUint16:
		uint16Val := binary.LittleEndian.Uint16(buffer[*pos:])
		v.data = uint16Val
		*pos += 2

	case ValueTypeUint32:
		uint32Val := binary.LittleEndian.Uint32(buffer[*pos:])
		v.data = uint32Val
		*pos += 4

	case ValueTypeUint64:
		uint64Val := binary.LittleEndian.Uint64(buffer[*pos:])
		v.data = uint64Val
		*pos += 8

	case ValueTypeFloat32:
		float32Val := math.Float32frombits(
			binary.LittleEndian.Uint32(buffer[*pos:]),
		)
		v.data = float32Val
		*pos += 4

	case ValueTypeFloat64:
		float64Val := math.Float64frombits(
			binary.LittleEndian.Uint64(buffer[*pos:]),
		)
		v.data = float64Val
		*pos += 8

	case ValueTypeString, ValueTypeJSONString:
		strLen := binary.LittleEndian.Uint32(buffer[*pos:])
		*pos += 4

		if strLen == 0 {
			v.data = ""
		} else {
			if *pos+int(strLen) > len(buffer) {
				return newBufferProtocolError(ErrorCodeGeneric,
					"string data exceeds buffer boundary")
			}
			stringVal := string(buffer[*pos : *pos+int(strLen)])
			v.data = stringVal
			*pos += int(strLen)
		}

	case ValueTypeBytes:
		bufLen := binary.LittleEndian.Uint32(buffer[*pos:])
		*pos += 4

		if bufLen == 0 {
			v.data = ([]byte)(nil)
		} else {
			if *pos+int(bufLen) > len(buffer) {
				return newBufferProtocolError(ErrorCodeGeneric,
					"bytes data exceeds buffer boundary")
			}
			bytesVal := make([]byte, bufLen)
			copy(bytesVal, buffer[*pos:*pos+int(bufLen)])
			v.data = bytesVal
			*pos += int(bufLen)
		}

	case ValueTypeArray:
		arrayLen := binary.LittleEndian.Uint32(buffer[*pos:])
		*pos += 4

		arrayVal := make([]Value, arrayLen)

		for i := uint32(0); i < arrayLen; i++ {
			if *pos >= len(buffer) {
				return newBufferProtocolError(ErrorCodeGeneric,
					"array item exceeds buffer boundary")
			}

			itemType := bufferTypeToValueType(buffer[*pos])
			*pos++

			item := &Value{typ: itemType}
			err := item.deserializeContent(buffer, pos)
			if err != nil {
				return err
			}

			arrayVal[i] = *item
		}
		v.data = arrayVal

	case ValueTypeObject:
		objSize := binary.LittleEndian.Uint32(buffer[*pos:])
		*pos += 4

		objectVal := make(map[string]Value)

		for i := uint32(0); i < objSize; i++ {
			// Read key
			if *pos+4 > len(buffer) {
				return newBufferProtocolError(ErrorCodeGeneric,
					"object key length exceeds buffer boundary")
			}

			keyLen := binary.LittleEndian.Uint32(buffer[*pos:])
			*pos += 4

			if *pos+int(keyLen) > len(buffer) {
				return newBufferProtocolError(ErrorCodeGeneric,
					"object key data exceeds buffer boundary")
			}

			key := string(buffer[*pos : *pos+int(keyLen)])
			*pos += int(keyLen)

			// Read value
			if *pos >= len(buffer) {
				return newBufferProtocolError(ErrorCodeGeneric,
					"object value type exceeds buffer boundary")
			}

			valType := bufferTypeToValueType(buffer[*pos])
			*pos++

			val := &Value{typ: valType}
			err := val.deserializeContent(buffer, pos)
			if err != nil {
				return err
			}

			objectVal[key] = *val
		}
		v.data = objectVal

	default:
		panic("unsupported value type for deserialization")
	}

	return nil
}
