"use client";

import { AudioControls } from "@/components/Agent/AudioControls";
import { AudioVisualizer } from "@/components/Agent/AudioVisualizer";
import { ChatHistory } from "@/components/Agent/ChatHistory";
import { ConnectionStatus } from "@/components/Agent/ConnectionStatus";
import { TranscriptionDisplay } from "@/components/Agent/TranscriptionDisplay";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { useAudioPlayer } from "@/hooks/useAudioPlayer";
import { useAudioRecorder } from "@/hooks/useAudioRecorder";
import { useWebSocket } from "@/hooks/useWebSocket";
import { useAgentLifecycle } from "@/hooks/useAgentLifecycle";
import { useAgentStore } from "@/store/agentStore";
import { getOrGeneratePort, getWebSocketUrl } from "@/lib/portManager";
import { useEffect, useState } from "react";

export function WebSocketClient() {
  const [port, setPort] = useState<number | null>(null);
  const [initError, setInitError] = useState<string | null>(null);
  const [hasAttemptedStart, setHasAttemptedStart] = useState(false);

  // Initialize port on mount (client-side only)
  useEffect(() => {
    const wsPort = getOrGeneratePort();
    setPort(wsPort);
  }, []);

  // Agent lifecycle management
  const { state: agentState, startAgent, isStarting, isRunning, hasError } = useAgentLifecycle();

  // WebSocket connection (don't auto-connect, but allow unlimited retries)
  const { wsManager, connect: connectWebSocket } = useWebSocket({
    url: port ? getWebSocketUrl(port) : "ws://localhost:8765",
    autoConnect: false,
    maxReconnectAttempts: -1, // Unlimited retries
    reconnectInterval: 3000, // 3 seconds between retries
  });

  // Initialize audio recorder
  const { isRecording, startRecording, stopRecording, getMediaStream } =
    useAudioRecorder(wsManager);

  // Initialize audio player
  useAudioPlayer(wsManager);

  // Get store state
  const { wsConnected } = useAgentStore();

  // Start agent when port is available (only once, no retries)
  useEffect(() => {
    // Only attempt if:
    // 1. Port is available
    // 2. Not already running
    // 3. Not currently starting
    // 4. Haven't already attempted (prevents retries on error)
    // 5. No previous error
    if (port && !isRunning && !isStarting && !hasAttemptedStart && !hasError) {
      setHasAttemptedStart(true);
      startAgent({ port })
        .then(() => {
          console.log(`Agent started successfully on port ${port}`);
          // Wait a bit for the WebSocket server to be ready before connecting
          // The agent needs time to initialize the WebSocket server extension
          setTimeout(() => {
            console.log(`Attempting WebSocket connection to port ${port}...`);
            connectWebSocket();
          }, 2000); // 2 second delay to allow server to start
        })
        .catch((error) => {
          console.error("Failed to start agent:", error);
          setInitError(error.message || "Failed to start agent");
          // Don't retry - just show the error
        });
    }
  }, [port, isRunning, isStarting, hasAttemptedStart, hasError, startAgent, connectWebSocket]);

  const handleStartRecording = async () => {
    await startRecording();
  };

  const handleStopRecording = () => {
    stopRecording();
  };

  return (
    <div className="container mx-auto max-w-4xl py-8 px-4">
      <div className="space-y-6">
        {/* Header */}
        <div className="flex items-center justify-between">
          <div>
            <h1 className="text-3xl font-bold">WebSocket Voice Assistant</h1>
            <p className="text-sm text-muted-foreground mt-1">
              Speak to interact with your AI assistant
              {port && ` • Port: ${port}`}
            </p>
          </div>
          <ConnectionStatus />
        </div>

        {/* Audio Controls Card */}
        <Card>
          <CardHeader>
            <CardTitle>Voice Control</CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            {/* Audio Visualizer */}
            <div className="flex justify-center">
              <AudioVisualizer
                stream={getMediaStream()}
                isActive={isRecording}
                barCount={40}
                barWidth={3}
                barGap={2}
                height={80}
              />
            </div>

            {/* Record Button */}
            <AudioControls
              isRecording={isRecording}
              isDisabled={!wsConnected}
              onStartRecording={handleStartRecording}
              onStopRecording={handleStopRecording}
            />

            {/* Status Text */}
            <div className="text-center text-sm text-muted-foreground">
              {(initError || agentState.error) && (
                <span className="text-red-500">
                  Error: {initError || agentState.error}
                </span>
              )}
              {!initError && !agentState.error && isStarting && "Starting agent..."}
              {!initError && !agentState.error && !isStarting && !wsConnected && "Connecting to WebSocket..."}
              {!initError && !agentState.error && wsConnected && !isRecording && "Click the microphone to start speaking"}
              {!initError && !agentState.error && wsConnected && isRecording && "Listening... Click to stop"}
            </div>

            {/* Live Transcription */}
            <TranscriptionDisplay />
          </CardContent>
        </Card>

        {/* Chat History Card */}
        <Card>
          <CardHeader>
            <CardTitle>Conversation</CardTitle>
          </CardHeader>
          <CardContent>
            <ChatHistory />
          </CardContent>
        </Card>
      </div>
    </div>
  );
}
