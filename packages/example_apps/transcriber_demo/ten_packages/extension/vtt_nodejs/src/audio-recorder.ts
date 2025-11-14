//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
import * as fs from "fs";
import * as wav from "node-wav";
import { AudioFrame } from "ten-runtime-nodejs";

/**
 * AudioRecorder - Audio recorder that writes PCM data stream to WAV file
 */
export class AudioRecorder {
    private audioPath: string;
    private sampleRate: number = 16000;
    private channels: number = 1;
    private bitDepth: number = 16;
    private audioBuffers: Buffer[] = [];
    private currentTimestamp: number = 0;
    private isRecording: boolean = false;
    private totalBytesWritten: number = 0;
    private startTime: number = 0;
    private totalSamplesReceived: number = 0;

    constructor(audioPath: string) {
        this.audioPath = audioPath;
    }

    /**
     * Start recording
     */
    start(): void {
        this.isRecording = true;
        this.audioBuffers = [];
        this.totalBytesWritten = 0;
        this.currentTimestamp = 0;
        this.startTime = Date.now();
        this.totalSamplesReceived = 0;
        console.log(`[AudioRecorder] Started recording to: ${this.audioPath}`);
    }

    /**
     * Write audio frame
     */
    writeFrame(audioFrame: AudioFrame): void {
        if (!this.isRecording) {
            return;
        }

        try {
            // Get audio frame parameters
            const sampleRate = audioFrame.getSampleRate();
            const channels = audioFrame.getNumberOfChannels();
            const bytesPerSample = audioFrame.getBytesPerSample();
            const samplesPerChannel = audioFrame.getSamplesPerChannel();
            const timestamp = audioFrame.getTimestamp();

            // Update parameters (based on first frame)
            if (this.audioBuffers.length === 0) {
                this.sampleRate = sampleRate;
                this.channels = channels;
                this.bitDepth = bytesPerSample * 8;
                console.log(
                    `[AudioRecorder] Audio format: ${this.sampleRate}Hz, ${this.channels}ch, ${this.bitDepth}bit`
                );
            }

            // Get audio data
            const buf = audioFrame.lockBuf();
            const audioData = Buffer.from(buf);
            audioFrame.unlockBuf(buf);

            // Store in buffer
            this.audioBuffers.push(audioData);
            this.totalBytesWritten += audioData.length;

            // Update total samples received
            this.totalSamplesReceived += samplesPerChannel;

            // Calculate timestamp from samples (milliseconds)
            // timestamp = (samples / sampleRate) * 1000
            this.currentTimestamp = (this.totalSamplesReceived / this.sampleRate) * 1000;

            // Log every 100 frames
            if (this.audioBuffers.length % 100 === 0) {
                console.log(
                    `[AudioRecorder] Buffered ${this.audioBuffers.length} frames, ${(this.totalBytesWritten / 1024).toFixed(2)}KB, time: ${(this.currentTimestamp / 1000).toFixed(2)}s`
                );
            }
        } catch (error) {
            console.error("[AudioRecorder] Error writing audio frame:", error);
        }
    }

    /**
     * Get current timestamp (milliseconds)
     */
    getCurrentTimestamp(): number {
        return this.currentTimestamp;
    }

    /**
     * Get recording duration (seconds)
     */
    getDuration(): number {
        return this.currentTimestamp / 1000;
    }

    /**
     * Stop recording and save file
     */
    async stop(): Promise<void> {
        if (!this.isRecording) {
            return;
        }

        this.isRecording = false;

        console.log(
            `[AudioRecorder] Stopping recording, total frames: ${this.audioBuffers.length}, total bytes: ${this.totalBytesWritten}`
        );

        try {
            // Merge all audio buffers
            const audioData = Buffer.concat(this.audioBuffers);

            // Calculate sample count
            const bytesPerSample = this.bitDepth / 8;
            const totalSamples = audioData.length / (bytesPerSample * this.channels);

            console.log(
                `[AudioRecorder] Total samples: ${totalSamples}, duration: ${(totalSamples / this.sampleRate).toFixed(2)}s`
            );

            // Convert to Float32Array (required by node-wav)
            const audioFloat32 = this.convertToFloat32(audioData, this.bitDepth);

            // Separate by channels (if multi-channel)
            const channelData: Float32Array[] = [];
            if (this.channels === 1) {
                channelData.push(audioFloat32);
            } else {
                // Interleaved data to separated channels
                for (let c = 0; c < this.channels; c++) {
                    const channelBuffer = new Float32Array(audioFloat32.length / this.channels);
                    for (let i = 0; i < channelBuffer.length; i++) {
                        channelBuffer[i] = audioFloat32[i * this.channels + c];
                    }
                    channelData.push(channelBuffer);
                }
            }

            // Encode to WAV
            const wavBuffer = wav.encode(channelData, {
                sampleRate: this.sampleRate,
                float: false,
                bitDepth: 16,
            });

            // Write to file
            await fs.promises.writeFile(this.audioPath, Buffer.from(wavBuffer));

            console.log(
                `[AudioRecorder] WAV file saved: ${this.audioPath}, size: ${(wavBuffer.byteLength / 1024).toFixed(2)}KB`
            );

            // Clear buffers
            this.audioBuffers = [];
        } catch (error) {
            console.error("[AudioRecorder] Error saving WAV file:", error);
            throw error;
        }
    }

    /**
     * Convert PCM data to Float32Array
     */
    private convertToFloat32(buffer: Buffer, bitDepth: number): Float32Array {
        const float32Array = new Float32Array(buffer.length / (bitDepth / 8));

        if (bitDepth === 16) {
            // 16-bit PCM (signed int16)
            for (let i = 0; i < float32Array.length; i++) {
                const int16 = buffer.readInt16LE(i * 2);
                float32Array[i] = int16 / 32768.0; // Normalize to [-1, 1]
            }
        } else if (bitDepth === 8) {
            // 8-bit PCM (unsigned int8)
            for (let i = 0; i < float32Array.length; i++) {
                const uint8 = buffer.readUInt8(i);
                float32Array[i] = (uint8 - 128) / 128.0; // Normalize to [-1, 1]
            }
        } else if (bitDepth === 24) {
            // 24-bit PCM (signed int24)
            for (let i = 0; i < float32Array.length; i++) {
                const byte1 = buffer.readUInt8(i * 3);
                const byte2 = buffer.readUInt8(i * 3 + 1);
                const byte3 = buffer.readUInt8(i * 3 + 2);
                let int24 = (byte3 << 16) | (byte2 << 8) | byte1;
                // Handle sign bit
                if (int24 & 0x800000) {
                    int24 |= ~0xffffff;
                }
                float32Array[i] = int24 / 8388608.0; // Normalize to [-1, 1]
            }
        } else if (bitDepth === 32) {
            // 32-bit PCM (signed int32)
            for (let i = 0; i < float32Array.length; i++) {
                const int32 = buffer.readInt32LE(i * 4);
                float32Array[i] = int32 / 2147483648.0; // Normalize to [-1, 1]
            }
        } else {
            throw new Error(`Unsupported bit depth: ${bitDepth}`);
        }

        return float32Array;
    }

    /**
     * Cancel recording (without saving)
     */
    cancel(): void {
        this.isRecording = false;
        this.audioBuffers = [];
        console.log("[AudioRecorder] Recording cancelled");
    }

    /**
     * Check if recording is active
     */
    isActive(): boolean {
        return this.isRecording;
    }
}
