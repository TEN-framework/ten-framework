import type {
  ICameraVideoTrack,
  ILocalVideoTrack,
  IMicrophoneAudioTrack,
  NetworkQuality,
  UID,
} from "agora-rtc-sdk-ng";

export interface IRtcUser extends IUserTracks {
  userId: UID;
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
