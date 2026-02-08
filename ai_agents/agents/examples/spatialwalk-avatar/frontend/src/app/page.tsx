"use client";

import { IMicrophoneAudioTrack } from "agora-rtc-sdk-ng";
import dynamic from "next/dynamic";
import React from "react";
import { EMobileActiveTab, useAppSelector, useIsCompactLayout } from "@/common";
import Avatar from "@/components/Agent/AvatarTrulience";
import AuthInitializer from "@/components/authInitializer";
import Action from "@/components/Layout/Action";
import Header from "@/components/Layout/Header";
import { cn } from "@/lib/utils";
import { type IRtcUser, IUserTracks } from "@/manager";

const DynamicRTCCard = dynamic(() => import("@/components/Dynamic/RTCCard"), {
  ssr: false,
});
const DynamicChatCard = dynamic(() => import("@/components/Chat/ChatCard"), {
  ssr: false,
});

export default function Home() {
  const mobileActiveTab = useAppSelector(
    (state) => state.global.mobileActiveTab
  );
  const trulienceSettings = useAppSelector(
    (state) => state.global.trulienceSettings
  );

  const isCompactLayout = useIsCompactLayout();
  const useTrulienceAvatar = trulienceSettings.enabled;
  const avatarInLargeWindow = trulienceSettings.avatarDesktopLargeWindow;
  const [remoteuser, setRemoteUser] = React.useState<IRtcUser>();

  React.useEffect(() => {
    const { rtcManager } = require("../manager/rtc/rtc");
    rtcManager.on("remoteUserChanged", onRemoteUserChanged);
    return () => {
      rtcManager.off("remoteUserChanged", onRemoteUserChanged);
    };
  }, []);

  const onRemoteUserChanged = (user: IRtcUser) => {
    if (useTrulienceAvatar) {
      user.audioTrack?.stop();
    }
    if (user.audioTrack) {
      setRemoteUser(user);
    }
  };

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

          {(!useTrulienceAvatar || isCompactLayout || !avatarInLargeWindow) && (
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

          {useTrulienceAvatar && avatarInLargeWindow && (
            <div
              className={cn("w-full", {
                ["h-60 flex-auto bg-[#181a1d] p-1"]: isCompactLayout,
                ["hidden md:block"]: mobileActiveTab === EMobileActiveTab.CHAT,
              })}
            >
              <Avatar audioTrack={remoteuser?.audioTrack} />
            </div>
          )}
        </div>
      </div>
    </AuthInitializer>
  );
}
