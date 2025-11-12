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

[Site officiel](https://theten.ai)
•
[Documentation](https://theten.ai/docs/ten_agent/overview)
•
[Blog](https://theten.ai/blog)

<a href="https://github.com/TEN-framework/ten-framework/blob/main/README.md"><img alt="README en anglais" src="https://img.shields.io/badge/English-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-CN.md"><img alt="Guide en chinois simplifié" src="https://img.shields.io/badge/简体中文-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-JP.md"><img alt="README en japonais" src="https://img.shields.io/badge/日本語-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-KR.md"><img alt="README en coréen" src="https://img.shields.io/badge/한국어-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-ES.md"><img alt="README en espagnol" src="https://img.shields.io/badge/Español-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-FR.md"><img alt="README en français" src="https://img.shields.io/badge/Français-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-IT.md"><img alt="README en italien" src="https://img.shields.io/badge/Italiano-lightgrey"></a>

<a href="https://trendshift.io/repositories/11978" target="_blank"><img src="https://trendshift.io/api/badge/repositories/11978" alt="TEN-framework%2Ften_framework | Trendshift" style="width: 250px; height: 55px;" width="250" height="55"/></a>

</div>

<br>

<details open>
  <summary><kbd>Table des matières</kbd></summary>

  <br>

- [Bienvenue chez TEN](#welcome-to-ten)
- [Exemples d’agents](#agent-examples)
- [Démarrage rapide avec les exemples d’agents](#quick-start-with-agent-examples)
  - [En local](#localhost)
  - [Codespaces](#codespaces)
- [Auto-hébergement des exemples d’agents](#agent-examples-self-hosting)
  - [Déployer avec Docker](#deploying-with-docker)
  - [Déployer sur d’autres services cloud](#deploying-with-other-cloud-services)
- [Restez informé·e](#stay-tuned)
- [Écosystème TEN](#ten-ecosystem)
- [Questions](#questions)
- [Contribuer](#contributing)
  - [Contributrices et contributeurs](#code-contributors)
  - [Guide de contribution](#contribution-guidelines)
  - [Licence](#license)

<br/>

</details>

<a name="welcome-to-ten"></a>

## Bienvenue chez TEN

TEN est un framework open source pour créer des agents conversationnels vocaux pilotés par l’IA.

L’[écosystème TEN](#ten-ecosystem) comprend [TEN Framework](https://github.com/ten-framework/ten-framework), les [Exemples d’agents](https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples), [VAD](https://github.com/ten-framework/ten-vad), [Turn Detection](https://github.com/ten-framework/ten-turn-detection) et [Portal](https://github.com/ten-framework/portal).

<br>

| Canal communautaire | Objectif |
| ---------------- | ------- |
| [![Follow on X](https://img.shields.io/twitter/follow/TenFramework?logo=X&color=%20%23f5f5f5)](https://twitter.com/intent/follow?screen_name=TenFramework) | Suivez TEN Framework sur X pour connaître les nouveautés et annonces |
| [![Discord TEN Community](https://img.shields.io/badge/Discord-Join%20TEN%20Community-5865F2?style=flat&logo=discord&logoColor=white)](https://discord.gg/VnPftUzAMJ) | Rejoignez notre communauté Discord pour échanger avec d’autres développeurs |
| [![Follow on LinkedIn](https://custom-icon-badges.demolab.com/badge/LinkedIn-TEN_Framework-0A66C2?logo=linkedin-white&logoColor=fff)](https://www.linkedin.com/company/ten-framework) | Abonnez-vous sur LinkedIn afin de recevoir nos actualités |
| [![Hugging Face Space](https://img.shields.io/badge/Hugging%20Face-TEN%20Framework-yellow?style=flat&logo=huggingface)](https://huggingface.co/TEN-framework) | Découvrez nos espaces et modèles sur Hugging Face |
| [![WeChat](https://img.shields.io/badge/TEN_Framework-WeChat_Group-%2307C160?logo=wechat&labelColor=darkgreen&color=gray)](https://github.com/TEN-framework/ten-agent/discussions/170) | Rejoignez le groupe WeChat pour discuter avec la communauté chinoise |

<br>

<a name="agent-examples"></a>

## Exemples d’agents

<br>

![Image](https://github.com/user-attachments/assets/dce3db80-fb48-4e2a-8ac7-33f50bcffa32)

<strong>Assistant vocal polyvalent</strong> — Assistant temps réel, basse latence et haute qualité, extensible avec des modules de <a href="ai_agents/agents/examples/voice-assistant-with-memU">mémoire</a>, de <a href="ai_agents/agents/examples/voice-assistant-with-ten-vad">VAD</a>, de <a href="ai_agents/agents/examples/voice-assistant-with-turn-detection">détection de tours</a>, etc.

Consultez le <a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples/voice-assistant">code d’exemple</a> pour en savoir plus.

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/aa3f2c9c-c82e-412f-8400-06378ba75794)

<strong>Avatars avec synchronisation labiale</strong> — Compatible avec plusieurs fournisseurs d’avatars. La démo met en scène Kei, un personnage animé avec synchronisation labiale Live2D, et proposera bientôt des avatars réalistes de Trulience, HeyGen et Tavus.

Voir le <a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples/voice-assistant-live2d">code d’exemple Live2D</a>.

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/f94b21b8-9dda-4efc-9274-b028cc01296a)

<strong>Diarisation vocale</strong> — Détection et étiquetage des locuteurs en temps réel. Le jeu "Who Likes What" illustre un cas d’usage interactif.

<a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples/speechmatics-diarization">Code d’exemple</a>

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/6ed5b04d-945a-4a30-a1cc-f8014b602b38)

<strong>Appels SIP</strong> — Extension SIP qui permet d’effectuer des appels téléphoniques propulsés par TEN.

<a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples/voice-assistant-sip-twilio">Code d’exemple</a>

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/d793bc6c-c8de-4996-bd85-9ce88c69dd8d)

<strong>Transcription</strong> — Outil de transcription qui convertit la voix en texte.

<a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples/transcription">Code d’exemple</a>

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/3d60f1ff-0f82-4fe7-b5c2-ac03d284f60c)

<strong>ESP32-S3 Korvo V3</strong> — Fait tourner un exemple TEN Agent sur la carte de développement Espressif ESP32-S3 Korvo V3 pour relier communication LLM et matériel.

Voir le <a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/esp32-client">guide d’intégration</a> pour plus d’informations.

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<a name="quick-start-with-agent-examples"></a>

## Démarrage rapide avec les exemples d’agents

<a name="localhost"></a>

### En local

#### Étape ⓵ - Prérequis

| Catégorie | Exigences |
| --- | --- |
| **Clés** | • Agora [App ID](https://docs.agora.io/en/video-calling/get-started/manage-agora-account?platform=web#create-an-agora-project) et [App Certificate](https://docs.agora.io/en/video-calling/get-started/manage-agora-account?platform=web#create-an-agora-project) (minutes gratuites chaque mois)<br>• Clé API de [OpenAI](https://openai.com/index/openai-api/) (n’importe quel LLM compatible OpenAI)<br>• ASR [Deepgram](https://deepgram.com/) (crédits offerts à l’inscription)<br>• TTS [ElevenLabs](https://elevenlabs.io/) (crédits offerts à l’inscription) |
| **Installation** | • [Docker](https://www.docker.com/) / [Docker Compose](https://docs.docker.com/compose/)<br>• [Node.js (LTS) v18](https://nodejs.org/en) |
| **Configuration minimale** | • CPU ≥ 2 cœurs<br>• RAM ≥ 4 Go |

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<!-- > [!NOTE]
> **macOS : réglages Docker sur Apple Silicon**
>
> Décochez "Use Rosetta for x86/amd64 emulation" dans Docker. Les builds peuvent être plus lents sur ARM mais les performances restent normales sur des serveurs x64. -->

#### Étape ⓶ - Compiler les exemples dans une VM

##### 1. Clonez le dépôt, placez-vous dans `ai_agents` et créez `.env` à partir de `.env.example`

```bash
cd ai_agents
cp ./.env.example ./.env
```

##### 2. Configurez Agora App ID et App Certificate dans `.env`

```bash
AGORA_APP_ID=
AGORA_APP_CERTIFICATE=

# Exécuter l’exemple d’assistant vocal par défaut
# Deepgram (requis pour la transcription)
DEEPGRAM_API_KEY=your_deepgram_api_key_here

# OpenAI (requis pour le modèle de langage)
OPENAI_API_KEY=your_openai_api_key_here
OPENAI_MODEL=gpt-4o

# ElevenLabs (requis pour la synthèse vocale)
ELEVENLABS_TTS_KEY=your_elevenlabs_api_key_here
```

##### 3. Lancez les conteneurs de développement

```bash
docker compose up -d
```

##### 4. Entrez dans le conteneur

```bash
docker exec -it ten_agent_dev bash
```

##### 5. Compilez l’agent avec l’exemple par défaut (~5-8 min)

D’autres exemples sont disponibles dans `agents/examples`.
Commencez par l’une des options suivantes :

```bash
# Assistant vocal chaîné
cd agents/examples/voice-assistant

# Assistant voix-à-voix temps réel
cd agents/examples/voice-assistant-realtime
```

##### 6. Démarrez le serveur web

Exécutez `task build` si vous avez modifié le code. Obligatoire pour les langages compilés (TypeScript, Go, etc.), inutile pour Python.

```bash
task install
task run
```

##### 7. Accédez à l’agent

Une fois l’exemple démarré, ces interfaces sont disponibles :

<table>
  <tr>
    <td align="center">
      <b>localhost:49483</b>
      <img src="https://github.com/user-attachments/assets/191a7c0a-d8e6-48f9-866f-6a70c58f0118" alt="Capture 1" /><br/>
    </td>
    <td align="center">
      <b>localhost:3000</b>
      <img src="https://github.com/user-attachments/assets/13e482b6-d907-4449-a779-9454bb24c0b1" alt="Capture 2" /><br/>
    </td>
  </tr>
</table>

- TMAN Designer : <http://localhost:49483>
- Interface des exemples : <http://localhost:3000>

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

#### Étape ⓷ - Personnaliser votre exemple

1. Ouvrez [localhost:49483](http://localhost:49483).
2. Cliquez droit sur les extensions STT, LLM et TTS.
3. Renseignez les clés API correspondantes.
4. Validez : la mise à jour apparaît sur [localhost:3000](http://localhost:3000).

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

<a name="codespaces"></a>

### Codespaces

GitHub fournit des Codespaces gratuits par dépôt. Vous pouvez exécuter les exemples d’agents sans Docker, avec des temps de démarrage souvent plus courts qu’en local.

[codespaces-shield]: <https://github.com/codespaces/badge.svg>
[![][codespaces-shield]](https://codespaces.new/ten-framework/ten-agent)

Consultez [ce guide](https://theten.ai/docs/ten_agent/setup_development_env/setting_up_development_inside_codespace) pour plus d’informations.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

<a name="agent-examples-self-hosting"></a>

## Auto-hébergement des exemples d’agents

<a name="deploying-with-docker"></a>

### Déployer avec Docker

Après avoir personnalisé votre agent (via TMAN Designer ou en modifiant `property.json`), générez une image Docker prête pour la production et déployez votre service.

##### Publier en image Docker

**Remarque** : exécutez ces commandes hors de tout conteneur Docker.

###### Construire l’image

```bash
cd ai_agents
docker build -f agents/examples/<example-name>/Dockerfile -t example-app .
```

###### Exécuter

```bash
docker run --rm -it --env-file .env -p 3000:3000 example-app
```

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<a name="deploying-with-other-cloud-services"></a>

### Déployer sur d’autres services cloud

Divisez le déploiement en deux parties pour héberger TEN sur des plateformes comme [Vercel](https://vercel.com) ou [Netlify](https://www.netlify.com).

1. Exécutez le backend TEN sur une plateforme compatible conteneurs (VM Docker, Fly.io, Render, ECS, Cloud Run, etc.). Utilisez l’image fournie et exposez le port `8080`.
2. Déployez uniquement le frontend sur Vercel ou Netlify. Pointez la racine du projet vers `ai_agents/agents/examples/<example>/frontend`, lancez `pnpm install` (ou `bun install`) puis `pnpm build` (ou `bun run build`) et conservez le répertoire `.next` par défaut.
3. Dans le tableau de bord d’hébergement, définissez `AGENT_SERVER_URL` vers l’URL du backend et ajoutez les variables `NEXT_PUBLIC_*` nécessaires (comme les identifiants Agora côté navigateur).
4. Autorisez le frontend à contacter le backend via CORS ouvert ou le middleware proxy intégré.

Ainsi, le backend gère les workers longue durée tandis que le frontend hébergé achemine simplement les requêtes.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

<a name="stay-tuned"></a>

## Restez informé·e

Recevez instantanément les nouvelles versions et les mises à jour. Votre soutien nous aide à faire grandir TEN.

<br>

![Image](https://github.com/user-attachments/assets/72c6cc46-a2a2-484d-82a9-f3079269c815)

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

<a name="ten-ecosystem"></a>

## Écosystème TEN

<br>

| Projet | Aperçu |
| ------- | ------- |
| [**️TEN Framework**][ten-framework-link]<br>Framework open source pour agents conversationnels.<br><br>![][ten-framework-shield] | ![][ten-framework-banner] |
| [**TEN VAD**][ten-vad-link]<br>Détecteur d’activité vocale (VAD) léger et à faible latence.<br><br>![][ten-vad-shield] | ![][ten-vad-banner] |
| [**️TEN Turn Detection**][ten-turn-detection-link]<br>Permet des dialogues full-duplex grâce à la détection de tours.<br><br>![][ten-turn-detection-shield] | ![][ten-turn-detection-banner] |
| [**TEN Agent Examples**][ten-agent-link]<br>Cas d’usage construits avec TEN.<br><br> | ![][ten-agent-banner] |
| [**TEN Portal**][ten-portal-link]<br>Site officiel avec documentation et blog.<br><br>![][ten-portal-shield] | ![][ten-portal-banner] |

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

<a name="questions"></a>

## Questions

TEN Framework est présent sur des plateformes de questions/réponses alimentées par l’IA. Elles fournissent des réponses multilingues, de la configuration de base aux cas avancés.

| Service | Lien |
| ------- | ---- |
| DeepWiki | [![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/TEN-framework/TEN-framework) |
| ReadmeX | [![ReadmeX](https://raw.githubusercontent.com/CodePhiliaX/resource-trusteeship/main/readmex.svg)](https://readmex.com/TEN-framework/ten-framework) |

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<a name="contributing"></a>

## Contribuer

Nous accueillons toute forme de collaboration open source ! Corrections de bugs, nouvelles fonctionnalités, documentation ou idées : vos contributions font progresser les outils d’IA personnalisés. Consultez les Issues et Projects GitHub pour trouver des sujets sur lesquels intervenir et montrer votre expertise. Ensemble, faisons grandir TEN !

<br>

> [!TIP]
>
> **Toutes les contributions comptent** 🙏
>
> Aidez-nous à améliorer TEN. Du code à la doc, chaque partage est précieux. Publiez vos projets TEN Agent sur les réseaux pour inspirer la communauté.
>
> Contactez un mainteneur, [@elliotchen200](https://x.com/elliotchen200) sur 𝕏 ou [@cyfyifanchen](https://github.com/cyfyifanchen) sur GitHub, pour suivre les actualités, échanger et collaborer.

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<a name="code-contributors"></a>

### Contributrices et contributeurs

[![TEN](https://contrib.rocks/image?repo=TEN-framework/ten-agent)](https://github.com/TEN-framework/ten-agent/graphs/contributors)

<a name="contribution-guidelines"></a>

### Guide de contribution

Les contributions sont les bienvenues ! Lisez d’abord le [guide de contribution](./code-of-conduct/contributing.md).

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<a name="license"></a>

### Licence

1. L’ensemble de TEN Framework (hors dossiers listés ci-dessous) est publié sous licence Apache 2.0 avec restrictions additionnelles. Voir le fichier [LICENSE](./../LICENSE) à la racine.
2. Les composants du dossier `packages` sont publiés sous Apache 2.0. Référez-vous au fichier `LICENSE` propre à chaque package.
3. Les bibliothèques tierces utilisées par TEN Framework sont référencées dans le dossier [third_party](./../third_party/).

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
