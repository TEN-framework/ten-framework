"use client";

import { Badge } from "@/components/ui/badge";
import { useAgentStore } from "@/store/agentStore";
import { Circle } from "lucide-react";

export function ConnectionStatus() {
  const { wsConnected, status } = useAgentStore();

  // Keep badge subtle regardless of state for a minimal look
  const getStatusVariant = (): "default" | "secondary" | "destructive" | "outline" => {
    return "outline";
  };

  const getStatusColor = () => {
    if (!wsConnected) return "text-red-500";
    if (status === "connected") return "text-emerald-500";
    if (status === "connecting") return "text-amber-500";
    return "text-muted-foreground";
  };

  const getStatusText = () => {
    if (!wsConnected) return "Disconnected";
    if (status === "connected") return "Connected";
    if (status === "connecting") return "Connecting...";
    return "Idle";
  };

  return (
    <Badge variant={getStatusVariant()} className="flex items-center gap-2 px-3 py-1">
      <Circle className={`h-2 w-2 fill-current ${getStatusColor()}`} />
      <span>{getStatusText()}</span>
    </Badge>
  );
}
