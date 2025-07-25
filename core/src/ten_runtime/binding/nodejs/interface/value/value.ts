//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

export enum ValueType {
  INVALID = 0,
  BOOLEAN = 1,
  NUMBER = 2,
  STRING = 3,
  BYTES = 4,
  ARRAY = 5,
  OBJECT = 6,
  JSON_STRING = 7,
}

type ValueDataType =
  | string
  | number
  | boolean
  | Uint8Array
  | Value[]
  | Record<string, Value>;

export class Value {
  private _type: ValueType;
  private _data: ValueDataType;

  private constructor(type: ValueType, data: ValueDataType) {
    this._type = type;
    this._data = data;
  }

  // Create a boolean Value.
  static createBoolean(value: boolean): Value {
    return new Value(ValueType.BOOLEAN, value);
  }

  // Create a number Value.
  static createNumber(value: number): Value {
    return new Value(ValueType.NUMBER, value);
  }

  // Create a string Value.
  static createString(value: string): Value {
    return new Value(ValueType.STRING, value);
  }

  // Create a bytes Value from Uint8Array.
  static createBytes(value: Uint8Array): Value {
    return new Value(ValueType.BYTES, value);
  }

  // Create an array Value.
  static createArray(value: Value[]): Value {
    return new Value(ValueType.ARRAY, value);
  }

  // Create an object Value.
  static createObject(value: Record<string, Value>): Value {
    return new Value(ValueType.OBJECT, value);
  }

  // Create a JSON string Value.
  static createJsonString(value: string): Value {
    return new Value(ValueType.JSON_STRING, value);
  }

  // Get the type of this Value.
  get type(): ValueType {
    return this._type;
  }

  // Get the boolean value. Throws Error if not a boolean.
  getBoolean(): boolean {
    if (this._type !== ValueType.BOOLEAN) {
      throw new Error(`Value is not a boolean, got ${ValueType[this._type]}`);
    }
    return this._data as boolean;
  }

  // Get the number value. Throws Error if not a number.
  getNumber(): number {
    if (this._type !== ValueType.NUMBER) {
      throw new Error(`Value is not a number, got ${ValueType[this._type]}`);
    }
    return this._data as number;
  }

  // Get the string value. Throws Error if not a string.
  getString(): string {
    if (this._type !== ValueType.STRING) {
      throw new Error(`Value is not a string, got ${ValueType[this._type]}`);
    }
    return this._data as string;
  }

  // Get the bytes value. Throws Error if not bytes.
  getBytes(): Uint8Array {
    if (this._type !== ValueType.BYTES) {
      throw new Error(`Value is not bytes, got ${ValueType[this._type]}`);
    }
    return this._data as Uint8Array;
  }

  // Get the array value. Throws Error if not an array.
  getArray(): Value[] {
    if (this._type !== ValueType.ARRAY) {
      throw new Error(`Value is not an array, got ${ValueType[this._type]}`);
    }
    return this._data as Value[];
  }

  // Get the object value. Throws Error if not an object.
  getObject(): Record<string, Value> {
    if (this._type !== ValueType.OBJECT) {
      throw new Error(`Value is not an object, got ${ValueType[this._type]}`);
    }
    return this._data as Record<string, Value>;
  }

  // Get the JSON string value. Throws Error if not a JSON string.
  getJsonString(): string {
    if (this._type !== ValueType.JSON_STRING) {
      throw new Error(
        `Value is not a JSON string, got ${ValueType[this._type]}`,
      );
    }
    return this._data as string;
  }
}
