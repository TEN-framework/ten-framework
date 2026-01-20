"use client";

import { Mic2 } from "lucide-react";
import { Card, CardContent } from "@/components/ui/card";
import { Skeleton } from "@/components/ui/skeleton";
import { useAgentStore } from "@/store/agentStore";

export function TranscriptionDisplay() {
  const { transcribing } = useAgentStore();

  if (!transcribing) return null;

  // Check if it's a loading state (no actual text yet)
  const isLoading = transcribing === "Listening..." || transcribing === "";

  return (
    <Card style={{ borderColor: "hsl(var(--primary) / 0.2)" }}>
      <CardContent className="p-4">
        {isLoading ? (
          <div className="flex items-center gap-3">
            <div
              className="rounded-full p-2"
              style={{ backgroundColor: "hsl(var(--primary) / 0.1)" }}
            >
              <Mic2 className="h-4 w-4 animate-pulse text-primary" />
            </div>
            <div className="flex-1 space-y-2">
              <Skeleton className="h-4 w-3/4" />
              <Skeleton className="h-4 w-1/2" />
            </div>
          </div>
        ) : (
          <div className="flex items-start gap-3">
            <div className="flex gap-1 pt-1">
              <span className="h-2 w-2 animate-bounce rounded-full bg-primary" />
              <span
                className="h-2 w-2 animate-bounce rounded-full bg-primary"
                style={{ animationDelay: "0.1s" }}
              />
              <span
                className="h-2 w-2 animate-bounce rounded-full bg-primary"
                style={{ animationDelay: "0.2s" }}
              />
            </div>
            <p className="flex-1 text-foreground text-sm leading-relaxed">
              {transcribing}
            </p>
          </div>
        )}
      </CardContent>
    </Card>
  );
}
