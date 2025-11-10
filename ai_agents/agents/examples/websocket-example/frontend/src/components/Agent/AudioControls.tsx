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
    <TooltipProvider>
      {isRecording ? (
        <Tooltip>
          <TooltipTrigger asChild>
            <Button
              size="lg"
              variant="destructive"
              className="gap-2"
              onClick={onStopRecording}
            >
              <MicOff className="h-4 w-4" />
              Recording
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
              className="gap-2"
              onClick={onStartRecording}
              disabled={isDisabled}
            >
              <Mic className="h-4 w-4" />
              Mic
            </Button>
          </TooltipTrigger>
          <TooltipContent>
            <p>{isDisabled ? "Connect agent first" : "Start recording"}</p>
          </TooltipContent>
        </Tooltip>
      )}
    </TooltipProvider>
  );
}
