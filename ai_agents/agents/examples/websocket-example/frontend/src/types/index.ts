/**
 * Type definitions for WebSocket Voice Assistant
 */

export type MessageType = "audio" | "data" | "error" | "cmd";

export interface WebSocketMessage {
  type: MessageType;
  [key: string]: any;
}

export interface AudioMessage extends WebSocketMessage {
  type: "audio";
  audio: string; // base64
  metadata?: {
    sample_rate?: number;
    channels?: number;
    bytes_per_sample?: number;
    samples_per_channel?: number;
  };
}

export interface DataMessage extends WebSocketMessage {
  type: "data";
  name: string;
  data: any;
}

export interface ErrorMessage extends WebSocketMessage {
  type: "error";
  error: string;
}

export interface CmdMessage extends WebSocketMessage {
  type: "cmd";
  name: string;
  data: any;
}

export interface ChatMessage {
  id: string;
  role: "user" | "assistant";
  content: string;
  timestamp: number;
}

export type AgentStatus =
  | "idle"
  | "connecting"
  | "connected"
  | "disconnected"
  | "error";

export type RecordingState = "idle" | "recording" | "processing";

export interface AgentState {
  status: AgentStatus;
  wsConnected: boolean;
  recording: RecordingState;
  messages: ChatMessage[];
  transcribing: string;
  error: string | null;
}
