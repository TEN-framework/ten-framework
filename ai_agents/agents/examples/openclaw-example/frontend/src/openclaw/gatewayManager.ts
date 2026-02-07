import { genUUID } from "@/common/utils";
import {
  OPENCLAW_CHAT_SESSION_KEY,
  OPENCLAW_GATEWAY_CLIENT_ID,
  OPENCLAW_GATEWAY_CLIENT_MODE,
  OPENCLAW_GATEWAY_SCOPES,
  OPENCLAW_GATEWAY_TOKEN,
  OPENCLAW_GATEWAY_URL,
} from "@/common/constant";
import { AGEventEmitter } from "@/manager/events";
import { OpenclawGatewayClient } from "./gatewayClient";

export type OpenclawGatewayEvents = {
  agentPhase: (phase: string) => void;
  openclawResponse: (text: string, timestamp: number) => void;
};

type AgentEventPayload = {
  data?: Record<string, unknown>;
};

class OpenclawGatewayManager extends AGEventEmitter<OpenclawGatewayEvents> {
  private client: OpenclawGatewayClient | null = null;
  private connected = false;

  connect() {
    if (this.client) {
      return;
    }
    const scopes = OPENCLAW_GATEWAY_SCOPES.split(",")
      .map((scope) => scope.trim())
      .filter(Boolean);

    this.client = new OpenclawGatewayClient({
      url: OPENCLAW_GATEWAY_URL,
      token: OPENCLAW_GATEWAY_TOKEN || undefined,
      clientId: OPENCLAW_GATEWAY_CLIENT_ID,
      mode: OPENCLAW_GATEWAY_CLIENT_MODE,
      scopes: scopes.length > 0 ? scopes : undefined,
      onHello: () => {
        this.connected = true;
      },
      onClose: () => {
        this.connected = false;
      },
      onEvent: (evt) => this.handleEvent(evt),
      onConnectError: () => {
        this.connected = false;
      },
    });
    this.client.start();
  }

  disconnect() {
    if (!this.client) {
      return;
    }
    this.client.stop();
    this.client = null;
    this.connected = false;
  }

  isConnected() {
    return this.connected;
  }

  async send(message: string) {
    if (!this.client || !this.connected) {
      throw new Error("gateway not connected");
    }
    await this.client.request("chat.send", {
      sessionKey: OPENCLAW_CHAT_SESSION_KEY,
      message,
      deliver: false,
      idempotencyKey: genUUID(),
    });
  }

  private handleEvent(evt: { event?: string; payload?: unknown }) {
    if (evt.event === "agent") {
      this.handleAgentEvent(evt.payload as AgentEventPayload | undefined);
      return;
    }
    if (evt.event === "chat") {
      this.handleChatEvent(evt.payload as ChatEventPayload | undefined);
    }
  }

  private handleAgentEvent(payload?: AgentEventPayload) {
    const data = payload?.data;
    if (!data || typeof data !== "object") {
      return;
    }
    const phase = typeof data.phase === "string" ? data.phase : "";
    const name = typeof data.name === "string" ? data.name : "";
    const error = typeof data.error === "string" ? data.error : "";
    const labelParts = [phase, name].filter(Boolean).join(" · ");
    const label = error ? `${labelParts || "error"}: ${error}` : labelParts;
    if (label) {
      this.emit("agentPhase", label);
    }
  }

  private handleChatEvent(payload?: ChatEventPayload) {
    if (!payload) {
      return;
    }
    if (payload.state && payload.state !== "final") {
      return;
    }
    const text = extractChatMessageText(payload.message);
    if (!text) {
      return;
    }
    const timestamp =
      typeof payload.message === "object" && payload.message !== null
        ? extractChatTimestamp(payload.message)
        : null;
    this.emit("openclawResponse", text, timestamp ?? Date.now());
  }
}

export const openclawGateway = new OpenclawGatewayManager();

if (typeof window !== "undefined") {
  (window as typeof window & { openclawGateway?: OpenclawGatewayManager }).openclawGateway =
    openclawGateway;
}

type ChatEventPayload = {
  state?: "delta" | "final" | "aborted" | "error";
  message?: unknown;
};

function extractChatMessageText(message: unknown): string | null {
  if (typeof message === "string") {
    return message;
  }
  const msg = message as Record<string, unknown>;
  if (!msg || typeof msg !== "object") {
    return null;
  }
  const nested = msg.message;
  if (nested && typeof nested === "object") {
    const nestedText = extractChatMessageText(nested);
    if (nestedText) {
      return nestedText;
    }
  }
  const content = msg.content;
  if (typeof content === "string") {
    return content;
  }
  if (Array.isArray(content)) {
    const parts = content
      .map((entry) => {
        const item = entry as Record<string, unknown>;
        if (item.type === "text" && typeof item.text === "string") {
          return item.text;
        }
        return null;
      })
      .filter((value): value is string => typeof value === "string");
    if (parts.length > 0) {
      return parts.join("\n");
    }
  }
  if (typeof msg.text === "string") {
    return msg.text;
  }
  return null;
}

function extractChatTimestamp(message: unknown): number | null {
  const msg = message as Record<string, unknown>;
  const ts = msg?.timestamp;
  if (typeof ts === "number") {
    return ts;
  }
  if (typeof ts === "string") {
    const parsed = Date.parse(ts);
    if (!Number.isNaN(parsed)) {
      return parsed;
    }
  }
  return null;
}
