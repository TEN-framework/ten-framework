//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
import {
  Cmd,
  type CmdResult,
  Data,
  type TenEnv,
  // Loc,
  type TenError,
} from "ten-runtime-nodejs";

/**
 * Check if a character is punctuation (Chinese and English).
 */
export function isPunctuation(char: string): boolean {
  return [",", "，", ".", "。", "?", "？", "!", "！"].includes(char);
}

/**
 * Parse sentences from content based on punctuation.
 * It will return complete sentences and the remaining incomplete fragment.
 */
export function parseSentences(
  sentenceFragment: string,
  content: string
): [string[], string] {
  const sentences: string[] = [];
  let currentSentence = sentenceFragment;

  for (const char of content) {
    currentSentence += char;
    if (isPunctuation(char)) {
      const strippedSentence = currentSentence;
      if ([...strippedSentence].some((c) => /[0-9A-Za-z]/.test(c))) {
        sentences.push(strippedSentence);
      }
      currentSentence = "";
    }
  }

  const remain = currentSentence; // Remaining fragment
  return [sentences, remain];
}

// A minimal awaitable queue with optional abort support
export class AsyncQueue<T> {
  private items: T[] = [];
  private takers: Array<(v: T) => void> = [];

  enqueue(item: T) {
    const taker = this.takers.shift();
    taker ? taker(item) : this.items.push(item);
  }

  clear() {
    this.items.length = 0;
  }
  get length() {
    return this.items.length;
  }

  async dequeue(signal?: AbortSignal): Promise<T> {
    if (this.items.length) return this.items.shift()!;
    return new Promise<T>((resolve, reject) => {
      const onAbort = () => {
        cleanup();
        // DOMException is standard for AbortError; OK to use Error if preferred.
        reject(new DOMException("Aborted", "AbortError"));
      };
      const taker = (v: T) => {
        cleanup();
        resolve(v);
      };
      const cleanup = () => signal?.removeEventListener("abort", onAbort);

      this.takers.push(taker);
      if (signal) {
        if (signal.aborted) return onAbort();
        signal.addEventListener("abort", onAbort, { once: true });
      }
    });
  }
}

/**
 * Send a command with optional payload.
 * Shortcut for intra-graph communication.
 */
export async function sendCmd(
  tenEnv: TenEnv,
  cmdName: string,
  dest: string,
  payload?: any
): Promise<[CmdResult | undefined, TenError | undefined]> {
  const cmd = Cmd.Create(cmdName);
  cmd.setDests([
    {
      appUri: "",
      graphId: "",
      extensionName: dest,
    },
  ]);

  if (payload !== undefined) {
    cmd.setPropertyFromJson("", JSON.stringify(payload));
  }
  tenEnv.logDebug(`sendCmd: cmd_name ${cmdName}, dest ${dest}`);

  return await tenEnv.sendCmd(cmd);
}

/**
 * Send a command with optional payload and stream results back.
 */
export async function* sendCmdEx(
  tenEnv: TenEnv,
  cmdName: string,
  dest: string,
  payload?: any
): AsyncGenerator<[CmdResult | undefined, TenError | undefined]> {
  const cmd = Cmd.Create(cmdName);
  cmd.setDests([
    {
      appUri: "",
      graphId: "",
      extensionName: dest,
    },
  ]);

  if (payload !== undefined) {
    cmd.setPropertyFromJson("", JSON.stringify(payload));
  }

  // await new Promise(resolve => setTimeout(resolve, 1000)); // Simulate async delay

  for await (const [cmdResult, tenError] of tenEnv.sendCmdEx(cmd)) {
    if (cmdResult) {
      tenEnv.logDebug(`sendCmdEx: cmd_result ${cmdResult}`);
    }
    yield [cmdResult, tenError];
  }
  // const cmdResult1 = CmdResult.Create(StatusCode.OK, cmd);
  // const LLMResponse = LLMResponseMessageDeltaSchema.parse({
  //     response_id: "12345",
  //     cerated_at: new Date().toISOString(),
  //     role: "assistant",
  //     delta: "Hey, how can i help you?",
  //     content: "Hey, how can i help you?",
  //     type: "message_content_delta",
  // });
  // cmdResult1.setPropertyFromJson("", JSON.stringify(LLMResponse));
  // yield [cmdResult1, undefined];

  // await new Promise(resolve => setTimeout(resolve, 100)); // Simulate async delay

  // const cmdResult2 = CmdResult.Create(StatusCode.OK, cmd);
  // const LLMResponse2 = LLMResponseMessageDoneSchema.parse({
  //     response_id: "12345",
  //     cerated_at: new Date().toISOString(),
  //     role: "assistant",
  //     content: "Hey, how can i help you?",
  //     type: "message_content_done",
  // });
  // cmdResult2.setPropertyFromJson("", JSON.stringify(LLMResponse2));
  // cmdResult2.setFinal(true);
  // yield [cmdResult2, undefined];
}

/**
 * Send data with optional payload.
 * Shortcut for intra-graph communication.
 */
export async function sendData(
  tenEnv: TenEnv,
  dataName: string,
  dest: string,
  payload?: any
): Promise<TenError | undefined> {
  const data = Data.Create(dataName);
  data.setDests([
    {
      appUri: "",
      graphId: "",
      extensionName: dest,
    },
  ]);

  if (payload !== undefined) {
    data.setPropertyFromJson("", JSON.stringify(payload));
  }
  tenEnv.logInfo(`sendData: data_name ${dataName}, dest ${dest}`);

  return await tenEnv.sendData(data);
}
