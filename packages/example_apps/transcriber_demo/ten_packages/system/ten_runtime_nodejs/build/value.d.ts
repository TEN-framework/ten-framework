import { TenError } from "./error.js";
export declare enum ValueType {
    INVALID = 0,
    NULL = 1,
    BOOLEAN = 2,
    NUMBER = 3,
    STRING = 4,
    BYTES = 5,
    ARRAY = 6,
    OBJECT = 7,
    JSON_STRING = 8
}
export declare class Value {
    private _type;
    private _data;
    private constructor();
    static fromBoolean(value: boolean): Value;
    static fromNumber(value: number): Value;
    static fromString(value: string): Value;
    static fromBuf(value: ArrayBuffer): Value;
    static fromArray(value: Value[]): Value;
    static fromObject(value: Record<string, Value>): Value;
    static fromJsonString(value: string): Value;
    /**
     * Convert native TypeScript/JavaScript types to Value object.
     * Supports: boolean, number, string, ArrayBuffer, Array, Object, null, undefined
     */
    static fromNative(value: unknown): Value;
    getType(): ValueType;
    getBoolean(): [boolean, TenError | undefined];
    getNumber(): [number, TenError | undefined];
    getString(): [string, TenError | undefined];
    getBuf(): [ArrayBuffer, TenError | undefined];
    getArray(): [Value[], TenError | undefined];
    getObject(): [Record<string, Value>, TenError | undefined];
    getJsonString(): [string, TenError | undefined];
}
