export interface AgentStatus {
    isRunning: boolean;
    isConnected: boolean;
    lastPing?: Date;
    error?: string;
}

export interface AgoraConfig {
    appId: string;
    channel: string;
    token?: string;
    uid?: number;
}

export interface Live2DModel {
    id: string;
    name: string;
    path: string;
    preview?: string;
}

export interface TranscriptMessage {
    id: string;
    text: string;
    timestamp: Date;
    isUser: boolean;
    confidence?: number;
    isFinal?: boolean;
}

export interface VoiceSettings {
    volume: number;
    pitch: number;
    speed: number;
    voice: string;
}

export interface ConnectionStatus {
    rtc: 'connected' | 'connecting' | 'disconnected' | 'error';
    rtm: 'connected' | 'connecting' | 'disconnected' | 'error';
    agent: 'running' | 'stopped' | 'error';
}
