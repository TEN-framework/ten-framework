"use client";

import { Badge } from "@/components/ui/badge";
import { useAgentStore } from "@/store/agentStore";
import { Circle } from "lucide-react";

export function ConnectionStatus() {
  const { wsConnected, status } = useAgentStore();

  const getStatusColor = () => {
    if (!wsConnected) return "text-red-500";
    if (status === "connected") return "text-green-500";
    if (status === "connecting") return "text-yellow-500";
    return "text-gray-500";
  };

  const getStatusText = () => {
    if (!wsConnected) return "Disconnected";
    if (status === "connected") return "Connected";
    if (status === "connecting") return "Connecting...";
    return "Idle";
  };

  return (
    <Badge variant="outline" className="flex items-center gap-2">
      <Circle className={`h-2 w-2 fill-current ${getStatusColor()}`} />
      <span>{getStatusText()}</span>
    </Badge>
  );
}
