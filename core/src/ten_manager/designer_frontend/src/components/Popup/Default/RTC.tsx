//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { useTranslation } from "react-i18next";
import AgoraRTC from "agora-rtc-react";
import { 
  AgoraRTCScreenShareProvider, 
  useLocalScreenTrack 
} from "agora-rtc-react";
import {
  AgoraRTCProvider,
  useIsConnected,
  useJoin,
  useLocalCameraTrack,
  useLocalMicrophoneTrack,
  usePublish,
} from "agora-rtc-react";

import { IWidget } from "@/types/widgets";
import { useEffect, useState } from "react";
import { RtcTokenBuilder } from "agora-token";
import { useRTCEnvVar } from "@/api/services/env-var";
import React from "react";
import { toast } from "sonner";
import { useFlowStore } from "@/store";
import AgentView from "@/components/Agent";
import MicrophoneBlock from "@/components/RTC/Microphone";
import VideoBlock from "@/components/RTC/Camera";
import { VideoSourceType } from "@/types/rtc";

const client = AgoraRTC.createClient({ mode: "rtc", codec: "vp8" });

export const RTCWidgetTitle = () => {
  const { t } = useTranslation();
  return t("rtcInteraction.title");
};

const RTCWidgetContentInner = ({ widget }: { widget: IWidget }) => {
  const [ready, setReady] = useState(false);
  const { nodes } = useFlowStore();
  const isConnected = useIsConnected();
  const { value, error } = useRTCEnvVar();
  const { appId, appCert } = value || {};
  const [channel, setChannel] = useState<string | null>(null);
  const [token, setToken] = useState<string | null>(null);
  const [uid, setUid] = useState<number | null>(null);

  React.useEffect(() => {
    if (error) {
      toast.error(error.message);
    }
  }, [error]);

  React.useEffect(() => {
    const rtcNode = nodes.find((node) => node.data.addon === "agora_rtc");
    if (rtcNode) {
      const property = rtcNode.data.property;
      if (property) {
        const propChannel = (property["channel"] || "") as string;
        const propUid = (property["remote_stream_id"]) as (number | null);
        setChannel(propChannel);
        setUid(propUid);
      }
    }
  }, [nodes]);

  useEffect(() => {
    if (!appId || !channel || uid === null) return;
    let token = appId;

    if (appCert) {
      token = RtcTokenBuilder.buildTokenWithUserAccount(
        appId,
        appCert || "",
        channel,
        uid,
        1,
        Math.floor(Date.now() / 1000) + 3600, // 1 hour expiration
        Math.floor(Date.now() / 1000) + 3600  // 1 hour expiration
      );
    }
    setToken(token);
    setReady(true);

    return () => { };
  }, [channel, appId, appCert, uid]);

  useJoin(
    {
      appid: appId || "",
      channel: channel || "",
      token: token ? token : null,
      uid: uid
    },
    ready
  );
  //local user
  const [micOn, setMicOn] = useState(true);
  const [videoOn, setVideoOn] = useState(true);
  const [videoSourceType, setVideoSourceType] = useState<VideoSourceType>(
    VideoSourceType.CAMERA
  );
  const { localMicrophoneTrack } = useLocalMicrophoneTrack(micOn);
  const { localCameraTrack } = useLocalCameraTrack(
    videoSourceType === VideoSourceType.CAMERA ? videoOn : false
  );
  const { screenTrack } = useLocalScreenTrack(
    videoSourceType === VideoSourceType.SCREEN ? videoOn : false,
    {},
    "disable" // withAudio: "enable" | "disable"
  );

  const setMic = async (value: boolean) => {
    if (localMicrophoneTrack) {
      await localMicrophoneTrack.setMuted(!value);
      setMicOn(value);
    }
  };

  const setVideo = async (value: boolean) => {
    if (localCameraTrack) {
      await localCameraTrack.setMuted(!value);
    }
    if (screenTrack) {
      await screenTrack.close();
    }
    setVideoOn(value);
  };

  const setVideoSource = async (value: VideoSourceType) => {
    // If the video source type is changed, close the current track
    if (value !== videoSourceType) {
      if (screenTrack && videoSourceType === VideoSourceType.SCREEN) {
        await screenTrack.close();
      }
    }
    setVideoSourceType(value);
  };

  const publishTracks = videoSourceType === VideoSourceType.CAMERA
    ? [localMicrophoneTrack, localCameraTrack]
    : [localMicrophoneTrack, screenTrack];

  usePublish(publishTracks);

  return (
    <div className="flex flex-col h-full w-full gap-2">
      <AgentView />
      <MicrophoneBlock
        audioTrack={localMicrophoneTrack}
        micOn={micOn}
        setMicOn={setMic}
      />
      <AgoraRTCScreenShareProvider client={client}>
        <VideoBlock
          cameraTrack={localCameraTrack}
          screenTrack={screenTrack}
          videoOn={videoOn}
          setVideoOn={setVideo}
          videoSourceType={videoSourceType}
          onVideoSourceChange={setVideoSource}
        />
      </AgoraRTCScreenShareProvider>
    </div>
  );
};

export const RTCWidgetContent = (props: { widget: IWidget }) => {
  const { widget } = props;

  return (
    <AgoraRTCProvider client={client}>
      <RTCWidgetContentInner widget={widget} />
    </AgoraRTCProvider>
  );
};