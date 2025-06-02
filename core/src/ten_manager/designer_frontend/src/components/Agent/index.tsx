//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
"use client";

import { cn } from "@/lib/utils";
import AudioVisualizer from "@/components/Agent/AudioVisualizer";
import {
  RemoteAudioTrack,
  useRemoteUsers,
  useRemoteUserTrack,
} from "agora-rtc-react";
import { BotMessageSquareIcon } from "lucide-react";
import { useAppStore } from "@/store";
import Avatar from "@/components/Agent/AvatarTrulience";

export default function AgentView() {
  const remoteUsers = useRemoteUsers();
  const { track } = useRemoteUserTrack(remoteUsers[0], "audio");
  const { preferences } = useAppStore();

  return (
    <div
      className={cn(
        "flex h-full w-full flex-col items-center justify-center relative"
      )}
    >
      {!preferences?.trulience?.enabled ? (
        <>
          <div className="text-lg font-semibold text-primary absolute top-4">
            <BotMessageSquareIcon size={48} />
          </div>
          <div className="h-12 w-full flex items-center justify-center mt-16">
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
        <div className="h-64 w-full flex items-center justify-center">
          <Avatar audioTrack={track} />
          {track && (
            <RemoteAudioTrack key={track.getUserId()} play track={track} />
          )}
        </div>
      )}
    </div>
  );
}
