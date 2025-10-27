//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import type { AudioFrame } from "./msg/audio_frame.js";
import type { VideoFrame } from "./msg/video_frame.js";
import type { Cmd } from "./msg/cmd/cmd.js";
import type { CmdResult } from "./msg/cmd/cmd_result.js";
import type { Data } from "./msg/data.js";
import type { TenError } from "./error.js";
import ten_addon from "./ten_addon.js";
import { LogLevel } from "./log_level.js";
import type { Value } from "./value.js";
import type { LogOption } from "./log_option.js";
import { DefaultLogOption } from "./log_option.js";
import type { SendOptions } from "./send_options.js";

export class TenEnv {
  async sendCmd(
    cmd: Cmd,
  ): Promise<[CmdResult | undefined, TenError | undefined]> {
    return new Promise<[CmdResult | undefined, TenError | undefined]>(
      (resolve) => {
        const err = ten_addon.ten_nodejs_ten_env_send_cmd(
          this,
          cmd,
          async (
            cmdResult: CmdResult | undefined,
            error: TenError | undefined,
          ) => {
            resolve([cmdResult, error]);
          },
          false,
        );

        if (err) {
          resolve([undefined, err]);
        }
      },
    );
  }

  async *sendCmdEx(
    cmd: Cmd,
  ): AsyncGenerator<
    [CmdResult | undefined, TenError | undefined],
    void,
    unknown
  > {
    let resolvePromise:
      | ((value: [CmdResult | undefined, TenError | undefined]) => void)
      | undefined;
    let promise = new Promise<[CmdResult | undefined, TenError | undefined]>(
      (resolve) => {
        resolvePromise = resolve;
      },
    );

    const err = ten_addon.ten_nodejs_ten_env_send_cmd(
      this,
      cmd,
      async (cmdResult: CmdResult | undefined, error: TenError | undefined) => {
        resolvePromise?.([cmdResult, error]);
        promise = new Promise<[CmdResult | undefined, TenError | undefined]>(
          (resolve) => {
            resolvePromise = resolve;
          },
        );
      },
      true, // is_ex = true
    );

    if (err) {
      yield [undefined, err];
      return;
    }

    while (true) {
      const [result, error] = await promise;
      yield [result, error];

      if (error !== undefined) {
        break;
      }

      if (result?.isCompleted()) {
        break;
      }
    }
  }

  async sendData(
    data: Data,
    options?: SendOptions,
  ): Promise<TenError | undefined> {
    // If options is undefined or doesn't wait for result, use fire-and-forget
    // mode.
    if (!options || !options.waitForResult) {
      return ten_addon.ten_nodejs_ten_env_send_data(this, data, undefined);
    }

    // If wait for result, use the original async waiting mode.
    return new Promise<TenError | undefined>((resolve) => {
      const err = ten_addon.ten_nodejs_ten_env_send_data(
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
      return ten_addon.ten_nodejs_ten_env_send_video_frame(
        this,
        videoFrame,
        undefined,
      );
    }

    // If wait for result, use the original async waiting mode.
    return new Promise<TenError | undefined>((resolve) => {
      const err = ten_addon.ten_nodejs_ten_env_send_video_frame(
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
      return ten_addon.ten_nodejs_ten_env_send_audio_frame(
        this,
        audioFrame,
        undefined,
      );
    }

    // If wait for result, use the original async waiting mode.
    return new Promise<TenError | undefined>((resolve) => {
      const err = ten_addon.ten_nodejs_ten_env_send_audio_frame(
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
      return ten_addon.ten_nodejs_ten_env_return_result(
        this,
        cmdResult,
        undefined,
      );
    }

    // If wait for result, use the original async waiting mode.
    return new Promise<TenError | undefined>((resolve) => {
      const err = ten_addon.ten_nodejs_ten_env_return_result(
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

  async isPropertyExist(path: string): Promise<boolean> {
    return new Promise<boolean>((resolve) => {
      ten_addon.ten_nodejs_ten_env_is_property_exist(
        this,
        path,
        async (result: boolean) => {
          resolve(result);
        },
      );
    });
  }

  async getPropertyToJson(
    path: string,
  ): Promise<[string, TenError | undefined]> {
    return new Promise<[string, TenError | undefined]>((resolve) => {
      const err = ten_addon.ten_nodejs_ten_env_get_property_to_json(
        this,
        path,
        async (result: string, error: TenError | undefined) => {
          resolve([result, error]);
        },
      );

      if (err) {
        resolve(["", err]);
      }
    });
  }

  async setPropertyFromJson(
    path: string,
    jsonStr: string,
  ): Promise<TenError | undefined> {
    return new Promise<TenError | undefined>((resolve) => {
      const err = ten_addon.ten_nodejs_ten_env_set_property_from_json(
        this,
        path,
        jsonStr,
        async (error: TenError | undefined) => {
          resolve(error);
        },
      );

      if (err) {
        resolve(err);
      }
    });
  }

  async getPropertyNumber(
    path: string,
  ): Promise<[number, TenError | undefined]> {
    return new Promise<[number, TenError | undefined]>((resolve) => {
      const err = ten_addon.ten_nodejs_ten_env_get_property_number(
        this,
        path,
        async (result: number, error: TenError | undefined) => {
          resolve([result, error]);
        },
      );

      if (err) {
        resolve([0, err]);
      }
    });
  }

  async setPropertyNumber(
    path: string,
    value: number,
  ): Promise<TenError | undefined> {
    return new Promise<TenError | undefined>((resolve) => {
      const err = ten_addon.ten_nodejs_ten_env_set_property_number(
        this,
        path,
        value,
        async (error: TenError | undefined) => {
          resolve(error);
        },
      );

      if (err) {
        resolve(err);
      }
    });
  }

  async getPropertyString(
    path: string,
  ): Promise<[string, TenError | undefined]> {
    return new Promise<[string, TenError | undefined]>((resolve) => {
      const err = ten_addon.ten_nodejs_ten_env_get_property_string(
        this,
        path,
        async (result: string, error: TenError | undefined) => {
          resolve([result, error]);
        },
      );

      if (err) {
        resolve(["", err]);
      }
    });
  }

  async setPropertyString(
    path: string,
    value: string,
  ): Promise<TenError | undefined> {
    return new Promise<TenError | undefined>((resolve) => {
      const err = ten_addon.ten_nodejs_ten_env_set_property_string(
        this,
        path,
        value,
        async (error: TenError | undefined) => {
          resolve(error);
        },
      );

      if (err) {
        resolve(err);
      }
    });
  }

  async initPropertyFromJson(jsonStr: string): Promise<TenError | undefined> {
    return new Promise<TenError | undefined>((resolve) => {
      const err = ten_addon.ten_nodejs_ten_env_init_property_from_json(
        this,
        jsonStr,
        async (error: TenError | undefined) => {
          resolve(error);
        },
      );

      if (err) {
        resolve(err);
      }
    });
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

    return ten_addon.ten_nodejs_ten_env_log_internal(
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

ten_addon.ten_nodejs_ten_env_register_class(TenEnv);
