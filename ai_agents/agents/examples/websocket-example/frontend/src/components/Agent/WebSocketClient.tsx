"use client";

import { AudioControls } from "@/components/Agent/AudioControls";
import { AudioVisualizer } from "@/components/Agent/AudioVisualizer";
import { ChatHistory } from "@/components/Agent/ChatHistory";
import { ConnectionStatus } from "@/components/Agent/ConnectionStatus";
import { TranscriptionDisplay } from "@/components/Agent/TranscriptionDisplay";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
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

  // Initialize port on mount (client-side only)
  useEffect(() => {
    const wsPort = getOrGeneratePort();
    setPort(wsPort);
  }, []);

  // Agent lifecycle management
  const { state: agentState, startAgent, stopAgent, reset, isStarting, isRunning, hasError } = useAgentLifecycle();

  // WebSocket connection (don't auto-connect, but allow unlimited retries)
  const { wsManager, connect: connectWebSocket, disconnect: disconnectWebSocket } = useWebSocket({
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

  // Handle start agent
  const handleStartAgent = async () => {
    if (!port) {
      setInitError("Port not available");
      return;
    }

    setInitError(null);
    try {
      await startAgent({ port });
      console.log(`Agent started successfully on port ${port}`);
      // Wait a bit for the WebSocket server to be ready before connecting
      setTimeout(() => {
        console.log(`Attempting WebSocket connection to port ${port}...`);
        connectWebSocket();
      }, 2000); // 2 second delay to allow server to start
    } catch (error) {
      console.error("Failed to start agent:", error);
      setInitError(error instanceof Error ? error.message : "Failed to start agent");
    }
  };

  // Handle stop agent
  const handleStopAgent = async () => {
    try {
      // Disconnect WebSocket first
      disconnectWebSocket();
      // Stop the agent
      await stopAgent();
      // Reset error state so user can try again
      setInitError(null);
      reset();
      console.log("Agent stopped successfully");
    } catch (error) {
      console.error("Failed to stop agent:", error);
      // Continue anyway - best effort
      setInitError(null);
      reset();
    }
  };

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

        {/* Connection Control Card */}
        <Card>
          <CardHeader>
            <CardTitle>Connection</CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex items-center justify-between">
              <div className="flex-1">
                <p className="text-sm text-muted-foreground">
                  {!isRunning && !isStarting && "Click Start to connect to the agent"}
                  {isStarting && "Starting agent..."}
                  {isRunning && !wsConnected && "Agent running, connecting to WebSocket..."}
                  {isRunning && wsConnected && "Connected and ready"}
                </p>
              </div>
              <div className="flex gap-2">
                {!isRunning && !isStarting && (
                  <Button
                    onClick={handleStartAgent}
                    disabled={!port}
                    variant="default"
                  >
                    Start
                  </Button>
                )}
                {isStarting && (
                  <Button disabled variant="default">
                    Starting...
                  </Button>
                )}
                {isRunning && (
                  <Button
                    onClick={handleStopAgent}
                    variant="destructive"
                  >
                    Stop
                  </Button>
                )}
              </div>
            </div>
            {(initError || agentState.error) && (
              <div className="text-sm text-red-500">
                Error: {initError || agentState.error}
              </div>
            )}
          </CardContent>
        </Card>

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
              {!isRunning && "Start the connection to begin"}
              {isRunning && !wsConnected && "Connecting to WebSocket..."}
              {isRunning && wsConnected && !isRecording && "Click the microphone to start speaking"}
              {isRunning && wsConnected && isRecording && "Listening... Click to stop"}
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
