//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import assert from "assert";
import { Value, ValueType } from "./value.js";

// Buffer protocol constants - must match C layer
export const VALUE_BUFFER_MAGIC = 0x010e;
export const VALUE_BUFFER_VERSION = 1;
export const VALUE_BUFFER_HEADER_SIZE = 8; // magic(2) + version(1) + type(1) + size(4)

// Buffer type constants - must match TEN_VALUE_BUFFER_TYPE in C
export const BUFFER_TYPE_INVALID = 0;
export const BUFFER_TYPE_BOOL = 1;
export const BUFFER_TYPE_INT8 = 2;
export const BUFFER_TYPE_INT16 = 3;
export const BUFFER_TYPE_INT32 = 4;
export const BUFFER_TYPE_INT64 = 5;
export const BUFFER_TYPE_UINT8 = 6;
export const BUFFER_TYPE_UINT16 = 7;
export const BUFFER_TYPE_UINT32 = 8;
export const BUFFER_TYPE_UINT64 = 9;
export const BUFFER_TYPE_FLOAT32 = 10;
export const BUFFER_TYPE_FLOAT64 = 11;
export const BUFFER_TYPE_STRING = 12;
export const BUFFER_TYPE_BUF = 13;
export const BUFFER_TYPE_ARRAY = 14;
export const BUFFER_TYPE_OBJECT = 15;
export const BUFFER_TYPE_PTR = 16;
export const BUFFER_TYPE_JSON_STRING = 17;

// Represents the buffer header structure.
export interface ValueBufferHeader {
  magic: number;
  version: number;
  typeId: number;
  size: number;
}

// Convert Node.js ValueType to buffer type.
function valueTypeToBufferType(valueType: ValueType): number {
  const mapping: Record<ValueType, number> = {
    [ValueType.INVALID]: BUFFER_TYPE_INVALID,
    [ValueType.BOOLEAN]: BUFFER_TYPE_BOOL,
    [ValueType.NUMBER]: BUFFER_TYPE_FLOAT64, // JavaScript number maps to float64
    [ValueType.STRING]: BUFFER_TYPE_STRING,
    [ValueType.BYTES]: BUFFER_TYPE_BUF,
    [ValueType.ARRAY]: BUFFER_TYPE_ARRAY,
    [ValueType.OBJECT]: BUFFER_TYPE_OBJECT,
    [ValueType.JSON_STRING]: BUFFER_TYPE_JSON_STRING,
  };
  return mapping[valueType] ?? BUFFER_TYPE_INVALID;
}

// Convert buffer type to Node.js ValueType.
function bufferTypeToValueType(bufferType: number): ValueType {
  const mapping: Record<number, ValueType> = {
    [BUFFER_TYPE_INVALID]: ValueType.INVALID,
    [BUFFER_TYPE_BOOL]: ValueType.BOOLEAN,
    [BUFFER_TYPE_INT8]: ValueType.NUMBER,
    [BUFFER_TYPE_INT16]: ValueType.NUMBER,
    [BUFFER_TYPE_INT32]: ValueType.NUMBER,
    [BUFFER_TYPE_INT64]: ValueType.NUMBER,
    [BUFFER_TYPE_UINT8]: ValueType.NUMBER,
    [BUFFER_TYPE_UINT16]: ValueType.NUMBER,
    [BUFFER_TYPE_UINT32]: ValueType.NUMBER,
    [BUFFER_TYPE_UINT64]: ValueType.NUMBER,
    [BUFFER_TYPE_FLOAT32]: ValueType.NUMBER,
    [BUFFER_TYPE_FLOAT64]: ValueType.NUMBER,
    [BUFFER_TYPE_STRING]: ValueType.STRING,
    [BUFFER_TYPE_BUF]: ValueType.BYTES,
    [BUFFER_TYPE_ARRAY]: ValueType.ARRAY,
    [BUFFER_TYPE_OBJECT]: ValueType.OBJECT,
    [BUFFER_TYPE_JSON_STRING]: ValueType.JSON_STRING,
  };
  return mapping[bufferType] ?? ValueType.INVALID;
}

// Calculate the size needed for the value content.
function calculateContentSize(value: Value): number {
  const valueType = value.type;

  if (valueType === ValueType.INVALID) {
    return 0;
  }

  if (valueType === ValueType.BOOLEAN) {
    return 1;
  }

  if (valueType === ValueType.NUMBER) {
    return 8; // Always serialize as float64
  }

  if (valueType === ValueType.STRING || valueType === ValueType.JSON_STRING) {
    const data =
      value.type === ValueType.STRING
        ? value.getString()
        : value.getJsonString();
    const encoded = Buffer.from(data, "utf-8");
    return 4 + encoded.length; // length(4) + data
  }

  if (valueType === ValueType.BYTES) {
    const data = value.getBytes();
    return 4 + data.length; // length(4) + data
  }

  if (valueType === ValueType.ARRAY) {
    let size = 4; // array length
    for (const item of value.getArray()) {
      size += 1; // item type
      size += calculateContentSize(item);
    }
    return size;
  }

  if (valueType === ValueType.OBJECT) {
    let size = 4; // object size
    for (const [key, val] of Object.entries(value.getObject()) as [
      string,
      Value,
    ][]) {
      const keyBytes = Buffer.from(key, "utf-8");
      size += 4 + keyBytes.length; // key length + key data
      size += 1; // value type
      size += calculateContentSize(val);
    }
    return size;
  }

  return 0;
}

// Serialize the value content to buffer. Returns new position.
function serializeContent(value: Value, buffer: Buffer, pos: number): number {
  const valueType = value.type;

  if (valueType === ValueType.INVALID) {
    // No additional data
    return pos;
  }

  if (valueType === ValueType.BOOLEAN) {
    const val = value.getBoolean() ? 1 : 0;
    buffer.writeUInt8(val, pos);
    return pos + 1;
  }

  if (valueType === ValueType.NUMBER) {
    // Always serialize as float64
    const val = value.getNumber();
    buffer.writeDoubleLE(val, pos);
    return pos + 8;
  }

  if (valueType === ValueType.STRING || valueType === ValueType.JSON_STRING) {
    const data =
      valueType === ValueType.STRING
        ? value.getString()
        : value.getJsonString();
    const encoded = Buffer.from(data, "utf-8");
    const dataLen = encoded.length;

    buffer.writeUInt32LE(dataLen, pos);
    pos += 4;

    if (dataLen > 0) {
      encoded.copy(buffer, pos);
      pos += dataLen;
    }

    return pos;
  }

  if (valueType === ValueType.BYTES) {
    const data = value.getBytes();
    const dataLen = data.length;

    buffer.writeUInt32LE(dataLen, pos);
    pos += 4;

    if (dataLen > 0) {
      buffer.set(data, pos);
      pos += dataLen;
    }

    return pos;
  }

  if (valueType === ValueType.ARRAY) {
    const arrayData = value.getArray();
    const arrayLen = arrayData.length;
    buffer.writeUInt32LE(arrayLen, pos);
    pos += 4;

    for (const item of arrayData) {
      const itemType = valueTypeToBufferType(item.type);
      buffer.writeUInt8(itemType, pos);
      pos += 1;

      pos = serializeContent(item, buffer, pos);
    }

    return pos;
  }

  if (valueType === ValueType.OBJECT) {
    const objData = value.getObject();
    const objSize = Object.keys(objData).length;
    buffer.writeUInt32LE(objSize, pos);
    pos += 4;

    for (const [key, val] of Object.entries(objData)) {
      // Write key
      const keyBytes = Buffer.from(key, "utf-8");
      const keyLen = keyBytes.length;

      buffer.writeUInt32LE(keyLen, pos);
      pos += 4;
      keyBytes.copy(buffer, pos);
      pos += keyLen;

      // Write value type and content
      const valType = valueTypeToBufferType(val.type);
      buffer.writeUInt8(valType, pos);
      pos += 1;

      pos = serializeContent(val, buffer, pos);
    }

    return pos;
  }

  assert(
    false,
    `Unsupported value type for serialization: ${ValueType[valueType]}`,
  );
}

// Serialize a Value to a buffer using only Node.js operations.
export function serializeToBuffer(value: Value): Buffer {
  const contentSize = calculateContentSize(value);
  const totalSize = VALUE_BUFFER_HEADER_SIZE + contentSize;
  const buffer = Buffer.alloc(totalSize);

  let pos = 0;

  // Write header
  const header: ValueBufferHeader = {
    magic: VALUE_BUFFER_MAGIC,
    version: VALUE_BUFFER_VERSION,
    typeId: valueTypeToBufferType(value.type),
    size: contentSize,
  };

  buffer.writeUInt16LE(header.magic, pos);
  pos += 2;
  buffer.writeUInt8(header.version, pos);
  pos += 1;
  buffer.writeUInt8(header.typeId, pos);
  pos += 1;
  buffer.writeUInt32LE(header.size, pos);
  pos += 4;

  // Write content
  const finalPos = serializeContent(value, buffer, pos);

  if (finalPos !== totalSize) {
    assert(
      false,
      `Buffer size mismatch: expected ${totalSize}, got ${finalPos}`,
    );
  }

  return buffer;
}

// Validate the buffer header and return header info.
function validateBufferHeader(buffer: Buffer): ValueBufferHeader {
  if (buffer.length < VALUE_BUFFER_HEADER_SIZE) {
    assert(false, "Buffer too small to contain header");
  }

  const magic = buffer.readUInt16LE(0);
  const version = buffer.readUInt8(2);
  const typeId = buffer.readUInt8(3);
  const size = buffer.readUInt32LE(4);

  if (magic !== VALUE_BUFFER_MAGIC) {
    assert(false, "Invalid buffer magic number");
  }

  if (version !== VALUE_BUFFER_VERSION) {
    assert(false, "Unsupported buffer protocol version");
  }

  if (bufferTypeToValueType(typeId) === ValueType.INVALID) {
    assert(false, "Invalid or unknown buffer type");
  }

  if (buffer.length < VALUE_BUFFER_HEADER_SIZE + size) {
    assert(false, "Buffer size doesn't match header specification");
  }

  return { magic, version, typeId, size };
}

// Deserialize value content from buffer. Returns [value, newPosition].
function deserializeContent(
  buffer: Buffer,
  pos: number,
  valueType: ValueType,
): [Value, number] {
  if (valueType === ValueType.INVALID) {
    assert(false, "Invalid value type");
  }

  if (valueType === ValueType.BOOLEAN) {
    if (pos >= buffer.length) {
      assert(false, "Buffer too small for bool value");
    }
    const val = buffer.readUInt8(pos);
    return [Value.createBoolean(val !== 0), pos + 1];
  }

  if (valueType === ValueType.NUMBER) {
    if (pos + 8 > buffer.length) {
      assert(false, "Buffer too small for number value");
    }
    const val = buffer.readDoubleLE(pos);
    return [Value.createNumber(val), pos + 8];
  }

  if (valueType === ValueType.STRING || valueType === ValueType.JSON_STRING) {
    if (pos + 4 > buffer.length) {
      assert(false, "Buffer too small for string length");
    }
    const strLen = buffer.readUInt32LE(pos);
    pos += 4;

    let data: string;
    if (strLen === 0) {
      data = "";
    } else {
      if (pos + strLen > buffer.length) {
        assert(false, "Buffer too small for string data");
      }
      data = buffer.subarray(pos, pos + strLen).toString("utf-8");
      pos += strLen;
    }

    if (valueType === ValueType.STRING) {
      return [Value.createString(data), pos];
    } else {
      return [Value.createJsonString(data), pos];
    }
  }

  if (valueType === ValueType.BYTES) {
    if (pos + 4 > buffer.length) {
      assert(false, "Buffer too small for bytes length");
    }
    const bufLen = buffer.readUInt32LE(pos);
    pos += 4;

    let data: Uint8Array;
    if (bufLen === 0) {
      data = new Uint8Array(0);
    } else {
      if (pos + bufLen > buffer.length) {
        assert(false, "Buffer too small for bytes data");
      }
      data = new Uint8Array(buffer.subarray(pos, pos + bufLen));
      pos += bufLen;
    }

    return [Value.createBytes(data), pos];
  }

  if (valueType === ValueType.ARRAY) {
    if (pos + 4 > buffer.length) {
      assert(false, "Buffer too small for array length");
    }
    const arrayLen = buffer.readUInt32LE(pos);
    pos += 4;

    const arrayData: Value[] = [];
    for (let i = 0; i < arrayLen; i++) {
      if (pos >= buffer.length) {
        assert(false, "Buffer too small for array item type");
      }
      const itemTypeId = buffer.readUInt8(pos);
      pos += 1;

      const itemType = bufferTypeToValueType(itemTypeId);
      const [item, newPos] = deserializeContent(buffer, pos, itemType);
      arrayData.push(item);
      pos = newPos;
    }

    return [Value.createArray(arrayData), pos];
  }

  if (valueType === ValueType.OBJECT) {
    if (pos + 4 > buffer.length) {
      assert(false, "Buffer too small for object size");
    }
    const objSize = buffer.readUInt32LE(pos);
    pos += 4;

    const objData: Record<string, Value> = {};
    for (let i = 0; i < objSize; i++) {
      // Read key
      if (pos + 4 > buffer.length) {
        assert(false, "Buffer too small for object key length");
      }
      const keyLen = buffer.readUInt32LE(pos);
      pos += 4;

      if (pos + keyLen > buffer.length) {
        assert(false, "Buffer too small for object key data");
      }
      const key = buffer.subarray(pos, pos + keyLen).toString("utf-8");
      pos += keyLen;

      // Read value
      if (pos >= buffer.length) {
        assert(false, "Buffer too small for object value type");
      }
      const valTypeId = buffer.readUInt8(pos);
      pos += 1;

      const valType = bufferTypeToValueType(valTypeId);
      const [val, newPos] = deserializeContent(buffer, pos, valType);
      objData[key] = val;
      pos = newPos;
    }

    return [Value.createObject(objData), pos];
  }

  assert(
    false,
    `Unsupported value type for deserialization: ${ValueType[valueType]}`,
  );
}

// Deserialize a Value from buffer.
export function deserializeFromBuffer(buffer: Buffer): [Value, number] {
  const header = validateBufferHeader(buffer);

  const pos = VALUE_BUFFER_HEADER_SIZE;
  const valueType = bufferTypeToValueType(header.typeId);

  const [value, finalPos] = deserializeContent(buffer, pos, valueType);

  return [value, finalPos];
}
