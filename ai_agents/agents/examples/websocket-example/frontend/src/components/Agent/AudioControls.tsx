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
  isDisabled: boolean;
  onStartRecording: () => void;
  onStopRecording: () => void;
}

export function AudioControls({
  isRecording,
  isDisabled,
  onStartRecording,
  onStopRecording,
}: AudioControlsProps) {
  return (
    <div className="flex items-center justify-center">
      <TooltipProvider>
        {isRecording ? (
          <Tooltip>
            <TooltipTrigger asChild>
              <Button
                size="lg"
                variant="destructive"
                className="h-20 w-20 rounded-full shadow-md hover:shadow-lg transition-all duration-300 hover:scale-105 animate-pulse-subtle"
                onClick={onStopRecording}
              >
                <MicOff className="h-8 w-8" />
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
                size="lg"
                className="h-20 w-20 rounded-full shadow-md hover:shadow-lg transition-all duration-300 hover:scale-105 disabled:opacity-40 disabled:cursor-not-allowed disabled:hover:scale-100"
                onClick={onStartRecording}
                disabled={isDisabled}
              >
                <Mic className="h-8 w-8" />
              </Button>
            </TooltipTrigger>
            <TooltipContent>
              <p>{isDisabled ? "Connect agent first" : "Start recording"}</p>
            </TooltipContent>
          </Tooltip>
        )}
      </TooltipProvider>
    </div>
  );
}
