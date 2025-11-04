//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
import * as fs from "fs";
import * as wav from "node-wav";
import { AudioFrame } from "ten-runtime-nodejs";

/**
 * AudioRecorder - 音频录制器，将 PCM 数据流写入 WAV 文件
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

    constructor(audioPath: string) {
        this.audioPath = audioPath;
    }

    /**
     * 开始录制
     */
    start(): void {
        this.isRecording = true;
        this.audioBuffers = [];
        this.totalBytesWritten = 0;
        this.currentTimestamp = 0;
        console.log(`[AudioRecorder] Started recording to: ${this.audioPath}`);
    }

    /**
     * 写入音频帧
     */
    writeFrame(audioFrame: AudioFrame): void {
        if (!this.isRecording) {
            return;
        }

        try {
            // 获取音频帧参数
            const sampleRate = audioFrame.getSampleRate();
            const channels = audioFrame.getNumberOfChannels();
            const bytesPerSample = audioFrame.getBytesPerSample();
            const samplesPerChannel = audioFrame.getSamplesPerChannel();
            const timestamp = audioFrame.getTimestamp();

            // 更新参数（以第一帧为准）
            if (this.audioBuffers.length === 0) {
                this.sampleRate = sampleRate;
                this.channels = channels;
                this.bitDepth = bytesPerSample * 8;
                console.log(
                    `[AudioRecorder] Audio format: ${this.sampleRate}Hz, ${this.channels}ch, ${this.bitDepth}bit`
                );
            }

            // 获取音频数据
            const buf = audioFrame.lockBuf();
            const audioData = Buffer.from(buf);
            audioFrame.unlockBuf(buf);

            // 存入缓冲区
            this.audioBuffers.push(audioData);
            this.totalBytesWritten += audioData.length;

            // 更新时间戳（微秒转换为毫秒）
            this.currentTimestamp = timestamp / 1000;

            // 每100帧打印一次日志
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
     * 获取当前时间戳（毫秒）
     */
    getCurrentTimestamp(): number {
        return this.currentTimestamp;
    }

    /**
     * 获取录制时长（秒）
     */
    getDuration(): number {
        return this.currentTimestamp / 1000;
    }

    /**
     * 停止录制并保存文件
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
            // 合并所有音频缓冲区
            const audioData = Buffer.concat(this.audioBuffers);

            // 计算采样数
            const bytesPerSample = this.bitDepth / 8;
            const totalSamples = audioData.length / (bytesPerSample * this.channels);

            console.log(
                `[AudioRecorder] Total samples: ${totalSamples}, duration: ${(totalSamples / this.sampleRate).toFixed(2)}s`
            );

            // 转换为 Float32Array（node-wav 需要）
            const audioFloat32 = this.convertToFloat32(audioData, this.bitDepth);

            // 按声道分离（如果是多声道）
            const channelData: Float32Array[] = [];
            if (this.channels === 1) {
                channelData.push(audioFloat32);
            } else {
                // 交错数据转为分离声道
                for (let c = 0; c < this.channels; c++) {
                    const channelBuffer = new Float32Array(audioFloat32.length / this.channels);
                    for (let i = 0; i < channelBuffer.length; i++) {
                        channelBuffer[i] = audioFloat32[i * this.channels + c];
                    }
                    channelData.push(channelBuffer);
                }
            }

            // 编码为 WAV
            const wavBuffer = wav.encode(channelData, {
                sampleRate: this.sampleRate,
                float: false,
                bitDepth: 16,
            });

            // 写入文件
            await fs.promises.writeFile(this.audioPath, Buffer.from(wavBuffer));

            console.log(
                `[AudioRecorder] WAV file saved: ${this.audioPath}, size: ${(wavBuffer.byteLength / 1024).toFixed(2)}KB`
            );

            // 清空缓冲区
            this.audioBuffers = [];
        } catch (error) {
            console.error("[AudioRecorder] Error saving WAV file:", error);
            throw error;
        }
    }

    /**
     * 将 PCM 数据转换为 Float32Array
     */
    private convertToFloat32(buffer: Buffer, bitDepth: number): Float32Array {
        const float32Array = new Float32Array(buffer.length / (bitDepth / 8));

        if (bitDepth === 16) {
            // 16-bit PCM (signed int16)
            for (let i = 0; i < float32Array.length; i++) {
                const int16 = buffer.readInt16LE(i * 2);
                float32Array[i] = int16 / 32768.0; // 归一化到 [-1, 1]
            }
        } else if (bitDepth === 8) {
            // 8-bit PCM (unsigned int8)
            for (let i = 0; i < float32Array.length; i++) {
                const uint8 = buffer.readUInt8(i);
                float32Array[i] = (uint8 - 128) / 128.0; // 归一化到 [-1, 1]
            }
        } else if (bitDepth === 24) {
            // 24-bit PCM (signed int24)
            for (let i = 0; i < float32Array.length; i++) {
                const byte1 = buffer.readUInt8(i * 3);
                const byte2 = buffer.readUInt8(i * 3 + 1);
                const byte3 = buffer.readUInt8(i * 3 + 2);
                let int24 = (byte3 << 16) | (byte2 << 8) | byte1;
                // 处理符号位
                if (int24 & 0x800000) {
                    int24 |= ~0xffffff;
                }
                float32Array[i] = int24 / 8388608.0; // 归一化到 [-1, 1]
            }
        } else if (bitDepth === 32) {
            // 32-bit PCM (signed int32)
            for (let i = 0; i < float32Array.length; i++) {
                const int32 = buffer.readInt32LE(i * 4);
                float32Array[i] = int32 / 2147483648.0; // 归一化到 [-1, 1]
            }
        } else {
            throw new Error(`Unsupported bit depth: ${bitDepth}`);
        }

        return float32Array;
    }

    /**
     * 取消录制（不保存）
     */
    cancel(): void {
        this.isRecording = false;
        this.audioBuffers = [];
        console.log("[AudioRecorder] Recording cancelled");
    }

    /**
     * 检查是否正在录制
     */
    isActive(): boolean {
        return this.isRecording;
    }
}
