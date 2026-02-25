// API client for Twilio Voice Assistant
const TWILIO_SERVER_URL =
  process.env.NEXT_PUBLIC_TWILIO_SERVER_URL || "http://localhost:8080";
const TENAPP_SERVER_URL =
  process.env.NEXT_PUBLIC_TENAPP_SERVER_URL || "http://localhost:9000";

export interface CallResponse {
  call_sid: string;
  phone_number: string;
  message: string;
  status: string;
  created_at: number;
}

export interface CallInfo {
  call_sid: string;
  phone_number: string;
  status: string;
  created_at: number;
  has_websocket?: boolean;
}

export interface CallListResponse {
  calls: CallResponse[];
  total: number;
}

export interface ServerConfig {
  twilio_from_number: string;
  server_port: number;
  tenapp_port: number;
  tenapp_url: string;
  public_server_url: string;
  use_https: boolean;
  use_wss: boolean;
  media_stream_enabled: boolean;
  media_ws_url: string;
  webhook_enabled: boolean;
  webhook_url: string;
}

export interface HealthResponse {
  status: string;
  active_calls: number;
}

export interface CreateCallRequest {
  phone_number: string;
  message?: string;
}

export interface HealthResponse {
  status: string;
  active_calls: number;
}

class TwilioAPI {
  private twilioServerUrl: string;
  private tenappServerUrl: string;
  private config: ServerConfig | null = null;

  constructor(
    twilioServerUrl: string = TWILIO_SERVER_URL,
    tenappServerUrl: string = TENAPP_SERVER_URL
  ) {
    this.twilioServerUrl = twilioServerUrl;
    this.tenappServerUrl = tenappServerUrl;
  }

  private async request<T>(
    endpoint: string,
    options: RequestInit = {},
    useTenapp: boolean = false
  ): Promise<T> {
    const baseUrl = useTenapp ? this.tenappServerUrl : this.twilioServerUrl;
    const url = `${baseUrl}${endpoint}`;

    const response = await fetch(url, {
      headers: {
        "Content-Type": "application/json",
        ...options.headers,
      },
      ...options,
    });

    if (!response.ok) {
      const errorText = await response.text();
      throw new Error(`API request failed: ${response.status} ${errorText}`);
    }

    return response.json();
  }

  async createCall(data: CreateCallRequest): Promise<CallResponse> {
    return this.request<CallResponse>(
      "/api/call",
      {
        method: "POST",
        body: JSON.stringify(data),
      },
      true
    ); // Use tenapp server
  }

  async getCall(callSid: string): Promise<CallInfo> {
    return this.request<CallInfo>(`/api/call/${callSid}`, {}, true); // Use tenapp server
  }

  async deleteCall(callSid: string): Promise<{ message: string }> {
    return this.request<{ message: string }>(
      `/api/call/${callSid}`,
      {
        method: "DELETE",
      },
      true
    ); // Use tenapp server
  }

  async listCalls(): Promise<CallListResponse> {
    return this.request<CallListResponse>("/api/calls", {}, true); // Use tenapp server
  }

  async getHealth(): Promise<HealthResponse> {
    return this.request<HealthResponse>("/health", {}, true); // Use tenapp server
  }

  async getConfig(): Promise<ServerConfig> {
    if (!this.config) {
      this.config = await this.request<ServerConfig>("/api/config"); // Use twilio server
    }
    return this.config;
  }
}

// Export singleton instance
export const twilioAPI = new TwilioAPI();

// Export class for custom instances
export { TwilioAPI };
