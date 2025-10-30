//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import type { Cmd } from "./msg/cmd/cmd.js";
import type { CmdResult } from "./msg/cmd/cmd_result.js";
import type { Data } from "./msg/data.js";
import type { VideoFrame } from "./msg/video_frame.js";
import type { AudioFrame } from "./msg/audio_frame.js";
import ten_addon from "./ten_addon.js";
import { LogLevel } from "./log_level.js";
import type { TenError } from "./error.js";
import type { Value } from "./value.js";
import type { LogOption } from "./log_option.js";
import { DefaultLogOption } from "./log_option.js";
import type { SendOptions } from "./send_options.js";

export class TenEnvTester {
  async sendCmd(
    cmd: Cmd,
  ): Promise<[CmdResult | undefined, TenError | undefined]> {
    return new Promise<[CmdResult | undefined, TenError | undefined]>(
      (resolve) => {
        const err = ten_addon.ten_nodejs_ten_env_tester_send_cmd(
          this,
          cmd,
          async (
            cmdResult: CmdResult | undefined,
            error: TenError | undefined,
          ) => {
            resolve([cmdResult, error]);
          },
        );

        if (err) {
          resolve([undefined, err]);
        }
      },
    );
  }

  async sendData(
    data: Data,
    options?: SendOptions,
  ): Promise<TenError | undefined> {
    // If options is undefined or doesn't wait for result, use fire-and-forget
    // mode.
    if (!options || !options.waitForResult) {
      return ten_addon.ten_nodejs_ten_env_tester_send_data(
        this,
        data,
        undefined,
      );
    }

    // If care about result, use the original async waiting mode.
    return new Promise<TenError | undefined>((resolve) => {
      const err = ten_addon.ten_nodejs_ten_env_tester_send_data(
        this,
        data,
        async (error: TenError | undefined) => {
          resolve(error);
        },
      );

      if (err) {
        resolve(err);
      }
    });
  }

  async sendVideoFrame(
    videoFrame: VideoFrame,
    options?: SendOptions,
  ): Promise<TenError | undefined> {
    // If options is undefined or doesn't wait for result, use fire-and-forget
    // mode.
    if (!options || !options.waitForResult) {
      return ten_addon.ten_nodejs_ten_env_tester_send_video_frame(
        this,
        videoFrame,
        undefined,
      );
    }

    // If care about result, use the original async waiting mode.
    return new Promise<TenError | undefined>((resolve) => {
      const err = ten_addon.ten_nodejs_ten_env_tester_send_video_frame(
        this,
        videoFrame,
        async (error: TenError | undefined) => {
          resolve(error);
        },
      );

      if (err) {
        resolve(err);
      }
    });
  }

  async sendAudioFrame(
    audioFrame: AudioFrame,
    options?: SendOptions,
  ): Promise<TenError | undefined> {
    // If options is undefined or doesn't wait for result, use fire-and-forget
    // mode.
    if (!options || !options.waitForResult) {
      return ten_addon.ten_nodejs_ten_env_tester_send_audio_frame(
        this,
        audioFrame,
        undefined,
      );
    }

    // If care about result, use the original async waiting mode.
    return new Promise<TenError | undefined>((resolve) => {
      const err = ten_addon.ten_nodejs_ten_env_tester_send_audio_frame(
        this,
        audioFrame,
        async (error: TenError | undefined) => {
          resolve(error);
        },
      );

      if (err) {
        resolve(err);
      }
    });
  }

  async returnResult(
    cmdResult: CmdResult,
    options?: SendOptions,
  ): Promise<TenError | undefined> {
    // If options is undefined or doesn't wait for result, use fire-and-forget
    // mode.
    if (!options || !options.waitForResult) {
      return ten_addon.ten_nodejs_ten_env_tester_return_result(
        this,
        cmdResult,
        undefined,
      );
    }

    // If care about result, use the original async waiting mode.
    return new Promise<TenError | undefined>((resolve) => {
      const err = ten_addon.ten_nodejs_ten_env_tester_return_result(
        this,
        cmdResult,
        async (error: TenError | undefined) => {
          resolve(error);
        },
      );

      if (err) {
        resolve(err);
      }
    });
  }

  stopTest(result: TenError | undefined = undefined): TenError | undefined {
    if (result) {
      return ten_addon.ten_nodejs_ten_env_tester_stop_test(
        this,
        result.errorCode,
        result.errorMessage,
      );
    }

    return ten_addon.ten_nodejs_ten_env_tester_stop_test(this, 0, "");
  }

  logDebug(
    message: string,
    category: string | undefined = undefined,
    fields: Value | undefined = undefined,
    option: LogOption = DefaultLogOption,
  ): TenError | undefined {
    return this.log_internal(LogLevel.DEBUG, message, category, fields, option);
  }

  logInfo(
    message: string,
    category: string | undefined = undefined,
    fields: Value | undefined = undefined,
    option: LogOption = DefaultLogOption,
  ): TenError | undefined {
    return this.log_internal(LogLevel.INFO, message, category, fields, option);
  }

  logWarn(
    message: string,
    category: string | undefined = undefined,
    fields: Value | undefined = undefined,
    option: LogOption = DefaultLogOption,
  ): TenError | undefined {
    return this.log_internal(LogLevel.WARN, message, category, fields, option);
  }

  logError(
    message: string,
    category: string | undefined = undefined,
    fields: Value | undefined = undefined,
    option: LogOption = DefaultLogOption,
  ): TenError | undefined {
    return this.log_internal(LogLevel.ERROR, message, category, fields, option);
  }

  log(
    level: LogLevel,
    message: string,
    category: string | undefined = undefined,
    fields: Value | undefined = undefined,
    option: LogOption = DefaultLogOption,
  ): TenError | undefined {
    return this.log_internal(level, message, category, fields, option);
  }

  private log_internal(
    level: number,
    message: string,
    category: string | undefined,
    fields: Value | undefined,
    option: LogOption,
  ): TenError | undefined {
    const _prepareStackTrace = Error.prepareStackTrace;
    Error.prepareStackTrace = (_, stack): NodeJS.CallSite[] => stack;
    const stack_ = new Error().stack as unknown as NodeJS.CallSite[];
    const stack = stack_.slice(1);
    Error.prepareStackTrace = _prepareStackTrace;

    const skipIndex = Math.min(option.skip - 1, stack.length - 1);
    const _callerFile = stack[skipIndex].getFileName();
    const _callerLine = stack[skipIndex].getLineNumber();
    const _callerFunction = stack[skipIndex].getFunctionName();

    const callerFile = _callerFile ? _callerFile : "unknown";
    const callerLine = _callerLine ? _callerLine : 0;
    const callerFunction = _callerFunction ? _callerFunction : "anonymous";

    return ten_addon.ten_nodejs_ten_env_tester_log_internal(
      this,
      level,
      callerFunction,
      callerFile,
      callerLine,
      category,
      message,
    );
  }
}

ten_addon.ten_nodejs_ten_env_tester_register_class(TenEnvTester);
