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
import { useAgentStore } from "@/store/agentStore";

export function WebSocketClient() {
  const wsUrl =
    process.env.NEXT_PUBLIC_WS_URL || "ws://localhost:8765";

  // Initialize WebSocket connection
  const wsManager = useWebSocket(wsUrl);

  // Initialize audio recorder
  const { isRecording, startRecording, stopRecording, getMediaStream } =
    useAudioRecorder(wsManager);

  // Initialize audio player
  useAudioPlayer(wsManager);

  // Get store state
  const { wsConnected } = useAgentStore();

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
              {!wsConnected && "Waiting for WebSocket connection..."}
              {wsConnected && !isRecording && "Click the microphone to start speaking"}
              {wsConnected && isRecording && "Listening... Click to stop"}
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
