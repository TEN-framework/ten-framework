//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { TenError, TenErrorCode } from "./error.js";
export var ValueType;
(function (ValueType) {
    ValueType[ValueType["INVALID"] = 0] = "INVALID";
    ValueType[ValueType["NULL"] = 1] = "NULL";
    ValueType[ValueType["BOOLEAN"] = 2] = "BOOLEAN";
    ValueType[ValueType["NUMBER"] = 3] = "NUMBER";
    ValueType[ValueType["STRING"] = 4] = "STRING";
    ValueType[ValueType["BYTES"] = 5] = "BYTES";
    ValueType[ValueType["ARRAY"] = 6] = "ARRAY";
    ValueType[ValueType["OBJECT"] = 7] = "OBJECT";
    ValueType[ValueType["JSON_STRING"] = 8] = "JSON_STRING";
})(ValueType || (ValueType = {}));
export class Value {
    _type;
    _data;
    constructor(type, data) {
        this._type = type;
        this._data = data;
    }
    static fromBoolean(value) {
        return new Value(ValueType.BOOLEAN, value);
    }
    static fromNumber(value) {
        return new Value(ValueType.NUMBER, value);
    }
    static fromString(value) {
        return new Value(ValueType.STRING, value);
    }
    static fromBuf(value) {
        return new Value(ValueType.BYTES, value);
    }
    static fromArray(value) {
        return new Value(ValueType.ARRAY, value);
    }
    static fromObject(value) {
        return new Value(ValueType.OBJECT, value);
    }
    static fromJsonString(value) {
        return new Value(ValueType.JSON_STRING, value);
    }
    /**
     * Convert native TypeScript/JavaScript types to Value object.
     * Supports: boolean, number, string, ArrayBuffer, Array, Object, null, undefined
     */
    static fromNative(value) {
        // If it's already a Value object, return a new instance
        if (value instanceof Value) {
            return new Value(value._type, value._data);
        }
        // Handle primitive types
        if (typeof value === "boolean") {
            return Value.fromBoolean(value);
        }
        if (typeof value === "number") {
            return Value.fromNumber(value);
        }
        if (typeof value === "string") {
            return Value.fromString(value);
        }
        // Handle ArrayBuffer
        if (value instanceof ArrayBuffer) {
            return Value.fromBuf(value);
        }
        // Handle arrays
        if (Array.isArray(value)) {
            return Value.fromArray(value.map((item) => Value.fromNative(item)));
        }
        // Handle objects (including null, which will be converted to string)
        if (typeof value === "object" && value !== null) {
            const obj = {};
            for (const [key, val] of Object.entries(value)) {
                obj[key] = Value.fromNative(val);
            }
            return Value.fromObject(obj);
        }
        // For other types (including null and undefined), convert to string
        // This matches Python's behavior where None is converted to string
        return Value.fromString(String(value));
    }
    getType() {
        return this._type;
    }
    getBoolean() {
        if (this._type !== ValueType.BOOLEAN) {
            return [
                false,
                new TenError(TenErrorCode.ErrorCodeInvalidType, `Value is not a boolean, got ${ValueType[this._type]}`),
            ];
        }
        return [this._data, undefined];
    }
    getNumber() {
        if (this._type !== ValueType.NUMBER) {
            return [
                0,
                new TenError(TenErrorCode.ErrorCodeInvalidType, `Value is not a number, got ${ValueType[this._type]}`),
            ];
        }
        return [this._data, undefined];
    }
    getString() {
        if (this._type !== ValueType.STRING) {
            return [
                "",
                new TenError(TenErrorCode.ErrorCodeInvalidType, `Value is not a string, got ${ValueType[this._type]}`),
            ];
        }
        return [this._data, undefined];
    }
    getBuf() {
        if (this._type !== ValueType.BYTES) {
            return [
                new ArrayBuffer(0),
                new TenError(TenErrorCode.ErrorCodeInvalidType, `Value is not bytes, got ${ValueType[this._type]}`),
            ];
        }
        return [this._data, undefined];
    }
    getArray() {
        if (this._type !== ValueType.ARRAY) {
            return [
                [],
                new TenError(TenErrorCode.ErrorCodeInvalidType, `Value is not an array, got ${ValueType[this._type]}`),
            ];
        }
        return [this._data, undefined];
    }
    getObject() {
        if (this._type !== ValueType.OBJECT) {
            return [
                {},
                new TenError(TenErrorCode.ErrorCodeInvalidType, `Value is not an object, got ${ValueType[this._type]}`),
            ];
        }
        return [this._data, undefined];
    }
    getJsonString() {
        if (this._type !== ValueType.JSON_STRING) {
            return [
                "",
                new TenError(TenErrorCode.ErrorCodeInvalidType, `Value is not a JSON string, got ${ValueType[this._type]}`),
            ];
        }
        return [this._data, undefined];
    }
}
//# sourceMappingURL=value.js.map