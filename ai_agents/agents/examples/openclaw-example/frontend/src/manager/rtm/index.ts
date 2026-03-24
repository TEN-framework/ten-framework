"use client";

import type { RTMClient } from "agora-rtm";
import { ERTMTextType, type IRTMTextItem } from "@/types";
import { AGEventEmitter } from "../events";

export interface IOpenclawResultMessage {
  data_type: "openclaw_result";
  text?: string;
  ts?: number;
}

export interface IOpenclawPhaseMessage {
  data_type: "openclaw_phase";
  phase?: string;
}

export type TRtmMessage =
  | IRTMTextItem
  | IOpenclawResultMessage
  | IOpenclawPhaseMessage;

export interface IRtmEvents {
  rtmMessage: (text: TRtmMessage) => void;
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
  private _rtmCtor: any;
  channel: string = "";
  userId: number = 0;
  appId: string = "";
  token: string = "";
  private readonly _onMessageBound: (e: TRTMMessageEvent) => Promise<void>;
  private readonly _onPresenceBound: (e: unknown) => Promise<void>;

  constructor() {
    super();
    this._joined = false;
    this._client = null;
    this._rtmCtor = null;
    this._onMessageBound = this.handleRtmMessage.bind(this);
    this._onPresenceBound = this.handleRtmPresence.bind(this);
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
      return;
    }
    if (!this._rtmCtor) {
      const mod = await import("agora-rtm");
      this._rtmCtor = mod.default;
    }
    this.channel = channel;
    this.userId = userId;
    this.appId = appId;
    this.token = token;
    const rtm = new this._rtmCtor.RTM(appId, String(userId), {
      logLevel: "debug", // TODO: use INFO
      // update config: https://doc.shengwang.cn/api-ref/rtm2/javascript/toc-configuration/configuration#rtmConfig
    });
    await rtm.login({ token });
    try {
      // subscribe message channel(will be created automatically)
      const subscribeResult = await rtm.subscribe(channel, {
        withMessage: true,
      });
      console.log(
        "[RTM] Subscribe Message Channel success!: ",
        subscribeResult
      );

      this._joined = true;
      this._client = rtm;

      // listen events
      this._listenRtmEvents();
    } catch (status) {
      console.error("Failed to Create/Join Message Channel", status);
    }
  }

  private _listenRtmEvents() {
    this._client?.addEventListener("message", this._onMessageBound);
    // tmp add presence
    this._client?.addEventListener("presence", this._onPresenceBound);
    console.log("[RTM] Listen RTM events success!");
  }

  async handleRtmMessage(e: TRTMMessageEvent) {
    console.log("[RTM] [TRTMMessageEvent] RAW", JSON.stringify(e));
    const { message, messageType } = e;
    if (messageType === "STRING") {
      const parsed = JSON.parse(message as string);
      if (
        parsed?.data_type === "openclaw_result" ||
        parsed?.data_type === "openclaw_phase"
      ) {
        this.emit(
          "rtmMessage",
          parsed as IOpenclawResultMessage | IOpenclawPhaseMessage
        );
        return;
      }
      const msg: IRTMTextItem = parsed;
      if (msg?.type) {
        console.log("[RTM] Emitting rtmMessage event with msg:", msg);
        this.emit("rtmMessage", msg);
      }
    }
    if (messageType === "BINARY") {
      const decoder = new TextDecoder("utf-8");
      const decodedMessage = decoder.decode(message as Uint8Array);
      const parsed = JSON.parse(decodedMessage);
      if (
        parsed?.data_type === "openclaw_result" ||
        parsed?.data_type === "openclaw_phase"
      ) {
        this.emit(
          "rtmMessage",
          parsed as IOpenclawResultMessage | IOpenclawPhaseMessage
        );
        return;
      }
      const msg: IRTMTextItem = parsed;
      if (msg?.type) {
        this.emit("rtmMessage", msg);
      }
    }
  }

  async handleRtmPresence(e: unknown) {
    console.log("[RTM] [TRTMPresenceEvent] RAW", JSON.stringify(e));
  }

  async sendText(text: string) {
    const msg: IRTMTextItem = {
      is_final: true,
      ts: Date.now(),
      text,
      type: ERTMTextType.INPUT_TEXT,
      stream_id: String(this.userId),
    };
    await this._client?.publish(this.channel, JSON.stringify(msg), {
      customType: "PainTxt",
    });
    this.emit("rtmMessage", msg);
  }

  async destroy() {
    // remove listener
    this._client?.removeEventListener(
      "message",
      this._onMessageBound
    );
    this._client?.removeEventListener(
      "presence",
      this._onPresenceBound
    );
    // unsubscribe
    await this._client?.unsubscribe(this.channel);
    // logout
    await this._client?.logout();

    this._client = null;
    this._joined = false;
  }
}

export const rtmManager = new RtmManager();
