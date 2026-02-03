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
}

export const openclawGateway = new OpenclawGatewayManager();

if (typeof window !== "undefined") {
  (window as typeof window & { openclawGateway?: OpenclawGatewayManager }).openclawGateway =
    openclawGateway;
}
