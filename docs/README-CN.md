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

[官方网站](https://theten.ai)
•
[文档](https://theten.ai/docs/ten_agent/overview)
•
[博客](https://theten.ai/blog)

<a href="https://github.com/TEN-framework/ten-framework/blob/main/README.md"><img alt="README（英文）" src="https://img.shields.io/badge/English-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-CN.md"><img alt="简体中文指南" src="https://img.shields.io/badge/简体中文-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-JP.md"><img alt="README（日语）" src="https://img.shields.io/badge/日本語-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-KR.md"><img alt="README（韩语）" src="https://img.shields.io/badge/한국어-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-ES.md"><img alt="README（西班牙语）" src="https://img.shields.io/badge/Español-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-FR.md"><img alt="README（法语）" src="https://img.shields.io/badge/Français-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-IT.md"><img alt="README（意大利语）" src="https://img.shields.io/badge/Italiano-lightgrey"></a>

<a href="https://trendshift.io/repositories/11978" target="_blank"><img src="https://trendshift.io/api/badge/repositories/11978" alt="TEN-framework%2Ften_framework | Trendshift" style="width: 250px; height: 55px;" width="250" height="55"/></a>

</div>

<br>

<details open>
  <summary><kbd>目录</kbd></summary>

  <br>

- [欢迎来到 TEN](#welcome-to-ten)
- [代理示例](#agent-examples)
- [代理示例快速上手](#quick-start-with-agent-examples)
  - [本地环境](#localhost)
  - [Codespaces](#codespaces)
- [代理示例自托管](#agent-examples-self-hosting)
  - [使用 Docker 部署](#deploying-with-docker)
  - [部署到其他云服务](#deploying-with-other-cloud-services)
- [保持关注](#stay-tuned)
- [TEN 生态](#ten-ecosystem)
- [常见问题](#questions)
- [参与贡献](#contributing)
  - [代码贡献者](#code-contributors)
  - [贡献指南](#contribution-guidelines)
  - [许可证](#license)

<br/>

</details>

<a name="welcome-to-ten"></a>

## 欢迎来到 TEN

TEN 是一个面向语音对话 AI 代理的开源框架。

[TEN 生态](#ten-ecosystem) 包含 [TEN Framework](https://github.com/ten-framework/ten-framework)、[代理示例](https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples)、[VAD](https://github.com/ten-framework/ten-vad)、[Turn Detection](https://github.com/ten-framework/ten-turn-detection) 以及 [Portal](https://github.com/ten-framework/portal)。

<br>

| 社区渠道 | 用途 |
| ---------------- | ------- |
| [![Follow on X](https://img.shields.io/twitter/follow/TenFramework?logo=X&color=%20%23f5f5f5)](https://twitter.com/intent/follow?screen_name=TenFramework) | 在 X 上关注 TEN Framework，获取更新与公告 |
| [![Discord TEN Community](https://img.shields.io/badge/Discord-Join%20TEN%20Community-5865F2?style=flat&logo=discord&logoColor=white)](https://discord.gg/VnPftUzAMJ) | 加入 Discord 社区，与开发者交流 |
| [![Follow on LinkedIn](https://custom-icon-badges.demolab.com/badge/LinkedIn-TEN_Framework-0A66C2?logo=linkedin-white&logoColor=fff)](https://www.linkedin.com/company/ten-framework) | 在 LinkedIn 上关注 TEN Framework，获取动态和公告 |
| [![Hugging Face Space](https://img.shields.io/badge/Hugging%20Face-TEN%20Framework-yellow?style=flat&logo=huggingface)](https://huggingface.co/TEN-framework) | 加入 Hugging Face 社区，探索我们的空间与模型 |
| [![WeChat](https://img.shields.io/badge/TEN_Framework-WeChat_Group-%2307C160?logo=wechat&labelColor=darkgreen&color=gray)](https://github.com/TEN-framework/ten-agent/discussions/170) | 加入微信社群，与中文社区讨论 |

<br>

<a name="agent-examples"></a>

## 代理示例

<br>

![Image](https://github.com/user-attachments/assets/dce3db80-fb48-4e2a-8ac7-33f50bcffa32)

<strong>多用途语音助手</strong> —— 低延迟、高质量的实时助手，可通过 <a href="ai_agents/agents/examples/voice-assistant-with-memU">记忆</a>、<a href="ai_agents/agents/examples/voice-assistant-with-ten-vad">VAD</a>、<a href="ai_agents/agents/examples/voice-assistant-with-turn-detection">回合检测</a> 等扩展能力进行增强。

详见 <a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples/voice-assistant">示例代码</a>。

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/aa3f2c9c-c82e-412f-8400-06378ba75794)

<strong>唇形同步头像</strong> —— 适配多个头像供应商，演示中包含 Live2D 唇形同步的动漫角色 Kei，并即将支持 Trulience、HeyGen、Tavus 等写实头像。

查看 <a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples/voice-assistant-live2d">Live2D 示例代码</a>。

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/f94b21b8-9dda-4efc-9274-b028cc01296a)

<strong>语音分离（Diarization）</strong> —— 实时检测并标记不同说话人，“Who Likes What” 游戏展示了一个交互式场景。

<a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples/speechmatics-diarization">示例代码</a>

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/6ed5b04d-945a-4a30-a1cc-f8014b602b38)

<strong>SIP 通话</strong> —— 通过 TEN 提供电话功能的 SIP 扩展。

<a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples/voice-assistant-sip-twilio">示例代码</a>

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/d793bc6c-c8de-4996-bd85-9ce88c69dd8d)

<strong>转写（Transcription）</strong> —— 将音频实时转换为文本的工具。

<a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples/transcription">示例代码</a>

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/3d60f1ff-0f82-4fe7-b5c2-ac03d284f60c)

<strong>ESP32-S3 Korvo V3</strong> —— 在 Espressif ESP32-S3 Korvo V3 开发板上运行 TEN 代理示例，让硬件具备 LLM 驱动的交互能力。

更多细节请参阅 <a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/esp32-client">集成指南</a>。

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<a name="quick-start-with-agent-examples"></a>

## 代理示例快速上手

<a name="localhost"></a>

### 本地环境

#### 步骤 ⓵ - 前置条件

| 类别 | 要求 |
| --- | --- |
| **密钥** | • Agora [App ID](https://docs.agora.io/en/video-calling/get-started/manage-agora-account?platform=web#create-an-agora-project) 与 [App Certificate](https://docs.agora.io/en/video-calling/get-started/manage-agora-account?platform=web#create-an-agora-project)（每月赠送免费分钟）<br>• [OpenAI](https://openai.com/index/openai-api/) API Key（兼容 OpenAI 协议的任意 LLM）<br>• [Deepgram](https://deepgram.com/) ASR（注册即可获得免费额度）<br>• [ElevenLabs](https://elevenlabs.io/) TTS（注册即可获得免费额度） |
| **安装** | • [Docker](https://www.docker.com/) / [Docker Compose](https://docs.docker.com/compose/)<br>• [Node.js (LTS) v18](https://nodejs.org/en) |
| **最低系统要求** | • CPU ≥ 2 核<br>• RAM ≥ 4 GB |

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

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

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<!-- > [!NOTE]
> **macOS：Apple Silicon 上的 Docker 设置**
>
> 在 Docker 设置中取消选中 “Use Rosetta for x86/amd64 emulation”。虽然在 ARM 设备上的构建速度可能更慢，但部署到 x64 服务器后性能正常。 -->

#### 步骤 ⓶ - 在虚拟机中构建代理示例

##### 1. 克隆仓库，进入 `ai_agents`，并用 `.env.example` 创建 `.env`

```bash
cd ai_agents
cp ./.env.example ./.env
```

##### 2. 在 `.env` 中配置 Agora App ID 与 App Certificate

```bash
AGORA_APP_ID=
AGORA_APP_CERTIFICATE=

# 运行默认的语音助手示例
# Deepgram（语音转文本所需）
DEEPGRAM_API_KEY=your_deepgram_api_key_here

# OpenAI（语言模型所需）
OPENAI_API_KEY=your_openai_api_key_here
OPENAI_MODEL=gpt-4o

# ElevenLabs（语音合成所需）
ELEVENLABS_TTS_KEY=your_elevenlabs_api_key_here
```

##### 3. 启动代理开发容器

```bash
docker compose up -d
```

##### 4. 进入容器

```bash
docker exec -it ten_agent_dev bash
```

##### 5. 使用默认示例构建代理（约 5-8 分钟）

在 `agents/examples` 文件夹中可以找到更多示例。
从以下默认示例之一开始：

```bash
# 使用串联语音助手
cd agents/examples/voice-assistant

# 或者使用实时语音对话助手
cd agents/examples/voice-assistant-realtime
```

##### 6. 启动 Web 服务

如果修改过本地源码，先运行 `task build`。对于 TypeScript、Go 等需要编译的语言必须执行此步骤，Python 项目则无需。

```bash
task install
task run
```

##### 7. 访问代理

启动示例后，可以访问以下界面：

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

- TMAN Designer：<http://localhost:49483>
- 代理示例界面：<http://localhost:3000>

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

#### 步骤 ⓷ - 自定义代理示例

1. 打开 [localhost:49483](http://localhost:49483)。
2. 右键单击 STT、LLM、TTS 扩展。
3. 在属性面板中填写对应的 API Key。
4. 提交更改后，即可在 [localhost:3000](http://localhost:3000) 查看更新效果。

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

<a name="codespaces"></a>

### Codespaces

GitHub 为每个仓库提供免费的 Codespaces。无需 Docker 即可运行代理示例，并且通常比本地 Docker 环境启动更快。

[codespaces-shield]: <https://github.com/codespaces/badge.svg>
[![][codespaces-shield]](https://codespaces.new/ten-framework/ten-agent)

更多细节请查看[这篇指南](https://theten.ai/docs/ten_agent/setup_development_env/setting_up_development_inside_codespace)。

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

<a name="agent-examples-self-hosting"></a>

## 代理示例自托管

<a name="deploying-with-docker"></a>

### 使用 Docker 部署

当你通过 TMAN Designer 或直接编辑 `property.json` 自定义完代理后，可以为服务构建发布用的 Docker 镜像并部署。

##### 以 Docker 镜像方式发布

**注意**：以下命令需在 Docker 容器外部执行。

###### 构建镜像

```bash
cd ai_agents
docker build -f agents/examples/<example-name>/Dockerfile -t example-app .
```

###### 运行

```bash
docker run --rm -it --env-file .env -p 3000:3000 example-app
```

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<a name="deploying-with-other-cloud-services"></a>

### 部署到其他云服务

若需将 TEN 部署到 [Vercel](https://vercel.com)、[Netlify](https://www.netlify.com) 等平台，可以拆分为前后端两部分：

1. 在任意支持容器的平台（Docker 主机、Fly.io、Render、ECS、Cloud Run 等）运行 TEN 后端。直接使用示例镜像，开放 `8080` 端口。
2. 仅部署前端到 Vercel 或 Netlify。将项目根目录指向 `ai_agents/agents/examples/<example>/frontend`，执行 `pnpm install`（或 `bun install`）与 `pnpm build`（或 `bun run build`），并保留默认 `.next` 输出。
3. 在托管平台的环境变量面板设置 `AGENT_SERVER_URL` 指向后端 URL，同时配置前端需要的 `NEXT_PUBLIC_*` 变量（例如需要暴露给浏览器的 Agora 凭据）。
4. 确保后端允许来自前端域名的请求，可通过开放 CORS 或使用内置代理中间件。

这样后端负责长时间运行的工作进程，托管前端只需将 API 请求转发给后端即可。

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

<a name="stay-tuned"></a>

## 保持关注

实时获取版本更新与最新动态。你的支持能帮助 TEN 变得更好！

<br>

![Image](https://github.com/user-attachments/assets/72c6cc46-a2a2-484d-82a9-f3079269c815)

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

<a name="ten-ecosystem"></a>

## TEN 生态

<br>

| 项目 | 预览 |
| ------- | ------- |
| [**️TEN Framework**][ten-framework-link]<br>面向对话式 AI 代理的开源框架。<br><br>![][ten-framework-shield] | ![][ten-framework-banner] |
| [**TEN VAD**][ten-vad-link]<br>低延迟、轻量且高性能的流式语音活动检测。<br><br>![][ten-vad-shield] | ![][ten-vad-banner] |
| [**️TEN Turn Detection**][ten-turn-detection-link]<br>实现全双工对话的回合检测。<br><br>![][ten-turn-detection-shield] | ![][ten-turn-detection-banner] |
| [**TEN Agent Examples**][ten-agent-link]<br>基于 TEN 的多种应用示例。<br><br> | ![][ten-agent-banner] |
| [**TEN Portal**][ten-portal-link]<br>TEN 官方站点，提供文档与博客。<br><br>![][ten-portal-shield] | ![][ten-portal-banner] |

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

<a name="questions"></a>

## 常见问题

TEN Framework 也可通过以下 AI 驱动的问答平台获取信息，它们支持多语言检索，覆盖从基础配置到高级实践的内容。

| 服务 | 链接 |
| ------- | ---- |
| DeepWiki | [![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/TEN-framework/TEN-framework) |
| ReadmeX | [![ReadmeX](https://raw.githubusercontent.com/CodePhiliaX/resource-trusteeship/main/readmex.svg)](https://readmex.com/TEN-framework/ten-framework) |

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<a name="contributing"></a>

## 参与贡献

欢迎所有形式的开源协作！无论是修复缺陷、添加功能、改进文档还是分享创意，你的参与都能推动个性化 AI 工具向前发展。访问 GitHub Issues 与 Projects 寻找合适的任务，展示你的技能，让我们一起构建更出色的 TEN！

<br>

> [!TIP]
>
> **欢迎所有类型的贡献** 🙏
>
> 帮助我们让 TEN 变得更好！从代码到文档，每一次贡献都弥足珍贵。也欢迎在社交平台分享你的 TEN 代理项目，激励更多创作者。
>
> 可以通过 𝕏 上的 [@elliotchen200](https://x.com/elliotchen200) 或 GitHub 上的 [@cyfyifanchen](https://github.com/cyfyifanchen) 与维护者联系，获取项目动态、讨论与合作机会。

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<a name="code-contributors"></a>

### 代码贡献者

[![TEN](https://contrib.rocks/image?repo=TEN-framework/ten-agent)](https://github.com/TEN-framework/ten-agent/graphs/contributors)

<a name="contribution-guidelines"></a>

### 贡献指南

欢迎贡献！在提交之前，请先阅读[贡献指南](./code-of-conduct/contributing.md)。

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<a name="license"></a>

### 许可证

1. 除下列目录外，TEN Framework 均以 Apache License 2.0（附加条款）发布，详见根目录下的 [LICENSE](./../LICENSE) 文件。
2. `packages` 目录中的组件以 Apache License 2.0 发布，详情请参考各组件根目录内的 `LICENSE` 文件。
3. TEN Framework 使用的第三方库均在 [third_party](./../third_party/) 目录中列出并说明。

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
