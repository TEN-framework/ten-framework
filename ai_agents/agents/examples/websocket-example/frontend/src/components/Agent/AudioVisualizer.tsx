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
    analyser.fftSize = 512; // Larger FFT for better frequency resolution
    analyser.smoothingTimeConstant = 0.2; // Lower smoothing for more sensitivity
    analyserRef.current = analyser;

    const source = audioContextRef.current.createMediaStreamSource(stream);
    source.connect(analyser);

    const dataArray = new Uint8Array(analyser.frequencyBinCount);

    const updateFrequencyData = () => {
      if (!analyserRef.current) return;

      analyserRef.current.getByteFrequencyData(dataArray);

      // Sample the data to match barCount with logarithmic scaling for better high-frequency visibility
      const sampledData = new Uint8Array(barCount);
      const dataLength = dataArray.length;

      for (let i = 0; i < barCount; i++) {
        // Use logarithmic scaling to better represent higher frequencies
        // Map bar index to frequency bin using logarithmic scale
        const normalizedIndex = i / (barCount - 1);
        const logIndex = Math.pow(normalizedIndex, 0.5); // Square root for better distribution
        const binIndex = Math.floor(logIndex * dataLength);
        const clampedIndex = Math.min(binIndex, dataLength - 1);

        // Take the max of nearby bins for smoother visualization
        const windowSize = Math.max(1, Math.floor(dataLength / barCount / 4));
        let maxValue = 0;
        for (let j = Math.max(0, clampedIndex - windowSize); j < Math.min(dataLength, clampedIndex + windowSize); j++) {
          maxValue = Math.max(maxValue, dataArray[j]);
        }

        sampledData[i] = maxValue;
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
        // Adjust sensitivity: less sensitive on left (low freq), more sensitive on right (high freq)
        const normalizedIndex = index / (barCount - 1);
        // Left side gets lower sensitivity, right side gets higher sensitivity
        // Use a curve: left side (0) = 0.8x, right side (1) = 2.5x
        const sensitivityMultiplier = 0.8 + (normalizedIndex * normalizedIndex * 1.7); // Quadratic curve for smoother transition
        const amplifiedValue = Math.pow(value / 255, 0.5) * 255; // Stronger amplification (0.5 = even more amplification)
        const barHeight = isActive
          ? Math.min(maxBarHeight, Math.max(base, (amplifiedValue / 255) * maxBarHeight * sensitivityMultiplier))
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
                ? "hsl(var(--foreground))"
                : "hsl(var(--foreground) / 0.5)",
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
