export enum VideoSourceType {
    CAMERA = 'camera',
    SCREEN = 'screen',
}

export const VIDEO_SOURCE_OPTIONS = [{
    label: "Camera",
    value: VideoSourceType.CAMERA,
}, {
    label: "Screen Share",
    value: VideoSourceType.SCREEN,
}];

export type TDeviceSelectItem = {
    label: string
    value: string
    deviceId: string
  }
  
  export const DEFAULT_DEVICE_ITEM: TDeviceSelectItem = {
    label: "Default",
    value: "default",
    deviceId: "",
  };