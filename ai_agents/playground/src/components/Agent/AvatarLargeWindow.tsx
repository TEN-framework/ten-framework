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

  const showTrulience = mode === "trulience";
  const showAnam = mode === "anam";
  const showBlank = mode === "blank";
  const showTransferring = mode === "transferring";

  return (
    <div
      className={cn("relative h-full w-full overflow-hidden rounded-lg bg-[#0F0F11]", {
        ["absolute top-0 left-0 h-screen w-screen rounded-none"]: fullscreen,
      })}
    >
      {/* Fullscreen toggle button */}
      <button
        className="absolute top-2 right-2 z-20 rounded-lg bg-black/50 p-2 transition hover:bg-black/70"
        onClick={() => setFullscreen((prevValue) => !prevValue)}
      >
        {fullscreen ? (
          <Minimize className="text-white" size={24} />
        ) : (
          <Maximize className="text-white" size={24} />
        )}
      </button>

      {/* Trulience Avatar - always rendered, moved off-screen when not active to preload */}
      <div
        className={cn("absolute inset-0", {
          ["-translate-x-[200%]"]: !showTrulience,
        })}
      >
        {finalAvatarId && (
          <TrulienceAvatar
            url={trulienceSettings.trulienceSDK}
            ref={trulienceAvatarRef}
            avatarId={finalAvatarId}
            token={trulienceSettings.avatarToken}
            eventCallbacks={eventCallbacks}
            width="100%"
            height="100%"
          />
        )}

        {/* Trulience loader overlay */}
        {showTrulience && errorMessage ? (
          <div className="absolute inset-0 z-10 flex items-center justify-center bg-red-500 bg-opacity-80 text-white">
            <div>{errorMessage}</div>
          </div>
        ) : (
          showTrulience && loadProgress < 1 && (
            <div className="absolute inset-0 z-10 flex items-center justify-center bg-black bg-opacity-80">
              <Progress
                className="relative h-[15px] w-[200px] overflow-hidden rounded-full bg-blackA6"
                style={{ transform: "translateZ(0)" }}
                value={loadProgress * 100}
              >
                <ProgressIndicator
                  className="size-full bg-white transition-transform duration-660 ease-[cubic-bezier(0.65,0,0.35,1)]"
                  style={{ transform: `translateX(-${100 - loadProgress * 100}%)` }}
                />
              </Progress>
            </div>
          )
        )}
      </div>

      {/* Anam video container */}
      <div
        className={cn("absolute inset-0", {
          ["invisible"]: !showAnam,
        })}
      >
        <div
          id="avatar-large-window-video"
          className="h-full w-full"
          style={{ minHeight: "100%" }}
        />
        {showAnam && !videoTrack && (
          <div className="absolute inset-0 flex items-center justify-center">
            <p className="text-gray-500">Waiting for avatar video...</p>
          </div>
        )}
      </div>

      {/* Blank mode overlay */}
      {showBlank && (
        <div className="absolute inset-0 flex items-center justify-center">
          <div className="text-center text-gray-500">
            <p className="text-lg">Press connect to begin</p>
          </div>
        </div>
      )}

      {/* Transferring mode overlay */}
      {showTransferring && (
        <div className="absolute inset-0 flex items-center justify-center">
          <div className="text-center text-white">
            <p className="text-lg animate-pulse">Transferring...</p>
          </div>
        </div>
      )}
    </div>
  );
}
