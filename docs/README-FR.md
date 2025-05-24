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

- [👋 Bienvenue sur TEN](#-bienvenue-sur-ten)
- [🎨 TMAN Designer](#-tman-designer)
- [🤖 TEN Agent](#-ten-agent)
  - [1️⃣ Real-time Avatar](#1️⃣-real-time-avatar)
  - [2️⃣ Real-time voice with MCP servers](#2️⃣-real-time-voice-with-mcp-servers)
  - [3️⃣ Real-time communication with hardware](#3️⃣-real-time-communication-with-hardware)
  - [4️⃣ Real-time vision and real-time screenshare detection](#4️⃣-real-time-vision-and-real-time-screenshare-detection)
  - [5️⃣ TEN with other LLM platforms](#5️⃣-ten-with-other-llm-platforms)
  - [6️⃣ StoryTeller - TEN image generation](#6️⃣-storyteller---ten-image-generation)
- [🛝 Démarrage Rapide avec TEN Agent Playground](#-ten-agent-playground)
  - [🅰️ Exécuter le Playground en localhost](#🅰️-exécuter-le-playground-en-localhost)
  - [🅱️ Exécuter le Playground dans Codespace (sans docker)](#🅱️-exécuter-le-playground-dans-codespaceno-docker)
- [🛳️ Auto-hébergement de TEN Agent](#️-ten-agent-self-hosting)
  - [🅰️ 🐳 Déploiement avec Docker](#️--deploying-with-docker)
  - [🅱️ Déploiement avec d'autres services cloud](#️-deploying-with-other-cloud-services)
- [🏗️ TEN Agent Architecture](#️-ten-agent-architecture)
- [🌏 Écosystème TEN](#-ten-framework-ecosystem)
- [❓ Poser des Questions](#-ask-questions)
- [🥰 Contribuer](#-contributing)
  - [Contributeurs de Code](#code-contributors)
  - [Guide de Contribution](#contribution-guidelines)
  - [Licence](#license)

<br/>

</details>

## �� Bienvenue sur TEN

TEN est une collection de projets open-source pour construire des agents vocaux conversationnels multimodaux en temps réel. Il comprend [TEN Framework](https://github.com/ten-framework/ten-framework), [TEN Turn Detection](https://github.com/ten-framework/ten-turn-detection), TEN Agent, TMAN Designer et [TEN Portal](https://github.com/ten-framework/portal), tous entièrement open-source. [TEN VAD](https://github.com/ten-framework/ten-vad) n'est pas encore entièrement open-source, mais il est ouvert à l'utilisation publique.

<br>

| Canal Communautaire | Objectif |
| ---------------- | ------- |
| [![Follow on X](https://img.shields.io/twitter/follow/TenFramework?logo=X&color=%20%23f5f5f5)](https://twitter.com/intent/follow?screen_name=TenFramework) | Suivez TEN Framework sur X pour les mises à jour et les annonces |
| [![Follow on LinkedIn](https://custom-icon-badges.demolab.com/badge/LinkedIn-TEN_Framework-0A66C2?logo=linkedin-white&logoColor=fff)](https://www.linkedin.com/company/ten-framework) | Suivez TEN Framework sur LinkedIn pour les mises à jour et les annonces |
| [![Discord TEN Community](https://dcbadge.vercel.app/api/server/VnPftUzAMJ?&style=flat&theme=light&color=lightgray)](https://discord.gg/VnPftUzAMJ) | Rejoignez notre communauté Discord pour échanger avec les développeurs |
| [![Hugging Face Space](https://img.shields.io/badge/Hugging%20Face-TEN%20Framework-yellow?style=flat&logo=huggingface)](https://huggingface.co/TEN-framework) | Rejoignez notre communauté Hugging Face pour explorer nos espaces et modèles |
| [![WeChat](https://img.shields.io/badge/TEN_Framework-WeChat_Group-%2307C160?logo=wechat&labelColor=darkgreen&color=gray)](https://github.com/TEN-framework/ten-agent/discussions/170) | Rejoignez notre groupe WeChat pour les discussions de la communauté chinoise |

<br>

> \[!IMPORTANT]
>
> **Star TEN Repositories** ⭐️
>
> Recevez des notifications instantanées pour les nouvelles versions et mises à jour. Votre soutien nous aide à faire grandir et améliorer TEN !

<br>

![TEN star us gif](https://github.com/user-attachments/assets/eeebe996-8c14-4bf7-82ae-f1a1f7e30705)

<br>

<details>
  <summary><kbd>Historique des Stars</kbd></summary>
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

TMAN Designer est une option low-code/no-code pour créer des agents vocaux avec une interface utilisateur de workflow facile à utiliser. Il peut charger des applications et des graphes, et inclut un éditeur en ligne, un visualiseur de logs, et bien plus encore.

Consultez [ce blog](https://theten.ai/blog/tman-designer-of-ten-framework) pour plus de détails.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<!-- ## 🧩 Extensions

![Ready-to-use Extensions](https://ten-framework-assets.s3.us-east-1.amazonaws.com/extensions.jpg)

<div align="right">

[![][back-to-top]](#readme-top)

</div> -->

<br>

## ✨ Fonctionnalités

![TEN Agent with Trulience](https://github.com/user-attachments/assets/2f1dfd55-14a3-47ea-ae25-40ad40ceadea)

### 1️⃣ Avatar en Temps Réel

Créez des avatars IA engageants avec TEN Agent en utilisant la collection diversifiée d'options d'avatars gratuits de [Trulience](https://trulience.com). Pour le mettre en marche, vous n'avez besoin que de 2 étapes :

1. Suivez le README pour terminer la configuration et l'exécution du Playground
2. Entrez l'ID de l'avatar et le [token](https://trulience.com/docs#/authentication/jwt-tokens/jwt-tokens?id=use-your-custom-userid) que vous obtenez de [Trulience](https://trulience.com)

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

![TEN Agent with MCP servers](https://github.com/user-attachments/assets/934ba928-a4a3-4662-8624-cebefc88ce05)

### 2️⃣ Voix en temps réel avec les serveurs MCP

TEN Agent s'intègre désormais parfaitement avec les serveurs MCP, étendant ses capacités LLM. Pour commencer :

1. Ouvrez le Module Picker dans le Playground
2. Ajoutez l'outil serveur MCP pour l'intégration LLM
3. Collez une URL de votre serveur MCP dans l'extension
4. Démarrez une conversation en temps réel avec TEN Agent

Cette intégration vous permet de tirer parti des diverses offres de serveurs MCP tout en conservant les puissantes capacités conversationnelles de TEN Agent.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

<https://github.com/user-attachments/assets/78647eef-2d66-44e6-99a8-1918a940fb9f>

### 3️⃣ Communication en temps réel avec le matériel

TEN Agent fonctionne maintenant sur la carte de développement Espressif ESP32-S3 Korvo V3, un excellent moyen d'intégrer la communication en temps réel avec LLM sur le matériel.

Consultez le [guide d'intégration](https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/esp32-client) pour plus de détails.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

![Real-time vision](https://github.com/user-attachments/assets/7be06e38-994e-4f82-8ec6-183d08fe90f1)

### 4️⃣ Vision en temps réel et détection du partage d'écran

Essayez l'API Multimodale Live de Google Gemini avec des capacités de vision en temps réel et de détection du partage d'écran, c'est une extension prête à l'emploi, accompagnée d'outils puissants comme Weather Check et Web Search parfaitement intégrés dans TEN Agent.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

![TEN with other LLM platforms](https://github.com/user-attachments/assets/a3766d50-6a25-4299-b28c-e15772e4201c)

### 5️⃣ TEN avec d'autres plateformes LLM

[TEN Agent + Dify](https://doc.theten.ai/docs/ten_agent/playground/use-cases/voice-assistant/run_dify)

TEN offre un excellent support pour améliorer l'expérience interactive en temps réel sur d'autres plateformes LLM également, consultez la documentation pour plus d'informations.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

![Image](https://github.com/user-attachments/assets/ea1025d4-b22b-4416-ab35-36cd910bc28c)

### 6️⃣ StoryTeller - Génération d'images TEN

Découvrez la génération d'images en temps réel avec StoryTeller, c'est une extension prête à l'emploi, accompagnée d'outils puissants comme Weather Check et Web Search parfaitement intégrés dans TEN.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

## 🛝 Démarrage Rapide avec TEN Agent Playground

#### 🅰️ Exécuter le Playground en localhost

#### Étape ⓵ - Prérequis

| Catégorie | Exigences |
| --- | --- |
| **Clés** | • Agora [App ID](https://docs.agora.io/en/video-calling/get-started/manage-agora-account?platform=web#create-an-agora-project) et [App Certificate](https://docs.agora.io/en/video-calling/get-started/manage-agora-account?platform=web#create-an-agora-project) (minutes gratuites chaque mois) <br>• Clé API [OpenAI](https://openai.com/index/openai-api/) (tout LLM compatible avec OpenAI)<br>• ASR [Deepgram](https://deepgram.com/) (crédits gratuits disponibles à l'inscription)<br>• TTS [Elevenlabs](https://elevenlabs.io/) (crédits gratuits disponibles à l'inscription) |
| **Installation** | • [Docker](https://www.docker.com/) / [Docker Compose](https://docs.docker.com/compose/)<br>• [Node.js(LTS) v18](https://nodejs.org/en) |
| **Configuration Système Minimale** | • CPU >= 2 Cœurs<br>• RAM >= 4 GB |

<br>

> \[!NOTE]
>
> **macOS : Configuration Docker sur Apple Silicon**
>
> Décochez "Use Rosetta for x86/amd64 emulation" dans les paramètres Docker, cela peut entraîner des temps de construction plus lents sur ARM, mais les performances seront normales lors du déploiement sur des serveurs x64.

<br>

#### Étape ⓶ - Construire l'agent dans la VM

##### 1. Clonez le repo, `cd` vers `ai-agents` et créez le fichier `.env` à partir de `.env.example`

```bash
cd ai_agent
cp ./.env.example ./.env
```

##### 2. Configurez l'App ID et l'App Certificate Agora dans `.env`

```bash
AGORA_APP_ID=
AGORA_APP_CERTIFICATE=
```

##### 3. Démarrez les conteneurs de développement de l'agent

```bash
docker compose up -d
```

##### 4. Entrez dans le conteneur

```bash
docker exec -it ten_agent_dev bash
```

##### 5. Construisez l'agent avec le `graph` par défaut ( ~5min - ~8min)

consultez le dossier `/examples` pour plus d'exemples

```bash
# utiliser l'agent par défaut
task use

# ou utiliser l'agent de démonstration
task use AGENT=agents/examples/demo
```

##### 6. Démarrez le serveur web

```bash
task run
```

<br>

#### Étape ⓷ - Personnalisez votre agent avec TMAN Designer

![Customize your agent with TMAN Designer](https://github.com/user-attachments/assets/33f8357b-6762-45eb-8231-c2d83bb77591)

 1. Ouvrez [localhost:49483](http://localhost:49483).
 2. Chargez le graphique correspondant depuis le menu (par exemple, Voice Assistant).
 3. Entrez les clés API et définissez les préférences pour chaque extension.
 4. Ouvrez [localhost:3000](http://localhost:3000) pour voir les changements après avoir sélectionné Voice Assistant.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

### 🅱️ Exécuter le Playground dans Codespace (sans docker)

GitHub offre un Codespace gratuit pour chaque dépôt, vous pouvez exécuter le playground dans Codespace sans utiliser Docker. De plus, la vitesse de Codespace est beaucoup plus rapide que localhost.

[codespaces-shield]: <https://github.com/codespaces/badge.svg>
[![][codespaces-shield]](https://codespaces.new/ten-framework/ten-agent)

Consultez [ce guide](https://theten.ai/docs/ten_agent/setup_development_env/setting_up_development_inside_codespace) pour plus de détails.

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

## 🛳️ Auto-hébergement de TEN Agent

#### 🅰️ 🐳 Déploiement avec Docker

Une fois que vous avez personnalisé votre agent (soit en utilisant le TMAN Manager, le Playground, ou en modifiant directement `property.json`), vous pouvez le déployer en créant une image Docker de release pour votre service.

Lisez le [Guide de Déploiement](https://theten.ai/docs/ten_agent/deploy_ten_agent/deploy_agent_service) pour des informations détaillées sur le déploiement.

<br>

#### 🅱️ Déploiement avec d'autres services cloud

*bientôt disponible*

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

## 🌏 Écosystème TEN

| Projet | Aperçu |
| ------- | ------- |
| [**🏚️ TEN Framework**][ten-framework-link]<br>TEN est un framework open-source pour l'IA conversationnelle multimodale en temps réel.<br><br>![][ten-framework-shield] | ![][ten-framework-banner] |
| [**️🔂 TEN Turn Detection**][ten-turn-detection-link]<br>TEN est pour la communication de dialogue full-duplex.<br><br>![][ten-turn-detection-shield] | ![][ten-turn-detection-banner] |
| [**🔉 TEN VAD**][ten-vad-link]<br>TEN VAD est un détecteur d'activité vocale (VAD) de streaming à faible latence, léger et haute performance.<br><br>![][ten-vad-shield] | ![][ten-vad-banner] |
| [**🎙️ TEN Agent**][ten-agent-link]<br>TEN Agent est une démonstration du Framework TEN.<br><br> | ![][ten-agent-banner] |
| **🎨 TMAN Designer** <br>TMAN Designer est une option low-code/no-code pour créer un agent vocal avec une interface utilisateur de workflow facile à utiliser.<br><br> | ![][tman-designer-banner] |
| [**📒 TEN Portal**][ten-portal-link]<br>Le site officiel du framework TEN, il contient la documentation et le blog.<br><br>![][ten-portal-shield] | ![][ten-portal-banner] |

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

## ❓ Poser des Questions

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/TEN-framework/TEN-framework)

La plupart des questions peuvent être répondues en utilisant DeepWiki, c'est rapide, intuitif à utiliser et prend en charge plusieurs langues.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

## 🥰 Contribuer

Nous accueillons toutes les formes de collaboration open-source ! Que vous corrigiez des bugs, ajoutiez des fonctionnalités, amélioriez la documentation ou partagiez des idées - vos contributions aident à faire progresser les outils d'IA personnalisés. Consultez nos Issues et Projets GitHub pour trouver des moyens de contribuer et montrer vos compétences. Ensemble, nous pouvons construire quelque chose d'incroyable !

<br>

> \[!TIP]
>
> **Bienvenue à tous les types de contributions** 🙏
>
> Rejoignez-nous pour améliorer TEN ! Chaque contribution fait la différence, du code à la documentation. Partagez vos projets TEN Agent sur les réseaux sociaux pour inspirer les autres !
>
> Connectez-vous avec l'un des mainteneurs de TEN [@elliotchen100](https://x.com/elliotchen100) sur 𝕏 ou [@cyfyifanchen](https://github.com/cyfyifanchen) sur GitHub pour les mises à jour du projet, les discussions et les opportunités de collaboration.

<br>

### Contributeurs de Code

[![TEN](https://contrib.rocks/image?repo=TEN-framework/ten-agent)](https://github.com/TEN-framework/ten-agent/graphs/contributors)

### Guide de Contribution

Les contributions sont les bienvenues ! Veuillez d'abord lire les [directives de contribution](./docs/code-of-conduct/contributing.md).

### Licence

1. L'ensemble du framework TEN (à l'exception des dossiers explicitement listés ci-dessous) est publié sous la Licence Apache, Version 2.0, avec des restrictions supplémentaires. Pour plus de détails, veuillez vous référer au fichier [LICENSE](./LICENSE) situé dans le répertoire racine du framework TEN.

2. Les composants dans le répertoire `packages` sont publiés sous la Licence Apache, Version 2.0. Pour plus de détails, veuillez vous référer au fichier `LICENSE` situé dans le répertoire racine de chaque package.

3. Les bibliothèques tierces utilisées par le framework TEN sont listées et décrites en détail. Pour plus d'informations, veuillez vous référer au dossier [third_party](./third_party/).

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
