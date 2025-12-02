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

[Site officiel][official-site]
•
[Documentation][documentation]
•
[Blog][blog]

[![README en anglais][lang-en-badge]][lang-en-readme]
[![Guide en chinois simplifié][lang-zh-badge]][lang-zh-readme]
[![README en japonais][lang-jp-badge]][lang-jp-readme]
[![README en coréen][lang-kr-badge]][lang-kr-readme]
[![README en espagnol][lang-es-badge]][lang-es-readme]
[![README en français][lang-fr-badge]][lang-fr-readme]
[![README en italien][lang-it-badge]][lang-it-readme]

[![TEN-framework%2Ften_framework | Trendshift][trendshift-badge]][trendshift]

</div>

<br>

<details open>
  <summary><kbd>Table des matières</kbd></summary>

  <br>

- [Bienvenue chez TEN][welcome-to-ten]
- [Exemples d’agents][agent-examples-section]
- [Démarrage rapide avec les exemples d’agents][quick-start]
  - [En local][localhost-section]
  - [Codespaces][codespaces-section]
- [Auto-hébergement des exemples d’agents][agent-examples-self-hosting]
  - [Déployer avec Docker][deploying-with-docker]
  - [Déployer sur d’autres services cloud][deploying-with-other-cloud-services]
- [Restez informé·e][stay-tuned]
- [Écosystème TEN][ten-ecosystem-anchor]
- [Questions][questions]
- [Contribuer][contributing]
  - [Contributrices et contributeurs][code-contributors]
  - [Guide de contribution][contribution-guidelines]
  - [Licence][license-section]

<br/>

</details>

<a name="welcome-to-ten"></a>

## Bienvenue chez TEN

TEN est un framework open source pour créer des agents conversationnels vocaux pilotés par l’IA.

L’[écosystème TEN][ten-ecosystem-anchor] comprend [TEN Framework][ten-framework-link], les [Exemples d’agents][ten-agent-example-link], [VAD][ten-vad-link], [Turn Detection][ten-turn-detection-link] et [Portal][ten-portal-link].

<br>

| Canal communautaire | Objectif |
| ---------------- | ------- |
| [![Follow on X][follow-on-x-badge]][follow-on-x] | Suivez TEN Framework sur X pour connaître les nouveautés et annonces |
| [![Discord TEN Community][discord-badge]][discord-invite] | Rejoignez notre communauté Discord pour échanger avec d’autres développeurs |
| [![Follow on LinkedIn][linkedin-badge]][linkedin] | Abonnez-vous sur LinkedIn afin de recevoir nos actualités |
| [![Hugging Face Space][hugging-face-badge]][hugging-face] | Découvrez nos espaces et modèles sur Hugging Face |
| [![WeChat][wechat-badge]][wechat-discussion] | Rejoignez le groupe WeChat pour discuter avec la communauté chinoise |

<br>

<a name="agent-examples"></a>

## Exemples d’agents

<br>

![Image][voice-assistant-image]

<strong>Assistant vocal polyvalent</strong> — Assistant temps réel, basse latence et haute qualité, extensible avec des modules de [mémoire][memory-example], de [VAD][voice-assistant-vad-example], de [détection de tours][voice-assistant-turn-detection-example], etc.

Consultez le [code d’exemple][voice-assistant-example] pour en savoir plus.

<br>

![divider][divider-light]
![divider][divider-dark]

<br>

![Image][lip-sync-image]

<strong>Avatars avec synchronisation labiale</strong> — Compatible avec plusieurs fournisseurs d’avatars. La démo met en scène Kei, un personnage animé avec synchronisation labiale Live2D, et proposera bientôt des avatars réalistes de Trulience, HeyGen et Tavus.

Voir le [code d’exemple Live2D][voice-assistant-live2d-example].

<br>

![divider][divider-light]
![divider][divider-dark]

<br>

![Image][speech-diarization-image]

<strong>Diarisation vocale</strong> — Détection et étiquetage des locuteurs en temps réel. Le jeu "Who Likes What" illustre un cas d’usage interactif.

[Code d’exemple][speechmatics-diarization-example]

<br>

![divider][divider-light]
![divider][divider-dark]

<br>

![Image][sip-call-image]

<strong>Appels SIP</strong> — Extension SIP qui permet d’effectuer des appels téléphoniques propulsés par TEN.

[Code d’exemple][voice-assistant-sip-example]

<br>

![divider][divider-light]
![divider][divider-dark]

<br>

![Image][transcription-image]

<strong>Transcription</strong> — Outil de transcription qui convertit la voix en texte.

[Code d’exemple][transcription-example]

<br>

![divider][divider-light]
![divider][divider-dark]

<br>

![Image][esp32-image]

<strong>ESP32-S3 Korvo V3</strong> — Fait tourner un exemple TEN Agent sur la carte de développement Espressif ESP32-S3 Korvo V3 pour relier communication LLM et matériel.

Voir le [guide d’intégration][esp32-guide] pour plus d’informations.

<br>
<div align="right">

[![][back-to-top]][readme-top]

</div>

<a name="quick-start-with-agent-examples"></a>

## Démarrage rapide avec les exemples d’agents

<a name="localhost"></a>

### En local

#### Étape ⓵ - Prérequis

| Catégorie | Exigences |
| --- | --- |
| **Clés** | • Agora [App ID][agora-app-certificate] et [App Certificate][agora-app-certificate] (minutes gratuites chaque mois)<br>• Clé API de [OpenAI][openai-api] (n’importe quel LLM compatible OpenAI)<br>• ASR [Deepgram][deepgram] (crédits offerts à l’inscription)<br>• TTS [ElevenLabs][elevenlabs] (crédits offerts à l’inscription) |
| **Installation** | • [Docker][docker] / [Docker Compose][docker-compose]<br>• [Node.js (LTS) v18][nodejs] |
| **Configuration minimale** | • CPU ≥ 2 cœurs<br>• RAM ≥ 4 Go |

<br>

![divider][divider-light]
![divider][divider-dark]

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

![divider][divider-light]
![divider][divider-dark]

#### Étape ⓷ - Personnaliser votre exemple

1. Ouvrez [localhost:49483][localhost-49483].
2. Cliquez droit sur les extensions STT, LLM et TTS.
3. Renseignez les clés API correspondantes.
4. Validez : la mise à jour apparaît sur [localhost:3000][localhost-3000].

<br>

![divider][divider-light]
![divider][divider-dark]

<br>

#### Exécuter une application de transcription depuis TEN Manager sans Docker (Beta)

TEN propose aussi une application de transcription que vous pouvez lancer dans TEN Manager sans utiliser Docker.

Consultez le [guide de démarrage rapide][quick-start-guide-ten-manager] pour en savoir plus.

<br>

![divider][divider-light]
![divider][divider-dark]

<br>

<a name="codespaces"></a>

### Codespaces

GitHub fournit des Codespaces gratuits par dépôt. Vous pouvez exécuter les exemples d’agents sans Docker, avec des temps de démarrage souvent plus courts qu’en local.

[codespaces-shield]: <https://github.com/codespaces/badge.svg>
[![][codespaces-shield]][codespaces-new]

Consultez [ce guide][codespaces-guide] pour plus d’informations.

<div align="right">

[![][back-to-top]][readme-top]

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

![divider][divider-light]
![divider][divider-dark]

<a name="deploying-with-other-cloud-services"></a>

### Déployer sur d’autres services cloud

Divisez le déploiement en deux parties pour héberger TEN sur des plateformes comme [Vercel][vercel] ou [Netlify][netlify].

1. Exécutez le backend TEN sur une plateforme compatible conteneurs (VM Docker, Fly.io, Render, ECS, Cloud Run, etc.). Utilisez l’image fournie et exposez le port `8080`.
2. Déployez uniquement le frontend sur Vercel ou Netlify. Pointez la racine du projet vers `ai_agents/agents/examples/<example>/frontend`, lancez `pnpm install` (ou `bun install`) puis `pnpm build` (ou `bun run build`) et conservez le répertoire `.next` par défaut.
3. Dans le tableau de bord d’hébergement, définissez `AGENT_SERVER_URL` vers l’URL du backend et ajoutez les variables `NEXT_PUBLIC_*` nécessaires (comme les identifiants Agora côté navigateur).
4. Autorisez le frontend à contacter le backend via CORS ouvert ou le middleware proxy intégré.

Ainsi, le backend gère les workers longue durée tandis que le frontend hébergé achemine simplement les requêtes.

<div align="right">

[![][back-to-top]][readme-top]

</div>

<br>

<a name="stay-tuned"></a>

## Restez informé·e

Recevez instantanément les nouvelles versions et les mises à jour. Votre soutien nous aide à faire grandir TEN.

<br>

![Image][stay-tuned-image]

<br>
<div align="right">

[![][back-to-top]][readme-top]

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
| [**TEN Agent Examples**][ten-agent-example-link]<br>Cas d'usage construits avec TEN.<br><br> | ![][ten-agent-example-banner] |
| [**TEN Portal**][ten-portal-link]<br>Site officiel avec documentation et blog.<br><br>![][ten-portal-shield] | ![][ten-portal-banner] |

<br>
<div align="right">

[![][back-to-top]][readme-top]

</div>

<br>

<a name="questions"></a>

## Questions

TEN Framework est présent sur des plateformes de questions/réponses alimentées par l’IA. Elles fournissent des réponses multilingues, de la configuration de base aux cas avancés.

| Service | Lien |
| ------- | ---- |
| DeepWiki | [![Ask DeepWiki][deepwiki-badge]][deepwiki] |
| ReadmeX | [![ReadmeX][readmex-badge]][readmex] |

<br>
<div align="right">

[![][back-to-top]][readme-top]

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
> Contactez un mainteneur, [@elliotchen200][elliotchen200-x] sur 𝕏 ou [@cyfyifanchen][cyfyifanchen-github] sur GitHub, pour suivre les actualités, échanger et collaborer.

<br>

![divider][divider-light]
![divider][divider-dark]

<a name="code-contributors"></a>

### Contributrices et contributeurs

[![TEN][contributors-image]][contributors]

<a name="contribution-guidelines"></a>

### Guide de contribution

Les contributions sont les bienvenues ! Lisez d’abord le [guide de contribution][contribution-guidelines-doc].

<br>

![divider][divider-light]
![divider][divider-dark]

<a name="license"></a>

### Licence

1. L’ensemble de TEN Framework (hors dossiers listés ci-dessous) est publié sous licence Apache 2.0 avec restrictions additionnelles. Voir le fichier [LICENSE][license-file] à la racine.
2. Les composants du dossier `packages` sont publiés sous Apache 2.0. Référez-vous au fichier `LICENSE` propre à chaque package.
3. Les bibliothèques tierces utilisées par TEN Framework sont référencées dans le dossier [third_party][third-party-folder].

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
