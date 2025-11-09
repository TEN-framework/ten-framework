"use client";

import { Button } from "@/components/ui/button";
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
      {isRecording ? (
        <Button
          size="lg"
          variant="destructive"
          className="h-16 w-16 rounded-full"
          onClick={onStopRecording}
        >
          <MicOff className="h-6 w-6" />
        </Button>
      ) : (
        <Button
          size="lg"
          className="h-16 w-16 rounded-full"
          onClick={onStartRecording}
          disabled={isDisabled}
        >
          <Mic className="h-6 w-6" />
        </Button>
      )}
    </div>
  );
}
