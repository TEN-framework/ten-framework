"use client";

import AgoraRTM, { type RTMClient, type RTMStreamChannel } from "agora-rtm";
import { apiGenAgoraData } from "@/common";
import { type IRTMTextItem } from "@/types";
import { AGEventEmitter } from "../events";

export interface IRtmEvents {
  rtmMessage: (text: IRTMTextItem) => void;
}

export type TRTMMessageEvent = {
  channelType: "STREAM" | "MESSAGE" | "USER";
  channelName: string;
  topicName?: string;
  messageType: "STRING" | "BINARY";
  customType?: string;
  publisher: string;
  message: string | Uint8Array;
  timestamp: number;
};

export class RtmManager extends AGEventEmitter<IRtmEvents> {
  private _joined: boolean;
  _client: RTMClient | null;
  channel: string = "";
  userId: number = 0;
  appId: string = "";
  token: string = "";

  constructor() {
    super();
    this._joined = false;
    this._client = null;
  }

  async init({
    channel,
    userId,
    appId,
    token,
  }: {
    channel: string;
    userId: number;
    appId: string;
    token: string;
  }) {
    if (this._joined) {
      console.log("[RTM] Already initialized");
      return;
    }

    try {
      this.channel = channel;
      this.userId = userId;
      this.appId = appId;
      this.token = token;

      const rtm = new AgoraRTM.RTM(appId, String(userId));

      await rtm.login({ token });
      console.log("[RTM] Login successful");

      const subscribeResult = await rtm.subscribe(channel, {
        withMessage: true,
        withPresence: true,
        beQuiet: false,
        withMetadata: true,
        withLock: true,
      });
      console.log("[RTM] Subscribe successful:", subscribeResult);

      this._joined = true;
      this._client = rtm;

      this._listenRtmEvents();
    } catch (error) {
      console.error("[RTM] Initialization failed:", error);
      this._joined = false;
      this._client = null;
      throw error;
    }
  }

  private _listenRtmEvents() {
    this._client!.addEventListener("message", this.handleRtmMessage.bind(this));
    // tmp add presence
    this._client!.addEventListener(
      "presence",
      this.handleRtmPresence.bind(this)
    );
    console.log("[RTM] Listen RTM events success!");
  }

  async handleRtmMessage(e: TRTMMessageEvent) {
    console.log("[RTM] [TRTMMessageEvent] RAW", JSON.stringify(e));
    const { message, messageType } = e;
    if (messageType === "STRING") {
      const msg: IRTMTextItem = JSON.parse(message as string);
      if (msg) {
        console.log("[RTM] Emitting rtmMessage event with msg:", msg);
        this.emit("rtmMessage", msg);
      }
    }
    if (messageType === "BINARY") {
      const decoder = new TextDecoder("utf-8");
      const decodedMessage = decoder.decode(message as Uint8Array);
      const msg: IRTMTextItem = JSON.parse(decodedMessage);
      this.emit("rtmMessage", msg);
    }
  }

  async handleRtmPresence(e: any) {
    console.log("[RTM] [TRTMPresenceEvent] RAW", JSON.stringify(e));
  }

  async sendText(text: string) {
    const msg: IRTMTextItem = {
      is_final: true,
      text_ts: Date.now(),
      text,
      data_type: "input_text",
      role: "user",
      stream_id: this.userId,
    };
    await this._client?.publish(this.channel, JSON.stringify(msg), {
      customType: "PainTxt",
    });
    this.emit("rtmMessage", msg);
  }

  async destroy() {
    if (!this._client || !this._joined) {
      console.log("[RTM] Already destroyed or not initialized");
      return;
    }

    try {
      // Remove event listeners
      this._client.removeEventListener(
        "message",
        this.handleRtmMessage.bind(this)
      );
      this._client.removeEventListener(
        "presence",
        this.handleRtmPresence.bind(this)
      );

      // Unsubscribe from channel
      await this._client.unsubscribe(this.channel);
      console.log("[RTM] Unsubscribed from channel");

      // Logout
      await this._client.logout();
      console.log("[RTM] Logout successful");

      this._client = null;
      this._joined = false;
    } catch (error) {
      console.error("[RTM] Destroy failed:", error);
      // Reset state anyway
      this._client = null;
      this._joined = false;
    }
  }
}

export const rtmManager = new RtmManager();
