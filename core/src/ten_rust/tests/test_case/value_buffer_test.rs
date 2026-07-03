//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

use ten_rust::value_buffer::{deserialize_from_buffer, Value};

#[test]
fn test_deserialize_bool() {
    // Header: magic=0x010E, version=1, type=1 (bool), size=1
    // Content: 1 (true)
    let buffer = vec![0x0E, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x01];
    let value = deserialize_from_buffer(&buffer).unwrap();
    match value {
        Value::Bool(true) => {}
        _ => panic!("Expected Bool(true)"),
    }
}

#[test]
fn test_deserialize_int64() {
    // Header: magic=0x010E, version=1, type=5 (int64), size=8
    // Content: 42 as int64
    let buffer = vec![
        0x0E, 0x01, 0x01, 0x05, 0x08, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00,
    ];
    let value = deserialize_from_buffer(&buffer).unwrap();
    match value {
        Value::Int64(42) => {}
        _ => panic!("Expected Int64(42)"),
    }
}

#[test]
fn test_deserialize_string() {
    // Header: magic=0x010E, version=1, type=12 (string), size=9
    // Content: length=5, "hello"
    let buffer = vec![
        0x0E, 0x01, 0x01, 0x0C, 0x09, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, b'h', b'e', b'l',
        b'l', b'o',
    ];
    let value = deserialize_from_buffer(&buffer).unwrap();
    match value {
        Value::String(s) if s == "hello" => {}
        _ => panic!("Expected String(\"hello\")"),
    }
}

#[test]
fn test_deserialize_object() {
    // Create a test buffer for an object with:
    // {"user_id": 12345 (int64), "name": "test" (string)}

    // Header: magic=0x010E, version=1, type=15 (object), size will be calculated
    // Object size: 2 items
    // Key 1: "user_id" (7 bytes)
    // Value 1: type=5 (int64), value=12345
    // Key 2: "name" (4 bytes)
    // Value 2: type=12 (string), length=4, "test"

    let mut buffer = vec![
        0x0E, 0x01, 0x01, 0x0F, // Header: magic, version, type=object
        0x00, 0x00, 0x00, 0x00, // size placeholder
        0x02, 0x00, 0x00, 0x00, // object size: 2
    ];

    // Key 1: "user_id"
    buffer.extend_from_slice(&[0x07, 0x00, 0x00, 0x00]); // key length: 7
    buffer.extend_from_slice(b"user_id");
    buffer.push(0x05); // value type: int64
    buffer.extend_from_slice(&12345i64.to_le_bytes());

    // Key 2: "name"
    buffer.extend_from_slice(&[0x04, 0x00, 0x00, 0x00]); // key length: 4
    buffer.extend_from_slice(b"name");
    buffer.push(0x0C); // value type: string
    buffer.extend_from_slice(&[0x04, 0x00, 0x00, 0x00]); // string length: 4
    buffer.extend_from_slice(b"test");

    // Update size in header
    let content_size = (buffer.len() - 8) as u32;
    buffer[4..8].copy_from_slice(&content_size.to_le_bytes());

    let value = deserialize_from_buffer(&buffer).unwrap();
    match value {
        Value::Object(obj) => {
            assert_eq!(obj.len(), 2);
            match obj.get("user_id") {
                Some(Value::Int64(12345)) => {}
                _ => panic!("Expected user_id to be Int64(12345)"),
            }
            match obj.get("name") {
                Some(Value::String(s)) if s == "test" => {}
                _ => panic!("Expected name to be String(\"test\")"),
            }
        }
        _ => panic!("Expected Object"),
    }
}

#[test]
fn test_deserialize_array() {
    // Create a test buffer for an array: [1, 2, 3] (int64)
    let mut buffer = vec![
        0x0E, 0x01, 0x01, 0x0E, // Header: magic, version, type=array
        0x00, 0x00, 0x00, 0x00, // size placeholder
        0x03, 0x00, 0x00, 0x00, // array length: 3
    ];

    // Item 1
    buffer.push(0x05); // type: int64
    buffer.extend_from_slice(&1i64.to_le_bytes());

    // Item 2
    buffer.push(0x05); // type: int64
    buffer.extend_from_slice(&2i64.to_le_bytes());

    // Item 3
    buffer.push(0x05); // type: int64
    buffer.extend_from_slice(&3i64.to_le_bytes());

    // Update size in header
    let content_size = (buffer.len() - 8) as u32;
    buffer[4..8].copy_from_slice(&content_size.to_le_bytes());

    let value = deserialize_from_buffer(&buffer).unwrap();
    match value {
        Value::Array(arr) => {
            assert_eq!(arr.len(), 3);
            for (i, val) in arr.iter().enumerate() {
                match val {
                    Value::Int64(v) if *v == (i as i64 + 1) => {}
                    _ => panic!("Expected Int64({})", i + 1),
                }
            }
        }
        _ => panic!("Expected Array"),
    }
}
