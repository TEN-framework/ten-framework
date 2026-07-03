//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import type { IMicrophoneAudioTrack, IRemoteAudioTrack } from "agora-rtc-react";
import { useMultibandTrackVolume } from "@/hooks/use-audio-visualizer";

export interface AudioVisualizerProps {
  type: "agent" | "user";
  track?: IMicrophoneAudioTrack | IRemoteAudioTrack | null;
  bands: number;
  gap: number;
  barWidth: number;
  minBarHeight: number;
  maxBarHeight: number;
  borderRadius: number;
}

export default function AudioVisualizer(props: AudioVisualizerProps) {
  const {
    track,
    bands = 5,
    gap,
    barWidth,
    minBarHeight,
    maxBarHeight,
    borderRadius,
  } = props;

  const frequencies = useMultibandTrackVolume(track, bands);

  const summedFrequencies = frequencies.map((bandFrequencies) => {
    const sum = bandFrequencies.reduce((a, b) => a + b, 0);
    if (sum <= 0) {
      return 0;
    }
    return Math.sqrt(sum / bandFrequencies.length);
  });

  return (
    <div
      className={`flex items-center justify-center`}
      style={{ gap: `${gap}px` }}
    >
      {summedFrequencies.map((frequency, index) => {
        const style = {
          // eslint-disable-next-line max-len
          height: `${minBarHeight + frequency * (maxBarHeight - minBarHeight)}px`,
          borderRadius: `${borderRadius}px`,
          width: `${barWidth}px`,
        };

        // biome-ignore lint/suspicious/noArrayIndexKey: <ignore>
        return <span key={index} className="bg-foreground" style={style} />;
      })}
    </div>
  );
}
