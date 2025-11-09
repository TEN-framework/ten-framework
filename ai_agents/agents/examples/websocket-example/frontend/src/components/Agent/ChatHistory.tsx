"use client";

import { ScrollArea } from "@/components/ui/scroll-area";
import { useAgentStore } from "@/store/agentStore";
import { useEffect, useRef } from "react";

export function ChatHistory() {
  const { messages } = useAgentStore();
  const scrollRef = useRef<HTMLDivElement>(null);

  // Auto-scroll to bottom when new messages arrive
  useEffect(() => {
    if (scrollRef.current) {
      scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
    }
  }, [messages]);

  if (messages.length === 0) {
    return (
      <div className="flex h-[400px] items-center justify-center rounded-xl border bg-muted/20">
        <p className="text-sm text-muted-foreground">
          Start speaking to see your conversation here
        </p>
      </div>
    );
  }

  return (
    <ScrollArea className="h-[400px] w-full rounded-xl border bg-muted/20 p-4">
      <div ref={scrollRef} className="space-y-4">
        {messages.map((message) => (
          <div
            key={message.id}
            className={`flex ${message.role === "user" ? "justify-end" : "justify-start"}`}
          >
            <div
              className={`max-w-[80%] rounded-lg px-4 py-2 ${
                message.role === "user"
                  ? "bg-primary text-primary-foreground"
                  : "bg-secondary text-secondary-foreground"
              }`}
            >
              <p className="text-sm whitespace-pre-wrap">{message.content}</p>
              <p className="mt-1 text-xs opacity-60">
                {new Date(message.timestamp).toLocaleTimeString()}
              </p>
            </div>
          </div>
        ))}
      </div>
    </ScrollArea>
  );
}
