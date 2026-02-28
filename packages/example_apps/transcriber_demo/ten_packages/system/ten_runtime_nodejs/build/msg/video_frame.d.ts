import { Msg } from "./msg.js";
export declare enum PixelFmt {
    RGB24 = 1,
    RGBA = 2,
    BGR24 = 3,
    BGRA = 4,
    I422 = 5,
    I420 = 6,
    NV21 = 7,
    NV12 = 8
}
export declare class VideoFrame extends Msg {
    private constructor();
    static Create(name: string): VideoFrame;
    allocBuf(size: number): void;
    lockBuf(): ArrayBuffer;
    unlockBuf(buf: ArrayBuffer): void;
    getBuf(): ArrayBuffer;
    getWidth(): number;
    setWidth(width: number): void;
    getHeight(): number;
    setHeight(height: number): void;
    getTimestamp(): number;
    setTimestamp(timestamp: number): void;
    getPixelFmt(): PixelFmt;
    setPixelFmt(pixelFmt: PixelFmt): void;
    isEof(): boolean;
    setEof(eof: boolean): void;
}
