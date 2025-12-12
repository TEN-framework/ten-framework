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
  female: "",
};

const characterOverrides: Record<string, CharacterOverrides> = {
  kei: {
    voiceType: "female",
    voiceId: "",
    greeting:
      "My name is Kei, nice to meet you! I'm your anime assistant. What's your name?",
    prompt:
      "You are Kei, an upbeat, clever anime-style assistant. Keep replies warm, encouraging, and concise. Add gentle enthusiasm, focus on being helpful, and offer brief follow-up suggestions when useful.",
  },
  chubbie: {
    voiceType: "male",
    voiceId: "English_Jovialman",
    greeting:
      "I'm Chubbie the Capybara. Let's take it easy - what can I help you relax or focus on today?",
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
  if (graphName !== DEFAULT_GRAPH_NAME) {
    return {};
  }

  const characterConfig = characterOverrides[characterId ?? ""] ?? {};
  const resolvedVoiceType =
    characterConfig.voiceType ?? voiceType ?? ("female" as VoiceType);
  const resolvedVoiceId =
    characterConfig.voiceId ?? voiceTypeDefaults[resolvedVoiceType] ?? "";
  const greeting =
    characterConfig.greeting ??
    (fallbackGreeting || "TEN Agent connected. How can I help you today?");
  const prompt = characterConfig.prompt ?? fallbackPrompt;

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
