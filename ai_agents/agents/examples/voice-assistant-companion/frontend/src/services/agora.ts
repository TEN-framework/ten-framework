import AgoraRTC, {
  type ConnectionState,
  type IAgoraRTCClient,
  type IMicrophoneAudioTrack,
  type IRemoteAudioTrack,
  type NetworkQuality,
} from "agora-rtc-sdk-ng";
import axios from "axios";
import type { AgoraConfig, ConnectionStatus, TranscriptMessage } from "@/types";

export class AgoraService {
  private rtcClient: IAgoraRTCClient | null = null;
  private localAudioTrack: IMicrophoneAudioTrack | null = null;
  private remoteAudioTrack: IRemoteAudioTrack | null = null;
  private config: AgoraConfig | null = null;
  private connectionStatus: ConnectionStatus = {
    rtc: "disconnected",
    rtm: "disconnected",
    agent: "stopped",
  };

  // Event callbacks
  private onConnectionStatusChange?: (status: ConnectionStatus) => void;
  private onRemoteAudioTrack?: (track: IRemoteAudioTrack | null) => void;
  private onNetworkQuality?: (quality: NetworkQuality) => void;
  private onTranscriptMessage?: (message: TranscriptMessage) => void;

  constructor() {
    if (typeof window !== "undefined") {
      this.initializeAgora();
    }
  }

  private async initializeAgora() {
    try {
      // Initialize RTC client
      this.rtcClient = AgoraRTC.createClient({
        mode: "rtc",
        codec: "vp8",
      });

      // Set up RTC event listeners
      this.setupRTCEventListeners();
    } catch (error) {
      console.error("Failed to initialize Agora:", error);
    }
  }

  private setupRTCEventListeners() {
    if (!this.rtcClient) return;

    this.rtcClient.on(
      "connection-state-change",
      (curState: ConnectionState) => {
        this.connectionStatus.rtc =
          curState === "CONNECTED"
            ? "connected"
            : curState === "CONNECTING"
              ? "connecting"
              : "disconnected";
        this.onConnectionStatusChange?.(this.connectionStatus);
      }
    );

    this.rtcClient.on("user-published", async (user, mediaType) => {
      if (mediaType === "audio") {
        await this.rtcClient?.subscribe(user, mediaType);
        this.remoteAudioTrack = user.audioTrack as IRemoteAudioTrack;

        // Play the remote audio track
        this.remoteAudioTrack.play();

        this.onRemoteAudioTrack?.(this.remoteAudioTrack);
      }
    });

    this.rtcClient.on("user-unpublished", (_user, mediaType) => {
      if (mediaType === "audio") {
        if (this.remoteAudioTrack) {
          this.remoteAudioTrack.stop();
        }
        this.remoteAudioTrack = null;
      }
    });

    this.rtcClient.on("network-quality", (stats) => {
      this.onNetworkQuality?.(stats);
    });
  }

  // RTM functionality will be added later

  async fetchCredentials(
    channelName: string,
    uid: number,
    baseUrl: string = "http://localhost:8080"
  ): Promise<AgoraConfig | null> {
    try {
      const response = await axios.post(`${baseUrl}/token/generate`, {
        request_id: `token-${Date.now()}`,
        channel_name: channelName,
        uid: uid,
      });

      if (response.status === 200 && response.data.code === 0) {
        const data = response.data.data;
        return {
          appId: data.appId,
          channel: data.channel_name,
          token: data.token,
          uid: data.uid,
        };
      }

      return null;
    } catch (error) {
      console.error("Failed to fetch Agora credentials:", error);
      return null;
    }
  }

  async connect(config: AgoraConfig): Promise<boolean> {
    if (typeof window === "undefined") return false;

    try {
      this.config = config;

      // Connect to RTC
      if (this.rtcClient) {
        await this.rtcClient.join(
          config.appId,
          config.channel,
          config.token || null,
          config.uid
        );

        // Create and publish local audio track
        this.localAudioTrack = await AgoraRTC.createMicrophoneAudioTrack();
        await this.rtcClient.publish([this.localAudioTrack]);
      }

      return true;
    } catch (error) {
      console.error("Failed to connect to Agora:", error);
      return false;
    }
  }

  async disconnect(): Promise<void> {
    try {
      // Notify that we're disconnecting to allow components to clean up first
      this.connectionStatus = {
        rtc: "disconnected",
        rtm: "disconnected",
        agent: "stopped",
      };
      this.onConnectionStatusChange?.(this.connectionStatus);

      // Stop and unpublish local audio track
      if (this.localAudioTrack) {
        try {
          this.localAudioTrack.stop();
          this.localAudioTrack.close();
        } catch (trackError) {
          console.warn("Error stopping local audio track:", trackError);
        }
        this.localAudioTrack = null;
      }

      // Stop remote audio track and notify components
      if (this.remoteAudioTrack) {
        try {
          this.remoteAudioTrack.stop();
        } catch (trackError) {
          console.warn("Error stopping remote audio track:", trackError);
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
          console.warn("Error leaving RTC channel:", leaveError);
        }
      }
    } catch (error) {
      console.error("Failed to disconnect from Agora:", error);
    }
  }

  async sendTranscriptMessage(message: TranscriptMessage): Promise<void> {
    // RTM functionality will be added later
    console.log("Transcript message:", message);
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
}

// Singleton instance
export const agoraService = new AgoraService();
