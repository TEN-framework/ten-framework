import { Value } from "./value.js";
export declare const VALUE_BUFFER_MAGIC = 270;
export declare const VALUE_BUFFER_VERSION = 1;
export declare const VALUE_BUFFER_HEADER_SIZE = 8;
export declare const BUFFER_TYPE_INVALID = 0;
export declare const BUFFER_TYPE_BOOL = 1;
export declare const BUFFER_TYPE_INT8 = 2;
export declare const BUFFER_TYPE_INT16 = 3;
export declare const BUFFER_TYPE_INT32 = 4;
export declare const BUFFER_TYPE_INT64 = 5;
export declare const BUFFER_TYPE_UINT8 = 6;
export declare const BUFFER_TYPE_UINT16 = 7;
export declare const BUFFER_TYPE_UINT32 = 8;
export declare const BUFFER_TYPE_UINT64 = 9;
export declare const BUFFER_TYPE_FLOAT32 = 10;
export declare const BUFFER_TYPE_FLOAT64 = 11;
export declare const BUFFER_TYPE_STRING = 12;
export declare const BUFFER_TYPE_BUF = 13;
export declare const BUFFER_TYPE_ARRAY = 14;
export declare const BUFFER_TYPE_OBJECT = 15;
export declare const BUFFER_TYPE_JSON_STRING = 17;
export interface ValueBufferHeader {
    magic: number;
    version: number;
    typeId: number;
    size: number;
}
export declare function serializeToBuffer(value: Value): Buffer;
export declare function deserializeFromBuffer(buffer: Buffer): Value;
