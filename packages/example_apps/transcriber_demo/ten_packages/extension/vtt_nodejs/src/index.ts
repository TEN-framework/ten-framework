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
 * Features:
 * 1. Receive audio frames (audio_frame) and record to WAV file
 * 2. Receive ASR results (asr_result) and generate VTT subtitle file
 * 3. Provide session management functionality
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

    // Read configuration from property (if available)
    try {
      const [recordingsPath, error] = await tenEnv.getPropertyString("recordings_path");
      if (error) {
        tenEnv.logWarn(`[VttExtension] Error getting recordings path: ${error}, using default path`);
        this.recordingsPath = "./recordings";
      } else {
        this.recordingsPath = recordingsPath;
      }
    } catch (error) {
      tenEnv.logError(`[VttExtension] Error getting recordings path: ${error}`);
      this.recordingsPath = "./recordings";
    }

    this.sessionManager = new SessionManager(this.recordingsPath);
    tenEnv.logInfo(`[VttExtension] Using recordings path: ${this.recordingsPath}`);
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
   * Handle start recording command
   */
  private async handleStartRecording(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    if (this.audioRecorder && this.audioRecorder.isActive()) {
      const cmdResult = CmdResult.Create(StatusCode.ERROR, cmd);
      cmdResult.setPropertyString("detail", "Already recording");
      tenEnv.returnResult(cmdResult);
      return;
    }

    // Create new session
    const session = this.sessionManager.createSession();
    this.currentSessionId = session.sessionId;

    tenEnv.log(
      LogLevel.INFO,
      `[VttExtension] Starting new recording session: ${this.currentSessionId}`,
      "key_point"
    );

    // Initialize recorder and VTT generator
    const audioPath = this.sessionManager.getAudioPath(this.currentSessionId);
    this.audioRecorder = new AudioRecorder(audioPath);
    this.vttGenerator = new VTTGenerator();

    // Start recording
    this.audioRecorder.start();

    // Return success result
    const cmdResult = CmdResult.Create(StatusCode.OK, cmd);
    cmdResult.setPropertyString("session_id", this.currentSessionId);
    cmdResult.setPropertyString("detail", "Recording started");
    tenEnv.returnResult(cmdResult);
  }

  /**
   * Handle stop recording command
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
      "key_point"
    );

    try {
      // Finalize VTT generation
      const currentTime = this.audioRecorder.getCurrentTimestamp();
      this.vttGenerator.finalize(currentTime);

      // Stop audio recording and save
      await this.audioRecorder.stop();

      // Save VTT file
      const vttPath = this.sessionManager.getVTTPath(this.currentSessionId);
      await this.vttGenerator.save(vttPath);

      // Save JSON format
      const jsonPath = this.sessionManager
        .getSessionPath(this.currentSessionId)
        .concat("/transcript.json");
      await fs.promises.writeFile(
        jsonPath,
        this.vttGenerator.generateJSON(),
        "utf-8"
      );

      // End session and save metadata
      await this.sessionManager.endSession(this.currentSessionId, {
        totalWords: this.vttGenerator.getTotalWords(),
        totalSegments: this.vttGenerator.getSegmentCount(),
      });

      const duration = this.audioRecorder.getDuration();

      tenEnv.log(
        LogLevel.INFO,
        `[VttExtension] Recording completed: ${this.currentSessionId}, duration: ${duration.toFixed(2)}s, segments: ${this.vttGenerator.getSegmentCount()}`,
        "key_point"
      );

      // Return success result
      const cmdResult = CmdResult.Create(StatusCode.OK, cmd);
      cmdResult.setPropertyString("session_id", this.currentSessionId);
      cmdResult.setPropertyString("detail", "Recording stopped");
      cmdResult.setPropertyNumber("duration", Math.floor(duration * 1000));
      cmdResult.setPropertyNumber("segments", this.vttGenerator.getSegmentCount());
      cmdResult.setPropertyNumber("words", this.vttGenerator.getTotalWords());
      tenEnv.returnResult(cmdResult);

      // Cleanup
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
   * Handle list sessions command
   */
  private async handleListSessions(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    try {
      const sessions = await this.sessionManager.listSessions();

      tenEnv.logInfo(`[VttExtension] Listed ${sessions.length} sessions`);

      // Return result (via JSON string)
      const cmdResult = CmdResult.Create(StatusCode.OK, cmd);
      cmdResult.setPropertyString("sessions", JSON.stringify(sessions));
      cmdResult.setPropertyNumber("count", sessions.length);
      tenEnv.returnResult(cmdResult);
    } catch (error) {
      tenEnv.logError(`[VttExtension] Error listing sessions: ${error}`);
      const cmdResult = CmdResult.Create(StatusCode.ERROR, cmd);
      cmdResult.setPropertyString("detail", `Error: ${error}`);
      tenEnv.returnResult(cmdResult);
    }
  }

  /**
   * Handle delete session command
   */
  private async handleDeleteSession(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    try {
      const [sessionId, error] = await cmd.getPropertyString("session_id");
      if (error) {
        tenEnv.logError(`[VttExtension] Error getting session_id: ${error}`);
        const cmdResult = CmdResult.Create(StatusCode.ERROR, cmd);
        cmdResult.setPropertyString("detail", `Error: ${error}`);
        tenEnv.returnResult(cmdResult);
        return;
      }

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
   * Handle get session command
   */
  private async handleGetSession(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    try {
      const [sessionId, error] = await cmd.getPropertyString("session_id");
      if (error) {
        tenEnv.logError(`[VttExtension] Error getting session_id: ${error}`);
        const cmdResult = CmdResult.Create(StatusCode.ERROR, cmd);
        cmdResult.setPropertyString("detail", `Error: ${error}`);
        tenEnv.returnResult(cmdResult);
        return;
      }

      if (!sessionId) {
        const cmdResult = CmdResult.Create(StatusCode.ERROR, cmd);
        cmdResult.setPropertyString("detail", "Missing session_id");
        tenEnv.returnResult(cmdResult);
        return;
      }

      const metadata = await this.sessionManager.getSessionMetadata(sessionId);
      if (!metadata) {
        const cmdResult = CmdResult.Create(StatusCode.ERROR, cmd);
        cmdResult.setPropertyString("detail", "Session not found");
        tenEnv.returnResult(cmdResult);
        return;
      }

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
   * Receive audio frames
   */
  async onAudioFrame(tenEnv: TenEnv, audioFrame: AudioFrame): Promise<void> {
    if (this.audioRecorder && this.audioRecorder.isActive()) {
      this.audioRecorder.writeFrame(audioFrame);
    }
  }

  /**
   * Receive data (ASR results)
   */
  async onData(tenEnv: TenEnv, data: Data): Promise<void> {
    const dataName = data.getName();

    if (dataName === "asr_result") {
      try {
        // First try to get the complete JSON data (pass empty string to get root object)
        const [jsonStr, jsonError] = await data.getPropertyToJson("");

        let text = "";
        let isFinal = false;
        let actualStartMs = 0;
        let actualDurationMs = 0;

        if (!jsonError && jsonStr) {
          // Parse JSON to get all fields
          try {
            const asrResult = JSON.parse(jsonStr);
            text = asrResult.text || "";
            isFinal = asrResult.final || false;
            actualStartMs = asrResult.start_ms || 0;
            actualDurationMs = asrResult.duration_ms || 0;

            tenEnv.logDebug(
              `[VttExtension] Received ASR result from JSON: final=${isFinal}, start_ms=${actualStartMs}, duration_ms=${actualDurationMs}, text="${text}"`
            );
          } catch (parseError) {
            tenEnv.logError(`[VttExtension] Error parsing JSON: ${parseError}`);
            return;
          }
        } else {
          // Fallback to getting fields individually
          const [textVal, error1] = await data.getPropertyString("text");
          if (error1) {
            tenEnv.logError(`[VttExtension] Error getting text: ${error1}`);
            return;
          }
          const [finalVal, error2] = await data.getPropertyBool("final");
          if (error2) {
            tenEnv.logError(`[VttExtension] Error getting final: ${error2}`);
            return;
          }

          text = textVal;
          isFinal = finalVal;

          // Try to get timestamps
          const [startMs, error3] = await data.getPropertyNumber("start_ms");
          if (!error3) {
            actualStartMs = startMs;
          }
          const [durationMs, error4] = await data.getPropertyNumber("duration_ms");
          if (!error4) {
            actualDurationMs = durationMs;
          }

          tenEnv.logDebug(
            `[VttExtension] Received ASR result: final=${isFinal}, start_ms=${actualStartMs}, duration_ms=${actualDurationMs}, text="${text}"`
          );
        }

        if (this.vttGenerator) {
          // Use the actual timestamp provided by ASR instead of recorder's cumulative time
          this.vttGenerator.addAsrResult(text, actualStartMs, isFinal, actualDurationMs);

          // Log final results
          if (isFinal) {
            tenEnv.log(
              LogLevel.INFO,
              `[VttExtension] ASR final result at ${(actualStartMs / 1000).toFixed(2)}s (duration: ${(actualDurationMs / 1000).toFixed(2)}s): "${text}"`,
              "key_point"
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

    // If recording, automatically stop
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
