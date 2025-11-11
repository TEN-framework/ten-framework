//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
import * as fs from "fs";
import * as path from "path";
import { v4 as uuidv4 } from "uuid";

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
export class SessionManager {
    private basePath: string;
    private currentSession: SessionMetadata | null = null;

    constructor(basePath: string = "./recordings") {
        this.basePath = basePath;
        this.ensureBaseDirectory();
    }

    /**
     * Ensure base directory exists
     */
    private ensureBaseDirectory(): void {
        if (!fs.existsSync(this.basePath)) {
            fs.mkdirSync(this.basePath, { recursive: true });
        }
    }

    /**
     * Create new session
     */
    createSession(): SessionMetadata {
        const sessionId = uuidv4();
        const sessionPath = path.join(this.basePath, sessionId);

        // Create session directory
        fs.mkdirSync(sessionPath, { recursive: true });

        this.currentSession = {
            sessionId,
            startTime: Date.now(),
        };

        return this.currentSession;
    }

    /**
     * Get current session
     */
    getCurrentSession(): SessionMetadata | null {
        return this.currentSession;
    }

    /**
     * Get session path
     */
    getSessionPath(sessionId: string): string {
        return path.join(this.basePath, sessionId);
    }

    /**
     * Get audio file path
     */
    getAudioPath(sessionId: string): string {
        return path.join(this.getSessionPath(sessionId), "audio.wav");
    }

    /**
     * Get VTT file path
     */
    getVTTPath(sessionId: string): string {
        return path.join(this.getSessionPath(sessionId), "transcript.vtt");
    }

    /**
     * Get metadata file path
     */
    getMetadataPath(sessionId: string): string {
        return path.join(this.getSessionPath(sessionId), "metadata.json");
    }

    /**
     * End session and save metadata
     */
    async endSession(
        sessionId: string,
        metadata: Partial<SessionMetadata>
    ): Promise<void> {
        if (this.currentSession && this.currentSession.sessionId === sessionId) {
            this.currentSession.endTime = Date.now();
            this.currentSession.duration =
                this.currentSession.endTime - this.currentSession.startTime;

            // Merge additional metadata
            Object.assign(this.currentSession, metadata);

            // Set file paths
            this.currentSession.audioFile = this.getAudioPath(sessionId);
            this.currentSession.vttFile = this.getVTTPath(sessionId);

            // Save metadata to file
            await this.saveMetadata(sessionId, this.currentSession);

            // Clear current session
            this.currentSession = null;
        }
    }

    /**
     * Save metadata to file
     */
    private async saveMetadata(
        sessionId: string,
        metadata: SessionMetadata
    ): Promise<void> {
        const metadataPath = this.getMetadataPath(sessionId);
        await fs.promises.writeFile(
            metadataPath,
            JSON.stringify(metadata, null, 2),
            "utf-8"
        );
    }

    /**
     * List all sessions
     */
    async listSessions(): Promise<SessionMetadata[]> {
        const sessions: SessionMetadata[] = [];

        try {
            const dirs = await fs.promises.readdir(this.basePath);

            for (const dir of dirs) {
                const metadataPath = path.join(this.basePath, dir, "metadata.json");
                if (fs.existsSync(metadataPath)) {
                    const content = await fs.promises.readFile(metadataPath, "utf-8");
                    sessions.push(JSON.parse(content));
                }
            }

            // Sort by time in descending order
            sessions.sort((a, b) => b.startTime - a.startTime);
        } catch (error) {
            console.error("Error listing sessions:", error);
        }

        return sessions;
    }

    /**
     * Delete session
     */
    async deleteSession(sessionId: string): Promise<boolean> {
        try {
            const sessionPath = this.getSessionPath(sessionId);
            if (fs.existsSync(sessionPath)) {
                await fs.promises.rm(sessionPath, { recursive: true, force: true });
                return true;
            }
        } catch (error) {
            console.error("Error deleting session:", error);
        }
        return false;
    }

    /**
     * Get session metadata
     */
    async getSessionMetadata(sessionId: string): Promise<SessionMetadata | null> {
        try {
            const metadataPath = this.getMetadataPath(sessionId);
            if (fs.existsSync(metadataPath)) {
                const content = await fs.promises.readFile(metadataPath, "utf-8");
                return JSON.parse(content);
            }
        } catch (error) {
            console.error("Error reading session metadata:", error);
        }
        return null;
    }
}
