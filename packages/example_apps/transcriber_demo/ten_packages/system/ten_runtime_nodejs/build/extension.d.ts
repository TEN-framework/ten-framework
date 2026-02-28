import type { Cmd } from "./msg/cmd/cmd.js";
import type { Data } from "./msg/data.js";
import type { AudioFrame } from "./msg/audio_frame.js";
import type { VideoFrame } from "./msg/video_frame.js";
import type { TenEnv } from "./ten_env.js";
export declare abstract class Extension {
    constructor(name: string);
    private onConfigureProxy;
    private onInitProxy;
    private onStartProxy;
    private onStopProxy;
    private onDeinitProxy;
    private onCmdProxy;
    private onDataProxy;
    private onAudioFrameProxy;
    private onVideoFrameProxy;
    onConfigure(tenEnv: TenEnv): Promise<void>;
    onInit(tenEnv: TenEnv): Promise<void>;
    onStart(tenEnv: TenEnv): Promise<void>;
    onStop(tenEnv: TenEnv): Promise<void>;
    onDeinit(tenEnv: TenEnv): Promise<void>;
    onCmd(tenEnv: TenEnv, cmd: Cmd): Promise<void>;
    onData(tenEnv: TenEnv, data: Data): Promise<void>;
    onAudioFrame(tenEnv: TenEnv, frame: AudioFrame): Promise<void>;
    onVideoFrame(tenEnv: TenEnv, frame: VideoFrame): Promise<void>;
}
