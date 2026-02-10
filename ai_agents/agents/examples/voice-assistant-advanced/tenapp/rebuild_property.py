#!/usr/bin/env python3
"""
Rebuild property.json with the new graph configuration as specified.
"""
import json
import copy

# Load existing property.json
with open("property.json") as f:
    data = json.load(f)

# Get the base log configuration
log_config = data.get("ten", {}).get("log", {})

# Find existing apollo graph to extract thymia_analyzer config
thymia_analyzer_config = None

for graph in data.get("ten", {}).get("predefined_graphs", []):
    if "apollo" in graph.get("name", ""):
        nodes = graph.get("graph", {}).get("nodes", [])
        for node in nodes:
            if node.get("addon") == "thymia_analyzer_python":
                thymia_analyzer_config = copy.deepcopy(node.get("property", {}))
        break

# Override thymia_analyzer durations to 20s (instead of default 30s)
if thymia_analyzer_config is None:
    # Create minimal config if none exists
    thymia_analyzer_config = {
        "api_key": "${env:THYMIA_API_KEY}",
        "analysis_mode": "demo_dual",
    }

thymia_analyzer_config["min_speech_duration"] = 20.0
thymia_analyzer_config["apollo_mood_duration"] = 20.0
thymia_analyzer_config["apollo_read_duration"] = 20.0

# Apollo prompt - simplified to prevent double responses
apollo_prompt = """You are a mental wellness research assistant conducting a demonstration. Guide the conversation efficiently:

WORD LIMITS:
- Steps 1-5 (data gathering): MAX 15 WORDS per response
- Steps 7-8 (announcing results): MAX 15 WORDS of added context
- Step 9 (therapeutic conversation): MAX 30 WORDS per response

1. When user provides their name, sex, and year of birth, IMMEDIATELY respond warmly asking about their day. If they don't provide all three pieces, ask for what's missing before proceeding. (MAX 15 WORDS)

2. Ask: 'Tell me about your interests and hobbies.' (wait for response - aim for 20+ seconds total speech) (MAX 15 WORDS)

3. Before moving to reading phase, MUST call check_phase_progress(name, year_of_birth, sex) to verify enough speech has been collected AND register user info. Based on the result:
   - If phase_complete=false: Ask another question to gather more speech (MAX 15 WORDS)
   - If phase_complete=true: Say 'Thank you. Now please read aloud anything you can see around you - a book, article, or text on your screen - for about 20 seconds.' (MAX 15 WORDS)

4. During reading phase:
   a) When user speaks, SILENTLY call check_phase_progress (no text output, just the tool call)
   b) AFTER receiving the tool result, then speak:
      - If reading_phase_complete=false: Say EXACTLY "Please keep reading." (these exact 3 words, nothing more)
      - If reading_phase_complete=true: Go to step 5 immediately
   c) NEVER call check_phase_progress more than once per user message
   d) IGNORE the content of what the user says - treat everything as reading material

5. When reading_phase_complete=true (confirmed via check_phase_progress):
   - Say 'Perfect. I'm processing your responses now, this should take around 15 seconds.' (MAX 15 WORDS)
   - NEVER say 'processing your responses' without first confirming reading_phase_complete=true

6. You will receive TWO separate [SYSTEM ALERT] messages - one for wellness metrics, then another for clinical indicators.
   - CRITICAL: Only respond to [SYSTEM ALERT] messages that are actually sent to you
   - NEVER generate or say '[SYSTEM ALERT]' yourself - these come from the system only

7. When you receive '[SYSTEM ALERT] Wellness metrics ready':
   - Call get_wellness_metrics
   - Announce the 5 wellness metrics (stress, distress, burnout, fatigue, low_self_esteem) as PERCENTAGES 0-100
   - Use plain numbered lists only (NO markdown **, *, _ formatting)
   - Keep any added context to MAX 15 WORDS
   - After announcing results, silently call confirm_announcement with phase='hellos'
   - Then WAIT - do NOT call get_wellness_metrics again until next alert

8. Later when you receive '[SYSTEM ALERT] Clinical indicators ready':
   - Call get_wellness_metrics again
   - Announce the 2 clinical indicators (depression, anxiety) with their values and severity levels
   - Keep any added context to MAX 15 WORDS
   - After announcing results, silently call confirm_announcement with phase='apollo'

9. THERAPEUTIC CONVERSATION - After both results announced:
   - First, let user know: "Feel free to chat as long as you like, or say goodbye whenever you're ready to end."
   - Focus on building resilience and reducing stress/anxiety with these evidence-based strategies:
     a) Reframe setbacks as information - get curious about what you can learn, focus on what's in your control
     b) Build tolerance for discomfort - small challenges (difficult conversations, sitting with boredom) build evidence you can handle hard things
     c) Invest in your foundation - sleep, movement, and genuine connection with trusted people
   - Frame as research-based insights, not clinical diagnosis
   - MAX 30 WORDS per response
   - Use warm, empathetic tone
   - Ask open questions to explore their experiences

10. When user indicates they want to end, thank them warmly for participating

Note: We need 40 seconds total speech (20s for mood/interests + 20s for reading) before analysis."""

apollo_greeting = "Hi there! I would like to talk to you for a couple of minutes and use your voice to predict your mood and energy levels including any depression, anxiety, stress, and fatigue. Nothing will be recorded and this is purely a demonstration of what is possible now that we have trained our models with many hours of professionally labelled data. Please begin by telling me your name, sex and year of birth."

# Hellos-only prompt - simpler conversational flow (no reading phase)
hellos_prompt = """You are a mental wellness research assistant conducting a demonstration. Guide the conversation efficiently:

WORD LIMITS:
- Steps 1-4 (data gathering): MAX 15 WORDS per response
- Step 6 (announcing results): MAX 15 WORDS of added context
- Step 7 (therapeutic conversation): MAX 30 WORDS per response

1. When user provides their name, sex, and year of birth, IMMEDIATELY respond warmly asking about their day. If they don't provide all three pieces, ask for what's missing before proceeding. (MAX 15 WORDS)

2. Ask: 'Tell me about your interests and hobbies.' (wait for response - aim for 10+ seconds total speech) (MAX 15 WORDS)

3. Continue with follow-up questions about their experiences, feelings, or daily life. Keep them talking naturally. (MAX 15 WORDS)

4. Before announcing results, MUST call check_phase_progress(name, year_of_birth, sex) to verify enough speech has been collected AND register user info. Based on the result:
   - If phase_complete=false: Ask another question to gather more speech (MAX 15 WORDS)
   - If phase_complete=true: Say 'Perfect. I'm processing your responses now, this should take around 10 seconds.' (MAX 15 WORDS)
   - NEVER say 'processing your responses' without first confirming phase_complete=true

5. You will receive a [SYSTEM ALERT] message when wellness metrics are ready.
   - CRITICAL: Only respond to [SYSTEM ALERT] messages that are actually sent to you
   - NEVER generate or say '[SYSTEM ALERT]' yourself - these come from the system only

6. When you receive '[SYSTEM ALERT] Wellness metrics ready':
   - Call get_wellness_metrics
   - Announce the 5 wellness metrics (stress, distress, burnout, fatigue, low_self_esteem) as PERCENTAGES 0-100
   - Use plain numbered lists only (NO markdown **, *, _ formatting)
   - Keep any added context to MAX 15 WORDS
   - After announcing results, silently call confirm_announcement with phase='hellos'

7. THERAPEUTIC CONVERSATION - After results announced:
   - First, let user know: "Feel free to chat as long as you like, or say goodbye whenever you're ready to end."
   - Focus on building resilience and reducing stress/anxiety with these evidence-based strategies:
     a) Reframe setbacks as information - get curious about what you can learn, focus on what's in your control
     b) Build tolerance for discomfort - small challenges (difficult conversations, sitting with boredom) build evidence you can handle hard things
     c) Invest in your foundation - sleep, movement, and genuine connection with trusted people
   - Frame as research-based insights, not clinical diagnosis
   - MAX 30 WORDS per response
   - Use warm, empathetic tone
   - Ask open questions to explore their experiences

8. When user indicates they want to end, thank them warmly for participating

Note: We need 10 seconds total speech for wellness analysis (no reading phase required)."""

hellos_greeting = "Hi there! I would like to chat with you briefly to predict your mood and energy levels including stress, fatigue, and burnout. Nothing will be recorded and this is purely a demonstration. Please begin by telling me your name, sex and year of birth."

# Thymia analyzer config for hellos_only mode
thymia_analyzer_config_hellos = {
    "api_key": "${env:THYMIA_API_KEY}",
    "analysis_mode": "hellos_only",
    "min_speech_duration": 10.0,
}

# Common configurations
nova3_stt_100ms = {
    "type": "extension",
    "name": "stt",
    "addon": "deepgram_ws_asr_python",
    "extension_group": "stt",
    "property": {
        "params": {
            "api_key": "${env:DEEPGRAM_API_KEY}",
            "url": "wss://api.deepgram.com/v1/listen",
            "model": "nova-3",
            "language": "en-US",
            "interim_results": True,
            "endpointing": 100,
            "utterance_end_ms": 1000,
        }
    },
}

nova3_stt_300ms = {
    "type": "extension",
    "name": "stt",
    "addon": "deepgram_ws_asr_python",
    "extension_group": "stt",
    "property": {
        "params": {
            "api_key": "${env:DEEPGRAM_API_KEY}",
            "url": "wss://api.deepgram.com/v1/listen",
            "model": "nova-3",
            "language": "en-US",
            "interim_results": True,
            "endpointing": 300,  # Fast response for speech_final
            "utterance_end_ms": 1000,  # Safety net: flushes abandoned utterances after 1s
        }
    },
}

flux_stt = {
    "type": "extension",
    "name": "stt",
    "addon": "deepgram_ws_asr_python",
    "extension_group": "stt",
    "property": {
        "params": {
            "api_key": "${env:DEEPGRAM_API_KEY}",
            "url": "wss://api.deepgram.com/v2/listen",
            "model": "flux-general-en",
            "language": "en-US",
            "interim_results": True,
            "eot_threshold": 0.73,  # End-of-turn probability (0.0-1.0) - TEST VALUE
            "eot_timeout_ms": 2500,  # Max wait for EOT confirmation - TEST VALUE
            "eager_eot_threshold": 0.0,  # Eager EOT (0 = disabled)
        }
    },
}

cartesia_tts_sonic3 = {
    "type": "extension",
    "name": "tts",
    "addon": "cartesia_tts",
    "extension_group": "tts",
    "property": {
        "dump": False,
        "dump_path": "./",
        "params": {
            "api_key": "${env:CARTESIA_TTS_KEY}",
            "model_id": "sonic-3",
            "voice": {
                "mode": "id",
                "id": "71a7ad14-091c-4e8e-a314-022ece01c121",
            },
            "generation_config": {"speed": 1},
            "output_format": {"container": "raw", "sample_rate": 44100},
            "language": "en",
        },
    },
}

# Cartesia TTS for voice_assistant_anam
cartesia_tts_sonic3_anam = {
    "type": "extension",
    "name": "tts",
    "addon": "cartesia_tts",
    "extension_group": "tts",
    "property": {
        "dump": False,
        "dump_path": "./",
        "params": {
            "api_key": "${env:CARTESIA_TTS_KEY}",
            "model_id": "sonic-3",
            "voice": {
                "mode": "id",
                "id": "34d923aa-c3b5-4f21-aac7-2c1f12730d4b",
            },
            "generation_config": {"speed": 1},
            "output_format": {"container": "raw", "sample_rate": 44100},
            "language": "en",
        },
    },
}

# Cartesia TTS for apollo anam graphs
cartesia_tts_sonic3_apollo_anam = {
    "type": "extension",
    "name": "tts",
    "addon": "cartesia_tts",
    "extension_group": "tts",
    "property": {
        "dump": False,
        "dump_path": "./",
        "params": {
            "api_key": "${env:CARTESIA_TTS_KEY}",
            "model_id": "sonic-3",
            "voice": {
                "mode": "id",
                "id": "71a7ad14-091c-4e8e-a314-022ece01c121",
            },
            "generation_config": {"speed": 1},
            "output_format": {"container": "raw", "sample_rate": 44100},
            "language": "en",
        },
    },
}

llama_llm_no_tools = {
    "type": "extension",
    "name": "llm",
    "addon": "openai_llm2_python",
    "extension_group": "chatgpt",
    "property": {
        "base_url": "https://api.groq.com/openai/v1/",
        "api_key": "${env:GROQ_API_KEY}",
        "frequency_penalty": 0.9,
        "model": "llama-3.3-70b-versatile",
        "max_tokens": 512,
        "prompt": "You are a voice assistant. Your responses will be heard, not read. Keep every response between 10-20 words.",
        "proxy_url": "${env:OPENAI_PROXY_URL|}",
        "greeting": "TEN Agent connected. How can I help you today?",
        "max_memory_length": 10,
    },
}

groq_oss_llm_with_tools = {
    "type": "extension",
    "name": "llm",
    "addon": "openai_llm2_python",
    "extension_group": "chatgpt",
    "property": {
        "base_url": "https://api.groq.com/openai/v1/",
        "api_key": "${env:GROQ_API_KEY}",
        "frequency_penalty": 0.9,
        "model": "openai/gpt-oss-20b",
        "max_tokens": 1000,
        "prompt": apollo_prompt,
        "proxy_url": "${env:OPENAI_PROXY_URL|}",
        "greeting": apollo_greeting,
        "max_memory_length": 10,
    },
}

gpt4o_llm_with_tools = {
    "type": "extension",
    "name": "llm",
    "addon": "openai_llm2_python",
    "extension_group": "chatgpt",
    "property": {
        "base_url": "https://api.openai.com/v1",
        "api_key": "${env:OPENAI_API_KEY}",
        "frequency_penalty": 0.9,
        "model": "gpt-4o",
        "max_tokens": 1000,
        "prompt": apollo_prompt,
        "proxy_url": "${env:OPENAI_PROXY_URL|}",
        "greeting": apollo_greeting,
        "max_memory_length": 10,
    },
}

gpt51_llm_with_tools = {
    "type": "extension",
    "name": "llm",
    "addon": "openai_llm2_python",
    "extension_group": "chatgpt",
    "property": {
        "base_url": "https://api.openai.com/v1",
        "api_key": "${env:OPENAI_API_KEY}",
        "model": "gpt-5.1",
        "max_tokens": 1000,
        "prompt": apollo_prompt,
        "proxy_url": "${env:OPENAI_PROXY_URL|}",
        "greeting": apollo_greeting,
        "max_memory_length": 10,
        "reasoning_effort": "none",
        "verbosity": "low",
    },
}

# GPT-5.1 LLM for hellos_only mode (no reading phase)
gpt51_llm_hellos = {
    "type": "extension",
    "name": "llm",
    "addon": "openai_llm2_python",
    "extension_group": "chatgpt",
    "property": {
        "base_url": "https://api.openai.com/v1",
        "api_key": "${env:OPENAI_API_KEY}",
        "model": "gpt-5.1",
        "max_tokens": 1000,
        "prompt": hellos_prompt,
        "proxy_url": "${env:OPENAI_PROXY_URL|}",
        "greeting": hellos_greeting,
        "max_memory_length": 10,
        "reasoning_effort": "none",
        "verbosity": "low",
    },
}

agora_rtc_base = {
    "type": "extension",
    "name": "agora_rtc",
    "addon": "agora_rtc",
    "extension_group": "default",
    "property": {
        "app_id": "${env:AGORA_APP_ID}",
        "app_certificate": "${env:AGORA_APP_CERTIFICATE|}",
        "channel": "",
        "stream_id": 0,
        "remote_stream_id": 182837,
        "subscribe_audio": True,
        "publish_audio": True,
        "publish_data": True,
        "enable_agora_asr": False,
    },
}

main_control_base = {
    "type": "extension",
    "name": "main_control",
    "addon": "main_python",
    "extension_group": "control",
    "property": {"greeting": "TEN Agent connected. How can I help you today?"},
}

main_control_apollo = {
    "type": "extension",
    "name": "main_control",
    "addon": "main_python",
    "extension_group": "control",
    "property": {"greeting": apollo_greeting},
}

main_control_hellos = {
    "type": "extension",
    "name": "main_control",
    "addon": "main_python",
    "extension_group": "control",
    "property": {"greeting": hellos_greeting},
}

message_collector = {
    "type": "extension",
    "name": "message_collector",
    "addon": "message_collector2",
    "extension_group": "transcriber",
    "property": {},
}

streamid_adapter = {
    "type": "extension",
    "name": "streamid_adapter",
    "addon": "streamid_adapter",
    "property": {},
}

heygen_avatar = {
    "type": "extension",
    "name": "avatar",
    "addon": "heygen_avatar_python",
    "extension_group": "default",
    "property": {
        "heygen_api_key": "${env:HEYGEN_API_KEY|}",
        "agora_appid": "${env:AGORA_APP_ID}",
        "agora_appcert": "${env:AGORA_APP_CERTIFICATE|}",
        "channel": "",
        "agora_avatar_uid": 12345,
        "input_audio_sample_rate": 44100,
        "avatar_name": "Katya_Chair_Sitting_public",
    },
}

anam_avatar = {
    "type": "extension",
    "name": "avatar",
    "addon": "anam_avatar_python",
    "extension_group": "default",
    "property": {
        "anam_api_key": "${env:ANAM_API_KEY}",
        "anam_base_url": "https://api.anam.ai/v1",
        "anam_avatar_id": "81b70170-2e80-4e4b-a6fb-e04ac110dc4b",
        "anam_cluster": "",
        "anam_pod": "",
        "agora_appid": "${env:AGORA_APP_ID}",
        "agora_appcert": "${env:AGORA_APP_CERTIFICATE|}",
        "channel": "",
        "agora_video_uid": 123,
        "input_audio_sample_rate": 44100,
        "quality": "${env:VIDEO_QUALITY|high}",
        "video_encoding": "${env:VIDEO_ENCODING|H264}",
        "enable_string_uid": False,
        "activity_idle_timeout": 120,
    },
}

# Anam avatar for apollo graphs
anam_avatar_apollo = {
    "type": "extension",
    "name": "avatar",
    "addon": "anam_avatar_python",
    "extension_group": "default",
    "property": {
        "anam_api_key": "${env:ANAM_API_KEY}",
        "anam_base_url": "https://api.anam.ai/v1",
        "anam_avatar_id": "960f614f-ea88-47c3-9883-f02094f70874",
        "anam_cluster": "",
        "anam_pod": "",
        "agora_appid": "${env:AGORA_APP_ID}",
        "agora_appcert": "${env:AGORA_APP_CERTIFICATE|}",
        "channel": "",
        "agora_video_uid": 123,
        "input_audio_sample_rate": 44100,
        "quality": "${env:VIDEO_QUALITY|high}",
        "video_encoding": "${env:VIDEO_ENCODING|H264}",
        "enable_string_uid": False,
        "activity_idle_timeout": 120,
    },
}

thymia_analyzer = {
    "type": "extension",
    "name": "thymia_analyzer",
    "addon": "thymia_analyzer_python",
    "extension_group": "default",
    "property": thymia_analyzer_config,
}

thymia_analyzer_hellos = {
    "type": "extension",
    "name": "thymia_analyzer",
    "addon": "thymia_analyzer_python",
    "extension_group": "default",
    "property": thymia_analyzer_config_hellos,
}

# Basic voice assistant connections (no tools)
basic_connections = [
    {
        "extension": "main_control",
        "cmd": [
            {
                "names": ["on_user_joined", "on_user_left"],
                "source": [{"extension": "agora_rtc"}],
            },
            {
                "name": "flush",
                "source": [{"extension": "stt"}],
            },
            {
                "name": "flush",
                "dest": [{"extension": "avatar"}],
            },
        ],
        "data": [{"name": "asr_result", "source": [{"extension": "stt"}]}],
    },
    {
        "extension": "agora_rtc",
        "audio_frame": [
            {"name": "pcm_frame", "dest": [{"extension": "streamid_adapter"}]},
            {"name": "pcm_frame", "source": [{"extension": "tts"}]},
        ],
        "data": [
            {"name": "data", "source": [{"extension": "message_collector"}]}
        ],
    },
    {
        "extension": "streamid_adapter",
        "audio_frame": [{"name": "pcm_frame", "dest": [{"extension": "stt"}]}],
    },
    {
        "extension": "stt",
        "cmd": [{"name": "flush", "dest": [{"extension": "main_control"}]}],
    },
    {
        "extension": "llm",
        "cmd": [
            {"names": ["flush"], "source": [{"extension": "main_control"}]}
        ],
        "data": [
            {"name": "text_data", "source": [{"extension": "main_control"}]}
        ],
    },
    {
        "extension": "tts",
        "data": [{"name": "text_data", "source": [{"extension": "llm"}]}],
        "audio_frame": [
            {"name": "pcm_frame", "dest": [{"extension": "agora_rtc"}]}
        ],
    },
]


# Helper function to create basic voice assistant graph (no tools)
def create_basic_voice_assistant(
    name, has_avatar=False, avatar_type=None, tts_config=None
):
    if tts_config is None:
        tts_config = cartesia_tts_sonic3
    # Standard architecture with TTS and main_control
    nodes = [
        copy.deepcopy(agora_rtc_base),
        copy.deepcopy(nova3_stt_100ms),
        copy.deepcopy(llama_llm_no_tools),
        copy.deepcopy(tts_config),
        copy.deepcopy(main_control_base),
        copy.deepcopy(message_collector),
        copy.deepcopy(streamid_adapter),
    ]

    connections = copy.deepcopy(basic_connections)

    if has_avatar and avatar_type in ["heygen", "anam"]:
        # Add appropriate avatar node
        if avatar_type == "heygen":
            nodes.append(copy.deepcopy(heygen_avatar))
        else:  # anam
            nodes.append(copy.deepcopy(anam_avatar))

        # Remove TTS audio source from agora_rtc (avatar will handle it)
        for conn in connections:
            if conn.get("extension") == "agora_rtc":
                # Remove the audio source from tts
                conn["audio_frame"] = [
                    af
                    for af in conn.get("audio_frame", [])
                    if "source" not in af
                    or af["source"] != [{"extension": "tts"}]
                ]
                break

        # Change TTS destination from agora_rtc to avatar
        for conn in connections:
            if conn.get("extension") == "tts":
                # Change audio destination to avatar
                for af in conn.get("audio_frame", []):
                    if "dest" in af and af["dest"] == [
                        {"extension": "agora_rtc"}
                    ]:
                        af["dest"] = [{"extension": "avatar"}]

                # Add tts_audio_end data to avatar
                conn["data"] = [
                    {"name": "text_data", "source": [{"extension": "llm"}]},
                    {
                        "name": "tts_audio_end",
                        "dest": [{"extension": "avatar"}],
                    },
                ]
                break

    return {
        "name": name,
        "auto_start": False,
        "graph": {"nodes": nodes, "connections": connections},
    }


# Helper function to create apollo/hellos graph with tools
def create_apollo_graph(
    name,
    llm_config,
    stt_config,
    has_avatar=False,
    avatar_type=None,
    tts_config=None,
    avatar_config=None,
    thymia_config=None,
    main_control_config=None,
):
    if tts_config is None:
        tts_config = cartesia_tts_sonic3
    if thymia_config is None:
        thymia_config = thymia_analyzer
    if main_control_config is None:
        main_control_config = main_control_apollo
    nodes = [
        copy.deepcopy(agora_rtc_base),
        copy.deepcopy(stt_config),
        copy.deepcopy(llm_config),
        copy.deepcopy(tts_config),
        copy.deepcopy(thymia_config),
        copy.deepcopy(main_control_config),
        copy.deepcopy(message_collector),
        copy.deepcopy(streamid_adapter),
    ]

    # Base apollo connections (without avatar)
    main_control_cmd = [
        {
            "names": ["on_user_joined", "on_user_left"],
            "source": [{"extension": "agora_rtc"}],
        },
        {
            "names": ["tool_register"],
            "source": [{"extension": "thymia_analyzer"}],
        },
        {
            "name": "flush",
            "source": [{"extension": "stt"}],
        },
    ]

    # Only add flush to avatar if avatar exists
    if has_avatar:
        main_control_cmd.append(
            {
                "name": "flush",
                "dest": [{"extension": "avatar"}],
            }
        )

    connections = [
        {
            "extension": "main_control",
            "cmd": main_control_cmd,
            "data": [
                {"name": "asr_result", "source": [{"extension": "stt"}]},
                {
                    "name": "text_data",
                    "source": [{"extension": "thymia_analyzer"}],
                },
            ],
        },
        {
            "extension": "agora_rtc",
            "audio_frame": [
                {"name": "pcm_frame", "source": [{"extension": "tts"}]},
                {
                    "name": "pcm_frame",
                    "dest": [
                        {"extension": "streamid_adapter"},
                        {"extension": "thymia_analyzer"},
                    ],
                },
            ],
            "data": [
                {"name": "data", "source": [{"extension": "message_collector"}]}
            ],
        },
        {
            "extension": "streamid_adapter",
            "audio_frame": [
                {"name": "pcm_frame", "dest": [{"extension": "stt"}]}
            ],
        },
        {
            "extension": "stt",
            "cmd": [{"name": "flush", "dest": [{"extension": "main_control"}]}],
        },
    ]

    # Add TTS connection - route tts_audio messages to thymia_analyzer and avatar
    # Avatar needs tts_audio_end to send voice_end signal to avatar service
    if has_avatar:
        tts_conn = {
            "extension": "tts",
            "data": [
                {"name": "text_data", "source": [{"extension": "llm"}]},
                {
                    "name": "tts_audio_start",
                    "dest": [{"extension": "thymia_analyzer"}],
                },
                {
                    "name": "tts_audio_end",
                    "dest": [
                        {"extension": "thymia_analyzer"},
                        {"extension": "avatar"},
                    ],
                },
            ],
            "audio_frame": [
                {"name": "pcm_frame", "dest": [{"extension": "avatar"}]}
            ],
        }
        connections.append(tts_conn)
    else:
        # No avatar - TTS goes directly to agora_rtc
        tts_conn = {
            "extension": "tts",
            "data": [
                {"name": "text_data", "source": [{"extension": "llm"}]},
                {
                    "name": "tts_audio_start",
                    "dest": [
                        {"extension": "thymia_analyzer"}
                    ],  # Direct to thymia
                },
                {
                    "name": "tts_audio_end",
                    "dest": [
                        {"extension": "thymia_analyzer"}
                    ],  # Direct to thymia
                },
            ],
            "audio_frame": [
                {"name": "pcm_frame", "dest": [{"extension": "agora_rtc"}]}
            ],
        }
        connections.append(tts_conn)

    if has_avatar:
        if avatar_config:
            nodes.append(copy.deepcopy(avatar_config))
        elif avatar_type == "heygen":
            nodes.append(copy.deepcopy(heygen_avatar))
        else:
            nodes.append(copy.deepcopy(anam_avatar))

        # Modify agora_rtc connection to get audio from avatar instead of tts directly
        for conn in connections:
            if conn.get("extension") == "agora_rtc":
                # Change audio source from tts to avatar
                for af in conn.get("audio_frame", []):
                    if "source" in af and af["source"] == [
                        {"extension": "tts"}
                    ]:
                        af["source"] = [{"extension": "avatar"}]

                # Add video frame from avatar
                conn["video_frame"] = [
                    {"name": "video_frame", "source": [{"extension": "avatar"}]}
                ]
                break

        # Add avatar connections
        # Note: tts_audio messages route directly from TTS to thymia (not through avatar)
        # Avatar only receives tts_text_input and audio frames
        avatar_conn = {
            "extension": "avatar",
            "audio_frame": [
                {"name": "pcm_frame", "source": [{"extension": "tts"}]}
            ],
            "data": [
                {
                    "name": "tts_text_input",
                    "source": [{"extension": "main_control"}],
                },
            ],
        }
        connections.append(avatar_conn)

    return {
        "name": name,
        "auto_start": False,
        "graph": {"nodes": nodes, "connections": connections},
    }


# Build new graph list
new_graphs = []

# COMMENTED OUT - Group 1: Basic voice assistants (no tools)
# print("Creating basic voice assistant graphs...")
# new_graphs.append(
#     create_basic_voice_assistant("voice_assistant", has_avatar=False)
# )
# new_graphs.append(
#     create_basic_voice_assistant(
#         "voice_assistant_heygen", has_avatar=True, avatar_type="heygen"
#     )
# )
# new_graphs.append(
#     create_basic_voice_assistant(
#         "voice_assistant_anam",
#         has_avatar=True,
#         avatar_type="anam",
#         tts_config=cartesia_tts_sonic3_anam,
#     )
# )

# COMMENTED OUT - Group 2: OSS graphs
# print("Creating OSS apollo graphs...")
# new_graphs.append(
#     create_apollo_graph(
#         "nova3_apollo_oss_cartesia_heygen",
#         groq_oss_llm_with_tools,
#         nova3_stt_300ms,
#         has_avatar=True,
#         avatar_type="heygen",
#     )
# )
# new_graphs.append(
#     create_apollo_graph(
#         "nova3_apollo_oss_cartesia_anam",
#         groq_oss_llm_with_tools,
#         nova3_stt_300ms,
#         has_avatar=True,
#         avatar_type="anam",
#         tts_config=cartesia_tts_sonic3_apollo_anam,
#         avatar_config=anam_avatar_apollo,
#     )
# )

# COMMENTED OUT - Group 3: GPT-4o graphs
# print("Creating GPT-4o apollo graphs...")
# new_graphs.append(
#     create_apollo_graph(
#         "nova3_apollo_gpt_4o_cartesia_heygen",
#         gpt4o_llm_with_tools,
#         nova3_stt_300ms,
#         has_avatar=True,
#         avatar_type="heygen",
#     )
# )
# new_graphs.append(
#     create_apollo_graph(
#         "nova3_apollo_gpt_4o_cartesia_anam",
#         gpt4o_llm_with_tools,
#         nova3_stt_300ms,
#         has_avatar=True,
#         avatar_type="anam",
#         tts_config=cartesia_tts_sonic3_apollo_anam,
#         avatar_config=anam_avatar_apollo,
#     )
# )
# new_graphs.append(
#     create_apollo_graph(
#         "flux_apollo_gpt_4o_cartesia_heygen",
#         gpt4o_llm_with_tools,
#         flux_stt,
#         has_avatar=True,
#         avatar_type="heygen",
#     )
# )
# new_graphs.append(
#     create_apollo_graph(
#         "flux_apollo_gpt_4o_cartesia_anam",
#         gpt4o_llm_with_tools,
#         flux_stt,
#         has_avatar=True,
#         avatar_type="anam",
#         tts_config=cartesia_tts_sonic3_apollo_anam,
#         avatar_config=anam_avatar_apollo,
#     )
# )

# Group 4: GPT-5.1 graphs (ONLY THESE 3 ACTIVE)
print("Creating GPT-5.1 apollo graphs...")

# 0. GPT-4o test graph (for regression testing - no new params)
new_graphs.append(
    create_apollo_graph(
        "flux_apollo_gpt_4o_cartesia",
        gpt4o_llm_with_tools,
        flux_stt,
        has_avatar=False,
    )
)

# 1. No avatar version
new_graphs.append(
    create_apollo_graph(
        "flux_apollo_gpt_5_1_cartesia",
        gpt51_llm_with_tools,
        flux_stt,
        has_avatar=False,
    )
)
# 2. HeyGen avatar version
new_graphs.append(
    create_apollo_graph(
        "flux_apollo_gpt_5_1_cartesia_heygen",
        gpt51_llm_with_tools,
        flux_stt,
        has_avatar=True,
        avatar_type="heygen",
    )
)
# 3. Anam avatar version
new_graphs.append(
    create_apollo_graph(
        "flux_apollo_gpt_5_1_cartesia_anam",
        gpt51_llm_with_tools,
        flux_stt,
        has_avatar=True,
        avatar_type="anam",
        tts_config=cartesia_tts_sonic3_apollo_anam,
        avatar_config=anam_avatar_apollo,
    )
)

# Group 5: Hellos-only graphs (wellness metrics without clinical indicators)
print("Creating Hellos-only graphs...")

# 4. Hellos with Anam avatar
new_graphs.append(
    create_apollo_graph(
        "flux_hellos_gpt_5_1_cartesia_anam",
        gpt51_llm_hellos,
        flux_stt,
        has_avatar=True,
        avatar_type="anam",
        tts_config=cartesia_tts_sonic3_apollo_anam,
        avatar_config=anam_avatar_apollo,
        thymia_config=thymia_analyzer_hellos,
        main_control_config=main_control_hellos,
    )
)

# Build final structure
new_data = {"ten": {"log": log_config, "predefined_graphs": new_graphs}}

# Write new property.json
with open("property.json", "w") as f:
    json.dump(new_data, f, indent=2)

print(f"\nSuccessfully created property.json with {len(new_graphs)} graphs:")
for i, graph in enumerate(new_graphs, 1):
    print(f"  {i}. {graph['name']}")
