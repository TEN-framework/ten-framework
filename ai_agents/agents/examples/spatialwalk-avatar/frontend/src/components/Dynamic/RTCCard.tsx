"use client";

import type {
  ICameraVideoTrack,
  ILocalVideoTrack,
  IMicrophoneAudioTrack,
} from "agora-rtc-sdk-ng";
import * as React from "react";
import {
  useAppDispatch,
  useAppSelector,
  useIsCompactLayout,
  VideoSourceType,
} from "@/common";
import Avatar from "@/components/Agent/AvatarSpatialwalk";
import VideoBlock from "@/components/Agent/Camera";
import MicrophoneBlock from "@/components/Agent/Microphone";
import ChatCard from "@/components/Chat/ChatCard";
import { cn } from "@/lib/utils";
import {
  type IRtcUser,
  type IUiActionPayload,
  type IUserTracks,
  rtcManager,
} from "@/manager";
import {
  addChatItem,
  setOptions,
  setRoomConnected,
  showFortuneModal,
  triggerFestivalEffect,
} from "@/store/reducers/global";
import { type IChatItem } from "@/types";

let hasInit: boolean = false;

export default function RTCCard(props: { className?: string }) {
  const { className } = props;

  const dispatch = useAppDispatch();
  const options = useAppSelector((state) => state.global.options);
  const spatialwalkSettings = useAppSelector(
    (state) => state.global.spatialwalkSettings
  );
  const { userId, channel } = options;
  const [videoTrack, setVideoTrack] = React.useState<ICameraVideoTrack>();
  const [audioTrack, setAudioTrack] = React.useState<IMicrophoneAudioTrack>();
  const [screenTrack, setScreenTrack] = React.useState<ILocalVideoTrack>();
  const [videoSourceType, setVideoSourceType] = React.useState<VideoSourceType>(
    VideoSourceType.CAMERA
  );
  const avatarInLargeWindow = spatialwalkSettings.avatarDesktopLargeWindow;

  const isCompactLayout = useIsCompactLayout();

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
  }, [options.channel]);

  const init = async () => {
    try {
      console.log("[rtc] init");
      rtcManager.on("localTracksChanged", onLocalTracksChanged);
      rtcManager.on("textChanged", onTextChanged);
      rtcManager.on("remoteUserChanged", onRemoteUserChanged);
      rtcManager.on("uiAction", onUiAction);
      await rtcManager.prepareSession({
        channel,
        userId,
        enableSpatialwalk: true,
      });
      dispatch(
        setOptions({
          ...options,
          appId: rtcManager.appId ?? "",
          token: rtcManager.token ?? "",
          spatialwalkToken: rtcManager.spatialwalkToken ?? "",
        })
      );
      await rtcManager.waitForNativeClient(10000);
      await rtcManager.createCameraTracks();
      await rtcManager.createMicrophoneAudioTrack();
      await rtcManager.publish();
      dispatch(setRoomConnected(true));
      hasInit = true;
    } catch (error) {
      console.error("[rtc] init failed", error);
      dispatch(setRoomConnected(false));
      rtcManager.off("textChanged", onTextChanged);
      rtcManager.off("localTracksChanged", onLocalTracksChanged);
      rtcManager.off("remoteUserChanged", onRemoteUserChanged);
      rtcManager.off("uiAction", onUiAction);
      hasInit = false;
    }
  };

  const destory = async () => {
    console.log("[rtc] destory");
    rtcManager.off("textChanged", onTextChanged);
    rtcManager.off("localTracksChanged", onLocalTracksChanged);
    rtcManager.off("remoteUserChanged", onRemoteUserChanged);
    rtcManager.off("uiAction", onUiAction);
    await rtcManager.destroy();
    dispatch(setRoomConnected(false));
    hasInit = false;
  };

  const onRemoteUserChanged = (user: IRtcUser) => {
    console.log("[rtc] onRemoteUserChanged", user);
    // Spatialwalk SDK will play audio in sync with animation
    user.audioTrack?.stop();
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

  const onTextChanged = (text: IChatItem) => {
    console.log("[rtc] onTextChanged", text);
    dispatch(addChatItem(text));
  };

  const onUiAction = (payload: IUiActionPayload) => {
    console.log("[rtc] onUiAction", payload);
    if (payload.action === "trigger_effect") {
      const effectName = payload.data?.name;
      if (effectName === "gold_rain" || effectName === "fireworks") {
        dispatch(triggerFestivalEffect({ name: effectName }));
      }
      return;
    }
    if (payload.action === "show_fortune_result") {
      const imageId = payload.data?.image_id;
      if (typeof imageId === "string" && imageId.trim().length > 0) {
        dispatch(showFortuneModal({ imageId }));
      }
    }
  };

  const onVideoSourceTypeChange = async (value: VideoSourceType) => {
    await rtcManager.switchVideoSource(value);
    setVideoSourceType(value);
  };

  return (
    <div className={cn("flex h-full min-h-0 flex-col", className)}>
      {/* Top region (Avatar or ChatCard) */}
      <div className="z-10 min-h-0 flex-1 overflow-y-auto" style={{ minHeight: '240px' }}>
        {!avatarInLargeWindow ? (
          <div className="h-60 w-full p-1">
            <Avatar />
          </div>
        ) : (
          !isCompactLayout && (
            <ChatCard className="m-0 h-full w-full rounded-b-lg bg-[#181a1d] md:rounded-lg" />
          )
        )}
      </div>

      {/* Bottom region for microphone and video blocks - always visible */}
      <div className="w-full flex-shrink-0 space-y-2 px-2 py-2">
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
