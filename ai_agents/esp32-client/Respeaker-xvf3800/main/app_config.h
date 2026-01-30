#pragma once


//LLM Agent Service (请替换为您的 TEN 服务器地址)
#define TENAI_AGENT_URL       "http://18.143.78.135:8080"  // 示例: 修改为您的服务器 IP 和端口

// LLM Agent Graph, you can select openai or gemini or voice_assistant
// #define CONFIG_GRAPH_OPENAI   /* openai realtime API, just only audio */
// #define CONFIG_GRAPH_GEMINI     /* gemini, for video and audio, but not support chinese language */
#define CONFIG_GRAPH_VOICE_ASSISTANT  /* standard voice assistant (ASR + LLM + TTS) */

/* greeting */
#define GREETING               "Can I help You?"
#define PROMPT                 ""

/* different settings for different agent graph */
#if defined(CONFIG_GRAPH_OPENAI)
#define GRAPH_NAME             "va_openai_v2v"

#define V2V_MODEL              "gpt-realtime"
#define LANGUAGE               "en-US"
#define VOICE                  "ash"
#elif defined(CONFIG_GRAPH_GEMINI)
#define GRAPH_NAME             "va_gemini_v2v"
#elif defined(CONFIG_GRAPH_VOICE_ASSISTANT)
#define GRAPH_NAME             "voice_assistant"
#else
#error "not config graph for aiAgent"
#endif

// LLM Agent Task Name
#define AI_AGENT_NAME          "tenai0125-11"
// LLM Agent Channel Name
#define AI_AGENT_CHANNEL_NAME  "aiAgent_chn0124-11"
// LLM User Id
#define AI_AGENT_USER_ID        12345 // user id, for device



/* function config */
/* audio codec */
#define CONFIG_USE_G711U_CODEC
/* video process */
#define CONFIG_AUDIO_ONLY
