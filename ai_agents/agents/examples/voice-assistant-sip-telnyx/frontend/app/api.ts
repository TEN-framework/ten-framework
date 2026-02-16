// API utilities for Telnyx voice assistant

const API_BASE_URL = process.env.NEXT_PUBLIC_API_URL || 'http://localhost:9000';

export interface CallRequest {
  phone_number: string;
  message?: string;
}

export interface CallResponse {
  success: boolean;
  call_id: string;
  status: string;
  phone_number: string;
  message: string;
}

export interface CallInfo {
  call_id: string;
  status: string;
  phone_number: string;
  message: string;
  created_at: string;
  ended_at?: string;
}

export interface ConfigResponse {
  telnyx_from_number: string;
  server_port: number;
  public_server_url: string | null;
  use_https: boolean;
  use_wss: boolean;
  media_stream_enabled: boolean;
  media_ws_url: string | null;
  webhook_enabled: boolean;
  webhook_url: string | null;
}

export async function createCall(request: CallRequest): Promise<CallResponse> {
  const response = await fetch(`${API_BASE_URL}/api/call`, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify(request),
  });

  if (!response.ok) {
    throw new Error(`Failed to create call: ${response.statusText}`);
  }

  return response.json();
}

export async function getCallStatus(callId: string): Promise<CallInfo> {
  const response = await fetch(`${API_BASE_URL}/api/call/${callId}`);

  if (!response.ok) {
    throw new Error(`Failed to get call status: ${response.statusText}`);
  }

  return response.json();
}

export async function listCalls(): Promise<{ active_calls: number; calls: string[] }> {
  const response = await fetch(`${API_BASE_URL}/api/calls`);

  if (!response.ok) {
    throw new Error(`Failed to list calls: ${response.statusText}`);
  }

  return response.json();
}

export async function endCall(callId: string): Promise<{ success: boolean; call_id: string; status: string }> {
  const response = await fetch(`${API_BASE_URL}/api/call/${callId}`, {
    method: 'DELETE',
  });

  if (!response.ok) {
    throw new Error(`Failed to end call: ${response.statusText}`);
  }

  return response.json();
}

export async function getHealth(): Promise<{ status: string; server_time: string }> {
  const response = await fetch(`${API_BASE_URL}/health`);

  if (!response.ok) {
    throw new Error(`Health check failed: ${response.statusText}`);
  }

  return response.json();
}

export async function getConfig(): Promise<ConfigResponse> {
  const response = await fetch(`${API_BASE_URL}/api/config`);

  if (!response.ok) {
    throw new Error(`Failed to get config: ${response.statusText}`);
  }

  return response.json();
}