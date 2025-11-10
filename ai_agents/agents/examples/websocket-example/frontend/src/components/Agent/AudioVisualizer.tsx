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
  barCount = 40,
  barWidth = 3,
  barGap = 2,
  height = 80,
  className,
}: AudioVisualizerProps) {
  const [frequencyData, setFrequencyData] = useState<Uint8Array>(
    new Uint8Array(barCount),
  );
  // Prevent hydration mismatches by avoiding time-based inline styles
  // on the server-rendered HTML. We render static bars until mounted.
  const [mounted, setMounted] = useState(false);
  const analyserRef = useRef<AnalyserNode | null>(null);
  const audioContextRef = useRef<AudioContext | null>(null);
  const animationFrameRef = useRef<number | null>(null);

  useEffect(() => {
    setMounted(true);
  }, []);

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
    analyser.fftSize = 512;
    analyser.smoothingTimeConstant = 0.8;
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
      suppressHydrationWarning
      className={cn(
        "absolute inset-0 flex w-full h-full items-end justify-center gap-0.5",
        className,
      )}
      style={{
        height: `${height}px`,
      }}
    >
      {Array.from(frequencyData).map((value, index) => {
        const base = 6; // ensure visibility when idle
        const barHeight = isActive
          ? Math.max(base, (value / 255) * maxBarHeight)
          : mounted
            ? base + Math.sin(index * 0.5 + Date.now() / 1000) * 4
            : base; // static height pre-hydration

        // Create gradient effect from center
        const distanceFromCenter = Math.abs(index - barCount / 2) / (barCount / 2);
        const opacity = 1 - distanceFromCenter * 0.3;

        return (
          <div
            key={index}
            className={cn(
              "rounded-full transition-all duration-150 ease-out",
              isActive ? "shadow-sm" : undefined,
            )}
            style={{
              backgroundColor: isActive
                ? "hsl(var(--color-foreground))"
                : "hsl(var(--color-foreground) / 0.5)",
              width: `${barWidth}px`,
              height: `${barHeight}px`,
              marginRight: index < barCount - 1 ? `${barGap}px` : 0,
              opacity,
            }}
          />
        );
      })}
    </div>
  );
}
