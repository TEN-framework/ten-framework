import AgoraRTC, {
    IAgoraRTCClient,
    IMicrophoneAudioTrack,
    IRemoteAudioTrack,
    ConnectionState,
    NetworkQuality
} from 'agora-rtc-sdk-ng';
import { AgoraConfig, ConnectionStatus, TranscriptMessage } from '@/types';

export class AgoraService {
    private rtcClient: IAgoraRTCClient | null = null;
    private screenShareClient: IAgoraRTCClient | null = null; // Second client for screen sharing
    private localAudioTrack: IMicrophoneAudioTrack | null = null;
    private remoteAudioTrack: IRemoteAudioTrack | null = null;
    private screenTrack: any = null;
    private screenAudioTrack: any = null;
    private config: AgoraConfig | null = null;
    private screenShareConfig: AgoraConfig | null = null;
    private connectionStatus: ConnectionStatus = {
        rtc: 'disconnected',
        rtm: 'disconnected',
        agent: 'stopped'
    };
    private isScreenSharing: boolean = false;

    // Event callbacks
    private onConnectionStatusChange?: (status: ConnectionStatus) => void;
    private onTranscriptMessage?: (message: TranscriptMessage) => void;
    private onRemoteAudioTrack?: (track: IRemoteAudioTrack | null) => void;
    private onNetworkQuality?: (quality: NetworkQuality) => void;
    private onLocalVolumeChange?: (level: number) => void;
    private onDanmakuMessage?: (uid: number, message: string) => void;

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

        // Enable audio volume indicator for detecting when user is speaking
        try {
            // @ts-ignore - enableAudioVolumeIndicator might not be in all versions
            this.rtcClient.enableAudioVolumeIndicator?.();
            this.rtcClient.on('volume-indicator', (volumes: any[]) => {
                // Find local user's volume
                volumes.forEach((vol) => {
                    if (vol.uid === this.config?.uid || vol.uid === 0) {
                        // uid 0 or current uid means local user
                        if (this.onLocalVolumeChange) {
                            this.onLocalVolumeChange(vol.level);
                        }
                    }
                });
            });
        } catch (error) {
            console.warn('[AgoraService] Failed to enable volume indicator:', error);
        }

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
    }

    // RTM functionality will be added later

    async fetchCredentials(channelName: string, uid: number, baseUrl: string = 'http://localhost:8080'): Promise<AgoraConfig | null> {
        try {
            // Use Next.js API route for token generation
            const response = await fetch('/api/token/generate', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    request_id: `token-${Date.now()}`,
                    channel_name: channelName,
                    uid: uid
                }),
            });

            if (!response.ok) {
                console.error('Token generation failed:', response.statusText);
                return null;
            }

            const responseData = await response.json();

            // Handle the response structure from agent server
            const credentials = responseData.data || responseData;

            return {
                appId: credentials.appId || credentials.app_id,
                channel: channelName,
                token: credentials.token,
                uid: uid
            };
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
        this.onTranscriptMessage = callback;
    }

    setOnRemoteAudioTrack(callback: (track: IRemoteAudioTrack | null) => void) {
        this.onRemoteAudioTrack = callback;
    }

    setOnNetworkQuality(callback: (quality: NetworkQuality) => void) {
        this.onNetworkQuality = callback;
    }

    setOnLocalVolumeChange(callback: (level: number) => void) {
        this.onLocalVolumeChange = callback;
    }

    setOnDanmakuMessage(callback: (uid: number, message: string) => void) {
        this.onDanmakuMessage = callback;
    }

    // Screen sharing methods
    async startScreenShare(broadcastChannel: string): Promise<boolean> {
        console.log('[AgoraService] Starting screen share process...');

        try {
            if (this.isScreenSharing) {
                console.warn('[AgoraService] Screen sharing already active');
                return false;
            }

            if (!this.config) {
                const errorMsg = 'Main RTC connection not established. Please connect first.';
                console.error('[AgoraService]', errorMsg);
                throw new Error(errorMsg);
            }

            console.log('[AgoraService] Step 1: Creating second RTC client for screen sharing...');
            // Create a second client for screen sharing
            this.screenShareClient = AgoraRTC.createClient({
                mode: 'rtc',
                codec: 'vp8'
            });
            console.log('[AgoraService] ✓ Second RTC client created');

            // Generate a new token for the broadcast channel using the same API endpoint
            const screenShareUid = Math.floor(Math.random() * 100000) + 100000;

            console.log('[AgoraService] Step 2: Fetching token for broadcast channel:', broadcastChannel, 'UID:', screenShareUid);
            const credentials = await this.fetchCredentials(
                broadcastChannel,
                screenShareUid,
                window.location.origin // Use current origin for API calls
            );

            if (!credentials) {
                const errorMsg = 'Failed to get credentials for broadcast channel. Please check your API server.';
                console.error('[AgoraService]', errorMsg);
                throw new Error(errorMsg);
            }

            this.screenShareConfig = credentials;
            console.log('[AgoraService] ✓ Got credentials for screen share');
            console.log('[AgoraService] AppId:', credentials.appId);
            console.log('[AgoraService] Channel:', broadcastChannel);
            console.log('[AgoraService] UID:', screenShareUid);

            console.log('[AgoraService] Step 3: Creating screen share track...');
            console.log('[AgoraService] Please select the screen/window to share in the browser dialog...');

            // Create screen share track
            const screenTrack = await AgoraRTC.createScreenVideoTrack({
                encoderConfig: '1080p_2',
                optimizationMode: 'detail'
            }, 'auto'); // 'auto' tries to capture system audio

            console.log('[AgoraService] ✓ Screen share track created');

            // Handle both video-only and video+audio cases
            if (Array.isArray(screenTrack)) {
                this.screenTrack = screenTrack[0];
                this.screenAudioTrack = screenTrack[1];
                console.log('[AgoraService] Screen sharing with system audio enabled');
            } else {
                this.screenTrack = screenTrack;
                console.log('[AgoraService] Screen sharing (video only, no system audio)');
            }

            // Set up track-ended event handler
            this.screenTrack.on('track-ended', () => {
                console.log('[AgoraService] Screen sharing stopped by user');
                this.stopScreenShare();
            });

            console.log('[AgoraService] Step 4: Joining broadcast channel...');

            // Set up stream-message listener for danmaku
            this.screenShareClient.on('stream-message', (uid, payload) => {
                try {
                    const decoder = new TextDecoder();
                    const message = decoder.decode(payload);
                    const numericUid = typeof uid === 'string' ? parseInt(uid) : uid;
                    console.log('[AgoraService] Received danmaku from', numericUid, ':', message);
                    this.onDanmakuMessage?.(numericUid, message);
                } catch (error) {
                    console.error('[AgoraService] Failed to decode danmaku message:', error);
                }
            });

            // Join the broadcast channel with screen share client
            await this.screenShareClient.join(
                credentials.appId,
                broadcastChannel,
                credentials.token || null,
                credentials.uid
            );
            console.log('[AgoraService] ✓ Joined broadcast channel');

            console.log('[AgoraService] Step 5: Publishing screen tracks...');
            // Publish screen tracks
            const tracksToPublish = this.screenAudioTrack
                ? [this.screenTrack, this.screenAudioTrack]
                : [this.screenTrack];

            await this.screenShareClient.publish(tracksToPublish);
            console.log('[AgoraService] ✓ Screen tracks published');

            this.isScreenSharing = true;
            console.log('[AgoraService] ✅ Screen sharing started successfully on channel:', broadcastChannel);
            return true;

        } catch (error: any) {
            console.error('[AgoraService] ❌ Failed to start screen sharing');
            console.error('[AgoraService] Error name:', error?.name);
            console.error('[AgoraService] Error message:', error?.message);
            console.error('[AgoraService] Error code:', error?.code);
            console.error('[AgoraService] Full error:', error);

            // Provide user-friendly error messages
            if (error?.message?.includes('Permission denied') || error?.name === 'NotAllowedError') {
                console.error('[AgoraService] User denied screen sharing permission');
            } else if (error?.message?.includes('not established')) {
                console.error('[AgoraService] Please connect to voice channel first before starting broadcast');
            } else if (error?.message?.includes('credentials')) {
                console.error('[AgoraService] Failed to get broadcast token from server');
            }

            await this.stopScreenShare();
            return false;
        }
    }

    async stopScreenShare(): Promise<void> {
        try {
            if (!this.isScreenSharing) {
                return;
            }

            // Stop and close screen tracks
            if (this.screenTrack) {
                this.screenTrack.stop();
                this.screenTrack.close();
                this.screenTrack = null;
            }

            if (this.screenAudioTrack) {
                this.screenAudioTrack.stop();
                this.screenAudioTrack.close();
                this.screenAudioTrack = null;
            }

            // Leave the broadcast channel
            if (this.screenShareClient) {
                await this.screenShareClient.leave();
                this.screenShareClient = null;
            }

            this.isScreenSharing = false;
            this.screenShareConfig = null;
            console.log('[AgoraService] Screen sharing stopped successfully');

        } catch (error) {
            console.error('[AgoraService] Error stopping screen sharing:', error);
        }
    }

    isCurrentlyScreenSharing(): boolean {
        return this.isScreenSharing;
    }

    getScreenTrack(): any {
        return this.screenTrack;
    }
}

// Singleton instance
export const agoraService = new AgoraService();
