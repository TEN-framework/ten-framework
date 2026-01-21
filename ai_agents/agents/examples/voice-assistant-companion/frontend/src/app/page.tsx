"use client";

import { useEffect, useState } from "react";
export const dynamic = "force-dynamic";

import dynamicImport from "next/dynamic";
import { Inter, Poppins } from "next/font/google";

const ClientOnlyLive2D = dynamicImport(
  () => import("@/components/ClientOnlyLive2D"),
  {
    ssr: false,
    loading: () => (
      <div className="flex h-full items-center justify-center">
        <div className="text-center">
          <div className="mx-auto mb-4 h-12 w-12 animate-spin rounded-full border-gray-300 border-b-2"></div>
          <p className="text-gray-500">Loading Companion...</p>
        </div>
      </div>
    ),
  }
);

import { apiPing, apiStartService, apiStopService } from "@/lib/request";
import type { AgoraConfig, Live2DModel } from "@/types";

const defaultModel: Live2DModel = {
  id: "memU",
  name: "AI Companion",
  path: "/models/memU/memu_cat.model3.json",
  preview: "/models/memU/preview.png",
};

const titleFont = Inter({
  subsets: ["latin"],
  weight: ["600", "700"],
});

const bodyFont = Poppins({
  subsets: ["latin"],
  weight: ["400", "500", "600"],
});

export default function Home() {
  const [isConnected, setIsConnected] = useState(false);
  const [isMuted, setIsMuted] = useState(false);
  const [isConnecting, setIsConnecting] = useState(false);
  const [selectedModel, _setSelectedModel] =
    useState<Live2DModel>(defaultModel);
  const [remoteAudioTrack, setRemoteAudioTrack] = useState<any>(null);
  const [agoraService, setAgoraService] = useState<any>(null);
  const [pingInterval, setPingInterval] = useState<NodeJS.Timeout | null>(null);
  const [isAssistantSpeaking, setIsAssistantSpeaking] = useState(false);
  const [callDuration, setCallDuration] = useState(0);

  useEffect(() => {
    let interval: NodeJS.Timeout | null = null;
    if (isConnected) {
      interval = setInterval(() => {
        setCallDuration((prev) => prev + 1);
      }, 1000);
    } else {
      setCallDuration(0);
    }

    return () => {
      if (interval) clearInterval(interval);
    };
  }, [isConnected]);

  const formatDuration = (seconds: number) => {
    const mins = Math.floor(seconds / 60);
    const secs = seconds % 60;
    return `${mins.toString().padStart(2, "0")}:${secs.toString().padStart(2, "0")}`;
  };

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
      if (interval) clearInterval(interval);
    };
  }, [remoteAudioTrack]);

  const startPing = () => {
    if (pingInterval) stopPing();
    const interval = setInterval(() => {
      apiPing("companion_channel");
    }, 3000);
    setPingInterval(interval);
  };

  const stopPing = () => {
    if (pingInterval) {
      clearInterval(pingInterval);
      setPingInterval(null);
    }
  };

  useEffect(() => {
    if (typeof window !== "undefined") {
      import("@/services/agora").then((module) => {
        const service = module.agoraService;
        setAgoraService(service);
        service.setOnConnectionStatusChange(handleConnectionChange);
        service.setOnRemoteAudioTrack(handleAudioTrackChange);
      });
    }

    return () => {
      stopPing();
    };
  }, [handleAudioTrackChange, handleConnectionChange, stopPing]);

  const _handleMicToggle = () => {
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
          try {
            // Disconnect RTC first to trigger UserLeftEvent
            await agoraService.disconnect();

            // Wait a bit to let UserLeftEvent handler save the conversation
            console.log("Waiting for memory save...");
            await new Promise((resolve) => setTimeout(resolve, 2000)); // 2 seconds delay

            // Then stop the companion service
            await apiStopService("companion_channel");
            console.log("Companion stopped");
          } catch (error) {
            console.error("Failed to stop companion:", error);
          }

          setIsConnected(false);
          stopPing();
          setIsConnecting(false);
        } else {
          setIsConnecting(true);
          // Use relative path - will be rewritten by Next.js middleware
          const response = await fetch(`/api/token/generate`, {
            method: "POST",
            headers: {
              "Content-Type": "application/json",
            },
            body: JSON.stringify({
              request_id: Math.random().toString(36).substring(2, 15),
              uid: Math.floor(Math.random() * 100000),
              channel_name: "companion_channel",
            }),
          });

          if (!response.ok) {
            throw new Error(
              `Failed to get Agora credentials: ${response.statusText}`
            );
          }

          const responseData = await response.json();
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
            setIsMuted(agoraService.isMicrophoneMuted());

            try {
              const startResult = await apiStartService({
                channel: agoraConfig.channel,
                userId: agoraConfig.uid || 0,
                graphName: "voice_assistant_companion",
                language: "en",
                voiceType: "female",
              });

              console.log("Companion started:", startResult);
              startPing();
            } catch (error) {
              console.error("Failed to start companion:", error);
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
    <div className="relative min-h-[100svh] overflow-hidden bg-white">
      {/* Subtle background pattern */}
      <div className="absolute inset-0 opacity-30">
        <div className="absolute inset-0 bg-[radial-gradient(circle_at_30%_20%,rgba(99,102,241,0.1),transparent_50%)]"></div>
        <div className="absolute inset-0 bg-[radial-gradient(circle_at_70%_80%,rgba(147,51,234,0.1),transparent_50%)]"></div>
      </div>

      {/* Main content */}
      <div className="relative z-10 flex min-h-[100svh] flex-col items-center justify-center gap-6 px-4 py-8">
        {/* Phone call header */}
        <div className="w-full max-w-md">
          <div className="mb-8 space-y-2 text-center">
            <div
              className={`${titleFont.className} font-semibold text-gray-800 text-xl tracking-tight`}
            >
              {isConnected ? "Call in Progress" : "AI Companion"}
            </div>
            {isConnected && (
              <div className={`${bodyFont.className} text-gray-500 text-sm`}>
                {formatDuration(callDuration)}
              </div>
            )}
          </div>

          {/* Live2D Character - Phone style */}
          <div className="relative mb-8">
            <div className="-inset-8 absolute rounded-full bg-gradient-to-r from-purple-100 via-indigo-100 to-blue-100 blur-3xl"></div>

            <div className="relative">
              {/* Status indicator ring */}
              {isConnected && (
                <div className="absolute inset-0 animate-ping rounded-full border-4 border-green-400/50"></div>
              )}

              {/* Main character container */}
              <div
                className={`relative overflow-hidden rounded-full shadow-2xl transition-all duration-300 ${
                  isAssistantSpeaking
                    ? "ring-4 ring-green-400 ring-offset-4 ring-offset-white"
                    : "ring-2 ring-gray-200"
                }`}
              >
                <div className="aspect-square bg-gradient-to-br from-purple-50 to-blue-50">
                  <ClientOnlyLive2D
                    key={selectedModel.id}
                    modelPath={selectedModel.path}
                    audioTrack={remoteAudioTrack}
                    className="h-full w-full"
                  />
                </div>
              </div>

              {/* Ripple effect when speaking */}
              {isAssistantSpeaking && (
                <div className="absolute inset-0 animate-pulse rounded-full border-4 border-green-400/30"></div>
              )}
            </div>
          </div>

          {/* Status text */}
          <div
            className={`${bodyFont.className} mb-12 text-center text-gray-600 text-sm`}
          >
            {isConnecting ? (
              "Connecting..."
            ) : isConnected ? (
              <span className="flex items-center justify-center gap-2">
                <span className="h-2 w-2 animate-pulse rounded-full bg-green-500"></span>
                "Hi! I'm here to chat, listen, and help."
              </span>
            ) : (
              "Tap the call button to start"
            )}
          </div>

          {/* Call/Hang up button */}
          <div className="flex flex-col items-center gap-4">
            <button
              onClick={handleConnectToggle}
              disabled={isConnecting}
              className={`group relative transition-all duration-300 ${
                isConnecting ? "cursor-wait" : "cursor-pointer"
              }`}
            >
              {/* Button glow effect */}
              <div
                className={`absolute inset-0 rounded-full blur-xl transition-all duration-300 ${
                  isConnecting
                    ? "bg-gray-400/30"
                    : isConnected
                      ? "bg-red-500/40 group-hover:bg-red-500/60"
                      : "bg-green-500/40 group-hover:bg-green-500/60"
                }`}
              ></div>

              {/* Main button */}
              <div
                className={`relative flex h-24 w-24 items-center justify-center rounded-full shadow-2xl transition-all duration-300 ${
                  isConnecting
                    ? "scale-95 bg-gradient-to-br from-gray-400 to-gray-500"
                    : isConnected
                      ? "bg-gradient-to-br from-red-500 to-red-600 group-hover:scale-105 group-hover:shadow-red-500/50"
                      : "bg-gradient-to-br from-green-500 to-green-600 group-hover:scale-105 group-hover:shadow-green-500/50"
                } ${isConnecting ? "" : "group-active:scale-95"}`}
              >
                {isConnecting ? (
                  <svg
                    className="h-10 w-10 animate-spin text-white"
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
                ) : isConnected ? (
                  // Hang up icon
                  <svg
                    className="h-10 w-10 rotate-135 transform text-white"
                    fill="currentColor"
                    viewBox="0 0 24 24"
                  >
                    <path d="M20.01 15.38c-1.23 0-2.42-.2-3.53-.56-.35-.12-.74-.03-1.01.24l-1.57 1.97c-2.83-1.35-5.48-3.9-6.89-6.83l1.95-1.66c.27-.28.35-.67.24-1.02-.37-1.11-.56-2.3-.56-3.53 0-.54-.45-.99-.99-.99H4.19C3.65 3 3 3.24 3 3.99 3 13.28 10.73 21 20.01 21c.71 0 .99-.63.99-1.18v-3.45c0-.54-.45-.99-.99-.99z" />
                  </svg>
                ) : (
                  // Call icon
                  <svg
                    className="h-10 w-10 text-white"
                    fill="currentColor"
                    viewBox="0 0 24 24"
                  >
                    <path d="M20.01 15.38c-1.23 0-2.42-.2-3.53-.56-.35-.12-.74-.03-1.01.24l-1.57 1.97c-2.83-1.35-5.48-3.9-6.89-6.83l1.95-1.66c.27-.28.35-.67.24-1.02-.37-1.11-.56-2.3-.56-3.53 0-.54-.45-.99-.99-.99H4.19C3.65 3 3 3.24 3 3.99 3 13.28 10.73 21 20.01 21c.71 0 .99-.63.99-1.18v-3.45c0-.54-.45-.99-.99-.99z" />
                  </svg>
                )}
              </div>

              {/* Inner ring animation */}
              {!isConnecting && (
                <div
                  className={`absolute inset-0 rounded-full border-2 transition-all duration-300 ${
                    isConnected
                      ? "scale-110 border-red-300/50 group-hover:scale-115"
                      : "scale-110 border-green-300/50 group-hover:scale-115"
                  }`}
                ></div>
              )}
            </button>

            {/* Button label */}
            <div className={`${bodyFont.className} text-center`}>
              <div className="font-semibold text-base text-gray-700">
                {isConnecting
                  ? "Connecting..."
                  : isConnected
                    ? "Hang Up"
                    : "Call"}
              </div>
              {isConnected && (
                <div className="mt-1 text-gray-500 text-xs">
                  Tap to end conversation
                </div>
              )}
            </div>
          </div>

          {/* Connection status */}
          {isConnected && (
            <div className="mt-8 flex items-center justify-center gap-3 text-sm">
              <div className="flex items-center gap-2 rounded-full border border-green-200 bg-green-50 px-4 py-2">
                <span className="h-2 w-2 animate-pulse rounded-full bg-green-500"></span>
                <span className="text-green-700">Connected</span>
              </div>
            </div>
          )}
        </div>

        {/* Footer */}
        <div
          className={`${bodyFont.className} mt-8 text-center text-gray-400 text-xs`}
        >
          <p>Your AI Companion is always here for you</p>
        </div>
      </div>
    </div>
  );
}
