//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { TenError, TenErrorCode } from "./error.js";

export enum ValueType {
  INVALID = 0,
  NULL = 1,
  BOOLEAN = 2,
  NUMBER = 3,
  STRING = 4,
  BYTES = 5,
  ARRAY = 6,
  OBJECT = 7,
  JSON_STRING = 8,
}

type ValueDataType =
  | string
  | number
  | boolean
  | ArrayBuffer
  | Value[]
  | Record<string, Value>;

export class Value {
  private _type: ValueType;
  private _data: ValueDataType;

  private constructor(type: ValueType, data: ValueDataType) {
    this._type = type;
    this._data = data;
  }

  static fromBoolean(value: boolean): Value {
    return new Value(ValueType.BOOLEAN, value);
  }

  static fromNumber(value: number): Value {
    return new Value(ValueType.NUMBER, value);
  }

  static fromString(value: string): Value {
    return new Value(ValueType.STRING, value);
  }

  static fromBuf(value: ArrayBuffer): Value {
    return new Value(ValueType.BYTES, value);
  }

  static fromArray(value: Value[]): Value {
    return new Value(ValueType.ARRAY, value);
  }

  static fromObject(value: Record<string, Value>): Value {
    return new Value(ValueType.OBJECT, value);
  }

  static fromJsonString(value: string): Value {
    return new Value(ValueType.JSON_STRING, value);
  }

  getType(): ValueType {
    return this._type;
  }

  getBoolean(): [boolean, TenError | undefined] {
    if (this._type !== ValueType.BOOLEAN) {
      return [
        false,
        new TenError(
          TenErrorCode.ErrorCodeInvalidType,
          `Value is not a boolean, got ${ValueType[this._type]}`,
        ),
      ];
    }
    return [this._data as boolean, undefined];
  }

  getNumber(): [number, TenError | undefined] {
    if (this._type !== ValueType.NUMBER) {
      return [
        0,
        new TenError(
          TenErrorCode.ErrorCodeInvalidType,
          `Value is not a number, got ${ValueType[this._type]}`,
        ),
      ];
    }
    return [this._data as number, undefined];
  }

  getString(): [string, TenError | undefined] {
    if (this._type !== ValueType.STRING) {
      return [
        "",
        new TenError(
          TenErrorCode.ErrorCodeInvalidType,
          `Value is not a string, got ${ValueType[this._type]}`,
        ),
      ];
    }
    return [this._data as string, undefined];
  }

  getBuf(): [ArrayBuffer, TenError | undefined] {
    if (this._type !== ValueType.BYTES) {
      return [
        new ArrayBuffer(0),
        new TenError(
          TenErrorCode.ErrorCodeInvalidType,
          `Value is not bytes, got ${ValueType[this._type]}`,
        ),
      ];
    }
    return [this._data as ArrayBuffer, undefined];
  }

  getArray(): [Value[], TenError | undefined] {
    if (this._type !== ValueType.ARRAY) {
      return [
        [],
        new TenError(
          TenErrorCode.ErrorCodeInvalidType,
          `Value is not an array, got ${ValueType[this._type]}`,
        ),
      ];
    }
    return [this._data as Value[], undefined];
  }

  getObject(): [Record<string, Value>, TenError | undefined] {
    if (this._type !== ValueType.OBJECT) {
      return [
        {},
        new TenError(
          TenErrorCode.ErrorCodeInvalidType,
          `Value is not an object, got ${ValueType[this._type]}`,
        ),
      ];
    }
    return [this._data as Record<string, Value>, undefined];
  }

  getJsonString(): [string, TenError | undefined] {
    if (this._type !== ValueType.JSON_STRING) {
      return [
        "",
        new TenError(
          TenErrorCode.ErrorCodeInvalidType,
          `Value is not a JSON string, got ${ValueType[this._type]}`,
        ),
      ];
    }
    return [this._data as string, undefined];
  }
}
