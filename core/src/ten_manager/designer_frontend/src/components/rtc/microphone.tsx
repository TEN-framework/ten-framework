//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
"use client";

import AgoraRTC, { type IMicrophoneAudioTrack } from "agora-rtc-react";
import { MicIcon, MicOffIcon } from "lucide-react";
import * as React from "react";
import AudioVisualizer from "@/components/agent/audio-visualizer";
import { DeviceSelect } from "@/components/rtc/device";
import { Button } from "@/components/ui/button";
import { DEFAULT_DEVICE_ITEM, type TDeviceSelectItem } from "@/types/rtc";

export default function MicrophoneBlock(props: {
  audioTrack: IMicrophoneAudioTrack | null;
  micOn: boolean;
  setMicOn: (value: boolean) => void;
}) {
  const { audioTrack, micOn, setMicOn } = props;
  const onClickMute = () => {
    setMicOn(!micOn);
  };

  return (
    <MicrophoneDeviceWrapper
      Icon={micOn ? MicIcon : MicOffIcon}
      onIconClick={onClickMute}
      select={<MicrophoneSelect audioTrack={audioTrack} />}
      audioTrack={audioTrack}
    ></MicrophoneDeviceWrapper>
  );
}

export function MicrophoneDeviceWrapper(props: {
  children?: React.ReactNode;
  Icon: (
    props: React.SVGProps<SVGSVGElement> & { active?: boolean }
  ) => React.ReactNode;
  onIconClick: () => void;
  select?: React.ReactNode;
  audioTrack: IMicrophoneAudioTrack | null;
}) {
  const { Icon, onIconClick, select, children, audioTrack } = props;

  return (
    <div className="flex flex-col">
      <div className="flex items-center justify-between">
        <div className="flex w-full items-center gap-2">
          <Button
            variant="outline"
            className="w-32 flex-shrink-0 border-secondary bg-transparent"
            onClick={onIconClick}
          >
            <Icon className="h-5 w-5" />

            <AudioVisualizer
              type="user"
              barWidth={2}
              minBarHeight={2}
              maxBarHeight={20}
              track={audioTrack}
              bands={10}
              borderRadius={2}
              gap={4}
            />
          </Button>
          <div className="flex-grow">
            <div className="flex justify-end">{select}</div>
          </div>
        </div>
      </div>
      {children}
    </div>
  );
}

export const MicrophoneSelect = (props: {
  audioTrack: IMicrophoneAudioTrack | null;
}) => {
  const { audioTrack } = props;
  const [items, setItems] = React.useState<TDeviceSelectItem[]>([
    DEFAULT_DEVICE_ITEM,
  ]);
  const [value, setValue] = React.useState("default");

  React.useEffect(() => {
    if (audioTrack) {
      const label = audioTrack?.getTrackLabel();
      setValue(label);
      AgoraRTC.getMicrophones().then((arr) => {
        setItems(
          arr.map((item) => ({
            label: item.label,
            value: item.label,
            deviceId: item.deviceId,
          }))
        );
      });
    }
  }, [audioTrack]);

  const onChange = async (value: string) => {
    const target = items.find((item) => item.value === value);
    if (target) {
      setValue(target.value);
      if (audioTrack) {
        await audioTrack.setDevice(target.deviceId);
      }
    }
  };

  return <DeviceSelect items={items} value={value} onChange={onChange} />;
};
