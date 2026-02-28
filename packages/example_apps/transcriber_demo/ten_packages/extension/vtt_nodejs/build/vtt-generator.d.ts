/**
 * VTT subtitle segment interface
 */
export interface VTTSegment {
    start: number;
    end: number;
    text: string;
}
/**
 * VTTGenerator - Generate WebVTT format subtitle file
 */
export declare class VTTGenerator {
    private segments;
    private currentText;
    private currentStartTime;
    private currentEndTime;
    private maxSegmentDuration;
    private minSegmentDuration;
    /**
     * Add ASR result
     * @param text Recognized text
     * @param startMs ASR recognition start time (relative to audio timeline)
     * @param isFinal Whether it is the final result
     * @param durationMs ASR recognition duration
     */
    addAsrResult(text: string, startMs: number, isFinal: boolean, durationMs?: number): void;
    /**
     * Format text
     */
    private formatText;
    /**
     * Check if it's the end of a sentence
     */
    private isEndOfSentence;
    /**
     * Create new segment (old method, kept for compatibility)
     */
    private createSegment;
    /**
     * Create new segment directly
     */
    private createSegmentDirect;
    /**
     * Finalize recording (process final segment)
     */
    finalize(endTime?: number): void;
    /**
     * Generate VTT content
     */
    generate(): string;
    /**
     * Format timestamp (milliseconds to VTT time format)
     */
    private formatTimestamp;
    /**
     * Pad number with zeros
     */
    private pad;
    /**
     * Save VTT file
     */
    save(vttPath: string): Promise<void>;
    /**
     * Get segment count
     */
    getSegmentCount(): number;
    /**
     * Get total word count (rough estimate)
     */
    getTotalWords(): number;
    /**
     * Get all segments
     */
    getSegments(): VTTSegment[];
    /**
     * Get plain text (all segments concatenated)
     */
    getPlainText(): string;
    /**
     * Generate JSON format (for frontend display)
     */
    generateJSON(): string;
    /**
     * Set max segment duration
     */
    setMaxSegmentDuration(ms: number): void;
    /**
     * Set min segment duration
     */
    setMinSegmentDuration(ms: number): void;
}
