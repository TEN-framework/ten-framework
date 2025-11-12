<div align="center"> <a name="readme-top"></a>

![Image](https://github.com/user-attachments/assets/2a560a74-68f3-4f4a-9ec8-89464c42a9c7)

[![TEN Releases]( https://img.shields.io/github/v/release/ten-framework/ten-framework?color=369eff&labelColor=gray&logo=github&style=flat-square )](https://github.com/TEN-framework/ten-framework/releases)
[![Coverage Status](https://coveralls.io/repos/github/TEN-framework/ten-framework/badge.svg?branch=HEAD)](https://coveralls.io/github/TEN-framework/ten-framework?branch=HEAD)
[![](https://img.shields.io/github/release-date/ten-framework/ten-framework?labelColor=gray&style=flat-square)](https://github.com/TEN-framework/ten-framework/releases)
[![Discussion posts](https://img.shields.io/github/discussions/TEN-framework/ten_framework?labelColor=gray&color=%20%23f79009)](https://github.com/TEN-framework/ten-framework/discussions/)
[![Commits](https://img.shields.io/github/commit-activity/m/TEN-framework/ten_framework?labelColor=gray&color=pink)](https://github.com/TEN-framework/ten-framework/graphs/commit-activity)
[![Issues closed](https://img.shields.io/github/issues-search?query=repo%3ATEN-framework%2Ften-framework%20is%3Aclosed&label=issues%20closed&labelColor=gray&color=green)](https://github.com/TEN-framework/ten-framework/issues)
[![](https://img.shields.io/github/contributors/ten-framework/ten-framework?color=c4f042&labelColor=gray&style=flat-square)](https://github.com/TEN-framework/ten-framework/graphs/contributors)
[![GitHub license](https://img.shields.io/badge/License-Apache_2.0_with_certain_conditions-blue.svg?labelColor=%20%23155EEF&color=%20%23528bff)](https://github.com/TEN-framework/ten_framework/blob/main/LICENSE)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/TEN-framework/TEN-framework)
[![ReadmeX](https://raw.githubusercontent.com/CodePhiliaX/resource-trusteeship/main/readmex.svg)](https://readmex.com/TEN-framework/ten-framework)

[공식 사이트](https://theten.ai)
•
[문서](https://theten.ai/docs/ten_agent/overview)
•
[블로그](https://theten.ai/blog)

<a href="https://github.com/TEN-framework/ten-framework/blob/main/README.md"><img alt="README (영어)" src="https://img.shields.io/badge/English-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-CN.md"><img alt="README (중국어)" src="https://img.shields.io/badge/简体中文-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-JP.md"><img alt="README (일본어)" src="https://img.shields.io/badge/日本語-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-KR.md"><img alt="README (한국어)" src="https://img.shields.io/badge/한국어-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-ES.md"><img alt="README (스페인어)" src="https://img.shields.io/badge/Español-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-FR.md"><img alt="README (프랑스어)" src="https://img.shields.io/badge/Français-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-IT.md"><img alt="README (이탈리아어)" src="https://img.shields.io/badge/Italiano-lightgrey"></a>

<a href="https://trendshift.io/repositories/11978" target="_blank"><img src="https://trendshift.io/api/badge/repositories/11978" alt="TEN-framework%2Ften_framework | Trendshift" style="width: 250px; height: 55px;" width="250" height="55"/></a>

</div>

<br>

<details open>
  <summary><kbd>목차</kbd></summary>

  <br>

- [TEN 소개](#welcome-to-ten)
- [에이전트 예시](#agent-examples)
- [에이전트 예시 빠른 시작](#quick-start-with-agent-examples)
  - [로컬 환경](#localhost)
  - [Codespaces](#codespaces)
- [에이전트 예시 셀프 호스팅](#agent-examples-self-hosting)
  - [Docker 배포](#deploying-with-docker)
  - [기타 클라우드 제공업체 배포](#deploying-with-other-cloud-services)
- [소식 받기](#stay-tuned)
- [TEN 에코시스템](#ten-ecosystem)
- [질문](#questions)
- [기여하기](#contributing)
  - [코드 기여자](#code-contributors)
  - [기여 가이드](#contribution-guidelines)
  - [라이선스](#license)

<br/>

</details>

<a name="welcome-to-ten"></a>

## TEN 소개

TEN은 음성 대화형 AI 에이전트를 위한 오픈소스 프레임워크입니다.

[TEN 에코시스템](#ten-ecosystem)에는 [TEN Framework](https://github.com/ten-framework/ten-framework), [에이전트 예시](https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples), [VAD](https://github.com/ten-framework/ten-vad), [Turn Detection](https://github.com/ten-framework/ten-turn-detection), [Portal](https://github.com/ten-framework/portal) 이 포함됩니다.

<br>

| 커뮤니티 채널 | 설명 |
| ---------------- | ------- |
| [![Follow on X](https://img.shields.io/twitter/follow/TenFramework?logo=X&color=%20%23f5f5f5)](https://twitter.com/intent/follow?screen_name=TenFramework) | X에서 TEN Framework 소식을 받아보세요 |
| [![Discord TEN Community](https://img.shields.io/badge/Discord-Join%20TEN%20Community-5865F2?style=flat&logo=discord&logoColor=white)](https://discord.gg/VnPftUzAMJ) | Discord 커뮤니티에 참여해 개발자들과 교류하세요 |
| [![Follow on LinkedIn](https://custom-icon-badges.demolab.com/badge/LinkedIn-TEN_Framework-0A66C2?logo=linkedin-white&logoColor=fff)](https://www.linkedin.com/company/ten-framework) | LinkedIn에서 업데이트와 공지를 확인하세요 |
| [![Hugging Face Space](https://img.shields.io/badge/Hugging%20Face-TEN%20Framework-yellow?style=flat&logo=huggingface)](https://huggingface.co/TEN-framework) | Hugging Face 커뮤니티에서 스페이스와 모델을 살펴보세요 |
| [![WeChat](https://img.shields.io/badge/TEN_Framework-WeChat_Group-%2307C160?logo=wechat&labelColor=darkgreen&color=gray)](https://github.com/TEN-framework/ten-agent/discussions/170) | 중국 커뮤니티와 대화할 수 있는 WeChat 그룹 |

<br>

<a name="agent-examples"></a>

## 에이전트 예시

<br>

![Image](https://github.com/user-attachments/assets/dce3db80-fb48-4e2a-8ac7-33f50bcffa32)

<strong>멀티 퍼포즈 보이스 어시스턴트</strong> — 저지연·고품질 실시간 어시스턴트로, <a href="ai_agents/agents/examples/voice-assistant-with-memU">메모리</a>, <a href="ai_agents/agents/examples/voice-assistant-with-ten-vad">VAD</a>, <a href="ai_agents/agents/examples/voice-assistant-with-turn-detection">턴 감지</a> 등으로 확장할 수 있습니다.

자세한 내용은 <a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples/voice-assistant">예시 코드</a>를 확인하세요.

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/aa3f2c9c-c82e-412f-8400-06378ba75794)

<strong>립싱크 아바타</strong> — 다양한 아바타 공급업체를 지원합니다. 데모에서는 Live2D 립싱크를 갖춘 애니메이션 캐릭터 Kei를 소개하며, 곧 Trulience, HeyGen, Tavus의 실사 아바타도 지원할 예정입니다.

<a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples/voice-assistant-live2d">Live2D 예시 코드</a>

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/f94b21b8-9dda-4efc-9274-b028cc01296a)

<strong>음성 화자 분리</strong> — 실시간으로 화자를 감지하고 라벨링합니다. "Who Likes What" 게임에서 인터랙티브한 사용 사례를 확인할 수 있습니다.

<a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples/speechmatics-diarization">예시 코드</a>

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/6ed5b04d-945a-4a30-a1cc-f8014b602b38)

<strong>SIP 통화</strong> — TEN 기반 전화 통화를 가능하게 하는 SIP 확장입니다.

<a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples/voice-assistant-sip-twilio">예시 코드</a>

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/d793bc6c-c8de-4996-bd85-9ce88c69dd8d)

<strong>전사(Transcription)</strong> — 오디오를 텍스트로 변환하는 전사 도구입니다.

<a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples/transcription">예시 코드</a>

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/3d60f1ff-0f82-4fe7-b5c2-ac03d284f60c)

<strong>ESP32-S3 Korvo V3</strong> — Espressif ESP32-S3 Korvo V3 개발 보드에서 TEN Agent 예시를 실행해 LLM 기반 커뮤니케이션을 하드웨어와 통합합니다.

<a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/esp32-client">통합 가이드</a>를 참고하세요.

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<a name="quick-start-with-agent-examples"></a>

## 에이전트 예시 빠른 시작

<a name="localhost"></a>

### 로컬 환경

#### 단계 ⓵ - 준비 사항

| 구분 | 요구 사항 |
| --- | --- |
| **키/토큰** | • Agora [App ID](https://docs.agora.io/en/video-calling/get-started/manage-agora-account?platform=web#create-an-agora-project), [App Certificate](https://docs.agora.io/en/video-calling/get-started/manage-agora-account?platform=web#create-an-agora-project) (매월 무료 분 제공)<br>• [OpenAI](https://openai.com/index/openai-api/) API 키 (OpenAI 호환 LLM)<br>• [Deepgram](https://deepgram.com/) ASR (회원가입 시 무료 크레딧)<br>• [ElevenLabs](https://elevenlabs.io/) TTS (회원가입 시 무료 크레딧) |
| **설치** | • [Docker](https://www.docker.com/) / [Docker Compose](https://docs.docker.com/compose/)<br>• [Node.js (LTS) v18](https://nodejs.org/en) |
| **최소 사양** | • CPU 2코어 이상<br>• RAM 4GB 이상 |

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<!-- > [!NOTE]
> **macOS: Apple Silicon 용 Docker 설정**
>
> Docker 설정에서 "Use Rosetta for x86/amd64 emulation" 옵션을 해제하세요. ARM 장비에서 빌드 속도가 다소 느릴 수 있지만, x64 서버에 배포하면 정상 성능을 보입니다. -->

#### 단계 ⓶ - VM 안에서 예시 빌드

##### 1. 저장소를 클론하고 `ai_agents`로 이동한 뒤 `.env.example`로부터 `.env` 생성

```bash
cd ai_agents
cp ./.env.example ./.env
```

##### 2. `.env`에 Agora App ID / App Certificate 설정

```bash
AGORA_APP_ID=
AGORA_APP_CERTIFICATE=

# 기본 보이스 어시스턴트 예시 실행
# Deepgram (STT 필수)
DEEPGRAM_API_KEY=your_deepgram_api_key_here

# OpenAI (LLM 필수)
OPENAI_API_KEY=your_openai_api_key_here
OPENAI_MODEL=gpt-4o

# ElevenLabs (TTS 필수)
ELEVENLABS_TTS_KEY=your_elevenlabs_api_key_here
```

##### 3. 개발용 컨테이너 시작

```bash
docker compose up -d
```

##### 4. 컨테이너 접속

```bash
docker exec -it ten_agent_dev bash
```

##### 5. 기본 예시로 에이전트 빌드 (약 5~8분)

`agents/examples` 폴더에서 다른 샘플도 확인할 수 있습니다.
다음 중 하나부터 시작하세요:

```bash
# 체이닝형 보이스 어시스턴트
cd agents/examples/voice-assistant

# 실시간 음성-음성 어시스턴트
cd agents/examples/voice-assistant-realtime
```

##### 6. 웹 서버 시작

로컬 소스를 수정했다면 `task build`를 실행하세요. TypeScript, Go 등 컴파일 언어에서는 필수이며 Python 에서는 선택입니다.

```bash
task install
task run
```

##### 7. 에이전트 접속

예시가 실행되면 아래 인터페이스를 이용할 수 있습니다.

<table>
  <tr>
    <td align="center">
      <b>localhost:49483</b>
      <img src="https://github.com/user-attachments/assets/191a7c0a-d8e6-48f9-866f-6a70c58f0118" alt="스크린샷 1" /><br/>
    </td>
    <td align="center">
      <b>localhost:3000</b>
      <img src="https://github.com/user-attachments/assets/13e482b6-d907-4449-a779-9454bb24c0b1" alt="스크린샷 2" /><br/>
    </td>
  </tr>
</table>

- TMAN Designer: <http://localhost:49483>
- 에이전트 예시 UI: <http://localhost:3000>

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

#### 단계 ⓷ - 예시 커스터마이징

1. [localhost:49483](http://localhost:49483)에 접속합니다.
2. STT, LLM, TTS 확장을 우클릭합니다.
3. 속성 창에서 해당 API 키를 입력합니다.
4. 저장 후 [localhost:3000](http://localhost:3000)에서 변경된 결과를 확인합니다.

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

<a name="codespaces"></a>

### Codespaces

GitHub는 저장소마다 무료 Codespaces를 제공합니다. Docker 없이도 에이전트 예시를 실행할 수 있으며, 일반적으로 로컬 Docker 환경보다 빠르게 시작됩니다.

[codespaces-shield]: <https://github.com/codespaces/badge.svg>
[![][codespaces-shield]](https://codespaces.new/ten-framework/ten-agent)

자세한 내용은 [이 가이드](https://theten.ai/docs/ten_agent/setup_development_env/setting_up_development_inside_codespace)를 참고하세요.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

<a name="agent-examples-self-hosting"></a>

## 에이전트 예시 셀프 호스팅

<a name="deploying-with-docker"></a>

### Docker 배포

TMAN Designer 또는 `property.json` 수정으로 에이전트를 커스터마이징했다면, 서비스용 Docker 릴리스 이미지를 만들어 배포할 수 있습니다.

##### Docker 이미지로 배포

**주의**: 아래 명령은 Docker 컨테이너 밖에서 실행하세요.

###### 이미지 빌드

```bash
cd ai_agents
docker build -f agents/examples/<example-name>/Dockerfile -t example-app .
```

###### 실행

```bash
docker run --rm -it --env-file .env -p 3000:3000 example-app
```

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<a name="deploying-with-other-cloud-services"></a>

### 기타 클라우드 제공업체 배포

[TEN을 Vercel](https://vercel.com)이나 [Netlify](https://www.netlify.com) 같은 플랫폼에 호스팅할 때는 백엔드와 프런트엔드를 분리할 수 있습니다.

1. Docker를 지원하는 플랫폼(도커 가능한 VM, Fly.io, Render, ECS, Cloud Run 등)에서 TEN 백엔드를 실행합니다. 예시 이미지를 그대로 사용하고 포트 `8080`을 개방하세요.
2. 프런트엔드는 Vercel 또는 Netlify에만 배포합니다. 프로젝트 루트를 `ai_agents/agents/examples/<example>/frontend`로 지정하고 `pnpm install`(또는 `bun install`) 후 `pnpm build`(또는 `bun run build`)를 실행하며 기본 `.next` 출력 디렉터리를 유지합니다.
3. 호스팅 대시보드에서 `AGENT_SERVER_URL`을 백엔드 주소로 설정하고, 브라우저에서 필요한 `NEXT_PUBLIC_*` 환경 변수를 추가합니다(예: Agora 자격 증명).
4. 백엔드가 프런트엔드 오리진을 허용하도록 CORS를 열거나 기본 제공 프록시 미들웨어를 사용하세요.

이 구조에서는 백엔드가 장기 실행 작업을 처리하고, 호스팅된 프런트엔드는 API 요청을 전달하기만 하면 됩니다.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

<a name="stay-tuned"></a>

## 소식 받기

새 릴리스와 업데이트를 즉시 받아보세요. 여러분의 응원이 TEN을 성장시킵니다!

<br>

![Image](https://github.com/user-attachments/assets/72c6cc46-a2a2-484d-82a9-f3079269c815)

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

<a name="ten-ecosystem"></a>

## TEN 에코시스템

<br>

| 프로젝트 | 미리보기 |
| ------- | ------- |
| [**️TEN Framework**][ten-framework-link]<br>대화형 AI 에이전트를 위한 오픈소스 프레임워크.<br><br>![][ten-framework-shield] | ![][ten-framework-banner] |
| [**TEN VAD**][ten-vad-link]<br>저지연·경량·고성능 스트리밍 음성 활동 감지.<br><br>![][ten-vad-shield] | ![][ten-vad-banner] |
| [**️TEN Turn Detection**][ten-turn-detection-link]<br>풀 듀플렉스 대화를 지원하는 턴 감지.<br><br>![][ten-turn-detection-shield] | ![][ten-turn-detection-banner] |
| [**TEN Agent Examples**][ten-agent-link]<br>TEN 기반 활용 사례 모음.<br><br> | ![][ten-agent-banner] |
| [**TEN Portal**][ten-portal-link]<br>문서와 블로그를 제공하는 공식 사이트.<br><br>![][ten-portal-shield] | ![][ten-portal-banner] |

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

<a name="questions"></a>

## 질문

TEN Framework는 AI 기반 Q&A 플랫폼에서도 만나볼 수 있습니다. 멀티랭귀지를 지원하며 기본 설정부터 고급 구현까지 빠르게 답을 찾을 수 있습니다.

| 서비스 | 링크 |
| ------- | ---- |
| DeepWiki | [![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/TEN-framework/TEN-framework) |
| ReadmeX | [![ReadmeX](https://raw.githubusercontent.com/CodePhiliaX/resource-trusteeship/main/readmex.svg)](https://readmex.com/TEN-framework/ten-framework) |

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<a name="contributing"></a>

## 기여하기

버그 수정, 기능 추가, 문서 개선, 아이디어 공유 등 모든 형태의 오픈소스 협업을 환영합니다. GitHub Issues와 Projects에서 참여할 작업을 찾아 능력을 보여주세요. 함께 멋진 TEN을 만들어봅시다!

<br>

> [!TIP]
>
> **어떤 형태의 기여든 환영합니다** 🙏
>
> 코드부터 문서까지, 모든 기여가 TEN을 더 좋게 만듭니다. TEN Agent 프로젝트를 SNS에 공유해 다른 제작자에게 영감을 주세요.
>
> 𝕏의 [@elliotchen200](https://x.com/elliotchen200) 또는 GitHub의 [@cyfyifanchen](https://github.com/cyfyifanchen)에게 연락하면 업데이트, 토론, 협업 기회를 얻을 수 있습니다.

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<a name="code-contributors"></a>

### 코드 기여자

[![TEN](https://contrib.rocks/image?repo=TEN-framework/ten-agent)](https://github.com/TEN-framework/ten-agent/graphs/contributors)

<a name="contribution-guidelines"></a>

### 기여 가이드

기여를 환영합니다! 먼저 [기여 가이드](./code-of-conduct/contributing.md)를 읽어 주세요.

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<a name="license"></a>

### 라이선스

1. 아래에 명시된 디렉터리를 제외한 TEN Framework 전체는 추가 제한이 포함된 Apache License 2.0으로 배포됩니다. 루트 디렉터리의 [LICENSE](./../LICENSE)를 참고하세요.
2. `packages` 디렉터리의 구성요소 역시 Apache License 2.0으로 배포되며, 각 패키지 루트의 `LICENSE` 파일에서 세부 내용을 확인할 수 있습니다.
3. TEN Framework에서 사용하는 서드파티 라이브러리는 [third_party](./../third_party/) 폴더에 정리되어 있습니다.

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

[ten-agent-link]: https://github.com/TEN-framework/ten-framework/tree/main/ai_agents
[ten-agent-banner]: https://github.com/user-attachments/assets/7f735633-c7f6-4432-b6b4-d2a2977ca588

[ten-portal-link]: https://github.com/ten-framework/portal
[ten-portal-shield]: https://img.shields.io/github/stars/ten-framework/portal?color=ffcb47&labelColor=gray&style=flat-square&logo=github
[ten-portal-banner]: https://github.com/user-attachments/assets/f56c75b9-722c-4156-902d-ae98ce2b3b5e
