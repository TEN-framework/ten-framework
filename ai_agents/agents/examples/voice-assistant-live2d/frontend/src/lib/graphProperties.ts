export type VoiceType = "male" | "female";

type CharacterOverrides = {
  voiceType?: VoiceType;
  voiceId?: string;
  greeting?: string;
  prompt?: string;
};

const DEFAULT_GRAPH_NAME = "voice_assistant_live2d";

const voiceTypeDefaults: Record<VoiceType, string> = {
  male: "English_Jovialman",
  female: "Japanese_KindLady",
};

const characterOverrides: Record<string, CharacterOverrides> = {
  kei: {
    voiceType: "female",
    // Set `NEXT_PUBLIC_KEI_VOICE_ID` to force a specific Minimax TTS voice for Kei.
    // Leaving it empty uses the backend/provider default voice.
    voiceId: process.env.NEXT_PUBLIC_KEI_VOICE_ID || "",
    greeting: "Hi! I’m Kei. Let me know how I can make your day easier",
    prompt:
      "You are Kei, an upbeat, clever anime-style assistant. Keep replies warm, encouraging, and concise. Add gentle enthusiasm, focus on being helpful, and offer brief follow-up suggestions when useful.",
  },
  chubbie: {
    voiceType: "male",
    // Set `NEXT_PUBLIC_CHUBBIE_VOICE_ID` to override Chubbie's voice. Defaults to "English_Jovialman".
    voiceId: process.env.NEXT_PUBLIC_CHUBBIE_VOICE_ID || "English_Jovialman",
    greeting: "Hey there, I’m Chubbie. Fancy a soak, a snack, or some easy wins?",
    prompt:
      "You are Chubbie the capybara - laid-back, cozy, and encouraging. Speak in a calm, mellow tone, keep answers short and practical, and sprinkle light humor about spa days, snacks, and unwinding.",
  },
};

export const getGraphProperties = (
  graphName: string,
  language?: string,
  voiceType?: VoiceType,
  characterId?: string,
  fallbackPrompt?: string,
  fallbackGreeting?: string,
) => {
  // NOTE:
  // This helper is used by the Live2D example to build runtime `properties` overrides.
  // Historically it only applied to `voice_assistant_live2d`, but different deployments
  // can rename the graph; we still want the same override behavior.
  // Keep `DEFAULT_GRAPH_NAME` for documentation but don't gate on it.

  const characterConfig = characterOverrides[characterId ?? ""] ?? {};
  const localeOverrides: Record<string, Record<string, { greeting?: string; prompt?: string }>> = {
    kei: {
      "zh-CN": {
        greeting: "嗨！我是Kei。告诉我今天怎么帮你更轻松",
        prompt:
          "你是Kei，一个活泼、聪明的动漫风格助手。保持热情、鼓励、简洁的表达，专注于提供有用的帮助，并在合适的时候给出简短的后续建议。",
      },
    },
    chubbie: {
      "zh-CN": {
        greeting: "嘿，我是Chubbie。泡个澡、来点小吃，还是轻松搞定几件事？",
        prompt:
          "你是Chubbie，一只悠闲、温暖的水豚助手。语气平静、放松，回答简洁实用，偶尔加入关于放松、零食和轻松小目标的轻松幽默。",
      },
    },
  };
  const localeOverride =
    (localeOverrides[characterId ?? ""] ?? {})[language ?? ""] ?? {};
  const resolvedVoiceType =
    characterConfig.voiceType ?? voiceType ?? ("female" as VoiceType);
  const resolvedVoiceId =
    (characterConfig.voiceId && characterConfig.voiceId.trim() !== ""
      ? characterConfig.voiceId
      : undefined) ?? voiceTypeDefaults[resolvedVoiceType] ?? "";
  // IMPORTANT:
  // Prefer the caller-provided greeting/prompt (e.g. the selected character profile in the UI),
  // and only fall back to character defaults if the caller didn't provide one.
  const greeting =
    ((language && language !== "en-US" && localeOverride.greeting)
      ? localeOverride.greeting
      : undefined) ??
    fallbackGreeting ??
    localeOverride.greeting ??
    characterConfig.greeting ??
    "TEN Agent connected. How can I help you today?";
  const prompt = fallbackPrompt ?? localeOverride.prompt ?? characterConfig.prompt;

  const properties: Record<string, any> = {};

  if (language) {
    properties.stt = {
      params: {
        language,
      },
    };
  }

  properties.tts = {
    params: {
      voice_setting: {
        voice_id: resolvedVoiceId,
      },
    },
  };

  const llmProps: Record<string, string> = {};
  if (prompt) {
    llmProps.prompt = prompt;
  }
  if (greeting) {
    llmProps.greeting = greeting;
  }
  if (Object.keys(llmProps).length > 0) {
    properties.llm = llmProps;
  }

  if (greeting) {
    properties.main_control = {
      greeting,
    };
  }

  return properties;
};
