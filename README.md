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

- [👋 Welcome to TEN](#-welcome-to-ten)
- [🎨 TMAN Designer](#-tman-designer)
- [✨ Features](#-features)
  - [1️⃣ Real-time Avatar](#1️⃣-real-time-avatar)
  - [2️⃣ Real-time voice with MCP servers](#2️⃣-real-time-voice-with-mcp-servers)
  - [3️⃣ Real-time communication with hardware](#3️⃣-real-time-communication-with-hardware)
  - [4️⃣ Real-time vision and real-time screenshare detection](#4️⃣-real-time-vision-and-real-time-screenshare-detection)
  - [5️⃣ TEN with other LLM platforms](#5️⃣-ten-with-other-llm-platforms)
  - [6️⃣ StoryTeller - TEN image generation](#6️⃣-storyteller---ten-image-generation)
- [👩‍💻 Get TEN Agent up and running](#-get-ten-agent-up-and-running)
  - [🅰️ Run TEN Agent in `localhost`](#🅰️-run-ten-agent-in-localhost)
  - [🅱️ Run TEN Agent in Codespace(no docker)](#🅱️-run-ten-agent-in-codespaceno-docker)
- [🛳️ TEN Agent Self Hosting](#️-ten-agent-self-hosting)
  - [🅰️ 🐳 Deploying with Docker](#️--deploying-with-docker)
  - [🅱️ Deploying with other cloud services](#️-deploying-with-other-cloud-services)
- [🌍 TEN Ecosystem](#-ten-ecosystem)
- [❓ Ask Questions](#-ask-questions)
- [🥰 Contributing](#-contributing)
  - [Code Contributors](#code-contributors)
  - [Contribution Guidelines](#contribution-guidelines)
  - [License](#license)

<br/>

</details>

## 👋 Welcome to TEN

TEN is a comprehensive open-source ecosystem for creating, customizing, and deploying real-time conversational AI agents with multimodal capabilities including voice, vision, and avatar interactions.

TEN includes [TEN Framework](https://github.com/ten-framework/ten-framework), [TEN Turn Detection](https://github.com/ten-framework/ten-turn-detection), [TEN VAD](https://github.com/ten-framework/ten-vad), [TEN Agent](https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/demo), [TMAN Designer](https://github.com/TEN-framework/ten-framework/tree/main/core/src/ten_manager/designer_frontend), and [TEN Portal](https://github.com/ten-framework/portal). Check out [🌍 TEN Ecosystem](#-ten-ecosystem) for more details.

<br>

| Community Channel | Purpose |
| ---------------- | ------- |
| [![Follow on X](https://img.shields.io/twitter/follow/TenFramework?logo=X&color=%20%23f5f5f5)](https://twitter.com/intent/follow?screen_name=TenFramework) | Follow TEN Framework on X for updates and announcements |
| [![Follow on LinkedIn](https://custom-icon-badges.demolab.com/badge/LinkedIn-TEN_Framework-0A66C2?logo=linkedin-white&logoColor=fff)](https://www.linkedin.com/company/ten-framework) | Follow TEN Framework on LinkedIn for updates and announcements |
| [![Discord TEN Community](https://dcbadge.vercel.app/api/server/VnPftUzAMJ?&style=flat&theme=light&color=lightgray)](https://discord.gg/VnPftUzAMJ) | Join our Discord community to connect with developers |
| [![Hugging Face Space](https://img.shields.io/badge/Hugging%20Face-TEN%20Framework-yellow?style=flat&logo=huggingface)](https://huggingface.co/TEN-framework) | Join our Hugging Face community to explore our spaces and models |
| [![WeChat](https://img.shields.io/badge/TEN_Framework-WeChat_Group-%2307C160?logo=wechat&labelColor=darkgreen&color=gray)](https://github.com/TEN-framework/ten-agent/discussions/170) | Join our WeChat group for Chinese community discussions |

<br>

> \[!IMPORTANT]
>
> **Star TEN Repositories** ⭐️
>
> Get instant notifications for new releases and updates. Your support helps us grow and improve TEN!

<br>

![TEN star us gif](https://github.com/user-attachments/assets/eeebe996-8c14-4bf7-82ae-f1a1f7e30705)

<br>

<details>
  <summary><kbd>Star History</kbd></summary>
  <picture>
    <img width="100%" src="https://api.star-history.com/svg?repos=ten-framework/ten-framework&type=Date">
  </picture>
</details>

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

## 🎨 TMAN Designer

<https://github.com/user-attachments/assets/44c6a087-ec7a-45b0-a084-dab5dac5e36b>

### TMAN Designer

TMAN Designer is a low/no-code option to create voice agents with an easy-to-use workflow UI. It can load apps and graphs, and includes an online editor, log viewer, and much more.

Check out [this blog](https://theten.ai/blog/tman-designer-of-ten-framework) for more details.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<!-- ## 🧩 Extensions

![Ready-to-use Extensions](https://ten-framework-assets.s3.us-east-1.amazonaws.com/extensions.jpg)

<div align="right">

[![][back-to-top]](#readme-top)

</div> -->

<br>

## ✨ Features

![TEN Agent with Trulience](https://github.com/user-attachments/assets/c6702995-de94-4d3e-8cae-af097f087ac1)

### 1️⃣ Real-time Avatar

Build engaging AI avatars with TEN Agent using [Trulience](https://trulience.com)'s diverse collection of free avatar options. To get it up and running, you only need 2 steps:

1. Follow the README to finish setting up and running the Playground
2. Enter the avatar ID and [token](https://trulience.com/docs#/authentication/jwt-tokens/jwt-tokens?id=use-your-custom-userid) you get from [Trulience](https://trulience.com)

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

![TEN with MCP servers](https://github.com/user-attachments/assets/afb77ad3-9c23-452f-b870-216687779017)

### 2️⃣ Real-time voice with MCP servers

TEN Agent now integrates seamlessly with MCP servers, expanding its LLM capabilities. To get started:

1. Open the Module Picker in Playground
2. Add the MCP server tool for LLM integration
3. Paste a URL from your MCP server in the extension
4. Start a realtime conversation with TEN Agent

This integration allows you to leverage MCP's diverse servers offerings while maintaining TEN Agent's powerful conversational abilities.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

<https://github.com/user-attachments/assets/78647eef-2d66-44e6-99a8-1918a940fb9f>

### 3️⃣ Real-time communication with hardware

TEN Agent is now running on the Espressif ESP32-S3 Korvo V3 development board, an excellent way to integrate realtime communication with LLM on hardware.

Check out the [integration guide](https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/esp32-client) for more details.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

![Real-time Vision](https://github.com/user-attachments/assets/a1addb02-a450-47be-8cb2-d25e3b574f53)

### 4️⃣ Real-time vision and real-time screenshare detection

Try Google Gemini Multimodal Live API with realtime vision and realtime screenshare detection capabilities, it is a ready-to-use extension, along with powerful tools like Weather Check and Web Search integrated perfectly into TEN Agent.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

![TEN with Dify](https://github.com/user-attachments/assets/234ff443-bef8-4cc4-9a10-09d6ec3f5bc1)

### 5️⃣ TEN with other LLM platforms

[TEN Agent + Dify](https://theten.ai/docs/ten_agent/playground/use-cases/voice-assistant/run_dify#steps)

TEN offers a great support to make the realtime interactive experience even better on other LLM platform as well, check out docs for more.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

![TEN StoryTeller](https://github.com/user-attachments/assets/fe28a549-ddb9-431e-9282-57539fb87371)

### 6️⃣ StoryTeller - TEN image generation

Experience the real-time image generation with StoryTeller, it is a ready-to-use extension, along with powerful tools like Weather Check and Web Search integrated perfectly into TEN.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

## 👩‍💻 Get TEN Agent up and running

#### 🅰️ Run TEN Agent in localhost

#### Step ⓵ - Prerequisites

| Category | Requirements |
| --- | --- |
| **Keys** | • Agora [App ID](https://docs.agora.io/en/video-calling/get-started/manage-agora-account?platform=web#create-an-agora-project) and [App Certificate](https://docs.agora.io/en/video-calling/get-started/manage-agora-account?platform=web#create-an-agora-project) (free minutes every month) <br>• [OpenAI](https://openai.com/index/openai-api/) API key (any LLM that is compatible with OpenAI)<br>• [Deepgram](https://deepgram.com/) ASR (free credits available with signup)<br>• [Elevenlabs](https://elevenlabs.io/) TTS (free credits available with signup) |
| **Installation** | • [Docker](https://www.docker.com/) / [Docker Compose](https://docs.docker.com/compose/)<br>• [Node.js(LTS) v18](https://nodejs.org/en) |
| **Minimum System Requirements** | • CPU >= 2 Core<br>• RAM >= 4 GB |

<br>

> \[!NOTE]
>
> **macOS: Docker setting on Apple Silicon**
>
> Uncheck "Use Rosetta for x86/amd64 emulation" in Docker settings, it may result in slower build times on ARM, but performance will be normal when deployed to x64 servers.

<br>

#### Step ⓶ - Build agent in VM

##### 1. Clone down the repo,`cd` to `ai-agents` and create `.env` file from `.env.example`

```bash
cd ai_agents
cp ./.env.example ./.env
```

##### 2. Setup Agora App ID and App Certificate in `.env`

```bash
AGORA_APP_ID=
AGORA_APP_CERTIFICATE=
```

##### 3. Start agent development containers

```bash
docker compose up -d
```

##### 4. Enter container

```bash
docker exec -it ten_agent_dev bash
```

##### 5. Build agent with the default `graph` ( ~5min - ~8min)

check the `/examples` folder for more examples

```bash
# use the default agent
task use

# or use the demo agent
task use AGENT=agents/examples/demo
```

##### 6. Start the web server

```bash
task run
```

<br>

#### Step ⓷ - Customize your agent with TMAN Designer

 1. Open [localhost:49483](http://localhost:49483).
 2. Right click to load the corresponding graph (e.g., Voice Assistant).
 3. Enter API keys and set preferences for each extension.
 4. Right click and select `Run` to start the agent.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

### 🅱️ Run TEN Agent in Codespace(no docker)

GitHub offers free Codespace for each repository, you can run the playground in Codespace without using Docker.Also, the speed of Codespace is much faster than localhost.

[codespaces-shield]: <https://github.com/codespaces/badge.svg>
[![][codespaces-shield]](https://codespaces.new/ten-framework/ten-agent)

Check out [this guide](https://theten.ai/docs/ten_agent/setup_development_env/setting_up_development_inside_codespace) for more details.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

<!-- ## 👀 TEN Agent Demo

Playground and Demo server different purposes, in a nut shell, think it as Playground is for you to customize you agent, and Demo is for you to deploy your agent.

Check out [this guide](https://theten.ai/docs/ten_agent/demo) for more details.
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br> -->

## 🛳️ TEN Agent Self Hosting

#### 🅰️ 🐳 Deploying with Docker

Once you have customized your agent (either by using the TMAN Manager, Playground, or editing `property.json` directly), you can deploy it by creating a release Docker image for your service.

Read the [Deployment Guide](https://theten.ai/docs/ten_agent/deploy_ten_agent/deploy_agent_service) for detailed information about deployment.

<br>

#### 🅱️ Deploying with other cloud services

*coming soon*

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

<!--
## 🏗️ TEN Agent Architecture

1️⃣ **TEN Agent App**: Core application that manages extensions and data flow based on graph configuration

2️⃣ **Dev Server**: `port:49480`- local server for development purposes.

3️⃣ **Web Server**: `port:8080`- Golang server handling HTTP requests and agent process management

4️⃣ **Front-end UI**:

- `port:3000` Playground - To customize and test your agent configurations.
- `port:3002` Demo - To deploy your agent without module picker.

![Components Diagram](https://ten-framework-assets.s3.us-east-1.amazonaws.com/diagram.jpg)

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br> -->

## 🌍 TEN Ecosystem

| Project | Preview |
| ------- | ------- |
| [**🏚️ TEN Framework**][ten-framework-link]<br>TEN is an open-source framework for real-time, multimodal conversational AI.<br><br>![][ten-framework-shield] | ![][ten-framework-banner] |
| [**️🔂 TEN Turn Detection**][ten-turn-detection-link]<br>TEN is for full-duplex dialogue communication.<br><br>![][ten-turn-detection-shield] | ![][ten-turn-detection-banner] |
| [**🔉 TEN VAD**][ten-vad-link]<br>TEN VAD is a low-latency, lightweight and high-performance streaming voice activity detector (VAD).<br><br>![][ten-vad-shield] | ![][ten-vad-banner] |
| [**🎙️ TEN Agent**][ten-agent-link]<br>TEN Agent is a showcase of TEN Framewrok.<br><br> | ![][ten-agent-banner] |
| [**🎨 TMAN Designer**][tman-designer-link]<br>TMAN Designer is low/no code option to make a voice agent with easy to use workflow UI.<br><br> | ![][tman-designer-banner] |
| [**📒 TEN Portal**][ten-portal-link]<br>The official site of TEN framework, it has documentation and blog.<br><br>![][ten-portal-shield] | ![][ten-portal-banner] |

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

## ❓ Ask Questions

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/TEN-framework/TEN-framework)

Most questions can be answered by using DeepWiki, it is fast, intutive to use and supports multiple languages.

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

## 🥰 Contributing

We welcome all forms of open-source collaboration! Whether you're fixing bugs, adding features, improving documentation, or sharing ideas - your contributions help advance personalized AI tools. Check out our GitHub Issues and Projects to find ways to contribute and show your skills. Together, we can build something amazing!

<br>

> \[!TIP]
>
> **Welcome all kinds of contributions** 🙏
>
> Join us in building TEN better! Every contribution makes a difference, from code to documentation. Share your TEN Agent projects on social media with to inspire others!
>
> Connect with one of the TEN maintainers [@elliotchen100](https://x.com/elliotchen100) on 𝕏 or [@cyfyifanchen](https://github.com/cyfyifanchen) on GitHub for project updates, discussions and collaboration opportunities.

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
