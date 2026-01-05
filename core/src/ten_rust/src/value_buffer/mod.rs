//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

use std::collections::HashMap;

use anyhow::{anyhow, Result};

// Buffer protocol constants - must match C layer
const VALUE_BUFFER_MAGIC: u16 = 0x010E;
const VALUE_BUFFER_VERSION: u8 = 1;
const VALUE_BUFFER_HEADER_SIZE: usize = 8;

// Buffer type constants - must match TEN_VALUE_BUFFER_TYPE in C
#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum BufferType {
    Invalid = 0,
    Bool = 1,
    Int8 = 2,
    Int16 = 3,
    Int32 = 4,
    Int64 = 5,
    Uint8 = 6,
    Uint16 = 7,
    Uint32 = 8,
    Uint64 = 9,
    Float32 = 10,
    Float64 = 11,
    String = 12,
    Buf = 13,
    Array = 14,
    Object = 15,
    Ptr = 16,
    JsonString = 17,
}

impl BufferType {
    fn from_u8(value: u8) -> Result<Self> {
        match value {
            0 => Ok(BufferType::Invalid),
            1 => Ok(BufferType::Bool),
            2 => Ok(BufferType::Int8),
            3 => Ok(BufferType::Int16),
            4 => Ok(BufferType::Int32),
            5 => Ok(BufferType::Int64),
            6 => Ok(BufferType::Uint8),
            7 => Ok(BufferType::Uint16),
            8 => Ok(BufferType::Uint32),
            9 => Ok(BufferType::Uint64),
            10 => Ok(BufferType::Float32),
            11 => Ok(BufferType::Float64),
            12 => Ok(BufferType::String),
            13 => Ok(BufferType::Buf),
            14 => Ok(BufferType::Array),
            15 => Ok(BufferType::Object),
            16 => Ok(BufferType::Ptr),
            17 => Ok(BufferType::JsonString),
            _ => Err(anyhow!("Unknown buffer type: {}", value)),
        }
    }
}

/// Represents a deserialized value from the buffer
#[derive(Debug, Clone)]
pub enum Value {
    Bool(bool),
    Int8(i8),
    Int16(i16),
    Int32(i32),
    Int64(i64),
    Uint8(u8),
    Uint16(u16),
    Uint32(u32),
    Uint64(u64),
    Float32(f32),
    Float64(f64),
    String(String),
    Buf(Vec<u8>),
    Array(Vec<Value>),
    Object(HashMap<String, Value>),
    JsonString(String),
}

impl Value {
    /// Check if this value is an Object type
    pub fn is_object(&self) -> bool {
        matches!(self, Value::Object(_))
    }

    /// Get the object if this is an Object type
    pub fn as_object(&self) -> Option<&HashMap<String, Value>> {
        match self {
            Value::Object(obj) => Some(obj),
            _ => None,
        }
    }

    /// Convert this Value to a serde_json::Value
    pub fn to_json(&self) -> serde_json::Value {
        match self {
            Value::Bool(x) => serde_json::Value::Bool(*x),
            Value::Int8(x) => serde_json::Value::Number((*x as i64).into()),
            Value::Int16(x) => serde_json::Value::Number((*x as i64).into()),
            Value::Int32(x) => serde_json::Value::Number((*x as i64).into()),
            Value::Int64(x) => serde_json::Value::Number((*x).into()),
            Value::Uint8(x) => serde_json::Value::Number((*x as u64).into()),
            Value::Uint16(x) => serde_json::Value::Number((*x as u64).into()),
            Value::Uint32(x) => serde_json::Value::Number((*x as u64).into()),
            Value::Uint64(x) => serde_json::Value::Number((*x).into()),
            Value::Float32(x) => serde_json::Number::from_f64(*x as f64)
                .map_or(serde_json::Value::Null, serde_json::Value::Number),
            Value::Float64(x) => serde_json::Number::from_f64(*x)
                .map_or(serde_json::Value::Null, serde_json::Value::Number),
            Value::String(s) => serde_json::Value::String(s.clone()),
            Value::Buf(b) => serde_json::Value::Array(
                b.iter().map(|x| serde_json::Value::Number((*x as u64).into())).collect(),
            ),
            Value::Array(arr) => {
                serde_json::Value::Array(arr.iter().map(|v| v.to_json()).collect())
            }
            Value::Object(obj) => {
                let mut map = serde_json::Map::with_capacity(obj.len());
                for (k, v) in obj.iter() {
                    map.insert(k.clone(), v.to_json());
                }
                serde_json::Value::Object(map)
            }
            Value::JsonString(s) => serde_json::from_str::<serde_json::Value>(s)
                .unwrap_or_else(|_| serde_json::Value::String(s.clone())),
        }
    }
}

#[derive(Debug)]
#[allow(dead_code)]
struct BufferHeader {
    magic: u16,
    version: u8,
    type_id: BufferType,
    size: u32,
}

impl BufferHeader {
    fn parse(buffer: &[u8]) -> Result<Self> {
        if buffer.len() < VALUE_BUFFER_HEADER_SIZE {
            return Err(anyhow!("Buffer too small to contain header"));
        }

        let magic = u16::from_le_bytes([buffer[0], buffer[1]]);
        let version = buffer[2];
        let type_id = BufferType::from_u8(buffer[3])?;
        let size = u32::from_le_bytes([buffer[4], buffer[5], buffer[6], buffer[7]]);

        if magic != VALUE_BUFFER_MAGIC {
            return Err(anyhow!(
                "Invalid buffer magic number: expected 0x{:04X}, got 0x{:04X}",
                VALUE_BUFFER_MAGIC,
                magic
            ));
        }

        if version != VALUE_BUFFER_VERSION {
            return Err(anyhow!(
                "Unsupported buffer protocol version: expected {}, got {}",
                VALUE_BUFFER_VERSION,
                version
            ));
        }

        if buffer.len() < VALUE_BUFFER_HEADER_SIZE + size as usize {
            return Err(anyhow!(
                "Buffer size doesn't match header specification: expected {}, got {}",
                VALUE_BUFFER_HEADER_SIZE + size as usize,
                buffer.len()
            ));
        }

        Ok(BufferHeader {
            magic,
            version,
            type_id,
            size,
        })
    }
}

/// Deserialize content from buffer based on type
fn deserialize_content(buffer: &[u8], pos: &mut usize, buffer_type: BufferType) -> Result<Value> {
    match buffer_type {
        BufferType::Invalid => Err(anyhow!("Cannot deserialize invalid type")),

        BufferType::Bool => {
            if *pos >= buffer.len() {
                return Err(anyhow!("Buffer too small for bool value"));
            }
            let val = buffer[*pos] != 0;
            *pos += 1;
            Ok(Value::Bool(val))
        }

        BufferType::Int8 => {
            if *pos >= buffer.len() {
                return Err(anyhow!("Buffer too small for int8 value"));
            }
            let val = buffer[*pos] as i8;
            *pos += 1;
            Ok(Value::Int8(val))
        }

        BufferType::Int16 => {
            if *pos + 2 > buffer.len() {
                return Err(anyhow!("Buffer too small for int16 value"));
            }
            let val = i16::from_le_bytes([buffer[*pos], buffer[*pos + 1]]);
            *pos += 2;
            Ok(Value::Int16(val))
        }

        BufferType::Int32 => {
            if *pos + 4 > buffer.len() {
                return Err(anyhow!("Buffer too small for int32 value"));
            }
            let val = i32::from_le_bytes([
                buffer[*pos],
                buffer[*pos + 1],
                buffer[*pos + 2],
                buffer[*pos + 3],
            ]);
            *pos += 4;
            Ok(Value::Int32(val))
        }

        BufferType::Int64 => {
            if *pos + 8 > buffer.len() {
                return Err(anyhow!("Buffer too small for int64 value"));
            }
            let val = i64::from_le_bytes([
                buffer[*pos],
                buffer[*pos + 1],
                buffer[*pos + 2],
                buffer[*pos + 3],
                buffer[*pos + 4],
                buffer[*pos + 5],
                buffer[*pos + 6],
                buffer[*pos + 7],
            ]);
            *pos += 8;
            Ok(Value::Int64(val))
        }

        BufferType::Uint8 => {
            if *pos >= buffer.len() {
                return Err(anyhow!("Buffer too small for uint8 value"));
            }
            let val = buffer[*pos];
            *pos += 1;
            Ok(Value::Uint8(val))
        }

        BufferType::Uint16 => {
            if *pos + 2 > buffer.len() {
                return Err(anyhow!("Buffer too small for uint16 value"));
            }
            let val = u16::from_le_bytes([buffer[*pos], buffer[*pos + 1]]);
            *pos += 2;
            Ok(Value::Uint16(val))
        }

        BufferType::Uint32 => {
            if *pos + 4 > buffer.len() {
                return Err(anyhow!("Buffer too small for uint32 value"));
            }
            let val = u32::from_le_bytes([
                buffer[*pos],
                buffer[*pos + 1],
                buffer[*pos + 2],
                buffer[*pos + 3],
            ]);
            *pos += 4;
            Ok(Value::Uint32(val))
        }

        BufferType::Uint64 => {
            if *pos + 8 > buffer.len() {
                return Err(anyhow!("Buffer too small for uint64 value"));
            }
            let val = u64::from_le_bytes([
                buffer[*pos],
                buffer[*pos + 1],
                buffer[*pos + 2],
                buffer[*pos + 3],
                buffer[*pos + 4],
                buffer[*pos + 5],
                buffer[*pos + 6],
                buffer[*pos + 7],
            ]);
            *pos += 8;
            Ok(Value::Uint64(val))
        }

        BufferType::Float32 => {
            if *pos + 4 > buffer.len() {
                return Err(anyhow!("Buffer too small for float32 value"));
            }
            let bytes = [buffer[*pos], buffer[*pos + 1], buffer[*pos + 2], buffer[*pos + 3]];
            let val = f32::from_le_bytes(bytes);
            *pos += 4;
            Ok(Value::Float32(val))
        }

        BufferType::Float64 => {
            if *pos + 8 > buffer.len() {
                return Err(anyhow!("Buffer too small for float64 value"));
            }
            let bytes = [
                buffer[*pos],
                buffer[*pos + 1],
                buffer[*pos + 2],
                buffer[*pos + 3],
                buffer[*pos + 4],
                buffer[*pos + 5],
                buffer[*pos + 6],
                buffer[*pos + 7],
            ];
            let val = f64::from_le_bytes(bytes);
            *pos += 8;
            Ok(Value::Float64(val))
        }

        BufferType::String | BufferType::JsonString => {
            if *pos + 4 > buffer.len() {
                return Err(anyhow!("Buffer too small for string length"));
            }
            let str_len = u32::from_le_bytes([
                buffer[*pos],
                buffer[*pos + 1],
                buffer[*pos + 2],
                buffer[*pos + 3],
            ]) as usize;
            *pos += 4;

            let data = if str_len == 0 {
                String::new()
            } else {
                if *pos + str_len > buffer.len() {
                    return Err(anyhow!("Buffer too small for string data"));
                }
                let string_bytes = &buffer[*pos..*pos + str_len];
                let data = String::from_utf8(string_bytes.to_vec())
                    .map_err(|e| anyhow!("Invalid UTF-8: {}", e))?;
                *pos += str_len;
                data
            };

            if buffer_type == BufferType::String {
                Ok(Value::String(data))
            } else {
                Ok(Value::JsonString(data))
            }
        }

        BufferType::Buf => {
            if *pos + 4 > buffer.len() {
                return Err(anyhow!("Buffer too small for buf length"));
            }
            let buf_len = u32::from_le_bytes([
                buffer[*pos],
                buffer[*pos + 1],
                buffer[*pos + 2],
                buffer[*pos + 3],
            ]) as usize;
            *pos += 4;

            let data = if buf_len == 0 {
                Vec::new()
            } else {
                if *pos + buf_len > buffer.len() {
                    return Err(anyhow!("Buffer too small for buf data"));
                }
                let data = buffer[*pos..*pos + buf_len].to_vec();
                *pos += buf_len;
                data
            };

            Ok(Value::Buf(data))
        }

        BufferType::Array => {
            if *pos + 4 > buffer.len() {
                return Err(anyhow!("Buffer too small for array length"));
            }
            let array_len = u32::from_le_bytes([
                buffer[*pos],
                buffer[*pos + 1],
                buffer[*pos + 2],
                buffer[*pos + 3],
            ]) as usize;
            *pos += 4;

            let mut array_data = Vec::with_capacity(array_len);
            for _ in 0..array_len {
                if *pos >= buffer.len() {
                    return Err(anyhow!("Buffer too small for array item type"));
                }
                let item_type = BufferType::from_u8(buffer[*pos])?;
                *pos += 1;

                let item = deserialize_content(buffer, pos, item_type)?;
                array_data.push(item);
            }

            Ok(Value::Array(array_data))
        }

        BufferType::Object => {
            if *pos + 4 > buffer.len() {
                return Err(anyhow!("Buffer too small for object size"));
            }
            let obj_size = u32::from_le_bytes([
                buffer[*pos],
                buffer[*pos + 1],
                buffer[*pos + 2],
                buffer[*pos + 3],
            ]) as usize;
            *pos += 4;

            let mut obj_data = HashMap::with_capacity(obj_size);
            for _ in 0..obj_size {
                // Read key
                if *pos + 4 > buffer.len() {
                    return Err(anyhow!("Buffer too small for object key length"));
                }
                let key_len = u32::from_le_bytes([
                    buffer[*pos],
                    buffer[*pos + 1],
                    buffer[*pos + 2],
                    buffer[*pos + 3],
                ]) as usize;
                *pos += 4;

                if *pos + key_len > buffer.len() {
                    return Err(anyhow!("Buffer too small for object key data"));
                }
                let key_bytes = &buffer[*pos..*pos + key_len];
                let key = String::from_utf8(key_bytes.to_vec())
                    .map_err(|e| anyhow!("Invalid UTF-8 in key: {}", e))?;
                *pos += key_len;

                // Read value
                if *pos >= buffer.len() {
                    return Err(anyhow!("Buffer too small for object value type"));
                }
                let val_type = BufferType::from_u8(buffer[*pos])?;
                *pos += 1;

                let val = deserialize_content(buffer, pos, val_type)?;
                obj_data.insert(key, val);
            }

            Ok(Value::Object(obj_data))
        }

        BufferType::Ptr => Err(anyhow!("Ptr type is not supported for deserialization")),
    }
}

/// Deserialize a Value from buffer
pub fn deserialize_from_buffer(buffer: &[u8]) -> Result<Value> {
    let header = BufferHeader::parse(buffer)?;

    let mut pos = VALUE_BUFFER_HEADER_SIZE;
    let value = deserialize_content(buffer, &mut pos, header.type_id)?;

    Ok(value)
}
