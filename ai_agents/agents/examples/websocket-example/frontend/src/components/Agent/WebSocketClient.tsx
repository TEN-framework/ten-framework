"use client";

import { AudioControls } from "@/components/Agent/AudioControls";
import { AudioVisualizer } from "@/components/Agent/AudioVisualizer";
import { ChatHistory } from "@/components/Agent/ChatHistory";
import { ConnectionStatus } from "@/components/Agent/ConnectionStatus";
import { TranscriptionDisplay } from "@/components/Agent/TranscriptionDisplay";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Badge } from "@/components/ui/badge";
// Minimal design: avoid heavy separators for cleaner cards
import { Progress } from "@/components/ui/progress";
import { Alert, AlertDescription, AlertTitle } from "@/components/ui/alert";
import { useAudioPlayer } from "@/hooks/useAudioPlayer";
import { useAudioRecorder } from "@/hooks/useAudioRecorder";
import { useWebSocket } from "@/hooks/useWebSocket";
import { useAgentLifecycle } from "@/hooks/useAgentLifecycle";
import { useAgentStore } from "@/store/agentStore";
import { getOrGeneratePort, getWebSocketUrl } from "@/lib/portManager";
import { useEffect, useState } from "react";
import { Play, Square, Loader2, Wifi, WifiOff, AlertCircle, CheckCircle2 } from "lucide-react";

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
    <div className="min-h-screen bg-background">
      <div className="container mx-auto max-w-6xl py-6 px-4">
        <div className="space-y-6">
          {/* Header */}
          <div className="flex flex-col gap-4 sm:flex-row sm:items-center sm:justify-between">
            <div className="space-y-2">
              <h1 className="text-3xl font-bold tracking-tight text-foreground">
                WebSocket Voice Assistant
              </h1>
              <div className="flex items-center gap-2 flex-wrap text-sm text-muted-foreground">
                <span>Real-time voice interaction with AI assistant</span>
                {port && (
                  <Badge variant="secondary" className="font-normal">
                    Port: {port}
                  </Badge>
                )}
              </div>
            </div>
            <ConnectionStatus />
          </div>

          {/* Main Grid */}
          <div className="grid grid-cols-1 lg:grid-cols-2 gap-6 items-start">
          {/* Left column: Connection + Voice stacked */}
          <div className="space-y-6 lg:col-span-1">
          {/* Connection Control Card */}
          <Card className="shadow-sm">
            <CardHeader>
              <CardTitle className="flex items-center gap-2">
                <Wifi className="h-5 w-5" />
                Connection
              </CardTitle>
              <CardDescription>
                Start or stop the agent connection
              </CardDescription>
            </CardHeader>
            <CardContent className="pt-2">
              <div className="space-y-4">
                <div className="flex items-center justify-between gap-4">
                  <div className="flex-1 space-y-2">
                    {!isRunning && !isStarting && (
                      <div className="flex items-center gap-2 text-sm text-muted-foreground">
                        <WifiOff className="h-4 w-4" />
                        <span>Click Start to connect to the agent</span>
                      </div>
                    )}
                    {isStarting && (
                      <div className="space-y-2">
                        <div className="flex items-center gap-2 text-sm text-muted-foreground">
                          <Loader2 className="h-4 w-4 animate-spin" />
                          <span>Starting agent...</span>
                        </div>
                        <Progress value={undefined} className="h-1 opacity-60" />
                      </div>
                    )}
                    {isRunning && !wsConnected && (
                      <div className="space-y-2">
                        <div className="flex items-center gap-2 text-sm text-muted-foreground">
                          <Loader2 className="h-4 w-4 animate-spin" />
                          <span>Agent running, connecting to WebSocket...</span>
                        </div>
                        <Progress value={undefined} className="h-1 opacity-60" />
                      </div>
                    )}
                    {isRunning && wsConnected && (
                      <div className="flex items-center gap-2 text-sm">
                        <CheckCircle2 className="h-4 w-4 text-primary" />
                        <span className="text-foreground">Connected and ready</span>
                      </div>
                    )}
                  </div>
                  <div className="flex gap-2">
                    {!isRunning && !isStarting && (
                      <Button
                        onClick={handleStartAgent}
                        disabled={!port}
                        size="lg"
                        className="gap-2"
                      >
                        <Play className="h-4 w-4" />
                        Start
                      </Button>
                    )}
                    {isStarting && (
                      <Button disabled size="lg" className="gap-2">
                        <Loader2 className="h-4 w-4 animate-spin" />
                        Starting...
                      </Button>
                    )}
                    {isRunning && (
                      <Button
                        onClick={handleStopAgent}
                        variant="destructive"
                        size="lg"
                        className="gap-2"
                      >
                        <Square className="h-4 w-4" />
                        Stop
                      </Button>
                    )}
                  </div>
                </div>
                {(initError || agentState.error) && (
                  <Alert variant="destructive">
                    <AlertCircle className="h-4 w-4" />
                    <AlertTitle>Connection Error</AlertTitle>
                    <AlertDescription>
                      {initError || agentState.error}
                    </AlertDescription>
                  </Alert>
                )}
              </div>
            </CardContent>
          </Card>

          {/* Audio Controls Card */}
          <Card className="shadow-sm">
            <CardHeader>
              <CardTitle>Voice Control</CardTitle>
              <CardDescription>
                Record and interact with the AI assistant using your voice
              </CardDescription>
            </CardHeader>
            <CardContent className="pt-2 space-y-4">
              {/* Audio Visualizer */}
              <div className="flex justify-center rounded-xl bg-muted-a30 p-4 shadow-sm ring-1 ring-border-a40 border border-border-a30">
                <AudioVisualizer
                  stream={getMediaStream()}
                  isActive={isRecording}
                  barCount={40}
                  barWidth={4}
                  barGap={2}
                  height={120}
                />
              </div>

              {/* Record Button */}
              <div className="flex justify-center">
                <AudioControls
                  isRecording={isRecording}
                  isDisabled={!wsConnected}
                  onStartRecording={handleStartRecording}
                  onStopRecording={handleStopRecording}
                />
              </div>

              {/* Status Text */}
              <div className="text-center">
                {!isRunning && (
                  <p className="text-sm text-muted-foreground">
                    Start the connection to begin
                  </p>
                )}
                {isRunning && !wsConnected && (
                  <div className="flex items-center justify-center gap-2 text-sm text-muted-foreground">
                    <Loader2 className="h-4 w-4 animate-spin" />
                    <span>Connecting to WebSocket...</span>
                  </div>
                )}
                {isRunning && wsConnected && !isRecording && (
                  <p className="text-sm text-muted-foreground">
                    Click the microphone to start speaking
                  </p>
                )}
                {isRunning && wsConnected && isRecording && (
                  <div className="flex items-center justify-center gap-2 text-sm text-destructive">
                    <div className="h-2 w-2 rounded-full bg-destructive animate-pulse" />
                    <span>Listening... Click to stop</span>
                  </div>
                )}
              </div>

              {/* Live Transcription */}
              <TranscriptionDisplay />
            </CardContent>
          </Card>

          </div>

          {/* Right column: Conversation */}
          <Card className="shadow-sm lg:col-span-1">
            <CardHeader>
              <CardTitle>Conversation</CardTitle>
              <CardDescription>
                View your conversation history with the AI assistant
              </CardDescription>
            </CardHeader>
            <CardContent className="pt-2">
              <ChatHistory />
            </CardContent>
          </Card>
          </div>
        </div>
      </div>
    </div>
  );
}
