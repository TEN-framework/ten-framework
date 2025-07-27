//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
"use client";

import {
  RemoteAudioTrack,
  useRemoteUsers,
  useRemoteUserTrack,
} from "agora-rtc-react";
import { BotMessageSquareIcon } from "lucide-react";
import AudioVisualizer from "@/components/agent/audio-visualizer";
import Avatar from "@/components/agent/avatar-trulience";
import { cn } from "@/lib/utils";
import { useAppStore } from "@/store";

export default function AgentView() {
  const remoteUsers = useRemoteUsers();
  const { track } = useRemoteUserTrack(remoteUsers[0], "audio");
  const { preferences } = useAppStore();

  return (
    <div
      className={cn(
        "relative flex h-full w-full flex-col items-center justify-center"
      )}
    >
      {!preferences?.trulience?.enabled ? (
        <>
          <div className="absolute top-4 font-semibold text-lg text-primary">
            <BotMessageSquareIcon size={48} />
          </div>
          <div className="mt-16 flex h-12 w-full items-center justify-center">
            <AudioVisualizer
              type="agent"
              track={track}
              bands={12}
              barWidth={4}
              minBarHeight={4}
              maxBarHeight={28}
              borderRadius={2}
              gap={4}
            />
            {track && (
              <RemoteAudioTrack key={track.getUserId()} play track={track} />
            )}
          </div>
        </>
      ) : (
        <div className="flex h-64 w-full items-center justify-center">
          <Avatar audioTrack={track} />
          {track && (
            <RemoteAudioTrack key={track.getUserId()} play track={track} />
          )}
        </div>
      )}
    </div>
  );
}
