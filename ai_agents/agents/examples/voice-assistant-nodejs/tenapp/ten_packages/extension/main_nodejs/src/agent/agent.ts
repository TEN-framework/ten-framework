//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file in the root directory for more information.
//
import {
  type Cmd,
  CmdResult,
  type Data,
  StatusCode,
  type TenEnv,
} from "ten-runtime-nodejs";
import { AsyncQueue } from "../helper.js";
// import { LLMToolMetadata } from "ten-ai-base";
import {
  type AgentEvent,
  ASRResultEvent,
  LLMResponseEvent,
  ToolRegisterEvent,
  UserJoinedEvent,
  UserLeftEvent,
} from "./events.js";
import { LLMExec } from "./llm_exec.js";

// Types for handler registration
type EventHandler<T extends AgentEvent> = (event: T) => Promise<void>;

export class Agent {
  private tenEnv: TenEnv;
  private stopped = false;

  private callbacks: Map<Function, EventHandler<any>[]> = new Map();

  // Awaitable queues
  private asrQueue = new AsyncQueue<ASRResultEvent>();
  private llmQueue = new AsyncQueue<LLMResponseEvent>();

  // Drain state
  private drainingASR = false;
  private drainingLLM = false;

  // LLM task tracking / cancellation
  private llmActiveTask?: Promise<void>;
  private llmAbort = new AbortController();
  private llmExec: LLMExec;

  constructor(tenEnv: TenEnv) {
    this.tenEnv = tenEnv;
    this.llmExec = new LLMExec(tenEnv);
    this.llmExec.onResponse = this._onLLMResponse.bind(this);
    this.llmExec.onReasoningResponse = this._onLLMReasoningResponse.bind(this);
  }

  // === Register handlers ===
  on<T extends AgentEvent>(
    eventClass: new (...args: any[]) => T,
    handler?: EventHandler<T>
  ) {
    const register = (fn: EventHandler<T>) => {
      const list = this.callbacks.get(eventClass) || [];
      list.push(fn);
      this.callbacks.set(eventClass, list);
      return fn;
    };
    return handler ? register(handler) : register;
  }

  private async dispatch(event: AgentEvent) {
    for (const [etype, handlers] of this.callbacks.entries()) {
      if (event instanceof (etype as any)) {
        for (const h of handlers) {
          try {
            await h(event);
          } catch (err) {
            this.tenEnv.logError(`Handler error for ${etype}: ${err}`);
          }
        }
      }
    }
  }

  // === Drainers (scheduled; no polling) ===
  private scheduleASRDrain() {
    if (this.drainingASR || this.stopped) return;
    this.drainingASR = true;
    queueMicrotask(async () => {
      try {
        // drain everything currently enqueued; new arrivals will schedule another drain
        while (this.asrQueue.length > 0 && !this.stopped) {
          const evt = await this.asrQueue.dequeue();
          await this.dispatch(evt);
        }
      } finally {
        this.drainingASR = false;
        // If something slipped in after we checked length, schedule again.
        if (this.asrQueue.length > 0 && !this.stopped) this.scheduleASRDrain();
      }
    });
  }

  private scheduleLLMDrain() {
    if (this.drainingLLM || this.stopped) return;
    this.drainingLLM = true;
    queueMicrotask(async () => {
      try {
        // Strictly serialize LLM events
        while (this.llmQueue.length > 0 && !this.stopped) {
          const evt = await this.llmQueue.dequeue(this.llmAbort.signal);
          this.llmActiveTask = this.dispatch(evt);
          try {
            await this.llmActiveTask;
          } catch (e) {
            // Abort is expected during flush() or stop()
            if ((e as any)?.name !== "AbortError") {
              this.tenEnv.logError(`[Agent] LLM task failed: ${e}`);
            }
          } finally {
            this.llmActiveTask = undefined;
          }
        }
      } finally {
        this.drainingLLM = false;
        if (this.llmQueue.length > 0 && !this.stopped) this.scheduleLLMDrain();
      }
    });
  }

  // === Emit events ===
  private emitASR(event: ASRResultEvent) {
    this.asrQueue.enqueue(event);
    this.scheduleASRDrain();
  }

  private emitLLM(event: LLMResponseEvent) {
    this.llmQueue.enqueue(event);
    this.scheduleLLMDrain();
  }

  private async emitDirect(event: AgentEvent) {
    await this.dispatch(event);
  }

  // === Incoming from runtime ===
  async onCmd(cmd: Cmd) {
    try {
      const name = cmd.getName();
      if (name === "on_user_joined") {
        await this.emitDirect(new UserJoinedEvent());
      } else if (name === "on_user_left") {
        await this.emitDirect(new UserLeftEvent());
      } else if (name === "tool_register") {
        // parse & emit your ToolRegisterEvent here if needed
      } else {
        this.tenEnv.logWarn(`Unhandled cmd: ${name}`);
      }
      await this.tenEnv.returnResult(CmdResult.Create(StatusCode.OK, cmd));
    } catch (e) {
      this.tenEnv.logError(`onCmd error: ${e}`);
      await this.tenEnv.returnResult(CmdResult.Create(StatusCode.ERROR, cmd));
    }
  }

  async onData(data: Data) {
    try {
      if (data.getName() === "asr_result") {
        const [asrJson] = data.getPropertyToJson("");
        const asr = JSON.parse(asrJson);
        this.emitASR(
          new ASRResultEvent(
            asr.text || "",
            asr.final || false,
            asr.metadata || {}
          )
        );
      } else {
        this.tenEnv.logWarn(`Unhandled data: ${data.getName()}`);
      }
    } catch (e) {
      this.tenEnv.logError(`onData error: ${e}`);
    }
  }

  queueLLMInput(input: string) {
    this.llmExec.queueInput(input);
  }

  // === LLM callbacks ===
  private async _onLLMResponse(
    _tenEnv: TenEnv,
    delta: string,
    text: string,
    isFinal: boolean
  ) {
    this.tenEnv.logInfo(
      `LLM response: ${text}, isFinal: ${isFinal}, delta: ${delta}`
    );
    this.emitLLM(new LLMResponseEvent(delta, text, isFinal, "message"));
  }

  private async _onLLMReasoningResponse(
    _tenEnv: TenEnv,
    delta: string,
    text: string,
    isFinal: boolean
  ) {
    this.emitLLM(new LLMResponseEvent(delta, text, isFinal, "reasoning"));
  }

  // === Optional: targeted LLM flush / cancel ===
  // You can wire this to a request_id filter if needed:
  async flushLLM() {
    await this.llmExec.flush();

    // Cancel any pending dequeue + active LLM task
    this.llmAbort.abort();
    this.llmAbort = new AbortController(); // new token for future dequeues
    this.llmQueue.clear(); // drop queued partials
    // Note: active handler should respect AbortSignal if it does I/O
  }

  async stop() {
    this.stopped = true;
    this.flushLLM();
  }
}
