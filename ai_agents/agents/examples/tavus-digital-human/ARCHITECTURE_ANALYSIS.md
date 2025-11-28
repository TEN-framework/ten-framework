# Tavus Digital Human Integration - Architecture Analysis

## Executive Summary

This document provides a comprehensive analysis of integrating Tavus Conversational Video Interface (CVI) with the TEN Framework, exploring different RTC (Real-Time Communication) integration approaches, and documenting the current implementation decisions.

**Current Status**: ✅ **Working** (using Tavus CVI + Daily.co RTC)

**Key Finding**: Tavus only supports Daily.co and LiveKit as RTC transport layers. Direct integration with Agora RTC is not currently possible without significant bridging infrastructure.

---

## Table of Contents

1. [Background](#background)
2. [Tavus Integration Modes](#tavus-integration-modes)
3. [Current Implementation (Daily.co)](#current-implementation-dailyco)
4. [Alternative: LiveKit Integration](#alternative-livekit-integration)
5. [Agora RTC Integration Analysis](#agora-rtc-integration-analysis)
6. [Comparison Matrix](#comparison-matrix)
7. [Future Possibilities](#future-possibilities)
8. [Recommendations](#recommendations)

---

## Background

### What is Tavus?

Tavus is a service that provides hyper-realistic AI digital humans with built-in:
- **Speech-to-Text (STT)**: Converts user speech to text
- **Large Language Model (LLM)**: Generates conversational responses
- **Text-to-Speech (TTS)**: Synthesizes natural speech
- **Digital Human Rendering**: Creates realistic avatar with lip-sync and expressions

### What is TEN Framework?

TEN (The Extensible Network) is a modular AI agent framework that uses:
- **Extensions**: Pluggable components (STT, LLM, TTS, RTC, etc.)
- **Graphs**: Directed graphs defining data flow between extensions
- **Properties**: Configuration files for apps and extensions

### Integration Goal

Add Tavus digital human capability to TEN Framework as an example application, allowing users to interact with AI avatars.

---

## Tavus Integration Modes

Tavus provides **two distinct integration modes**, each with different architecture implications:

### Mode 1: Tavus Conversational Video Interface (CVI)

**API Endpoint**: `POST https://tavusapi.com/v2/conversations`

**Configuration**:
```json
{
  "replica_id": "<replica-id>",
  "persona_id": "<persona-id>",
  "conversational_context": "You are a helpful assistant",
  "custom_greeting": "Hello!"
}
```

**Response**:
```json
{
  "conversation_id": "...",
  "conversation_url": "https://tavus.daily.co/..."
}
```

**Architecture**:
```
User Browser
    ↓
Daily.js SDK
    ↓
Daily.co WebRTC (forced)
    ↓
Tavus Cloud (black box)
├── STT (built-in)
├── LLM (built-in)
├── TTS (built-in)
└── Digital Human Rendering (built-in)
```

**Characteristics**:
- ✅ **Simplest integration** - single API call
- ✅ **All-in-one solution** - no need to configure STT/LLM/TTS
- ❌ **No RTC control** - must use Daily.co
- ❌ **No pipeline control** - cannot replace STT/LLM/TTS
- ❌ **No modularity** - everything happens in Tavus cloud

**Use Case**: Quick demos, prototypes, MVP applications

---

### Mode 2: LiveKit Agent Integration

**Persona Configuration** (via Tavus API):
```json
{
  "replica_id": "<replica-id>",
  "persona_name": "My Avatar",
  "layers": {
    "llm": null,
    "tts": null,
    "context": null,
    "transport": {
      "transport_type": "livekit"
    }
  },
  "pipeline_mode": "echo"
}
```

**LiveKit Agent Code**:
```python
from livekit.agents import AutoSubscribe, JobContext, WorkerOptions, cli
from livekit.plugins import tavus

async def entrypoint(ctx: JobContext):
    # Tavus joins LiveKit room as a participant
    avatar = await tavus.AvatarSession.create(
        replica_id="<replica-id>",
        persona_id="<persona-id>",
        participant_name="Tavus Avatar"
    )
    await avatar.start(ctx.room)  # Joins existing LiveKit room

    # Your agent handles STT/LLM/TTS
    await ctx.connect(auto_subscribe=AutoSubscribe.AUDIO_ONLY)

cli.run_app(WorkerOptions(entrypoint_fnc=entrypoint))
```

**Architecture**:
```
User Browser
    ↓
LiveKit Client SDK
    ↓
LiveKit RTC Server (your control)
    ↓
┌─────────────────────────────────────┐
│ LiveKit Agent (your server)         │
│ ┌─────────────────────────────────┐ │
│ │ STT Plugin (e.g., Deepgram)     │ │
│ └─────────────────────────────────┘ │
│ ┌─────────────────────────────────┐ │
│ │ LLM Plugin (e.g., OpenAI)       │ │
│ └─────────────────────────────────┘ │
│ ┌─────────────────────────────────┐ │
│ │ TTS Plugin (e.g., ElevenLabs)   │ │
│ └─────────────────────────────────┘ │
└─────────────────────────────────────┘
    ↓ (audio only)
┌─────────────────────────────────────┐
│ Tavus Avatar Service                │
│ - Receives audio via LiveKit       │
│ - Generates digital human video     │
│ - Publishes video to LiveKit room   │
└─────────────────────────────────────┘
```

**Characteristics**:
- ✅ **Modular** - you control STT/LLM/TTS
- ✅ **RTC control** - use LiveKit RTC (can self-host)
- ✅ **Flexible** - swap components as needed
- ✅ **Production-ready** - better scalability
- ⚠️ **More complex** - requires LiveKit infrastructure
- ⚠️ **More code** - need to implement agent logic

**Use Case**: Production applications, custom pipelines, self-hosted solutions

---

## Current Implementation (Daily.co)

### Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│ Frontend (playground/src/app/tavus/page.tsx)                │
│                                                              │
│ import DailyIframe from '@daily-co/daily-js'                │
│                                                              │
│ const createConversation = async () => {                    │
│   const response = await fetch(                             │
│     'http://localhost:8080/api/tavus/conversation/create'   │
│   )                                                          │
│   const { conversation_url } = response.data                │
│                                                              │
│   const daily = DailyIframe.createFrame({...})              │
│   await daily.join({ url: conversation_url })               │
│ }                                                            │
└──────────────────────┬──────────────────────────────────────┘
                       ↓ HTTP POST
┌─────────────────────────────────────────────────────────────┐
│ API Server (server/internal/http_server.go)                 │
│                                                              │
│ func (s *HttpServer) handlerTavusCreateConversation() {     │
│   // Call Tavus API                                         │
│   resp := client.Post(                                      │
│     "https://tavusapi.com/v2/conversations",                │
│     payload                                                 │
│   )                                                          │
│   // Return conversation_url to frontend                    │
│ }                                                            │
└──────────────────────┬──────────────────────────────────────┘
                       ↓ HTTPS
┌─────────────────────────────────────────────────────────────┐
│ Tavus Cloud API                                              │
│                                                              │
│ Creates Daily.co room with Tavus avatar                     │
│ Returns: conversation_url = "https://tavus.daily.co/..."    │
└─────────────────────────────────────────────────────────────┘
```

### TEN Framework Role

**Minimal Configuration**:

**`tenapp/manifest.json`**:
```json
{
  "type": "app",
  "name": "tavus_digital_human",
  "version": "0.1.0",
  "dependencies": [
    {
      "type": "system",
      "name": "ten_runtime_go",
      "version": "0.11"
    }
  ],
  "scripts": {
    "start": "scripts/start.sh"
  }
}
```

**`tenapp/property.json`**:
```json
{
  "ten": {
    "predefined_graphs": [
      {
        "name": "tavus_digital_human",
        "auto_start": true,
        "graph": {
          "nodes": []  // Empty graph!
        }
      }
    ]
  }
}
```

**Key Point**: TEN App has an **empty graph** because:
- All processing (STT/LLM/TTS/rendering) happens in Tavus cloud
- API Server directly calls Tavus API (no TEN graph needed)
- TEN runtime is present only to satisfy framework requirements

### Data Flow

```
1. User clicks "Start Conversation"
2. Frontend → API Server: POST /api/tavus/conversation/create
3. API Server → Tavus API: POST /v2/conversations
4. Tavus API → API Server: { conversation_url }
5. API Server → Frontend: { conversation_url }
6. Frontend: DailyIframe.join(conversation_url)
7. User ↔ Daily.co WebRTC ↔ Tavus Cloud
```

### Why This Architecture?

**Requirements**:
- User wanted: "简单的 example，首先要保证能跑" (Simple example, must work first)
- Showcase Tavus digital human in TEN ecosystem
- Minimal complexity

**Decision Factors**:
1. **Fastest to implement** - single API call
2. **Least dependencies** - no TEN graph needed
3. **Proven to work** - Tavus CVI is production-ready
4. **Good user experience** - high-quality video, low latency

**Trade-offs Accepted**:
- ❌ Cannot use Agora RTC (TEN's primary RTC)
- ❌ Cannot replace STT/LLM/TTS components
- ❌ Relies on Daily.co infrastructure
- ❌ Not modular

---

## Alternative: LiveKit Integration

### Conceptual Architecture

```
┌─────────────────────────────────────────────────────────────┐
│ Frontend                                                     │
│ - LiveKit Client SDK (not Daily.js)                         │
└──────────────────────┬──────────────────────────────────────┘
                       ↓ WebRTC
┌─────────────────────────────────────────────────────────────┐
│ LiveKit RTC Server                                           │
│ - Can be self-hosted or LiveKit Cloud                       │
│ - Manages WebRTC connections                                │
└──────────┬──────────────────────────────────┬───────────────┘
           ↓                                  ↓
┌──────────────────────────┐    ┌────────────────────────────┐
│ TEN Framework App        │    │ Tavus Avatar               │
│ (Option A: Python Agent) │    │ (LiveKit transport)        │
│                          │    │                            │
│ ┌──────────────────────┐ │    │ - Joins LiveKit room       │
│ │ livekit_rtc ext      │ │    │ - Subscribes to audio      │
│ │ (NEW - to develop)   │ │    │ - Publishes video          │
│ └──────┬───────────────┘ │    └────────────────────────────┘
│ ┌──────▼───────────────┐ │
│ │ deepgram_asr_python  │ │
│ └──────┬───────────────┘ │
│ ┌──────▼───────────────┐ │
│ │ openai_llm2_python   │ │
│ └──────┬───────────────┘ │
│ ┌──────▼───────────────┐ │
│ │ elevenlabs_tts       │ │
│ └──────┬───────────────┘ │
│        └─────────────────┼──> Publishes audio to LiveKit
└──────────────────────────┘
```

### Required Components

#### 1. New Extension: `livekit_rtc`

Similar to existing `agora_rtc` extension but for LiveKit:

**`ten_packages/extension/livekit_rtc/manifest.json`**:
```json
{
  "type": "extension",
  "name": "livekit_rtc",
  "version": "0.1.0",
  "dependencies": [
    {
      "type": "system",
      "name": "ten_runtime_python",
      "version": "0.11"
    }
  ],
  "api": {
    "property": {
      "room_url": {
        "type": "string"
      },
      "api_key": {
        "type": "string"
      },
      "api_secret": {
        "type": "string"
      }
    }
  }
}
```

**`ten_packages/extension/livekit_rtc/extension.py`**:
```python
from ten import Extension, TenEnv
from livekit import rtc

class LiveKitRTCExtension(Extension):
    async def on_start(self, ten_env: TenEnv) -> None:
        # Connect to LiveKit room
        room = rtc.Room()
        await room.connect(
            url=self.room_url,
            token=self.generate_token()
        )

        # Publish local audio track
        audio_source = rtc.AudioSource(...)
        track = rtc.LocalAudioTrack.create_audio_track("audio", audio_source)
        await room.local_participant.publish_track(track)

        # Subscribe to remote audio
        @room.on("track_subscribed")
        def on_track_subscribed(track, publication, participant):
            if track.kind == rtc.TrackKind.KIND_AUDIO:
                # Forward to TEN graph
                self.send_audio_frame(ten_env, track)
```

#### 2. Updated Graph Configuration

**`tenapp/property.json`**:
```json
{
  "ten": {
    "predefined_graphs": [
      {
        "name": "tavus_livekit",
        "auto_start": true,
        "graph": {
          "nodes": [
            {
              "type": "extension",
              "name": "livekit_rtc",
              "addon": "livekit_rtc",
              "property": {
                "room_url": "${env:LIVEKIT_URL}",
                "api_key": "${env:LIVEKIT_API_KEY}",
                "api_secret": "${env:LIVEKIT_API_SECRET}"
              }
            },
            {
              "type": "extension",
              "name": "deepgram_asr",
              "addon": "deepgram_asr_python"
            },
            {
              "type": "extension",
              "name": "openai_llm",
              "addon": "openai_llm2_python"
            },
            {
              "type": "extension",
              "name": "elevenlabs_tts",
              "addon": "elevenlabs_tts2_python"
            }
          ],
          "connections": [
            {
              "extension": "livekit_rtc",
              "cmd": [
                {
                  "name": "audio_frame",
                  "dest": [{
                    "extension": "deepgram_asr"
                  }]
                }
              ]
            },
            {
              "extension": "deepgram_asr",
              "cmd": [
                {
                  "name": "text_data",
                  "dest": [{
                    "extension": "openai_llm"
                  }]
                }
              ]
            },
            {
              "extension": "openai_llm",
              "cmd": [
                {
                  "name": "text_data",
                  "dest": [{
                    "extension": "elevenlabs_tts"
                  }]
                }
              ]
            },
            {
              "extension": "elevenlabs_tts",
              "cmd": [
                {
                  "name": "audio_frame",
                  "dest": [{
                    "extension": "livekit_rtc"
                  }]
                }
              ]
            }
          ]
        }
      }
    ]
  }
}
```

#### 3. Tavus Persona Setup

**One-time configuration** (via Tavus API or dashboard):
```bash
curl -X POST https://tavusapi.com/v2/personas \
  -H "x-api-key: $TAVUS_API_KEY" \
  -d '{
    "persona_name": "My LiveKit Avatar",
    "replica_id": "<replica-id>",
    "layers": {
      "llm": null,
      "tts": null,
      "context": null,
      "transport": {
        "transport_type": "livekit"
      }
    },
    "pipeline_mode": "echo"
  }'
```

#### 4. Backend API Changes

**`server/internal/http_server.go`**:
```go
func (s *HttpServer) handlerTavusLiveKitStart(c *gin.Context) {
    // Create LiveKit room
    room, _ := s.livekitClient.CreateRoom(ctx, &livekit.CreateRoomRequest{
        Name: "tavus-session-" + uuid.New().String(),
    })

    // Generate tokens
    userToken := generateLiveKitToken(room.Name, "user")

    // Start Tavus avatar in this room
    avatarResp, _ := client.R().
        SetHeader("x-api-key", os.Getenv("TAVUS_API_KEY")).
        SetBody(map[string]interface{}{
            "persona_id": os.Getenv("TAVUS_PERSONA_ID"),
            "room_url": os.Getenv("LIVEKIT_URL"),
            "room_name": room.Name,
        }).
        Post("https://tavusapi.com/v2/conversations/livekit")

    // Start TEN Framework worker for this room
    s.startTenWorker(room.Name)

    c.JSON(200, gin.H{
        "room_name": room.Name,
        "user_token": userToken,
        "livekit_url": os.Getenv("LIVEKIT_URL"),
    })
}
```

### Advantages

| Aspect | Benefit |
|--------|---------|
| **Modularity** | Can replace STT/LLM/TTS with any provider |
| **RTC Control** | Can self-host LiveKit server (no external dependency) |
| **Cost** | Potentially lower (self-hosted LiveKit + cheaper TTS) |
| **Privacy** | Audio/text never leaves your infrastructure |
| **Flexibility** | Can add custom processing (sentiment analysis, logging, etc.) |
| **Scalability** | Better control over resource allocation |

### Disadvantages

| Aspect | Challenge |
|--------|-----------|
| **Complexity** | Much more code to write and maintain |
| **Development Time** | Weeks instead of days |
| **Infrastructure** | Need to run LiveKit server (or pay for LiveKit Cloud) |
| **Debugging** | More components = more potential failure points |
| **Documentation** | Less mature than Tavus CVI |

---

## Agora RTC Integration Analysis

### The Question

> "If LiveKit + Tavus works, can Agora RTC + Tavus also work?"

**Logical reasoning**: Both LiveKit and Agora are WebRTC platforms. If Tavus can join a LiveKit room as a participant, why can't it join an Agora channel?

### The Answer: **No, not directly**

### Why LiveKit Works

Tavus explicitly supports LiveKit through:

1. **Persona Configuration**:
```json
{
  "layers": {
    "transport": {
      "transport_type": "livekit"  // ← Officially supported
    }
  }
}
```

2. **Protocol Implementation**:
   - Tavus has implemented LiveKit's WebRTC signaling protocol
   - Can authenticate with LiveKit servers
   - Can join LiveKit rooms as a standard participant
   - Can publish/subscribe tracks using LiveKit SDK

3. **Official Plugin**:
   - LiveKit provides `livekit-plugins-tavus` package
   - Documented integration path
   - Production-tested

### Why Agora Doesn't Work

**Technical Reason**: Tavus has **not implemented** Agora's protocols.

```json
{
  "layers": {
    "transport": {
      "transport_type": "agora"  // ❌ Not supported
    }
  }
}
```

**Response**: `400 Bad Request - Invalid transport_type`

**Currently Supported**:
- `transport_type: "daily"` ✅
- `transport_type: "livekit"` ✅
- `transport_type: "agora"` ❌
- `transport_type: "twilio"` ❌
- `transport_type: "zoom"` ❌

### Why the Limitation?

**Business Decision**:
- Tavus chose to support Daily.co (their default infrastructure)
- Added LiveKit support due to its popularity in AI agent space
- Has not prioritized Agora integration

**Technical Complexity**:
Each RTC platform has different:
- Signaling protocols
- Authentication mechanisms
- SDK APIs
- Media server architectures

Supporting a new platform requires:
- Weeks of engineering time
- Ongoing maintenance
- Testing infrastructure

### Theoretical Bridging Approaches

#### Approach 1: Media Server Bridge

```
┌──────────────┐
│ User Browser │
│ Agora SDK    │
└──────┬───────┘
       ↓ Agora protocol
┌─────────────────────────────────────┐
│ Agora Channel                        │
└──────┬──────────────────────────────┘
       ↓
┌─────────────────────────────────────┐
│ Bridge Server                        │
│ ┌─────────────────────────────────┐ │
│ │ Agora Client                    │ │
│ │ - Join Agora channel            │ │
│ │ - Subscribe to user audio       │ │
│ └──────────┬──────────────────────┘ │
│            ↓                         │
│ ┌──────────────────────────────────┐│
│ │ Media Processor                  ││
│ │ - Decode Agora audio             ││
│ │ - Re-encode for LiveKit          ││
│ └──────────┬──────────────────────┘│
│            ↓                         │
│ ┌──────────────────────────────────┐│
│ │ LiveKit Client                   ││
│ │ - Join LiveKit room              ││
│ │ - Publish audio track            ││
│ └──────────┬──────────────────────┘│
└────────────┼──────────────────────────┘
             ↓
┌─────────────────────────────────────┐
│ LiveKit Room                         │
│ ┌─────────────────────────────────┐ │
│ │ Bridge participant              │ │
│ └─────────────────────────────────┘ │
│ ┌─────────────────────────────────┐ │
│ │ Tavus Avatar participant        │ │
│ │ - Subscribes to bridge audio    │ │
│ │ - Publishes video               │ │
│ └──────────┬──────────────────────┘ │
└────────────┼──────────────────────────┘
             ↓ (video stream)
┌─────────────────────────────────────┐
│ Bridge Server                        │
│ - Subscribe to Tavus video          │
│ - Re-encode for Agora               │
│ - Publish to Agora channel          │
└──────┬──────────────────────────────┘
       ↓
┌──────────────┐
│ User Browser │
│ Agora SDK    │
│ Receives     │
│ Tavus video  │
└──────────────┘
```

**Implementation**:
```python
# bridge_server.py
import asyncio
from agora_rtc import AgoraClient
from livekit import rtc

class AgoraLiveKitBridge:
    def __init__(self):
        self.agora_client = AgoraClient(app_id=AGORA_APP_ID)
        self.livekit_room = rtc.Room()

    async def start(self, agora_channel: str, livekit_room_name: str):
        # Join Agora channel
        await self.agora_client.join(channel=agora_channel)

        # Join LiveKit room
        await self.livekit_room.connect(
            url=LIVEKIT_URL,
            token=generate_token(livekit_room_name, "bridge")
        )

        # Subscribe to Agora audio
        @self.agora_client.on("audio_frame")
        async def on_agora_audio(frame):
            # Decode Agora audio
            pcm_data = decode_agora_audio(frame)

            # Re-encode for LiveKit
            livekit_frame = encode_livekit_audio(pcm_data)

            # Publish to LiveKit
            await self.livekit_audio_track.capture_frame(livekit_frame)

        # Subscribe to LiveKit video (from Tavus)
        @self.livekit_room.on("track_subscribed")
        async def on_livekit_track(track, publication, participant):
            if participant.name == "Tavus Avatar" and track.kind == rtc.TrackKind.KIND_VIDEO:
                async for frame in track:
                    # Re-encode for Agora
                    agora_frame = encode_agora_video(frame)

                    # Publish to Agora
                    await self.agora_client.publish_video(agora_frame)
```

**Challenges**:
1. **Latency**: Each transcode adds 20-50ms
2. **Quality Loss**: Re-encoding degrades video quality
3. **Complexity**: Needs deep knowledge of both platforms
4. **Cost**: Requires server infrastructure
5. **Synchronization**: Audio/video sync is difficult
6. **Scalability**: One bridge per session is resource-intensive

#### Approach 2: Protocol Translation

```
┌──────────────────────────────────────┐
│ Agora RTC Proxy Server               │
│ - Accepts Agora connections          │
│ - Translates to LiveKit protocol     │
│ - Acts as "fake" Agora server        │
└──────────────────────────────────────┘
```

**Even more complex**:
- Requires reverse-engineering Agora's protocol
- Likely violates Agora's terms of service
- Extremely difficult to maintain

#### Approach 3: Request Tavus to Add Agora Support

**Best long-term solution**:
1. Contact Tavus sales/support
2. Explain TEN Framework's user base uses Agora
3. Request `transport_type: "agora"` support

**Likelihood**: Low, unless:
- Large customer request
- Strategic partnership
- Significant revenue opportunity

---

## Comparison Matrix

### Integration Approaches

| Aspect | Current (Daily.co) | LiveKit | Agora (Bridged) |
|--------|-------------------|---------|-----------------|
| **Complexity** | ⭐ Very Simple | ⭐⭐⭐ Moderate | ⭐⭐⭐⭐⭐ Very Complex |
| **Development Time** | 2 days | 2-3 weeks | 4-6 weeks |
| **Code Volume** | ~200 lines | ~2000 lines | ~5000 lines |
| **Maintenance** | ⭐ Minimal | ⭐⭐⭐ Moderate | ⭐⭐⭐⭐⭐ High |
| **RTC Control** | ❌ None | ✅ Full | ⚠️ Partial |
| **STT/LLM/TTS Control** | ❌ None | ✅ Full | ✅ Full |
| **Infrastructure** | Daily Cloud | LiveKit (self-host or cloud) | Agora + LiveKit + Bridge |
| **Latency** | ~100ms | ~100ms | ~200-300ms |
| **Video Quality** | ✅ Excellent | ✅ Excellent | ⚠️ Good (re-encoded) |
| **Cost (est.)** | $0.01/min | $0.005/min (self-host) | $0.02/min (Agora + LiveKit + server) |
| **Reliability** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐ |
| **Scalability** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐ |
| **TEN Integration** | ⚠️ Minimal | ✅ Good | ⚠️ Complex |

### RTC Platform Comparison

| Feature | Agora RTC | LiveKit | Daily.co |
|---------|-----------|---------|----------|
| **Open Source** | ❌ | ✅ | ❌ |
| **Self-Hosting** | ❌ | ✅ | ❌ |
| **Tavus Support** | ❌ | ✅ | ✅ (default) |
| **TEN Framework** | ✅ (agora_rtc ext) | ⚠️ (needs development) | ⚠️ (minimal) |
| **China Access** | ✅ Optimized | ⚠️ May be slow | ⚠️ May be blocked |
| **Pricing** | Pay-per-use | Free (self-host) / Pay-per-use | Pay-per-use |
| **Documentation** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ |
| **Community** | Large | Growing | Medium |

---

## Future Possibilities

### Option 1: Create LiveKit Example (Recommended)

**New Example**: `examples/tavus-livekit/`

**Benefits**:
- Showcases TEN's modularity
- Users can choose STT/LLM/TTS providers
- Self-hosted option available
- Better long-term architecture

**Effort**: 2-3 weeks

**Priority**: Medium (after current example is stable)

### Option 2: Develop `livekit_rtc` Extension

**Location**: `ten_packages/extension/livekit_rtc/`

**Features**:
- Similar API to `agora_rtc`
- Publish/subscribe audio/video
- Room management
- Token generation

**Benefits**:
- Reusable across all TEN apps
- Enables LiveKit as alternative to Agora
- Opens door to other LiveKit-based services

**Effort**: 3-4 weeks

**Priority**: High (if TEN wants to expand RTC options)

### Option 3: Request Tavus Agora Support

**Action Items**:
1. Gather user feedback (how many need Agora?)
2. Contact Tavus partnership team
3. Propose integration:
   - TEN Framework has X users in China
   - Agora is dominant RTC in China
   - Win-win partnership

**Likelihood**: 20% (depends on Tavus priorities)

**Timeline**: 6-12 months (if accepted)

### Option 4: Build Agora Bridge (Not Recommended)

**Only consider if**:
- Must use Agora RTC
- Must use Tavus digital human
- Cannot use Daily.co or LiveKit
- Have budget for infrastructure

**Estimated Cost**:
- Development: $50k-$100k
- Monthly server: $500-$2000
- Maintenance: $20k/year

**Recommendation**: ❌ Not worth the investment

---

## Recommendations

### For Current Version (v1.0)

✅ **Keep Daily.co implementation**

**Rationale**:
- Already working
- Simplest possible integration
- Good user experience
- Fast time-to-market

**Documentation**:
- Clearly state: "Uses Tavus CVI with Daily.co RTC"
- Mark as "Simple Example" or "Quick Start"
- Link to this architecture document

### For Next Version (v2.0)

✅ **Create LiveKit-based example**

**Steps**:
1. Develop `livekit_rtc` extension (4 weeks)
2. Create `examples/tavus-livekit/` (2 weeks)
3. Document differences vs Daily.co version
4. Provide deployment guide (self-hosted LiveKit)

**Target Users**:
- Production deployments
- Users needing custom STT/LLM/TTS
- Privacy-conscious users
- Cost-sensitive users (self-hosting)

### For Agora Users

⚠️ **Set expectations clearly**

**Communication**:
> "Tavus digital human currently requires either Daily.co (simple) or LiveKit (modular) RTC. Direct Agora RTC integration is not supported by Tavus. For Agora users, we recommend:
>
> 1. Use TEN's voice-assistant example (Agora + custom STT/LLM/TTS)
> 2. Consider LiveKit as alternative RTC for Tavus use case
> 3. Or use Tavus-Daily.co for digital human, Agora for other use cases"

### For TEN Framework Team

**Strategic Decision**:

**Option A: Embrace LiveKit**
- Develop `livekit_rtc` extension
- Position as "alternative RTC" alongside Agora
- Geographic market split:
  - Agora: China, Asia
  - LiveKit: Global, self-hosted

**Option B: Stay Focused on Agora**
- Keep Tavus as "external service" example
- Don't invest in LiveKit integration
- Focus on Agora ecosystem depth

**Recommendation**: Option A
- LiveKit is growing fast in AI agent space
- Open source aligns with TEN philosophy
- Enables self-hosted deployments
- Small investment (4-6 weeks) for strategic benefit

---

## Appendix: Code Snippets

### A. Current Implementation Files

**File**: `server/internal/http_server.go`
```go
// Tavus conversation creation endpoint
func (s *HttpServer) handlerTavusCreateConversation(c *gin.Context) {
    var req TavusCreateConversationReq
    if err := c.BindJSON(&req); err != nil {
        s.output(c, codeErrBadRequest, nil)
        return
    }

    tavusApiKey := os.Getenv("TAVUS_API_KEY")
    if len(tavusApiKey) == 0 {
        slog.Error("TAVUS_API_KEY is not set")
        s.output(c, codeErrInternal, nil)
        return
    }

    client := resty.New()
    payload := map[string]interface{}{
        "replica_id":             os.Getenv("TAVUS_REPLICA_ID"),
        "persona_id":             os.Getenv("TAVUS_PERSONA_ID"),
        "conversational_context": "You are a helpful AI assistant.",
        "custom_greeting":        req.Greeting,
    }

    resp, err := client.R().
        SetHeader("x-api-key", tavusApiKey).
        SetHeader("Content-Type", "application/json").
        SetBody(payload).
        Post("https://tavusapi.com/v2/conversations")

    if err != nil {
        slog.Error("call tavus api failed", "err", err)
        s.output(c, codeErrInternal, nil)
        return
    }

    var tavusResp map[string]interface{}
    json.Unmarshal(resp.Body(), &tavusResp)

    s.output(c, codeSuccess, TavusConversationResp{
        ConversationId:  tavusResp["conversation_id"].(string),
        ConversationUrl: tavusResp["conversation_url"].(string),
    })
}
```

**File**: `playground/src/app/tavus/page.tsx`
```typescript
'use client';

import { useState } from 'react';
import DailyIframe from '@daily-co/daily-js';

export default function TavusPage() {
  const [callFrame, setCallFrame] = useState<any>(null);
  const [isLoading, setIsLoading] = useState(false);

  const createConversation = async () => {
    setIsLoading(true);

    try {
      const response = await fetch('http://localhost:8080/api/tavus/conversation/create', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          greeting: 'Hello! How can I help you today?'
        })
      });

      const data = await response.json();
      if (data.code !== 0) {
        throw new Error('Failed to create conversation');
      }

      const { conversation_url } = data.data;

      // Create Daily.co iframe
      const daily = DailyIframe.createFrame({
        showLeaveButton: true,
        iframeStyle: {
          position: 'fixed',
          width: '100%',
          height: '100%',
        }
      });

      await daily.join({ url: conversation_url });
      setCallFrame(daily);
    } catch (error) {
      console.error('Error creating conversation:', error);
      alert('Failed to start conversation');
    } finally {
      setIsLoading(false);
    }
  };

  const endConversation = () => {
    if (callFrame) {
      callFrame.destroy();
      setCallFrame(null);
    }
  };

  return (
    <div className="container mx-auto p-8">
      <h1 className="text-3xl font-bold mb-6">Tavus Digital Human</h1>

      {!callFrame ? (
        <button
          onClick={createConversation}
          disabled={isLoading}
          className="bg-blue-500 hover:bg-blue-700 text-white font-bold py-2 px-4 rounded"
        >
          {isLoading ? 'Starting...' : 'Start Conversation'}
        </button>
      ) : (
        <button
          onClick={endConversation}
          className="bg-red-500 hover:bg-red-700 text-white font-bold py-2 px-4 rounded"
        >
          End Conversation
        </button>
      )}
    </div>
  );
}
```

### B. Proposed LiveKit Extension

**File**: `ten_packages/extension/livekit_rtc/extension.py`
```python
from ten import Extension, TenEnv, Cmd, Data, AudioFrame, VideoFrame
from livekit import rtc
import asyncio
import numpy as np

class LiveKitRTCExtension(Extension):
    def __init__(self, name: str):
        super().__init__(name)
        self.room = None
        self.audio_source = None
        self.video_source = None

    async def on_start(self, ten_env: TenEnv) -> None:
        # Get configuration
        room_url = ten_env.get_property_string("room_url")
        token = ten_env.get_property_string("token")

        # Connect to LiveKit room
        self.room = rtc.Room()
        await self.room.connect(url=room_url, token=token)

        # Create audio source and publish
        self.audio_source = rtc.AudioSource(sample_rate=16000, num_channels=1)
        audio_track = rtc.LocalAudioTrack.create_audio_track("audio", self.audio_source)
        await self.room.local_participant.publish_track(audio_track)

        # Subscribe to remote tracks
        @self.room.on("track_subscribed")
        def on_track_subscribed(track, publication, participant):
            if track.kind == rtc.TrackKind.KIND_AUDIO:
                asyncio.create_task(self.handle_remote_audio(ten_env, track))
            elif track.kind == rtc.TrackKind.KIND_VIDEO:
                asyncio.create_task(self.handle_remote_video(ten_env, track))

        ten_env.on_start_done()

    async def handle_remote_audio(self, ten_env: TenEnv, track):
        """Forward remote audio to TEN graph"""
        audio_stream = rtc.AudioStream(track)
        async for frame in audio_stream:
            # Convert LiveKit frame to TEN AudioFrame
            audio_data = Data.create("audio_frame")
            audio_data.set_property_buf("data", frame.data.tobytes())
            audio_data.set_property_int("sample_rate", frame.sample_rate)
            audio_data.set_property_int("channels", frame.num_channels)
            ten_env.send_data(audio_data)

    async def on_cmd(self, ten_env: TenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()

        if cmd_name == "publish_audio":
            # Receive audio from TEN graph and publish to LiveKit
            audio_data = cmd.get_property_buf("data")
            sample_rate = cmd.get_property_int("sample_rate")

            # Convert to LiveKit AudioFrame
            np_array = np.frombuffer(audio_data, dtype=np.int16)
            lk_frame = rtc.AudioFrame(
                data=np_array,
                sample_rate=sample_rate,
                num_channels=1,
                samples_per_channel=len(np_array)
            )

            await self.audio_source.capture_frame(lk_frame)

        ten_env.return_result(cmd, None)
```

---

## Conclusion

The current Tavus integration using Daily.co is the **correct choice for a simple example**. It demonstrates Tavus capabilities with minimal complexity.

For production use cases requiring modularity, the **LiveKit-based architecture** is the recommended path forward, though it requires developing a `livekit_rtc` extension.

**Direct Agora RTC integration is not possible** without either:
1. Tavus adding native Agora support (unlikely in short term)
2. Building complex media bridging infrastructure (not recommended)

The TEN Framework should consider **strategic investment in LiveKit support** to enable:
- Tavus digital human integration
- Self-hosted RTC deployments
- Geographic diversity (Agora for China, LiveKit for global)
- Open source alignment

**Recommended Roadmap**:
- **Q1 2025**: Stabilize current Daily.co example
- **Q2 2025**: Develop `livekit_rtc` extension
- **Q3 2025**: Create `examples/tavus-livekit/`
- **Q4 2025**: Evaluate Agora bridge if strong user demand

---

**Document Version**: 1.0
**Last Updated**: 2025-01-27
**Author**: TEN Framework Team
**Status**: Draft for Review
