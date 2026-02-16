"use client";

import AgoraRTC, {
  type IAgoraRTCClient,
  type IMicrophoneAudioTrack,
  type IRemoteAudioTrack,
} from "agora-rtc-sdk-ng";
import { apiGenAgoraData, VideoSourceType } from "@/common";
import { AGEventEmitter } from "../events";
import type { IUserTracks, RtcEvents } from "./types";

export class RtcManager extends AGEventEmitter<RtcEvents> {
  private _joined;
  client: IAgoraRTCClient;
  localTracks: IUserTracks;
  appId: string | null = null;
  token: string | null = null;
  userId: number | null = null;

  constructor() {
    super();
    this._joined = false;
    this.localTracks = {};
    this.client = AgoraRTC.createClient({ mode: "rtc", codec: "vp8" });
    this._listenRtcEvents();
  }

  async join({ channel, userId }: { channel: string; userId: number }) {
    if (!this._joined) {
      const res = await apiGenAgoraData({ channel, userId });
      const { code, data } = res;
      if (String(code) !== "0") {
        throw new Error("Failed to get Agora token");
      }
      const { appId, token } = data;
      this.appId = appId;
      this.token = token;
      this.userId = userId;
      await this.client?.join(appId, channel, token, userId);
      this._joined = true;
    }
  }

  async createCameraTracks() {
    try {
      const videoTrack = await AgoraRTC.createCameraVideoTrack();
      this.localTracks.videoTrack = videoTrack;
    } catch (err) {
      console.error("Failed to create video track", err);
    }
    this.emit("localTracksChanged", this.localTracks);
  }

  async createMicrophoneAudioTrack() {
    try {
      const audioTrack = await AgoraRTC.createMicrophoneAudioTrack();
      this.localTracks.audioTrack = audioTrack;
    } catch (err) {
      console.error("Failed to create audio track", err);
    }
    this.emit("localTracksChanged", this.localTracks);
  }

  async createScreenShareTrack() {
    try {
      const screenTrack = await AgoraRTC.createScreenVideoTrack(
        {
          encoderConfig: {
            width: 1200,
            height: 800,
            frameRate: 5,
          },
        },
        "disable"
      );
      this.localTracks.screenTrack = screenTrack;
    } catch (err) {
      console.error("Failed to create screen track", err);
    }
    this.emit("localTracksChanged", this.localTracks);
  }

  async switchVideoSource(type: VideoSourceType) {
    if (type === VideoSourceType.SCREEN) {
      await this.createScreenShareTrack();
      if (this.localTracks.screenTrack) {
        this.client.unpublish(this.localTracks.videoTrack);
        this.localTracks.videoTrack?.close();
        this.localTracks.videoTrack = undefined;
        this.client.publish(this.localTracks.screenTrack);
        this.emit("localTracksChanged", this.localTracks);
      }
    } else if (type === VideoSourceType.CAMERA) {
      await this.createCameraTracks();
      if (this.localTracks.videoTrack) {
        this.client.unpublish(this.localTracks.screenTrack);
        this.localTracks.screenTrack?.close();
        this.localTracks.screenTrack = undefined;
        this.client.publish(this.localTracks.videoTrack);
        this.emit("localTracksChanged", this.localTracks);
      }
    }
  }

  async publish() {
    const tracks = [];
    if (this.localTracks.videoTrack) {
      tracks.push(this.localTracks.videoTrack);
    }
    if (this.localTracks.audioTrack) {
      tracks.push(this.localTracks.audioTrack);
    }
    if (tracks.length) {
      await this.client.publish(tracks);
    }
  }

  async destroy() {
    this.localTracks?.audioTrack?.close();
    this.localTracks?.videoTrack?.close();
    if (this._joined) {
      await this.client?.leave();
    }
    this._resetData();
  }

  // ----------- public methods ------------

  // -------------- private methods --------------
  private _listenRtcEvents() {
    this.client.on("network-quality", (quality) => {
      this.emit("networkQuality", quality);
    });
    this.client.on("user-published", async (user, mediaType) => {
      await this.client.subscribe(user, mediaType);
      if (mediaType === "audio") {
        this._playAudio(user.audioTrack);
      }
      this.emit("remoteUserChanged", {
        userId: user.uid,
        audioTrack: user.audioTrack,
        videoTrack: user.videoTrack,
      });
    });
    this.client.on("user-unpublished", async (user, mediaType) => {
      await this.client.unsubscribe(user, mediaType);
      this.emit("remoteUserChanged", {
        userId: user.uid,
        audioTrack: user.audioTrack,
        videoTrack: user.videoTrack,
      });
    });
  }

  _playAudio(
    audioTrack: IMicrophoneAudioTrack | IRemoteAudioTrack | undefined
  ) {
    if (audioTrack && !audioTrack.isPlaying) {
      audioTrack.play();
    }
  }

  private _resetData() {
    this.localTracks = {};
    this._joined = false;
  }
}

export const rtcManager = new RtcManager();
