//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
"use client";

import * as React from "react";
import { CameraIcon, CameraOffIcon } from "lucide-react";
import { DeviceSelect } from "@/components/RTC/Device";
import { Button } from "@/components/ui/Button";
import { SelectItem } from "@/components/ui/Select";
import { MonitorIcon, MonitorXIcon } from "lucide-react";
import AgoraRTC, {
  ICameraVideoTrack,
  ILocalVideoTrack,
  LocalVideoTrack,
} from "agora-rtc-react";
import { VideoSourceType } from "@/types/rtc";
import { t } from "i18next";

export function VideoDeviceWrapper(props: {
  children: React.ReactNode;
  onIconClick: () => void;
  videoSourceType: VideoSourceType;
  onVideoSourceChange: (value: VideoSourceType) => void;
  isActive: boolean;
  select?: React.ReactNode;
}) {
  const { onIconClick, isActive, select, children, videoSourceType } = props;

  return (
    <div className="flex flex-col">
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-2 w-full">
          {/* Fixed width button */}
          <Button
            variant="outline"
            size="default"
            className="border-secondary bg-transparent w-32 flex-shrink-0"
            onClick={onIconClick}
          >
            {videoSourceType === VideoSourceType.SCREEN ? (
              <>
                {isActive ? (
                  <MonitorIcon className="h-5 w-5" />
                ) : (
                  <MonitorXIcon className="h-5 w-5" />
                )}
                <span className="ml-2">{t("rtc.videoSource.screen")}</span>
              </>
            ) : (
              <>
                {isActive ? (
                  <CameraIcon className="h-5 w-5" />
                ) : (
                  <CameraOffIcon className="h-5 w-5" />
                )}
                <span className="ml-2">{t("rtc.videoSource.camera")}</span>
              </>
            )}
          </Button>

          {/* Select grows to fill the remaining space */}
          <div className="flex-grow">
            <div className="flex justify-end">{select}</div>
          </div>
        </div>
      </div>
      {children}
    </div>
  );
}

export default function VideoBlock(props: {
  videoSourceType: VideoSourceType;
  onVideoSourceChange: (value: VideoSourceType) => void;
  cameraTrack: ICameraVideoTrack | null;
  screenTrack: ILocalVideoTrack | null;
  videoOn: boolean;
  setVideoOn: (value: boolean) => void;
}) {
  const {
    cameraTrack,
    screenTrack,
    videoOn,
    setVideoOn,
    videoSourceType,
    onVideoSourceChange,
  } = props;

  const onClickMute = () => {
    setVideoOn(!videoOn);
  };

  return (
    <VideoDeviceWrapper
      onIconClick={onClickMute}
      isActive={videoOn}
      videoSourceType={videoSourceType}
      onVideoSourceChange={onVideoSourceChange}
      select={
        <VideoDeviceSelect
          cameraTrack={cameraTrack as ICameraVideoTrack}
          videoSourceType={videoSourceType}
          onVideoSourceChange={onVideoSourceChange}
        />
      }
    >
      <div className="mt-3 h-60 w-full overflow-hidden rounded-lg">
        <LocalVideoTrack
          key={
            videoSourceType === VideoSourceType.CAMERA
              ? cameraTrack?.getTrackId()
              : VideoSourceType.SCREEN
          }
          track={
            videoSourceType === VideoSourceType.CAMERA
              ? cameraTrack
              : screenTrack
          }
          play
        />
      </div>
    </VideoDeviceWrapper>
  );
}

interface SelectItem {
  label: string;
  value: string;
  deviceId: string;
}

const DEFAULT_ITEM: SelectItem = {
  label: "Default",
  value: "default",
  deviceId: "",
};

const VideoDeviceSelect = (props: {
  cameraTrack?: ICameraVideoTrack;
  videoSourceType: VideoSourceType;
  onVideoSourceChange: (value: VideoSourceType) => void;
}) => {
  const { cameraTrack, onVideoSourceChange, videoSourceType } = props;
  const [items, setItems] = React.useState<SelectItem[]>([DEFAULT_ITEM]);
  const [value, setValue] = React.useState("default");

  React.useEffect(() => {
    if (cameraTrack) {
      const label = cameraTrack?.getTrackLabel();
      if (videoSourceType === VideoSourceType.SCREEN) {
        setValue(VideoSourceType.SCREEN);
      } else {
        setValue(label);
      }
      AgoraRTC.getCameras().then((arr) => {
        setItems([
          ...arr.map((item) => ({
            label: item.label,
            value: item.label,
            deviceId: item.deviceId,
          })),
          ...[
            {
              label: t("rtc.videoSource.screen"),
              value: VideoSourceType.SCREEN,
              deviceId: VideoSourceType.SCREEN,
            },
          ],
        ]);
      });
    }
  }, [videoSourceType, cameraTrack]);

  const onChange = async (value: string) => {
    const target = items.find((item) => item.value === value);
    if (value === VideoSourceType.SCREEN) {
      setValue(value);
      onVideoSourceChange(VideoSourceType.SCREEN);
      return;
    }
    if (target) {
      setValue(target.value);
      if (cameraTrack) {
        onVideoSourceChange(VideoSourceType.CAMERA);
        await cameraTrack.setDevice(target.deviceId);
      }
    }
  };

  return (
    <DeviceSelect
      items={items}
      value={value}
      onChange={onChange}
      placeholder={t("rtc.videoSource.cameraPlaceholder")}
    />
  );
};
