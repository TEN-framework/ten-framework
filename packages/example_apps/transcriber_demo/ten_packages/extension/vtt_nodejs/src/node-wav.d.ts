declare module 'node-wav' {
    export interface EncodeOptions {
        sampleRate: number;
        float?: boolean;
        bitDepth?: number;
    }

    export interface DecodeResult {
        sampleRate: number;
        channelData: Float32Array[];
    }

    export function encode(
        channelData: Float32Array[],
        options: EncodeOptions
    ): ArrayBuffer;

    export function decode(buffer: Buffer): DecodeResult;
}
