//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
//

// ==== Base Event ====
export abstract class AgentEventBase {
  constructor(
    public type: "cmd" | "data" | "message",
    public name: string
  ) {}
}

// ==== CMD Events ====

export class UserJoinedEvent extends AgentEventBase {
  constructor() {
    super("cmd", "on_user_joined");
  }
}

export class UserLeftEvent extends AgentEventBase {
  constructor() {
    super("cmd", "on_user_left");
  }
}

export class ToolRegisterEvent extends AgentEventBase {
  constructor(
    public tool: any, // TODO: replace with LLMToolMetadata type
    public source?: string
  ) {
    super("cmd", "tool_register");
  }
}

// ==== DATA Events ====

export class ASRResultEvent extends AgentEventBase {
  constructor(
    public text: string,
    public final: boolean,
    public metadata?: Record<string, any>
  ) {
    super("data", "asr_result");
  }
}

// ==== MESSAGE Events ====

export class LLMResponseEvent extends AgentEventBase {
  constructor(
    public delta: string,
    public content: string,
    public is_final: boolean,
    public kind: "message" | "reasoning" = "message"
  ) {
    super("data", "llm_response");
    this.content = content;
    this.delta = delta;
    this.is_final = is_final;
  }
}

// ==== Union Type Helper ====

export type AgentEvent =
  | UserJoinedEvent
  | UserLeftEvent
  | ToolRegisterEvent
  | ASRResultEvent
  | LLMResponseEvent;
