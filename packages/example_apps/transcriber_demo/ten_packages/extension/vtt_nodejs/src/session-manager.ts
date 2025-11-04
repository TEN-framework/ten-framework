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
 * SessionManager - 管理录制会话
 */
export class SessionManager {
    private basePath: string;
    private currentSession: SessionMetadata | null = null;

    constructor(basePath: string = "./recordings") {
        this.basePath = basePath;
        this.ensureBaseDirectory();
    }

    /**
     * 确保基础目录存在
     */
    private ensureBaseDirectory(): void {
        if (!fs.existsSync(this.basePath)) {
            fs.mkdirSync(this.basePath, { recursive: true });
        }
    }

    /**
     * 创建新会话
     */
    createSession(): SessionMetadata {
        const sessionId = uuidv4();
        const sessionPath = path.join(this.basePath, sessionId);

        // 创建会话目录
        fs.mkdirSync(sessionPath, { recursive: true });

        this.currentSession = {
            sessionId,
            startTime: Date.now(),
        };

        return this.currentSession;
    }

    /**
     * 获取当前会话
     */
    getCurrentSession(): SessionMetadata | null {
        return this.currentSession;
    }

    /**
     * 获取会话路径
     */
    getSessionPath(sessionId: string): string {
        return path.join(this.basePath, sessionId);
    }

    /**
     * 获取音频文件路径
     */
    getAudioPath(sessionId: string): string {
        return path.join(this.getSessionPath(sessionId), "audio.wav");
    }

    /**
     * 获取 VTT 文件路径
     */
    getVTTPath(sessionId: string): string {
        return path.join(this.getSessionPath(sessionId), "transcript.vtt");
    }

    /**
     * 获取元数据文件路径
     */
    getMetadataPath(sessionId: string): string {
        return path.join(this.getSessionPath(sessionId), "metadata.json");
    }

    /**
     * 结束会话并保存元数据
     */
    async endSession(
        sessionId: string,
        metadata: Partial<SessionMetadata>
    ): Promise<void> {
        if (this.currentSession && this.currentSession.sessionId === sessionId) {
            this.currentSession.endTime = Date.now();
            this.currentSession.duration =
                this.currentSession.endTime - this.currentSession.startTime;

            // 合并额外的元数据
            Object.assign(this.currentSession, metadata);

            // 设置文件路径
            this.currentSession.audioFile = this.getAudioPath(sessionId);
            this.currentSession.vttFile = this.getVTTPath(sessionId);

            // 保存元数据到文件
            await this.saveMetadata(sessionId, this.currentSession);

            // 清空当前会话
            this.currentSession = null;
        }
    }

    /**
     * 保存元数据到文件
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
     * 列出所有会话
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

            // 按时间倒序排序
            sessions.sort((a, b) => b.startTime - a.startTime);
        } catch (error) {
            console.error("Error listing sessions:", error);
        }

        return sessions;
    }

    /**
     * 删除会话
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
     * 获取会话元数据
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
