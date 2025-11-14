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

[公式サイト](https://theten.ai)
•
[ドキュメント](https://theten.ai/docs/ten_agent/overview)
•
[ブログ](https://theten.ai/blog)

<a href="https://github.com/TEN-framework/ten-framework/blob/main/README.md"><img alt="README（英語）" src="https://img.shields.io/badge/English-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-CN.md"><img alt="README（簡体字中国語）" src="https://img.shields.io/badge/简体中文-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-JP.md"><img alt="README（日本語）" src="https://img.shields.io/badge/日本語-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-KR.md"><img alt="README（韓国語）" src="https://img.shields.io/badge/한국어-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-ES.md"><img alt="README（スペイン語）" src="https://img.shields.io/badge/Español-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-FR.md"><img alt="README（フランス語）" src="https://img.shields.io/badge/Français-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-IT.md"><img alt="README（イタリア語）" src="https://img.shields.io/badge/Italiano-lightgrey"></a>

<a href="https://trendshift.io/repositories/11978" target="_blank"><img src="https://trendshift.io/api/badge/repositories/11978" alt="TEN-framework%2Ften_framework | Trendshift" style="width: 250px; height: 55px;" width="250" height="55"/></a>

</div>

<br>

<details open>
  <summary><kbd>目次</kbd></summary>

  <br>

- [TEN へようこそ](#welcome-to-ten)
- [エージェント事例](#agent-examples)
- [エージェント事例のクイックスタート](#quick-start-with-agent-examples)
  - [ローカル環境](#localhost)
  - [Codespaces](#codespaces)
- [エージェント事例のセルフホスティング](#agent-examples-self-hosting)
  - [Docker でデプロイ](#deploying-with-docker)
  - [その他のクラウドサービスへデプロイ](#deploying-with-other-cloud-services)
- [最新情報](#stay-tuned)
- [TEN エコシステム](#ten-ecosystem)
- [質問](#questions)
- [コントリビュート](#contributing)
  - [コードコントリビューター](#code-contributors)
  - [貢献ガイドライン](#contribution-guidelines)
  - [ライセンス](#license)

<br/>

</details>

<a name="welcome-to-ten"></a>

## TEN へようこそ

TEN は音声会話型 AI エージェント向けのオープンソースフレームワークです。

[TEN エコシステム](#ten-ecosystem) には [TEN Framework](https://github.com/ten-framework/ten-framework)、[エージェント事例](https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples)、[VAD](https://github.com/ten-framework/ten-vad)、[Turn Detection](https://github.com/ten-framework/ten-turn-detection)、[Portal](https://github.com/ten-framework/portal) が含まれます。

<br>

| コミュニティ | 目的 |
| ---------------- | ------- |
| [![Follow on X](https://img.shields.io/twitter/follow/TenFramework?logo=X&color=%20%23f5f5f5)](https://twitter.com/intent/follow?screen_name=TenFramework) | X で TEN Framework をフォローして最新情報をチェック |
| [![Discord TEN Community](https://img.shields.io/badge/Discord-Join%20TEN%20Community-5865F2?style=flat&logo=discord&logoColor=white)](https://discord.gg/VnPftUzAMJ) | Discord コミュニティに参加し、開発者同士で交流 |
| [![Follow on LinkedIn](https://custom-icon-badges.demolab.com/badge/LinkedIn-TEN_Framework-0A66C2?logo=linkedin-white&logoColor=fff)](https://www.linkedin.com/company/ten-framework) | LinkedIn で TEN Framework をフォローし、ニュースを受け取る |
| [![Hugging Face Space](https://img.shields.io/badge/Hugging%20Face-TEN%20Framework-yellow?style=flat&logo=huggingface)](https://huggingface.co/TEN-framework) | Hugging Face コミュニティでスペースやモデルを探索 |
| [![WeChat](https://img.shields.io/badge/TEN_Framework-WeChat_Group-%2307C160?logo=wechat&labelColor=darkgreen&color=gray)](https://github.com/TEN-framework/ten-agent/discussions/170) | 中国語コミュニティ向けの WeChat グループに参加 |

<br>

<a name="agent-examples"></a>

## エージェント事例

<br>

![Image](https://github.com/user-attachments/assets/dce3db80-fb48-4e2a-8ac7-33f50bcffa32)

<strong>多目的ボイスアシスタント</strong> — 低レイテンシ・高品質のリアルタイムアシスタント。<a href="ai_agents/agents/examples/voice-assistant-with-memU">メモリ</a>、<a href="ai_agents/agents/examples/voice-assistant-with-ten-vad">VAD</a>、<a href="ai_agents/agents/examples/voice-assistant-with-turn-detection">ターン検出</a> などの拡張機能を追加できます。

詳細は <a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples/voice-assistant">サンプルコード</a> を参照してください。

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/aa3f2c9c-c82e-412f-8400-06378ba75794)

<strong>リップシンク対応アバター</strong> — 複数のアバタープロバイダーに対応。デモでは Live2D のリップシンクを備えたアニメキャラクター Kei を紹介し、今後 Trulience、HeyGen、Tavus のリアルアバターにも対応予定です。

<a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples/voice-assistant-live2d">Live2D 用サンプルコード</a>

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/f94b21b8-9dda-4efc-9274-b028cc01296a)

<strong>話者分離（Diarization）</strong> — 話者をリアルタイムで検出・ラベル付けします。ゲーム「Who Likes What」でインタラクティブな活用例を紹介しています。

<a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples/speechmatics-diarization">サンプルコード</a>

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/6ed5b04d-945a-4a30-a1cc-f8014b602b38)

<strong>SIP 通話</strong> — TEN で電話を実現する SIP 拡張機能です。

<a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples/voice-assistant-sip-twilio">サンプルコード</a>

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/d793bc6c-c8de-4996-bd85-9ce88c69dd8d)

<strong>文字起こし</strong> — 音声をテキストへ変換するトランスクリプションツール。

<a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples/transcription">サンプルコード</a>

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/3d60f1ff-0f82-4fe7-b5c2-ac03d284f60c)

<strong>ESP32-S3 Korvo V3</strong> — Espressif ESP32-S3 Korvo V3 開発ボード上で TEN Agent のサンプルを動作させ、LLM ベースのコミュニケーションをハードウェアに組み込みます。

<a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/esp32-client">統合ガイド</a> を参照してください。

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<a name="quick-start-with-agent-examples"></a>

## エージェント事例のクイックスタート

<a name="localhost"></a>

### ローカル環境

#### ステップ ⓵ - 事前準備

| カテゴリ | 必要なもの |
| --- | --- |
| **キー類** | • Agora [App ID](https://docs.agora.io/en/video-calling/get-started/manage-agora-account?platform=web#create-an-agora-project) と [App Certificate](https://docs.agora.io/en/video-calling/get-started/manage-agora-account?platform=web#create-an-agora-project)（毎月無料分あり）<br>• [OpenAI](https://openai.com/index/openai-api/) API キー（OpenAI 互換の任意の LLM）<br>• [Deepgram](https://deepgram.com/) ASR（登録で無料クレジット）<br>• [ElevenLabs](https://elevenlabs.io/) TTS（登録で無料クレジット） |
| **インストール** | • [Docker](https://www.docker.com/) / [Docker Compose](https://docs.docker.com/compose/)<br>• [Node.js (LTS) v18](https://nodejs.org/en) |
| **最小システム要件** | • CPU 2 コア以上<br>• RAM 4 GB 以上 |

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<!-- > [!NOTE]
> **macOS：Apple Silicon での Docker 設定**
>
> Docker 設定で「Use Rosetta for x86/amd64 emulation」のチェックを外してください。ARM 端末ではビルドが遅くなる場合がありますが、x64 サーバーにデプロイしたあとは通常どおり動作します。 -->

#### ステップ ⓶ - VM 内でサンプルをビルド

##### 1. リポジトリをクローンし、`ai_agents` に移動して `.env.example` から `.env` を作成

```bash
cd ai_agents
cp ./.env.example ./.env
```

##### 2. `.env` に Agora App ID と App Certificate を設定

```bash
AGORA_APP_ID=
AGORA_APP_CERTIFICATE=

# デフォルトのボイスアシスタント例を実行
# Deepgram（音声認識に必須）
DEEPGRAM_API_KEY=your_deepgram_api_key_here

# OpenAI（言語モデルに必須）
OPENAI_API_KEY=your_openai_api_key_here
OPENAI_MODEL=gpt-4o

# ElevenLabs（音声合成に必須）
ELEVENLABS_TTS_KEY=your_elevenlabs_api_key_here
```

##### 3. 開発用コンテナを起動

```bash
docker compose up -d
```

##### 4. コンテナに入る

```bash
docker exec -it ten_agent_dev bash
```

##### 5. デフォルトサンプルでエージェントをビルド（約 5〜8 分）

`agents/examples` ディレクトリには他のサンプルもあります。
以下のいずれかで開始できます：

```bash
# チェーン型ボイスアシスタント
cd agents/examples/voice-assistant

# リアルタイムの音声対音声アシスタント
cd agents/examples/voice-assistant-realtime
```

##### 6. Web サーバーを起動

ローカルコードを変更した場合は `task build` を実行してください。TypeScript や Go などのコンパイル言語では必須、Python では不要です。

```bash
task install
task run
```

##### 7. エージェントにアクセス

サンプルが起動すると次の UI を利用できます。

<table>
  <tr>
    <td align="center">
      <b>localhost:49483</b>
      <img src="https://github.com/user-attachments/assets/191a7c0a-d8e6-48f9-866f-6a70c58f0118" alt="スクリーンショット 1" /><br/>
    </td>
    <td align="center">
      <b>localhost:3000</b>
      <img src="https://github.com/user-attachments/assets/13e482b6-d907-4449-a779-9454bb24c0b1" alt="スクリーンショット 2" /><br/>
    </td>
  </tr>
</table>

- TMAN Designer: <http://localhost:49483>
- エージェント事例 UI: <http://localhost:3000>

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

#### ステップ ⓷ - サンプルをカスタマイズ

1. [localhost:49483](http://localhost:49483) を開く。
2. STT・LLM・TTS 拡張を右クリック。
3. プロパティで対応する API キーを入力。
4. 変更を保存すると [localhost:3000](http://localhost:3000) で更新内容を確認できます。

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

<a name="codespaces"></a>

### Codespaces

GitHub はリポジトリごとに無料の Codespaces を提供しています。Docker を使わずにエージェント事例を実行でき、通常ローカル環境よりも起動が速くなります。

[codespaces-shield]: <https://github.com/codespaces/badge.svg>
[![][codespaces-shield]](https://codespaces.new/ten-framework/ten-agent)

詳細は[こちらのガイド](https://theten.ai/docs/ten_agent/setup_development_env/setting_up_development_inside_codespace)をご覧ください。

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

<a name="agent-examples-self-hosting"></a>

## エージェント事例のセルフホスティング

<a name="deploying-with-docker"></a>

### Docker でデプロイ

TMAN Designer でカスタマイズするか `property.json` を編集したら、本番用の Docker イメージを作成してサービスをデプロイしましょう。

##### Docker イメージとして公開

**注意**: 以下のコマンドは Docker コンテナの外で実行してください。

###### イメージをビルド

```bash
cd ai_agents
docker build -f agents/examples/<example-name>/Dockerfile -t example-app .
```

###### 実行

```bash
docker run --rm -it --env-file .env -p 3000:3000 example-app
```

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<a name="deploying-with-other-cloud-services"></a>

### その他のクラウドサービスへデプロイ

[TEN を Vercel](https://vercel.com) や [Netlify](https://www.netlify.com) などでホストする場合、バックエンドとフロントエンドを分けて配置できます。

1. Docker 対応の任意のプラットフォーム（Docker が動く VM、Fly.io、Render、ECS、Cloud Run など）で TEN バックエンドを実行。用意されたサンプルイメージをそのまま使い、ポート `8080` を公開します。
2. フロントエンドのみ Vercel / Netlify にデプロイします。プロジェクトルートを `ai_agents/agents/examples/<example>/frontend` に設定し、`pnpm install`（または `bun install`）→ `pnpm build`（または `bun run build`）を実行し、デフォルトの `.next` 出力を保持します。
3. ホスティング側の環境変数で `AGENT_SERVER_URL` をバックエンド URL に設定し、必要な `NEXT_PUBLIC_*` キー（ブラウザで使う Agora 資格情報など）を追加します。
4. CORS を開放する、または内蔵のプロキシミドルウェアを使うなどして、フロントエンドのオリジンからバックエンドへのリクエストを許可します。

この構成では、バックエンドがワーカー処理を担い、ホストしたフロントエンドは API リクエストを転送するだけで済みます。

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

<a name="stay-tuned"></a>

## 最新情報

新しいリリースやアップデートを即座に受け取れます。あなたのサポートが TEN を成長させます！

<br>

![Image](https://github.com/user-attachments/assets/72c6cc46-a2a2-484d-82a9-f3079269c815)

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

<a name="ten-ecosystem"></a>

## TEN エコシステム

<br>

| プロジェクト | プレビュー |
| ------- | ------- |
| [**️TEN Framework**][ten-framework-link]<br>会話型 AI エージェント向けオープンソースフレームワーク。<br><br>![][ten-framework-shield] | ![][ten-framework-banner] |
| [**TEN VAD**][ten-vad-link]<br>低遅延・軽量・高性能なストリーミング音声活動検出。<br><br>![][ten-vad-shield] | ![][ten-vad-banner] |
| [**️TEN Turn Detection**][ten-turn-detection-link]<br>全二重会話を可能にするターン検出。<br><br>![][ten-turn-detection-shield] | ![][ten-turn-detection-banner] |
| [**TEN Agent Examples**][ten-agent-link]<br>TEN を使ったユースケース集。<br><br> | ![][ten-agent-banner] |
| [**TEN Portal**][ten-portal-link]<br>公式サイト。ドキュメントとブログを掲載。<br><br>![][ten-portal-shield] | ![][ten-portal-banner] |

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

<a name="questions"></a>

## 質問

TEN Framework は AI 駆動の Q&A プラットフォームにも掲載されています。マルチリンガルでの検索が可能で、初期設定から高度な実装までサポートします。

| サービス | リンク |
| ------- | ---- |
| DeepWiki | [![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/TEN-framework/TEN-framework) |
| ReadmeX | [![ReadmeX](https://raw.githubusercontent.com/CodePhiliaX/resource-trusteeship/main/readmex.svg)](https://readmex.com/TEN-framework/ten-framework) |

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<a name="contributing"></a>

## コントリビュート

バグ修正、機能追加、ドキュメント改善、アイデア共有など、あらゆる OSS での協力を歓迎します。GitHub の Issues や Projects をチェックして活躍の場を見つけ、スキルを発揮してください。一緒に TEN をより良いものにしましょう！

<br>

> [!TIP]
>
> **すべてのコントリビューションに感謝します** 🙏
>
> コードでもドキュメントでも、どんな貢献も力になります。TEN Agent プロジェクトを SNS で紹介し、コミュニティを盛り上げましょう。
>
> メンテナー [@elliotchen200](https://x.com/elliotchen200)（𝕏）や [@cyfyifanchen](https://github.com/cyfyifanchen)（GitHub）に連絡すると、最新情報や議論、コラボの機会を得られます。

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<a name="code-contributors"></a>

### コードコントリビューター

[![TEN](https://contrib.rocks/image?repo=TEN-framework/ten-agent)](https://github.com/TEN-framework/ten-agent/graphs/contributors)

<a name="contribution-guidelines"></a>

### 貢献ガイドライン

いつでも歓迎です！まずは[貢献ガイドライン](./code-of-conduct/contributing.md)をご確認ください。

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<a name="license"></a>

### ライセンス

1. 下記のディレクトリを除き、TEN Framework 全体は追加条件付きの Apache License 2.0 で配布されています。プロジェクトルートの [LICENSE](./../LICENSE) を参照してください。
2. `packages` 配下のコンポーネントも Apache License 2.0 で提供されます。各パッケージの `LICENSE` ファイルをご確認ください。
3. TEN Framework が利用するサードパーティライブラリは [third_party](./../third_party/) ディレクトリで一覧化されています。

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
