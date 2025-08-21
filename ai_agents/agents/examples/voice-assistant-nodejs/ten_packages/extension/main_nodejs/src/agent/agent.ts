//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file in the root directory for more information.
//
import {
    TenEnv,
    Cmd,
    CmdResult,
    Data,
    StatusCode,
} from "ten-runtime-nodejs";
// import { LLMToolMetadata } from "ten-ai-base";
import {
    AgentEvent,
    UserJoinedEvent,
    UserLeftEvent,
    ToolRegisterEvent,
    ASRResultEvent,
    LLMResponseEvent,
} from "./events.js";
// import { LLMExec } from "./llm_exec.js";

// Types for handler registration
type EventHandler<T extends AgentEvent> = (event: T) => Promise<void>;

export class Agent {
    private tenEnv: TenEnv;
    private stopped = false;

    private callbacks: Map<Function, EventHandler<any>[]> = new Map();

    // Queues
    private asrQueue: ASRResultEvent[] = [];
    private llmQueue: LLMResponseEvent[] = [];

    // Consumer loops
    private asrConsumer?: Promise<void>;
    private llmConsumer?: Promise<void>;
    private llmActiveTask?: Promise<void>;

    // public llmExec: LLMExec;

    constructor(tenEnv: TenEnv) {
        this.tenEnv = tenEnv;

        // this.llmExec = new LLMExec(tenEnv);
        // this.llmExec.onResponse = this._onLLMResponse.bind(this);
        // this.llmExec.onReasoningResponse = this._onLLMReasoningResponse.bind(this);

        this.asrConsumer = this.consumeASR();
        this.llmConsumer = this.consumeLLM();
    }

    // === Register handlers ===
    on<T extends AgentEvent>(
        eventClass: new (...args: any[]) => T,
        handler?: EventHandler<T>
    ): any {
        const decorator = (fn: EventHandler<T>) => {
            const list = this.callbacks.get(eventClass) || [];
            list.push(fn);
            this.callbacks.set(eventClass, list);
            return fn;
        };

        if (handler) {
            return decorator(handler);
        }
        return decorator;
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

    // === Consumers ===
    private async consumeASR() {
        while (!this.stopped) {
            const event = this.asrQueue.shift();
            if (event) await this.dispatch(event);
            else await new Promise((r) => setTimeout(r, 5));
        }
    }

    private async consumeLLM() {
        while (!this.stopped) {
            const event = this.llmQueue.shift();
            if (event) {
                this.llmActiveTask = this.dispatch(event);
                try {
                    await this.llmActiveTask;
                } catch {
                    this.tenEnv.logInfo("[Agent] Active LLM task cancelled");
                } finally {
                    this.llmActiveTask = undefined;
                }
            } else {
                await new Promise((r) => setTimeout(r, 5));
            }
        }
    }

    // === Emit events ===
    private async emitASR(event: ASRResultEvent) {
        this.asrQueue.push(event);
    }

    private async emitLLM(event: LLMResponseEvent) {
        this.llmQueue.push(event);
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
                // const [toolJson, err] = cmd.getPropertyToJson("tool");
                // if (err) throw new Error(`Invalid tool metadata: ${err}`);
                // const tool = LLMToolMetadata.model_validate_json(toolJson);
                // await this.emitDirect(
                //     new ToolRegisterEvent({ tool, source: cmd.getSource().extensionName })
                // );
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
                await this.emitASR(
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

    // === LLM Callbacks ===
    private async _onLLMResponse(
        _tenEnv: TenEnv,
        delta: string,
        text: string,
        isFinal: boolean
    ) {
        await this.emitLLM(new LLMResponseEvent(
            text, isFinal, delta, "message"
        ))
    }

    private async _onLLMReasoningResponse(
        _tenEnv: TenEnv,
        delta: string,
        text: string,
        isFinal: boolean
    ) {
        await this.emitLLM(new LLMResponseEvent(
            text, isFinal, delta, "reasoning"
        ));
    }

    // === LLM control ===
    // async registerLLMTool(tool: LLMToolMetadata, source: string) {
    //     await this.llmExec.registerTool(tool, source);
    // }

    // async queueLLMInput(text: string) {
    //     await this.llmExec.queueInput(text);
    // }

    // async flushLLM() {
    //     await this.llmExec.flush();

    //     this.llmQueue.length = 0;

    //     if (this.llmActiveTask) {
    //         // cancel simulation: ignore resolution
    //         this.llmActiveTask = undefined;
    //     }
    // }

    async stop() {
        this.stopped = true;
        // await this.llmExec.stop();
        // await this.flushLLM();
    }
}
