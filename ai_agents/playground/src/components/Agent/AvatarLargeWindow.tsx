"use client";

import { TrulienceAvatar } from "@trulience/react-sdk";
import type {
  IMicrophoneAudioTrack,
  IRemoteAudioTrack,
  IRemoteVideoTrack,
} from "agora-rtc-sdk-ng";
import { Maximize, Minimize } from "lucide-react";
import React, { useEffect, useMemo, useRef, useState } from "react";
import { useAppSelector } from "@/common";
import { cn } from "@/lib/utils";
import { Progress, ProgressIndicator } from "../ui/progress";

export type AvatarMode = "blank" | "anam" | "trulience" | "transferring";

interface AvatarLargeWindowProps {
  mode: AvatarMode;
  audioTrack?: IMicrophoneAudioTrack | IRemoteAudioTrack;
  videoTrack?: IRemoteVideoTrack;
}

export default function AvatarLargeWindow({
  mode,
  audioTrack,
  videoTrack,
}: AvatarLargeWindowProps) {
  const agentConnected = useAppSelector((state) => state.global.agentConnected);
  const trulienceSettings = useAppSelector(
    (state) => state.global.trulienceSettings
  );
  const trulienceAvatarRef = useRef<TrulienceAvatar>(null);
  const [errorMessage, setErrorMessage] = useState<string>("");
  const [loadProgress, setLoadProgress] = useState(0);
  const [finalAvatarId, setFinalAvatarId] = useState("");
  const [fullscreen, setFullscreen] = useState(false);

  // Safely read URL param on the client for Trulience avatar ID - do this on mount to preload
  useEffect(() => {
    if (typeof window !== "undefined") {
      const urlParams = new URLSearchParams(window.location.search);
      const avatarIdFromURL = urlParams.get("avatarId");
      setFinalAvatarId(avatarIdFromURL || trulienceSettings.avatarId || "");
    }
  }, [trulienceSettings.avatarId]);

  // Play Anam video when in anam mode
  useEffect(() => {
    if (mode === "anam" && videoTrack) {
      console.log("[AvatarLargeWindow] Playing Anam video track");
      videoTrack.play(`avatar-large-window-video`, { fit: "cover" });
      return () => {
        videoTrack.stop();
      };
    }
  }, [mode, videoTrack]);

  // Trulience event callbacks
  const eventCallbacks = useMemo(() => {
    return {
      "auth-success": (resp: string) => {
        console.log("Trulience Avatar auth-success:", resp);
      },
      "auth-fail": (resp: any) => {
        console.log("Trulience Avatar auth-fail:", resp);
        setErrorMessage(resp.message);
      },
      "websocket-connect": (resp: string) => {
        console.log("Trulience Avatar websocket-connect:", resp);
      },
      "load-progress": (details: Record<string, any>) => {
        console.log("Trulience Avatar load-progress:", details.progress);
        setLoadProgress(details.progress);
      },
    };
  }, []);

  // Always create TrulienceAvatar instance to preload it (hidden until needed)
  const trulienceAvatarInstance = useMemo(() => {
    if (!finalAvatarId) return null;
    return (
      <TrulienceAvatar
        url={trulienceSettings.trulienceSDK}
        ref={trulienceAvatarRef}
        avatarId={finalAvatarId}
        token={trulienceSettings.avatarToken}
        eventCallbacks={eventCallbacks}
        width="100%"
        height="100%"
      />
    );
  }, [finalAvatarId, eventCallbacks, trulienceSettings]);

  // Update Trulience Avatar's audio stream and stop RTC playback
  useEffect(() => {
    console.log("[AvatarLargeWindow] useEffect triggered:", {
      mode,
      hasAudioTrack: !!audioTrack,
      agentConnected,
      hasTrulienceRef: !!trulienceAvatarRef.current,
      loadProgress,
    });

    if (mode === "trulience" && trulienceAvatarRef.current) {
      if (audioTrack && agentConnected) {
        try {
          // Stop RTC audio playback to avoid double audio
          audioTrack.stop();
          console.log("[AvatarLargeWindow] Stopped RTC audio playback");

          const mediaStreamTrack = audioTrack.getMediaStreamTrack();
          console.log("[AvatarLargeWindow] Got MediaStreamTrack:", mediaStreamTrack);
          const stream = new MediaStream([mediaStreamTrack]);
          console.log("[AvatarLargeWindow] Created MediaStream:", stream);
          trulienceAvatarRef.current.setMediaStream(stream);
          // Enable speaker so Trulience plays the audio
          const trulienceObj = trulienceAvatarRef.current.getTrulienceObject();
          console.log("[AvatarLargeWindow] TrulienceObj:", trulienceObj);
          trulienceObj?.setSpeakerEnabled(true);
          console.warn("[AvatarLargeWindow] Trulience MediaStream set successfully");
        } catch (err) {
          console.error("[AvatarLargeWindow] Error setting MediaStream:", err);
        }
      } else if (!agentConnected) {
        const trulienceObj = trulienceAvatarRef.current.getTrulienceObject();
        trulienceObj?.sendMessageToAvatar(
          "<trl-stop-background-audio immediate='true' />"
        );
        trulienceObj?.sendMessageToAvatar(
          "<trl-content position='DefaultCenter' />"
        );
      }
    }
  }, [mode, audioTrack, agentConnected, loadProgress]);

  // Hidden preload container for Trulience - always render but hide when not in trulience mode
  const hiddenTruliencePreload = mode !== "trulience" && trulienceAvatarInstance && (
    <div style={{ position: "absolute", left: "-9999px", width: "1px", height: "1px", overflow: "hidden" }}>
      {trulienceAvatarInstance}
    </div>
  );

  // Blank mode - show empty dark background
  if (mode === "blank") {
    return (
      <div
        className={cn(
          "relative flex h-full w-full items-center justify-center overflow-hidden rounded-lg bg-[#0F0F11]",
          {
            ["absolute top-0 left-0 h-screen w-screen rounded-none"]: fullscreen,
          }
        )}
      >
        {hiddenTruliencePreload}
        <div className="text-center text-gray-500">
          <p className="text-lg">Press connect to begin</p>
        </div>
      </div>
    );
  }

  // Transferring mode - show loading state
  if (mode === "transferring") {
    return (
      <div
        className={cn(
          "relative flex h-full w-full items-center justify-center overflow-hidden rounded-lg bg-[#0F0F11]",
          {
            ["absolute top-0 left-0 h-screen w-screen rounded-none"]: fullscreen,
          }
        )}
      >
        {hiddenTruliencePreload}
        <div className="text-center text-white">
          <p className="text-lg animate-pulse">Transferring...</p>
        </div>
      </div>
    );
  }

  // Anam mode - show RTC video
  if (mode === "anam") {
    return (
      <div
        className={cn(
          "relative h-full w-full overflow-hidden rounded-lg bg-[#0F0F11]",
          {
            ["absolute top-0 left-0 h-screen w-screen rounded-none"]: fullscreen,
          }
        )}
      >
        {hiddenTruliencePreload}
        <button
          className="absolute top-2 right-2 z-10 rounded-lg bg-black/50 p-2 transition hover:bg-black/70"
          onClick={() => setFullscreen((prevValue) => !prevValue)}
        >
          {fullscreen ? (
            <Minimize className="text-white" size={24} />
          ) : (
            <Maximize className="text-white" size={24} />
          )}
        </button>

        {/* Anam video container */}
        <div
          id="avatar-large-window-video"
          className="h-full w-full"
          style={{ minHeight: "100%" }}
        />

        {/* Show placeholder if no video */}
        {!videoTrack && (
          <div className="absolute inset-0 flex items-center justify-center">
            <p className="text-gray-500">Waiting for avatar video...</p>
          </div>
        )}
      </div>
    );
  }

  // Trulience mode
  return (
    <div
      className={cn("relative h-full w-full overflow-hidden rounded-lg", {
        ["absolute top-0 left-0 h-screen w-screen rounded-none"]: fullscreen,
      })}
    >
      <button
        className="absolute top-2 right-2 z-10 rounded-lg bg-black/50 p-2 transition hover:bg-black/70"
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
        <div className="absolute inset-0 z-10 flex items-center justify-center bg-red-500 bg-opacity-80 text-white">
          <div>{errorMessage}</div>
        </div>
      ) : (
        loadProgress < 1 && (
          <div className="absolute inset-0 z-10 flex items-center justify-center bg-black bg-opacity-80">
            <Progress
              className="relative h-[15px] w-[200px] overflow-hidden rounded-full bg-blackA6"
              style={{
                transform: "translateZ(0)",
              }}
              value={loadProgress * 100}
            >
              <ProgressIndicator
                className="0, 0.35, 1)] size-full bg-white transition-transform duration-660 ease-[cubic-bezier(0.65,"
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
