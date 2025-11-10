"use client";

import { AudioControls } from "@/components/Agent/AudioControls";
import { AudioVisualizer } from "@/components/Agent/AudioVisualizer";
import { ChatHistory } from "@/components/Agent/ChatHistory";
import { ConnectionStatus } from "@/components/Agent/ConnectionStatus";
import { TranscriptionDisplay } from "@/components/Agent/TranscriptionDisplay";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Badge } from "@/components/ui/badge";
import { Alert, AlertDescription, AlertTitle } from "@/components/ui/alert";
import { useAudioPlayer } from "@/hooks/useAudioPlayer";
import { useAudioRecorder } from "@/hooks/useAudioRecorder";
import { useWebSocket } from "@/hooks/useWebSocket";
import { useAgentLifecycle } from "@/hooks/useAgentLifecycle";
import { useAgentStore } from "@/store/agentStore";
import { getOrGeneratePort, getWebSocketUrl } from "@/lib/portManager";
import { useEffect, useState } from "react";
import { Play, Square, Loader2, Wifi, AlertCircle } from "lucide-react";

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

  // Auto-start recording on mount
  useEffect(() => {
    const autoStart = async () => {
      try {
        await startRecording();
      } catch (err) {
        console.error("Failed to auto-start recording:", err);
      }
    };
    autoStart();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []); // Only run on mount

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
            {/* Left column: Connection + Voice */}
            <div className="lg:col-span-1">
              <Card className="shadow-sm">
                <CardHeader className="flex flex-row items-center justify-between gap-4">
                  <div className="flex-1">
                    <CardTitle className="flex items-center gap-2">
                      <Wifi className="h-5 w-5" />
                      Connection
                    </CardTitle>
                    <CardDescription>
                      Start or stop the agent connection and interact with voice
                    </CardDescription>
                  </div>
                  <div className="flex items-center gap-2">
                    {!isRunning && !isStarting && (
                      <Button
                        onClick={handleStartAgent}
                        disabled={!port}
                        size="sm"
                        className="gap-2"
                      >
                        <Play className="h-4 w-4" />
                        Start
                      </Button>
                    )}
                    {isStarting && (
                      <Button disabled size="sm" className="gap-2">
                        <Loader2 className="h-4 w-4 animate-spin" />
                        Starting...
                      </Button>
                    )}
                    {isRunning && (
                      <Button
                        onClick={handleStopAgent}
                        variant="destructive"
                        size="sm"
                        className="gap-2"
                      >
                        <Square className="h-4 w-4" />
                        Stop
                      </Button>
                    )}
                    <AudioControls
                      isRecording={isRecording}
                      onStartRecording={handleStartRecording}
                      onStopRecording={handleStopRecording}
                    />
                  </div>
                </CardHeader>
                <CardContent className="pt-2 space-y-4">
                  {(initError || agentState.error) && (
                    <Alert variant="destructive">
                      <AlertCircle className="h-4 w-4" />
                      <AlertTitle>Connection Error</AlertTitle>
                      <AlertDescription>
                        {initError || agentState.error}
                      </AlertDescription>
                    </Alert>
                  )}

                  {/* Audio Visualizer */}
                  <div className="relative rounded-xl bg-muted/30 overflow-hidden p-0 ring-1 ring-border/40 border border-border/30" style={{ height: 48 }}>
                    <AudioVisualizer
                      stream={getMediaStream()}
                      isActive={isRecording}
                      barCount={40}
                      barWidth={4}
                      barGap={2}
                      height={48}
                    />
                  </div>

                  {/* Status Text */}
                  <div className="text-center">
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
                      <div className="flex items-center justify-center gap-2 text-sm text-white">
                        <div className="h-2 w-2 rounded-full bg-white animate-pulse" />
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
