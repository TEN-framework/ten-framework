"use client";

import { Bot, User } from "lucide-react";
import { useEffect, useRef } from "react";
import { Avatar, AvatarFallback } from "@/components/ui/avatar";
import { ScrollArea } from "@/components/ui/scroll-area";
import { useAgentStore } from "@/store/agentStore";

export function ChatHistory() {
  const { messages } = useAgentStore();
  const scrollRef = useRef<HTMLDivElement>(null);

  // Auto-scroll to bottom when new messages arrive
  useEffect(() => {
    if (scrollRef.current) {
      scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
    }
  }, []);

  if (messages.length === 0) {
    return (
      <div className="flex h-[360px] items-center justify-center rounded-xl bg-muted/20 shadow-sm md:h-[420px]">
        <div className="flex flex-col items-center gap-4 text-center">
          <div className="rounded-full bg-muted p-4 shadow-sm">
            <Bot className="h-8 w-8 text-muted-foreground" />
          </div>
          <p className="text-muted-foreground text-sm">
            Start speaking to see your conversation here
          </p>
        </div>
      </div>
    );
  }

  return (
    <ScrollArea className="h-[360px] w-full rounded-xl border border-border/30 ring-1 ring-border/40 md:h-[420px]">
      <div ref={scrollRef} className="space-y-4 p-4">
        {messages.map((message, _index) => (
          <div
            key={message.id}
            className={`flex gap-3 ${
              message.role === "user" ? "flex-row-reverse" : "flex-row"
            }`}
          >
            <Avatar className="h-8 w-8 shrink-0">
              <AvatarFallback
                className={
                  message.role === "user"
                    ? "bg-primary text-primary-foreground"
                    : "bg-secondary text-secondary-foreground"
                }
              >
                {message.role === "user" ? (
                  <User className="h-4 w-4" />
                ) : (
                  <Bot className="h-4 w-4" />
                )}
              </AvatarFallback>
            </Avatar>
            <div
              className={`flex flex-col gap-1 ${
                message.role === "user" ? "items-end" : "items-start"
              }`}
            >
              <div
                className={`max-w-[85%] rounded-lg px-4 py-2.5 ${
                  message.role === "user"
                    ? "bg-primary text-primary-foreground"
                    : "bg-muted/60 text-foreground"
                }`}
              >
                <p className="whitespace-pre-wrap text-sm leading-relaxed">
                  {message.content}
                </p>
              </div>
              <p className="px-2 text-muted-foreground text-xs">
                {new Date(message.timestamp).toLocaleTimeString([], {
                  hour: "2-digit",
                  minute: "2-digit",
                })}
              </p>
            </div>
          </div>
        ))}
      </div>
    </ScrollArea>
  );
}
