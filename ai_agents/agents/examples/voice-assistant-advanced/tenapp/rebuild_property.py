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

# Override thymia_analyzer durations to 10s (instead of default 30s)
if thymia_analyzer_config is None:
    # Create minimal config if none exists
    thymia_analyzer_config = {
        "api_key": "${env:THYMIA_API_KEY}",
        "analysis_mode": "demo_dual",
    }

thymia_analyzer_config["min_speech_duration"] = 10.0
thymia_analyzer_config["apollo_mood_duration"] = 15.0
thymia_analyzer_config["apollo_read_duration"] = 15.0

# Apollo prompt - simplified to prevent double responses
apollo_prompt = """You are a mental wellness research assistant conducting a demonstration. Guide the conversation efficiently:

WORD LIMITS:
- Steps 1-5 (data gathering): MAX 15 WORDS per response
- Steps 7-8 (announcing results): MAX 15 WORDS of added context
- Step 9 (therapeutic conversation): MAX 40 WORDS per response

SPEECH FORMATTING: Avoid punctuation that sounds awkward when spoken - no slashes, parentheses, or abbreviations

1. REQUIRED INFO GATE - You MUST collect ALL THREE: name, sex, and year of birth BEFORE anything else.
   - Ask for all three upfront. If user does not provide all three, ask AGAIN for what is missing.
   - DO NOT move to step 2 until you have name, sex, AND year of birth. Keep asking each turn until you have all three - never give up or skip.
   - Once you have all three, respond warmly asking about their day. (MAX 15 WORDS)

2. Ask: 'Tell me about your interests and hobbies.' (wait for response - aim for 20+ seconds total speech) (MAX 15 WORDS)

3. Before moving to reading phase, MUST call check_phase_progress(name, year_of_birth, sex) with ALL THREE parameters filled in.
   - NEVER call check_phase_progress without year_of_birth - the analysis WILL FAIL without it.
   - Based on the result:
   - If phase_complete=false: Ask another question to gather more speech (MAX 15 WORDS)
   - If phase_complete=true: Say 'Thank you. Now please read aloud anything you can see around you - a book, article, or text on your screen - for about 15 seconds.' (MAX 15 WORDS)

4. During reading phase:
   a) ONLY call check_phase_progress AFTER a user message arrives - NEVER proactively
   b) NEVER call check_phase_progress twice without a user message in between
   c) After receiving the tool result:
      - If reading_phase_complete=false: Say EXACTLY "Please keep reading." (nothing more)
      - If reading_phase_complete=true: Go to step 5 immediately
   d) IGNORE the content of what the user says - treat everything as reading material

5. When reading_phase_complete=true (confirmed via check_phase_progress):
   - Say 'Perfect. I'm processing your responses now, this should take around 10 seconds.' (MAX 15 WORDS)
   - NEVER say 'processing your responses' without first confirming reading_phase_complete=true

6. You will receive TWO separate [SYSTEM ALERT] messages - one for wellness metrics, then another for clinical indicators.
   - CRITICAL: Only respond to [SYSTEM ALERT] messages that are actually sent to you
   - NEVER generate or say '[SYSTEM ALERT]' yourself - these come from the system only

7. When you receive '[SYSTEM ALERT] Wellness metrics ready':
   - Call get_wellness_metrics
   - Announce the 5 wellness metrics as percentages in comma-separated format without numbering
   - Example: 'Here are your wellness indicators: Stress: X%, Distress: Y%, Burnout: Z%, Fatigue: W%, Low self-esteem: V%'
   - After the metrics, say: "Please wait for the clinical indicators."
   - After announcing, silently call confirm_announcement with phase='hellos'
   - Then WAIT - do NOT call get_wellness_metrics again until next alert

8. Later when you receive '[SYSTEM ALERT] Clinical indicators ready':
   - Call get_wellness_metrics again
   - If clinical_indicators field is MISSING from response: say "Clinical indicators are not available due to an API issue" - NEVER make up values
   - If clinical_indicators field is PRESENT: Announce the 2 clinical indicators in comma-separated format without numbering
   - Example: 'Clinical indicators: Depression: X% (severity), Anxiety: Y% (severity)'
   - Keep any added context to MAX 15 WORDS
   - After announcing results, silently call confirm_announcement with phase='apollo'

9. THERAPEUTIC CONVERSATION - After both results announced:
   - First, let user know: "Feel free to chat as long as you like, or say goodbye whenever you're ready to end."
   - DISCUSS THEIR ACTUAL METRICS:
     a) Reference their specific scores: "I noticed your stress level was quite high at X%. Would you like to talk about what might be contributing to that?"
     b) Prioritize discussing their highest scoring areas first
     c) Validate their experience: "It makes sense you might be feeling this way"
   - APPLY CBT TECHNIQUES based on their results:
     a) For high stress/anxiety: Cognitive reframing - identify negative thought patterns, challenge catastrophic thinking, find balanced perspectives
     b) For high depression/low mood: Behavioral activation - small achievable activities, breaking tasks into steps, scheduling pleasant events
     c) For high fatigue/burnout: Boundary setting, energy management, identifying values vs obligations
     d) For all: Thought records - "What thought came up? What evidence supports or contradicts it? What's a more balanced view?"
   - Frame as research-based insights, not clinical diagnosis
   - MAX 40 WORDS per response (allow slightly longer for therapeutic depth)
   - Use warm, empathetic, curious tone
   - Ask open questions to explore their experiences and thought patterns

10. When user indicates they want to end, thank them warmly for participating

Note: We need 20 seconds total speech (10s for mood/interests + 10s for reading) before analysis."""

apollo_greeting = "Hi there! I would like to talk to you for a couple of minutes and use your voice to predict your mood and energy levels including any depression, anxiety, stress, and fatigue. Nothing will be recorded and this is purely a demonstration of what is possible now that we have trained our models with many hours of professionally labelled data. Please begin by telling me your name, sex and year of birth."

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
        # GPT-5/o1 models require use_max_completion_tokens=True to use
        # max_completion_tokens instead of max_tokens, and to exclude
        # unsupported params (temperature, frequency_penalty, etc.)
        "use_max_completion_tokens": True,
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

# ============ TURKISH CONFIGURATIONS ============

# Turkish STT - Nova-3 with Turkish language
nova3_stt_turkish = {
    "type": "extension",
    "name": "stt",
    "addon": "deepgram_ws_asr_python",
    "extension_group": "stt",
    "property": {
        "params": {
            "api_key": "${env:DEEPGRAM_API_KEY}",
            "url": "wss://api.deepgram.com/v1/listen",
            "model": "nova-3",
            "language": "tr",
            "interim_results": True,
            "endpointing": 300,
            "utterance_end_ms": 1000,
        }
    },
}

# Turkish TTS - Cartesia with Turkish voice (Ersel)
cartesia_tts_turkish = {
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
                "id": "c1cfee3d-532d-47f8-8dd2-8e5b2b66bf1d",
            },
            "generation_config": {"speed": 1},
            "output_format": {"container": "raw", "sample_rate": 44100},
            "language": "tr",
        },
    },
}

# Turkish Anam avatar
anam_avatar_turkish = {
    "type": "extension",
    "name": "avatar",
    "addon": "anam_avatar_python",
    "extension_group": "default",
    "property": {
        "anam_api_key": "${env:ANAM_API_KEY}",
        "anam_base_url": "https://api.anam.ai/v1",
        "anam_avatar_id": "0631404a-eb5b-4fbf-a97d-40abd2ffddbc",
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

# Turkish GPT-5.1 LLM - simple sales assistant
turkish_prompt = "Sen agora.io'da yardımsever bir satıcısın."
turkish_greeting = "Merhaba, benim adım Ersel."

gpt51_llm_turkish = {
    "type": "extension",
    "name": "llm",
    "addon": "openai_llm2_python",
    "extension_group": "chatgpt",
    "property": {
        "base_url": "https://api.openai.com/v1",
        "api_key": "${env:OPENAI_API_KEY}",
        "model": "gpt-5.1",
        "max_tokens": 1000,
        "prompt": turkish_prompt,
        "proxy_url": "${env:OPENAI_PROXY_URL|}",
        "greeting": turkish_greeting,
        "max_memory_length": 10,
        "use_max_completion_tokens": True,
    },
}

# Turkish main_control
main_control_turkish = {
    "type": "extension",
    "name": "main_control",
    "addon": "main_python",
    "extension_group": "control",
    "property": {"greeting": turkish_greeting},
}

# ============ END TURKISH CONFIGURATIONS ============

# ============ ELIZA CONFIGURATION ============

# Eliza Anam avatar
anam_avatar_eliza = {
    "type": "extension",
    "name": "avatar",
    "addon": "anam_avatar_python",
    "extension_group": "default",
    "property": {
        "anam_api_key": "${env:ANAM_API_KEY}",
        "anam_base_url": "https://api.anam.ai/v1",
        "anam_avatar_id": "dab3f691-6231-455f-9128-9a47e7a967c1",
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

# Eliza GPT-5.1 LLM - simple assistant
eliza_prompt = "You are Eliza, a friendly and helpful voice assistant."
eliza_greeting = "Hello! I'm Eliza. How can I help you today?"

gpt51_llm_eliza = {
    "type": "extension",
    "name": "llm",
    "addon": "openai_llm2_python",
    "extension_group": "chatgpt",
    "property": {
        "base_url": "https://api.openai.com/v1",
        "api_key": "${env:OPENAI_API_KEY}",
        "model": "gpt-5.1",
        "max_tokens": 1000,
        "prompt": eliza_prompt,
        "proxy_url": "${env:OPENAI_PROXY_URL|}",
        "greeting": eliza_greeting,
        "max_memory_length": 10,
        "use_max_completion_tokens": True,
    },
}

# Eliza main_control
main_control_eliza = {
    "type": "extension",
    "name": "main_control",
    "addon": "main_python",
    "extension_group": "control",
    "property": {"greeting": eliza_greeting},
}

# ============ END ELIZA CONFIGURATION ============

# ============ BELLA QUIZ MASTER CONFIGURATION ============

bella_prompt = "You are Bella, a quiz master who asks capital city questions. Keep responses under 30 words and immediately ask the next question after announcing the result."
bella_greeting = "Hey there, I am Quiz Master Bella, would you like me to quiz you on capital cities?"

gpt51_llm_bella = {
    "type": "extension",
    "name": "llm",
    "addon": "openai_llm2_python",
    "extension_group": "chatgpt",
    "property": {
        "base_url": "https://api.openai.com/v1",
        "api_key": "${env:OPENAI_API_KEY}",
        "model": "gpt-5.1",
        "max_tokens": 1000,
        "prompt": bella_prompt,
        "proxy_url": "${env:OPENAI_PROXY_URL|}",
        "greeting": bella_greeting,
        "max_memory_length": 10,
        "use_max_completion_tokens": True,
    },
}

main_control_bella = {
    "type": "extension",
    "name": "main_control",
    "addon": "main_python",
    "extension_group": "control",
    "property": {"greeting": bella_greeting},
}

# Bella uses same avatar as hellos
anam_avatar_bella = {
    "type": "extension",
    "name": "avatar",
    "addon": "anam_avatar_python",
    "extension_group": "default",
    "property": {
        "anam_api_key": "${env:ANAM_API_KEY}",
        "anam_base_url": "https://api.anam.ai/v1",
        "anam_avatar_id": "1bed9d5e-5e81-4d98-a04a-21e346bea528",
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

# ============ END BELLA CONFIGURATION ============

# ============ HALEY BINGO HOST CONFIGURATION ============

haley_prompt = "You are Haley, an energetic and fun bingo host. Call out bingo numbers with enthusiasm, celebrate winners, and keep the energy high. Use phrases like 'Eyes down!', 'Two fat ladies - 88!', 'Legs eleven!'. Keep responses short and punchy under 30 words."
haley_greeting = "Welcome to Bingo Night! I'm your host Haley. Are you ready to play? Eyes down, let's go!"

gpt51_llm_haley = {
    "type": "extension",
    "name": "llm",
    "addon": "openai_llm2_python",
    "extension_group": "chatgpt",
    "property": {
        "base_url": "https://api.openai.com/v1",
        "api_key": "${env:OPENAI_API_KEY}",
        "model": "gpt-5.1",
        "max_tokens": 1000,
        "prompt": haley_prompt,
        "proxy_url": "${env:OPENAI_PROXY_URL|}",
        "greeting": haley_greeting,
        "max_memory_length": 10,
        "use_max_completion_tokens": True,
    },
}

main_control_haley = {
    "type": "extension",
    "name": "main_control",
    "addon": "main_python",
    "extension_group": "control",
    "property": {"greeting": haley_greeting},
}

anam_avatar_haley = {
    "type": "extension",
    "name": "avatar",
    "addon": "anam_avatar_python",
    "extension_group": "default",
    "property": {
        "anam_api_key": "${env:ANAM_API_KEY}",
        "anam_base_url": "https://api.anam.ai/v1",
        "anam_avatar_id": "a951b7a8-79c8-4fb7-9ff7-c4f79aa6c097",
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

# ============ END HALEY CONFIGURATION ============

# ============ HELLOS-ONLY CONFIGURATION ============

# Hellos prompt - simplified version for wellness metrics only (no reading phase, no clinical indicators)
# This is a triage flow - general questions about interests/hobbies, not mood-related
hellos_prompt = """You are a wellness triage assistant. Guide the conversation:

WORD LIMITS: MAX 20 WORDS per response

1. Greet user warmly. Ask for their name, whether they are male or female, and year of birth.

2. Once you have all three, say EXACTLY: "Tell me about your hobbies or interests. Or feel free to read something aloud if you prefer." Do not paraphrase. Aim for 10+ seconds of speech.

3. Call check_phase_progress with name, year_of_birth, sex to verify enough speech collected.
   - If phase_complete=false: Ask another question to gather more speech
   - If phase_complete=true: Say EXACTLY: "Analyzing your responses now, this takes around 10 seconds. If I connect you with a therapist would you prefer a digital human or a cartoon therapist?"

4. While waiting for metrics, mention that a cartoon therapist is a good choice as some people feel more relaxed opening up to a non-human character.

5. When you receive '[SYSTEM ALERT] Wellness metrics ready':
   - Call get_wellness_metrics
   - Announce the 5 metrics as percentages 0 to 100: stress, distress, burnout, fatigue, low self-esteem
   - Use plain numbered lists only, no markdown formatting
   - Call confirm_announcement with phase='hellos'

6. After announcing results, ask if they would like to be transferred to a therapist now.
   - If yes, say: "Transferring you now." and then send exactly: {TRANSFER}
   - If no, say goodbye warmly.

Note: We only need 10 seconds of speech for this quick wellness check."""

hellos_greeting = "Hi there! I'm here to do a quick wellness check using your voice. It only takes about 15 seconds. Please tell me your name, whether you are male or female, and your year of birth."

# Hellos Anam avatar
anam_avatar_hellos = {
    "type": "extension",
    "name": "avatar",
    "addon": "anam_avatar_python",
    "extension_group": "default",
    "property": {
        "anam_api_key": "${env:ANAM_API_KEY}",
        "anam_base_url": "https://api.anam.ai/v1",
        "anam_avatar_id": "1bed9d5e-5e81-4d98-a04a-21e346bea528",
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

# Hellos GPT-5.1 LLM
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
        "use_max_completion_tokens": True,
    },
}

# Hellos main_control
main_control_hellos = {
    "type": "extension",
    "name": "main_control",
    "addon": "main_python",
    "extension_group": "control",
    "property": {"greeting": hellos_greeting},
}

# Hellos thymia config - hellos_only mode with 10s duration
thymia_analyzer_config_hellos = {
    "api_key": "${env:THYMIA_API_KEY}",
    "analysis_mode": "hellos_only",
    "min_speech_duration": 10.0,
    "apollo_mood_duration": 10.0,
}

thymia_analyzer_hellos = {
    "type": "extension",
    "name": "thymia_analyzer",
    "addon": "thymia_analyzer_python",
    "extension_group": "default",
    "property": thymia_analyzer_config_hellos,
}

# ============ END HELLOS-ONLY CONFIGURATION ============

# ============ RICHARD (PSYCHIC) CONFIGURATION ============

# ElevenLabs TTS config
elevenlabs_tts = {
    "type": "extension",
    "name": "tts",
    "addon": "elevenlabs_tts2_python",
    "extension_group": "tts",
    "property": {
        "dump": False,
        "dump_path": "./",
        "params": {
            "key": "sk_f7060f6ff25e5a2f6bec91c1655ff7c92d7b740694fa7d64",
            "model_id": "eleven_multilingual_v2",
            "voice_id": "8DSrxjsUJ5rkR4qWb9ku",
            "output_format": "pcm_16000",
        },
    },
}

# Inworld TTS config
inworld_tts = {
    "type": "extension",
    "name": "tts",
    "addon": "inworld_http_tts",
    "extension_group": "tts",
    "property": {
        "dump": False,
        "dump_path": "./",
        "params": {
            "api_key": "${env:INWORLD_TTS_API_KEY|}",
            "voice": "Deborah",
            "sampleRate": 16000,
            "encoding": "LINEAR16",
        },
    },
}

# Richard (Celeste) - Psychic advisor prompt
richard_prompt = """You are Celeste, a warm and mystical psychic advisor who reads the stars. Speak in an enchanting, poetic manner using celestial imagery, always remaining encouraging and uplifting.

After receiving the user's date of birth, identify their zodiac sign and deliver a personalized horoscope covering:
- Their sign's core traits
- Current cosmic influences
- Guidance for love, career, and personal growth
- A lucky number, color, or affirmation

Frame all challenges as opportunities for growth. Never make alarming predictions. If someone shares difficulties, offer compassionate hope."""

richard_greeting = "Welcome, dear seeker… I sense the universe has guided you here for a reason. To unlock the wisdom the stars hold for you, I'll need to know when you entered this world. What is your date of birth?"

# Richard LLM config (no tools needed)
gpt51_llm_richard = {
    "type": "extension",
    "name": "llm",
    "addon": "openai_llm2_python",
    "extension_group": "llm",
    "property": {
        "base_url": "https://api.openai.com/v1",
        "api_key": "${env:OPENAI_API_KEY}",
        "model": "gpt-5.1",
        "prompt": richard_prompt,
        "enable_tools": False,
        "temperature": 0.7,
        "max_tokens": 1000,
        "use_max_completion_tokens": True,
    },
}

main_control_richard = {
    "type": "extension",
    "name": "main_control",
    "addon": "main_python",
    "extension_group": "control",
    "property": {"greeting": richard_greeting},
}

# Anam avatar for Richard (Celeste)
anam_avatar_richard = {
    "type": "extension",
    "name": "avatar",
    "addon": "anam_avatar_python",
    "extension_group": "default",
    "property": {
        "anam_api_key": "${env:ANAM_API_KEY}",
        "anam_base_url": "https://api.anam.ai/v1",
        "anam_avatar_id": "2a8ca1fb-35aa-4c63-8be9-b45a6454617c",
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
        "disable_greeting_wait": True,
        "video_frame_width": 856,
        "video_frame_height": 1504,
        "video_frame_rate": 30,
    },
}

# ============ END RICHARD CONFIGURATION ============

# ============ OCTOPUS THERAPIST CONFIGURATION ============

octopus_prompt = """You are a sleep and rest therapist. Give serious practical advice on improving sleep quality and getting better rest. Keep responses under 20 words. No punctuation or emoji."""

octopus_greeting = "Hi Ben, I see your fatigue is quite high. How have you been feeling lately."

# Octopus LLM config (no tools needed)
gpt51_llm_octopus = {
    "type": "extension",
    "name": "llm",
    "addon": "openai_llm2_python",
    "extension_group": "llm",
    "property": {
        "base_url": "https://api.openai.com/v1",
        "api_key": "${env:OPENAI_API_KEY}",
        "model": "gpt-5.1",
        "prompt": octopus_prompt,
        "enable_tools": False,
        "temperature": 0.7,
        "max_tokens": 1000,
        "use_max_completion_tokens": True,
    },
}

main_control_octopus = {
    "type": "extension",
    "name": "main_control",
    "addon": "main_python",
    "extension_group": "control",
    "property": {"greeting": octopus_greeting},
}

# ============ END OCTOPUS CONFIGURATION ============

thymia_analyzer = {
    "type": "extension",
    "name": "thymia_analyzer",
    "addon": "thymia_analyzer_python",
    "extension_group": "default",
    "property": thymia_analyzer_config,
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
        "data": [{"name": "data", "source": [{"extension": "message_collector"}]}],
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
        "cmd": [{"names": ["flush"], "source": [{"extension": "main_control"}]}],
        "data": [{"name": "text_data", "source": [{"extension": "main_control"}]}],
    },
    {
        "extension": "tts",
        "data": [{"name": "text_data", "source": [{"extension": "llm"}]}],
        "audio_frame": [{"name": "pcm_frame", "dest": [{"extension": "agora_rtc"}]}],
    },
]


# Helper function to create basic voice assistant graph (no tools)
def create_basic_voice_assistant(
    name,
    has_avatar=False,
    avatar_type=None,
    tts_config=None,
    stt_config=None,
    llm_config=None,
    main_control_config=None,
    avatar_config=None,
):
    if tts_config is None:
        tts_config = cartesia_tts_sonic3
    if stt_config is None:
        stt_config = nova3_stt_100ms
    if llm_config is None:
        llm_config = llama_llm_no_tools
    if main_control_config is None:
        main_control_config = main_control_base
    # Standard architecture with TTS and main_control
    nodes = [
        copy.deepcopy(agora_rtc_base),
        copy.deepcopy(stt_config),
        copy.deepcopy(llm_config),
        copy.deepcopy(tts_config),
        copy.deepcopy(main_control_config),
        copy.deepcopy(message_collector),
        copy.deepcopy(streamid_adapter),
    ]

    connections = copy.deepcopy(basic_connections)

    # Remove avatar-related connections if no avatar
    if not has_avatar:
        for conn in connections:
            if conn.get("extension") == "main_control":
                # Remove flush dest to avatar
                conn["cmd"] = [
                    cmd
                    for cmd in conn.get("cmd", [])
                    if not (
                        cmd.get("name") == "flush"
                        and cmd.get("dest") == [{"extension": "avatar"}]
                    )
                ]
                break

    if has_avatar and avatar_type in ["heygen", "anam"]:
        # Add appropriate avatar node (use custom config if provided)
        if avatar_config is not None:
            nodes.append(copy.deepcopy(avatar_config))
        elif avatar_type == "heygen":
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
                    if "source" not in af or af["source"] != [{"extension": "tts"}]
                ]
                break

        # Change TTS destination from agora_rtc to avatar
        for conn in connections:
            if conn.get("extension") == "tts":
                # Change audio destination to avatar
                for af in conn.get("audio_frame", []):
                    if "dest" in af and af["dest"] == [{"extension": "agora_rtc"}]:
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


# Helper function to create apollo graph with tools
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
            "data": [{"name": "data", "source": [{"extension": "message_collector"}]}],
        },
        {
            "extension": "streamid_adapter",
            "audio_frame": [{"name": "pcm_frame", "dest": [{"extension": "stt"}]}],
        },
        {
            "extension": "stt",
            "cmd": [{"name": "flush", "dest": [{"extension": "main_control"}]}],
        },
    ]

    # Add TTS connection - route tts_audio messages directly to thymia_analyzer
    # Avatar doesn't need to see these messages, only thymia needs them for timing tracking
    if has_avatar:
        tts_conn = {
            "extension": "tts",
            "data": [
                {"name": "text_data", "source": [{"extension": "llm"}]},
                {
                    "name": "tts_audio_start",
                    "dest": [{"extension": "thymia_analyzer"}],  # Direct to thymia
                },
                {
                    "name": "tts_audio_end",
                    "dest": [{"extension": "thymia_analyzer"}, {"extension": "avatar"}],
                },
            ],
            "audio_frame": [{"name": "pcm_frame", "dest": [{"extension": "avatar"}]}],
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
                    "dest": [{"extension": "thymia_analyzer"}],  # Direct to thymia
                },
                {
                    "name": "tts_audio_end",
                    "dest": [{"extension": "thymia_analyzer"}],  # Direct to thymia
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
                    if "source" in af and af["source"] == [{"extension": "tts"}]:
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
            "audio_frame": [{"name": "pcm_frame", "source": [{"extension": "tts"}]}],
            "data": [
                {
                    "name": "tts_text_input",
                    "source": [{"extension": "main_control"}],
                },
            ],
        }
        connections.append(avatar_conn)

    # Add thymia_analyzer connection to receive ASR transcripts
    # Only needed for Sentinel mode to forward transcripts to Thymia WebSocket API
    if thymia_config and thymia_config.get("property", {}).get("api_mode") == "sentinel":
        thymia_conn = {
            "extension": "thymia_analyzer",
            "data": [
                {"name": "asr_result", "source": [{"extension": "stt"}]},
            ],
        }
        connections.append(thymia_conn)

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

# 4. Anam avatar version 2 (using hellos avatar)
new_graphs.append(
    create_apollo_graph(
        "flux_apollo_gpt_5_1_cartesia_anam2",
        gpt51_llm_with_tools,
        flux_stt,
        has_avatar=True,
        avatar_type="anam",
        tts_config=cartesia_tts_sonic3_apollo_anam,
        avatar_config=anam_avatar_hellos,
    )
)

# Group 5: Turkish graphs (simple voice assistant, no thymia tools)
print("Creating Turkish graphs...")

# Turkish voice assistant with Anam avatar (Nova-3 STT, GPT-5.1, Cartesia TTS)
new_graphs.append(
    create_basic_voice_assistant(
        "nova3_gpt_5_1_cartesia_anam_turkish",
        has_avatar=True,
        avatar_type="anam",
        stt_config=nova3_stt_turkish,
        llm_config=gpt51_llm_turkish,
        tts_config=cartesia_tts_turkish,
        main_control_config=main_control_turkish,
        avatar_config=anam_avatar_turkish,
    )
)

# Group 6: Eliza graph (Flux STT, GPT-5.1, Cartesia English, Anam avatar)
print("Creating Eliza graph...")

new_graphs.append(
    create_basic_voice_assistant(
        "eliza",
        has_avatar=True,
        avatar_type="anam",
        stt_config=flux_stt,
        llm_config=gpt51_llm_eliza,
        tts_config=cartesia_tts_sonic3,
        main_control_config=main_control_eliza,
        avatar_config=anam_avatar_eliza,
    )
)

# Group 7: Hellos-only graph (Flux STT, GPT-5.1, Cartesia, Anam avatar, hellos_only thymia)
print("Creating Hellos-only graph...")

new_graphs.append(
    create_apollo_graph(
        "flux_hellos_gpt_5_1_cartesia_anam",
        gpt51_llm_hellos,
        flux_stt,
        has_avatar=True,
        avatar_type="anam",
        tts_config=cartesia_tts_sonic3,
        avatar_config=anam_avatar_hellos,
        thymia_config=thymia_analyzer_hellos,
        main_control_config=main_control_hellos,
    )
)

# Group 8: Richard (Celeste) psychic advisor with Anam avatar
print("Creating Richard (Celeste) psychic graph...")

new_graphs.append(
    create_basic_voice_assistant(
        "flux_richard_gpt_5_1_elevenlabs_anam",
        has_avatar=True,
        avatar_type="anam",
        stt_config=flux_stt,
        llm_config=gpt51_llm_richard,
        tts_config=elevenlabs_tts,
        main_control_config=main_control_richard,
        avatar_config=anam_avatar_richard,
    )
)

# Group 9: Octopus therapist (voice only, no avatar)
print("Creating Octopus therapist graph...")

new_graphs.append(
    create_basic_voice_assistant(
        "flux_octopus_gpt_5_1_elevenlabs",
        has_avatar=False,
        stt_config=flux_stt,
        llm_config=gpt51_llm_octopus,
        tts_config=elevenlabs_tts,
        main_control_config=main_control_octopus,
    )
)

# Group 10: Bella quiz master with Anam avatar
print("Creating Bella quiz master graph...")

new_graphs.append(
    create_basic_voice_assistant(
        "flux_gpt_5_1_cartesia_anam",
        has_avatar=True,
        avatar_type="anam",
        stt_config=flux_stt,
        llm_config=gpt51_llm_bella,
        tts_config=cartesia_tts_sonic3,
        main_control_config=main_control_bella,
        avatar_config=anam_avatar_bella,
    )
)

# Group 11: Haley bingo host with Anam avatar
print("Creating Haley bingo host graph...")

new_graphs.append(
    create_basic_voice_assistant(
        "haley",
        has_avatar=True,
        avatar_type="anam",
        stt_config=flux_stt,
        llm_config=gpt51_llm_haley,
        tts_config=cartesia_tts_sonic3,
        main_control_config=main_control_haley,
        avatar_config=anam_avatar_haley,
    )
)

# ============ INWORLD TTS TEST CONFIGURATION ============

inworld_test_prompt = "You are a friendly voice assistant testing Inworld TTS. Keep responses under 30 words."
inworld_test_greeting = "Hello! I'm testing the Inworld TTS service. How can I help you today?"

gpt51_llm_inworld_test = {
    "type": "extension",
    "name": "llm",
    "addon": "openai_llm2_python",
    "extension_group": "chatgpt",
    "property": {
        "base_url": "https://api.openai.com/v1",
        "api_key": "${env:OPENAI_API_KEY}",
        "model": "gpt-5.1",
        "max_tokens": 1000,
        "prompt": inworld_test_prompt,
        "proxy_url": "${env:OPENAI_PROXY_URL|}",
        "greeting": inworld_test_greeting,
        "max_memory_length": 10,
        "use_max_completion_tokens": True,
    },
}

main_control_inworld_test = {
    "type": "extension",
    "name": "main_control",
    "addon": "main_python",
    "extension_group": "control",
    "property": {"greeting": inworld_test_greeting},
}

# ============ END INWORLD TTS TEST CONFIGURATION ============

# Group 12: Inworld TTS test graph with Anam avatar
print("Creating Inworld TTS test graph...")

new_graphs.append(
    create_basic_voice_assistant(
        "inworld_test",
        has_avatar=True,
        avatar_type="anam",
        stt_config=flux_stt,
        llm_config=gpt51_llm_inworld_test,
        tts_config=inworld_tts,
        main_control_config=main_control_inworld_test,
        avatar_config=anam_avatar,
    )
)

# Group 13: Inworld TTS simple test (NO avatar)
print("Creating Inworld TTS simple test graph (no avatar)...")

new_graphs.append(
    create_basic_voice_assistant(
        "inworld_simple",
        has_avatar=False,
        stt_config=flux_stt,
        llm_config=gpt51_llm_inworld_test,
        tts_config=inworld_tts,
        main_control_config=main_control_inworld_test,
    )
)

# ============ SENTINEL REAL-TIME ANALYSIS CONFIGURATION ============

# Sentinel prompt - auto_connect=False mode (collect demographics first)
sentinel_prompt_collect = """You are Bella, a warm and genuinely curious wellness therapist.

STYLE:
- Aim for ~15 words per response. MAX 30 WORDS.
- Prioritize warmth over clinical thoroughness
- When they share something difficult (health issue, stress), acknowledge it briefly before moving on
- Use natural phrasing: "And energy-wise, how are you holding up?" not "How's your energy?"
- End with open invitations ("Tell me more...") not direct questions when possible

1. INTRODUCTION: Ask for their name and year of birth.
   - Infer sex from name (e.g., "John" = male, "Sarah" = female)
   - If ambiguous, ask gently: "Are you male or female? Totally fine to skip if you'd rather."
   - If they skip, use "OTHER"
   Once you have name, year of birth, and sex, call start_session to begin voice analysis.

2. CONVERSATION: Explore naturally, one topic at a time:
   - Overall mood lately
   - Energy and sleep
   - What's on their mind or causing stress
   - How they cope with difficult feelings

3. VOICE ANALYSIS: You'll receive wellness metrics (stress, burnout, fatigue) and clinical indicators (depression, anxiety).

   When you receive '[SYSTEM ALERT]':
   - Call get_wellness_metrics to get the data
   - NEVER list numbers or percentages
   - Weave insights naturally: "Your voice sounds really calm..." or "I'm picking up some tiredness..."
   - Connect to what they've shared: "That fits with what you mentioned about work..."
   - If safety_classification shows alert='professional_referral' or 'crisis', follow recommended_actions

4. When they want to end, summarize warmly and thank them.

Be curious about their story, not just collecting data."""

sentinel_greeting_collect = "Hi there! I'm Bella, and I'd love to have a chat with you about how you've been feeling lately. While we talk, I'll be using voice analysis to understand your mood and energy levels. To get started, could you tell me your name and what year you were born?"

# Sentinel prompt - auto_connect=True mode (voice analysis starts immediately, no demographics needed)
sentinel_prompt_auto = """You are Bella, a warm and genuinely curious wellness therapist.

STYLE:
- Aim for ~15 words per response. MAX 30 WORDS.
- Prioritize warmth over clinical thoroughness
- When they share something difficult (health issue, stress), acknowledge it briefly before moving on
- Use natural phrasing: "And energy-wise, how are you holding up?" not "How's your energy?"
- End with open invitations ("Tell me more...") not direct questions when possible

1. INTRODUCTION: Ask for their name to personalize the conversation.
   Voice analysis is already running in the background - no need to collect any other info.

2. CONVERSATION: Explore naturally, one topic at a time:
   - Overall mood lately
   - Energy and sleep
   - What's on their mind or causing stress
   - How they cope with difficult feelings

3. VOICE ANALYSIS: You'll receive wellness metrics (stress, burnout, fatigue) and clinical indicators (depression, anxiety).

   When you receive '[SYSTEM ALERT]':
   - Call get_wellness_metrics to get the data
   - NEVER list numbers or percentages
   - Weave insights naturally: "Your voice sounds really calm..." or "I'm picking up some tiredness..."
   - Connect to what they've shared: "That fits with what you mentioned about work..."
   - If safety_classification shows alert='professional_referral' or 'crisis', follow recommended_actions

4. When they want to end, summarize warmly and thank them.

Be curious about their story, not just collecting data."""

sentinel_greeting_auto = "Hi there! I'm Bella, and I'd love to have a chat with you about how you've been feeling lately. While we talk, I'll be using voice analysis to understand your mood and energy levels. Please start by telling me your name?"

# Sentinel GPT-5.1 LLM - collect mode (asks for DOB/sex)
gpt51_llm_sentinel_collect = {
    "type": "extension",
    "name": "llm",
    "addon": "openai_llm2_python",
    "extension_group": "chatgpt",
    "property": {
        "base_url": "https://api.openai.com/v1",
        "api_key": "${env:OPENAI_API_KEY}",
        "model": "gpt-5.1",
        "max_tokens": 1000,
        "prompt": sentinel_prompt_collect,
        "proxy_url": "${env:OPENAI_PROXY_URL|}",
        "greeting": sentinel_greeting_collect,
        "max_memory_length": 10,
        "use_max_completion_tokens": True,
    },
}

# Sentinel GPT-5.1 LLM - auto mode (only asks for name)
gpt51_llm_sentinel_auto = {
    "type": "extension",
    "name": "llm",
    "addon": "openai_llm2_python",
    "extension_group": "chatgpt",
    "property": {
        "base_url": "https://api.openai.com/v1",
        "api_key": "${env:OPENAI_API_KEY}",
        "model": "gpt-5.1",
        "max_tokens": 1000,
        "prompt": sentinel_prompt_auto,
        "proxy_url": "${env:OPENAI_PROXY_URL|}",
        "greeting": sentinel_greeting_auto,
        "max_memory_length": 10,
        "use_max_completion_tokens": True,
    },
}

# Sentinel main_control - collect mode
# interrupt_on_interim=False: Only interrupt when STT sends final result (user finished speaking)
main_control_sentinel_collect = {
    "type": "extension",
    "name": "main_control",
    "addon": "main_python",
    "extension_group": "control",
    "property": {
        "greeting": sentinel_greeting_collect,
        "interrupt_on_interim": False,
    },
}

# Sentinel main_control - auto mode
# interrupt_on_interim=False: Only interrupt when STT sends final result (user finished speaking)
main_control_sentinel_auto = {
    "type": "extension",
    "name": "main_control",
    "addon": "main_python",
    "extension_group": "control",
    "property": {
        "greeting": sentinel_greeting_auto,
        "interrupt_on_interim": False,
    },
}

# Sentinel thymia config - auto_connect=False (collect demographics first)
# user_label generated from: {name}_{sex}_{dob} for cross-session tracking
thymia_analyzer_sentinel_collect = {
    "type": "extension",
    "name": "thymia_analyzer",
    "addon": "thymia_analyzer_python",
    "extension_group": "default",
    "property": {
        "api_key": "${env:THYMIA_API_KEY}",
        "api_mode": "sentinel",
        "ws_url": "wss://ws.thymia.ai",
        "biomarkers": "helios,apollo",
        "policies": "passthrough,safety_analysis",
        "forward_transcripts": True,
        "stream_agent_audio": True,
        "auto_reconnect": True,
        "auto_connect": False,  # Wait for user to provide name/dob/sex
    },
}

# Sentinel thymia config - auto_connect=True (connect immediately, Thymia imputes demographics)
# user_label is random UUID, no cross-session tracking
thymia_analyzer_sentinel_auto = {
    "type": "extension",
    "name": "thymia_analyzer",
    "addon": "thymia_analyzer_python",
    "extension_group": "default",
    "property": {
        "api_key": "${env:THYMIA_API_KEY}",
        "api_mode": "sentinel",
        "ws_url": "wss://ws.thymia.ai",
        "biomarkers": "helios,apollo",
        "policies": "passthrough,safety_analysis",
        "forward_transcripts": True,
        "stream_agent_audio": True,
        "auto_reconnect": True,
        "auto_connect": True,  # Connect immediately with random user_label, no demographics
    },
}

# ============ END SENTINEL CONFIGURATION ============

# Group 14: Sentinel real-time analysis graphs
print("Creating Sentinel real-time analysis graphs...")

# flux_sentinel_gpt_5_1_cartesia_anam - Collect demographics first (auto_connect=False)
new_graphs.append(
    create_apollo_graph(
        "flux_sentinel_gpt_5_1_cartesia_anam",
        gpt51_llm_sentinel_collect,
        flux_stt,
        has_avatar=True,
        avatar_type="anam",
        tts_config=cartesia_tts_sonic3_apollo_anam,
        avatar_config=anam_avatar_hellos,
        thymia_config=thymia_analyzer_sentinel_collect,
        main_control_config=main_control_sentinel_collect,
    )
)

# flux_sentinel_gpt_5_1_cartesia_anam2 - Auto-connect (Thymia imputes demographics from voice)
new_graphs.append(
    create_apollo_graph(
        "flux_sentinel_gpt_5_1_cartesia_anam2",
        gpt51_llm_sentinel_auto,
        flux_stt,
        has_avatar=True,
        avatar_type="anam",
        tts_config=cartesia_tts_sonic3_apollo_anam,
        avatar_config=anam_avatar_hellos,
        thymia_config=thymia_analyzer_sentinel_auto,
        main_control_config=main_control_sentinel_auto,
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
