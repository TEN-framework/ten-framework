"use client";

import type {
  ICameraVideoTrack,
  ILocalVideoTrack,
  IMicrophoneAudioTrack,
} from "agora-rtc-sdk-ng";
import * as React from "react";
import { VideoSourceType } from "@/common/constant";
import { useAppDispatch, useAppSelector } from "@/common/hooks";
import VideoBlock from "@/components/Agent/Camera";
import MicrophoneBlock from "@/components/Agent/Microphone";
import AgentView from "@/components/Agent/View";
import AgentVoicePresetSelect from "@/components/Agent/VoicePresetSelect";
import { cn } from "@/lib/utils";
import { type IRtcUser, type IUserTracks, rtcManager } from "@/manager";
import {
  addChatItem,
  setOptions,
  setRoomConnected,
  setVoiceType,
} from "@/store/reducers/global";
import type { IChatItem } from "@/types";

export default function RTCCard(props: { className?: string }) {
  const { className } = props;

  const dispatch = useAppDispatch();
  const options = useAppSelector((state) => state.global.options);
  const _voiceType = useAppSelector((state) => state.global.voiceType);
  const _selectedGraphId = useAppSelector((state) => state.global.graphName);
  const { userId, channel } = options;
  const [videoTrack, setVideoTrack] = React.useState<ICameraVideoTrack>();
  const [audioTrack, setAudioTrack] = React.useState<IMicrophoneAudioTrack>();
  const [screenTrack, setScreenTrack] = React.useState<ILocalVideoTrack>();
  const [remoteuser, setRemoteUser] = React.useState<IRtcUser>();
  const [videoSourceType, setVideoSourceType] = React.useState<VideoSourceType>(
    VideoSourceType.CAMERA
  );
  const hasInitRef = React.useRef(false);
  const isInitializingRef = React.useRef(false);

  const onRemoteUserChanged = (user: IRtcUser) => {
    console.log("[rtc] onRemoteUserChanged", user);
    setRemoteUser(user);
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

  const destory = React.useCallback(async () => {
    console.log("[rtc] destory");
    rtcManager.off("textChanged", onTextChanged);
    rtcManager.off("localTracksChanged", onLocalTracksChanged);
    rtcManager.off("remoteUserChanged", onRemoteUserChanged);
    await rtcManager.destroy();
    dispatch(setRoomConnected(false));
    hasInitRef.current = false;
    isInitializingRef.current = false;
  }, [dispatch]);

  const init = React.useCallback(async () => {
    if (hasInitRef.current || isInitializingRef.current) {
      return;
    }

    console.log("[rtc] init");
    isInitializingRef.current = true;
    rtcManager.on("localTracksChanged", onLocalTracksChanged);
    rtcManager.on("textChanged", onTextChanged);
    rtcManager.on("remoteUserChanged", onRemoteUserChanged);

    try {
      await rtcManager.createCameraTracks();
      await rtcManager.createMicrophoneTracks();
      await rtcManager.join({
        channel,
        userId,
      });
      dispatch(
        setOptions({
          appId: rtcManager.appId ?? "",
          token: rtcManager.token ?? "",
        })
      );
      await rtcManager.publish();
      dispatch(setRoomConnected(true));
      hasInitRef.current = true;
    } catch (err) {
      console.error("[rtc] init failed", err);
      await destory();
    } finally {
      isInitializingRef.current = false;
    }
  }, [channel, userId, dispatch, destory]);

  React.useEffect(() => {
    if (!channel) {
      return;
    }
    void init();

    return () => {
      if (hasInitRef.current || isInitializingRef.current) {
        void destory();
      }
    };
  }, [channel, init, destory]);

  const _onVoiceChange = (value: any) => {
    dispatch(setVoiceType(value));
  };

  const onVideoSourceTypeChange = async (value: VideoSourceType) => {
    await rtcManager.switchVideoSource(value);
    setVideoSourceType(value);
  };

  return (
    <div className={cn("flex-shrink-0", "overflow-y-auto", className)}>
      <div className="flex h-full w-full flex-col">
        {/* -- Agent */}
        <div className="w-full">
          <div className="flex w-full items-center justify-between p-2">
            <h2 className="mb-2 font-semibold text-xl">Audio & Video</h2>
            <AgentVoicePresetSelect />
          </div>
          <AgentView audioTrack={remoteuser?.audioTrack} />
        </div>

        {/* -- You */}
        <div className="w-full space-y-2 px-2">
          <MicrophoneBlock audioTrack={audioTrack} />
          <VideoBlock
            cameraTrack={videoTrack}
            screenTrack={screenTrack}
            videoSourceType={videoSourceType}
            onVideoSourceChange={onVideoSourceTypeChange}
          />
        </div>
      </div>
    </div>
  );
}
