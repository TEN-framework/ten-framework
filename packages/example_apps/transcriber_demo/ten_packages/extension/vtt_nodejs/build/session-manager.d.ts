/**
 * Session metadata interface
 */
export interface SessionMetadata {
    sessionId: string;
    startTime: number;
    endTime?: number;
    duration?: number;
    totalWords?: number;
    totalSegments?: number;
    audioFile?: string;
    vttFile?: string;
}
/**
 * SessionManager - Manages recording sessions
 */
export declare class SessionManager {
    private basePath;
    private currentSession;
    constructor(basePath?: string);
    /**
     * Ensure base directory exists
     */
    private ensureBaseDirectory;
    /**
     * Create new session
     */
    createSession(): SessionMetadata;
    /**
     * Get current session
     */
    getCurrentSession(): SessionMetadata | null;
    /**
     * Get session path
     */
    getSessionPath(sessionId: string): string;
    /**
     * Get audio file path
     */
    getAudioPath(sessionId: string): string;
    /**
     * Get VTT file path
     */
    getVTTPath(sessionId: string): string;
    /**
     * Get metadata file path
     */
    getMetadataPath(sessionId: string): string;
    /**
     * End session and save metadata
     */
    endSession(sessionId: string, metadata: Partial<SessionMetadata>): Promise<void>;
    /**
     * Save metadata to file
     */
    private saveMetadata;
    /**
     * List all sessions
     */
    listSessions(): Promise<SessionMetadata[]>;
    /**
     * Delete session
     */
    deleteSession(sessionId: string): Promise<boolean>;
    /**
     * Get session metadata
     */
    getSessionMetadata(sessionId: string): Promise<SessionMetadata | null>;
}
