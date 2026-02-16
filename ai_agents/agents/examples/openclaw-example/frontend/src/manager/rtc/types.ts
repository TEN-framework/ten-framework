import type {
  ICameraVideoTrack,
  ILocalVideoTrack,
  IMicrophoneAudioTrack,
  IRemoteAudioTrack,
  IRemoteVideoTrack,
  NetworkQuality,
  UID,
} from "agora-rtc-sdk-ng";

export interface IRtcUser {
  userId: UID;
  videoTrack?: IRemoteVideoTrack;
  screenTrack?: ILocalVideoTrack;
  audioTrack?: IRemoteAudioTrack;
}

export interface RtcEvents {
  remoteUserChanged: (user: IRtcUser) => void;
  localTracksChanged: (tracks: IUserTracks) => void;
  networkQuality: (quality: NetworkQuality) => void;
}

export interface IUserTracks {
  videoTrack?: ICameraVideoTrack;
  screenTrack?: ILocalVideoTrack;
  audioTrack?: IMicrophoneAudioTrack;
}
