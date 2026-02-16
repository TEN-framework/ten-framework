"use client";

// import AudioVisualizer from "../audioVisualizer"
import type { IRemoteAudioTrack, IRemoteVideoTrack } from "agora-rtc-sdk-ng";
import { Activity, AlertTriangle, CheckCircle2, Wrench } from "lucide-react";
import { useEffect } from "react";
import { useAppSelector, useMultibandTrackVolume } from "@/common";
import AudioVisualizer from "@/components/Agent/AudioVisualizer";
import { cn } from "@/lib/utils";

export interface AgentViewProps {
  audioTrack?: IRemoteAudioTrack;
  videoTrack?: IRemoteVideoTrack;
}

export default function AgentView(props: AgentViewProps) {
  const { audioTrack, videoTrack } = props;
  const agentPhase = useAppSelector((state) => state.global.agentPhase);

  const subscribedVolumes = useMultibandTrackVolume(audioTrack, 12);

  useEffect(() => {
    if (videoTrack) {
      const currentTrack = videoTrack;
      currentTrack.play(`remote-video-${currentTrack.getUserId()}`, {
        fit: "cover",
      });

      return () => {
        currentTrack.stop();
      };
    }
  }, [videoTrack]);

  return videoTrack ? (
    <div
      id={`remote-video-${videoTrack.getUserId()}`}
      className="relative w-full overflow-hidden bg-[#0F0F11]"
      style={{ minHeight: "240px", height: "240px", position: "relative" }}
    ></div>
  ) : (
    <div
      className={cn(
        "flex w-full flex-col items-center justify-center px-4",
        "bg-[#0F0F11] bg-linear-to-br from-[rgba(27,66,166,0.16)] via-[rgba(27,45,140,0.00)] to-[#11174E] shadow-[0px_3.999px_48.988px_0px_rgba(0,7,72,0.12)] backdrop-blur-[7px]"
      )}
      style={{ minHeight: "240px", height: "240px" }}
    >
      <div className="mb-4 text-center">
        <div className="font-semibold text-[#EAECF0] text-lg">Agent</div>
        <AgentPhasePill phase={agentPhase} />
      </div>
      <div className="h-20 w-full">
        <AudioVisualizer
          type="agent"
          frequencies={subscribedVolumes}
          barWidth={6}
          minBarHeight={6}
          maxBarHeight={80}
          borderRadius={2}
          gap={6}
        />
      </div>
    </div>
  );
}

function AgentPhasePill(props: { phase: string }) {
  const { phase } = props;
  const normalized = phase.toLowerCase();
  const label = getPhaseLabel(normalized);
  const effectivePhase = normalized || "idle";
  const isError = normalized.includes("error");
  const isTool = normalized.includes("tool");
  const isDone = normalized.includes("end") || normalized.includes("done");
  const isIdle = !normalized;

  const Icon = isError
    ? AlertTriangle
    : isTool
      ? Wrench
      : isDone
        ? CheckCircle2
        : Activity;

  const pillClass = isError
    ? "border-[#7A1D1D] bg-[#2A0E0E] text-[#F6B5B5]"
    : isTool
      ? "border-[#2C5A73] bg-[#0E1F2A] text-[#CDEBFF]"
      : isDone
        ? "border-[#1E5A36] bg-[#0E2416] text-[#BCE8C7]"
        : isIdle
          ? "border-[#2A2F3A] bg-[#151820] text-[#B3BCCB]"
          : "border-[#2C5A73] bg-[#0E1F2A] text-[#CDEBFF]";

  return (
    <div className="mt-2 flex justify-center">
      <div
        className={cn(
          "relative flex items-center gap-2 overflow-hidden rounded-full border px-2.5 py-1 text-xs shadow-sm",
          "transition-colors duration-200 ring-1 ring-current/20 shadow-[0_0_12px_rgba(64,160,255,0.18)]",
          pillClass
        )}
      >
        <span
          aria-hidden="true"
          className="pointer-events-none absolute inset-0 -translate-x-full bg-linear-to-r from-transparent via-white/35 to-transparent blur-[1px] animate-[neon-sweep_2.4s_linear_infinite]"
        />
        <span className="relative flex h-2.5 w-2.5">
          <span className="absolute inline-flex h-full w-full animate-ping rounded-full bg-current opacity-60 blur-[1px]" />
          <span className="relative inline-flex h-2.5 w-2.5 rounded-full bg-current shadow-[0_0_6px_rgba(255,255,255,0.35)]" />
        </span>
        <Icon className="relative h-3.5 w-3.5 motion-safe:animate-bounce" />
        <span className="relative max-w-[180px] truncate">{label}</span>
      </div>
    </div>
  );
}

function getPhaseLabel(phase: string) {
  if (!phase) {
    return "Idle";
  }
  if (phase.includes("start")) {
    return "Thinking";
  }
  if (phase.includes("tool")) {
    return "Using tool";
  }
  if (phase.includes("end") || phase.includes("done")) {
    return "Completed";
  }
  if (phase.includes("error") || phase.includes("fail")) {
    return "Error";
  }
  return phase;
}
