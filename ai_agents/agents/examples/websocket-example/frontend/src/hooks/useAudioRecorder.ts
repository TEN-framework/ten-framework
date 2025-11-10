/**
 * useAudioRecorder Hook
 * Manages microphone recording and sends audio to WebSocket
 */

import { AudioRecorder, DEFAULT_AUDIO_CONFIG } from "@/lib/audioUtils";
import type { WebSocketManager } from "@/manager/websocket";
import { useAgentStore } from "@/store/agentStore";
import { useCallback, useRef, useState } from "react";

export function useAudioRecorder(wsManager: WebSocketManager | null) {
  const [isRecording, setIsRecording] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const recorderRef = useRef<AudioRecorder | null>(null);
  // Track the latest wsManager even if start() was called earlier
  const wsRef = useRef<WebSocketManager | null>(null);
  wsRef.current = wsManager;
  const { setRecording } = useAgentStore();

  const startRecording = useCallback(async () => {
    if (isRecording) {
      console.warn("Already recording");
      return;
    }

    try {
      const recorder = new AudioRecorder(DEFAULT_AUDIO_CONFIG);
      recorderRef.current = recorder;

      await recorder.start((audioBase64) => {
        const ws = wsRef.current;
        if (ws?.isConnected()) {
          ws.sendAudio(audioBase64);
        }
      });

      setIsRecording(true);
      setRecording("recording");
      setError(null);
      console.log("Recording started");
    } catch (err) {
      const errorMessage = err instanceof Error ? err.message : "Unknown error";
      setError(errorMessage);
      setRecording("idle");
      console.error("Failed to start recording:", err);
    }
  }, [isRecording, wsManager, setRecording]);

  const stopRecording = useCallback(() => {
    if (!isRecording || !recorderRef.current) {
      return;
    }

    recorderRef.current.stop();
    recorderRef.current = null;
    setIsRecording(false);
    setRecording("idle");
    console.log("Recording stopped");
  }, [isRecording, setRecording]);

  const getMediaStream = useCallback((): MediaStream | null => {
    return recorderRef.current?.getStream() || null;
  }, []);

  return {
    isRecording,
    error,
    startRecording,
    stopRecording,
    getMediaStream,
  };
}
