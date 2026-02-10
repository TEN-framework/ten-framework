"use client";

import {
  AvatarManager,
  AvatarSDK,
  AvatarView,
  DrivingServiceMode,
  Environment,
} from "@spatialwalk/avatarkit";
import { AgoraProvider, AvatarPlayer } from "@spatialwalk/avatarkit-rtc";
import { Maximize, Minimize } from "lucide-react";
import React, { useEffect, useRef, useState } from "react";
import { useAppSelector } from "@/common";
import {
  getSpatialwalkUrlConfig,
  validateSpatialwalkRequiredConfig,
} from "@/common/spatialwalk";
import { cn } from "@/lib/utils";

let initKey: string | null = null;
let initPromise: Promise<void> | null = null;

const ensureSdkInitialized = async (appId: string, environment: "cn" | "intl") => {
  const nextKey = `${appId}:${environment}`;
  if (initPromise && initKey === nextKey) {
    return initPromise;
  }
  if (initKey && initKey !== nextKey) {
    AvatarSDK.cleanup();
    initPromise = null;
  }
  initKey = nextKey;
  initPromise = AvatarSDK.initialize(appId, {
    environment: environment === "cn" ? Environment.cn : Environment.intl,
    drivingServiceMode: DrivingServiceMode.host,
  });
  return initPromise;
};

export default function AvatarSpatialwalk() {
  const options = useAppSelector((state) => state.global.options);
  const spatialwalkSettings = useAppSelector(
    (state) => state.global.spatialwalkSettings
  );
  const [errorMessage, setErrorMessage] = useState<string>("");
  const [loading, setLoading] = useState<boolean>(false);
  const [fullscreen, setFullscreen] = useState(false);

  const containerRef = useRef<HTMLDivElement>(null);
  const avatarViewRef = useRef<AvatarView | null>(null);
  const playerRef = useRef<AvatarPlayer | null>(null);
  const playerConnectedRef = useRef(false);
  const lastConnectKeyRef = useRef<string | null>(null);
  const [spatialwalkUrlConfig] = useState(getSpatialwalkUrlConfig());

  useEffect(() => {
    let cancelled = false;

    const setup = async () => {
      setErrorMessage("");
      const validation = validateSpatialwalkRequiredConfig(spatialwalkUrlConfig);
      if (!validation.isValid) {
        setErrorMessage(validation.message);
        return;
      }
      if (!containerRef.current) {
        return;
      }
      setLoading(true);

      try {
        const connectKey = [
          spatialwalkUrlConfig.appId,
          spatialwalkUrlConfig.avatarId,
          spatialwalkSettings.environment,
          options.appId || "",
          options.channel || "",
          options.spatialwalkToken || "",
          "0",
        ].join("|");
        if (lastConnectKeyRef.current === connectKey && playerRef.current) {
          return;
        }
        // Keep a single live player instance. Avoid disconnect/recreate churn
        // on rerenders because SDK disconnect can race with internal render loop.
        if (playerRef.current) {
          return;
        }

        await ensureSdkInitialized(
          spatialwalkUrlConfig.appId,
          spatialwalkSettings.environment
        );
        if (cancelled) return;

        const avatar = await AvatarManager.shared.load(
          spatialwalkUrlConfig.avatarId
        );
        if (cancelled) return;

        const avatarView = new AvatarView(avatar, containerRef.current);
        avatarViewRef.current = avatarView;

        const provider = new AgoraProvider();
        // Package typings currently miss BaseProvider event methods; runtime is compatible.
        const player = new AvatarPlayer(provider as any, avatarView, {
          logLevel: "warning",
        });
        playerRef.current = player;
        playerConnectedRef.current = false;

        if (options.channel && options.appId) {
          const connectConfig = {
            appId: options.appId,
            channel: options.channel,
            token: options.spatialwalkToken || undefined,
            uid: 0,
          };
          await player.connect(connectConfig);
          playerConnectedRef.current = true;
          lastConnectKeyRef.current = connectKey;
        }
      } catch (error: any) {
        const message =
          error?.message || "Failed to initialize Spatialwalk Avatar.";
        setErrorMessage(message);
        console.error("[spatialwalk] init error:", error);
      } finally {
        if (!cancelled) {
          setLoading(false);
        }
      }
    };

    setup();

    return () => {
      cancelled = true;
      // Intentionally avoid disconnect in React cleanup to prevent SDK
      // race ("AvatarView not initialized") during passive unmount.
    };
  }, [
    options.appId,
    options.channel,
    options.spatialwalkToken,
    spatialwalkUrlConfig.appId,
    spatialwalkUrlConfig.avatarId,
    spatialwalkSettings.environment,
  ]);

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

      <div ref={containerRef} className="h-full w-full" />

      {loading && (
        <div className="absolute inset-0 z-10 flex items-center justify-center bg-black/70 text-white">
          Loading avatar...
        </div>
      )}

      {errorMessage && (
        <div className="absolute inset-0 z-10 flex items-center justify-center bg-red-500/80 text-white">
          <div>{errorMessage}</div>
        </div>
      )}
    </div>
  );
}
