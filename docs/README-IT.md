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

[Sito ufficiale](https://theten.ai)
•
[Documentazione](https://theten.ai/docs/ten_agent/overview)
•
[Blog](https://theten.ai/blog)

<a href="https://github.com/TEN-framework/ten-framework/blob/main/README.md"><img alt="README in inglese" src="https://img.shields.io/badge/English-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-CN.md"><img alt="Guida in cinese semplificato" src="https://img.shields.io/badge/简体中文-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-JP.md"><img alt="README in giapponese" src="https://img.shields.io/badge/日本語-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-KR.md"><img alt="README in coreano" src="https://img.shields.io/badge/한국어-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-ES.md"><img alt="README in spagnolo" src="https://img.shields.io/badge/Español-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-FR.md"><img alt="README in francese" src="https://img.shields.io/badge/Français-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-IT.md"><img alt="README in italiano" src="https://img.shields.io/badge/Italiano-lightgrey"></a>

<a href="https://trendshift.io/repositories/11978" target="_blank"><img src="https://trendshift.io/api/badge/repositories/11978" alt="TEN-framework%2Ften_framework | Trendshift" style="width: 250px; height: 55px;" width="250" height="55"/></a>

</div>

<br>

<details open>
  <summary><kbd>Indice</kbd></summary>

  <br>

- [Benvenuto in TEN](#welcome-to-ten)
- [Esempi di agenti](#agent-examples)
- [Guida rapida agli esempi di agenti](#quick-start-with-agent-examples)
  - [Ambiente locale](#localhost)
  - [Codespaces](#codespaces)
- [Auto-hosting degli esempi](#agent-examples-self-hosting)
  - [Distribuire con Docker](#deploying-with-docker)
  - [Distribuire su altri servizi cloud](#deploying-with-other-cloud-services)
- [Rimani aggiornato](#stay-tuned)
- [Ecosistema TEN](#ten-ecosystem)
- [Domande](#questions)
- [Contribuire](#contributing)
  - [Contributor del codice](#code-contributors)
  - [Linee guida per contribuire](#contribution-guidelines)
  - [Licenza](#license)

<br/>

</details>

<a name="welcome-to-ten"></a>

## Benvenuto in TEN

TEN è un framework open source per creare agenti vocali conversazionali.

L’[ecosistema TEN](#ten-ecosystem) comprende [TEN Framework](https://github.com/ten-framework/ten-framework), [Esempi di agenti](https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples), [VAD](https://github.com/ten-framework/ten-vad), [Turn Detection](https://github.com/ten-framework/ten-turn-detection) e [Portal](https://github.com/ten-framework/portal).

<br>

| Canale della community | Scopo |
| ---------------- | ------- |
| [![Follow on X](https://img.shields.io/twitter/follow/TenFramework?logo=X&color=%20%23f5f5f5)](https://twitter.com/intent/follow?screen_name=TenFramework) | Segui TEN Framework su X per aggiornamenti e annunci |
| [![Discord TEN Community](https://img.shields.io/badge/Discord-Join%20TEN%20Community-5865F2?style=flat&logo=discord&logoColor=white)](https://discord.gg/VnPftUzAMJ) | Unisciti alla community Discord per confrontarti con altri sviluppatori |
| [![Follow on LinkedIn](https://custom-icon-badges.demolab.com/badge/LinkedIn-TEN_Framework-0A66C2?logo=linkedin-white&logoColor=fff)](https://www.linkedin.com/company/ten-framework) | Segui TEN Framework su LinkedIn per non perdere nessuna novità |
| [![Hugging Face Space](https://img.shields.io/badge/Hugging%20Face-TEN%20Framework-yellow?style=flat&logo=huggingface)](https://huggingface.co/TEN-framework) | Esplora i nostri spazi e modelli su Hugging Face |
| [![WeChat](https://img.shields.io/badge/TEN_Framework-WeChat_Group-%2307C160?logo=wechat&labelColor=darkgreen&color=gray)](https://github.com/TEN-framework/ten-agent/discussions/170) | Entra nel gruppo WeChat per parlare con la community cinese |

<br>

<a name="agent-examples"></a>

## Esempi di agenti

<br>

![Image](https://github.com/user-attachments/assets/dce3db80-fb48-4e2a-8ac7-33f50bcffa32)

<strong>Assistente vocale multiuso</strong> — Assistente in tempo reale, a bassa latenza e alta qualità, estendibile con <a href="ai_agents/agents/examples/voice-assistant-with-memU">memoria</a>, <a href="ai_agents/agents/examples/voice-assistant-with-ten-vad">VAD</a>, <a href="ai_agents/agents/examples/voice-assistant-with-turn-detection">rilevamento dei turni</a> e altre estensioni.

Consulta il <a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples/voice-assistant">codice di esempio</a> per maggiori dettagli.

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/aa3f2c9c-c82e-412f-8400-06378ba75794)

<strong>Avatar con lip sync</strong> — Supporta diversi provider di avatar. La demo mostra Kei, un personaggio anime con sincronizzazione labiale Live2D, e presto includerà avatar realistici di Trulience, HeyGen e Tavus.

Guarda il <a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples/voice-assistant-live2d">codice di esempio Live2D</a>.

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/f94b21b8-9dda-4efc-9274-b028cc01296a)

<strong>Diarizzazione vocale</strong> — Rilevamento e etichettatura dei parlanti in tempo reale. Il gioco "Who Likes What" mostra un caso d’uso interattivo.

<a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples/speechmatics-diarization">Codice di esempio</a>

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/6ed5b04d-945a-4a30-a1cc-f8014b602b38)

<strong>Chiamata SIP</strong> — Estensione SIP che abilita chiamate telefoniche gestite da TEN.

<a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples/voice-assistant-sip-twilio">Codice di esempio</a>

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/d793bc6c-c8de-4996-bd85-9ce88c69dd8d)

<strong>Trascrizione</strong> — Strumento che trascrive l’audio in testo.

<a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples/transcription">Codice di esempio</a>

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/3d60f1ff-0f82-4fe7-b5c2-ac03d284f60c)

<strong>ESP32-S3 Korvo V3</strong> — Esegue un esempio di TEN Agent sulla scheda di sviluppo Espressif ESP32-S3 Korvo V3 per portare comunicazioni basate su LLM sull’hardware.

Consulta la <a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/esp32-client">guida di integrazione</a> per ulteriori informazioni.

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<a name="quick-start-with-agent-examples"></a>

## Guida rapida agli esempi di agenti

<a name="localhost"></a>

### Ambiente locale

#### Passaggio ⓵ - Prerequisiti

| Categoria | Requisiti |
| --- | --- |
| **Chiavi** | • Agora [App ID](https://docs.agora.io/en/video-calling/get-started/manage-agora-account?platform=web#create-an-agora-project) e [App Certificate](https://docs.agora.io/en/video-calling/get-started/manage-agora-account?platform=web#create-an-agora-project) (minuti gratuiti ogni mese)<br>• Chiave API [OpenAI](https://openai.com/index/openai-api/) (qualsiasi LLM compatibile con OpenAI)<br>• ASR [Deepgram](https://deepgram.com/) (crediti gratuiti alla registrazione)<br>• TTS [ElevenLabs](https://elevenlabs.io/) (crediti gratuiti alla registrazione) |
| **Installazione** | • [Docker](https://www.docker.com/) / [Docker Compose](https://docs.docker.com/compose/)<br>• [Node.js (LTS) v18](https://nodejs.org/en) |
| **Requisiti minimi** | • CPU ≥ 2 core<br>• RAM ≥ 4 GB |

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<!-- > [!NOTE]
> **macOS: impostazioni Docker su Apple Silicon**
>
> Deseleziona "Use Rosetta for x86/amd64 emulation" nelle impostazioni di Docker. Le build possono essere più lente su ARM, ma le prestazioni risultano normali sui server x64. -->

#### Passaggio ⓶ - Compila gli esempi nella VM

##### 1. Clona il repo, entra in `ai_agents` e crea `.env` da `.env.example`

```bash
cd ai_agents
cp ./.env.example ./.env
```

##### 2. Configura Agora App ID e App Certificate in `.env`

```bash
AGORA_APP_ID=
AGORA_APP_CERTIFICATE=

# Esegui l’esempio predefinito dell’assistente vocale
# Deepgram (necessario per STT)
DEEPGRAM_API_KEY=your_deepgram_api_key_here

# OpenAI (necessario per il modello linguistico)
OPENAI_API_KEY=your_openai_api_key_here
OPENAI_MODEL=gpt-4o

# ElevenLabs (necessario per TTS)
ELEVENLABS_TTS_KEY=your_elevenlabs_api_key_here
```

##### 3. Avvia i container di sviluppo

```bash
docker compose up -d
```

##### 4. Entra nel container

```bash
docker exec -it ten_agent_dev bash
```

##### 5. Compila l’agente con l’esempio predefinito (≈5-8 min)

Trovi altre demo nella cartella `agents/examples`.
Inizia da una di queste:

```bash
# Assistente vocale concatenato
cd agents/examples/voice-assistant

# Assistente voce-a-voce in tempo reale
cd agents/examples/voice-assistant-realtime
```

##### 6. Avvia il server web

Esegui `task build` se hai modificato il codice locale. È obbligatorio per i linguaggi compilati (TypeScript, Go, ecc.) e facoltativo per Python.

```bash
task install
task run
```

##### 7. Accedi all’agente

Quando l’esempio è in esecuzione puoi usare queste interfacce:

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

- TMAN Designer: <http://localhost:49483>
- UI degli esempi: <http://localhost:3000>

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

#### Passaggio ⓷ - Personalizza l’esempio

1. Apri [localhost:49483](http://localhost:49483).
2. Fai clic con il tasto destro sulle estensioni STT, LLM e TTS.
3. Inserisci le relative API key.
4. Dopo aver salvato, la versione aggiornata sarà visibile su [localhost:3000](http://localhost:3000).

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

<a name="codespaces"></a>

### Codespaces

GitHub offre Codespaces gratuiti per ogni repository. Puoi eseguire gli esempi senza Docker e, in genere, l’avvio è più rapido rispetto all’ambiente locale basato su container.

[codespaces-shield]: <https://github.com/codespaces/badge.svg>
[![][codespaces-shield]](https://codespaces.new/ten-framework/ten-agent)

Consulta [questa guida](https://theten.ai/docs/ten_agent/setup_development_env/setting_up_development_inside_codespace) per i dettagli.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

<a name="agent-examples-self-hosting"></a>

## Auto-hosting degli esempi

<a name="deploying-with-docker"></a>

### Distribuire con Docker

Dopo aver personalizzato l’agente (con TMAN Designer o modificando `property.json`), crea un’immagine Docker di release e distribuiscila.

##### Pubblicare come immagine Docker

**Nota**: esegui i comandi al di fuori di qualsiasi container.

###### Build dell’immagine

```bash
cd ai_agents
docker build -f agents/examples/<example-name>/Dockerfile -t example-app .
```

###### Esecuzione

```bash
docker run --rm -it --env-file .env -p 3000:3000 example-app
```

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<a name="deploying-with-other-cloud-services"></a>

### Distribuire su altri servizi cloud

Puoi dividere il deployment in due parti quando ospiti TEN su piattaforme come [Vercel](https://vercel.com) o [Netlify](https://www.netlify.com).

1. Esegui il backend TEN su una piattaforma compatibile con container (VM Docker, Fly.io, Render, ECS, Cloud Run, ecc.). Usa l’immagine di esempio senza modificarla ed esponi la porta `8080`.
2. Distribuisci solo il frontend su Vercel o Netlify. Imposta la radice del progetto su `ai_agents/agents/examples/<example>/frontend`, esegui `pnpm install` (o `bun install`) e poi `pnpm build` (o `bun run build`), mantenendo la cartella di output `.next` predefinita.
3. Configura le variabili di ambiente dal pannello del provider: `AGENT_SERVER_URL` deve puntare al backend e aggiungi le chiavi `NEXT_PUBLIC_*` necessarie (per esempio le credenziali Agora esposte al browser).
4. Consenti al backend di accettare richieste dall’origine del frontend tramite CORS aperto o usando il middleware proxy incluso.

In questo modo il backend gestisce i processi di lunga durata e il frontend hostato instrada semplicemente il traffico API.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

<a name="stay-tuned"></a>

## Rimani aggiornato

Ricevi notifiche immediate su nuove release e aggiornamenti. Il tuo supporto ci aiuta a migliorare TEN!

<br>

![Image](https://github.com/user-attachments/assets/72c6cc46-a2a2-484d-82a9-f3079269c815)

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

<a name="ten-ecosystem"></a>

## Ecosistema TEN

<br>

| Progetto | Anteprima |
| ------- | ------- |
| [**️TEN Framework**][ten-framework-link]<br>Framework open source per agenti conversazionali.<br><br>![][ten-framework-shield] | ![][ten-framework-banner] |
| [**TEN VAD**][ten-vad-link]<br>Rilevatore di attività vocale in streaming, leggero e a bassa latenza.<br><br>![][ten-vad-shield] | ![][ten-vad-banner] |
| [**️TEN Turn Detection**][ten-turn-detection-link]<br>Abilita dialoghi full-duplex tramite rilevamento dei turni.<br><br>![][ten-turn-detection-shield] | ![][ten-turn-detection-banner] |
| [**TEN Agent Examples**][ten-agent-link]<br>Casi d’uso costruiti con TEN.<br><br> | ![][ten-agent-banner] |
| [**TEN Portal**][ten-portal-link]<br>Sito ufficiale con documentazione e blog.<br><br>![][ten-portal-shield] | ![][ten-portal-banner] |

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

<a name="questions"></a>

## Domande

TEN Framework è presente anche su piattaforme di Q&A alimentate dall’IA. Offrono risposte multilingue, dalla configurazione di base agli scenari avanzati.

| Servizio | Link |
| ------- | ---- |
| DeepWiki | [![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/TEN-framework/TEN-framework) |
| ReadmeX | [![ReadmeX](https://raw.githubusercontent.com/CodePhiliaX/resource-trusteeship/main/readmex.svg)](https://readmex.com/TEN-framework/ten-framework) |

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<a name="contributing"></a>

## Contribuire

Accogliamo qualsiasi forma di collaborazione open source! Bugfix, nuove funzionalità, documentazione o idee: ogni contributo aiuta a far crescere strumenti di IA personalizzati. Dai un’occhiata a Issues e Projects su GitHub per trovare attività su cui lavorare e mostrare le tue competenze. Costruiamo TEN insieme!

<br>

> [!TIP]
>
> **Ogni contributo è importante** 🙏
>
> Aiutaci a migliorare TEN: dal codice alla documentazione, tutto conta. Condividi i tuoi progetti TEN Agent sui social per ispirare altre persone.
>
> Contatta un maintainer — [@elliotchen200](https://x.com/elliotchen200) su 𝕏 o [@cyfyifanchen](https://github.com/cyfyifanchen) su GitHub — per aggiornamenti, discussioni e collaborazioni.

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<a name="code-contributors"></a>

### Contributor del codice

[![TEN](https://contrib.rocks/image?repo=TEN-framework/ten-agent)](https://github.com/TEN-framework/ten-agent/graphs/contributors)

<a name="contribution-guidelines"></a>

### Linee guida per contribuire

Le contribuzioni sono benvenute! Leggi prima le [linee guida per contribuire](./code-of-conduct/contributing.md).

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<a name="license"></a>

### Licenza

1. L’intero TEN Framework (esclusi i folder elencati qui sotto) è rilasciato sotto Apache License 2.0 con restrizioni aggiuntive. Consulta il file [LICENSE](./../LICENSE) nella root del progetto.
2. I componenti nella directory `packages` sono distribuiti sotto Apache License 2.0. Ogni package contiene il proprio file `LICENSE`.
3. Le librerie di terze parti utilizzate da TEN Framework sono elencate nella cartella [third_party](./../third_party/).

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
