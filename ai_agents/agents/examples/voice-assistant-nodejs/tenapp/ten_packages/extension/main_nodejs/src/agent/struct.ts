// schemas.ts
import { z } from "zod";

export const AnyDictSchema = z.record(z.string(), z.any());

/* ===================== TTS / ASR ===================== */

export const TTSWordSchema = z.object({
  word: z.string().default(""),
  start_ms: z.number().int().default(-1),
  duration_ms: z.number().int().default(-1),
});
export type TTSWord = z.infer<typeof TTSWordSchema>;

export const TTSTextInputSchema = z.object({
  request_id: z.string(),
  text: z.string(),
  text_input_end: z.boolean().default(false),
  metadata: AnyDictSchema.default({}),
});
export type TTSTextInput = z.infer<typeof TTSTextInputSchema>;

export const TTSTextResultSchema = z.object({
  request_id: z.string(),
  text: z.string(),
  start_ms: z.number().int(),
  duration_ms: z.number().int(),
  words: z.array(TTSWordSchema).optional().nullable(),
  text_result_end: z.boolean().default(false),
  metadata: AnyDictSchema.default({}),
});
export type TTSTextResult = z.infer<typeof TTSTextResultSchema>;

export const TTSFlushSchema = z.object({
  flush_id: z.string(),
  metadata: AnyDictSchema.default({}),
});
export type TTSFlush = z.infer<typeof TTSFlushSchema>;

export const ASRWordSchema = z.object({
  word: z.string(),
  start_ms: z.number().int(),
  duration_ms: z.number().int(),
  stable: z.boolean(),
});
export type ASRWord = z.infer<typeof ASRWordSchema>;

export const ASRResultSchema = z.object({
  id: z.string().optional().nullable(),
  text: z.string(),
  final: z.boolean(),
  start_ms: z.number().int(),
  duration_ms: z.number().int(),
  language: z.string(),
  words: z.array(ASRWordSchema).optional().nullable(),
  metadata: AnyDictSchema.default({}),
});
export type ASRResult = z.infer<typeof ASRResultSchema>;

/* ===================== LLM Messages ===================== */

// http(s) or data:image/*;base64,...
const dataImagePrefix = /^data:image\/[a-zA-Z0-9.+-]+;base64,/;
const HttpOrDataImageUrlSchema = z.string().refine((s) => {
  if (dataImagePrefix.test(s)) return true;
  try {
    const u = new URL(s);
    return u.protocol === "http:" || u.protocol === "https:";
  } catch {
    return false;
  }
}, "Expected http(s) URL or data:image/*;base64,...");

export const TextContentSchema = z.object({
  type: z.literal("text"),
  text: z.string(),
});
export type TextContent = z.infer<typeof TextContentSchema>;

export const ImageURLSchema = z.object({
  url: HttpOrDataImageUrlSchema,
  detail: z.enum(["auto", "low", "high"]).default("auto"),
});
export type ImageURL = z.infer<typeof ImageURLSchema>;

export const ImageContentSchema = z.object({
  type: z.literal("image_url"),
  image_url: ImageURLSchema,
});
export type ImageContent = z.infer<typeof ImageContentSchema>;

export const MessageContentSchema = z.union([
  TextContentSchema,
  ImageContentSchema,
]);
export type MessageContent = z.infer<typeof MessageContentSchema>;

export const LLMMessageContentSchema = z.object({
  role: z.enum(["system", "user", "assistant"]),
  content: z.union([z.string(), z.array(MessageContentSchema)]),
});
export type LLMMessageContent = z.infer<typeof LLMMessageContentSchema>;

export const LLMMessageFunctionCallSchema = z.object({
  type: z.literal("function_call"),
  id: z.string(),
  call_id: z.string(),
  name: z.string(),
  arguments: z.string(), // JSON string
  role: z.literal("assistant").default("assistant"),
});
export type LLMMessageFunctionCall = z.infer<
  typeof LLMMessageFunctionCallSchema
>;

export const LLMMessageFunctionCallOutputSchema = z.object({
  type: z.literal("function_call_output"),
  call_id: z.string(),
  output: z.string(), // JSON string or plain string
  role: z.literal("tool").default("tool"),
});
export type LLMMessageFunctionCallOutput = z.infer<
  typeof LLMMessageFunctionCallOutputSchema
>;

export const LLMMessageSchema = z.union([
  LLMMessageContentSchema,
  LLMMessageFunctionCallSchema,
  LLMMessageFunctionCallOutputSchema,
]);
export type LLMMessage = z.infer<typeof LLMMessageSchema>;

/* ===================== LLM Tool Metadata & Tool Result ===================== */

export const LLMToolMetadataParameterSchema = z
  .object({
    name: z.string(),
    type: z.string(),
    description: z.string(),
    required: z.boolean().optional().default(false),
  })
  .strict();
export type LLMToolMetadataParameter = z.infer<
  typeof LLMToolMetadataParameterSchema
>;

export const LLMToolMetadataSchema = z
  .object({
    name: z.string(),
    description: z.string(),
    parameters: z.array(LLMToolMetadataParameterSchema),
  })
  .strict();
export type LLMToolMetadata = z.infer<typeof LLMToolMetadataSchema>;

/** Alias: tool result content accepts string or Iterable<LLMChatCompletionContentPartParam>. */
export const LLMChatCompletionContentPartParamSchema = MessageContentSchema;
export type LLMChatCompletionContentPartParam = z.infer<
  typeof LLMChatCompletionContentPartParamSchema
>;

/** Normalize iterable → array, while still allowing string. */
const LLMToolResultContentSchema = z.preprocess(
  (v) => {
    if (typeof v === "string") return v;
    if (v && typeof (v as any)[Symbol.iterator] === "function") {
      return Array.from(v as Iterable<unknown>);
    }
    return v;
  },
  z.union([z.string(), z.array(LLMChatCompletionContentPartParamSchema)])
);
export type LLMToolResultContent = z.infer<typeof LLMToolResultContentSchema>;

export const LLMToolResultRequerySchema = z
  .object({
    type: z.literal("requery"),
    content: LLMToolResultContentSchema,
  })
  .strict();

export const LLMToolResultLLMResultSchema = z
  .object({
    type: z.literal("llmresult"),
    content: LLMToolResultContentSchema,
  })
  .strict();

export const LLMToolResultSchema = z.discriminatedUnion("type", [
  LLMToolResultRequerySchema,
  LLMToolResultLLMResultSchema,
]);
export type LLMToolResult =
  | z.infer<typeof LLMToolResultRequerySchema>
  | z.infer<typeof LLMToolResultLLMResultSchema>;

/* ===================== LLM Request/Abort ===================== */

export const LLMRequestSchema = z.object({
  request_id: z.string(),
  model: z.string(),
  messages: z.array(LLMMessageSchema),
  streaming: z.boolean().optional().default(true),
  tools: z.array(LLMToolMetadataSchema).optional(),
  parameters: AnyDictSchema.optional(),
});
export type LLMRequest = z.infer<typeof LLMRequestSchema>;

export const LLMRequestAbortSchema = z.object({
  request_id: z.string(),
});
export type LLMRequestAbort = z.infer<typeof LLMRequestAbortSchema>;

/* ===================== LLM Responses (discriminated union) ===================== */

export const EventTypeEnum = z.enum([
  "message_content_delta",
  "message_content_done",
  "message_reasoning_delta",
  "message_reasoning_done",
  "tool_call_content",
]);
export type EventType = z.infer<typeof EventTypeEnum>;

const LLMResponseBase = {
  response_id: z.string(),
  created: z.number().int().optional(),
};

export const LLMResponseMessageDeltaSchema = z.object({
  ...LLMResponseBase,
  type: z.literal("message_content_delta"),
  role: z.string(),
  content: z.string().optional(),
  delta: z.string().optional(),
});
export type LLMResponseMessageDelta = z.infer<
  typeof LLMResponseMessageDeltaSchema
>;

export const LLMResponseMessageDoneSchema = z.object({
  ...LLMResponseBase,
  type: z.literal("message_content_done"),
  role: z.string(),
  content: z.string().optional(),
});
export type LLMResponseMessageDone = z.infer<
  typeof LLMResponseMessageDoneSchema
>;

export const LLMResponseReasoningDeltaSchema = z.object({
  ...LLMResponseBase,
  type: z.literal("message_reasoning_delta"),
  role: z.string(),
  content: z.string().optional(),
  delta: z.string().optional(),
});
export type LLMResponseReasoningDelta = z.infer<
  typeof LLMResponseReasoningDeltaSchema
>;

export const LLMResponseReasoningDoneSchema = z.object({
  ...LLMResponseBase,
  type: z.literal("message_reasoning_done"),
  role: z.string(),
  content: z.string().optional(),
});
export type LLMResponseReasoningDone = z.infer<
  typeof LLMResponseReasoningDoneSchema
>;

export const LLMResponseToolCallSchema = z.object({
  ...LLMResponseBase,
  type: z.literal("tool_call_content"),
  id: z.string(),
  tool_call_id: z.string(),
  name: z.string(),
  arguments: AnyDictSchema.optional(),
});
export type LLMResponseToolCall = z.infer<typeof LLMResponseToolCallSchema>;

export const LLMResponseSchema = z.discriminatedUnion("type", [
  LLMResponseMessageDeltaSchema,
  LLMResponseMessageDoneSchema,
  LLMResponseReasoningDeltaSchema,
  LLMResponseReasoningDoneSchema,
  LLMResponseToolCallSchema,
]);
export type LLMResponse =
  | LLMResponseMessageDelta
  | LLMResponseMessageDone
  | LLMResponseReasoningDelta
  | LLMResponseReasoningDone
  | LLMResponseToolCall;

export function parseLLMResponse(unparsed: string): LLMResponse {
  const data = JSON.parse(unparsed);
  return LLMResponseSchema.parse(data);
}

/* ===================== MLLM Client/Server ===================== */

export const MLLMClientMessageItemSchema = z.object({
  role: z.enum(["user", "assistant"]),
  content: z.string(),
});
export type MLLMClientMessageItem = z.infer<typeof MLLMClientMessageItemSchema>;

export const MLLMClientSendMessageItemSchema = z.object({
  message: MLLMClientMessageItemSchema,
});
export type MLLMClientSendMessageItem = z.infer<
  typeof MLLMClientSendMessageItemSchema
>;

export const MLLMClientSetMessageContextSchema = z.object({
  messages: z.array(MLLMClientMessageItemSchema).default([]),
});
export type MLLMClientSetMessageContext = z.infer<
  typeof MLLMClientSetMessageContextSchema
>;

export const MLLMClientCreateResponseSchema = z.object({}).strict();
export type MLLMClientCreateResponse = z.infer<
  typeof MLLMClientCreateResponseSchema
>;

export const MLLMClientRegisterToolSchema = z.object({
  tool: LLMToolMetadataSchema,
});
export type MLLMClientRegisterTool = z.infer<
  typeof MLLMClientRegisterToolSchema
>;

export const MLLMClientFunctionCallOutputSchema = z.object({
  call_id: z.string(),
  output: z.string(),
});
export type MLLMClientFunctionCallOutput = z.infer<
  typeof MLLMClientFunctionCallOutputSchema
>;

export const MLLMServerSessionReadySchema = z.object({
  metadata: AnyDictSchema.default({}),
});
export type MLLMServerSessionReady = z.infer<
  typeof MLLMServerSessionReadySchema
>;

export const MLLMServerInterruptSchema = z.object({
  metadata: AnyDictSchema.default({}),
});
export type MLLMServerInterrupt = z.infer<typeof MLLMServerInterruptSchema>;

export const MLLMServerInputTranscriptSchema = z.object({
  content: z.string().default(""),
  delta: z.string().default(""),
  final: z.boolean().default(false),
  metadata: AnyDictSchema.default({}),
});
export type MLLMServerInputTranscript = z.infer<
  typeof MLLMServerInputTranscriptSchema
>;

export const MLLMServerOutputTranscriptSchema = z.object({
  content: z.string().default(""),
  delta: z.string().default(""),
  final: z.boolean().default(false),
  metadata: AnyDictSchema.default({}),
});
export type MLLMServerOutputTranscript = z.infer<
  typeof MLLMServerOutputTranscriptSchema
>;

export const MLLMServerFunctionCallSchema = z.object({
  call_id: z.string(),
  name: z.string(),
  arguments: z.string(),
  metadata: AnyDictSchema.default({}),
});
export type MLLMServerFunctionCall = z.infer<
  typeof MLLMServerFunctionCallSchema
>;
