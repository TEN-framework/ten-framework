/**
 * WebSocket Manager for Voice Assistant
 * Handles connection, reconnection, and message parsing
 */

import type {
  AudioMessage,
  CmdMessage,
  DataMessage,
  ErrorMessage,
  WebSocketMessage,
} from "@/types";

export interface WebSocketManagerConfig {
  url: string;
  reconnectInterval?: number;
  maxReconnectAttempts?: number;
}

export type MessageHandler = (message: WebSocketMessage) => void;

export class WebSocketManager {
  private ws: WebSocket | null = null;
  private config: Required<WebSocketManagerConfig>;
  private reconnectAttempts = 0;
  private reconnectTimeout: NodeJS.Timeout | null = null;
  private isIntentionalClose = false;

  // Event handlers
  private onOpenHandlers: Set<() => void> = new Set();
  private onCloseHandlers: Set<() => void> = new Set();
  private onErrorHandlers: Set<(error: Event) => void> = new Set();
  private onMessageHandlers: Set<MessageHandler> = new Set();

  // Specific message type handlers
  private onAudioHandlers: Set<(message: AudioMessage) => void> = new Set();
  private onDataHandlers: Set<(message: DataMessage) => void> = new Set();
  private onCmdHandlers: Set<(message: CmdMessage) => void> = new Set();
  private onErrorMessageHandlers: Set<(message: ErrorMessage) => void> =
    new Set();

  constructor(config: WebSocketManagerConfig) {
    this.config = {
      url: config.url,
      reconnectInterval: config.reconnectInterval || 3000,
      maxReconnectAttempts: config.maxReconnectAttempts || 10,
    };
  }

  connect(): void {
    if (this.ws?.readyState === WebSocket.OPEN) {
      console.warn("WebSocket already connected");
      return;
    }

    this.isIntentionalClose = false;

    try {
      this.ws = new WebSocket(this.config.url);

      this.ws.onopen = () => {
        console.log("WebSocket connected");
        this.reconnectAttempts = 0;
        this.onOpenHandlers.forEach((handler) => handler());
      };

      this.ws.onclose = () => {
        console.log("WebSocket disconnected");
        this.onCloseHandlers.forEach((handler) => handler());

        if (!this.isIntentionalClose) {
          this.attemptReconnect();
        }
      };

      this.ws.onerror = (error) => {
        console.error("WebSocket error:", error);
        this.onErrorHandlers.forEach((handler) => handler(error));
      };

      this.ws.onmessage = (event) => {
        try {
          const message: WebSocketMessage = JSON.parse(event.data);
          this.handleMessage(message);
        } catch (error) {
          console.error("Failed to parse WebSocket message:", error);
        }
      };
    } catch (error) {
      console.error("Failed to create WebSocket:", error);
      this.attemptReconnect();
    }
  }

  private handleMessage(message: WebSocketMessage): void {
    // Notify generic message handlers
    this.onMessageHandlers.forEach((handler) => handler(message));

    // Notify specific type handlers
    switch (message.type) {
      case "audio":
        this.onAudioHandlers.forEach((handler) =>
          handler(message as AudioMessage),
        );
        break;
      case "data":
        this.onDataHandlers.forEach((handler) =>
          handler(message as DataMessage),
        );
        break;
      case "cmd":
        this.onCmdHandlers.forEach((handler) => handler(message as CmdMessage));
        break;
      case "error":
        this.onErrorMessageHandlers.forEach((handler) =>
          handler(message as ErrorMessage),
        );
        break;
    }
  }

  private attemptReconnect(): void {
    if (this.isIntentionalClose) {
      console.log("Intentional close - not reconnecting");
      return;
    }

    // If maxReconnectAttempts is -1, retry indefinitely
    if (
      this.config.maxReconnectAttempts !== -1 &&
      this.reconnectAttempts >= this.config.maxReconnectAttempts
    ) {
      console.log("Max reconnection attempts reached");
      return;
    }

    this.reconnectAttempts++;
    if (this.config.maxReconnectAttempts === -1) {
      console.log(
        `Attempting to reconnect... (attempt ${this.reconnectAttempts}, unlimited retries)`,
      );
    } else {
      console.log(
        `Attempting to reconnect... (${this.reconnectAttempts}/${this.config.maxReconnectAttempts})`,
      );
    }

    this.reconnectTimeout = setTimeout(() => {
      this.connect();
    }, this.config.reconnectInterval);
  }

  disconnect(): void {
    this.isIntentionalClose = true;

    if (this.reconnectTimeout) {
      clearTimeout(this.reconnectTimeout);
      this.reconnectTimeout = null;
    }

    if (this.ws) {
      this.ws.close();
      this.ws = null;
    }
  }

  send(data: any): void {
    if (this.ws?.readyState === WebSocket.OPEN) {
      this.ws.send(JSON.stringify(data));
    } else {
      console.warn("WebSocket not connected, cannot send message");
    }
  }

  sendAudio(audioBase64: string, metadata?: any): void {
    this.send({
      audio: audioBase64,
      metadata: metadata || {},
    });
  }

  // Event handler registration
  onOpen(handler: () => void): () => void {
    this.onOpenHandlers.add(handler);
    return () => this.onOpenHandlers.delete(handler);
  }

  onClose(handler: () => void): () => void {
    this.onCloseHandlers.add(handler);
    return () => this.onCloseHandlers.delete(handler);
  }

  onError(handler: (error: Event) => void): () => void {
    this.onErrorHandlers.add(handler);
    return () => this.onErrorHandlers.delete(handler);
  }

  onMessage(handler: MessageHandler): () => void {
    this.onMessageHandlers.add(handler);
    return () => this.onMessageHandlers.delete(handler);
  }

  onAudio(handler: (message: AudioMessage) => void): () => void {
    this.onAudioHandlers.add(handler);
    return () => this.onAudioHandlers.delete(handler);
  }

  onData(handler: (message: DataMessage) => void): () => void {
    this.onDataHandlers.add(handler);
    return () => this.onDataHandlers.delete(handler);
  }

  onCmd(handler: (message: CmdMessage) => void): () => void {
    this.onCmdHandlers.add(handler);
    return () => this.onCmdHandlers.delete(handler);
  }

  onErrorMessage(handler: (message: ErrorMessage) => void): () => void {
    this.onErrorMessageHandlers.add(handler);
    return () => this.onErrorMessageHandlers.delete(handler);
  }

  isConnected(): boolean {
    return this.ws?.readyState === WebSocket.OPEN;
  }

  getReadyState(): number | null {
    return this.ws?.readyState ?? null;
  }
}
