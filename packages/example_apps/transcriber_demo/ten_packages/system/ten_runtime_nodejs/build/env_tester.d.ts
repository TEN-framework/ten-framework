import type { Cmd } from "./msg/cmd/cmd.js";
import type { CmdResult } from "./msg/cmd/cmd_result.js";
import type { Data } from "./msg/data.js";
import type { VideoFrame } from "./msg/video_frame.js";
import type { AudioFrame } from "./msg/audio_frame.js";
import { LogLevel } from "./log_level.js";
import { TenError } from "./error.js";
import { Value } from "./value.js";
import type { LogOption } from "./log_option.js";
import type { SendOptions } from "./send_options.js";
export declare class TenEnvTester {
    sendCmd(cmd: Cmd): Promise<[CmdResult | undefined, TenError | undefined]>;
    sendData(data: Data, options?: SendOptions): Promise<TenError | undefined>;
    sendVideoFrame(videoFrame: VideoFrame, options?: SendOptions): Promise<TenError | undefined>;
    sendAudioFrame(audioFrame: AudioFrame, options?: SendOptions): Promise<TenError | undefined>;
    returnResult(cmdResult: CmdResult, options?: SendOptions): Promise<TenError | undefined>;
    stopTest(result?: TenError | undefined): TenError | undefined;
    logDebug(message: string, category?: string | undefined, fields?: Value | Record<string, any> | undefined, option?: LogOption): TenError | undefined;
    logInfo(message: string, category?: string | undefined, fields?: Value | Record<string, any> | undefined, option?: LogOption): TenError | undefined;
    logWarn(message: string, category?: string | undefined, fields?: Value | Record<string, any> | undefined, option?: LogOption): TenError | undefined;
    logError(message: string, category?: string | undefined, fields?: Value | Record<string, any> | undefined, option?: LogOption): TenError | undefined;
    log(level: LogLevel, message: string, category?: string | undefined, fields?: Value | Record<string, any> | undefined, option?: LogOption): TenError | undefined;
    private log_internal;
}
