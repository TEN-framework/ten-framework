"use client";

import dynamic from "next/dynamic";
import React from "react";
import { toast } from "sonner";
import {
  apiStartService,
  apiStopService,
  EMobileActiveTab,
  useAppDispatch,
  useAppSelector,
  useIsCompactLayout,
} from "@/common";
import AvatarLargeWindow, {
  type AvatarMode,
} from "@/components/Agent/AvatarLargeWindow";
import AuthInitializer from "@/components/authInitializer";
import Action from "@/components/Layout/Action";
import Header from "@/components/Layout/Header";
import { cn } from "@/lib/utils";
import { type IRtcUser } from "@/manager";
import { type TransferRequest } from "@/manager/rtc/types";
import { setAgentConnected, setSelectedGraphId } from "@/store/reducers/global";

const DynamicRTCCard = dynamic(() => import("@/components/Dynamic/RTCCard"), {
  ssr: false,
});
const DynamicChatCard = dynamic(() => import("@/components/Chat/ChatCard"), {
  ssr: false,
});

// Target graph for transfer
const OCTOPUS_GRAPH_NAME = "flux_octopus_gpt_5_1_elevenlabs";

export default function Home() {
  const dispatch = useAppDispatch();
  const mobileActiveTab = useAppSelector(
    (state) => state.global.mobileActiveTab
  );
  const trulienceSettings = useAppSelector(
    (state) => state.global.trulienceSettings
  );
  const agentConnected = useAppSelector((state) => state.global.agentConnected);
  const selectedGraphId = useAppSelector(
    (state) => state.global.selectedGraphId
  );
  const graphList = useAppSelector((state) => state.global.graphList);
  const channel = useAppSelector((state) => state.global.options.channel);
  const userId = useAppSelector((state) => state.global.options.userId);
  const language = useAppSelector((state) => state.global.language);
  const voiceType = useAppSelector((state) => state.global.voiceType);

  const isCompactLayout = useIsCompactLayout();
  const avatarInLargeWindow = trulienceSettings.avatarDesktopLargeWindow;

  const [remoteuser, setRemoteUser] = React.useState<IRtcUser>();
  const [avatarMode, setAvatarMode] = React.useState<AvatarMode>("blank");
  const [isTransferring, setIsTransferring] = React.useState(false);

  // Determine current graph name
  const currentGraphName = React.useMemo(() => {
    const graph = graphList.find((g) => g.graph_id === selectedGraphId);
    return graph?.name || "";
  }, [graphList, selectedGraphId]);

  // Update avatar mode based on connection state and graph
  React.useEffect(() => {
    // Don't change mode while transferring
    if (isTransferring) {
      setAvatarMode("transferring");
      return;
    }
    if (!agentConnected) {
      setAvatarMode("blank");
    } else if (currentGraphName === OCTOPUS_GRAPH_NAME) {
      setAvatarMode("trulience");
    } else if (remoteuser?.videoTrack) {
      // Has video track = Anam avatar
      setAvatarMode("anam");
    } else {
      // Connected but no video yet, stay blank until video arrives
      setAvatarMode("blank");
    }
  }, [agentConnected, currentGraphName, remoteuser?.videoTrack, isTransferring]);

  // Listen for remote user changes and transfer requests
  React.useEffect(() => {
    const { rtcManager } = require("../manager/rtc/rtc");

    const onRemoteUserChanged = (user: IRtcUser) => {
      console.log("[Page] onRemoteUserChanged", user, "avatarMode:", avatarMode);
      // Don't stop audio - Trulience needs the track to be active
      // It will handle playback through lip-sync
      setRemoteUser(user);
    };

    const onTransferRequested = async (request: TransferRequest) => {
      console.log("[Page] *** TRANSFER REQUESTED ***", request);
      if (isTransferring) {
        console.log("[Page] Already transferring, ignoring");
        return;
      }

      setIsTransferring(true);
      toast.info("Transferring to therapist...");

      try {
        // 1. Stop current graph
        console.log("[Page] Stopping current graph...");
        await apiStopService(channel);
        dispatch(setAgentConnected(false));

        // 2. Find octopus graph and select it
        const octopusGraph = graphList.find(
          (g) => g.name === request.targetGraph
        );
        if (!octopusGraph) {
          throw new Error(`Graph ${request.targetGraph} not found`);
        }
        dispatch(setSelectedGraphId(octopusGraph.graph_id));

        // 3. Small delay to let things settle
        await new Promise((resolve) => setTimeout(resolve, 500));

        // 4. Start new graph
        console.log("[Page] Starting octopus graph...");
        const res = await apiStartService({
          channel,
          userId,
          graphName: request.targetGraph,
          language,
          voiceType,
        });

        if (res?.code != 0) {
          // code is string "0" on success
          throw new Error(res?.msg || "Failed to start graph");
        }

        dispatch(setAgentConnected(true));
        toast.success("Connected to therapist");
        // Note: Don't stop audio track - Trulience needs it active for lip-sync
      } catch (error: any) {
        console.error("[Page] Transfer failed:", error);
        toast.error(`Transfer failed: ${error.message}`);
      } finally {
        setIsTransferring(false);
      }
    };

    rtcManager.on("remoteUserChanged", onRemoteUserChanged);
    rtcManager.on("transferRequested", onTransferRequested);

    return () => {
      rtcManager.off("remoteUserChanged", onRemoteUserChanged);
      rtcManager.off("transferRequested", onTransferRequested);
    };
  }, [
    avatarMode,
    isTransferring,
    channel,
    userId,
    language,
    voiceType,
    graphList,
    dispatch,
    remoteuser?.audioTrack,
  ]);

  return (
    <AuthInitializer>
      <div className="relative mx-auto flex min-h-screen flex-1 flex-col md:h-screen">
        <Header className="h-[60px]" />
        <Action />
        <div
          className={cn(
            "mx-2 mb-2 flex h-full max-h-[calc(100vh-108px-24px)] flex-1 flex-col md:flex-row md:gap-2",
            {
              ["flex-col-reverse"]: avatarInLargeWindow && isCompactLayout,
            }
          )}
        >
          <DynamicRTCCard
            className={cn(
              "m-0 flex w-full flex-1 rounded-b-lg bg-[#181a1d] md:w-[480px] md:rounded-lg",
              {
                ["hidden md:flex"]: mobileActiveTab === EMobileActiveTab.CHAT,
              }
            )}
          />

          {/* Show chat when avatar is NOT in large window */}
          {(!avatarInLargeWindow || isCompactLayout) && (
            <DynamicChatCard
              className={cn(
                "m-0 w-full flex-auto rounded-b-lg bg-[#181a1d] md:rounded-lg",
                {
                  ["hidden md:flex"]:
                    mobileActiveTab === EMobileActiveTab.AGENT,
                }
              )}
            />
          )}

          {/* Unified Avatar Large Window */}
          {avatarInLargeWindow && (
            <div
              className={cn("w-full", {
                ["h-60 flex-auto bg-[#181a1d] p-1"]: isCompactLayout,
                ["hidden md:block"]: mobileActiveTab === EMobileActiveTab.CHAT,
              })}
            >
              <AvatarLargeWindow
                mode={avatarMode}
                audioTrack={remoteuser?.audioTrack}
                videoTrack={remoteuser?.videoTrack}
              />
            </div>
          )}
        </div>
      </div>
    </AuthInitializer>
  );
}
