import type { AudioFrame } from "./msg/audio_frame.js";
import type { Cmd } from "./msg/cmd/cmd.js";
import type { Data } from "./msg/data.js";
import type { VideoFrame } from "./msg/video_frame.js";
import type { TenEnvTester } from "./env_tester.js";
import type { TenError } from "./error.js";
export declare class ExtensionTester {
    constructor();
    onStart(tenEnvTester: TenEnvTester): Promise<void>;
    onStop(tenEnvTester: TenEnvTester): Promise<void>;
    onDeinit(tenEnvTester: TenEnvTester): Promise<void>;
    onCmd(tenEnvTester: TenEnvTester, cmd: Cmd): Promise<void>;
    onData(tenEnvTester: TenEnvTester, data: Data): Promise<void>;
    onAudioFrame(tenEnvTester: TenEnvTester, audioFrame: AudioFrame): Promise<void>;
    onVideoFrame(tenEnvTester: TenEnvTester, videoFrame: VideoFrame): Promise<void>;
    run(): Promise<TenError | undefined>;
    setTestModeSingle(addonName: string, propertyJsonStr: string): TenError | undefined;
    setTimeout(usec: number): TenError | undefined;
    private onInitProxy;
    private onStartProxy;
    private onStopProxy;
    private onDeinitProxy;
    private onCmdProxy;
    private onDataProxy;
    private onAudioFrameProxy;
    private onVideoFrameProxy;
}
