// API client for Twilio Voice Assistant
const API_BASE_URL = process.env.NEXT_PUBLIC_TWILIO_SERVER_URL || 'http://localhost:8080';

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
    private baseUrl: string;

    constructor(baseUrl: string = API_BASE_URL) {
        this.baseUrl = baseUrl;
    }

    private async request<T>(
        endpoint: string,
        options: RequestInit = {}
    ): Promise<T> {
        const url = `${this.baseUrl}${endpoint}`;

        const response = await fetch(url, {
            headers: {
                'Content-Type': 'application/json',
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
        return this.request<CallResponse>('/api/calls', {
            method: 'POST',
            body: JSON.stringify(data),
        });
    }

    async getCall(callSid: string): Promise<CallInfo> {
        return this.request<CallInfo>(`/api/calls/${callSid}`);
    }

    async deleteCall(callSid: string): Promise<{ message: string }> {
        return this.request<{ message: string }>(`/api/calls/${callSid}`, {
            method: 'DELETE',
        });
    }

    async listCalls(): Promise<CallListResponse> {
        return this.request<CallListResponse>('/api/calls');
    }

    async getHealth(): Promise<HealthResponse> {
        return this.request<HealthResponse>('/health');
    }

    async getConfig(): Promise<ServerConfig> {
        return this.request<ServerConfig>('/api/config');
    }
}

// Export singleton instance
export const twilioAPI = new TwilioAPI();

// Export class for custom instances
export { TwilioAPI };
