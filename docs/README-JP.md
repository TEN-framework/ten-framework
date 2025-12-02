<div align="center"> <a name="readme-top"></a>

![Image][ten-framework-banner]

[![TEN Releases][ten-releases-badge]][ten-releases]
[![Coverage Status][coverage-badge]][coverage]
[![][release-date-badge]][ten-releases]
[![Discussion posts][discussion-badge]][discussions]
[![Commits][commits-badge]][commit-activity]
[![Issues closed][issues-closed-badge]][issues-closed]
[![][contributors-badge]][contributors]
[![GitHub license][license-badge]][license]
[![Ask DeepWiki][deepwiki-badge]][deepwiki]
[![ReadmeX][readmex-badge]][readmex]

[公式サイト][official-site]
•
[ドキュメント][documentation]
•
[ブログ][blog]

[![README（英語）][lang-en-badge]][lang-en-readme]
[![README（簡体字中国語）][lang-zh-badge]][lang-zh-readme]
[![README（日本語）][lang-jp-badge]][lang-jp-readme]
[![README（韓国語）][lang-kr-badge]][lang-kr-readme]
[![README（スペイン語）][lang-es-badge]][lang-es-readme]
[![README（フランス語）][lang-fr-badge]][lang-fr-readme]
[![README（イタリア語）][lang-it-badge]][lang-it-readme]

[![TEN-framework%2Ften_framework | Trendshift][trendshift-badge]][trendshift]

</div>

<br>

<details open>
  <summary><kbd>目次</kbd></summary>

  <br>

- [TEN へようこそ][welcome-to-ten]
- [エージェント事例][agent-examples-section]
- [エージェント事例のクイックスタート][quick-start]
  - [ローカル環境][localhost-section]
  - [Codespaces][codespaces-section]
- [エージェント事例のセルフホスティング][agent-examples-self-hosting]
  - [Docker でデプロイ][deploying-with-docker]
  - [その他のクラウドサービスへデプロイ][deploying-with-other-cloud-services]
- [最新情報][stay-tuned]
- [TEN エコシステム][ten-ecosystem-anchor]
- [質問][questions]
- [コントリビュート][contributing]
  - [コードコントリビューター][code-contributors]
  - [貢献ガイドライン][contribution-guidelines]
  - [ライセンス][license-section]

<br/>

</details>

<a name="welcome-to-ten"></a>

## TEN へようこそ

TEN は音声会話型 AI エージェント向けのオープンソースフレームワークです。

[TEN エコシステム][ten-ecosystem-anchor] には [TEN Framework][ten-framework-link]、[エージェント事例][ten-agent-example-link]、[VAD][ten-vad-link]、[Turn Detection][ten-turn-detection-link]、[Portal][ten-portal-link] が含まれます。

<br>

| コミュニティ | 目的 |
| ---------------- | ------- |
| [![Follow on X][follow-on-x-badge]][follow-on-x] | X で TEN Framework をフォローして最新情報をチェック |
| [![Discord TEN Community][discord-badge]][discord-invite] | Discord コミュニティに参加し、開発者同士で交流 |
| [![Follow on LinkedIn][linkedin-badge]][linkedin] | LinkedIn で TEN Framework をフォローし、ニュースを受け取る |
| [![Hugging Face Space][hugging-face-badge]][hugging-face] | Hugging Face コミュニティでスペースやモデルを探索 |
| [![WeChat][wechat-badge]][wechat-discussion] | 中国語コミュニティ向けの WeChat グループに参加 |

<br>

<a name="agent-examples"></a>

## エージェント事例

<br>

![Image][voice-assistant-image]

<strong>多目的ボイスアシスタント</strong> — 低レイテンシ・高品質のリアルタイムアシスタント。[メモリ][memory-example]、[VAD][voice-assistant-vad-example]、[ターン検出][voice-assistant-turn-detection-example] などの拡張機能を追加できます。

詳細は [サンプルコード][voice-assistant-example] を参照してください。

<br>

![divider][divider-light]
![divider][divider-dark]

<br>

![Image][lip-sync-image]

<strong>リップシンク対応アバター</strong> — 複数のアバタープロバイダーに対応。デモでは Live2D のリップシンクを備えたアニメキャラクター Kei を紹介し、今後 Trulience、HeyGen、Tavus のリアルアバターにも対応予定です。

[Live2D 用サンプルコード][voice-assistant-live2d-example]

<br>

![divider][divider-light]
![divider][divider-dark]

<br>

![Image][speech-diarization-image]

<strong>話者分離（Diarization）</strong> — 話者をリアルタイムで検出・ラベル付けします。ゲーム「Who Likes What」でインタラクティブな活用例を紹介しています。

[サンプルコード][speechmatics-diarization-example]

<br>

![divider][divider-light]
![divider][divider-dark]

<br>

![Image][sip-call-image]

<strong>SIP 通話</strong> — TEN で電話を実現する SIP 拡張機能です。

[サンプルコード][voice-assistant-sip-example]

<br>

![divider][divider-light]
![divider][divider-dark]

<br>

![Image][transcription-image]

<strong>文字起こし</strong> — 音声をテキストへ変換するトランスクリプションツール。

[サンプルコード][transcription-example]

<br>

![divider][divider-light]
![divider][divider-dark]

<br>

![Image][esp32-image]

<strong>ESP32-S3 Korvo V3</strong> — Espressif ESP32-S3 Korvo V3 開発ボード上で TEN Agent のサンプルを動作させ、LLM ベースのコミュニケーションをハードウェアに組み込みます。

[統合ガイド][esp32-guide] を参照してください。

<br>
<div align="right">

[![][back-to-top]][readme-top]

</div>

<a name="quick-start-with-agent-examples"></a>

## エージェント事例のクイックスタート

<a name="localhost"></a>

### ローカル環境

#### ステップ ⓵ - 事前準備

| カテゴリ | 必要なもの |
| --- | --- |
| **キー類** | • Agora [App ID][agora-app-certificate] と [App Certificate][agora-app-certificate]（毎月無料分あり）<br>• [OpenAI][openai-api] API キー（OpenAI 互換の任意の LLM）<br>• [Deepgram][deepgram] ASR（登録で無料クレジット）<br>• [ElevenLabs][elevenlabs] TTS（登録で無料クレジット） |
| **インストール** | • [Docker][docker] / [Docker Compose][docker-compose]<br>• [Node.js (LTS) v18][nodejs] |
| **最小システム要件** | • CPU 2 コア以上<br>• RAM 4 GB 以上 |

<br>

![divider][divider-light]
![divider][divider-dark]

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

![divider][divider-light]
![divider][divider-dark]

#### ステップ ⓷ - サンプルをカスタマイズ

1. [localhost:49483][localhost-49483] を開く。
2. STT・LLM・TTS 拡張を右クリック。
3. プロパティで対応する API キーを入力。
4. 変更を保存すると [localhost:3000][localhost-3000] で更新内容を確認できます。

<br>

![divider][divider-light]
![divider][divider-dark]

<br>

#### Docker なしで TEN Manager からトランスクリプションアプリを実行 (Beta)

TEN には、Docker を使わずに TEN Manager から実行できるトランスクリプションアプリも用意されています。

詳しくは[クイックスタートガイド][quick-start-guide-ten-manager]をご覧ください。

<br>

![divider][divider-light]
![divider][divider-dark]

<br>

<a name="codespaces"></a>

### Codespaces

GitHub はリポジトリごとに無料の Codespaces を提供しています。Docker を使わずにエージェント事例を実行でき、通常ローカル環境よりも起動が速くなります。

[codespaces-shield]: <https://github.com/codespaces/badge.svg>
[![][codespaces-shield]][codespaces-new]

詳細は[こちらのガイド][codespaces-guide]をご覧ください。

<div align="right">

[![][back-to-top]][readme-top]

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

![divider][divider-light]
![divider][divider-dark]

<a name="deploying-with-other-cloud-services"></a>

### その他のクラウドサービスへデプロイ

[TEN を Vercel][vercel] や [Netlify][netlify] などでホストする場合、バックエンドとフロントエンドを分けて配置できます。

1. Docker 対応の任意のプラットフォーム（Docker が動く VM、Fly.io、Render、ECS、Cloud Run など）で TEN バックエンドを実行。用意されたサンプルイメージをそのまま使い、ポート `8080` を公開します。
2. フロントエンドのみ Vercel / Netlify にデプロイします。プロジェクトルートを `ai_agents/agents/examples/<example>/frontend` に設定し、`pnpm install`（または `bun install`）→ `pnpm build`（または `bun run build`）を実行し、デフォルトの `.next` 出力を保持します。
3. ホスティング側の環境変数で `AGENT_SERVER_URL` をバックエンド URL に設定し、必要な `NEXT_PUBLIC_*` キー（ブラウザで使う Agora 資格情報など）を追加します。
4. CORS を開放する、または内蔵のプロキシミドルウェアを使うなどして、フロントエンドのオリジンからバックエンドへのリクエストを許可します。

この構成では、バックエンドがワーカー処理を担い、ホストしたフロントエンドは API リクエストを転送するだけで済みます。

<div align="right">

[![][back-to-top]][readme-top]

</div>

<br>

<a name="stay-tuned"></a>

## 最新情報

新しいリリースやアップデートを即座に受け取れます。あなたのサポートが TEN を成長させます！

<br>

![Image][stay-tuned-image]

<br>
<div align="right">

[![][back-to-top]][readme-top]

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
| [**TEN Agent Examples**][ten-agent-example-link]<br>TEN を使ったユースケース集。<br><br> | ![][ten-agent-example-banner] |
| [**TEN Portal**][ten-portal-link]<br>公式サイト。ドキュメントとブログを掲載。<br><br>![][ten-portal-shield] | ![][ten-portal-banner] |

<br>
<div align="right">

[![][back-to-top]][readme-top]

</div>

<br>

<a name="questions"></a>

## 質問

TEN Framework は AI 駆動の Q&A プラットフォームにも掲載されています。マルチリンガルでの検索が可能で、初期設定から高度な実装までサポートします。

| サービス | リンク |
| ------- | ---- |
| DeepWiki | [![Ask DeepWiki][deepwiki-badge]][deepwiki] |
| ReadmeX | [![ReadmeX][readmex-badge]][readmex] |

<br>
<div align="right">

[![][back-to-top]][readme-top]

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
> メンテナー [@elliotchen200][elliotchen200-x]（𝕏）や [@cyfyifanchen][cyfyifanchen-github]（GitHub）に連絡すると、最新情報や議論、コラボの機会を得られます。

<br>

![divider][divider-light]
![divider][divider-dark]

<a name="code-contributors"></a>

### コードコントリビューター

[![TEN][contributors-image]][contributors]

<a name="contribution-guidelines"></a>

### 貢献ガイドライン

いつでも歓迎です！まずは[貢献ガイドライン][contribution-guidelines-doc]をご確認ください。

<br>

![divider][divider-light]
![divider][divider-dark]

<a name="license"></a>

### ライセンス

1. 下記のディレクトリを除き、TEN Framework 全体は追加条件付きの Apache License 2.0 で配布されています。プロジェクトルートの [LICENSE][license-file] を参照してください。
2. `packages` 配下のコンポーネントも Apache License 2.0 で提供されます。各パッケージの `LICENSE` ファイルをご確認ください。
3. TEN Framework が利用するサードパーティライブラリは [third_party][third-party-folder] ディレクトリで一覧化されています。

<div align="right">

[![][back-to-top]][readme-top]

</div>

[back-to-top]: https://img.shields.io/badge/-Back_to_top-gray?style=flat-square
[readme-top]: #readme-top

<!-- Navigation -->
[welcome-to-ten]: #welcome-to-ten
[agent-examples-section]: #agent-examples
[quick-start]: #quick-start-with-agent-examples
[localhost-section]: #localhost
[codespaces-section]: #codespaces
[agent-examples-self-hosting]: #agent-examples-self-hosting
[deploying-with-docker]: #deploying-with-docker
[deploying-with-other-cloud-services]: #deploying-with-other-cloud-services
[stay-tuned]: #stay-tuned
[ten-ecosystem-anchor]: #ten-ecosystem
[questions]: #questions
[contributing]: #contributing
[code-contributors]: #code-contributors
[contribution-guidelines]: #contribution-guidelines
[license-section]: #license

<!-- Header badges -->
[discussion-badge]: https://img.shields.io/github/discussions/TEN-framework/ten_framework?labelColor=gray&color=%20%23f79009
[discussions]: https://github.com/TEN-framework/ten-framework/discussions/
[ten-releases-badge]: https://img.shields.io/github/v/release/ten-framework/ten-framework?color=369eff&labelColor=gray&logo=github&style=flat-square
[ten-releases]: https://github.com/TEN-framework/ten-framework/releases
[coverage-badge]: https://coveralls.io/repos/github/TEN-framework/ten-framework/badge.svg?branch=main
[coverage]: https://coveralls.io/github/TEN-framework/ten-framework?branch=main
[release-date-badge]: https://img.shields.io/github/release-date/ten-framework/ten-framework?labelColor=gray&style=flat-square
[commits-badge]: https://img.shields.io/github/commit-activity/m/TEN-framework/ten-framework?labelColor=gray&color=pink
[commit-activity]: https://github.com/TEN-framework/ten-framework/graphs/commit-activity
[issues-closed-badge]: https://img.shields.io/github/issues-search?query=repo%3ATEN-framework%2Ften-framework%20is%3Aclosed&label=issues%20closed&labelColor=gray&color=green
[issues-closed]: https://github.com/TEN-framework/ten-framework/issues
[contributors-badge]: https://img.shields.io/github/contributors/ten-framework/ten-framework?color=c4f042&labelColor=gray&style=flat-square
[contributors]: https://github.com/TEN-framework/ten-framework/graphs/contributors
[license-badge]: https://img.shields.io/badge/License-Apache_2.0_with_certain_conditions-blue.svg?labelColor=%20%23155EEF&color=%20%23528bff
[license]: https://github.com/TEN-framework/ten-framework/blob/main/LICENSE
[deepwiki-badge]: https://deepwiki.com/badge.svg
[deepwiki]: https://deepwiki.com/TEN-framework/TEN-framework
[readmex-badge]: https://raw.githubusercontent.com/CodePhiliaX/resource-trusteeship/main/readmex.svg
[readmex]: https://readmex.com/TEN-framework/ten-framework
[trendshift-badge]: https://trendshift.io/api/badge/repositories/11978
[trendshift]: https://trendshift.io/repositories/11978

<!-- Localized READMEs -->
[lang-en-badge]: https://img.shields.io/badge/English-lightgrey
[lang-en-readme]: https://github.com/TEN-framework/ten-framework/blob/main/README.md
[lang-zh-badge]: https://img.shields.io/badge/简体中文-lightgrey
[lang-zh-readme]: https://github.com/TEN-framework/ten-framework/blob/main/docs/README-CN.md
[lang-jp-badge]: https://img.shields.io/badge/日本語-lightgrey
[lang-jp-readme]: https://github.com/TEN-framework/ten-framework/blob/main/docs/README-JP.md
[lang-kr-badge]: https://img.shields.io/badge/한국어-lightgrey
[lang-kr-readme]: https://github.com/TEN-framework/ten-framework/blob/main/docs/README-KR.md
[lang-es-badge]: https://img.shields.io/badge/Español-lightgrey
[lang-es-readme]: https://github.com/TEN-framework/ten-framework/blob/main/docs/README-ES.md
[lang-fr-badge]: https://img.shields.io/badge/Français-lightgrey
[lang-fr-readme]: https://github.com/TEN-framework/ten-framework/blob/main/docs/README-FR.md
[lang-it-badge]: https://img.shields.io/badge/Italiano-lightgrey
[lang-it-readme]: https://github.com/TEN-framework/ten-framework/blob/main/docs/README-IT.md

<!-- Primary sites -->
[official-site]: https://theten.ai
[documentation]: https://theten.ai/docs
[blog]: https://theten.ai/blog

<!-- Welcome -->
[ten-framework]: https://github.com/ten-framework/ten-framework
[agent-examples-repo]: https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples
[ten-vad]: https://github.com/ten-framework/ten-vad
[ten-turn-detection]: https://github.com/ten-framework/ten-turn-detection
[ten-portal]: https://github.com/ten-framework/portal

<!-- Community -->
[follow-on-x-badge]: https://img.shields.io/twitter/follow/TenFramework?logo=X&color=%20%23f5f5f5
[follow-on-x]: https://twitter.com/intent/follow?screen_name=TenFramework
[discord-badge]: https://img.shields.io/badge/Discord-Join%20TEN%20Community-5865F2?style=flat&logo=discord&logoColor=white
[discord-invite]: https://discord.gg/VnPftUzAMJ
[linkedin-badge]: https://custom-icon-badges.demolab.com/badge/LinkedIn-TEN_Framework-0A66C2?logo=linkedin-white&logoColor=fff
[linkedin]: https://www.linkedin.com/company/ten-framework
[hugging-face-badge]: https://img.shields.io/badge/Hugging%20Face-TEN%20Framework-yellow?style=flat&logo=huggingface
[hugging-face]: https://huggingface.co/TEN-framework
[wechat-badge]: https://img.shields.io/badge/TEN_Framework-WeChat_Group-%2307C160?logo=wechat&labelColor=darkgreen&color=gray
[wechat-discussion]: https://github.com/TEN-framework/ten-agent/discussions/170

<!-- Agent examples -->
[voice-assistant-image]: https://github.com/user-attachments/assets/dce3db80-fb48-4e2a-8ac7-33f50bcffa32
[websocket-example]: ../ai_agents/agents/examples/websocket-example
[memory-example]: ../ai_agents/agents/examples/voice-assistant-with-memU
[voice-assistant-vad-example]: ../ai_agents/agents/examples/voice-assistant-with-ten-vad
[voice-assistant-turn-detection-example]: ../ai_agents/agents/examples/voice-assistant-with-turn-detection
[voice-assistant-example]: ../ai_agents/agents/examples/voice-assistant
[divider-light]: https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd#gh-light-mode-only
[divider-dark]: https://github.com/user-attachments/assets/d57fad08-4f49-4a1c-bdfc-f659a5d86150#gh-dark-mode-only
[lip-sync-image]: https://github.com/user-attachments/assets/51ab1504-b67c-49d4-8a7a-5582d9b254da
[voice-assistant-live2d-example]: ../ai_agents/agents/examples/voice-assistant-live2d
[speech-diarization-image]: https://github.com/user-attachments/assets/f94b21b8-9dda-4efc-9274-b028cc01296a
[speechmatics-diarization-example]: ../ai_agents/agents/examples/speechmatics-diarization
[sip-call-image]: https://github.com/user-attachments/assets/6ed5b04d-945a-4a30-a1cc-f8014b602b38
[voice-assistant-sip-example]: ../ai_agents/agents/examples/voice-assistant-sip-twilio
[transcription-image]: https://github.com/user-attachments/assets/d793bc6c-c8de-4996-bd85-9ce88c69dd8d
[transcription-example]: ../ai_agents/agents/examples/transcription
[esp32-image]: https://github.com/user-attachments/assets/3d60f1ff-0f82-4fe7-b5c2-ac03d284f60c
[esp32-guide]: ../ai_agents/esp32-client

<!-- Quick start -->
[agora-app-id]: https://docs.agora.io/en/video-calling/get-started/manage-agora-account?platform=web#create-an-agora-project
[agora-app-certificate]: https://docs.agora.io/en/video-calling/get-started/manage-agora-account?platform=web#create-an-agora-project
[openai-api]: https://openai.com/index/openai-api/
[deepgram]: https://deepgram.com/
[elevenlabs]: https://elevenlabs.io/
[docker]: https://www.docker.com/
[docker-compose]: https://docs.docker.com/compose/
[nodejs]: https://nodejs.org/en
[quick-start-guide-ten-manager]: https://theten.ai/docs/ten_framework/getting-started/quick-start
[localhost-49483-image]: https://github.com/user-attachments/assets/191a7c0a-d8e6-48f9-866f-6a70c58f0118
[localhost-3000-image]: https://github.com/user-attachments/assets/13e482b6-d907-4449-a779-9454bb24c0b1
[localhost-49483]: http://localhost:49483
[localhost-3000]: http://localhost:3000

<!-- Codespaces -->
[codespaces-shield]: https://github.com/codespaces/badge.svg
[codespaces-new]: https://codespaces.new/ten-framework/ten-agent
[codespaces-guide]: https://theten.ai/docs/ten_agent_examples/setup_development_env/setting_up_development_inside_codespace

<!-- Deployment -->
[vercel]: https://vercel.com
[netlify]: https://www.netlify.com

<!-- Stay tuned -->
[stay-tuned-image]: https://github.com/user-attachments/assets/72c6cc46-a2a2-484d-82a9-f3079269c815

<!-- TEN ecosystem -->
[ten-framework-shield]: https://img.shields.io/github/stars/ten-framework/ten-framework?color=ffcb47&labelColor=gray&style=flat-square&logo=github
[ten-framework-banner]: https://github.com/user-attachments/assets/799584b2-61ff-4255-bdd1-2548d0fdba52
[ten-framework-link]: https://github.com/ten-framework/ten-framework

[ten-vad-link]: https://github.com/ten-framework/ten-vad
[ten-vad-shield]: https://img.shields.io/github/stars/ten-framework/ten-vad?color=ffcb47&labelColor=gray&style=flat-square&logo=github
[ten-vad-banner]: https://github.com/user-attachments/assets/e504135e-67fd-4fa1-b0e4-d495358d8aa5

[ten-turn-detection-link]: https://github.com/ten-framework/ten-turn-detection
[ten-turn-detection-shield]: https://img.shields.io/github/stars/ten-framework/ten-turn-detection?color=ffcb47&labelColor=gray&style=flat-square&logo=github
[ten-turn-detection-banner]: https://github.com/user-attachments/assets/c72d82cc-3667-496c-8bd6-3d194a91c452

[ten-agent-example-link]: https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples
[ten-agent-example-banner]: https://github.com/user-attachments/assets/7f735633-c7f6-4432-b6b4-d2a2977ca588

[ten-portal-link]: https://github.com/ten-framework/portal
[ten-portal-shield]: https://img.shields.io/github/stars/ten-framework/portal?color=ffcb47&labelColor=gray&style=flat-square&logo=github
[ten-portal-banner]: https://github.com/user-attachments/assets/f56c75b9-722c-4156-902d-ae98ce2b3b5e

<!-- Contributing -->
[elliotchen200-x]: https://x.com/elliotchen200
[cyfyifanchen-github]: https://github.com/cyfyifanchen
[contributors-image]: https://contrib.rocks/image?repo=TEN-framework/ten-framework
[contribution-guidelines-doc]: ./code-of-conduct/contributing.md
[license-file]: ../LICENSE
[third-party-folder]: ../third_party/
