//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
export enum VideoSourceType {
  CAMERA = "camera",
  SCREEN = "screen",
}

export const VIDEO_SOURCE_OPTIONS = [
  {
    label: "Camera",
    value: VideoSourceType.CAMERA,
  },
  {
    label: "Screen Share",
    value: VideoSourceType.SCREEN,
  },
];

export type TDeviceSelectItem = {
  label: string;
  value: string;
  deviceId: string;
};

export const DEFAULT_DEVICE_ITEM: TDeviceSelectItem = {
  label: "Default",
  value: "default",
  deviceId: "",
};

export enum EMessageType {
  AGENT = "agent",
  USER = "user",
}

export enum EMessageDataType {
  TEXT = "text",
  REASON = "reason",
  IMAGE = "image",
}

export interface IChatItem {
  userId: number | string;
  userName?: string;
  text: string;
  data_type: EMessageDataType;
  type: EMessageType;
  isFinal?: boolean;
  time: number;
}

export interface ITextDataChunk {
  message_id: string;
  part_index: number;
  total_parts: number;
  content: string;
}
