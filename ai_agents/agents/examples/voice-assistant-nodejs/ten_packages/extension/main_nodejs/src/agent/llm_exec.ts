// //
// // This file is part of TEN Framework, an open source project.
// // Licensed under the Apache License, Version 2.0.
// //
// import {
//     LLMMessage,
//     LLMMessageContent,
//     LLMMessageFunctionCall,
//     LLMMessageFunctionCallOutput,
//     LLMRequest,
//     LLMResponse,
//     LLMResponseMessageDelta,
//     LLMResponseMessageDone,
//     LLMResponseReasoningDelta,
//     LLMResponseReasoningDone,
//     LLMResponseToolCall,
//     parseLLMResponse,
// } from "../types/llm_structs";
// import { LLMToolMetadata, LLMToolResult } from "../types/llm_types";
// import { sendCmd, sendCmdEx } from "../helper.js";
// import { AsyncTenEnv, StatusCode } from "ten-runtime";
// import { v4 as uuidv4 } from "uuid";

// type ResponseHandler = (
//     env: AsyncTenEnv,
//     delta: string,
//     text: string,
//     isFinal: boolean
// ) => Promise<void>;

// type ReasoningHandler = (
//     env: AsyncTenEnv,
//     delta: string,
//     text: string,
//     isFinal: boolean
// ) => Promise<void>;

// type ToolCallHandler = (
//     env: AsyncTenEnv,
//     tool: LLMToolMetadata
// ) => Promise<void>;

// export class LLMExec {
//     private env: AsyncTenEnv;
//     private inputQueue: Array<string>;
//     private stopped = false;
//     private currentTask: Promise<void> | null = null;
//     private availableTools: LLMToolMetadata[] = [];
//     private toolRegistry: Record<string, string> = {};
//     private contexts: LLMMessage[] = [];
//     private currentRequestId: string | null = null;
//     private currentText: string | null = null;

//     public onResponse: ResponseHandler | null = null;
//     public onReasoningResponse: ReasoningHandler | null = null;
//     public onToolCall: ToolCallHandler | null = null;

//     constructor(env: AsyncTenEnv) {
//         this.env = env;
//         this.inputQueue = new Array<string>();
//         this._processInputQueue();
//     }

//     async queueInput(item: string): Promise<void> {
//         await this.inputQueue.push(item);
//     }

//     async flush(): Promise<void> {
//         this.inputQueue = [];
//         if (this.currentRequestId) {
//             const requestId = this.currentRequestId;
//             this.currentRequestId = null;
//             await sendCmd(this.env, "abort", "llm", { request_id: requestId });
//         }
//         if (this.currentTask) {
//             // not true cancel but mark stopped
//             this.env.log_info("Cancelling current LLM task");
//         }
//     }

//     async stop(): Promise<void> {
//         this.stopped = true;
//         await this.flush();
//     }

//     async registerTool(tool: LLMToolMetadata, source: string): Promise<void> {
//         this.availableTools.push(tool);
//         this.toolRegistry[tool.name] = source;
//     }

//     private async _processInputQueue(): Promise<void> {
//         while (!this.stopped) {
//             try {
//                 const text = await this.inputQueue.shift();
//                 const newMessage = new LLMMessageContent("user", text);

//                 this.currentTask = this._sendToLLM(newMessage);
//                 await this.currentTask;
//             } catch (err: any) {
//                 if (err?.name === "AbortError") {
//                     this.env.log_info("LLMExec processing aborted.");
//                     const text = this.currentText;
//                     this.currentText = null;
//                     if (this.onResponse && text) {
//                         await this.onResponse(this.env, "", text, true);
//                     }
//                 } else {
//                     this.env.log_error(
//                         `Error processing input queue: ${err?.stack || err}`
//                     );
//                 }
//             } finally {
//                 this.currentTask = null;
//             }
//         }
//     }

//     private async _queueContext(newMessage: LLMMessage): Promise<void> {
//         this.env.log_info(`_queueContext: ${JSON.stringify(newMessage)}`);
//         this.contexts.push(newMessage);
//     }

//     private async _writeContext(
//         role: "user" | "assistant",
//         content: string
//     ): Promise<void> {
//         const last = this.contexts[this.contexts.length - 1];
//         if (last && last.role === role) {
//             (last as LLMMessageContent).content = content;
//         } else {
//             const newMessage = new LLMMessageContent(role, content);
//             await this._queueContext(newMessage);
//         }
//     }

//     private async _sendToLLM(newMessage: LLMMessage): Promise<void> {
//         const messages = [...this.contexts, newMessage];
//         const requestId = uuidv4();
//         this.currentRequestId = requestId;

//         const llmInput = new LLMRequest({
//             request_id: requestId,
//             messages,
//             model: "qwen-max",
//             streaming: true,
//             parameters: { temperature: 0.7 },
//             tools: this.availableTools,
//         });

//         const inputJson = llmInput.toJSON();
//         const response = sendCmdEx(this.env, "chat_completion", "llm", inputJson);

//         await this._queueContext(newMessage);

//         for await (const [cmdResult, _] of response) {
//             if (cmdResult && !cmdResult.isFinal()) {
//                 if (cmdResult.getStatusCode() === StatusCode.OK) {
//                     const [responseJson, _] = cmdResult.getPropertyToJson("");
//                     this.env.log_info(`_sendToLLM: response_json ${responseJson}`);
//                     const completion = parseLLMResponse(responseJson);
//                     await this._handleLLMResponse(completion);
//                 }
//             }
//         }
//     }

//     private async _handleLLMResponse(llmOutput: LLMResponse | null) {
//         this.env.log_info(`_handleLLMResponse: ${JSON.stringify(llmOutput)}`);

//         if (!llmOutput) return;

//         if (llmOutput instanceof LLMResponseMessageDelta) {
//             const delta = llmOutput.delta;
//             const text = llmOutput.content;
//             this.currentText = text;
//             if (delta && this.onResponse) {
//                 await this.onResponse(this.env, delta, text, false);
//             }
//             if (text) {
//                 await this._writeContext("assistant", text);
//             }
//         } else if (llmOutput instanceof LLMResponseMessageDone) {
//             const text = llmOutput.content;
//             this.currentText = null;
//             if (this.onResponse && text) {
//                 await this.onResponse(this.env, "", text, true);
//             }
//         } else if (llmOutput instanceof LLMResponseReasoningDelta) {
//             const delta = llmOutput.delta;
//             const text = llmOutput.content;
//             if (delta && this.onReasoningResponse) {
//                 await this.onReasoningResponse(this.env, delta, text, false);
//             }
//         } else if (llmOutput instanceof LLMResponseReasoningDone) {
//             const text = llmOutput.content;
//             if (this.onReasoningResponse && text) {
//                 await this.onReasoningResponse(this.env, "", text, true);
//             }
//         } else if (llmOutput instanceof LLMResponseToolCall) {
//             this.env.log_info(
//                 `_handleLLMResponse: invoking tool call ${llmOutput.name}`
//             );
//             const srcExtensionName = this.toolRegistry[llmOutput.name];
//             const [result, _] = await sendCmd(
//                 this.env,
//                 "tool_call",
//                 srcExtensionName,
//                 {
//                     name: llmOutput.name,
//                     arguments: llmOutput.arguments,
//                 }
//             );

//             if (result?.getStatusCode() === StatusCode.OK) {
//                 const [r] = result?.getPropertyToJson("result") || [];
//                 const toolResult: LLMToolResult = JSON.parse(r);
//                 this.env.log_info(`tool_result: ${JSON.stringify(toolResult)}`);

//                 const contextFunctionCall = new LLMMessageFunctionCall({
//                     name: llmOutput.name,
//                     arguments: JSON.stringify(llmOutput.arguments),
//                     call_id: llmOutput.tool_call_id,
//                     id: llmOutput.response_id,
//                     type: "function_call",
//                 });

//                 if (toolResult.type === "llmresult") {
//                     const resultContent = toolResult.content;
//                     if (typeof resultContent === "string") {
//                         await this._queueContext(contextFunctionCall);
//                         await this._sendToLLM(
//                             new LLMMessageFunctionCallOutput({
//                                 output: resultContent,
//                                 call_id: llmOutput.tool_call_id,
//                                 type: "function_call_output",
//                             })
//                         );
//                     } else {
//                         this.env.log_error(
//                             `Unknown tool result content: ${resultContent}`
//                         );
//                     }
//                 } else if (toolResult.type === "requery") {
//                     // requery logic TODO
//                 }
//             } else {
//                 this.env.log_error("Tool call failed");
//             }
//         }
//     }
// }
