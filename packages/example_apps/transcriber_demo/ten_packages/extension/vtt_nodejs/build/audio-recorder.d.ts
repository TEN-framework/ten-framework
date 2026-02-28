import { AudioFrame } from "ten-runtime-nodejs";
/**
 * AudioRecorder - Audio recorder that writes PCM data stream to WAV file
 */
export declare class AudioRecorder {
    private audioPath;
    private sampleRate;
    private channels;
    private bitDepth;
    private audioBuffers;
    private currentTimestamp;
    private isRecording;
    private totalBytesWritten;
    private startTime;
    private totalSamplesReceived;
    constructor(audioPath: string);
    /**
     * Start recording
     */
    start(): void;
    /**
     * Write audio frame
     */
    writeFrame(audioFrame: AudioFrame): void;
    /**
     * Get current timestamp (milliseconds)
     */
    getCurrentTimestamp(): number;
    /**
     * Get recording duration (seconds)
     */
    getDuration(): number;
    /**
     * Stop recording and save file
     */
    stop(): Promise<void>;
    /**
     * Convert PCM data to Float32Array
     */
    private convertToFloat32;
    /**
     * Cancel recording (without saving)
     */
    cancel(): void;
    /**
     * Check if recording is active
     */
    isActive(): boolean;
}
