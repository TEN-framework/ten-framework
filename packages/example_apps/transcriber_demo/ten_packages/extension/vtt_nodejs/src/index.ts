//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
import {
  Addon,
  RegisterAddonAsExtension,
  Extension,
  TenEnv,
  Cmd,
  CmdResult,
  StatusCode,
  AudioFrame,
  Data,
  LogLevel,
} from "ten-runtime-nodejs";
import { SessionManager, SessionMetadata } from "./session-manager.js";
import { AudioRecorder } from "./audio-recorder.js";
import { VTTGenerator } from "./vtt-generator.js";
import * as fs from "fs";

/**
 * VTT Recorder Extension
 * 功能：
 * 1. 接收音频帧（audio_frame）并录制为 WAV 文件
 * 2. 接收 ASR 结果（asr_result）并生成 VTT 字幕文件
 * 3. 提供会话管理功能
 */
class VttExtension extends Extension {
  private sessionManager: SessionManager;
  private audioRecorder: AudioRecorder | null = null;
  private vttGenerator: VTTGenerator | null = null;
  private currentSessionId: string | null = null;
  private recordingsPath: string = "./recordings";

  constructor(name: string) {
    super(name);
    this.sessionManager = new SessionManager(this.recordingsPath);
  }

  async onConfigure(tenEnv: TenEnv): Promise<void> {
    tenEnv.logInfo("[VttExtension] onConfigure");
  }

  async onInit(tenEnv: TenEnv): Promise<void> {
    tenEnv.logInfo("[VttExtension] onInit");

    // 从 property 读取配置（如果有）
    try {
      const recordingsPath = tenEnv.getPropertyString("recordings_path");
      if (recordingsPath) {
        this.recordingsPath = recordingsPath;
        this.sessionManager = new SessionManager(this.recordingsPath);
        tenEnv.logInfo(`[VttExtension] Using recordings path: ${this.recordingsPath}`);
      }
    } catch (error) {
      // 使用默认值
      tenEnv.logDebug("[VttExtension] Using default recordings path");
    }
  }

  async onStart(tenEnv: TenEnv): Promise<void> {
    tenEnv.logInfo("[VttExtension] onStart - Ready to record");
  }

  async onCmd(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    const cmdName = cmd.getName();
    tenEnv.logDebug(`[VttExtension] onCmd: ${cmdName}`);

    try {
      switch (cmdName) {
        case "start_recording":
          await this.handleStartRecording(tenEnv, cmd);
          break;

        case "stop_recording":
          await this.handleStopRecording(tenEnv, cmd);
          break;

        case "list_sessions":
          await this.handleListSessions(tenEnv, cmd);
          break;

        case "delete_session":
          await this.handleDeleteSession(tenEnv, cmd);
          break;

        case "get_session":
          await this.handleGetSession(tenEnv, cmd);
          break;

        default:
          const cmdResult = CmdResult.Create(StatusCode.ERROR, cmd);
          cmdResult.setPropertyString("detail", `Unknown command: ${cmdName}`);
          tenEnv.returnResult(cmdResult);
      }
    } catch (error) {
      tenEnv.logError(`[VttExtension] Error handling command ${cmdName}: ${error}`);
      const cmdResult = CmdResult.Create(StatusCode.ERROR, cmd);
      cmdResult.setPropertyString("detail", `Error: ${error}`);
      tenEnv.returnResult(cmdResult);
    }
  }

  /**
   * 处理开始录制命令
   */
  private async handleStartRecording(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    if (this.audioRecorder && this.audioRecorder.isActive()) {
      const cmdResult = CmdResult.Create(StatusCode.ERROR, cmd);
      cmdResult.setPropertyString("detail", "Already recording");
      tenEnv.returnResult(cmdResult);
      return;
    }

    // 创建新会话
    const session = this.sessionManager.createSession();
    this.currentSessionId = session.sessionId;

    tenEnv.log(
      LogLevel.INFO,
      `[VttExtension] Starting new recording session: ${this.currentSessionId}`,
      { category: "key_point" }
    );

    // 初始化录制器和 VTT 生成器
    const audioPath = this.sessionManager.getAudioPath(this.currentSessionId);
    this.audioRecorder = new AudioRecorder(audioPath);
    this.vttGenerator = new VTTGenerator();

    // 开始录制
    this.audioRecorder.start();

    // 返回成功结果
    const cmdResult = CmdResult.Create(StatusCode.OK, cmd);
    cmdResult.setPropertyString("session_id", this.currentSessionId);
    cmdResult.setPropertyString("detail", "Recording started");
    tenEnv.returnResult(cmdResult);
  }

  /**
   * 处理停止录制命令
   */
  private async handleStopRecording(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    if (!this.audioRecorder || !this.audioRecorder.isActive()) {
      const cmdResult = CmdResult.Create(StatusCode.ERROR, cmd);
      cmdResult.setPropertyString("detail", "Not recording");
      tenEnv.returnResult(cmdResult);
      return;
    }

    if (!this.currentSessionId || !this.vttGenerator) {
      const cmdResult = CmdResult.Create(StatusCode.ERROR, cmd);
      cmdResult.setPropertyString("detail", "Invalid session state");
      tenEnv.returnResult(cmdResult);
      return;
    }

    tenEnv.log(
      LogLevel.INFO,
      `[VttExtension] Stopping recording session: ${this.currentSessionId}`,
      { category: "key_point" }
    );

    try {
      // 完成 VTT 生成
      const currentTime = this.audioRecorder.getCurrentTimestamp();
      this.vttGenerator.finalize(currentTime);

      // 停止音频录制并保存
      await this.audioRecorder.stop();

      // 保存 VTT 文件
      const vttPath = this.sessionManager.getVTTPath(this.currentSessionId);
      await this.vttGenerator.save(vttPath);

      // 保存 JSON 格式
      const jsonPath = this.sessionManager
        .getSessionPath(this.currentSessionId)
        .concat("/transcript.json");
      await fs.promises.writeFile(
        jsonPath,
        this.vttGenerator.generateJSON(),
        "utf-8"
      );

      // 结束会话并保存元数据
      await this.sessionManager.endSession(this.currentSessionId, {
        totalWords: this.vttGenerator.getTotalWords(),
        totalSegments: this.vttGenerator.getSegmentCount(),
      });

      const duration = this.audioRecorder.getDuration();

      tenEnv.log(
        LogLevel.INFO,
        `[VttExtension] Recording completed: ${this.currentSessionId}, duration: ${duration.toFixed(2)}s, segments: ${this.vttGenerator.getSegmentCount()}`,
        { category: "key_point" }
      );

      // 返回成功结果
      const cmdResult = CmdResult.Create(StatusCode.OK, cmd);
      cmdResult.setPropertyString("session_id", this.currentSessionId);
      cmdResult.setPropertyString("detail", "Recording stopped");
      cmdResult.setPropertyInt64("duration", Math.floor(duration * 1000));
      cmdResult.setPropertyInt64("segments", this.vttGenerator.getSegmentCount());
      cmdResult.setPropertyInt64("words", this.vttGenerator.getTotalWords());
      tenEnv.returnResult(cmdResult);

      // 清理
      this.audioRecorder = null;
      this.vttGenerator = null;
      this.currentSessionId = null;
    } catch (error) {
      tenEnv.logError(`[VttExtension] Error stopping recording: ${error}`);
      const cmdResult = CmdResult.Create(StatusCode.ERROR, cmd);
      cmdResult.setPropertyString("detail", `Error stopping recording: ${error}`);
      tenEnv.returnResult(cmdResult);
    }
  }

  /**
   * 处理列出会话命令
   */
  private async handleListSessions(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    try {
      const sessions = await this.sessionManager.listSessions();

      tenEnv.logInfo(`[VttExtension] Listed ${sessions.length} sessions`);

      // 返回结果（通过 JSON 字符串）
      const cmdResult = CmdResult.Create(StatusCode.OK, cmd);
      cmdResult.setPropertyString("sessions", JSON.stringify(sessions));
      cmdResult.setPropertyInt64("count", sessions.length);
      tenEnv.returnResult(cmdResult);
    } catch (error) {
      tenEnv.logError(`[VttExtension] Error listing sessions: ${error}`);
      const cmdResult = CmdResult.Create(StatusCode.ERROR, cmd);
      cmdResult.setPropertyString("detail", `Error: ${error}`);
      tenEnv.returnResult(cmdResult);
    }
  }

  /**
   * 处理删除会话命令
   */
  private async handleDeleteSession(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    try {
      const sessionId = cmd.getPropertyString("session_id");

      if (!sessionId) {
        const cmdResult = CmdResult.Create(StatusCode.ERROR, cmd);
        cmdResult.setPropertyString("detail", "Missing session_id");
        tenEnv.returnResult(cmdResult);
        return;
      }

      const success = await this.sessionManager.deleteSession(sessionId);

      if (success) {
        tenEnv.logInfo(`[VttExtension] Deleted session: ${sessionId}`);
        const cmdResult = CmdResult.Create(StatusCode.OK, cmd);
        cmdResult.setPropertyString("detail", "Session deleted");
        tenEnv.returnResult(cmdResult);
      } else {
        const cmdResult = CmdResult.Create(StatusCode.ERROR, cmd);
        cmdResult.setPropertyString("detail", "Session not found");
        tenEnv.returnResult(cmdResult);
      }
    } catch (error) {
      tenEnv.logError(`[VttExtension] Error deleting session: ${error}`);
      const cmdResult = CmdResult.Create(StatusCode.ERROR, cmd);
      cmdResult.setPropertyString("detail", `Error: ${error}`);
      tenEnv.returnResult(cmdResult);
    }
  }

  /**
   * 处理获取会话命令
   */
  private async handleGetSession(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    try {
      const sessionId = cmd.getPropertyString("session_id");

      if (!sessionId) {
        const cmdResult = CmdResult.Create(StatusCode.ERROR, cmd);
        cmdResult.setPropertyString("detail", "Missing session_id");
        tenEnv.returnResult(cmdResult);
        return;
      }

      const metadata = await this.sessionManager.getSessionMetadata(sessionId);

      if (metadata) {
        const cmdResult = CmdResult.Create(StatusCode.OK, cmd);
        cmdResult.setPropertyString("metadata", JSON.stringify(metadata));
        tenEnv.returnResult(cmdResult);
      } else {
        const cmdResult = CmdResult.Create(StatusCode.ERROR, cmd);
        cmdResult.setPropertyString("detail", "Session not found");
        tenEnv.returnResult(cmdResult);
      }
    } catch (error) {
      tenEnv.logError(`[VttExtension] Error getting session: ${error}`);
      const cmdResult = CmdResult.Create(StatusCode.ERROR, cmd);
      cmdResult.setPropertyString("detail", `Error: ${error}`);
      tenEnv.returnResult(cmdResult);
    }
  }

  /**
   * 接收音频帧
   */
  async onAudioFrame(tenEnv: TenEnv, audioFrame: AudioFrame): Promise<void> {
    if (this.audioRecorder && this.audioRecorder.isActive()) {
      this.audioRecorder.writeFrame(audioFrame);
    }
  }

  /**
   * 接收数据（ASR 结果）
   */
  async onData(tenEnv: TenEnv, data: Data): Promise<void> {
    const dataName = data.getName();

    if (dataName === "asr_result") {
      try {
        const text = data.getPropertyString("text");
        const isFinal = data.getPropertyBool("final");

        tenEnv.logDebug(
          `[VttExtension] Received ASR result: final=${isFinal}, text="${text}"`
        );

        if (this.vttGenerator && this.audioRecorder) {
          const timestamp = this.audioRecorder.getCurrentTimestamp();
          this.vttGenerator.addAsrResult(text, timestamp, isFinal);

          // 记录 final 结果
          if (isFinal) {
            tenEnv.log(
              LogLevel.INFO,
              `[VttExtension] ASR final result at ${(timestamp / 1000).toFixed(2)}s: "${text}"`,
              { category: "key_point" }
            );
          }
        }
      } catch (error) {
        tenEnv.logError(`[VttExtension] Error processing ASR result: ${error}`);
      }
    }
  }

  async onStop(tenEnv: TenEnv): Promise<void> {
    tenEnv.logInfo("[VttExtension] onStop");

    // 如果正在录制，自动停止
    if (this.audioRecorder && this.audioRecorder.isActive()) {
      tenEnv.logWarn("[VttExtension] Force stopping active recording");
      this.audioRecorder.cancel();
    }
  }

  async onDeinit(tenEnv: TenEnv): Promise<void> {
    tenEnv.logInfo("[VttExtension] onDeinit");
  }
}

@RegisterAddonAsExtension("vtt_nodejs")
class VttExtensionAddon extends Addon {
  async onCreateInstance(
    _tenEnv: TenEnv,
    instanceName: string
  ): Promise<Extension> {
    return new VttExtension(instanceName);
  }
}
