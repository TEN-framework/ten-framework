"use client";

import { Card, CardContent } from "@/components/ui/card";
import { useAgentStore } from "@/store/agentStore";

export function TranscriptionDisplay() {
  const { transcribing } = useAgentStore();

  if (!transcribing) return null;

  return (
    <Card className="border-primary/50">
      <CardContent className="p-4">
        <div className="flex items-center gap-2">
          <div className="flex gap-1">
            <span className="animate-bounce">●</span>
            <span className="animate-bounce delay-100">●</span>
            <span className="animate-bounce delay-200">●</span>
          </div>
          <p className="text-sm text-muted-foreground italic">
            {transcribing}
          </p>
        </div>
      </CardContent>
    </Card>
  );
}
