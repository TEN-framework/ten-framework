"use client";

import type { NetworkQuality } from "agora-rtc-sdk-ng";
// IRemoteAudioTrack will be imported dynamically
import { Activity, Mic, MicOff, Wifi, WifiOff } from "lucide-react";
import { useEffect, useState } from "react";
import { Button } from "@/components/ui/button";
import type { AgoraConfig, ConnectionStatus } from "@/types";

interface ConnectionPanelProps {
  onConnectionChange?: (connected: boolean) => void;
  onAudioTrackChange?: (track: any) => void;
}

export default function ConnectionPanel({
  onConnectionChange,
  onAudioTrackChange,
}: ConnectionPanelProps) {
  const [config, setConfig] = useState<AgoraConfig>({
    appId: "",
    channel: "live2d-voice-assistant",
    token: "",
    uid: Math.floor(Math.random() * 100000),
  });

  const [connectionStatus, setConnectionStatus] = useState<ConnectionStatus>({
    rtc: "disconnected",
    rtm: "disconnected",
    agent: "stopped",
  });

  const [isConnecting, setIsConnecting] = useState(false);
  const [isMicEnabled, setIsMicEnabled] = useState(true);
  const [_networkQuality, setNetworkQuality] = useState(0);
  const [agoraService, setAgoraService] = useState<any>(null);

  useEffect(() => {
    // Dynamically import Agora service only on client side
    if (typeof window !== "undefined") {
      import("@/services/agora").then((module) => {
        setAgoraService(module.agoraService);
      });
    }
  }, []);

  useEffect(() => {
    if (!agoraService) return;

    // Set up event listeners
    agoraService.setOnConnectionStatusChange(setConnectionStatus);
    if (onAudioTrackChange) {
      agoraService.setOnRemoteAudioTrack(onAudioTrackChange);
    }
    agoraService.setOnNetworkQuality((quality: NetworkQuality) => {
      setNetworkQuality(quality.uplinkNetworkQuality);
    });

    return () => {
      // Cleanup
      agoraService.disconnect();
    };
  }, [agoraService, onAudioTrackChange]);

  const handleConnect = async () => {
    if (isConnecting || !agoraService) return;

    setIsConnecting(true);
    try {
      // First, fetch Agora credentials from the server
      const credentials = await agoraService.fetchCredentials(
        config.channel,
        config.uid
      );
      if (!credentials) {
        console.error("Failed to fetch Agora credentials");
        setIsConnecting(false);
        return;
      }

      // Update config with fetched credentials
      setConfig(credentials);

      // Connect with the fetched credentials
      const success = await agoraService.connect(credentials);
      if (success) {
        onConnectionChange?.(true);
      }
    } catch (error) {
      console.error("Connection failed:", error);
    } finally {
      setIsConnecting(false);
    }
  };

  const handleDisconnect = async () => {
    if (agoraService) {
      await agoraService.disconnect();
    }
    onConnectionChange?.(false);
  };

  const toggleMic = () => {
    if (!agoraService) return;
    const localTrack = agoraService.getLocalAudioTrack();
    if (localTrack) {
      if (isMicEnabled) {
        localTrack.setEnabled(false);
      } else {
        localTrack.setEnabled(true);
      }
      setIsMicEnabled(!isMicEnabled);
    }
  };

  const getStatusColor = (status: string) => {
    switch (status) {
      case "connected":
      case "running":
        return "text-green-500";
      case "connecting":
        return "text-yellow-500";
      case "disconnected":
      case "stopped":
        return "text-gray-500";
      case "error":
        return "text-red-500";
      default:
        return "text-gray-500";
    }
  };

  const getStatusIcon = (status: string) => {
    switch (status) {
      case "connected":
      case "running":
        return <Wifi className="h-4 w-4" />;
      case "connecting":
        return <Activity className="h-4 w-4 animate-pulse" />;
      default:
        return <WifiOff className="h-4 w-4" />;
    }
  };

  const isConnected =
    agoraService &&
    connectionStatus.rtc === "connected" &&
    connectionStatus.rtm === "connected";

  return (
    <div className="space-y-2">
      {/* Main Controls */}
      <div className="flex gap-1">
        <Button
          onClick={isConnected ? handleDisconnect : handleConnect}
          disabled={isConnecting}
          className="flex-1"
        >
          {isConnecting ? (
            <>
              <Activity className="mr-2 h-4 w-4 animate-spin" />
              Connecting...
            </>
          ) : isConnected ? (
            <>
              <WifiOff className="mr-2 h-4 w-4" />
              Disconnect
            </>
          ) : (
            <>
              <Wifi className="mr-2 h-4 w-4" />
              Connect
            </>
          )}
        </Button>

        {isConnected && (
          <Button
            onClick={toggleMic}
            variant={isMicEnabled ? "default" : "destructive"}
            size="icon"
          >
            {isMicEnabled ? (
              <Mic className="h-4 w-4" />
            ) : (
              <MicOff className="h-4 w-4" />
            )}
          </Button>
        )}
      </div>

      {/* Status */}
      <div className="flex items-center justify-between text-sm">
        <span className="text-muted-foreground">Status:</span>
        <div
          className={`flex items-center gap-1 ${getStatusColor(connectionStatus.rtc)}`}
        >
          {getStatusIcon(connectionStatus.rtc)}
          <span className="text-xs capitalize">{connectionStatus.rtc}</span>
        </div>
      </div>
    </div>
  );
}
