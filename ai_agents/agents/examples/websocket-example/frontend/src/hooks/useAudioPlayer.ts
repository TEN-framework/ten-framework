/**
 * useAudioPlayer Hook
 * Manages audio playback from WebSocket server
 */

import { AudioPlayer, DEFAULT_AUDIO_CONFIG } from "@/lib/audioUtils";
import type { WebSocketManager } from "@/manager/websocket";
import { useEffect, useRef, useState } from "react";

export function useAudioPlayer(wsManager: WebSocketManager | null) {
  const [isPlaying, setIsPlaying] = useState(false);
  const playerRef = useRef<AudioPlayer | null>(null);

  useEffect(() => {
    // Initialize audio player
    const player = new AudioPlayer(DEFAULT_AUDIO_CONFIG);
    playerRef.current = player;

    player.initialize();

    // Listen for audio messages from WebSocket
    const unsubscribe = wsManager?.onAudio(async (message) => {
      setIsPlaying(true);
      try {
        await player.playBase64Audio(message.audio);
      } catch (error) {
        console.error("Failed to play audio:", error);
      } finally {
        setIsPlaying(false);
      }
    });

    // Cleanup
    return () => {
      unsubscribe?.();
      player.destroy();
    };
  }, [wsManager]);

  const setVolume = (volume: number) => {
    playerRef.current?.setVolume(volume);
  };

  return {
    isPlaying,
    setVolume,
  };
}
