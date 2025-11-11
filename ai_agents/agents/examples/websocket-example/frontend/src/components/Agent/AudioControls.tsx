"use client";

import { Button } from "@/components/ui/button";
import {
  Tooltip,
  TooltipContent,
  TooltipProvider,
  TooltipTrigger,
} from "@/components/ui/tooltip";
import { Mic, MicOff } from "lucide-react";

interface AudioControlsProps {
  isRecording: boolean;
  onStartRecording: () => void;
  onStopRecording: () => void;
}

export function AudioControls({
  isRecording,
  onStartRecording,
  onStopRecording,
}: AudioControlsProps) {
  return (
    <TooltipProvider>
      {isRecording ? (
        <Tooltip>
          <TooltipTrigger asChild>
            <Button
              size="sm"
              className="gap-2"
              onClick={onStopRecording}
            >
              <MicOff className="h-4 w-4" />
              Mic
            </Button>
          </TooltipTrigger>
          <TooltipContent>
            <p>Stop recording</p>
          </TooltipContent>
        </Tooltip>
      ) : (
        <Tooltip>
          <TooltipTrigger asChild>
            <Button
              size="sm"
              className="gap-2"
              onClick={onStartRecording}
            >
              <Mic className="h-4 w-4" />
              Mic
            </Button>
          </TooltipTrigger>
          <TooltipContent>
            <p>Start recording</p>
          </TooltipContent>
        </Tooltip>
      )}
    </TooltipProvider>
  );
}
