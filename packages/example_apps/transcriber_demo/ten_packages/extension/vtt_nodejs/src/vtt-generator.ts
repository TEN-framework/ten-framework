//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
import * as fs from "fs";

/**
 * VTT subtitle segment interface
 */
export interface VTTSegment {
    start: number; // Start time (milliseconds)
    end: number; // End time (milliseconds)
    text: string; // Text content
}

/**
 * VTTGenerator - Generate WebVTT format subtitle file
 */
export class VTTGenerator {
    private segments: VTTSegment[] = [];
    private currentText: string = "";
    private currentStartTime: number = 0;
    private currentEndTime: number = 0;
    private maxSegmentDuration: number = 7000; // Max segment duration (milliseconds)
    private minSegmentDuration: number = 1000; // Min segment duration (milliseconds)

    /**
     * Add ASR result
     * @param text Recognized text
     * @param startMs ASR recognition start time (relative to audio timeline)
     * @param isFinal Whether it is the final result
     * @param durationMs ASR recognition duration
     */
    addAsrResult(text: string, startMs: number, isFinal: boolean, durationMs: number = 0): void {
        // Only process final results
        if (!isFinal) {
            return;
        }

        // Format text
        const formattedText = this.formatText(text);

        if (!formattedText) {
            return;
        }

        // Calculate end time: start_ms + duration_ms
        const endMs = startMs + durationMs;

        console.log(
            `[VTTGenerator] Adding ASR result: start=${startMs}ms, duration=${durationMs}ms, end=${endMs}ms, text="${formattedText}"`
        );

        // If first segment or large time gap, create new segment directly
        if (this.segments.length === 0 || startMs - this.currentEndTime > 2000) {
            // If there is accumulated text, save it first
            if (this.currentText) {
                this.createSegmentDirect(this.currentStartTime, this.currentEndTime, this.currentText);
            }

            // Start new segment
            this.currentText = formattedText;
            this.currentStartTime = startMs;
            this.currentEndTime = endMs;
        } else {
            // Accumulate text
            if (this.currentText) {
                this.currentText += " " + formattedText;
            } else {
                this.currentText = formattedText;
                this.currentStartTime = startMs;
            }
            this.currentEndTime = endMs;

            // Check if need to create new segment
            const duration = this.currentEndTime - this.currentStartTime;
            const shouldCreateSegment =
                duration >= this.maxSegmentDuration || this.isEndOfSentence(formattedText);

            if (shouldCreateSegment && duration >= this.minSegmentDuration) {
                this.createSegmentDirect(this.currentStartTime, this.currentEndTime, this.currentText);
                this.currentText = "";
            }
        }
    }

    /**
     * Format text
     */
    private formatText(text: string): string {
        // Trim whitespace
        text = text.trim();

        if (!text) {
            return "";
        }

        // Capitalize first letter
        if (text.length > 0) {
            text = text.charAt(0).toUpperCase() + text.slice(1);
        }

        // Ensure sentence ends with punctuation
        if (this.isEndOfSentence(text)) {
            return text;
        }

        // If no ending punctuation, don't add yet (wait for more context)
        return text;
    }

    /**
     * Check if it's the end of a sentence
     */
    private isEndOfSentence(text: string): boolean {
        return /[.!?。！？]$/.test(text.trim());
    }

    /**
     * Create new segment (old method, kept for compatibility)
     */
    private createSegment(endTime: number): void {
        if (!this.currentText) {
            return;
        }

        this.createSegmentDirect(this.currentStartTime, endTime, this.currentText);

        // Reset current text
        this.currentText = "";
        this.currentStartTime = endTime;
    }

    /**
     * Create new segment directly
     */
    private createSegmentDirect(startTime: number, endTime: number, text: string): void {
        if (!text) {
            return;
        }

        // Ensure text ends with punctuation
        let finalText = text.trim();
        if (!this.isEndOfSentence(finalText)) {
            finalText += ".";
        }

        this.segments.push({
            start: startTime,
            end: endTime,
            text: finalText,
        });

        console.log(
            `[VTTGenerator] Created segment ${this.segments.length}: ${this.formatTimestamp(startTime)} --> ${this.formatTimestamp(endTime)}, text: "${finalText}"`
        );
    }

    /**
     * Finalize recording (process final segment)
     */
    finalize(endTime?: number): void {
        if (this.currentText) {
            const finalEndTime = endTime || this.currentEndTime || 0;
            this.createSegmentDirect(this.currentStartTime, finalEndTime, this.currentText);
            this.currentText = "";
        }

        console.log(`[VTTGenerator] Finalized with ${this.segments.length} segments`);
    }

    /**
     * Generate VTT content
     */
    generate(): string {
        let vtt = "WEBVTT\n\n";

        this.segments.forEach((segment, index) => {
            // Sequence number
            vtt += `${index + 1}\n`;

            // Time range
            vtt += `${this.formatTimestamp(segment.start)} --> ${this.formatTimestamp(segment.end)}\n`;

            // Text content
            vtt += `${segment.text}\n\n`;
        });

        return vtt;
    }

    /**
     * Format timestamp (milliseconds to VTT time format)
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
     * Pad number with zeros
     */
    private pad(num: number, size: number): string {
        return String(num).padStart(size, "0");
    }

    /**
     * Save VTT file
     */
    async save(vttPath: string): Promise<void> {
        const vttContent = this.generate();

        await fs.promises.writeFile(vttPath, vttContent, "utf-8");

        console.log(
            `[VTTGenerator] VTT file saved: ${vttPath}, ${this.segments.length} segments`
        );
    }

    /**
     * Get segment count
     */
    getSegmentCount(): number {
        return this.segments.length;
    }

    /**
     * Get total word count (rough estimate)
     */
    getTotalWords(): number {
        return this.segments.reduce((count, segment) => {
            return count + segment.text.split(/\s+/).length;
        }, 0);
    }

    /**
     * Get all segments
     */
    getSegments(): VTTSegment[] {
        return [...this.segments];
    }

    /**
     * Get plain text (all segments concatenated)
     */
    getPlainText(): string {
        return this.segments.map((seg) => seg.text).join(" ");
    }

    /**
     * Generate JSON format (for frontend display)
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
     * Set max segment duration
     */
    setMaxSegmentDuration(ms: number): void {
        this.maxSegmentDuration = ms;
    }

    /**
     * Set min segment duration
     */
    setMinSegmentDuration(ms: number): void {
        this.minSegmentDuration = ms;
    }
}
