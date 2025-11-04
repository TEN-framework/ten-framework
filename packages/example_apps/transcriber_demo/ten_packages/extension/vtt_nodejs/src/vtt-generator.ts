//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
import * as fs from "fs";

/**
 * VTT 字幕片段接口
 */
export interface VTTSegment {
    start: number; // 开始时间（毫秒）
    end: number; // 结束时间（毫秒）
    text: string; // 文本内容
}

/**
 * VTTGenerator - 生成 WebVTT 格式字幕文件
 */
export class VTTGenerator {
    private segments: VTTSegment[] = [];
    private currentText: string = "";
    private lastTimestamp: number = 0;
    private segmentStartTime: number = 0;
    private maxSegmentDuration: number = 7000; // 最大片段时长（毫秒）
    private minSegmentDuration: number = 1000; // 最小片段时长（毫秒）

    /**
     * 添加 ASR 结果
     */
    addAsrResult(text: string, timestamp: number, isFinal: boolean): void {
        // 只处理 final 结果
        if (!isFinal) {
            return;
        }

        // 格式化文本
        const formattedText = this.formatText(text);

        if (!formattedText) {
            return;
        }

        // 如果是第一个片段，设置开始时间
        if (this.segments.length === 0 && !this.currentText) {
            this.segmentStartTime = timestamp;
        }

        // 累积文本
        if (this.currentText) {
            this.currentText += " " + formattedText;
        } else {
            this.currentText = formattedText;
            this.segmentStartTime = this.lastTimestamp || timestamp;
        }

        this.lastTimestamp = timestamp;

        // 检查是否需要创建新片段
        const duration = timestamp - this.segmentStartTime;
        const shouldCreateSegment =
            duration >= this.maxSegmentDuration || this.isEndOfSentence(formattedText);

        if (shouldCreateSegment && duration >= this.minSegmentDuration) {
            this.createSegment(timestamp);
        }
    }

    /**
     * 格式化文本
     */
    private formatText(text: string): string {
        // 去除首尾空格
        text = text.trim();

        if (!text) {
            return "";
        }

        // 首字母大写
        if (text.length > 0) {
            text = text.charAt(0).toUpperCase() + text.slice(1);
        }

        // 确保句子结尾有标点
        if (this.isEndOfSentence(text)) {
            return text;
        }

        // 如果没有结尾标点，暂时不添加（等待更多上下文）
        return text;
    }

    /**
     * 判断是否为句子结尾
     */
    private isEndOfSentence(text: string): boolean {
        return /[.!?。！？]$/.test(text.trim());
    }

    /**
     * 创建新片段
     */
    private createSegment(endTime: number): void {
        if (!this.currentText) {
            return;
        }

        // 确保文本有结尾标点
        let finalText = this.currentText.trim();
        if (!this.isEndOfSentence(finalText)) {
            finalText += ".";
        }

        this.segments.push({
            start: this.segmentStartTime,
            end: endTime,
            text: finalText,
        });

        console.log(
            `[VTTGenerator] Created segment ${this.segments.length}: ${this.formatTimestamp(this.segmentStartTime)} --> ${this.formatTimestamp(endTime)}, text: "${finalText}"`
        );

        // 重置当前文本
        this.currentText = "";
        this.segmentStartTime = endTime;
    }

    /**
     * 完成录制（处理最后的片段）
     */
    finalize(endTime?: number): void {
        if (this.currentText) {
            const finalEndTime = endTime || this.lastTimestamp || 0;
            this.createSegment(finalEndTime);
        }

        console.log(`[VTTGenerator] Finalized with ${this.segments.length} segments`);
    }

    /**
     * 生成 VTT 内容
     */
    generate(): string {
        let vtt = "WEBVTT\n\n";

        this.segments.forEach((segment, index) => {
            // 序号
            vtt += `${index + 1}\n`;

            // 时间范围
            vtt += `${this.formatTimestamp(segment.start)} --> ${this.formatTimestamp(segment.end)}\n`;

            // 文本内容
            vtt += `${segment.text}\n\n`;
        });

        return vtt;
    }

    /**
     * 格式化时间戳（毫秒转 VTT 时间格式）
     */
    private formatTimestamp(ms: number): string {
        const totalSeconds = Math.floor(ms / 1000);
        const hours = Math.floor(totalSeconds / 3600);
        const minutes = Math.floor((totalSeconds % 3600) / 60);
        const seconds = totalSeconds % 60;
        const milliseconds = Math.floor(ms % 1000);

        return `${this.pad(hours, 2)}:${this.pad(minutes, 2)}:${this.pad(seconds, 2)}.${this.pad(milliseconds, 3)}`;
    }

    /**
     * 数字补零
     */
    private pad(num: number, size: number): string {
        return String(num).padStart(size, "0");
    }

    /**
     * 保存 VTT 文件
     */
    async save(vttPath: string): Promise<void> {
        const vttContent = this.generate();

        await fs.promises.writeFile(vttPath, vttContent, "utf-8");

        console.log(
            `[VTTGenerator] VTT file saved: ${vttPath}, ${this.segments.length} segments`
        );
    }

    /**
     * 获取片段数量
     */
    getSegmentCount(): number {
        return this.segments.length;
    }

    /**
     * 获取总词数（粗略估计）
     */
    getTotalWords(): number {
        return this.segments.reduce((count, segment) => {
            return count + segment.text.split(/\s+/).length;
        }, 0);
    }

    /**
     * 获取所有片段
     */
    getSegments(): VTTSegment[] {
        return [...this.segments];
    }

    /**
     * 获取纯文本（所有片段拼接）
     */
    getPlainText(): string {
        return this.segments.map((seg) => seg.text).join(" ");
    }

    /**
     * 生成 JSON 格式（用于前端展示）
     */
    generateJSON(): string {
        return JSON.stringify(
            {
                segments: this.segments,
                totalSegments: this.segments.length,
                totalWords: this.getTotalWords(),
                plainText: this.getPlainText(),
            },
            null,
            2
        );
    }

    /**
     * 设置最大片段时长
     */
    setMaxSegmentDuration(ms: number): void {
        this.maxSegmentDuration = ms;
    }

    /**
     * 设置最小片段时长
     */
    setMinSegmentDuration(ms: number): void {
        this.minSegmentDuration = ms;
    }
}
