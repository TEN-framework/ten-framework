<div align="center" id="readme-top">

![Image](https://github.com/user-attachments/assets/2a560a74-68f3-4f4a-9ec8-89464c42a9c7)

[![TEN Releases]( https://img.shields.io/github/v/release/ten-framework/ten-framework?color=369eff&labelColor=gray&logo=github&style=flat-square )](https://github.com/TEN-framework/ten-framework/releases)
[![Coverage Status](https://coveralls.io/repos/github/TEN-framework/ten-framework/badge.svg?branch=HEAD)](https://coveralls.io/github/TEN-framework/ten-framework?branch=HEAD)
[![](https://img.shields.io/github/release-date/ten-framework/ten-framework?labelColor=gray&style=flat-square)](https://github.com/TEN-framework/ten-framework/releases)
[![Commits](https://img.shields.io/github/commit-activity/m/TEN-framework/ten_framework?labelColor=gray&color=pink)](https://github.com/TEN-framework/ten-framework/graphs/commit-activity)
[![Issues closed](https://img.shields.io/github/issues-search?query=repo%3ATEN-framework%2Ften-framework%20is%3Aclosed&label=issues%20closed&labelColor=gray&color=green)](https://github.com/TEN-framework/ten-framework/issues)
[![](https://img.shields.io/github/contributors/ten-framework/ten-framework?color=c4f042&labelColor=gray&style=flat-square)](https://github.com/TEN-framework/ten-framework/graphs/contributors)
[![GitHub license](https://img.shields.io/badge/License-Apache_2.0_with_certain_conditions-blue.svg?labelColor=%20%23155EEF&color=%20%23528bff)](https://github.com/TEN-framework/ten_framework/blob/main/LICENSE)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/TEN-framework/TEN-framework)
[![ReadmeX](https://raw.githubusercontent.com/CodePhiliaX/resource-trusteeship/main/readmex.svg)](https://readmex.com/TEN-framework/ten-framework)


[![README in English](https://img.shields.io/badge/English-lightgrey)](https://github.com/TEN-framework/ten-framework/blob/main/README.md)
[![简体中文操作指南](https://img.shields.io/badge/简体中文-lightgrey)](https://github.com/TEN-framework/ten-framework/blob/main/docs/README-CN.md)
[![日本語のREADME](https://img.shields.io/badge/日本語-lightgrey)](https://github.com/TEN-framework/ten-framework/blob/main/docs/README-JP.md)
[![README in 한국어](https://img.shields.io/badge/한국어-lightgrey)](https://github.com/TEN-framework/ten-framework/blob/main/docs/README-KR.md)
[![README en Español](https://img.shields.io/badge/Español-lightgrey)](https://github.com/TEN-framework/ten-framework/blob/main/docs/README-ES.md)
[![README en Français](https://img.shields.io/badge/Français-lightgrey)](https://github.com/TEN-framework/ten-framework/blob/main/docs/README-FR.md)
[![README in Italiano](https://img.shields.io/badge/Italiano-lightgrey)](https://github.com/TEN-framework/ten-framework/blob/main/docs/README-IT.md)

[![TEN-framework%2Ften_framework | Trendshift](https://trendshift.io/api/badge/repositories/11978)](https://trendshift.io/repositories/11978)

[Official Site](https://theten.ai)
•
[Documentation](https://theten.ai/docs/ten_agent/overview)
•
[Blog](https://theten.ai/blog)

</div>

<br>

<details open>
  <summary><kbd>Table of Contents</kbd></summary>

  <br>

- [Welcome to TEN](#welcome-to-ten)
- [Agent Examples](#agent-examples)
- [Quick Start with Agent Examples](#quick-start-with-agent-examples)
  - [Localhost](#localhost)
  - [Codespaces](#codespaces)
- [Agent Examples Self-Hosting](#agent-examples-self-hosting)
  - [Deploying with Docker](#deploying-with-docker)
  - [Deploying with other cloud services](#deploying-with-other-cloud-services)
- [Stay Tuned](#stay-tuned)
- [TEN Ecosystem](#ten-ecosystem)
- [Questions](#questions)
- [Contributing](#contributing)
  - [Code Contributors](#code-contributors)
  - [Contribution Guidelines](#contribution-guidelines)
  - [License](#license)

<br/>

</details>

## Welcome to TEN

TEN is an open-source framework for conversational voice AI agents.

[TEN Ecosystem](#ten-ecosystem) includes [TEN Framework](https://github.com/ten-framework/ten-framework), [Agent Examples](https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples), [VAD](https://github.com/ten-framework/ten-vad), [Turn Detection](https://github.com/ten-framework/ten-turn-detection) and [Portal](https://github.com/ten-framework/portal).

<br>

| Community Channel | Purpose |
| ---------------- | ------- |
| [![Follow on X](https://img.shields.io/twitter/follow/TenFramework?logo=X&color=%20%23f5f5f5)](https://twitter.com/intent/follow?screen_name=TenFramework) | Follow TEN Framework on X for updates and announcements |
| [![Discord TEN Community](https://img.shields.io/badge/Discord-Join%20TEN%20Community-5865F2?style=flat&logo=discord&logoColor=white)](https://discord.gg/VnPftUzAMJ) | Join our Discord community to connect with developers |
| [![Follow on LinkedIn](https://custom-icon-badges.demolab.com/badge/LinkedIn-TEN_Framework-0A66C2?logo=linkedin-white&logoColor=fff)](https://www.linkedin.com/company/ten-framework) | Follow TEN Framework on LinkedIn for updates and announcements |
| [![Hugging Face Space](https://img.shields.io/badge/Hugging%20Face-TEN%20Framework-yellow?style=flat&logo=huggingface)](https://huggingface.co/TEN-framework) | Join our Hugging Face community to explore our spaces and models |
| [![WeChat](https://img.shields.io/badge/TEN_Framework-WeChat_Group-%2307C160?logo=wechat&labelColor=darkgreen&color=gray)](https://github.com/TEN-framework/ten-agent/discussions/170) | Join our WeChat group for Chinese community discussions |

<br>

## Agent Examples

<br>

![Image](https://github.com/user-attachments/assets/6e75e457-0a01-44cf-b203-025ce433460d)

<strong>Multi-Purpose Voice Assistant</strong> — This low-latency, high-quality real-time assistant supports both RTC and [WebSocket](ai_agents/agents/examples/websocket-example) connections, and you can extend it with [Memory](ai_agents/agents/examples/voice-assistant-with-memU), [VAD](ai_agents/agents/examples/voice-assistant-with-ten-vad), [Turn Detection](ai_agents/agents/examples/voice-assistant-with-turn-detection), and other extensions.

See the [Example code](ai_agents/agents/examples/voice-assistant) for more details.

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/aa3f2c9c-c82e-412f-8400-06378ba75794)

<strong>Lip Sync Avatars</strong> — Works with multiple avatar vendors, the demo features Kei, an anime character with Live2D-powered lip sync, and also supports realistic avatars from Trulience, HeyGen, and Tavus (coming soon).

See the [Example code](ai_agents/agents/examples/voice-assistant-live2d) for Live2D avatars.

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/f94b21b8-9dda-4efc-9274-b028cc01296a)

<strong>Speech Diarization</strong> — Real-time diarization that detects and labels speakers, the Who Likes What game shows an interactive use case.

[Example code](ai_agents/agents/examples/speechmatics-diarization)

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/6ed5b04d-945a-4a30-a1cc-f8014b602b38)

<strong>SIP Call</strong> — SIP extension that enables phone calls powered by TEN.

[Example code](ai_agents/agents/examples/voice-assistant-sip-twilio)

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/d793bc6c-c8de-4996-bd85-9ce88c69dd8d)

<strong>Transcription</strong> — A transcription tool that transcribes audio to text.

[Example code](ai_agents/agents/examples/transcription)

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/3d60f1ff-0f82-4fe7-b5c2-ac03d284f60c)

<strong>ESP32-S3 Korvo V3</strong> — Runs TEN agent example on the Espressif ESP32-S3 Korvo V3 development board to integrate LLM-powered communication with hardware.

See the [integration guide](ai_agents/esp32-client) for more details.

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

## Quick Start with Agent Examples

### Localhost

#### Step ⓵ - Prerequisites

| Category | Requirements |
| --- | --- |
| **Keys** | • Agora [App ID](https://docs.agora.io/en/video-calling/get-started/manage-agora-account?platform=web#create-an-agora-project) and [App Certificate](https://docs.agora.io/en/video-calling/get-started/manage-agora-account?platform=web#create-an-agora-project) (free minutes every month) <br>• [OpenAI](https://openai.com/index/openai-api/) API key (any LLM that is compatible with OpenAI)<br>• [Deepgram](https://deepgram.com/) ASR (free credits available with signup)<br>• [ElevenLabs](https://elevenlabs.io/) TTS (free credits available with signup) |
| **Installation** | • [Docker](https://www.docker.com/) / [Docker Compose](https://docs.docker.com/compose/)<br>• [Node.js (LTS) v18](https://nodejs.org/en) |
| **Minimum System Requirements** | • CPU >= 2 cores<br>• RAM >= 4 GB |

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

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

Check the `agents/examples` folder for additional samples.
Start with one of these defaults:

```bash
# use the chained voice assistant
cd agents/examples/voice-assistant

# or use the speech-to-speech voice assistant in real time
cd agents/examples/voice-assistant-realtime
```

##### 6. Start the web server

Run `task build` if you changed any local source code. This step is required for compiled languages (for example, TypeScript or Go) and not needed for Python.

```bash
task install
task run
```

##### 7. Access the agent

Once the agent example is running, you can access the following interfaces:

<table>
  <tr>
    <td align="center">
      <b>localhost:49483</b>
      <img src="https://github.com/user-attachments/assets/191a7c0a-d8e6-48f9-866f-6a70c58f0118" alt="Screenshot 1" /><br/>
    </td>
    <td align="center">
      <b>localhost:3000</b>
      <img src="https://github.com/user-attachments/assets/13e482b6-d907-4449-a779-9454bb24c0b1" alt="Screenshot 2" /><br/>
    </td>
  </tr>
</table>

- TMAN Designer: <http://localhost:49483>
- Agent Examples UI: <http://localhost:3000>

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

#### Step ⓷ - Customize your agent example

1. Open [localhost:49483](http://localhost:49483).
2. Right-click the STT, LLM, and TTS extensions.
3. Open their properties and enter the corresponding API keys.
4. Submit your changes, now you can see the updated Agent Example in [localhost:3000](http://localhost:3000).

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

### Codespaces

GitHub offers free Codespaces for each repository. You can run Agent Examples in Codespaces without using Docker. Codespaces typically start faster than local Docker environments.

[codespaces-shield]: <https://github.com/codespaces/badge.svg>
[![][codespaces-shield]](https://codespaces.new/ten-framework/ten-agent)

Check out [this guide](https://theten.ai/docs/ten_agent/setup_development_env/setting_up_development_inside_codespace) for more details.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

## Agent Examples Self-Hosting

### Deploying with Docker

Once you have customized your agent (either by using the TMAN Designer or editing `property.json` directly), you can deploy it by creating a release Docker image for your service.

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

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

### Deploying with other cloud services

You can split the deployment into two pieces when you want to host TEN on providers such as [Vercel](https://vercel.com) or [Netlify](https://www.netlify.com).

1. Run the TEN backend on any container-friendly platform (a VM with Docker, Fly.io, Render, ECS, Cloud Run, or similar). Use the example Docker image without modifying it and expose port `8080` from that service.

2. Deploy only the frontend to Vercel or Netlify. Point the project root to `ai_agents/agents/examples/<example>/frontend`, run `pnpm install` (or `bun install`) followed by `pnpm build` (or `bun run build`), and keep the default `.next` output directory.

3. Configure environment variables in your hosting dashboard so that `AGENT_SERVER_URL` points to the backend URL, and add any `NEXT_PUBLIC_*` keys the UI needs (for example, Agora credentials you surface to the browser).

4. Ensure your backend accepts requests from the frontend origin — via open CORS or by using the built-in proxy middleware.

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

<br>

| Project | Preview |
| ------- | ------- |
| [**️TEN Framework**][ten-framework-link]<br>Open-source framework for conversational AI Agents.<br><br>![][ten-framework-shield] | ![][ten-framework-banner] |
| [**TEN VAD**][ten-vad-link]<br>Low-latency, lightweight and high-performance streaming voice activity detector (VAD).<br><br>![][ten-vad-shield] | ![][ten-vad-banner] |
| [**️ TEN Turn Detection**][ten-turn-detection-link]<br>TEN Turn Detection enables full-duplex dialogue communication.<br><br>![][ten-turn-detection-shield] | ![][ten-turn-detection-banner] |
| [**TEN Agent Examples**][ten-agent-example-link]<br>Usecases powered by TEN.<br><br> | ![][ten-agent-example-banner] |
| [**TEN Portal**][ten-portal-link]<br>The official site of the TEN Framework with documentation and a blog.<br><br>![][ten-portal-shield] | ![][ten-portal-banner] |

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
> Connect with one of the TEN maintainers [@elliotchen200](https://x.com/elliotchen200) on 𝕏 or [@cyfyifanchen](https://github.com/cyfyifanchen) on GitHub for project updates, discussions, and collaboration opportunities.

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

### Code Contributors

[![TEN](https://contrib.rocks/image?repo=TEN-framework/ten-agent)](https://github.com/TEN-framework/ten-agent/graphs/contributors)

### Contribution Guidelines

Contributions are welcome! Please read the [contribution guidelines](./docs/code-of-conduct/contributing.md) first.

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

### License

1. The entire TEN framework (except for the folders explicitly listed below) is released under the Apache License, Version 2.0, with additional restrictions. For details, please refer to the [LICENSE](./LICENSE) file located in the root directory of the TEN framework.

2. The components within the `packages` directory are released under the Apache License, Version 2.0. For details, please refer to the `LICENSE` file located in each package's root directory.

3. The third-party libraries used by the TEN framework are listed and described in detail. For more information, please refer to the [third_party](./third_party/) folder.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

[back-to-top]: https://img.shields.io/badge/-Back_to_top-gray?style=flat-square

[ten-framework-shield]: https://img.shields.io/github/stars/ten-framework/ten_framework?color=ffcb47&labelColor=gray&style=flat-square&logo=github
[ten-framework-banner]: https://github.com/user-attachments/assets/2a560a74-68f3-4f4a-9ec8-89464c42a9c7
[ten-framework-link]: https://github.com/ten-framework/ten_framework

[ten-vad-link]: https://github.com/ten-framework/ten-vad
[ten-vad-shield]: https://img.shields.io/github/stars/ten-framework/ten-vad?color=ffcb47&labelColor=gray&style=flat-square&logo=github
[ten-vad-banner]: https://github.com/user-attachments/assets/e504135e-67fd-4fa1-b0e4-d495358d8aa5

[ten-turn-detection-link]: https://github.com/ten-framework/ten-turn-detection
[ten-turn-detection-shield]: https://img.shields.io/github/stars/ten-framework/ten-turn-detection?color=ffcb47&labelColor=gray&style=flat-square&logo=github
[ten-turn-detection-banner]: https://github.com/user-attachments/assets/c72d82cc-3667-496c-8bd6-3d194a91c452

[ten-agent-example-link]: https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples
[ten-agent-example-banner]:https://github.com/user-attachments/assets/7f735633-c7f6-4432-b6b4-d2a2977ca588

[ten-portal-link]: https://github.com/ten-framework/portal
[ten-portal-shield]: https://img.shields.io/github/stars/ten-framework/portal?color=ffcb47&labelColor=gray&style=flat-square&logo=github
[ten-portal-banner]: https://github.com/user-attachments/assets/f56c75b9-722c-4156-902d-ae98ce2b3b5e
