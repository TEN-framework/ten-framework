//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { AudioFrame } from "./audio_frame.js";
import { VideoFrame } from "./video_frame.js";
import { Cmd } from "./cmd.js";
import { CmdResult } from "./cmd_result.js";
import { Data } from "./data.js";
import ten_addon from "./ten_addon.js";
import { LogLevel } from "./log_level.js";
import { TenError } from "./error.js";

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
        );

        if (err) {
          resolve([undefined, err]);
        }
      },
    );
  }

  async sendData(data: Data): Promise<TenError | undefined> {
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

  async sendVideoFrame(videoFrame: VideoFrame): Promise<TenError | undefined> {
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

  async sendAudioFrame(audioFrame: AudioFrame): Promise<TenError | undefined> {
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

  async returnResult(cmdResult: CmdResult): Promise<TenError | undefined> {
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

  logVerbose(message: string): TenError | undefined {
    return this.log_internal(LogLevel.VERBOSE, message);
  }

  logDebug(message: string): TenError | undefined {
    return this.log_internal(LogLevel.DEBUG, message);
  }

  logInfo(message: string): TenError | undefined {
    return this.log_internal(LogLevel.INFO, message);
  }

  logWarn(message: string): TenError | undefined {
    return this.log_internal(LogLevel.WARN, message);
  }

  logError(message: string): TenError | undefined {
    return this.log_internal(LogLevel.ERROR, message);
  }

  logFatal(message: string): TenError | undefined {
    return this.log_internal(LogLevel.FATAL, message);
  }

  log(level: LogLevel, message: string): TenError | undefined {
    return this.log_internal(level, message);
  }

  private log_internal(level: number, message: string): TenError | undefined {
    const _prepareStackTrace = Error.prepareStackTrace;
    Error.prepareStackTrace = (_, stack): NodeJS.CallSite[] => stack;
    const stack_ = new Error().stack as unknown as NodeJS.CallSite[];
    const stack = stack_.slice(1);
    Error.prepareStackTrace = _prepareStackTrace;

    const _callerFile = stack[1].getFileName();
    const _callerLine = stack[1].getLineNumber();
    const _callerFunction = stack[1].getFunctionName();

    const callerFile = _callerFile ? _callerFile : "unknown";
    const callerLine = _callerLine ? _callerLine : 0;
    const callerFunction = _callerFunction ? _callerFunction : "anonymous";

    return ten_addon.ten_nodejs_ten_env_log_internal(
      this,
      level,
      callerFunction,
      callerFile,
      callerLine,
      message,
    );
  }
}

ten_addon.ten_nodejs_ten_env_register_class(TenEnv);
