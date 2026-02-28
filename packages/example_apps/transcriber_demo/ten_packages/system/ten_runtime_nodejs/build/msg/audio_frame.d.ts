import { Msg } from "./msg.js";
export declare enum AudioFrameDataFmt {
    INTERLEAVE = 1,
    NON_INTERLEAVE = 2
}
export declare class AudioFrame extends Msg {
    private constructor();
    static Create(name: string): AudioFrame;
    allocBuf(size: number): void;
    lockBuf(): ArrayBuffer;
    unlockBuf(buf: ArrayBuffer): void;
    getBuf(): ArrayBuffer;
    getTimestamp(): number;
    setTimestamp(timestamp: number): void;
    getSampleRate(): number;
    setSampleRate(sampleRate: number): void;
    getSamplesPerChannel(): number;
    setSamplesPerChannel(samplesPerChannel: number): void;
    getBytesPerSample(): number;
    setBytesPerSample(bytesPerSample: number): void;
    getNumberOfChannels(): number;
    setNumberOfChannels(numberOfChannels: number): void;
    getDataFmt(): AudioFrameDataFmt;
    setDataFmt(dataFmt: AudioFrameDataFmt): void;
    getLineSize(): number;
    setLineSize(lineSize: number): void;
    isEof(): boolean;
    setEof(eof: boolean): void;
}
