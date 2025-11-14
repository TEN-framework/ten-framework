import AgoraRTC, {
    IAgoraRTCClient,
    IMicrophoneAudioTrack,
    IRemoteAudioTrack,
    ConnectionState,
    NetworkQuality
} from 'agora-rtc-sdk-ng';
import { AgoraConfig, ConnectionStatus, TranscriptMessage } from '@/types';
import axios from 'axios';

type TextChunk = {
    messageId: string;
    partIndex: number;
    totalParts: number;
    content: string;
};

export class AgoraService {
    private rtcClient: IAgoraRTCClient | null = null;
    private localAudioTrack: IMicrophoneAudioTrack | null = null;
    private remoteAudioTrack: IRemoteAudioTrack | null = null;
    private config: AgoraConfig | null = null;
    private connectionStatus: ConnectionStatus = {
        rtc: 'disconnected',
        rtm: 'disconnected',
        agent: 'stopped'
    };

    // Event callbacks
    private onConnectionStatusChange?: (status: ConnectionStatus) => void;
    private onRemoteAudioTrack?: (track: IRemoteAudioTrack | null) => void;
    private onNetworkQuality?: (quality: NetworkQuality) => void;
    private transcriptListeners: Set<(message: TranscriptMessage) => void> = new Set();
    private messageCache: Record<string, TextChunk[]> = {};

    constructor() {
        if (typeof window !== 'undefined') {
            this.initializeAgora();
        }
    }

    private async initializeAgora() {
        try {
            // Initialize RTC client
            this.rtcClient = AgoraRTC.createClient({
                mode: 'rtc',
                codec: 'vp8'
            });

            // Set up RTC event listeners
            this.setupRTCEventListeners();
        } catch (error) {
            console.error('Failed to initialize Agora:', error);
        }
    }

    private setupRTCEventListeners() {
        if (!this.rtcClient) return;

        this.rtcClient.on('connection-state-change', (curState: ConnectionState) => {
            this.connectionStatus.rtc = curState === 'CONNECTED' ? 'connected' :
                curState === 'CONNECTING' ? 'connecting' : 'disconnected';
            this.onConnectionStatusChange?.(this.connectionStatus);
        });

        this.rtcClient.on('user-published', async (user, mediaType) => {
            if (mediaType === 'audio') {
                await this.rtcClient!.subscribe(user, mediaType);
                this.remoteAudioTrack = user.audioTrack as IRemoteAudioTrack;

                // Play the remote audio track
                this.remoteAudioTrack.play();

                this.onRemoteAudioTrack?.(this.remoteAudioTrack);
            }
        });

        this.rtcClient.on('user-unpublished', (user, mediaType) => {
            if (mediaType === 'audio') {
                if (this.remoteAudioTrack) {
                    this.remoteAudioTrack.stop();
                }
                this.remoteAudioTrack = null;
            }
        });

        this.rtcClient.on('network-quality', (stats) => {
            this.onNetworkQuality?.(stats);
        });

        this.rtcClient.on('stream-message', (_uid: any, stream: any) => {
            this.handleStreamMessage(stream);
        });
    }

    // RTM functionality will be added later

    async fetchCredentials(channelName: string, uid: number, baseUrl: string = 'http://localhost:8080'): Promise<AgoraConfig | null> {
        try {
            const response = await axios.post(`${baseUrl}/token/generate`, {
                request_id: `token-${Date.now()}`,
                channel_name: channelName,
                uid: uid
            });

            if (response.status === 200 && response.data.code === 0) {
                const data = response.data.data;
                return {
                    appId: data.appId,
                    channel: data.channel_name,
                    token: data.token,
                    uid: data.uid
                };
            }

            return null;
        } catch (error) {
            console.error('Failed to fetch Agora credentials:', error);
            return null;
        }
    }

    async connect(config: AgoraConfig): Promise<boolean> {
        if (typeof window === 'undefined') return false;

        try {
            this.config = config;

            // Connect to RTC
            if (this.rtcClient) {
                await this.rtcClient.join(config.appId, config.channel, config.token || null, config.uid);

                // Create and publish local audio track
                this.localAudioTrack = await AgoraRTC.createMicrophoneAudioTrack();
                await this.rtcClient.publish([this.localAudioTrack]);
            }

            return true;
        } catch (error) {
            console.error('Failed to connect to Agora:', error);
            return false;
        }
    }

    async disconnect(): Promise<void> {
        try {
            // Notify that we're disconnecting to allow components to clean up first
            this.connectionStatus = {
                rtc: 'disconnected',
                rtm: 'disconnected',
                agent: 'stopped'
            };
            this.onConnectionStatusChange?.(this.connectionStatus);

            // Stop and unpublish local audio track
            if (this.localAudioTrack) {
                try {
                    this.localAudioTrack.stop();
                    this.localAudioTrack.close();
                } catch (trackError) {
                    console.warn('Error stopping local audio track:', trackError);
                }
                this.localAudioTrack = null;
            }

            // Stop remote audio track and notify components
            if (this.remoteAudioTrack) {
                try {
                    this.remoteAudioTrack.stop();
                } catch (trackError) {
                    console.warn('Error stopping remote audio track:', trackError);
                }
                // Notify components that remote track is gone
                this.onRemoteAudioTrack?.(null);
                this.remoteAudioTrack = null;
            }

            // Leave RTC channel
            if (this.rtcClient) {
                try {
                    await this.rtcClient.leave();
                } catch (leaveError) {
                    console.warn('Error leaving RTC channel:', leaveError);
                }
            }
        } catch (error) {
            console.error('Failed to disconnect from Agora:', error);
        }
    }

    async sendTranscriptMessage(message: TranscriptMessage): Promise<void> {
        // RTM functionality will be added later
        console.log('Transcript message:', message);
    }

    private addToMessageCache(chunk: TextChunk) {
        const cache = this.messageCache;
        if (!cache[chunk.messageId]) {
            cache[chunk.messageId] = [];
            // cleanup timer to avoid memory leak in case total parts never arrive
            setTimeout(() => {
                if (cache[chunk.messageId]?.length !== chunk.totalParts) {
                    delete cache[chunk.messageId];
                }
            }, 15_000);
        }
        cache[chunk.messageId].push(chunk);
    }

    private reconstructMessage(messageId: string): string | null {
        const chunks = this.messageCache[messageId];
        if (!chunks || chunks.length === 0) {
            return null;
        }
        const sorted = [...chunks].sort((a, b) => a.partIndex - b.partIndex);
        const combined = sorted.map((chunk) => chunk.content).join('');
        delete this.messageCache[messageId];
        return combined;
    }

    private decodeBase64ToUtf8(base64: string): string {
        if (typeof window !== 'undefined') {
            const binary = window.atob(base64);
            const bytes = new Uint8Array(binary.length);
            for (let i = 0; i < binary.length; i += 1) {
                bytes[i] = binary.charCodeAt(i);
            }
            return new TextDecoder().decode(bytes);
        }
        return Buffer.from(base64, 'base64').toString('utf-8');
    }

    private emitTranscript(message: TranscriptMessage) {
        this.transcriptListeners.forEach((listener) => {
            try {
                listener(message);
            } catch (error) {
                console.warn('[AgoraService] Transcript listener error', error);
            }
        });
    }

    private handleStreamMessage(streamData: any) {
        try {
            const ascii = String.fromCharCode(...new Uint8Array(streamData));
            const [messageId, partIndexStr, totalPartsStr, content] = ascii.split('|');

            const partIndex = parseInt(partIndexStr, 10);
            const totalParts = totalPartsStr === '???' ? -1 : parseInt(totalPartsStr, 10);

            if (!messageId || Number.isNaN(partIndex) || Number.isNaN(totalParts)) {
                return;
            }

            if (totalParts === -1) {
                return;
            }

            this.addToMessageCache({
                messageId,
                partIndex,
                totalParts,
                content,
            });

            const cached = this.messageCache[messageId];
            if (!cached || cached.length !== totalParts) {
                return;
            }

            const reconstructed = this.reconstructMessage(messageId);
            if (!reconstructed) {
                return;
            }

            const payloadRaw = this.decodeBase64ToUtf8(reconstructed);
            const payload = JSON.parse(payloadRaw);
            const text: string = payload?.text ?? '';
            if (!text || !text.trim()) {
                return;
            }

            const timestampSource = payload?.text_ts ?? Date.now();
            const timestamp = new Date(timestampSource);

            const roleRaw = payload?.role;
            const roleLower =
                typeof roleRaw === 'string' ? roleRaw.toLowerCase() : '';
            const speakerRaw = payload?.speaker;
            const speaker =
                typeof speakerRaw === 'string' ? speakerRaw.toLowerCase() : '';
            let isUser = true;
            if (roleLower) {
                if (roleLower === 'assistant' || roleLower === 'agent' || roleLower === 'system') {
                    isUser = false;
                } else if (
                    roleLower === 'user' ||
                    roleLower === 'client' ||
                    roleLower === 'participant' ||
                    roleLower === 'speaker' ||
                    roleLower === 'speaker0'
                ) {
                    isUser = true;
                }
            } else if (speaker) {
                const lowered = speaker.toLowerCase();
                if (
                    lowered.includes('assistant') ||
                    lowered.includes('speaker1') ||
                    lowered.includes('agent')
                ) {
                    isUser = false;
                } else if (
                    lowered.includes('user') ||
                    lowered.includes('speaker0') ||
                    lowered.includes('participant')
                ) {
                    isUser = true;
                }
            }

            const message: TranscriptMessage = {
                id: messageId,
                text,
                timestamp,
                isUser,
                confidence: typeof payload?.confidence === 'number' ? payload.confidence : undefined,
                isFinal: Boolean(payload?.is_final ?? payload?.isFinal ?? true),
            };

            this.emitTranscript(message);
        } catch (error) {
            console.warn('[AgoraService] Failed to parse stream message', error);
        }
    }

    // Getters
    getConnectionStatus(): ConnectionStatus {
        return this.connectionStatus;
    }

    getRemoteAudioTrack(): IRemoteAudioTrack | null {
        return this.remoteAudioTrack;
    }

    getLocalAudioTrack(): IMicrophoneAudioTrack | null {
        return this.localAudioTrack;
    }

    // Microphone control methods
    muteMicrophone(): void {
        if (this.localAudioTrack) {
            this.localAudioTrack.setEnabled(false);
        }
    }

    unmuteMicrophone(): void {
        if (this.localAudioTrack) {
            this.localAudioTrack.setEnabled(true);
        }
    }

    isMicrophoneMuted(): boolean {
        return this.localAudioTrack ? !this.localAudioTrack.enabled : false;
    }

    // Event setters
    setOnConnectionStatusChange(callback: (status: ConnectionStatus) => void) {
        this.onConnectionStatusChange = callback;
    }

    setOnTranscriptMessage(callback: (message: TranscriptMessage) => void) {
        this.transcriptListeners.clear();
        this.transcriptListeners.add(callback);
    }

    addTranscriptListener(listener: (message: TranscriptMessage) => void): () => void {
        this.transcriptListeners.add(listener);
        return () => {
            this.transcriptListeners.delete(listener);
        };
    }

    setOnRemoteAudioTrack(callback: (track: IRemoteAudioTrack | null) => void) {
        this.onRemoteAudioTrack = callback;
    }

    setOnNetworkQuality(callback: (quality: NetworkQuality) => void) {
        this.onNetworkQuality = callback;
    }
}

// Singleton instance
export const agoraService = new AgoraService();
