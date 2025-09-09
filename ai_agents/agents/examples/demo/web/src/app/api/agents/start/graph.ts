import { LanguageMap } from "@/common/constant";
import { IOceanBaseSettings } from "@/types";

const OPENAI_REALTIME_MODEL = "gpt-realtime"

export const voiceNameMap: LanguageMap = {
    "zh-CN": {
        azure: {
            male: "zh-CN-YunyiMultilingualNeural",
            female: "zh-CN-XiaoxiaoMultilingualNeural",
        },
        elevenlabs: {
            male: "pNInz6obpgDQGcFmaJgB", // Adam
            female: "Xb7hH8MSUJpSbSDYk0k2", // Alice
        },
        polly: {
            male: "Zhiyu",
            female: "Zhiyu",
            langCode: "cmn-CN",
            langEngine: "neural"
        },
        openai: {
            male: "ash",
            female: "shimmer"
        },
        gemini: {
            male: "Charon",
            female: "Aoede",
            langCode: "cmn-CN",
        },
        azure_grok4: {
            male: "zh-CN-YunyiMultilingualNeural",
            female: "zh-CN-XiaoxiaoMultilingualNeural",
        }
    },
    "en-US": {
        azure: {
            male: "en-US-AndrewMultilingualNeural",
            female: "en-US-AvaMultilingualNeural",
        },
        elevenlabs: {
            male: "pNInz6obpgDQGcFmaJgB", // Adam
            female: "Xb7hH8MSUJpSbSDYk0k2", // Alice
        },
        polly: {
            male: "Matthew",
            female: "Ruth",
            langCode: "en-US",
            langEngine: "generative"
        },
        openai: {
            male: "ash",
            female: "shimmer"
        },
        gemini: {
            male: "Charon",
            female: "Aoede",
            langCode: "en-US",
        },
        azure_grok4: {
            male: "en-US-AndrewMultilingualNeural",
            female: "en-US-AvaMultilingualNeural",
        }
    },
    "ja-JP": {
        azure: {
            male: "ja-JP-KeitaNeural",
            female: "ja-JP-NanamiNeural",
        },
        openai: {
            male: "ash",
            female: "shimmer"
        },
        gemini: {
            male: "Charon",
            female: "Aoede",
            langCode: "ja-JP",
        },
        azure_grok4: {
            male: "ja-JP-KeitaNeural",
            female: "ja-JP-NanamiNeural",
        }
    },
    "ko-KR": {
        azure: {
            male: "ko-KR-InJoonNeural",
            female: "ko-KR-JiMinNeural",
        },
        openai: {
            male: "ash",
            female: "shimmer"
        },
        gemini: {
            male: "Charon",
            female: "Aoede",
            langCode: "ko-KR",
        },
        azure_grok4: {
            male: "ko-KR-InJoonNeural",
            female: "ko-KR-JiMinNeural",
        }
    },
};

export const convertLanguage = (language: string) => {
    if (language === "zh-CN") {
        return "zh";
    } else if (language === "en-US") {
        return "en";
    } else if (language === "ja-JP") {
        return "ja";
    } else if (language === "ko-KR") {
        return "ko";
    }
    return "en";
}

// Get the graph properties based on the graph name, language, and voice type
// This is the place where you can customize the properties for different graphs to override default property.json
export const getGraphProperties = (
    graphName: string,
    language: string,
    voiceType: string,
    prompt: string | undefined,
    greeting: string | undefined,
    oceanbaseSettings: IOceanBaseSettings | undefined
) => {
    let localizationOptions = {
        "greeting": "Hey, I\'m TEN Agent, I can speak, see, and reason from a knowledge base, ask me anything!",
        "checking_vision_text_items": "[\"Let me take a look...\",\"Let me check your camera...\",\"Please wait for a second...\"]",
        "coze_greeting": "Hey, I'm Coze Bot, I can chat with you, ask me anything!",
    }

    if (language === "zh-CN") {
        localizationOptions = {
            "greeting": "嗨，我是 TEN Agent，我可以说话、看东西，还能从知识库中推理，问我任何问题吧！",
            "checking_vision_text_items": "[\"让我看看你的摄像头...\",\"让我看一下...\",\"我看一下，请稍候...\"]",
            "coze_greeting": "嗨，我是扣子机器人，我可以和你聊天，问我任何问题吧！",
        }
    } else if (language === "ja-JP") {
        localizationOptions = {
            "greeting": "こんにちは、TEN Agentです。私は話したり、見たり、知識ベースから推論したりできます。何でも聞いてください！",
            "checking_vision_text_items": "[\"ちょっと見てみます...\",\"カメラをチェックします...\",\"少々お待ちください...\"]",
            "coze_greeting": "こんにちは、私はCoze Botです。お話しできますので、何でも聞いてください！",
        }
    } else if (language === "ko-KR") {
        localizationOptions = {
            "greeting": "안녕하세요, 저는 TEN Agent입니다. 말하고, 보고, 지식 베이스에서 추론할 수 있어요. 무엇이든 물어보세요!",
            "checking_vision_text_items": "[\"조금만 기다려 주세요...\",\"카메라를 확인해 보겠습니다...\",\"잠시만 기다려 주세요...\"]",
            "coze_greeting": "안녕하세요, 저는 Coze Bot입니다. 대화할 수 있어요. 무엇이든 물어보세요!",
        }
    }

    let combined_greeting = greeting || localizationOptions["greeting"];
    let converteLanguage = convertLanguage(language);

    if (graphName === "va_coze_azure") {
        combined_greeting = greeting || localizationOptions["coze_greeting"];
        return {
            "stt": {
                "params": {
                    "language": language
                },
            },
            "main_control": {
                "greeting": combined_greeting
            },
            "llm": {
                "prompt": prompt,
            },
            "tts": {
                "params": {
                    "propertys": [
                        ["SpeechServiceConnection_SynthVoice", voiceNameMap[language]["azure"][voiceType]]
                    ]
                }
            }
        }
    } else if (graphName === "va_openai_v2v") {
        return {
            "v2v": {
                "model": OPENAI_REALTIME_MODEL,
                "voice": voiceNameMap[language]["openai"][voiceType],
                "language": converteLanguage,
                "prompt": prompt,
            },
            "main_control": {
                "greeting": combined_greeting
            }
        }
    } else if (graphName === "va_openai_azure") {
        return {
            "stt": {
                "params": {
                    "language": language
                },
            },
            "llm": {
                "model": "gpt-4o",
                "prompt": prompt,
            },
            "main_control": {
                "greeting": combined_greeting
            },
            "tts": {
                "params": {
                    "propertys": [
                        ["SpeechServiceConnection_SynthVoice", voiceNameMap[language]["azure"][voiceType]]
                    ]
                }
            }
        }
    } else if (graphName === "va_gemini_v2v") {
        return {
            "v2v": {
                "prompt": prompt,
                "language": voiceNameMap[language]["gemini"]["langCode"],
                "voice": voiceNameMap[language]["gemini"][voiceType],
                // "greeting": combined_greeting,
            }
        }
    } else if (graphName === "va_gemini_v2v_native") {
        return {
            "v2v": {
                "prompt": prompt,
                "voice": voiceNameMap[language]["gemini"][voiceType],
                // "greeting": combined_greeting,
            }
        }
    } else if (graphName === "va_dify_azure") {
        return {
            "stt": {
                "params": {
                    "language": language
                },
            },
            "llm": {
                "prompt": prompt,
            },
            "main_control": {
                "greeting": combined_greeting
            },
            "tts": {
                "params": {
                    "propertys": [
                        ["SpeechServiceConnection_SynthVoice", voiceNameMap[language]["azure"][voiceType]]
                    ]
                }
            }
        }
    } else if (graphName === "deepseek_v3_1") {
        return {
            "stt": {
                "params": {
                    "language": language
                },
            },
            "llm": {
                "prompt": prompt,
                "greeting": combined_greeting,
                "model": "deepseek-chat-v3.1",
            },
            "main_control": {
                "greeting": combined_greeting
            },
            "tts": {
                "params": {
                    "propertys": [
                        ["SpeechServiceConnection_SynthVoice", voiceNameMap[language]["azure"][voiceType]]
                    ]
                }
            }
        }
    } else if (graphName === "qwen3") {
        return {
            "stt": {
                "params": {
                    "language": language
                },
            },
            "llm": {
                "prompt": prompt,
                "model": "qwq-plus",
            },
            "main_control": {
                "greeting": combined_greeting
            },
            "tts": {
                "params": {
                    "propertys": [
                        ["SpeechServiceConnection_SynthVoice", voiceNameMap[language]["azure"][voiceType]]
                    ]
                }
            }
        }
    } else if (graphName === "grok4") {
        // Grok4 specific greetings for different languages
        let grok4_greeting = "Hey, I'm Annie, you look like trouble. What’s your story?";

        if (language === "zh-CN") {
            grok4_greeting = "嗨，我是安妮，你看起来不太乖，说说你是什么来头？";
        } else if (language === "ja-JP") {
            grok4_greeting = "こんにちは、私はアンです。あなた、どんな人？";
        } else if (language === "ko-KR") {
            grok4_greeting = "안녕하세요, 저는 앤입니다. 네 얘기 좀 해봐.";
        }

        combined_greeting = greeting || grok4_greeting;

        return {
            "stt": {
                "params": {
                    "language": language
                },
            },
            "llm": {
                "prompt": prompt,
                "model": "grok-4-0709",
            },
            "main_control": {
                "greeting": combined_greeting
            },
            "tts": {
                "params": {
                    "propertys": [
                        ["SpeechServiceConnection_SynthVoice", voiceNameMap[language]["azure_grok4"][voiceType]]
                    ]
                }
            }
        }
    } else if (graphName === "va_llama4") {
        return {
            "stt": {
                "params": {
                    "language": language
                },
            },
            "llm": {
                "prompt": prompt,
            },
            "main_control": {
                "greeting": combined_greeting
            },
            "tts": {
                "params": {
                    "propertys": [
                        ["SpeechServiceConnection_SynthVoice", voiceNameMap[language]["azure"][voiceType]]
                    ]
                }
            }
        }
    // Note: duplicate grok4 branch removed for clarity; handled above
    } else if (graphName === "va_azure_v2v") {
        return {
            "v2v": {
                "model": "gpt-4o",
                "voice_name": voiceNameMap[language]["azure"][voiceType],
                "language": voiceNameMap[language]["azure"]["langCode"] || language,
                "prompt": prompt,
            },
            "main_control": {
                "greeting": combined_greeting
            }
        }
    } else if (graphName === "va_oceanbase_rag") {
        return {
            "stt": {
                "params": {
                    "language": language
                },
            },
            "llm": {
                "api_key": oceanbaseSettings?.api_key,
                "base_url": oceanbaseSettings?.base_url,
                "ai_database_name": oceanbaseSettings?.db_name,
                "collection_id": oceanbaseSettings?.collection_id
            },
            "main_control": {
                "greeting": combined_greeting
            },
            "tts": {
                "params": {
                    "propertys": [
                        ["SpeechServiceConnection_SynthVoice", voiceNameMap[language]["azure"][voiceType]]
                    ]
                }
            }
        }
    }

    return {}
}
