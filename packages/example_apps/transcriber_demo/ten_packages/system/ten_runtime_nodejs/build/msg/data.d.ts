import { Msg } from "./msg.js";
export declare class Data extends Msg {
    private constructor();
    static Create(name: string): Data;
    allocBuf(size: number): void;
    lockBuf(): ArrayBuffer;
    unlockBuf(buf: ArrayBuffer): void;
    getBuf(): ArrayBuffer;
}
