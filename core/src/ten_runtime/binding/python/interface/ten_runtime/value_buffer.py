#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import struct
from typing import cast

from .value import Value, ValueType


# Buffer protocol constants - must match C layer
VALUE_BUFFER_MAGIC = 0x010E
VALUE_BUFFER_VERSION = 1
VALUE_BUFFER_HEADER_SIZE = 8  # magic(2) + version(1) + type(1) + size(4)

# Buffer type constants - must match TEN_VALUE_BUFFER_TYPE in C
BUFFER_TYPE_INVALID = 0
BUFFER_TYPE_BOOL = 1
BUFFER_TYPE_INT8 = 2
BUFFER_TYPE_INT16 = 3
BUFFER_TYPE_INT32 = 4
BUFFER_TYPE_INT64 = 5
BUFFER_TYPE_UINT8 = 6
BUFFER_TYPE_UINT16 = 7
BUFFER_TYPE_UINT32 = 8
BUFFER_TYPE_UINT64 = 9
BUFFER_TYPE_FLOAT32 = 10
BUFFER_TYPE_FLOAT64 = 11
BUFFER_TYPE_STRING = 12
BUFFER_TYPE_BUF = 13
BUFFER_TYPE_ARRAY = 14
BUFFER_TYPE_OBJECT = 15
BUFFER_TYPE_PTR = 16
BUFFER_TYPE_JSON_STRING = 17


class ValueBufferHeader:
    """Represents the buffer header structure."""

    magic: int
    version: int
    type_id: int
    size: int

    def __init__(self, magic: int, version: int, type_id: int, size: int):
        self.magic = magic
        self.version = version
        self.type_id = type_id
        self.size = size


def _value_type_to_buffer_type(value_type: ValueType) -> int:
    """Convert Python ValueType to buffer type."""
    mapping = {
        ValueType.INVALID: BUFFER_TYPE_INVALID,
        ValueType.BOOL: BUFFER_TYPE_BOOL,
        ValueType.INT: BUFFER_TYPE_INT64,  # Python int maps to int64
        ValueType.FLOAT: BUFFER_TYPE_FLOAT64,  # Python float maps to float64
        ValueType.STRING: BUFFER_TYPE_STRING,
        ValueType.BYTES: BUFFER_TYPE_BUF,
        ValueType.ARRAY: BUFFER_TYPE_ARRAY,
        ValueType.OBJECT: BUFFER_TYPE_OBJECT,
        ValueType.JSON_STRING: BUFFER_TYPE_JSON_STRING,
    }
    return mapping.get(value_type, BUFFER_TYPE_INVALID)


def _buffer_type_to_value_type(buffer_type: int) -> ValueType:
    """Convert buffer type to Python ValueType."""
    mapping = {
        BUFFER_TYPE_INVALID: ValueType.INVALID,
        BUFFER_TYPE_BOOL: ValueType.BOOL,
        BUFFER_TYPE_INT8: ValueType.INT,
        BUFFER_TYPE_INT16: ValueType.INT,
        BUFFER_TYPE_INT32: ValueType.INT,
        BUFFER_TYPE_INT64: ValueType.INT,
        BUFFER_TYPE_UINT8: ValueType.INT,
        BUFFER_TYPE_UINT16: ValueType.INT,
        BUFFER_TYPE_UINT32: ValueType.INT,
        BUFFER_TYPE_UINT64: ValueType.INT,
        BUFFER_TYPE_FLOAT32: ValueType.FLOAT,
        BUFFER_TYPE_FLOAT64: ValueType.FLOAT,
        BUFFER_TYPE_STRING: ValueType.STRING,
        BUFFER_TYPE_BUF: ValueType.BYTES,
        BUFFER_TYPE_ARRAY: ValueType.ARRAY,
        BUFFER_TYPE_OBJECT: ValueType.OBJECT,
        BUFFER_TYPE_JSON_STRING: ValueType.JSON_STRING,
    }
    return mapping.get(buffer_type, ValueType.INVALID)


def _calculate_content_size(value: Value) -> int:
    """Calculate the size needed for the value content."""
    value_type = value.get_type()

    match value_type:
        case ValueType.INVALID:
            assert False, "Invalid value type"

        case ValueType.BOOL:
            return 1

        case ValueType.INT:
            return 8  # Always serialize as int64

        case ValueType.FLOAT:
            return 8  # Always serialize as float64

        case ValueType.STRING:
            data = value.get_string()[0]
            encoded = data.encode("utf-8")
            return 4 + len(encoded)  # length(4) + data

        case ValueType.JSON_STRING:
            data = value.get_json_string()[0]
            encoded = data.encode("utf-8")
            return 4 + len(encoded)  # length(4) + data

        case ValueType.BYTES:
            data = value.get_buf()[0]
            return 4 + len(data)  # length(4) + data

        case ValueType.ARRAY:
            size = 4  # array length
            for item in value.get_array()[0]:
                size += 1  # item type
                size += _calculate_content_size(item)
            return size

        case ValueType.OBJECT:
            size = 4  # object size
            for key, val in value.get_object()[0].items():
                key_bytes = key.encode("utf-8")
                size += 4 + len(key_bytes)  # key length + key data
                size += 1  # value type
                size += _calculate_content_size(val)
            return size

        case _:  # pyright: ignore[reportUnnecessaryComparison]
            assert (  # pyright: ignore[reportUnreachable]
                False
            ), "Invalid value type"


def _serialize_content(value: Value, buffer: bytearray, pos: int) -> int:
    """Serialize the value content to buffer. Returns new position."""
    value_type = value.get_type()

    match value_type:
        case ValueType.INVALID:
            assert False, "Invalid value type"

        case ValueType.BOOL:
            val = 1 if value.get_bool()[0] else 0
            struct.pack_into("<B", buffer, pos, val)
            pos += 1

        case ValueType.INT:
            # Always serialize as int64
            val = value.get_int()[0]
            struct.pack_into("<q", buffer, pos, val)
            pos += 8

        case ValueType.FLOAT:
            # Always serialize as float64
            val = value.get_float()[0]
            struct.pack_into("<d", buffer, pos, val)
            pos += 8

        case ValueType.STRING:
            data = value.get_string()[0]
            encoded = data.encode("utf-8")
            data_len = len(encoded)
            struct.pack_into("<I", buffer, pos, data_len)
            pos += 4
            if data_len > 0:
                buffer[pos : pos + data_len] = encoded
                pos += data_len

        case ValueType.JSON_STRING:
            data = value.get_json_string()[0]
            encoded = data.encode("utf-8")
            data_len = len(encoded)
            struct.pack_into("<I", buffer, pos, data_len)
            pos += 4
            if data_len > 0:
                buffer[pos : pos + data_len] = encoded
                pos += data_len

        case ValueType.BYTES:
            data = value.get_buf()[0]
            data_len = len(data)
            struct.pack_into("<I", buffer, pos, data_len)
            pos += 4
            if data_len > 0:
                buffer[pos : pos + data_len] = data
                pos += data_len

        case ValueType.ARRAY:
            array_data = value.get_array()[0]
            array_len = len(array_data)
            struct.pack_into("<I", buffer, pos, array_len)
            pos += 4
            for item in array_data:
                item_type = _value_type_to_buffer_type(item.get_type())
                struct.pack_into("<B", buffer, pos, item_type)
                pos += 1
                pos = _serialize_content(item, buffer, pos)

        case ValueType.OBJECT:
            obj_data = value.get_object()[0]
            obj_size = len(obj_data)
            struct.pack_into("<I", buffer, pos, obj_size)
            pos += 4
            for key, val in obj_data.items():
                # Write key
                key_bytes = key.encode("utf-8")
                key_len = len(key_bytes)
                struct.pack_into("<I", buffer, pos, key_len)
                pos += 4
                buffer[pos : pos + key_len] = key_bytes
                pos += key_len
                # Write value type and content
                val_type = _value_type_to_buffer_type(val.get_type())
                struct.pack_into("<B", buffer, pos, val_type)
                pos += 1
                pos = _serialize_content(val, buffer, pos)

        case _:  # pyright: ignore[reportUnnecessaryComparison]
            assert (  # pyright: ignore[reportUnreachable]
                False
            ), f"Unsupported value type for serialization: {value_type}"

    return pos


def serialize_to_buffer(value: Value) -> bytes:
    """Serialize a Value to a buffer using only Python operations."""
    content_size = _calculate_content_size(value)
    total_size = VALUE_BUFFER_HEADER_SIZE + content_size
    buffer = bytearray(total_size)

    pos = 0

    # Write header
    header = ValueBufferHeader(
        magic=VALUE_BUFFER_MAGIC,
        version=VALUE_BUFFER_VERSION,
        type_id=_value_type_to_buffer_type(value.get_type()),
        size=content_size,
    )

    struct.pack_into(
        "<HBBcI",
        buffer,
        pos,
        header.magic,
        header.version,
        header.type_id,
        content_size,
    )
    pos += VALUE_BUFFER_HEADER_SIZE

    # Write content
    final_pos = _serialize_content(value, buffer, pos)

    if final_pos != total_size:
        assert (
            False
        ), f"Buffer size mismatch: expected {total_size}, got {final_pos}"

    return bytes(buffer)


def _validate_buffer_header(buffer: bytes) -> ValueBufferHeader:
    """Validate the buffer header and return header info."""
    if len(buffer) < VALUE_BUFFER_HEADER_SIZE:
        assert False, "Buffer too small to contain header"

    magic, version, type_id, size = cast(
        tuple[int, int, int, int], struct.unpack_from("<HBBIN", buffer, 0)
    )

    if magic != VALUE_BUFFER_MAGIC:
        assert False, "Invalid buffer magic number"

    if version != VALUE_BUFFER_VERSION:
        assert False, "Unsupported buffer protocol version"

    if _buffer_type_to_value_type(type_id) == ValueType.INVALID:
        assert False, "Invalid or unknown buffer type"

    if len(buffer) < VALUE_BUFFER_HEADER_SIZE + size:
        assert False, "Buffer size doesn't match header specification"

    return ValueBufferHeader(magic, version, type_id, size)


def _deserialize_content(
    buffer: bytes, pos: int, value_type: ValueType
) -> tuple[Value, int]:
    """Deserialize value content from buffer. Returns (value, new_position)."""

    match value_type:
        case ValueType.INVALID:
            assert False, "Invalid value type"

        case ValueType.BOOL:
            if pos >= len(buffer):
                assert False, "Buffer too small for bool value"
            val = cast(bool, struct.unpack_from("<B", buffer, pos)[0])
            return Value.from_bool(val != 0), pos + 1

        case ValueType.INT:
            if pos + 8 > len(buffer):
                assert False, "Buffer too small for int value"
            val = cast(int, struct.unpack_from("<q", buffer, pos)[0])
            return Value.from_int(val), pos + 8

        case ValueType.FLOAT:
            if pos + 8 > len(buffer):
                assert False, "Buffer too small for float value"
            val = cast(float, struct.unpack_from("<d", buffer, pos)[0])
            return Value.from_float(val), pos + 8

        case ValueType.STRING | ValueType.JSON_STRING:
            if pos + 4 > len(buffer):
                assert False, "Buffer too small for string length"
            str_len = cast(int, struct.unpack_from("<I", buffer, pos)[0])
            pos += 4

            if str_len == 0:
                data = ""
            else:
                if pos + str_len > len(buffer):
                    assert False, "Buffer too small for string data"
                data_bytes = buffer[pos : pos + str_len]
                data = data_bytes.decode("utf-8")
                pos += str_len

            if value_type == ValueType.STRING:
                return Value.from_string(data), pos
            else:
                return Value.from_json_string(data), pos

        case ValueType.BYTES:
            if pos + 4 > len(buffer):
                assert False, "Buffer too small for bytes length"
            buf_len = cast(int, struct.unpack_from("<I", buffer, pos)[0])
            pos += 4

            if buf_len == 0:
                data = b""
            else:
                if pos + buf_len > len(buffer):
                    assert False, "Buffer too small for bytes data"
                data = bytes(buffer[pos : pos + buf_len])
                pos += buf_len

            return Value.from_buf(data), pos

        case ValueType.ARRAY:
            if pos + 4 > len(buffer):
                assert False, "Buffer too small for array length"
            array_len = cast(int, struct.unpack_from("<I", buffer, pos)[0])
            pos += 4

            array_data: list[Value] = []
            for _ in range(array_len):
                if pos >= len(buffer):
                    assert False, "Buffer too small for array item type"
                item_type_id = cast(
                    int, struct.unpack_from("<B", buffer, pos)[0]
                )
                pos += 1

                item_type = _buffer_type_to_value_type(item_type_id)
                item, pos = _deserialize_content(buffer, pos, item_type)
                array_data.append(item)

            return Value.from_array(array_data), pos

        case ValueType.OBJECT:
            if pos + 4 > len(buffer):
                assert False, "Buffer too small for object size"
            obj_size = cast(int, struct.unpack_from("<I", buffer, pos)[0])
            pos += 4

            obj_data: dict[str, Value] = {}
            for _ in range(obj_size):
                # Read key
                if pos + 4 > len(buffer):
                    assert False, "Buffer too small for object key length"
                key_len = cast(int, struct.unpack_from("<I", buffer, pos)[0])
                pos += 4

                if pos + key_len > len(buffer):
                    assert False, "Buffer too small for object key data"
                key_bytes = buffer[pos : pos + key_len]
                key = key_bytes.decode("utf-8")
                pos += key_len

                # Read value
                if pos >= len(buffer):
                    assert False, "Buffer too small for object value type"
                val_type_id = cast(
                    int, struct.unpack_from("<B", buffer, pos)[0]
                )
                pos += 1

                val_type = _buffer_type_to_value_type(val_type_id)
                val, pos = _deserialize_content(buffer, pos, val_type)
                obj_data[key] = val

            return Value.from_object(obj_data), pos

        case _:  # pyright: ignore[reportUnnecessaryComparison]
            assert (  # pyright: ignore[reportUnreachable]
                False
            ), f"Unknown value type: {value_type}"


def deserialize_from_buffer(buffer: bytes) -> Value:
    """Deserialize a Value from buffer."""
    header = _validate_buffer_header(buffer)

    pos = VALUE_BUFFER_HEADER_SIZE
    value_type = _buffer_type_to_value_type(header.type_id)

    value, _ = _deserialize_content(buffer, pos, value_type)

    return value
