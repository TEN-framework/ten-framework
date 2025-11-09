/**
 * Zustand Store for Agent State Management
 */

import type { AgentStatus, ChatMessage, RecordingState } from "@/types";
import { create } from "zustand";

interface AgentStore {
  // Connection state
  status: AgentStatus;
  wsConnected: boolean;
  error: string | null;

  // Recording state
  recording: RecordingState;

  // Messages
  messages: ChatMessage[];
  transcribing: string; // Current partial transcription

  // Actions
  setStatus: (status: AgentStatus) => void;
  setWsConnected: (connected: boolean) => void;
  setError: (error: string | null) => void;
  setRecording: (state: RecordingState) => void;
  addMessage: (message: Omit<ChatMessage, "id" | "timestamp">) => void;
  setTranscribing: (text: string) => void;
  clearTranscribing: () => void;
  clearMessages: () => void;
  reset: () => void;
}

const initialState = {
  status: "idle" as AgentStatus,
  wsConnected: false,
  error: null,
  recording: "idle" as RecordingState,
  messages: [],
  transcribing: "",
};

export const useAgentStore = create<AgentStore>((set) => ({
  ...initialState,

  setStatus: (status) => set({ status }),

  setWsConnected: (connected) => set({ wsConnected: connected }),

  setError: (error) => set({ error }),

  setRecording: (recording) => set({ recording }),

  addMessage: (message) =>
    set((state) => ({
      messages: [
        ...state.messages,
        {
          ...message,
          id: `${Date.now()}-${Math.random()}`,
          timestamp: Date.now(),
        },
      ],
    })),

  setTranscribing: (text) => set({ transcribing: text }),

  clearTranscribing: () => set({ transcribing: "" }),

  clearMessages: () => set({ messages: [] }),

  reset: () => set(initialState),
}));
