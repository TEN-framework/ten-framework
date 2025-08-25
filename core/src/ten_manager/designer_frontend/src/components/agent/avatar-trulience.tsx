//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
"use client";

import type { IRemoteAudioTrack } from "agora-rtc-react";
import { Maximize, Minimize } from "lucide-react";
import { useEffect, useMemo, useRef, useState } from "react";
import { TrulienceAvatar } from "trulience-sdk";
import { Progress, ProgressIndicator } from "@/components/ui/progress";
import { cn } from "@/lib/utils";
import { useAppStore } from "@/store";
import "./trulience.css";

interface AvatarProps {
  audioTrack?: IRemoteAudioTrack;
}

export default function Avatar({ audioTrack }: AvatarProps) {
  const { preferences } = useAppStore();
  const trulienceSettings = preferences.trulience;
  const trulienceAvatarRef = useRef<TrulienceAvatar>(null);
  const [errorMessage, setErrorMessage] = useState<string>("");

  // Track loading progress
  const [loadProgress, setLoadProgress] = useState(0);

  // State for toggling fullscreen
  const [fullscreen, setFullscreen] = useState(false);

  // Define event callbacks
  const eventCallbacks = useMemo(() => {
    return {
      "auth-success": (resp: string) => {
        console.log("Trulience Avatar auth-success:", resp);
      },
      "auth-fail": (resp: { message: string }) => {
        console.log("Trulience Avatar auth-fail:", resp);
        setErrorMessage(resp.message);
      },
      "websocket-connect": (resp: string) => {
        console.log("Trulience Avatar websocket-connect:", resp);
      },
      "load-progress": (details: { progress: number }) => {
        console.log("Trulience Avatar load-progress:", details.progress);
        setLoadProgress(details.progress);
      },
    };
  }, []);

  // Only create TrulienceAvatar instance once we have a final avatar ID
  const trulienceAvatarInstance = useMemo(() => {
    if (!trulienceSettings.trulienceAvatarId) return null;
    return (
      <TrulienceAvatar
        url={trulienceSettings.trulienceSdkUrl}
        ref={trulienceAvatarRef}
        avatarId={trulienceSettings.trulienceAvatarId}
        token={trulienceSettings.trulienceAvatarToken}
        eventCallbacks={eventCallbacks}
        width="100%"
        height="100%"
      />
    );
  }, [
    trulienceSettings.trulienceAvatarId,
    trulienceSettings.trulienceSdkUrl,
    trulienceSettings.trulienceAvatarToken,
    eventCallbacks,
  ]);

  // Update the Avatar’s audio stream whenever audioTrack changes
  // or when agentConnected changes
  useEffect(() => {
    const currentAvatarRef = trulienceAvatarRef.current;
    if (currentAvatarRef) {
      const trulienceObj = currentAvatarRef.getTrulienceObject();
      if (audioTrack) {
        const stream = new MediaStream([audioTrack.getMediaStreamTrack()]);
        currentAvatarRef.setMediaStream(null);
        currentAvatarRef.setMediaStream(stream);
        console.warn("[TrulienceAvatar] MediaStream set:", stream);
      }
      if (trulienceObj) {
        trulienceObj.setSpeakerEnabled(true);
      }
    }

    // Cleanup: unset media stream
    return () => {
      currentAvatarRef?.setMediaStream(null);
    };
  }, [audioTrack]);

  return (
    <div
      className={cn("relative h-full w-full overflow-hidden rounded-lg", {
        "absolute top-0 left-0 h-screen w-screen rounded-none": fullscreen,
      })}
    >
      <button
        type="button"
        className={cn(
          "absolute top-2 right-2 z-10",
          "rounded-lg bg-black/50 p-2",
          "transition hover:bg-black/70"
        )}
        onClick={() => setFullscreen((prevValue) => !prevValue)}
      >
        {fullscreen ? (
          <Minimize className="text-white" size={24} />
        ) : (
          <Maximize className="text-white" size={24} />
        )}
      </button>

      {/* Render the TrulienceAvatar */}
      {trulienceAvatarInstance}

      {/* Show a loader overlay while progress < 1 */}
      {errorMessage ? (
        <div
          className={cn(
            "absolute inset-0 z-10 flex items-center justify-center",
            "bg-red-500 bg-opacity-80 text-white"
          )}
        >
          <div>{errorMessage}</div>
        </div>
      ) : (
        loadProgress < 1 && (
          <div
            className={cn(
              "absolute inset-0 z-10 flex items-center justify-center",
              "bg-black bg-opacity-80"
            )}
          >
            {/* Simple Tailwind spinner */}
            <Progress
              className={cn(
                "relative h-[15px] w-[200px]",
                "overflow-hidden rounded-full bg-blackA6"
              )}
              style={{
                // Fix overflow clipping in Safari
                // https://gist.github.com/domske/b66047671c780a238b51c51ffde8d3a0
                transform: "translateZ(0)",
              }}
              value={loadProgress * 100}
            >
              <ProgressIndicator
                className={cn(
                  "0, 0.35, 1)] ease-[cubic-bezier(0.65,",
                  "size-full bg-white",
                  "transition-transform duration-[660ms]"
                )}
                style={{
                  transform: `translateX(-${100 - loadProgress * 100}%)`,
                }}
              />
            </Progress>
          </div>
        )
      )}
    </div>
  );
}
