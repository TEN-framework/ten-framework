import type { TenError } from "../error.js";
import { Loc } from "../loc.js";
export declare class Msg {
    getName(): string;
    getSource(): Loc;
    setDests(dests: Partial<Loc>[]): TenError | undefined;
    setPropertyFromJson(path: string, jsonStr: string): TenError | undefined;
    getPropertyToJson(path: string): [string, TenError | undefined];
    setPropertyNumber(path: string, value: number): TenError | undefined;
    getPropertyNumber(path: string): [number, TenError | undefined];
    setPropertyString(path: string, value: string): TenError | undefined;
    getPropertyString(path: string): [string, TenError | undefined];
    setPropertyBool(path: string, value: boolean): TenError | undefined;
    getPropertyBool(path: string): [boolean, TenError | undefined];
    setPropertyBuf(path: string, value: ArrayBuffer): TenError | undefined;
    getPropertyBuf(path: string): [ArrayBuffer, TenError | undefined];
}
