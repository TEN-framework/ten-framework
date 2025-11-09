"use client";

import { cn } from "@/lib/utils";
import { useEffect, useRef, useState } from "react";

interface AudioVisualizerProps {
  stream: MediaStream | null;
  isActive?: boolean;
  barCount?: number;
  barWidth?: number;
  barGap?: number;
  height?: number;
  className?: string;
}

export function AudioVisualizer({
  stream,
  isActive = false,
  barCount = 20,
  barWidth = 4,
  barGap = 2,
  height = 60,
  className,
}: AudioVisualizerProps) {
  const [frequencyData, setFrequencyData] = useState<Uint8Array>(
    new Uint8Array(barCount),
  );
  const analyserRef = useRef<AnalyserNode | null>(null);
  const audioContextRef = useRef<AudioContext | null>(null);
  const animationFrameRef = useRef<number | null>(null);

  useEffect(() => {
    if (!stream || !isActive) {
      // Clean up
      if (animationFrameRef.current) {
        cancelAnimationFrame(animationFrameRef.current);
      }
      if (audioContextRef.current) {
        audioContextRef.current.close();
        audioContextRef.current = null;
      }
      analyserRef.current = null;
      setFrequencyData(new Uint8Array(barCount).fill(0));
      return;
    }

    // Create audio context and analyser
    audioContextRef.current = new AudioContext();
    const analyser = audioContextRef.current.createAnalyser();
    analyser.fftSize = 256;
    analyserRef.current = analyser;

    const source = audioContextRef.current.createMediaStreamSource(stream);
    source.connect(analyser);

    const dataArray = new Uint8Array(analyser.frequencyBinCount);

    const updateFrequencyData = () => {
      if (!analyserRef.current) return;

      analyserRef.current.getByteFrequencyData(dataArray);

      // Sample the data to match barCount
      const step = Math.floor(dataArray.length / barCount);
      const sampledData = new Uint8Array(barCount);
      for (let i = 0; i < barCount; i++) {
        sampledData[i] = dataArray[i * step];
      }

      setFrequencyData(sampledData);
      animationFrameRef.current = requestAnimationFrame(updateFrequencyData);
    };

    updateFrequencyData();

    return () => {
      if (animationFrameRef.current) {
        cancelAnimationFrame(animationFrameRef.current);
      }
      if (audioContextRef.current) {
        audioContextRef.current.close();
      }
    };
  }, [stream, isActive, barCount]);

  const maxBarHeight = height;

  return (
    <div
      className={cn("flex items-end justify-center gap-0.5", className)}
      style={{ height: `${height}px` }}
    >
      {Array.from(frequencyData).map((value, index) => {
        const barHeight = Math.max(2, (value / 255) * maxBarHeight);
        return (
          <div
            key={index}
            className="bg-primary transition-all duration-75 ease-out"
            style={{
              width: `${barWidth}px`,
              height: `${barHeight}px`,
              marginRight: index < barCount - 1 ? `${barGap}px` : "0",
            }}
          />
        );
      })}
    </div>
  );
}
