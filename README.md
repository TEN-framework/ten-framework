<div align="center"> <a name="readme-top"></a>

![TEN banner][ten-framework-banner]

[![TEN Releases]( https://img.shields.io/github/v/release/ten-framework/ten-framework?color=369eff&labelColor=gray&logo=github&style=flat-square )](https://github.com/TEN-framework/ten-framework/releases)
[![](https://img.shields.io/github/release-date/ten-framework/ten-framework?labelColor=gray&style=flat-square)](https://github.com/TEN-framework/ten-framework/releases)
[![Discussion posts](https://img.shields.io/github/discussions/TEN-framework/ten_framework?labelColor=gray&color=%20%23f79009)](https://github.com/TEN-framework/ten-framework/discussions/)
[![Commits](https://img.shields.io/github/commit-activity/m/TEN-framework/ten_framework?labelColor=gray&color=pink)](https://github.com/TEN-framework/ten-framework/graphs/commit-activity)
[![Issues closed](https://img.shields.io/github/issues-search?query=repo%3ATEN-framework%2Ften-framework%20is%3Aclosed&label=issues%20closed&labelColor=gray&color=green)](https://github.com/TEN-framework/ten-framework/issues)
[![](https://img.shields.io/github/contributors/ten-framework/ten-framework?color=c4f042&labelColor=gray&style=flat-square)](https://github.com/TEN-framework/ten-framework/graphs/contributors)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome!-brightgreen.svg?style=flat-square)](https://github.com/TEN-framework/ten-framework/pulls)
[![GitHub license](https://img.shields.io/badge/License-Apache_2.0_with_certain_conditions-blue.svg?labelColor=%20%23155EEF&color=%20%23528bff)](https://github.com/TEN-framework/ten_framework/blob/main/LICENSE)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/TEN-framework/TEN-framework)
[![ReadmeX](https://raw.githubusercontent.com/CodePhiliaX/resource-trusteeship/main/readmex.svg)](https://readmex.com/TEN-framework/ten-framework)

[![GitHub watchers](https://img.shields.io/github/watchers/TEN-framework/ten_framework?style=social&label=Watch)](https://GitHub.com/TEN-framework/ten_framework/watchers/?WT.mc_id=academic-105485-koreyst)
[![GitHub forks](https://img.shields.io/github/forks/TEN-framework/ten_framework?style=social&label=Fork)](https://GitHub.com/TEN-framework/ten_framework/network/?WT.mc_id=academic-105485-koreyst)
[![GitHub stars](https://img.shields.io/github/stars/TEN-framework/ten_framework?style=social&label=Star)](https://GitHub.com/TEN-framework/ten_framework/stargazers/?WT.mc_id=academic-105485-koreyst)

<a href="https://github.com/TEN-framework/ten-framework/blob/main/README.md"><img alt="README in English" src="https://img.shields.io/badge/English-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-CN.md"><img alt="简体中文操作指南" src="https://img.shields.io/badge/简体中文-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-JP.md"><img alt="日本語のREADME" src="https://img.shields.io/badge/日本語-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-KR.md"><img alt="README in 한국어" src="https://img.shields.io/badge/한국어-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-ES.md"><img alt="README en Español" src="https://img.shields.io/badge/Español-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-FR.md"><img alt="README en Français" src="https://img.shields.io/badge/Français-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-IT.md"><img alt="README in Italiano" src="https://img.shields.io/badge/Italiano-lightgrey"></a>

[Official Site](https://theten.ai)
•
[Documentation](https://theten.ai/docs/ten_agent/overview)
•
[Blog](https://theten.ai/blog)

<a href="https://trendshift.io/repositories/11978" target="_blank"><img src="https://trendshift.io/api/badge/repositories/11978" alt="TEN-framework%2Ften_framework | Trendshift" style="width: 250px; height: 55px;" width="250" height="55"/></a>

</div>

<br>

<details>
  <summary><kbd>Table of Contents</kbd></summary>

#### Table of Contents

- [Welcome to TEN](#-welcome-to-ten)
- [Features](#-features)
  - [1️⃣ Real-time Avatar](#1️⃣-real-time-avatar)
  - [2️⃣ Real-time voice with MCP servers](#2️⃣-real-time-voice-with-mcp-servers)
  - [3️⃣ Real-time communication with hardware](#3️⃣-real-time-communication-with-hardware)
  - [4️⃣ Real-time vision and real-time screenshare detection](#4️⃣-real-time-vision-and-real-time-screenshare-detection)
  - [5️⃣ TEN with other LLM platforms](#5️⃣-ten-with-other-llm-platforms)
  - [6️⃣ StoryTeller - TEN image generation](#6️⃣-storyteller---ten-image-generation)
- [Get TEN up and running](#-get-ten-up-and-running)
  - [🅰️ Run TEN on localhost](#🅰️-run-ten-on-localhost)
  - [🅱️ Run TEN Agent in Codespaces (no Docker)](#🅱️-run-ten-agent-in-codespaces-no-docker)
- [Agent Examples](#-agent-examples)
- [️TEN Agent Self-Hosting](#️-ten-agent-self-hosting)
  - [🅰️ Deploying with Docker](#🅰️-deploying-with-docker)
  - [🅱️ Deploying with other cloud services](#🅱️-deploying-with-other-cloud-services)
- [TEN Ecosystem](#-ten-ecosystem)
- [Questions](#-ask-questions)
- [Contributing](#-contributing)
  - [Code Contributors](#code-contributors)
  - [Contribution Guidelines](#contribution-guidelines)
  - [License](#license)

<br/>

</details>

## Welcome to TEN

TEN is a comprehensive open-source ecosystem for creating, customizing, and deploying real-time conversational AI agents with multimodal capabilities including voice, vision, and avatar interactions.

TEN includes [TEN Framework](https://github.com/ten-framework/ten-framework), [TEN VAD](https://github.com/ten-framework/ten-vad), [TEN Turn Detection](https://github.com/ten-framework/ten-turn-detection) and [TEN Portal](https://github.com/ten-framework/portal). Check out [TEN Ecosystem](#-ten-ecosystem) for more details.

<br>

| Community Channel | Purpose |
| ---------------- | ------- |
| [![Follow on X](https://img.shields.io/twitter/follow/TenFramework?logo=X&color=%20%23f5f5f5)](https://twitter.com/intent/follow?screen_name=TenFramework) | Follow TEN Framework on X for updates and announcements |
| [![Follow on LinkedIn](https://custom-icon-badges.demolab.com/badge/LinkedIn-TEN_Framework-0A66C2?logo=linkedin-white&logoColor=fff)](https://www.linkedin.com/company/ten-framework) | Follow TEN Framework on LinkedIn for updates and announcements |
| [![Discord TEN Community](https://img.shields.io/badge/Discord-Join%20TEN%20Community-5865F2?style=flat&logo=discord&logoColor=white)](https://discord.gg/VnPftUzAMJ) | Join our Discord community to connect with developers |
| [![Hugging Face Space](https://img.shields.io/badge/Hugging%20Face-TEN%20Framework-yellow?style=flat&logo=huggingface)](https://huggingface.co/TEN-framework) | Join our Hugging Face community to explore our spaces and models |
| [![WeChat](https://img.shields.io/badge/TEN_Framework-WeChat_Group-%2307C160?logo=wechat&labelColor=darkgreen&color=gray)](https://github.com/TEN-framework/ten-agent/discussions/170) | Join our WeChat group for Chinese community discussions |

<br>

## Agent Examples

![Image](https://github.com/user-attachments/assets/f6744299-d987-4b26-8edd-8385204c2950)

### Multi-Purpose Voice Assistant

Build engaging AI avatars with TEN Agent using [Trulience](https://trulience.com)'s diverse collection of free avatar options. To get it up and running, you only need 2 steps:

1. Follow the README to finish setting up and running the Playground
2. Enter the avatar ID and [token](https://trulience.com/docs#/authentication/jwt-tokens/jwt-tokens?id=use-your-custom-userid) you get from [Trulience](https://trulience.com)

<br>

![image](https://github.com/user-attachments/assets/c763ffa6-0b9f-4599-9e50-8ea97021e412)

<br>

![Image](https://github.com/user-attachments/assets/b4028d45-1523-44a6-ae15-5a47f7af1c21)

### Anime Chompanion with Lip Sync

Build engaging AI avatars with TEN Agent using [Trulience](https://trulience.com)'s diverse collection of free avatar options. To get it up and running, you only need 2 steps:

1. Follow the README to finish setting up and running the Playground
2. Enter the avatar ID and [token](https://trulience.com/docs#/authentication/jwt-tokens/jwt-tokens?id=use-your-custom-userid) you get from [Trulience](https://trulience.com)

<br>

![image](https://github.com/user-attachments/assets/c763ffa6-0b9f-4599-9e50-8ea97021e412)

<br>

![Image](https://github.com/user-attachments/assets/44be01c1-9630-42d0-a30a-779cd78b92c5)

### Speech Diarization

Build engaging AI avatars with TEN Agent using [Trulience](https://trulience.com)'s diverse collection of free avatar options. To get it up and running, you only need 2 steps:

1. Follow the README to finish setting up and running the Playground
2. Enter the avatar ID and [token](https://trulience.com/docs#/authentication/jwt-tokens/jwt-tokens?id=use-your-custom-userid) you get from [Trulience](https://trulience.com)

<br>
<hr>
<br>

![Image](https://github.com/user-attachments/assets/2ef3a6c3-c370-43f7-86b1-46ee8d4c55da)

### Transcription

Build engaging AI avatars with TEN Agent using [Trulience](https://trulience.com)'s diverse collection of free avatar options. To get it up and running, you only need 2 steps:

1. Follow the README to finish setting up and running the Playground
2. Enter the avatar ID and [token](https://trulience.com/docs#/authentication/jwt-tokens/jwt-tokens?id=use-your-custom-userid) you get from [Trulience](https://trulience.com)

<br>
<hr>
<br>

<https://github.com/user-attachments/assets/78647eef-2d66-44e6-99a8-1918a940fb9f>

### ESP32-S3 Korvo V3

TEN Agent is now running on the Espressif ESP32-S3 Korvo V3 development board, an excellent way to integrate realtime communication with LLM on hardware.

Check out the [integration guide](https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/esp32-client) for more details.

<br>

<!-- ## Agent Examples

<table>
<tr>
<td width="50%">
<h3>Chained Voice Assistant</h3>
<p>A voice assistant optimized for voice conversations.</p>
<p>
<a href="ai_agents/agents/examples/voice-assistant/">Code</a>
</p>
</td>
<td width="50%">
<h3>Real-time Voice Assistant</h3>
<p>A voice assistant optimized for real-time conversations.</p>
<p>
<a href="ai_agents/agents/examples/voice-assistant-realtime/">Code</a>
</p>
</td>
</tr>

<tr>
<td width="50%">
<h3>Voice Assistant Node.js</h3>
<p>A voice assistant written in Node.js.</p>
<p>
<a href="ai_agents/agents/examples/voice-assistant-nodejs/">Code</a>
</p>
</td>

<td width="50%">
<h3>Voice Assistant Video</h3>
<p>A voice assistant with video capabilities.</p>
<p>
<a href="ai_agents/agents/examples/voice-assistant-video/">Code</a>
</p>
</td>
</tr>

<tr>
<td width="50%">
<h3>SIP (Twilio)</h3>
<p>A voice assistant that places outbound phone calls with Twilio.</p>
<p>
<a href="ai_agents/agents/examples/voice-assistant-sip-twilio/">Code</a>
</p>
</td>
<td width="50%">

<h3>Memory with MemU</h3>
<p>A voice assistant that uses MemU's memory management capabilities for persistent conversation context.</p>
<p>
<a href="ai_agents/agents/examples/voice-assistant-with-memU/">Code</a>
</p>
</td>
</tr>

<tr>
<td width="50%">
<h3>TEN VAD</h3>
<p>A voice assistant that uses TEN VAD for voice activity detection.</p>
<p>
<a href="ai_agents/agents/examples/voice-assistant-with-ten-vad/">Code</a>
</p>
</td>
<td width="50%">
<h3>TEN Turn Detection</h3>
<p>A voice assistant that uses TEN Turn Detection to manage conversation turns.</p>
<p>
<a href="ai_agents/agents/examples/voice-assistant-with-ten-turn-detector/">Code</a>
</p>
</td>
</tr>

<tr>
<td width="50%">
<h3>Transcription</h3>
<p>A transcription agent that captures audio from every user in the room.</p>
<p>
<a href="ai_agents/agents/examples/transcription/">Code</a>
</p>
</td>
<td width="50%">
<h3>Speech Diarization</h3>
<p>Identifies different speakers in multi-user conversations.</p>
<p>
<a href="ai_agents/agents/examples/speechmatics-diarization/">Code</a>
</p>
</td>
</tr>

</table>

<br> -->

## Get Agent Examples up and running

### 🅰️ Locahost

#### Step ⓵ - Prerequisites

| Category | Requirements |
| --- | --- |
| **Keys** | • Agora [App ID](https://docs.agora.io/en/video-calling/get-started/manage-agora-account?platform=web#create-an-agora-project) and [App Certificate](https://docs.agora.io/en/video-calling/get-started/manage-agora-account?platform=web#create-an-agora-project) (free minutes every month) <br>• [OpenAI](https://openai.com/index/openai-api/) API key (any LLM that is compatible with OpenAI)<br>• [Deepgram](https://deepgram.com/) ASR (free credits available with signup)<br>• [ElevenLabs](https://elevenlabs.io/) TTS (free credits available with signup) |
| **Installation** | • [Docker](https://www.docker.com/) / [Docker Compose](https://docs.docker.com/compose/)<br>• [Node.js (LTS) v18](https://nodejs.org/en) |
| **Minimum System Requirements** | • CPU >= 2 cores<br>• RAM >= 4 GB |

<br>

<!-- > [!NOTE]
> **macOS: Docker setting on Apple Silicon**
>
> Uncheck "Use Rosetta for x86/amd64 emulation" in Docker settings, it may result in slower build times on ARM, but performance will be normal when deployed to x64 servers. -->

#### Step ⓶ - Build agent examples in VM

##### 1. Clone the repo, `cd` into `ai_agents`, and create a `.env` file from `.env.example`

```bash
cd ai_agents
cp ./.env.example ./.env
```

##### 2. Set up the Agora App ID and App Certificate in `.env`

```bash
AGORA_APP_ID=
AGORA_APP_CERTIFICATE=

# In case you are running the default voice-assistant example
# Deepgram (required for speech-to-text)
DEEPGRAM_API_KEY=your_deepgram_api_key_here

# OpenAI (required for language model)
OPENAI_API_KEY=your_openai_api_key_here
OPENAI_MODEL=gpt-4o

# ElevenLabs (required for text-to-speech)
ELEVENLABS_TTS_KEY=your_elevenlabs_api_key_here
```

##### 3. Start agent development containers

```bash
docker compose up -d
```

##### 4. Enter the container

```bash
docker exec -it ten_agent_dev bash
```

##### 5. Build the agent with the default example (~5-8 min)

Check the `/examples` folder for more options. Start with one of the defaults below:

```bash
# use the chained voice assistant
cd agents/examples/voice-assistant

# or use the speech-to-speech voice assistant in real time
cd agents/examples/voice-assistant-realtime
```

##### 6. Start the web server

```bash
task install
# run task build if you changed any local source code.
# This is necessary for compiled languages (e.g., TypeScript or Go) and not needed for Python.
# task build

task run
```

##### 7. Access the agent

- TMAN Designer: <http://localhost:49483>
- Example UI: <http://localhost:3000>

#### Step ⓷ - Customize your agent

 1. Open [localhost:49483](http://localhost:49483).
 2. Right-click on the STT, LLM, and TTS extensions.
 3. Open their properties and enter APIs respectively.
 4. Right-click the canvas and select 'Manage Apps' to open the Apps Manager.
 5. Right under the Actions, click the ▶ to run the App.
 6. Check the 'Run with TEN Agent' option and click the Run button.

<br>

### 🅱️ Codespaces

GitHub offers free Codespaces for each repository. You can run Agent Examples in Codespaces without using Docker. Also, Codespaces are typically much faster than localhost.

[codespaces-shield]: <https://github.com/codespaces/badge.svg>
[![][codespaces-shield]](https://codespaces.new/ten-framework/ten-agent)

Check out [this guide](https://theten.ai/docs/ten_agent/setup_development_env/setting_up_development_inside_codespace) for more details.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

## Agent Examples Self-Hosting

#### 🅰️ Deploying with Docker

Once you have customized your agent (either by using the TMAN Manager, Playground, or editing `property.json` directly), you can deploy it by creating a release Docker image for your service.

##### Release as Docker image

**Note**: The following commands need to be executed outside of any Docker container.

###### Build image

```bash
cd ai_agents
docker build -f agents/examples/<example-name>/Dockerfile -t example-app .
```

###### Run

```bash
docker run --rm -it --env-file .env -p 3000:3000 example-app
```

<br>

#### 🅱️ Deploying with other cloud services

You can split the deployment into two pieces when you want to host TEN on providers such as Vercel or Netlify.

1. Run the TEN backend on any container-friendly platform (a VM with Docker, Fly.io, Render, ECS, Cloud Run, or similar). Use the example Docker image without modifying it and expose port `8080` from that service.

2. Deploy only the frontend to Vercel or Netlify. Point the project root to `ai_agents/agents/examples/<example>/frontend`, run `pnpm install` (or `bun install`) followed by `pnpm build` (or `bun run build`), and keep the default `.next` output directory.

3. Configure environment variables in your hosting dashboard so that `AGENT_SERVER_URL` points to the backend URL, and add any `NEXT_PUBLIC_*` keys the UI needs (for example, Agora credentials you surface to the browser).

4. Make sure the backend allows requests from the frontend origin (either by opening CORS on your container host or leaving the built-in middleware proxy in place).

With this setup, the backend handles long-running worker processes, while the hosted frontend simply forwards API traffic to it.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

## Stay Tuned

Get instant notifications for new releases and updates. Your support helps us grow and improve TEN!

<br>

![Image](https://github.com/user-attachments/assets/72c6cc46-a2a2-484d-82a9-f3079269c815)

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

## TEN Ecosystem

| Project | Preview |
| ------- | ------- |
| [**️TEN Framework**][ten-framework-link]<br>TEN is an open-source framework for real-time, multimodal conversational AI.<br><br>![][ten-framework-shield] | ![][ten-framework-banner] |
| [**TEN VAD**][ten-vad-link]<br>TEN VAD is a low-latency, lightweight and high-performance streaming voice activity detector (VAD).<br><br>![][ten-vad-shield] | ![][ten-vad-banner] |
| [**️ TEN Turn Detection**][ten-turn-detection-link]<br>TEN Turn Detection enables full-duplex dialogue communication.<br><br>![][ten-turn-detection-shield] | ![][ten-turn-detection-banner] |
| [**TEN Portal**][ten-portal-link]<br>The official site of the TEN Framework with documentation and a blog.<br><br>![][ten-portal-shield] | ![][ten-portal-banner] |
| [**TEN Agent**][ten-agent-link]<br>TEN Agent is a showcase of the TEN Framework.<br><br> | ![][ten-agent-banner] |
| [**TMAN Designer**][tman-designer-link]<br>TMAN Designer is a low/no-code option for building a voice agent with an easy-to-use workflow UI.<br><br> | ![][tman-designer-banner] |

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

## Questions

TEN Framework is available on these AI-powered Q&A platforms. They can help you find answers quickly and accurately in multiple languages, covering everything from basic setup to advanced implementation details.

| Service | Link |
| ------- | ---- |
| DeepWiki | [![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/TEN-framework/TEN-framework) |
| ReadmeX | [![ReadmeX](https://raw.githubusercontent.com/CodePhiliaX/resource-trusteeship/main/readmex.svg)](https://readmex.com/TEN-framework/ten-framework) |

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

## Contributing

We welcome all forms of open-source collaboration! Whether you're fixing bugs, adding features, improving documentation, or sharing ideas, your contributions help advance personalized AI tools. Check out our GitHub Issues and Projects to find ways to contribute and show your skills. Together, we can build something amazing!

<br>

> [!TIP]
>
> **Welcome all kinds of contributions** 🙏
>
> Join us in building TEN better! Every contribution makes a difference, from code to documentation. Share your TEN Agent projects on social media to inspire others!
>
> Connect with one of the TEN maintainers [@elliotchen100](https://x.com/elliotchen100) on 𝕏 or [@cyfyifanchen](https://github.com/cyfyifanchen) on GitHub for project updates, discussions, and collaboration opportunities.

<br>

### Code Contributors

[![TEN](https://contrib.rocks/image?repo=TEN-framework/ten-agent)](https://github.com/TEN-framework/ten-agent/graphs/contributors)

### Contribution Guidelines

Contributions are welcome! Please read the [contribution guidelines](./docs/code-of-conduct/contributing.md) first.

### License

1. The entire TEN framework (except for the folders explicitly listed below) is released under the Apache License, Version 2.0, with additional restrictions. For details, please refer to the [LICENSE](./LICENSE) file located in the root directory of the TEN framework.

2. The components within the `packages` directory are released under the Apache License, Version 2.0. For details, please refer to the `LICENSE` file located in each package's root directory.

3. The third-party libraries used by the TEN framework are listed and described in detail. For more information, please refer to the [third_party](./third_party/) folder.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

[back-to-top]: https://img.shields.io/badge/-Back_to_top-gray?style=flat-square

[ten-framework-shield]: https://img.shields.io/github/stars/ten-framework/ten_framework?color=ffcb47&labelColor=gray&style=flat-square&logo=github
[ten-framework-banner]: https://github.com/user-attachments/assets/7c8f72d7-3993-4d01-8504-b71578a22944
[ten-framework-link]: https://github.com/ten-framework/ten_framework

[ten-vad-link]: https://github.com/ten-framework/ten-vad
[ten-vad-shield]: https://img.shields.io/github/stars/ten-framework/ten-vad?color=ffcb47&labelColor=gray&style=flat-square&logo=github
[ten-vad-banner]: https://github.com/user-attachments/assets/d45870e4-9453-4047-8163-08737f82863f

[ten-turn-detection-link]: https://github.com/ten-framework/ten-turn-detection
[ten-turn-detection-shield]: https://img.shields.io/github/stars/ten-framework/ten-turn-detection?color=ffcb47&labelColor=gray&style=flat-square&logo=github
[ten-turn-detection-banner]: https://github.com/user-attachments/assets/8d0ec716-5d0e-43e4-ad9a-d97b17305658

[ten-agent-link]: https://github.com/TEN-framework/ten-framework/tree/main/ai_agents
[ten-agent-banner]: https://github.com/user-attachments/assets/38de2207-939b-4702-a0aa-04491f5b5275
[tman-designer-banner]: https://github.com/user-attachments/assets/804c3543-0a47-42b7-b40b-ef32b742fb8f
[tman-designer-link]: https://github.com/TEN-framework/ten-framework/tree/main/core/src/ten_manager/designer_frontend

[ten-portal-link]: https://github.com/ten-framework/portal
[ten-portal-shield]: https://img.shields.io/github/stars/ten-framework/portal?color=ffcb47&labelColor=gray&style=flat-square&logo=github
[ten-portal-banner]: https://github.com/user-attachments/assets/e17d8aaa-5928-45dd-ac71-814928e26a89
