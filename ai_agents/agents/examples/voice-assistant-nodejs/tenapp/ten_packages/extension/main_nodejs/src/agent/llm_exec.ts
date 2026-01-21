import { StatusCode, type TenEnv } from "ten-runtime-nodejs";
import { v4 as uuidv4 } from "uuid";
import { AsyncQueue, sendCmd, sendCmdEx } from "../helper.js";
// Import types separately so they are erased at runtime
import type {
  LLMMessage,
  LLMMessageContent,
  LLMMessageFunctionCall,
  LLMMessageFunctionCallOutput,
  LLMRequest,
  LLMResponse,
  LLMToolMetadata,
  LLMToolResult,
} from "./struct.js";
import { parseLLMResponse } from "./struct.js";

type ResponseHandler = (
  env: TenEnv,
  delta: string,
  text: string,
  isFinal: boolean
) => Promise<void>;
type ReasoningHandler = (
  env: TenEnv,
  delta: string,
  text: string,
  isFinal: boolean
) => Promise<void>;
type ToolCallHandler = (env: TenEnv, tool: LLMToolMetadata) => Promise<void>;

export class LLMExec {
  private env: TenEnv;
  private inputQueue = new AsyncQueue<string>(); // awaitable queue
  private draining = false;
  private stopped = false;

  private availableTools: LLMToolMetadata[] = [];
  private toolRegistry: Record<string, string> = {};
  private contexts: LLMMessage[] = [];

  private currentRequestId: string | null = null;
  private currentText: string | null = null;

  private llmAbort = new AbortController(); // cancels waiting & streaming
  private currentTask: Promise<void> | null = null;

  public onResponse: ResponseHandler | null = null;
  public onReasoningResponse: ReasoningHandler | null = null;
  public onToolCall: ToolCallHandler | null = null;

  constructor(env: TenEnv) {
    this.env = env;
  }

  /** Enqueue user text and schedule a drain (no busy loop). */
  async queueInput(item: string): Promise<void> {
    this.inputQueue.enqueue(item);
    this.scheduleDrain();
  }

  /** Cancel current request and clear queued inputs. */
  async flush(): Promise<void> {
    // Abort any pending dequeue/streaming loop
    this.llmAbort.abort();
    this.llmAbort = new AbortController(); // fresh token for future work

    this.inputQueue.clear();

    if (this.currentRequestId) {
      const requestId = this.currentRequestId;
      this.currentRequestId = null;
      await sendCmd(this.env, "abort", "llm", { request_id: requestId });
    }
    this.env.logInfo("LLMExec: flush requested");
  }

  async stop(): Promise<void> {
    this.stopped = true;
    await this.flush();
  }

  async registerTool(tool: LLMToolMetadata, source: string): Promise<void> {
    this.availableTools.push(tool);
    this.toolRegistry[tool.name] = source;
  }

  // ---------- Drain scheduler ----------
  private scheduleDrain() {
    if (this.draining || this.stopped) return;
    this.draining = true;
    queueMicrotask(async () => {
      try {
        // Drain everything currently queued; new arrivals will schedule another drain
        while (this.inputQueue.length > 0 && !this.stopped) {
          let text: string;
          try {
            text = await this.inputQueue.dequeue(this.llmAbort.signal);
          } catch (e: any) {
            if (e?.name === "AbortError") break; // flushed; stop this drain
            throw e;
          }

          // Build a plain content message (no class construction)
          const newMsg: LLMMessageContent = {
            role: "user",
            content: text,
          };

          this.currentTask = this._sendToLLM(newMsg);
          try {
            await this.currentTask;
          } finally {
            this.currentTask = null;
          }
        }
      } catch (err: any) {
        this.env.logError(`LLMExec drain error: ${err?.stack || err}`);
      } finally {
        this.draining = false;
        if (this.inputQueue.length > 0 && !this.stopped) this.scheduleDrain();
      }
    });
  }

  // ---------- Context helpers ----------
  private async _queueContext(newMessage: LLMMessage): Promise<void> {
    this.env.logInfo(`_queueContext: ${JSON.stringify(newMessage)}`);
    this.contexts.push(newMessage);
  }

  /** Append or update the last assistant/user text message. */
  private async _writeContext(
    role: "user" | "assistant",
    content: string
  ): Promise<void> {
    const last = this.contexts[this.contexts.length - 1];

    // Only mutate when the last item is a plain content message (no "type" field)
    if (last && (last as any).role === role && !("type" in (last as any))) {
      (last as LLMMessageContent).content = content;
    } else {
      const newMessage: LLMMessageContent = { role, content };
      await this._queueContext(newMessage);
    }
  }

  // ---------- LLM I/O ----------
  private async _sendToLLM(newMessage: LLMMessage): Promise<void> {
    const messages = [...this.contexts, newMessage];
    const requestId = uuidv4(); // or crypto.randomUUID() on modern runtimes
    this.currentRequestId = requestId;

    // Build the request as a plain object
    const llmInput: LLMRequest = {
      request_id: requestId,
      messages,
      model: "qwen-max",
      streaming: true,
      parameters: { temperature: 0.7 },
      tools: this.availableTools,
    };

    // If your runtime expects a JSON string, stringify here
    const stream = sendCmdEx(this.env, "chat_completion", "llm", llmInput);

    await this._queueContext(newMessage);

    // Stream loop (cooperative cancel via llmAbort + server-side abort cmd)
    for await (const [cmdResult] of stream) {
      if (this.llmAbort.signal.aborted) {
        this.env.logInfo(
          "LLMExec: abort signal observed; breaking stream loop"
        );
        break;
      }
      if (!cmdResult) continue;

      if (cmdResult.getStatusCode() === StatusCode.OK) {
        const [responseJson] = cmdResult.getPropertyToJson("");
        this.env.logInfo(`_sendToLLM: response_json ${responseJson}`);
        const completion = parseLLMResponse(responseJson);
        await this._handleLLMResponse(completion);
      }
    }
  }

  private async _handleLLMResponse(llmOutput: LLMResponse | null) {
    if (!llmOutput) return;

    switch (llmOutput.type) {
      case "message_content_delta": {
        const { delta, content: text } = llmOutput;
        this.currentText = text ?? null;
        if (delta && this.onResponse) {
          await this.onResponse(this.env, delta, text ?? "", false);
        }
        if (text) await this._writeContext("assistant", text);
        break;
      }

      case "message_content_done": {
        const { content: text } = llmOutput;
        this.currentText = null;
        if (this.onResponse && text) {
          await this.onResponse(this.env, "", text, true);
        }
        break;
      }

      case "message_reasoning_delta": {
        const { delta, content: text } = llmOutput;
        if (delta && this.onReasoningResponse) {
          await this.onReasoningResponse(this.env, delta, text ?? "", false);
        }
        break;
      }

      case "message_reasoning_done": {
        const { content: text } = llmOutput;
        if (this.onReasoningResponse && text) {
          await this.onReasoningResponse(this.env, "", text, true);
        }
        break;
      }

      case "tool_call_content": {
        const { name, tool_call_id, arguments: args } = llmOutput;

        this.env.logInfo(`_handleLLMResponse: invoking tool ${name}`);
        const srcExtensionName = this.toolRegistry[name];
        if (!srcExtensionName) {
          this.env.logError(`No tool registered for "${name}"`);
          break;
        }

        const [result] = await sendCmd(
          this.env,
          "tool_call",
          srcExtensionName,
          {
            name,
            arguments: args ?? {}, // ensure we pass an object to the tool
          }
        );

        if (result?.getStatusCode() !== StatusCode.OK) {
          this.env.logError("Tool call failed");
          break;
        }

        const [r] = result.getPropertyToJson("result") || [];
        let toolResult: LLMToolResult;
        try {
          toolResult = JSON.parse(r) as LLMToolResult;
        } catch {
          this.env.logError(`Invalid tool result JSON: ${r}`);
          break;
        }

        this.env.logInfo(`tool_result: ${JSON.stringify(toolResult)}`);

        // Construct a function_call message as a plain object
        const contextFunctionCall: LLMMessageFunctionCall = {
          type: "function_call",
          id: (llmOutput as any).id, // provided by tool_call_content response
          call_id: tool_call_id,
          name,
          arguments: JSON.stringify(args ?? {}),
          role: "assistant",
        };

        if (toolResult.type === "llmresult") {
          const resultContent = toolResult.content;
          if (typeof resultContent === "string") {
            await this._queueContext(contextFunctionCall);

            // Feed a function_call_output message back to the LLM
            const fnCallOutput: LLMMessageFunctionCallOutput = {
              type: "function_call_output",
              call_id: tool_call_id,
              output: resultContent,
              role: "tool",
            };

            await this._sendToLLM(fnCallOutput);
          } else {
            // If you want to support array-of-parts, extend the schema or serialize here
            this.env.logError(
              `Unsupported tool result content: ${JSON.stringify(resultContent)}`
            );
          }
        } else if (toolResult.type === "requery") {
          // TODO: implement your requery strategy (e.g., enqueue a follow-up user message)
        }

        break;
      }
    }
  }
}
