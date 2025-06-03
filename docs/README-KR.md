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
  <summary><kbd>목차</kbd></summary>

#### 목차

- [👋 TEN에 오신 것을 환영합니다](#-welcome-to-ten)
- [🎨 TMAN Designer](#-tman-designer)
- [🤖 TEN Agent](#-ten-agent)
  - [1️⃣ 실시간 아바타](#1️⃣-real-time-avatar)
  - [2️⃣ MCP 서버를 통한 실시간 음성](#2️⃣-real-time-voice-with-mcp-servers)
  - [3️⃣ 하드웨어와의 실시간 통신](#3️⃣-real-time-communication-with-hardware)
  - [4️⃣ 실시간 비전 및 화면 공유 감지](#4️⃣-real-time-vision-and-real-time-screenshare-detection)
  - [5️⃣ 다른 LLM 플랫폼과의 통합](#5️⃣-ten-with-other-llm-platforms)
  - [6️⃣ StoryTeller - TEN 이미지 생성](#6️⃣-storyteller---ten-image-generation)
- [🛝 TEN Agent Playground](#-ten-agent-playground)
  - [️🅰️ localhost에서 Playground 실행](#🅰️-run-playground-in-localhost)
  - [️🅱️ Codespace에서 Playground 실행 (Docker 불필요)](#🅱️-run-playground-in-codespaceno-docker)
- [🛳️ TEN Agent 자체 호스팅](#️-ten-agent-self-hosting)
  - [🅰️ 🐳 Docker로 배포](#️--deploying-with-docker)
  - [🅱️ 다른 클라우드 서비스로 배포](#️-deploying-with-other-cloud-services)
- [🏗️ TEN Agent 아키텍처](#️-ten-agent-architecture)
- [🌏 TEN Framework 생태계](#-ten-framework-ecosystem)
- [❓ 질문하기](#-ask-questions)
- [🥰 기여하기](#-contributing)
  - [코드 기여자](#code-contributors)
  - [기여 가이드라인](#contribution-guidelines)
  - [라이선스](#license)

<br/>

</details>

## 👋 TEN에 오신 것을 환영합니다

TEN은 실시간 멀티모달 대화형 음성 에이전트를 구축하기 위한 오픈소스 프로젝트 모음입니다. [TEN Framework](https://github.com/ten-framework/ten-framework), [TEN Turn Detection](https://github.com/ten-framework/ten-turn-detection), TEN Agent, TMAN Designer, [TEN Portal](https://github.com/ten-framework/portal)을 포함하며, 이들은 모두 완전한 오픈소스입니다. [TEN VAD](https://github.com/ten-framework/ten-vad)는 아직 완전한 오픈소스는 아니지만, 공개적으로 사용 가능합니다.

<br>

| 커뮤니티 채널 | 목적 |
| ---------------- | ------- |
| [![Follow on X](https://img.shields.io/twitter/follow/TenFramework?logo=X&color=%20%23f5f5f5)](https://twitter.com/intent/follow?screen_name=TenFramework) | X에서 TEN Framework를 팔로우하여 업데이트와 공지사항 확인 |
| [![Follow on LinkedIn](https://custom-icon-badges.demolab.com/badge/LinkedIn-TEN_Framework-0A66C2?logo=linkedin-white&logoColor=fff)](https://www.linkedin.com/company/ten-framework) | LinkedIn에서 TEN Framework를 팔로우하여 업데이트와 공지사항 확인 |
| [![Discord TEN Community](https://dcbadge.vercel.app/api/server/VnPftUzAMJ?&style=flat&theme=light&color=lightgray)](https://discord.gg/VnPftUzAMJ) | Discord 커뮤니티에 참여하여 개발자들과 연결 |
| [![Hugging Face Space](https://img.shields.io/badge/Hugging%20Face-TEN%20Framework-yellow?style=flat&logo=huggingface)](https://huggingface.co/TEN-framework) | Hugging Face 커뮤니티에 참여하여 스페이스와 모델 탐색 |
| [![WeChat](https://img.shields.io/badge/TEN_Framework-WeChat_Group-%2307C160?logo=wechat&labelColor=darkgreen&color=gray)](https://github.com/TEN-framework/ten-agent/discussions/170) | 중국어 커뮤니티 토론을 위해 WeChat 그룹 참여 |

<br>

> \[!IMPORTANT]
>
> **TEN 저장소에 스타를 눌러주세요** ⭐️
>
> 새로운 릴리스와 업데이트에 대한 즉각적인 알림을 받을 수 있습니다. 여러분의 지원이 TEN의 성장과 개선을 도와줍니다!

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

![TMAN Designer](https://github.com/user-attachments/assets/04fd75df-4de9-41b6-8aab-19014ecb46a4)

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

## ✨ 기능

![TEN Agent with Trulience](https://github.com/user-attachments/assets/2f1dfd55-14a3-47ea-ae25-40ad40ceadea)

### 1️⃣ 실시간 아바타

[TEN Agent](https://github.com/TEN-framework/ten-framework/tree/main/ai_agents)를 사용하여 [Trulience](https://trulience.com)의 다양한 무료 아바타 옵션으로 매력적인 AI 아바타를 구축할 수 있습니다. 실행하기 위해서는 단 2단계만 필요합니다:

1. README에 따라 Playground 설정 및 실행 완료
2. [Trulience](https://trulience.com)에서 받은 아바타 ID와 [토큰](https://trulience.com/docs#/authentication/jwt-tokens/jwt-tokens?id=use-your-custom-userid) 입력

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

![TEN Agent with MCP servers](https://github.com/user-attachments/assets/934ba928-a4a3-4662-8624-cebefc88ce05)

### 2️⃣ MCP 서버를 통한 실시간 음성

TEN Agent는 이제 MCP 서버와 원활하게 통합되어 LLM 기능을 확장합니다. 시작하려면:

1. Playground에서 Module Picker 열기
2. LLM 통합을 위한 MCP 서버 도구 추가
3. 확장 기능에 MCP 서버의 URL 붙여넣기
4. TEN Agent와 실시간 대화 시작

이 통합을 통해 TEN Agent의 강력한 대화 능력을 유지하면서 MCP의 다양한 서버 기능을 활용할 수 있습니다.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

<https://github.com/user-attachments/assets/78647eef-2d66-44e6-99a8-1918a940fb9f>

### 3️⃣ 하드웨어와의 실시간 통신

TEN Agent는 현재 Espressif ESP32-S3 Korvo V3 개발 보드에서 실행되고 있으며, 하드웨어에서 LLM과의 실시간 통신을 통합하는 훌륭한 방법을 제공합니다.

자세한 내용은 [통합 가이드](https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/esp32-client)를 참조하세요.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

![Real-time vision](https://github.com/user-attachments/assets/7be06e38-994e-4f82-8ec6-183d08fe90f1)

### 4️⃣ 실시간 비전 및 화면 공유 감지

Google Gemini Multimodal Live API를 사용하여 실시간 비전과 화면 공유 감지 기능을 경험해보세요. 이는 바로 사용할 수 있는 확장 기능으로, Weather Check와 Web Search와 같은 강력한 도구들이 TEN Agent에 완벽하게 통합되어 있습니다.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

![TEN with other LLM platforms](https://github.com/user-attachments/assets/a3766d50-6a25-4299-b28c-e15772e4201c)

### 5️⃣ 다른 LLM 플랫폼과의 통합

[TEN Agent + Dify](https://doc.theten.ai/docs/ten_agent/playground/use-cases/voice-assistant/run_dify)

TEN은 다른 LLM 플랫폼에서도 실시간 인터랙티브 경험을 더욱 향상시키는 훌륭한 지원을 제공합니다. 자세한 내용은 문서를 참조하세요.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

![Image](https://github.com/user-attachments/assets/ea1025d4-b22b-4416-ab35-36cd910bc28c)

### 6️⃣ StoryTeller - TEN 이미지 생성

StoryTeller를 사용한 실시간 이미지 생성을 경험해보세요. 이는 바로 사용할 수 있는 확장 기능으로, Weather Check와 Web Search와 같은 강력한 도구들이 TEN에 완벽하게 통합되어 있습니다.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

## 🛝 TEN Agent Playground로 빠르게 시작하기

#### 🅰️ localhost에서 Playground 실행

#### 단계 ⓵ - 필수 요구사항

| 카테고리 | 요구사항 |
| --- | --- |
| **키** | • Agora [App ID](https://docs.agora.io/en/video-calling/get-started/manage-agora-account?platform=web#create-an-agora-project)와 [App Certificate](https://docs.agora.io/en/video-calling/get-started/manage-agora-account?platform=web#create-an-agora-project) (매월 무료 사용량 제공)<br>• [OpenAI](https://openai.com/index/openai-api/) API 키 (OpenAI와 호환되는 모든 LLM)<br>• [Deepgram](https://deepgram.com/) ASR (가입 시 무료 크레딧 제공)<br>• [Elevenlabs](https://elevenlabs.io/) TTS (가입 시 무료 크레딧 제공) |
| **설치** | • [Docker](https://www.docker.com/) / [Docker Compose](https://docs.docker.com/compose/)<br>• [Node.js(LTS) v18](https://nodejs.org/en) |
| **최소 시스템 요구사항** | • CPU >= 2코어<br>• RAM >= 4 GB |

<br>

> \[!NOTE]
>
> **macOS: Apple Silicon에서의 Docker 설정**
>
> Docker 설정에서 "Use Rosetta for x86/amd64 emulation" 옵션의 체크를 해제하세요. ARM에서의 빌드 시간은 더 오래 걸릴 수 있지만, x64 서버에 배포할 때의 성능은 정상적입니다.

<br>

#### 단계 ⓶ - VM에서 에이전트 빌드

##### 1. 저장소를 클론하고 `ai-agents`로 이동한 후 `.env.example`에서 `.env` 파일 생성

```bash
cd ai_agent
cp ./.env.example ./.env
```

##### 2. `.env`에 Agora App ID와 App Certificate 설정

```bash
AGORA_APP_ID=
AGORA_APP_CERTIFICATE=
```

##### 3. 에이전트 개발 컨테이너 시작

```bash
docker compose up -d
```

##### 4. 컨테이너 접속

```bash
docker exec -it ten_agent_dev bash
```

##### 5. 기본 `graph`로 에이전트 빌드 (약 5분~8분 소요)

다른 예제는 `/examples` 폴더를 확인하세요

```bash
# 기본 에이전트 사용
task use

# 또는 데모 에이전트 사용
task use AGENT=agents/examples/demo
```

##### 6. 웹 서버 시작

```bash
task run
```

<br>

#### 단계 ⓷ - TMAN Designer로 에이전트 커스터마이징

![Customize your agent with TMAN Designer](https://github.com/user-attachments/assets/33f8357b-6762-45eb-8231-c2d83bb77591)

 1. [localhost:49483](http://localhost:49483) 열기
 2. 메뉴에서 해당하는 그래프 로드 (예: Voice Assistant)
 3. 각 확장 기능의 API 키 입력 및 설정
 4. Voice Assistant 선택 후 [localhost:3000](http://localhost:3000)을 열어 변경사항 확인

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

### 🅱️ Codespace에서 Playground 실행 (Docker 불필요)

GitHub는 각 저장소에 무료 Codespace를 제공하며, Docker 없이도 Playground를 실행할 수 있습니다. 또한 Codespace의 속도는 localhost보다 훨씬 빠릅니다.

[codespaces-shield]: <https://github.com/codespaces/badge.svg>
[![][codespaces-shield]](https://codespaces.new/ten-framework/ten-agent)

자세한 내용은 [이 가이드](https://theten.ai/docs/ten_agent/setup_development_env/setting_up_development_inside_codespace)를 참조하세요.

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

## 🛳️ TEN Agent 자체 호스팅

#### 🅰️ 🐳 Docker로 배포

에이전트를 커스터마이징한 후(TMAN Manager, Playground 또는 `property.json` 직접 편집), 릴리스용 Docker 이미지를 생성하여 서비스를 배포할 수 있습니다.

배포에 대한 자세한 내용은 [배포 가이드](https://theten.ai/docs/ten_agent/deploy_ten_agent/deploy_agent_service)를 참조하세요.

<br>

#### 🅱️ 다른 클라우드 서비스로 배포

*준비 중*

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

## 🌏 TEN Framework 생태계

| 프로젝트 | 미리보기 |
| ------- | ------- |
| [**🏚️ TEN Framework**][ten-framework-link]<br>TEN은 실시간 멀티모달 대화형 AI를 위한 오픈소스 프레임워크입니다.<br><br>![][ten-framework-shield] | ![][ten-framework-banner] |
| [**️🔂 TEN Turn Detection**][ten-turn-detection-link]<br>TEN은 전이중 대화 통신을 위한 것입니다.<br><br>![][ten-turn-detection-shield] | ![][ten-turn-detection-banner] |
| [**🔉 TEN VAD**][ten-vad-link]<br>TEN VAD는 저지연, 경량, 고성능 스트리밍 음성 활동 감지기(VAD)입니다.<br><br>![][ten-vad-shield] | ![][ten-vad-banner] |
| [**🎙️ TEN Agent**][ten-agent-link]<br>TEN Agent는 TEN Framework의 데모입니다.<br><br> | ![][ten-agent-banner] |
| **🎨 TMAN Designer** <br>TMAN Designer는 사용하기 쉬운 워크플로우 UI로 음성 에이전트를 만들기 위한 low/no-code 옵션입니다.<br><br> | ![][tman-designer-banner] |
| [**📒 TEN Portal**][ten-portal-link]<br>TEN framework의 공식 사이트로, 문서와 블로그가 있습니다.<br><br>![][ten-portal-shield] | ![][ten-portal-banner] |

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

## ❓ 질문하기

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/TEN-framework/TEN-framework)

대부분의 질문은 DeepWiki를 사용하여 답변할 수 있습니다. 빠르고 직관적으로 사용할 수 있으며 여러 언어를 지원합니다.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

## 🥰 기여하기

모든 형태의 오픈소스 협업을 환영합니다! 버그 수정, 기능 추가, 문서 개선, 아이디어 공유 등 여러분의 기여가 개인화된 AI 도구의 발전을 돕습니다. GitHub Issues와 Projects를 확인하여 기여 방법을 찾고 여러분의 기술을 보여주세요. 함께 멋진 것을 만들어봅시다!

<br>

> \[!TIP]
>
> **모든 종류의 기여를 환영합니다** 🙏
>
> TEN을 더 좋게 만드는 데 참여하세요! 코드부터 문서까지 모든 기여가 중요합니다. TEN Agent 프로젝트를 소셜 미디어에 공유하여 다른 사람들에게 영감을 주세요!
>
> 프로젝트 업데이트, 토론, 협업 기회에 대해 TEN의 메인테이너 [@elliotchen100](https://x.com/elliotchen100) (𝕏) 또는 [@cyfyifanchen](https://github.com/cyfyifanchen) (GitHub)에게 연락하세요.

<br>

### 코드 기여자

[![TEN](https://contrib.rocks/image?repo=TEN-framework/ten-agent)](https://github.com/TEN-framework/ten-agent/graphs/contributors)

### 기여 가이드라인

기여를 환영합니다! 먼저 [기여 가이드라인](./docs/code-of-conduct/contributing.md)을 읽어주세요.

### 라이선스

1. TEN framework 전체(아래에 명시적으로 나열된 폴더 제외)는 Apache License, Version 2.0으로 릴리스되며 추가 제한사항이 있습니다. 자세한 내용은 TEN framework의 루트 디렉토리에 있는 [LICENSE](./LICENSE) 파일을 참조하세요.

2. `packages` 디렉토리 내의 컴포넌트는 Apache License, Version 2.0으로 릴리스됩니다. 자세한 내용은 각 패키지의 루트 디렉토리에 있는 `LICENSE` 파일을 참조하세요.

3. TEN framework에서 사용하는 서드파티 라이브러리는 자세히 나열되어 설명되어 있습니다. 자세한 내용은 [third_party](./third_party/) 폴더를 참조하세요.

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
