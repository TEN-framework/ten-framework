"use client";

import React, { useEffect, useState } from "react";

// Force dynamic rendering
export const dynamic = "force-dynamic";

import dynamicImport from "next/dynamic";
import { Baloo_2, Quicksand } from "next/font/google";
import { HeartEmitter } from "@/components/HeartEmitter";

// Dynamically import Live2D component to prevent SSR issues
const ClientOnlyLive2D = dynamicImport(
  () => import("@/components/ClientOnlyLive2D"),
  {
    ssr: false,
    loading: () => (
      <div className="flex h-full items-center justify-center">
        <div className="text-center">
          <div className="mx-auto mb-4 h-12 w-12 animate-spin rounded-full border-white border-b-2"></div>
          <p className="text-white/70">Loading Live2D Model...</p>
        </div>
      </div>
    ),
  }
);

import { apiPing, apiStartService, apiStopService } from "@/lib/request";
import type { AgoraConfig, Live2DModel } from "@/types";

// Use Kei model with MotionSync support
const defaultModel: Live2DModel = {
  id: "kei",
  name: "Kei",
  path: "/models/kei_vowels_pro/kei_vowels_pro.model3.json",
  preview: "/models/kei_vowels_pro/preview.png",
};

const headlineFont = Baloo_2({
  subsets: ["latin"],
  weight: ["600", "700"],
});

const subtitleFont = Quicksand({
  subsets: ["latin"],
  weight: ["400", "500"],
});

type ScrawlType = "flower" | "swirl" | "spark" | "heart" | "star";

type ScrawlElement = {
  type: ScrawlType;
  top: number;
  left: number;
  size: number;
  rotate: number;
  scale: number;
  animation: string;
  duration: number;
  delay: string;
  opacity: number;
};

const scrawlElements: ScrawlElement[] = [
  {
    type: "flower",
    top: 6,
    left: 10,
    size: 140,
    rotate: -8,
    scale: 1,
    animation: "float0",
    duration: 16,
    delay: "0s",
    opacity: 0.92,
  },
  {
    type: "flower",
    top: 32,
    left: 72,
    size: 120,
    rotate: 12,
    scale: 0.9,
    animation: "float2",
    duration: 18,
    delay: "1.5s",
    opacity: 0.85,
  },
  {
    type: "swirl",
    top: 18,
    left: 52,
    size: 160,
    rotate: 4,
    scale: 1.1,
    animation: "float1",
    duration: 20,
    delay: "0.8s",
    opacity: 0.75,
  },
  {
    type: "spark",
    top: 58,
    left: 18,
    size: 150,
    rotate: -14,
    scale: 1,
    animation: "float2",
    duration: 17,
    delay: "2.2s",
    opacity: 0.8,
  },
  {
    type: "spark",
    top: 74,
    left: 70,
    size: 130,
    rotate: 18,
    scale: 0.95,
    animation: "float0",
    duration: 15,
    delay: "1s",
    opacity: 0.78,
  },
  {
    type: "swirl",
    top: 72,
    left: 42,
    size: 120,
    rotate: -6,
    scale: 0.85,
    animation: "float1",
    duration: 19,
    delay: "2.6s",
    opacity: 0.72,
  },
  {
    type: "flower",
    top: 82,
    left: 5,
    size: 110,
    rotate: 6,
    scale: 0.8,
    animation: "float0",
    duration: 22,
    delay: "3s",
    opacity: 0.65,
  },
  {
    type: "flower",
    top: 12,
    left: 82,
    size: 110,
    rotate: -18,
    scale: 0.8,
    animation: "float1",
    duration: 18,
    delay: "2s",
    opacity: 0.7,
  },
  {
    type: "heart",
    top: 40,
    left: 12,
    size: 90,
    rotate: 14,
    scale: 0.85,
    animation: "float2",
    duration: 14,
    delay: "0.4s",
    opacity: 0.78,
  },
  {
    type: "heart",
    top: 22,
    left: 88,
    size: 80,
    rotate: -22,
    scale: 0.75,
    animation: "float0",
    duration: 16,
    delay: "1.8s",
    opacity: 0.8,
  },
  {
    type: "star",
    top: 8,
    left: 46,
    size: 100,
    rotate: 6,
    scale: 0.9,
    animation: "float1",
    duration: 13,
    delay: "0.6s",
    opacity: 0.68,
  },
  {
    type: "star",
    top: 62,
    left: 88,
    size: 95,
    rotate: -12,
    scale: 0.82,
    animation: "float2",
    duration: 21,
    delay: "2.4s",
    opacity: 0.7,
  },
  {
    type: "heart",
    top: 66,
    left: 30,
    size: 85,
    rotate: -6,
    scale: 0.8,
    animation: "float0",
    duration: 18,
    delay: "1.2s",
    opacity: 0.74,
  },
  {
    type: "flower",
    top: 50,
    left: 52,
    size: 105,
    rotate: 18,
    scale: 0.88,
    animation: "float1",
    duration: 19,
    delay: "3.2s",
    opacity: 0.82,
  },
  {
    type: "star",
    top: 86,
    left: 60,
    size: 90,
    rotate: 10,
    scale: 0.78,
    animation: "float2",
    duration: 17,
    delay: "2.8s",
    opacity: 0.72,
  },
];

const renderScrawlShape = (type: ScrawlType): JSX.Element | null => {
  switch (type) {
    case "flower":
      return (
        <svg viewBox="0 0 120 120" className="h-full w-full">
          <path
            d="M60 14 C 44 20 32 38 36 52 C 20 50 18 64 32 71 C 24 84 36 101 52 95 C 56 108 70 108 74 95 C 90 101 102 84 94 71 C 108 64 106 50 90 52 C 94 38 82 20 66 14 C 63 12 61 12 60 14 Z"
            fill="rgba(255,190,213,0.28)"
            stroke="#ff92bb"
            strokeWidth={4}
            strokeLinejoin="round"
          />
          <path
            d="M60 34 C 50 38 44 48 46 56 C 38 56 34 62 38 68 C 34 74 40 84 50 80 C 52 90 68 90 70 80 C 80 84 86 74 82 68 C 86 62 82 56 74 56 C 76 48 70 38 60 34 Z"
            fill="rgba(255,245,250,0.6)"
            stroke="#ff92bb"
            strokeWidth={3}
            strokeLinejoin="round"
          />
          <circle cx="60" cy="60" r="6" fill="#ff92bb" opacity="0.75" />
        </svg>
      );
    case "swirl":
      return (
        <svg viewBox="0 0 140 140" className="h-full w-full">
          <path
            d="M30 86 C 24 52 58 34 86 40 C 108 46 124 70 112 92 C 102 110 78 116 62 104 C 52 96 52 76 68 70 C 78 66 88 74 84 84"
            fill="none"
            stroke="#8fb9ff"
            strokeWidth={6}
            strokeLinecap="round"
          />
          <path
            d="M44 38 C 58 26 90 26 110 46"
            fill="none"
            stroke="#c9deff"
            strokeWidth={4}
            strokeLinecap="round"
            strokeDasharray="12 14"
          />
          <circle cx="90" cy="102" r="6" fill="#d5e7ff" opacity="0.9" />
        </svg>
      );
    case "spark":
      return (
        <svg viewBox="0 0 140 140" className="h-full w-full">
          <path
            d="M70 14 L78 56 L118 48 L86 72 L104 110 L70 88 L36 110 L54 72 L22 48 L62 56 Z"
            fill="rgba(255,235,196,0.5)"
            stroke="#ffce7a"
            strokeWidth={5}
            strokeLinejoin="round"
          />
          <path
            d="M70 32 L74 54 L96 50 L78 64 L86 84 L70 72 L54 84 L62 64 L44 50 L66 54 Z"
            fill="rgba(255,248,225,0.7)"
            stroke="#ffce7a"
            strokeWidth={3}
            strokeLinejoin="round"
          />
        </svg>
      );
    case "heart":
      return (
        <svg viewBox="0 0 120 120" className="h-full w-full">
          <path
            d="M60 98 C 58 96 18 70 18 44 C 18 30 30 20 44 20 C 52 20 58 24 60 30 C 62 24 68 20 76 20 C 90 20 102 30 102 44 C 102 70 62 96 60 98 Z"
            fill="rgba(255,205,223,0.55)"
            stroke="#ff8fb4"
            strokeWidth={5}
            strokeLinejoin="round"
          />
          <path
            d="M45 42 C 46 34 54 32 60 38"
            fill="none"
            stroke="#ffe1ec"
            strokeWidth={4}
            strokeLinecap="round"
            strokeDasharray="10 8"
          />
        </svg>
      );
    case "star":
      return (
        <svg viewBox="0 0 120 120" className="h-full w-full">
          <path
            d="M60 12 L70 44 L104 46 L78 68 L86 102 L60 84 L34 102 L42 68 L16 46 L50 44 Z"
            fill="rgba(255,248,210,0.6)"
            stroke="#ffd27f"
            strokeWidth={4.5}
            strokeLinejoin="round"
          />
          <path
            d="M60 26 L66 44 L84 46 L70 58 L74 76 L60 66 L46 76 L50 58 L36 46 L54 44 Z"
            fill="rgba(255,242,220,0.75)"
            stroke="#ffd27f"
            strokeWidth={3}
            strokeLinejoin="round"
          />
          <circle cx="60" cy="72" r="5" fill="#ffe7b1" opacity="0.9" />
        </svg>
      );
    default:
      return null;
  }
};

export default function Home() {
  const [isConnected, setIsConnected] = useState(false);
  const [isMuted, setIsMuted] = useState(false);
  const [isConnecting, setIsConnecting] = useState(false);
  const [selectedModel, setSelectedModel] = useState<Live2DModel>(defaultModel);
  const [remoteAudioTrack, setRemoteAudioTrack] = useState<any>(null);
  const [agoraService, setAgoraService] = useState<any>(null);
  const [pingInterval, setPingInterval] = useState<NodeJS.Timeout | null>(null);
  const [isAssistantSpeaking, setIsAssistantSpeaking] = useState(false);

  useEffect(() => {
    // Dynamically import Agora service only on client side
    if (typeof window !== "undefined") {
      import("@/services/agora").then((module) => {
        const service = module.agoraService;
        setAgoraService(service);

        // Set up callbacks for Agora service
        service.setOnConnectionStatusChange(handleConnectionChange);
        service.setOnRemoteAudioTrack(handleAudioTrackChange);
      });
    }

    // Cleanup ping interval on unmount
    return () => {
      stopPing();
    };
  }, []);

  const handleConnectionChange = (status: any) => {
    setIsConnected(status.rtc === "connected");
  };

  const handleAudioTrackChange = (track: any) => {
    setRemoteAudioTrack(track);
  };

  useEffect(() => {
    let interval: NodeJS.Timeout | null = null;
    const track = remoteAudioTrack;

    const readLevel = () => {
      if (!track) return 0;
      if (typeof track.getVolumeLevel === "function") {
        return track.getVolumeLevel();
      }
      if (typeof track.getCurrentLevel === "function") {
        return track.getCurrentLevel();
      }
      return 0;
    };

    if (track) {
      interval = setInterval(() => {
        try {
          const level = readLevel();
          const speaking = level > 0.05;
          setIsAssistantSpeaking((prev) =>
            prev === speaking ? prev : speaking
          );
        } catch (err) {
          console.warn("Unable to read remote audio level:", err);
          setIsAssistantSpeaking(false);
        }
      }, 160);
    } else {
      setIsAssistantSpeaking(false);
    }

    return () => {
      if (interval) {
        clearInterval(interval);
      }
    };
  }, [remoteAudioTrack]);

  const startPing = () => {
    if (pingInterval) {
      stopPing();
    }
    const interval = setInterval(() => {
      apiPing("test-channel");
    }, 3000);
    setPingInterval(interval);
  };

  const stopPing = () => {
    if (pingInterval) {
      clearInterval(pingInterval);
      setPingInterval(null);
    }
  };

  const handleMicToggle = () => {
    if (agoraService) {
      try {
        if (isMuted) {
          agoraService.unmuteMicrophone();
          setIsMuted(false);
        } else {
          agoraService.muteMicrophone();
          setIsMuted(true);
        }
      } catch (error) {
        console.error("Error toggling microphone:", error);
      }
    }
  };

  const handleConnectToggle = async () => {
    if (agoraService) {
      try {
        if (isConnected) {
          setIsConnecting(true);
          // Stop the agent service first
          try {
            await apiStopService("test-channel");
            console.log("Agent stopped");
          } catch (error) {
            console.error("Failed to stop agent:", error);
          }

          await agoraService.disconnect();
          setIsConnected(false);
          stopPing(); // Stop ping when disconnecting
          setIsConnecting(false);
        } else {
          setIsConnecting(true);
          // Fetch Agora credentials from API server using the correct endpoint
          const response = await fetch("/api/token/generate", {
            method: "POST",
            headers: {
              "Content-Type": "application/json",
            },
            body: JSON.stringify({
              request_id: Math.random().toString(36).substring(2, 15),
              uid: Math.floor(Math.random() * 100000),
              channel_name: "test-channel",
            }),
          });

          if (!response.ok) {
            throw new Error(
              `Failed to get Agora credentials: ${response.statusText}`
            );
          }

          const responseData = await response.json();

          // Handle the response structure from agent server
          const credentials = responseData.data || responseData;

          const agoraConfig: AgoraConfig = {
            appId: credentials.appId || credentials.app_id,
            channel: credentials.channel_name,
            token: credentials.token,
            uid: credentials.uid,
          };

          console.log("Agora config:", agoraConfig);
          const success = await agoraService.connect(agoraConfig);
          if (success) {
            setIsConnected(true);

            // Sync microphone state with Agora service
            setIsMuted(agoraService.isMicrophoneMuted());

            // Start the agent service
            try {
              const startResult = await apiStartService({
                channel: agoraConfig.channel,
                userId: agoraConfig.uid || 0,
                graphName: "voice_assistant_live2d",
                language: "en",
                voiceType: "female",
              });

              console.log("Agent started:", startResult);
              startPing(); // Start ping when agent is started
            } catch (error) {
              console.error("Failed to start agent:", error);
            }
          } else {
            console.error("Failed to connect to Agora");
          }
          setIsConnecting(false);
        }
      } catch (error) {
        console.error("Error toggling connection:", error);
        setIsConnecting(false);
      }
    }
  };

  return (
    <div className="relative min-h-[100svh] overflow-hidden bg-[#fff9fd] text-[#2f2d4b]">
      <div className="absolute inset-0">
        <div className="absolute inset-0 bg-[linear-gradient(160deg,#ffeaf3_0%,#fffaf2_40%,#e3f1ff_100%)]" />
        <div className="absolute inset-0 bg-[radial-gradient(circle_at_top,#ffdff2_0%,transparent_60%),radial-gradient(circle_at_bottom,#d2e8ff_0%,transparent_65%)] opacity-75 mix-blend-screen" />
        <div
          className="absolute inset-0 opacity-20"
          style={{
            backgroundImage:
              "repeating-linear-gradient(135deg, rgba(255, 255, 255, 0.32) 0px, rgba(255, 255, 255, 0.32) 2px, transparent 2px, transparent 18px)",
          }}
        />
        <div
          className="absolute inset-0 mix-blend-multiply"
          style={{
            opacity: 0.22,
            backgroundImage:
              "repeating-radial-gradient(circle at 20% 20%, rgba(255, 204, 224, 0.25) 0px, rgba(255, 204, 224, 0.25) 12px, transparent 12px, transparent 48px)",
          }}
        />
      </div>

      <div className="pointer-events-none absolute inset-0 overflow-hidden">
        {scrawlElements.map((item, idx) => (
          <div
            key={`${item.type}-${idx}`}
            className="absolute"
            style={{
              top: `${item.top}%`,
              left: `${item.left}%`,
              width: `${item.size}px`,
              height: `${item.size}px`,
              transform: `rotate(${item.rotate}deg) scale(${item.scale})`,
              animation: `${item.animation} ${item.duration}s ease-in-out infinite`,
              animationDelay: item.delay,
              opacity: item.opacity,
              filter: "drop-shadow(0 18px 40px rgba(210, 180, 255, 0.35))",
            }}
          >
            {renderScrawlShape(item.type)}
          </div>
        ))}
      </div>

      <div className="relative z-10 flex min-h-[100svh] flex-col items-center justify-center gap-6 px-4 py-6 md:px-6 lg:gap-10">
        <header className="max-w-xl space-y-3 text-center lg:max-w-2xl">
          <span className="inline-flex items-center rounded-full bg-white/70 px-3.5 py-0.5 font-semibold text-[#ff79a8] text-[0.65rem] uppercase tracking-[0.25em] shadow-sm">
            Say hello to Kei
          </span>
          <h1
            className={`${headlineFont.className} text-3xl text-[#2f2d4b] leading-snug tracking-tight md:text-[2.75rem] md:leading-tight`}
          >
            Your Charming Clever Companion
          </h1>
          <p
            className={`${subtitleFont.className} text-[#6f6a92] text-sm md:text-base`}
          >
            Kei is a friendly guide who lights up every conversation. Connect
            with her for thoughtful answers, gentle encouragement, and a dash of
            anime sparkle whenever you need it.
          </p>
        </header>

        <main className="flex w-full max-w-5xl flex-col items-center gap-8">
          <div className="relative w-full max-w-3xl">
            <div className="-inset-5 absolute rounded-[40px] bg-gradient-to-br from-[#ffe1f1]/60 via-[#d8ecff]/60 to-[#fff6d9]/60 blur-3xl" />
            <div className="relative overflow-hidden rounded-[32px] border border-white/80 bg-white/80 px-5 pt-6 pb-8 shadow-[0_24px_60px_rgba(200,208,255,0.35)] backdrop-blur-xl md:px-8">
              <div className="flex w-full items-center justify-between font-semibold text-[#87a0ff] text-[0.6rem] uppercase tracking-[0.3em]">
                <span>Kei</span>
                <span className="flex items-center gap-2">
                  <span
                    className={`inline-flex h-2.5 w-2.5 rounded-full ${
                      isConnected ? "bg-[#7dd87d]" : "bg-[#ff9bae]"
                    }`}
                  />
                  {isConnected ? "Online" : "Waiting"}
                </span>
              </div>
              <div className="relative mt-4">
                <ClientOnlyLive2D
                  key={selectedModel.id}
                  modelPath={selectedModel.path}
                  audioTrack={remoteAudioTrack}
                  className="h-[26rem] w-full rounded-[28px] border border-white/70 bg-gradient-to-b from-white/60 to-[#f5e7ff]/40 md:h-[34rem]"
                />
                <HeartEmitter active={isAssistantSpeaking} />
              </div>
              <p className="mt-4 text-center text-[#6f6a92] text-xs md:text-sm">
                “Hi! I’m Kei. Let me know how I can make your day easier.”
              </p>
            </div>
          </div>

          <div className="flex w-full max-w-3xl flex-col items-center gap-4">
            <div className="flex flex-wrap items-center justify-center gap-2 font-medium text-[0.7rem] md:text-xs">
              <span
                className={`inline-flex items-center gap-2 rounded-full px-4 py-2 ${
                  isConnected
                    ? "bg-[#e6f8ff] text-[#236d94]"
                    : "bg-[#ffe8ef] text-[#b34f6a]"
                }`}
              >
                <span
                  className={`h-2.5 w-2.5 rounded-full ${
                    isConnected ? "bg-[#38a8d8]" : "bg-[#f0708f]"
                  }`}
                />
                {isConnected ? "Connected to channel" : "Not connected"}
              </span>
              <span
                className={`inline-flex items-center gap-2 rounded-full px-4 py-2 ${
                  isMuted
                    ? "bg-[#ffe8ef] text-[#b34f6a]"
                    : "bg-[#ecfce1] text-[#2f7d3e]"
                }`}
              >
                <span
                  className={`h-2.5 w-2.5 rounded-full ${
                    isMuted ? "bg-[#f0708f]" : "bg-[#4cc073]"
                  }`}
                />
                {isMuted ? "Mic muted" : "Mic open"}
              </span>
            </div>

            <div className="flex items-center justify-center gap-4">
              <button
                onClick={handleMicToggle}
                disabled={!isConnected}
                className={`relative flex h-14 w-14 items-center justify-center rounded-2xl border text-lg shadow-lg transition-all duration-200 ${
                  !isConnected
                    ? "cursor-not-allowed border-[#e9e7f7] bg-white text-[#b7b4c9] opacity-60"
                    : isMuted
                      ? "border-[#ffcfe0] bg-[#ffe7f0] text-[#b44f6c] hover:bg-[#ffd9e8]"
                      : "border-[#cde5ff] bg-[#e7f3ff] text-[#2f63a1] hover:bg-[#d8ecff]"
                }`}
              >
                {isMuted ? (
                  <svg
                    className="h-6 w-6"
                    fill="currentColor"
                    viewBox="0 0 24 24"
                  >
                    <path d="M12 14c1.66 0 3-1.34 3-3V5c0-1.66-1.34-3-3-3S9 3.34 9 5v6c0 1.66 1.34 3 3 3z" />
                    <path d="M17 11c0 2.76-2.24 5-5 5s-5-2.24-5-5H5c0 3.53 2.61 6.43 6 6.92V21h2v-3.08c3.39-.49 6-3.39 6-6.92h-2z" />
                    <path
                      d="M3 3l18 18"
                      stroke="currentColor"
                      strokeLinecap="round"
                      strokeWidth="2"
                    />
                  </svg>
                ) : (
                  <svg
                    className="h-6 w-6"
                    fill="currentColor"
                    viewBox="0 0 24 24"
                  >
                    <path d="M12 14c1.66 0 3-1.34 3-3V5c0-1.66-1.34-3-3-3S9 3.34 9 5v6c0 1.66 1.34 3 3 3z" />
                    <path d="M17 11c0 2.76-2.24 5-5 5s-5-2.24-5-5H5c0 3.53 2.61 6.43 6 6.92V21h2v-3.08c3.39-.49 6-3.39 6-6.92h-2z" />
                  </svg>
                )}
              </button>

              <button
                onClick={handleConnectToggle}
                disabled={isConnecting}
                className={`relative flex h-14 w-60 items-center justify-center gap-2 rounded-2xl border px-6 text-center font-semibold text-sm leading-tight shadow-lg transition-all duration-200 ${
                  isConnecting
                    ? "cursor-progress border-[#cde5ff] bg-[#e7f3ff] text-[#5a6a96]"
                    : isConnected
                      ? "border-[#ffcfe0] bg-[#ffe6f3] text-[#b44f6c] hover:bg-[#ffd9eb]"
                      : "border-[#cbeec4] bg-[#e7f8df] text-[#2f7036] hover:bg-[#def6d2]"
                }`}
              >
                {isConnecting ? (
                  <>
                    <svg
                      className="h-4 w-4 animate-spin"
                      fill="none"
                      viewBox="0 0 24 24"
                    >
                      <circle
                        className="opacity-25"
                        cx="12"
                        cy="12"
                        r="10"
                        stroke="currentColor"
                        strokeWidth="4"
                      />
                      <path
                        className="opacity-75"
                        fill="currentColor"
                        d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"
                      />
                    </svg>
                    <span className="text-center text-sm">Calling Kei...</span>
                  </>
                ) : isConnected ? (
                  <>
                    <svg
                      className="h-4 w-4"
                      fill="currentColor"
                      viewBox="0 0 24 24"
                    >
                      <rect x="6" y="6" width="12" height="12" rx="2" />
                    </svg>
                    <span className="text-center text-sm">End session</span>
                  </>
                ) : (
                  <>
                    <svg
                      className="h-4 w-4"
                      fill="currentColor"
                      viewBox="0 0 24 24"
                    >
                      <path d="M8 5v14l11-7z" />
                    </svg>
                    <span className="text-center text-sm">
                      Connect with Kei
                    </span>
                  </>
                )}
              </button>
            </div>
          </div>
        </main>
      </div>
    </div>
  );
}
