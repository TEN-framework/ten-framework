//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
import {
  Addon,
  type Cmd,
  CmdResult,
  type Data,
  Extension,
  RegisterAddonAsExtension,
  StatusCode,
  type TenEnv,
} from "ten-runtime-nodejs";
import z from "zod";
import { Agent } from "./agent/agent.js";
import {
  ASRResultEvent,
  LLMResponseEvent,
  UserJoinedEvent,
  UserLeftEvent,
} from "./agent/events.js";
import { parseSentences, sendCmd, sendData } from "./helper.js";

const MainControlConfig = z.object({
  greeting: z
    .string()
    .default("Ten Agent connected, how can i help you today?"),
});

type MainControlConfig = z.infer<typeof MainControlConfig>;

class MainControlExtension extends Extension {
  tenEnv!: TenEnv;
  agent!: Agent;
  config!: MainControlConfig;
  joinedUserCount: number = 0;
  session_id: string = "0";
  turn_id: number = 0;
  sentenceFragment: string = "";

  async onConfigure(_tenEnv: TenEnv): Promise<void> {
    console.log("MainControlExtension onConfigure");
  }

  async onInit(_tenEnv: TenEnv): Promise<void> {
    console.log("MainControlExtension onInit");
    this.tenEnv = _tenEnv;
    this.agent = new Agent(_tenEnv);

    const [config_json] = await _tenEnv.getPropertyToJson("");
    if (!config_json) {
      throw new Error("MainControlExtension config is not set");
    }

    this.config = MainControlConfig.parse(JSON.parse(config_json));

    this.agent.on(UserJoinedEvent, async (event) => {
      this.joinedUserCount++;
      if (this.joinedUserCount === 1) {
        await this._send_to_tts(this.config.greeting, true);
        await this._send_transcript(
          "assistant",
          this.config.greeting,
          true,
          100
        );
      }
    });

    this.agent.on(UserLeftEvent, async (event) => {
      this.joinedUserCount--;
      console.log(`User left, total: ${this.joinedUserCount}`);
    });

    this.agent.on(ASRResultEvent, async (event) => {
      console.log(`ASR Result: ${event.text}`);
      // Handle ASR result processing here
      this.session_id = String(event.metadata?.session_id ?? "100");
      const stream_id = Number(this.session_id) || 0;

      if (!event.text) return;
      if (event.final || event.text.length > 2) {
        await this._interrupt();
      }

      if (event.final) {
        this.turn_id += 1;
        await this.agent.queueLLMInput(event.text);
      }
      await this._send_transcript("user", event.text, event.final, stream_id);
    });

    this.agent.on(LLMResponseEvent, async (event) => {
      if (!event) return;
      if (!event.is_final && event.kind === "message") {
        const [sentences, remainText] = parseSentences(
          this.sentenceFragment,
          event.delta
        );
        this.sentenceFragment = remainText;
        for (const sentence of sentences) {
          await this._send_to_tts(sentence, false);
        }
      }

      const dataType = event.kind === "message" ? "text" : "reasoning";

      await this._send_transcript(
        "assistant",
        event.content,
        event.is_final,
        100,
        dataType
      );
    });
  }

  async onStart(_tenEnv: TenEnv): Promise<void> {
    console.log("MainControlExtension onStart");
  }

  async onCmd(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    console.log("MainControlExtension onCmd", cmd.getName());
    await this.agent.onCmd(cmd);
  }

  async onData(tenEnv: TenEnv, data: Data): Promise<void> {
    console.log("MainControlExtension onData", data.getName());
    await this.agent.onData(data);
  }

  async onStop(_tenEnv: TenEnv): Promise<void> {
    console.log("MainControlExtension onStop");
  }

  async onDeinit(_tenEnv: TenEnv): Promise<void> {
    console.log("MainControlExtension onDeinit");
  }

  // === helpers ===
  private async _send_transcript(
    role: string,
    text: string,
    final: boolean,
    stream_id: number,
    data_type: "text" | "reasoning" = "text"
  ): Promise<void> {
    /**
     * Sends the transcript (ASR or LLM output) to the message collector.
     */
    if (data_type === "text") {
      await sendData(this.tenEnv, "message", "message_collector", {
        data_type: "transcribe",
        role,
        text,
        text_ts: Date.now(), // int(time.time() * 1000)
        is_final: final,
        stream_id,
      });
    } else if (data_type === "reasoning") {
      await sendData(this.tenEnv, "message", "message_collector", {
        data_type: "raw",
        role,
        text: JSON.stringify({
          type: "reasoning",
          data: { text },
        }),
        text_ts: Date.now(),
        is_final: final,
        stream_id,
      });
    }

    this.tenEnv.logInfo(
      `[MainControlExtension] Sent transcript: ${role}, final=${final}, text=${text}`
    );
  }

  private async _send_to_tts(text: string, is_final: boolean): Promise<void> {
    /**
     * Sends a sentence to the TTS system.
     */
    const request_id = `tts-request-${this.turn_id}`;
    await sendData(this.tenEnv, "tts_text_input", "tts", {
      request_id,
      text,
      text_input_end: is_final,
      metadata: this._current_metadata(),
    });

    this.tenEnv.logInfo(
      `[MainControlExtension] Sent to TTS: is_final=${is_final}, text=${text}`
    );
  }

  private async _interrupt(): Promise<void> {
    /**
     * Interrupts the current LLM processing.
     */
    this.sentenceFragment = "";
    await this.agent.flushLLM();
    await sendData(this.tenEnv, "tts_flush", "tts", {
      flush_id: String(Date.now()),
    });
    await sendCmd(this.tenEnv, "flush", "agora_rtc");
    this.tenEnv.logInfo(`[MainControlExtension] Sent interrupt to LLM`);
  }

  _current_metadata(): Record<string, any> {
    /**
     * Returns the current metadata for the session.
     */
    return {
      session_id: this.session_id,
      turn_id: this.turn_id,
    };
  }
}

@RegisterAddonAsExtension("main_nodejs")
class MainControlExtensionAddon extends Addon {
  async onCreateInstance(
    _tenEnv: TenEnv,
    instanceName: string
  ): Promise<Extension> {
    return new MainControlExtension(instanceName);
  }
}
