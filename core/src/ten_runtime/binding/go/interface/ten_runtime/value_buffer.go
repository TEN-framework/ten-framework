//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten_runtime

import (
	"encoding/binary"
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

// Value buffer protocol error helper functions
func newBufferProtocolError(errorCode TenErrorCode, message string) *TenError {
	return NewTenError(errorCode, message)
}

// valueTypeToBufferType converts Go ValueType to buffer type
func valueTypeToBufferType(vt ValueType) uint8 {
	switch vt {
	case valueTypeInvalid:
		return bufferTypeInvalid
	case ValueTypeBool:
		return bufferTypeBool
	case ValueTypeInt8:
		return bufferTypeInt8
	case ValueTypeInt16:
		return bufferTypeInt16
	case ValueTypeInt32:
		return bufferTypeInt32
	case ValueTypeInt64, ValueTypeInt:
		return bufferTypeInt64
	case ValueTypeUint8:
		return bufferTypeUint8
	case ValueTypeUint16:
		return bufferTypeUint16
	case ValueTypeUint32:
		return bufferTypeUint32
	case ValueTypeUint64, ValueTypeUint:
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
	case ValueTypeJSONString:
		return bufferTypeJSONString
	default:
		return bufferTypeInvalid
	}
}

// bufferTypeToValueType converts buffer type to Go ValueType
func bufferTypeToValueType(bt uint8) ValueType {
	switch bt {
	case bufferTypeInvalid:
		return valueTypeInvalid
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
	case bufferTypeJSONString:
		return valueTypeInvalid // JSON string is not supported for deserialization
	default:
		return valueTypeInvalid
	}
}

// calculateSerializeSize calculates the buffer size needed to serialize a value
func (v *Value) calculateSerializeSize() int {
	return valueBufferHeaderSize + v.calculateContentSize()
}

// calculateContentSize calculates the size needed for the value content
func (v *Value) calculateContentSize() int {
	switch v.Type {
	case valueTypeInvalid:
		return 0

	case ValueTypeBool:
		return 1

	case ValueTypeInt8, ValueTypeUint8:
		return 1

	case ValueTypeInt16, ValueTypeUint16:
		return 2

	case ValueTypeInt32, ValueTypeUint32:
		return 4

	case ValueTypeInt64, ValueTypeUint64, ValueTypeInt, ValueTypeUint:
		return 8

	case ValueTypeFloat32:
		return 4

	case ValueTypeFloat64:
		return 8

	case ValueTypeString, ValueTypeJSONString:
		return 4 + len(v.stringVal) // length(4) + data

	case ValueTypeBytes:
		return 4 + len(v.bytesVal) // length(4) + data

	case ValueTypeArray:
		size := 4 // array length
		for _, item := range v.arrayVal {
			size++ // item type
			size += item.calculateContentSize()
		}
		return size

	case ValueTypeObject:
		size := 4 // object size
		for key, val := range v.objectVal {
			size += 4 + len(key) // key length + key data
			size++               // value type
			size += val.calculateContentSize()
		}
		return size

	default:
		return 0
	}
}

// serializeToBuffer serializes the Value to a buffer using only Go operations
func (v *Value) serializeToBuffer() ([]byte, error) {
	totalSize := v.calculateSerializeSize()
	buffer := make([]byte, totalSize)

	pos := 0

	// Write header
	header := valueBufferHeader{
		magic:    valueBufferMagic,
		version:  valueBufferVersion,
		typeName: valueTypeToBufferType(v.Type),
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
	switch v.Type {
	case valueTypeInvalid:
		// No additional data

	case ValueTypeBool:
		val := uint8(0)
		if v.boolVal {
			val = 1
		}
		buffer[*pos] = val
		*pos++

	case ValueTypeInt8:
		buffer[*pos] = uint8(v.int8Val)
		*pos++

	case ValueTypeInt16:
		binary.LittleEndian.PutUint16(buffer[*pos:], uint16(v.int16Val))
		*pos += 2

	case ValueTypeInt32:
		binary.LittleEndian.PutUint32(buffer[*pos:], uint32(v.int32Val))
		*pos += 4

	case ValueTypeInt64, ValueTypeInt:
		binary.LittleEndian.PutUint64(buffer[*pos:], uint64(v.int64Val))
		*pos += 8

	case ValueTypeUint8:
		buffer[*pos] = v.uint8Val
		*pos++

	case ValueTypeUint16:
		binary.LittleEndian.PutUint16(buffer[*pos:], v.uint16Val)
		*pos += 2

	case ValueTypeUint32:
		binary.LittleEndian.PutUint32(buffer[*pos:], v.uint32Val)
		*pos += 4

	case ValueTypeUint64, ValueTypeUint:
		binary.LittleEndian.PutUint64(buffer[*pos:], v.uint64Val)
		*pos += 8

	case ValueTypeFloat32:
		binary.LittleEndian.PutUint32(
			buffer[*pos:],
			math.Float32bits(v.float32Val),
		)
		*pos += 4

	case ValueTypeFloat64:
		binary.LittleEndian.PutUint64(
			buffer[*pos:],
			math.Float64bits(v.float64Val),
		)
		*pos += 8

	case ValueTypeString, ValueTypeJSONString:
		strBytes := []byte(v.stringVal)
		strLen := uint32(len(strBytes))

		binary.LittleEndian.PutUint32(buffer[*pos:], strLen)
		*pos += 4

		if strLen > 0 {
			copy(buffer[*pos:], strBytes)
			*pos += int(strLen)
		}

	case ValueTypeBytes:
		bufLen := uint32(len(v.bytesVal))

		binary.LittleEndian.PutUint32(buffer[*pos:], bufLen)
		*pos += 4

		if bufLen > 0 {
			copy(buffer[*pos:], v.bytesVal)
			*pos += int(bufLen)
		}

	case ValueTypeArray:
		arrayLen := uint32(len(v.arrayVal))
		binary.LittleEndian.PutUint32(buffer[*pos:], arrayLen)
		*pos += 4

		for _, item := range v.arrayVal {
			buffer[*pos] = valueTypeToBufferType(item.Type)
			*pos++

			err := item.serializeContent(buffer, pos)
			if err != nil {
				return err
			}
		}

	case ValueTypeObject:
		objSize := uint32(len(v.objectVal))
		binary.LittleEndian.PutUint32(buffer[*pos:], objSize)
		*pos += 4

		for key, val := range v.objectVal {
			// Write key
			keyBytes := []byte(key)
			keyLen := uint32(len(keyBytes))

			binary.LittleEndian.PutUint32(buffer[*pos:], keyLen)
			*pos += 4
			copy(buffer[*pos:], keyBytes)
			*pos += int(keyLen)

			// Write value type and content
			buffer[*pos] = valueTypeToBufferType(val.Type)
			*pos++

			err := val.serializeContent(buffer, pos)
			if err != nil {
				return err
			}
		}

	default:
		return newBufferProtocolError(ErrorCodeUnsupportedValueType,
			"unsupported value type for serialization")
	}

	return nil
}

// validateBufferHeader validates the buffer header and returns header info
func validateBufferHeader(buffer []byte) (*valueBufferHeader, error) {
	if len(buffer) < valueBufferHeaderSize {
		return nil, newBufferProtocolError(ErrorCodeInvalidBufferSize,
			"buffer too small to contain header")
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
		return nil, newBufferProtocolError(ErrorCodeInvalidMagicNumber,
			"invalid buffer magic number")
	}

	if header.version != valueBufferVersion {
		return nil, newBufferProtocolError(ErrorCodeUnsupportedVersion,
			"unsupported buffer protocol version")
	}

	if bufferTypeToValueType(header.typeName) == valueTypeInvalid {
		return nil, newBufferProtocolError(ErrorCodeInvalidBufferType,
			"invalid or unknown buffer type")
	}

	if uint32(len(buffer)) < uint32(valueBufferHeaderSize)+header.size {
		return nil, newBufferProtocolError(ErrorCodeBufferSizeMismatch,
			"buffer size doesn't match header specification")
	}

	return header, nil
}

// deserializeFromBuffer deserializes a Value from buffer
func deserializeFromBuffer(buffer []byte) (*Value, int, error) {
	header, err := validateBufferHeader(buffer)
	if err != nil {
		return nil, 0, err
	}

	pos := valueBufferHeaderSize
	value, err := deserializeContent(
		buffer,
		&pos,
		bufferTypeToValueType(header.typeName),
	)
	if err != nil {
		return nil, 0, err
	}

	return value, pos, nil
}

// deserializeContent deserializes value content from buffer
func deserializeContent(
	buffer []byte,
	pos *int,
	valueType ValueType,
) (*Value, error) {
	value := &Value{Type: valueType}

	switch valueType {
	case valueTypeInvalid:
		// No additional data

	case ValueTypeBool:
		if *pos >= len(buffer) {
			return nil, newBufferProtocolError(ErrorCodeInvalidBufferSize,
				"buffer too small for bool value")
		}
		value.boolVal = buffer[*pos] != 0
		*pos++

	case ValueTypeInt8:
		if *pos >= len(buffer) {
			return nil, newBufferProtocolError(ErrorCodeInvalidBufferSize,
				"buffer too small for int8 value")
		}
		value.int8Val = int8(buffer[*pos])
		*pos++

	case ValueTypeInt16:
		if *pos+2 > len(buffer) {
			return nil, newBufferProtocolError(ErrorCodeInvalidBufferSize,
				"buffer too small for int16 value")
		}
		value.int16Val = int16(binary.LittleEndian.Uint16(buffer[*pos:]))
		*pos += 2

	case ValueTypeInt32:
		if *pos+4 > len(buffer) {
			return nil, newBufferProtocolError(ErrorCodeInvalidBufferSize,
				"buffer too small for int32 value")
		}
		value.int32Val = int32(binary.LittleEndian.Uint32(buffer[*pos:]))
		*pos += 4

	case ValueTypeInt64, ValueTypeInt:
		if *pos+8 > len(buffer) {
			return nil, newBufferProtocolError(ErrorCodeInvalidBufferSize,
				"buffer too small for int64 value")
		}
		value.int64Val = int64(binary.LittleEndian.Uint64(buffer[*pos:]))
		*pos += 8

	case ValueTypeUint8:
		if *pos >= len(buffer) {
			return nil, newBufferProtocolError(ErrorCodeInvalidBufferSize,
				"buffer too small for uint8 value")
		}
		value.uint8Val = buffer[*pos]
		*pos++

	case ValueTypeUint16:
		if *pos+2 > len(buffer) {
			return nil, newBufferProtocolError(ErrorCodeInvalidBufferSize,
				"buffer too small for uint16 value")
		}
		value.uint16Val = binary.LittleEndian.Uint16(buffer[*pos:])
		*pos += 2

	case ValueTypeUint32:
		if *pos+4 > len(buffer) {
			return nil, newBufferProtocolError(ErrorCodeInvalidBufferSize,
				"buffer too small for uint32 value")
		}
		value.uint32Val = binary.LittleEndian.Uint32(buffer[*pos:])
		*pos += 4

	case ValueTypeUint64, ValueTypeUint:
		if *pos+8 > len(buffer) {
			return nil, newBufferProtocolError(ErrorCodeInvalidBufferSize,
				"buffer too small for uint64 value")
		}
		value.uint64Val = binary.LittleEndian.Uint64(buffer[*pos:])
		*pos += 8

	case ValueTypeFloat32:
		if *pos+4 > len(buffer) {
			return nil, newBufferProtocolError(ErrorCodeInvalidBufferSize,
				"buffer too small for float32 value")
		}
		value.float32Val = math.Float32frombits(
			binary.LittleEndian.Uint32(buffer[*pos:]),
		)
		*pos += 4

	case ValueTypeFloat64:
		if *pos+8 > len(buffer) {
			return nil, newBufferProtocolError(ErrorCodeInvalidBufferSize,
				"buffer too small for float64 value")
		}
		value.float64Val = math.Float64frombits(
			binary.LittleEndian.Uint64(buffer[*pos:]),
		)
		*pos += 8

	case ValueTypeString, ValueTypeJSONString:
		if *pos+4 > len(buffer) {
			return nil, newBufferProtocolError(ErrorCodeInvalidBufferSize,
				"buffer too small for string length")
		}
		strLen := binary.LittleEndian.Uint32(buffer[*pos:])
		*pos += 4

		if strLen == 0 {
			value.stringVal = ""
		} else {
			if *pos+int(strLen) > len(buffer) {
				return nil, newBufferProtocolError(ErrorCodeInvalidBufferSize,
					"buffer too small for string data")
			}
			value.stringVal = string(buffer[*pos : *pos+int(strLen)])
			*pos += int(strLen)
		}

	case ValueTypeBytes:
		if *pos+4 > len(buffer) {
			return nil, newBufferProtocolError(ErrorCodeInvalidBufferSize,
				"buffer too small for bytes length")
		}
		bufLen := binary.LittleEndian.Uint32(buffer[*pos:])
		*pos += 4

		if bufLen == 0 {
			value.bytesVal = nil
		} else {
			if *pos+int(bufLen) > len(buffer) {
				return nil, newBufferProtocolError(ErrorCodeInvalidBufferSize,
					"buffer too small for bytes data")
			}
			value.bytesVal = make([]byte, bufLen)
			copy(value.bytesVal, buffer[*pos:*pos+int(bufLen)])
			*pos += int(bufLen)
		}

	case ValueTypeArray:
		if *pos+4 > len(buffer) {
			return nil, newBufferProtocolError(ErrorCodeInvalidBufferSize,
				"buffer too small for array length")
		}
		arrayLen := binary.LittleEndian.Uint32(buffer[*pos:])
		*pos += 4

		value.arrayVal = make([]Value, arrayLen)
		for i := uint32(0); i < arrayLen; i++ {
			if *pos >= len(buffer) {
				return nil, newBufferProtocolError(ErrorCodeInvalidBufferSize,
					"buffer too small for array item type")
			}
			itemType := bufferTypeToValueType(buffer[*pos])
			*pos++

			item, err := deserializeContent(buffer, pos, itemType)
			if err != nil {
				return nil, err
			}
			value.arrayVal[i] = *item
		}

	case ValueTypeObject:
		if *pos+4 > len(buffer) {
			return nil, newBufferProtocolError(ErrorCodeInvalidBufferSize,
				"buffer too small for object size")
		}
		objSize := binary.LittleEndian.Uint32(buffer[*pos:])
		*pos += 4

		value.objectVal = make(map[string]Value)
		for i := uint32(0); i < objSize; i++ {
			// Read key
			if *pos+4 > len(buffer) {
				return nil, newBufferProtocolError(ErrorCodeInvalidBufferSize,
					"buffer too small for object key length")
			}
			keyLen := binary.LittleEndian.Uint32(buffer[*pos:])
			*pos += 4

			if *pos+int(keyLen) > len(buffer) {
				return nil, newBufferProtocolError(ErrorCodeInvalidBufferSize,
					"buffer too small for object key data")
			}
			key := string(buffer[*pos : *pos+int(keyLen)])
			*pos += int(keyLen)

			// Read value
			if *pos >= len(buffer) {
				return nil, newBufferProtocolError(ErrorCodeInvalidBufferSize,
					"buffer too small for object value type")
			}
			valType := bufferTypeToValueType(buffer[*pos])
			*pos++

			val, err := deserializeContent(buffer, pos, valType)
			if err != nil {
				return nil, err
			}
			value.objectVal[key] = *val
		}

	default:
		return nil, newBufferProtocolError(ErrorCodeUnsupportedValueType,
			"unsupported value type for deserialization")
	}

	return value, nil
}
