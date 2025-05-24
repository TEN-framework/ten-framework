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

[官方网站](https://theten.ai)
•
[文档](https://theten.ai/docs/ten_agent/overview)
•
[博客](https://theten.ai/blog)

<a href="https://trendshift.io/repositories/11978" target="_blank"><img src="https://trendshift.io/api/badge/repositories/11978" alt="TEN-framework%2Ften_framework | Trendshift" style="width: 250px; height: 55px;" width="250" height="55"/></a>

</div>

<br>

<details>
  <summary><kbd>目录</kbd></summary>

#### 目录

- [👋 欢迎使用 TEN](#-欢迎使用-ten)
- [🎨 TMAN Designer](#-tman-designer)
- [✨ 功能特性](#-功能特性)
  - [1️⃣ 实时头像](#1️⃣-实时头像)
  - [2️⃣ 使用 MCP 服务器的实时语音](#2️⃣-使用-mcp-服务器的实时语音)
  - [3️⃣ 与硬件的实时通信](#3️⃣-与硬件的实时通信)
  - [4️⃣ 实时视觉和实时屏幕共享检测](#4️⃣-实时视觉和实时屏幕共享检测)
  - [5️⃣ 与其他 LLM 平台的集成](#5️⃣-与其他-llm-平台的集成)
  - [6️⃣ StoryTeller - TEN 图像生成](#6️⃣-storyteller---ten-图像生成)
- [🛝 TEN Agent Playground](#-ten-agent-playground)
  - [🅰️ 在 `localhost` 运行 Playground](#🅰️-在-localhost-运行-playground)
  - [🅱️ 在 Codespace 中运行 Playground(无需 Docker)](#🅱️-在-codespace-中运行-playground无需-docker)
- [🛳️ TEN Agent 自托管](#️-ten-agent-自托管)
  - [🅰️ 🐳 使用 Docker 部署](#️--使用-docker-部署)
  - [🅱️ 使用其他云服务部署](#️-使用其他云服务部署)
- [🏗️ TEN Agent 架构](#️-ten-agent-架构)
- [🌍 TEN 生态](#-ten-生态)
- [❓问题](#-问题)
- [🥰 贡献](#-贡献)
  - [代码贡献者](#代码贡献者)
  - [贡献指南](#贡献指南)
  - [许可证](#许可证)

<br/>

</details>

## 👋 欢迎使用 TEN

TEN 是一系列用于构建实时、多模态对话语音代理的开源项目集合。它包括 [TEN Framework](https://github.com/ten-framework/ten-framework)、[TEN Turn Detection](https://github.com/ten-framework/ten-turn-detection)、TEN Agent、TMAN Designer 和 [TEN Portal](https://github.com/ten-framework/portal)，全部都是完全开源的。[TEN VAD](https://github.com/ten-framework/ten-vad) 目前还不是完全开源的，但已开放供公众使用。

<br>

| 社区渠道 | 用途 |
| ---------------- | ------- |
| [![Follow on X](https://img.shields.io/twitter/follow/TenFramework?logo=X&color=%20%23f5f5f5)](https://twitter.com/intent/follow?screen_name=TenFramework) | 在 X 上关注 TEN Framework 获取更新和公告 |
| [![Follow on LinkedIn](https://custom-icon-badges.demolab.com/badge/LinkedIn-TEN_Framework-0A66C2?logo=linkedin-white&logoColor=fff)](https://www.linkedin.com/company/ten-framework) | 在 LinkedIn 上关注 TEN Framework 获取更新和公告 |
| [![Discord TEN Community](https://dcbadge.vercel.app/api/server/VnPftUzAMJ?&style=flat&theme=light&color=lightgray)](https://discord.gg/VnPftUzAMJ) | 加入我们的 Discord 社区与开发者交流 |
| [![Hugging Face Space](https://img.shields.io/badge/Hugging%20Face-TEN%20Framework-yellow?style=flat&logo=huggingface)](https://huggingface.co/TEN-framework) | 加入我们的 Hugging Face 社区探索我们的空间和模型 |
| [![WeChat](https://img.shields.io/badge/TEN_Framework-WeChat_Group-%2307C160?logo=wechat&labelColor=darkgreen&color=gray)](https://github.com/TEN-framework/ten-agent/discussions/170) | 加入我们的微信群进行中文社区讨论 |

<br>

> \[!IMPORTANT]
>
> **为 TEN 仓库点赞** ⭐️
>
> 获取新版本和更新的即时通知。您的支持帮助我们成长和改进 TEN！

<br>

![TEN star us gif](https://github.com/user-attachments/assets/eeebe996-8c14-4bf7-82ae-f1a1f7e30705)

<br>

<details>
  <summary><kbd>Star 历史</kbd></summary>
  <picture>
    <img width="100%" src="https://api.star-history.com/svg?repos=ten-framework/ten-framework&type=Date">
  </picture>
</details>

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

## 🎨 TMAN Designer

![TMAN Designer](https://github.com/user-attachments/assets/04fd75df-4de9-41b6-8aab-19014ecb46a4)

### TMAN Designer

TMAN Designer 是一个低代码/无代码选项，用于通过易于使用的工作流 UI 创建语音代理。它可以加载应用程序和图表，并包含在线编辑器、日志查看器等更多功能。

查看[这篇博客](https://theten.ai/blog/tman-designer-of-ten-framework)了解更多详情。

<div align="right">

[![][back-to-top]](#readme-top)

</div>

## ✨ 功能特性

![TEN Agent with Trulience](https://github.com/user-attachments/assets/2f1dfd55-14a3-47ea-ae25-40ad40ceadea)

### 1️⃣ 实时头像

使用 TEN Agent 和 [Trulience](https://trulience.com) 提供的多样化免费头像选项构建引人入胜的 AI 头像。只需 2 个步骤即可开始使用：

1. 按照 README 完成游乐场的设置和运行
2. 输入从 [Trulience](https://trulience.com) 获取的头像 ID 和 [token](https://trulience.com/docs#/authentication/jwt-tokens/jwt-tokens?id=use-your-custom-userid)

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

![TEN Agent with MCP servers](https://github.com/user-attachments/assets/934ba928-a4a3-4662-8624-cebefc88ce05)

### 2️⃣ 使用 MCP 服务器的实时语音

TEN Agent 现在可以无缝集成 MCP 服务器，扩展其 LLM 功能。开始使用：

1. 在游乐场中打开模块选择器
2. 添加 MCP 服务器工具进行 LLM 集成
3. 在扩展中粘贴来自 MCP 服务器的 URL
4. 开始与 TEN Agent 进行实时对话

这种集成使您能够利用 MCP 的多样化服务器产品，同时保持 TEN Agent 强大的对话能力。

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

<https://github.com/user-attachments/assets/78647eef-2d66-44e6-99a8-1918a940fb9f>

### 3️⃣ 与硬件的实时通信

TEN Agent 现在可以在 Espressif ESP32-S3 Korvo V3 开发板上运行，这是在硬件上集成 LLM 实时通信的绝佳方式。

查看[集成指南](https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/esp32-client)了解更多详情。

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

![Real-time vision](https://github.com/user-attachments/assets/7be06e38-994e-4f82-8ec6-183d08fe90f1)

### 4️⃣ 实时视觉和实时屏幕共享检测

尝试使用 Google Gemini Multimodal Live API 的实时视觉和实时屏幕共享检测功能，这是一个即用型扩展，同时集成了天气查询和网络搜索等强大工具，完美融入 TEN Agent。

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

![TEN with other LLM platforms](https://github.com/user-attachments/assets/a3766d50-6a25-4299-b28c-e15772e4201c)

### 5️⃣ 与其他 LLM 平台的集成

[TEN Agent + Dify](https://doc.theten.ai/docs/ten_agent/playground/use-cases/voice-assistant/run_dify)

TEN 提供了出色的支持，使其他 LLM 平台上的实时交互体验更好，查看文档了解更多。

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

![Image](https://github.com/user-attachments/assets/ea1025d4-b22b-4416-ab35-36cd910bc28c)

### 6️⃣ StoryTeller - TEN 图像生成

体验 StoryTeller 的实时图像生成功能，这是一个即用型扩展，同时集成了天气查询和网络搜索等强大工具，完美融入 TEN。

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

## 🛝 TEN Agent Playground

#### 🅰️ 在 `localhost` Playground

#### 步骤 ⓵ - 先决条件

| 类别 | 要求 |
| --- | --- |
| **密钥** | • 声网 [APP ID](https://doc.shengwang.cn/doc/rtc/javascript/get-started/enable-service) 和 [APP 证书](https://doc.shengwang.cn/doc/rtc/javascript/get-started/enable-service)（每月免费分钟数）<br>• [OpenAI](https://openai.com/index/openai-api/) API 密钥（任何兼容 OpenAI 的 LLM）<br>• [Deepgram](https://deepgram.com/) ASR（注册可获得免费积分）<br>• [Elevenlabs](https://elevenlabs.io/) TTS（注册可获得免费积分） |
| **安装** | • [Docker](https://www.docker.com/) / [Docker Compose](https://docs.docker.com/compose/)<br>• [Node.js(LTS) v18](https://nodejs.org/en) |
| **最低系统要求** | • CPU >= 2 核<br>• 内存 >= 4 GB |

<br>

> \[!NOTE]
>
> **macOS：Apple Silicon 上的 Docker 设置**
>
> 在 Docker 设置中取消选中"Use Rosetta for x86/amd64 emulation"，这可能会在 ARM 上导致较慢的构建时间，但在部署到 x64 服务器时性能将正常。

<br>

如果在国内，我们强烈建议在 SSH 中把代理打开，下载和安装的依赖的时候会更加丝滑。

```bash
# 如果用的代理软件没有增强模式的话， 建议手动把所有代理协议都打开
# export 的有效期为一个 session
export https_proxy=http://127.0.0.1:<port>
export http_proxy=http://127.0.0.1:<port>
export all_proxy=socks5://127.0.0.1:<port>

# Docker
export https_proxy=http://host.docker.internal:<port>
export http_proxy=http://host.docker.internal:<port>
export all_proxy=http://host.docker.internal:<port>

# tman 镜像设置
mkdir -p ~/.tman && echo '{
  "registry": {
    "default": {
      "index": "https://registry-ten.rtcdeveloper.cn/api/ten-cloud-store/v1/packages"
    }
  }
}' > ~/.tman/config.json

# GO 代理设置
export GOPROXY=https://goproxy.cn,direct

# pip 代理设置, 此设置需要先安装 pip
pip config set global.index-url https://pypi.tuna.tsinghua.edu.cn/simple
export UV_INDEX_URL=https://pypi.tuna.tsinghua.edu.cn/simple
```

#### 步骤 ⓶ - 在 VM 中构建 Agent

##### 1. 克隆仓库，`cd` 到 `ai-agents` 并从 `.env.example` 创建 `.env` 文件

```bash
cd ai_agent
cp ./.env.example ./.env
```

##### 2. 在 `.env` 中设置 Agora App ID 和 App Certificate

```bash
AGORA_APP_ID=
AGORA_APP_CERTIFICATE=
```

##### 3. 启动代理开发容器

```bash
docker compose up -d
```

##### 4. 进入容器

```bash
docker exec -it ten_agent_dev bash
```

##### 5. 使用默认 `graph` 构建代理（约 5-8 分钟）

查看 `/examples` 文件夹获取更多示例

```bash
# 使用默认代理
task use

# 或使用演示代理
task use AGENT=agents/examples/demo
```

##### 6. 启动 Web 服务器

```bash
task run
```

<br>

#### 步骤 ⓷ - 使用 TMAN Designer 自定义您的 Agent

![使用 TMAN Designer 自定义您的代理](https://github.com/user-attachments/assets/33f8357b-6762-45eb-8231-c2d83bb77591)

 1. 打开 [localhost:49483](http://localhost:49483)。
 2. 从菜单中选择相应的图表（例如，语音助手）。
 3. 为每个扩展输入 API 密钥并设置首选项。
 4. 选择语音助手后，打开 [localhost:3000](http://localhost:3000) 查看更改。

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

### 🅱️ 在 Codespace 中运行 Playground(无需 Docker)

GitHub 为每个仓库提供免费的 Codespace，您可以在 Codespace 中运行 Playground 而无需使用 Docker。此外，Codespace 的速度比 localhost 快得多。

[codespaces-shield]: <https://github.com/codespaces/badge.svg>
[![][codespaces-shield]](https://codespaces.new/ten-framework/ten-agent)

查看[本指南](https://theten.ai/docs/ten_agent/setup_development_env/setting_up_development_inside_codespace)了解更多详情。

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

## 🛳️ TEN Agent 自托管

#### 🅰️ 🐳 使用 Docker 部署

一旦您自定义了代理（通过使用 TMAN Manager、Playground 或直接编辑 `property.json`），您就可以通过为您的服务创建发布 Docker 镜像来部署它。

阅读[部署指南](https://theten.ai/docs/ten_agent/deploy_ten_agent/deploy_agent_service)了解有关部署的详细信息。

<br>

#### 🅱️ 使用其他云服务部署

*即将推出*

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

## 🌏 TEN 生态

| 项目 | 预览 |
| ------- | ------- |
| [**🏚️ TEN Framework**][ten-framework-link]<br>TEN 是一个用于实时、多模态对话 AI 的开源框架。<br><br>![][ten-framework-shield] | ![][ten-framework-banner] |
| [**TEN VAD**][ten-vad-link]<br>TEN VAD 是一个低延迟、轻量级和高性能的流式语音活动检测器 (VAD)。<br><br>![][ten-vad-shield] | ![][ten-vad-banner] |
| [**️TEN Turn Detection**][ten-turn-detection-link]<br>TEN 用于全双工对话通信。<br><br>![][ten-turn-detection-shield] | ![][ten-turn-detection-banner] |
| [**🎙️ TEN Agent**][ten-agent-link]<br>TEN Agent 是 TEN Framework 的展示。<br><br> | ![][ten-agent-banner] |
| **🎨 TMAN Designer** `beta`<br>TMAN Designer 是一个低代码/无代码选项，用于通过易于使用的工作流 UI 创建语音代理。<br><br> | ![][tman-designer-banner] |
| [**📒 TEN Portal**][ten-portal-link]<br>TEN 框架的官方网站，包含文档和博客。<br><br>![][ten-portal-shield] | ![][ten-portal-banner] |

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

## ❓ 问题

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/TEN-framework/TEN-framework)

大多数问题都可以通过使用 DeepWiki 得到解答，它快速、直观且支持多种语言。

<div align="right">

[![][back-to-top]](#readme-top)

</div>

## 🥰 贡献

我们欢迎各种形式的开源合作！无论您是修复错误、添加功能、改进文档还是分享想法 - 您的贡献都有助于推进个性化 AI 工具的发展。查看我们的 GitHub Issues 和 Projects 找到贡献方式并展示您的技能。让我们一起构建一些令人惊叹的东西！

<br>

> \[!TIP]
>
> **欢迎各种形式的贡献** 🙏
>
> 加入我们，让 TEN 变得更好！从代码到文档，每个贡献都很重要。在社交媒体上分享您的 TEN Agent 项目，以激励他人！
>
> 在 𝕏 上联系 TEN 维护者 [@elliotchen100](https://x.com/elliotchen100) 或在 GitHub 上联系 [@cyfyifanchen](https://github.com/cyfyifanchen)，获取项目更新、讨论和合作机会。

<br>

### 代码贡献者

[![TEN](https://contrib.rocks/image?repo=TEN-framework/ten-agent)](https://github.com/TEN-framework/ten-agent/graphs/contributors)

### 贡献指南

欢迎贡献！请先阅读[贡献指南](./docs/code-of-conduct/contributing.md)。

### 许可证

1. 整个 TEN 框架（除了下面明确列出的文件夹外）在 Apache License, Version 2.0 下发布，并附有额外限制。详情请参阅位于 TEN 框架根目录的 [LICENSE](./LICENSE) 文件。

2. `packages` 目录中的组件在 Apache License, Version 2.0 下发布。详情请参阅每个包根目录中的 `LICENSE` 文件。

3. TEN 框架使用的第三方库已详细列出和描述。更多信息，请参阅 [third_party](./third_party/) 文件夹。

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

[ten-portal-link]: https://github.com/ten-framework/portal
[ten-portal-shield]: https://img.shields.io/github/stars/ten-framework/portal?color=ffcb47&labelColor=gray&style=flat-square&logo=github
[ten-portal-banner]: https://github.com/user-attachments/assets/e17d8aaa-5928-45dd-ac71-814928e26a89
