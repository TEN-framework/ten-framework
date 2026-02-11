"use client";

import type { IMicrophoneAudioTrack } from "agora-rtc-sdk-ng";
import * as React from "react";
import { useAppDispatch, useAppSelector } from "@/common";
import MicrophoneBlock from "@/components/Agent/Microphone";
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
  const { userId, channel } = options;
  const [audioTrack, setAudioTrack] = React.useState<IMicrophoneAudioTrack>();

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
    const { audioTrack } = tracks;
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

  return (
    <div className={cn("flex h-full min-h-0 flex-col", className)}>
      {/* Bottom region for microphone block - always visible */}
      <div className="w-full flex-shrink-0 space-y-2 px-2 py-2">
        <MicrophoneBlock audioTrack={audioTrack} />
      </div>
    </div>
  );
}
