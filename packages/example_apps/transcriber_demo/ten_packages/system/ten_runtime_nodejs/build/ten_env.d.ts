import type { AudioFrame } from "./msg/audio_frame.js";
import type { VideoFrame } from "./msg/video_frame.js";
import type { Cmd } from "./msg/cmd/cmd.js";
import type { CmdResult } from "./msg/cmd/cmd_result.js";
import type { Data } from "./msg/data.js";
import { TenError } from "./error.js";
import { LogLevel } from "./log_level.js";
import { Value } from "./value.js";
import type { LogOption } from "./log_option.js";
import type { SendOptions } from "./send_options.js";
export declare class TenEnv {
    sendCmd(cmd: Cmd): Promise<[CmdResult | undefined, TenError | undefined]>;
    sendCmdEx(cmd: Cmd): AsyncGenerator<[
        CmdResult | undefined,
        TenError | undefined
    ], void, unknown>;
    sendData(data: Data, options?: SendOptions): Promise<TenError | undefined>;
    sendVideoFrame(videoFrame: VideoFrame, options?: SendOptions): Promise<TenError | undefined>;
    sendAudioFrame(audioFrame: AudioFrame, options?: SendOptions): Promise<TenError | undefined>;
    returnResult(cmdResult: CmdResult, options?: SendOptions): Promise<TenError | undefined>;
    isPropertyExist(path: string): Promise<boolean>;
    getPropertyToJson(path: string): Promise<[string, TenError | undefined]>;
    setPropertyFromJson(path: string, jsonStr: string): Promise<TenError | undefined>;
    getPropertyNumber(path: string): Promise<[number, TenError | undefined]>;
    setPropertyNumber(path: string, value: number): Promise<TenError | undefined>;
    getPropertyString(path: string): Promise<[string, TenError | undefined]>;
    setPropertyString(path: string, value: string): Promise<TenError | undefined>;
    initPropertyFromJson(jsonStr: string): Promise<TenError | undefined>;
    logDebug(message: string, category?: string | undefined, fields?: Value | Record<string, any> | undefined, option?: LogOption): TenError | undefined;
    logInfo(message: string, category?: string | undefined, fields?: Value | Record<string, any> | undefined, option?: LogOption): TenError | undefined;
    logWarn(message: string, category?: string | undefined, fields?: Value | Record<string, any> | undefined, option?: LogOption): TenError | undefined;
    logError(message: string, category?: string | undefined, fields?: Value | Record<string, any> | undefined, option?: LogOption): TenError | undefined;
    log(level: LogLevel, message: string, category?: string | undefined, fields?: Value | Record<string, any> | undefined, option?: LogOption): TenError | undefined;
    private log_internal;
}
