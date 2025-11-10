/**
 * useWebSocket Hook
 * Manages WebSocket connection and integrates with Zustand store
 */

import { WebSocketManager } from "@/manager/websocket";
import { useAgentStore } from "@/store/agentStore";
import { useCallback, useEffect, useRef } from "react";

interface UseWebSocketOptions {
  url: string;
  autoConnect?: boolean;
  maxReconnectAttempts?: number;
  reconnectInterval?: number;
}

export function useWebSocket(options: UseWebSocketOptions | string) {
  // Support both object and string parameter for backwards compatibility
  const {
    url,
    autoConnect = false,
    maxReconnectAttempts,
    reconnectInterval,
  } = typeof options === "string"
      ? { url: options, autoConnect: false }
      : options;

  const wsManagerRef = useRef<WebSocketManager | null>(null);
  const {
    setWsConnected,
    setError,
    addMessage,
    setTranscribing,
    clearTranscribing,
  } = useAgentStore();

  // Initialize WebSocket manager
  useEffect(() => {
    // Create WebSocket manager (but don't connect yet)
    const wsManager = new WebSocketManager({
      url,
      maxReconnectAttempts,
      reconnectInterval,
    });
    wsManagerRef.current = wsManager;

    // Handle connection open
    wsManager.onOpen(() => {
      console.log("WebSocket connected");
      setWsConnected(true);
      setError(null);
    });

    // Handle connection close
    wsManager.onClose(() => {
      console.log("WebSocket closed");
      setWsConnected(false);
    });

    // Handle connection error
    wsManager.onError((error) => {
      console.error("WebSocket error:", error);
      setError("WebSocket connection error");
    });

    // Handle audio messages
    wsManager.onAudio((message) => {
      console.log("Received audio:", message.audio.length, "bytes");
      // Audio will be played by useAudioPlayer hook
    });

    // Handle data messages
    wsManager.onData((message) => {
      console.log("Received data:", message.name, message.data);

      // Handle ASR results
      if (message.name === "asr_result") {
        const text = message.data?.text || message.data?.transcript || "";
        const isFinal = message.data?.is_final || message.data?.final || false;

        if (isFinal) {
          // Final transcription - add as user message
          if (text) {
            addMessage({
              role: "user",
              content: text,
            });
          }
          clearTranscribing();
        } else {
          // Partial transcription - show as transcribing
          setTranscribing(text);
        }
      }

      // Handle LLM text responses
      if (
        message.name === "text_data" ||
        message.name === "llm_response" ||
        message.name === "chat_message"
      ) {
        const text = message.data?.text || message.data?.content || "";
        if (text) {
          addMessage({
            role: "assistant",
            content: text,
          });
        }
      }
    });

    // Handle command messages
    wsManager.onCmd((message) => {
      console.log("Received command:", message.name, message.data);
    });

    // Handle error messages
    wsManager.onErrorMessage((message) => {
      console.error("Server error:", message.error);
      setError(message.error);
    });

    // Auto-connect if enabled
    if (autoConnect) {
      wsManager.connect();
    }

    // Cleanup on unmount
    return () => {
      wsManager.disconnect();
    };
  }, [url, autoConnect, maxReconnectAttempts, reconnectInterval, setWsConnected, setError, addMessage, setTranscribing, clearTranscribing]);

  // Manual connect function
  const connect = useCallback(() => {
    wsManagerRef.current?.connect();
  }, []);

  // Manual disconnect function
  const disconnect = useCallback(() => {
    wsManagerRef.current?.disconnect();
  }, []);

  return {
    wsManager: wsManagerRef.current,
    connect,
    disconnect,
  };
}
