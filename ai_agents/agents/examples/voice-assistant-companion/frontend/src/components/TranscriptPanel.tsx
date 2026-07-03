"use client";

import {
  Bot,
  Clock,
  Download,
  MessageSquare,
  Trash2,
  User,
} from "lucide-react";
import { useEffect, useRef, useState } from "react";
import { Button } from "@/components/ui/button";
import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/ui/card";
import { Label } from "@/components/ui/label";
import { Switch } from "@/components/ui/switch";
// agoraService will be imported dynamically
import type { TranscriptMessage } from "@/types";

interface TranscriptPanelProps {
  className?: string;
}

export default function TranscriptPanel({ className }: TranscriptPanelProps) {
  const [messages, setMessages] = useState<TranscriptMessage[]>([]);
  const [isAutoScroll, setIsAutoScroll] = useState(true);
  const [isEnabled, setIsEnabled] = useState(true);
  const [agoraService, setAgoraService] = useState<any>(null);
  const messagesEndRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    // Dynamically import Agora service only on client side
    if (typeof window !== "undefined") {
      import("@/services/agora").then((module) => {
        setAgoraService(module.agoraService);
      });
    }
  }, []);

  useEffect(() => {
    if (!agoraService) return;

    // Set up transcript message listener
    agoraService.setOnTranscriptMessage((message: TranscriptMessage) => {
      if (isEnabled) {
        setMessages((prev) => [...prev, message]);
      }
    });

    return () => {
      // Cleanup if needed
    };
  }, [agoraService, isEnabled]);

  useEffect(() => {
    if (isAutoScroll && messagesEndRef.current) {
      messagesEndRef.current.scrollIntoView({ behavior: "smooth" });
    }
  }, [isAutoScroll]);

  const clearMessages = () => {
    setMessages([]);
  };

  const exportTranscript = () => {
    const transcript = messages
      .map(
        (msg) =>
          `[${msg.timestamp.toLocaleTimeString()}] ${msg.isUser ? "User" : "Assistant"}: ${msg.text}`
      )
      .join("\n");

    const blob = new Blob([transcript], { type: "text/plain" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = `transcript-${new Date().toISOString().split("T")[0]}.txt`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  };

  const formatTimestamp = (timestamp: Date) => {
    return timestamp.toLocaleTimeString([], {
      hour: "2-digit",
      minute: "2-digit",
      second: "2-digit",
    });
  };

  const getConfidenceColor = (confidence?: number) => {
    if (!confidence) return "";
    if (confidence > 0.8) return "text-green-600";
    if (confidence > 0.6) return "text-yellow-600";
    return "text-red-600";
  };

  return (
    <Card className={className}>
      <CardHeader>
        <div className="flex items-center justify-between">
          <div>
            <CardTitle className="flex items-center gap-2">
              <MessageSquare className="h-5 w-5" />
              Live Transcript
            </CardTitle>
            <CardDescription>Real-time conversation transcript</CardDescription>
          </div>

          <div className="flex items-center gap-2">
            <Label htmlFor="transcript-enabled" className="text-sm">
              Enable
            </Label>
            <Switch
              id="transcript-enabled"
              checked={isEnabled}
              onCheckedChange={setIsEnabled}
            />
          </div>
        </div>
      </CardHeader>

      <CardContent className="space-y-2">
        {/* Controls */}
        <div className="flex items-center justify-between">
          <div className="flex items-center gap-2">
            <Label htmlFor="auto-scroll" className="text-sm">
              Auto-scroll
            </Label>
            <Switch
              id="auto-scroll"
              checked={isAutoScroll}
              onCheckedChange={setIsAutoScroll}
            />
          </div>

          <div className="flex gap-2">
            <Button
              onClick={exportTranscript}
              variant="outline"
              size="sm"
              disabled={messages.length === 0}
            >
              <Download className="mr-1 h-4 w-4" />
              Export
            </Button>

            <Button
              onClick={clearMessages}
              variant="outline"
              size="sm"
              disabled={messages.length === 0}
            >
              <Trash2 className="mr-1 h-4 w-4" />
              Clear
            </Button>
          </div>
        </div>

        {/* Messages */}
        <div className="h-48 space-y-2 overflow-y-auto rounded-lg border bg-muted/20 p-2">
          {messages.length === 0 ? (
            <div className="flex h-full items-center justify-center text-muted-foreground">
              <div className="text-center">
                <MessageSquare className="mx-auto mb-2 h-8 w-8 opacity-50" />
                <p>No messages yet</p>
                <p className="text-sm">
                  Start a conversation to see the transcript
                </p>
              </div>
            </div>
          ) : (
            messages.map((message) => (
              <div
                key={message.id}
                className={`flex gap-3 rounded-lg p-3 ${
                  message.isUser
                    ? "border-primary border-l-4 bg-primary/10"
                    : "border-secondary border-l-4 bg-secondary/50"
                }`}
              >
                <div className="flex-shrink-0">
                  {message.isUser ? (
                    <User className="h-5 w-5 text-primary" />
                  ) : (
                    <Bot className="h-5 w-5 text-secondary-foreground" />
                  )}
                </div>

                <div className="min-w-0 flex-1">
                  <div className="mb-1 flex items-center gap-2">
                    <span className="font-medium text-sm">
                      {message.isUser ? "You" : "Assistant"}
                    </span>
                    <div className="flex items-center gap-1 text-muted-foreground text-xs">
                      <Clock className="h-3 w-3" />
                      {formatTimestamp(message.timestamp)}
                    </div>
                    {message.confidence && (
                      <span
                        className={`text-xs ${getConfidenceColor(message.confidence)}`}
                      >
                        ({Math.round(message.confidence * 100)}%)
                      </span>
                    )}
                  </div>

                  <p className="break-words text-sm">{message.text}</p>
                </div>
              </div>
            ))
          )}
          <div ref={messagesEndRef} />
        </div>

        {/* Stats */}
        {messages.length > 0 && (
          <div className="text-center text-muted-foreground text-xs">
            {messages.length} message{messages.length !== 1 ? "s" : ""} •
            {messages.filter((m) => m.isUser).length} from you •
            {messages.filter((m) => !m.isUser).length} from assistant
          </div>
        )}
      </CardContent>
    </Card>
  );
}
