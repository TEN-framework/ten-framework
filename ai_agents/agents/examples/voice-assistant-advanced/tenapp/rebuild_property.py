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

# Find existing apollo graph to extract apollo prompt and thymia_analyzer config
apollo_prompt = None
apollo_greeting = None
thymia_analyzer_config = None

for graph in data.get("ten", {}).get("predefined_graphs", []):
    if "apollo" in graph.get("name", ""):
        nodes = graph.get("graph", {}).get("nodes", [])
        for node in nodes:
            if node.get("addon") == "openai_llm2_python":
                apollo_prompt = node.get("property", {}).get("prompt")
                apollo_greeting = node.get("property", {}).get("greeting")
            elif node.get("addon") == "thymia_analyzer_python":
                thymia_analyzer_config = copy.deepcopy(node.get("property", {}))
        break

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
            "endpointing": 300,
            "utterance_end_ms": 1000,
        }
    },
}

flux_stt_300ms = {
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
            "endpointing": 300,
            "utterance_end_ms": 1000,
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
        "anam_base_url": "https://anam-lab-git-feat-agora-anam-20452f3b.vercel.app/v1",
        "anam_avatar_id": "edf6fdcb-acab-44b8-b974-ded72665ee26",
        "anam_cluster": "dev-eu",
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

# Basic voice assistant connections (no tools)
basic_connections = [
    {
        "extension": "main_control",
        "cmd": [
            {
                "names": ["on_user_joined", "on_user_left"],
                "source": [{"extension": "agora_rtc"}],
            }
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
        "cmd": [
            {"names": ["flush"], "source": [{"extension": "main_control"}]}
        ],
        "data": [{"name": "text_data", "source": [{"extension": "llm"}]}],
        "audio_frame": [
            {"name": "pcm_frame", "dest": [{"extension": "agora_rtc"}]}
        ],
    },
]


# Helper function to create basic voice assistant graph (no tools)
def create_basic_voice_assistant(name, has_avatar=False, avatar_type=None):
    # Standard architecture with TTS and main_control
    nodes = [
        copy.deepcopy(agora_rtc_base),
        copy.deepcopy(nova3_stt_100ms),
        copy.deepcopy(llama_llm_no_tools),
        copy.deepcopy(cartesia_tts_sonic3),
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

                # Replace cmd with flush to avatar (no source, only dest)
                conn["cmd"] = [
                    {"name": "flush", "dest": [{"extension": "avatar"}]}
                ]

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
    name, llm_config, stt_config, has_avatar=False, avatar_type=None
):
    nodes = [
        copy.deepcopy(agora_rtc_base),
        copy.deepcopy(stt_config),
        copy.deepcopy(llm_config),
        copy.deepcopy(cartesia_tts_sonic3),
        copy.deepcopy(thymia_analyzer),
        copy.deepcopy(main_control_apollo),
        copy.deepcopy(message_collector),
        copy.deepcopy(streamid_adapter),
    ]

    # Base apollo connections (without avatar)
    connections = [
        {
            "extension": "main_control",
            "cmd": [
                {
                    "names": ["on_user_joined", "on_user_left"],
                    "source": [{"extension": "agora_rtc"}],
                },
                {
                    "names": ["tool_register"],
                    "source": [{"extension": "thymia_analyzer"}],
                },
            ],
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
    ]

    if has_avatar:
        nodes.append(
            copy.deepcopy(heygen_avatar)
            if avatar_type == "heygen"
            else copy.deepcopy(anam_avatar)
        )

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
                {
                    "name": "tts_audio_start",
                    "dest": [{"extension": "thymia_analyzer"}],
                },
                {
                    "name": "tts_audio_end",
                    "dest": [{"extension": "thymia_analyzer"}],
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

# Group 1: Basic voice assistants (no tools)
print("Creating basic voice assistant graphs...")
new_graphs.append(
    create_basic_voice_assistant("voice_assistant", has_avatar=False)
)
new_graphs.append(
    create_basic_voice_assistant(
        "voice_assistant_heygen", has_avatar=True, avatar_type="heygen"
    )
)
new_graphs.append(
    create_basic_voice_assistant(
        "voice_assistant_anam", has_avatar=True, avatar_type="anam"
    )
)

# Group 2: OSS graphs
print("Creating OSS apollo graphs...")
new_graphs.append(
    create_apollo_graph(
        "nova3_apollo_oss_cartesia_heygen",
        groq_oss_llm_with_tools,
        nova3_stt_300ms,
        has_avatar=True,
        avatar_type="heygen",
    )
)
new_graphs.append(
    create_apollo_graph(
        "nova3_apollo_oss_cartesia_anam",
        groq_oss_llm_with_tools,
        nova3_stt_300ms,
        has_avatar=True,
        avatar_type="anam",
    )
)

# Group 3: GPT-4o graphs
print("Creating GPT-4o apollo graphs...")
new_graphs.append(
    create_apollo_graph(
        "nova3_apollo_gpt_4o_cartesia_heygen",
        gpt4o_llm_with_tools,
        nova3_stt_300ms,
        has_avatar=True,
        avatar_type="heygen",
    )
)
new_graphs.append(
    create_apollo_graph(
        "nova3_apollo_gpt_4o_cartesia_anam",
        gpt4o_llm_with_tools,
        nova3_stt_300ms,
        has_avatar=True,
        avatar_type="anam",
    )
)
new_graphs.append(
    create_apollo_graph(
        "flux_apollo_gpt_4o_cartesia_heygen",
        gpt4o_llm_with_tools,
        flux_stt_300ms,
        has_avatar=True,
        avatar_type="heygen",
    )
)
new_graphs.append(
    create_apollo_graph(
        "flux_apollo_gpt_4o_cartesia_anam",
        gpt4o_llm_with_tools,
        flux_stt_300ms,
        has_avatar=True,
        avatar_type="anam",
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
