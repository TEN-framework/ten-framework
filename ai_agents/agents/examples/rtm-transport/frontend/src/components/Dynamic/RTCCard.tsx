"use client";

import type {
  ICameraVideoTrack,
  ILocalVideoTrack,
  IMicrophoneAudioTrack,
} from "agora-rtc-sdk-ng";
import dynamic from "next/dynamic";
import * as React from "react";
import {
  useAppDispatch,
  useAppSelector,
  useIsCompactLayout,
  VideoSourceType,
} from "@/common";
import Avatar from "@/components/Agent/AvatarTrulience";
import VideoBlock from "@/components/Agent/Camera";
import MicrophoneBlock from "@/components/Agent/Microphone";
import AgentView from "@/components/Agent/View";
import ChatCard from "@/components/Chat/ChatCard";
import { cn } from "@/lib/utils";
import { type IRtcUser, type IUserTracks, rtcManager } from "@/manager";
import { rtmManager } from "@/manager/rtm";
import {
  setOptions,
  setRoomConnected,
  setRtmConnected,
  setVoiceType,
} from "@/store/reducers/global";

let hasInit: boolean = false;

export default function RTCCard(props: { className?: string }) {
  const { className } = props;

  const dispatch = useAppDispatch();
  const options = useAppSelector((state) => state.global.options);
  const trulienceSettings = useAppSelector(
    (state) => state.global.trulienceSettings
  );
  const { userId, channel } = options;
  const [videoTrack, setVideoTrack] = React.useState<ICameraVideoTrack>();
  const [audioTrack, setAudioTrack] = React.useState<IMicrophoneAudioTrack>();
  const [screenTrack, setScreenTrack] = React.useState<ILocalVideoTrack>();
  const [remoteuser, setRemoteUser] = React.useState<IRtcUser>();
  const [videoSourceType, setVideoSourceType] = React.useState<VideoSourceType>(
    VideoSourceType.CAMERA
  );
  const useTrulienceAvatar = trulienceSettings.enabled;
  const avatarInLargeWindow = trulienceSettings.avatarDesktopLargeWindow;

  const isCompactLayout = useIsCompactLayout();

  const _DynamicChatCard = dynamic(() => import("@/components/Chat/ChatCard"), {
    ssr: false,
  });

  const onRemoteUserChanged = (user: IRtcUser) => {
    console.log("[rtc] onRemoteUserChanged", user);
    if (useTrulienceAvatar) {
      // trulience SDK will play audio in synch with mouth
      user.audioTrack?.stop();
    }
    if (user.audioTrack) {
      setRemoteUser(user);
    }
  };

  const onLocalTracksChanged = (tracks: IUserTracks) => {
    console.log("[rtc] onLocalTracksChanged", tracks);
    const { videoTrack, audioTrack, screenTrack } = tracks;
    setVideoTrack(videoTrack);
    setScreenTrack(screenTrack);
    if (audioTrack) {
      setAudioTrack(audioTrack);
    }
  };

  const init = async () => {
    console.log("[rtc] init");
    rtcManager.on("localTracksChanged", onLocalTracksChanged);
    rtcManager.on("remoteUserChanged", onRemoteUserChanged);
    await rtcManager.createCameraTracks();
    await rtcManager.createMicrophoneAudioTrack();
    await rtcManager.join({
      channel,
      userId,
    });
    dispatch(
      setOptions({
        ...options,
        appId: rtcManager.appId ?? "",
        token: rtcManager.token ?? "",
      })
    );
    await rtcManager.publish();
    dispatch(setRoomConnected(true));

    // Initialize RTM
    try {
      console.log("[rtm] init");
      await rtmManager.init({
        channel,
        userId,
        appId: rtcManager.appId ?? "",
        token: rtcManager.token ?? "",
      });
      dispatch(setRtmConnected(true));
      console.log("[rtm] init complete");
    } catch (error) {
      console.error("[rtm] init failed:", error);
      dispatch(setRtmConnected(false));
    }

    hasInit = true;
  };

  const destory = async () => {
    console.log("[rtc] destory");
    rtcManager.off("localTracksChanged", onLocalTracksChanged);
    rtcManager.off("remoteUserChanged", onRemoteUserChanged);
    await rtcManager.destroy();
    dispatch(setRoomConnected(false));

    // Destroy RTM
    try {
      console.log("[rtm] destory");
      await rtmManager.destroy();
      dispatch(setRtmConnected(false));
    } catch (error) {
      console.error("[rtm] destory failed:", error);
    }

    hasInit = false;
  };

  React.useEffect(() => {
    if (!options.channel) {
      return;
    }
    if (hasInit) {
      return;
    }

    init();

    return () => {
      if (hasInit) {
        destory();
      }
    };
  }, [options.channel, destory, init]);

  const _onVoiceChange = (value: any) => {
    dispatch(setVoiceType(value));
  };

  const onVideoSourceTypeChange = async (value: VideoSourceType) => {
    await rtcManager.switchVideoSource(value);
    setVideoSourceType(value);
  };

  return (
    <div className={cn("flex h-full min-h-0 flex-col", className)}>
      {/* Scrollable top region (Avatar or ChatCard) */}
      <div className="z-10 min-h-0 overflow-y-auto">
        {useTrulienceAvatar ? (
          !avatarInLargeWindow ? (
            <div className="h-60 w-full p-1">
              <Avatar
                localAudioTrack={audioTrack}
                audioTrack={remoteuser?.audioTrack}
              />
            </div>
          ) : (
            !isCompactLayout && (
              <ChatCard className="m-0 h-full w-full rounded-b-lg bg-[#181a1d] md:rounded-lg" />
            )
          )
        ) : (
          <AgentView audioTrack={remoteuser?.audioTrack} />
        )}
      </div>

      {/* Bottom region for microphone and video blocks */}
      <div className="w-full space-y-2 px-2 py-2">
        <MicrophoneBlock audioTrack={audioTrack} />
        <VideoBlock
          cameraTrack={videoTrack}
          screenTrack={screenTrack}
          videoSourceType={videoSourceType}
          onVideoSourceChange={onVideoSourceTypeChange}
        />
      </div>
    </div>
  );
}
