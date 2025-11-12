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

[Sitio oficial](https://theten.ai)
•
[Documentación](https://theten.ai/docs/ten_agent/overview)
•
[Blog](https://theten.ai/blog)

<a href="https://github.com/TEN-framework/ten-framework/blob/main/README.md"><img alt="README en inglés" src="https://img.shields.io/badge/English-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-CN.md"><img alt="Guía en chino simplificado" src="https://img.shields.io/badge/简体中文-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-JP.md"><img alt="README en japonés" src="https://img.shields.io/badge/日本語-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-KR.md"><img alt="README en coreano" src="https://img.shields.io/badge/한국어-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-ES.md"><img alt="README en español" src="https://img.shields.io/badge/Español-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-FR.md"><img alt="README en francés" src="https://img.shields.io/badge/Français-lightgrey"></a>
<a href="https://github.com/TEN-framework/ten-framework/blob/main/docs/README-IT.md"><img alt="README en italiano" src="https://img.shields.io/badge/Italiano-lightgrey"></a>

<a href="https://trendshift.io/repositories/11978" target="_blank"><img src="https://trendshift.io/api/badge/repositories/11978" alt="TEN-framework%2Ften_framework | Trendshift" style="width: 250px; height: 55px;" width="250" height="55"/></a>

</div>

<br>

<details open>
  <summary><kbd>Tabla de contenido</kbd></summary>

  <br>

- [Bienvenido a TEN](#welcome-to-ten)
- [Ejemplos de agente](#agent-examples)
- [Inicio rápido con los ejemplos de agente](#quick-start-with-agent-examples)
  - [Entorno local](#localhost)
  - [Codespaces](#codespaces)
- [Auto-hospedaje de los ejemplos de agente](#agent-examples-self-hosting)
  - [Implementar con Docker](#deploying-with-docker)
  - [Implementar en otros servicios en la nube](#deploying-with-other-cloud-services)
- [Mantente al día](#stay-tuned)
- [Ecosistema TEN](#ten-ecosystem)
- [Preguntas](#questions)
- [Cómo contribuir](#contributing)
  - [Personas contribuidoras](#code-contributors)
  - [Guía de contribución](#contribution-guidelines)
  - [Licencia](#license)

<br/>

</details>

<a name="welcome-to-ten"></a>

## Bienvenido a TEN

TEN es un marco de código abierto para agentes conversacionales de voz impulsados por IA.

El [ecosistema TEN](#ten-ecosystem) incluye [TEN Framework](https://github.com/ten-framework/ten-framework), [Ejemplos de agente](https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples), [VAD](https://github.com/ten-framework/ten-vad), [Turn Detection](https://github.com/ten-framework/ten-turn-detection) y [Portal](https://github.com/ten-framework/portal).

<br>

| Canal de la comunidad | Propósito |
| ---------------- | ------- |
| [![Follow on X](https://img.shields.io/twitter/follow/TenFramework?logo=X&color=%20%23f5f5f5)](https://twitter.com/intent/follow?screen_name=TenFramework) | Sigue TEN Framework en X para enterarte de novedades y anuncios |
| [![Discord TEN Community](https://img.shields.io/badge/Discord-Join%20TEN%20Community-5865F2?style=flat&logo=discord&logoColor=white)](https://discord.gg/VnPftUzAMJ) | Únete a la comunidad de Discord y conecta con otras personas desarrolladoras |
| [![Follow on LinkedIn](https://custom-icon-badges.demolab.com/badge/LinkedIn-TEN_Framework-0A66C2?logo=linkedin-white&logoColor=fff)](https://www.linkedin.com/company/ten-framework) | Sigue TEN Framework en LinkedIn para recibir actualizaciones y noticias |
| [![Hugging Face Space](https://img.shields.io/badge/Hugging%20Face-TEN%20Framework-yellow?style=flat&logo=huggingface)](https://huggingface.co/TEN-framework) | Explora nuestros espacios y modelos en la comunidad de Hugging Face |
| [![WeChat](https://img.shields.io/badge/TEN_Framework-WeChat_Group-%2307C160?logo=wechat&labelColor=darkgreen&color=gray)](https://github.com/TEN-framework/ten-agent/discussions/170) | Únete al grupo de WeChat para conversar con la comunidad china |

<br>

<a name="agent-examples"></a>

## Ejemplos de agente

<br>

![Image](https://github.com/user-attachments/assets/dce3db80-fb48-4e2a-8ac7-33f50bcffa32)

<strong>Asistente de voz multipropósito</strong> — Un asistente en tiempo real, de baja latencia y alta calidad que puedes ampliar con <a href="ai_agents/agents/examples/voice-assistant-with-memU">memoria</a>, <a href="ai_agents/agents/examples/voice-assistant-with-ten-vad">VAD</a>, <a href="ai_agents/agents/examples/voice-assistant-with-turn-detection">detección de turnos</a> y otras extensiones.

Consulta el <a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples/voice-assistant">código de ejemplo</a> para obtener más detalles.

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/aa3f2c9c-c82e-412f-8400-06378ba75794)

<strong>Avatares con sincronización labial</strong> — Compatible con múltiples proveedores de avatares. La demo incluye a Kei, un personaje anime con sincronización labial gracias a Live2D, y pronto añadirá avatares realistas de Trulience, HeyGen y Tavus.

Revisa el <a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples/voice-assistant-live2d">código de ejemplo</a> para Live2D.

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/f94b21b8-9dda-4efc-9274-b028cc01296a)

<strong>Diarización de voz</strong> — Detección y etiquetado de hablantes en tiempo real. El juego "Who Likes What" muestra un caso de uso interactivo.

<a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples/speechmatics-diarization">Código de ejemplo</a>

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/6ed5b04d-945a-4a30-a1cc-f8014b602b38)

<strong>Llamada SIP</strong> — Extensión SIP que habilita llamadas telefónicas impulsadas por TEN.

<a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples/voice-assistant-sip-twilio">Código de ejemplo</a>

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/d793bc6c-c8de-4996-bd85-9ce88c69dd8d)

<strong>Transcripción</strong> — Herramienta de transcripción que convierte audio en texto.

<a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples/transcription">Código de ejemplo</a>

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

![Image](https://github.com/user-attachments/assets/3d60f1ff-0f82-4fe7-b5c2-ac03d284f60c)

<strong>ESP32-S3 Korvo V3</strong> — Ejecuta el ejemplo de TEN Agent en la placa de desarrollo Espressif ESP32-S3 Korvo V3 para integrar comunicación impulsada por LLM con hardware.

Consulta la <a href="https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/esp32-client">guía de integración</a> para conocer más.

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<a name="quick-start-with-agent-examples"></a>

## Inicio rápido con los ejemplos de agente

<a name="localhost"></a>

### Entorno local

#### Paso ⓵ - Requisitos previos

| Categoría | Requisitos |
| --- | --- |
| **Credenciales** | • Agora [App ID](https://docs.agora.io/en/video-calling/get-started/manage-agora-account?platform=web#create-an-agora-project) y [App Certificate](https://docs.agora.io/en/video-calling/get-started/manage-agora-account?platform=web#create-an-agora-project) (minutos gratuitos mensuales)<br>• Clave de API de [OpenAI](https://openai.com/index/openai-api/) (cualquier LLM compatible con el protocolo de OpenAI)<br>• ASR de [Deepgram](https://deepgram.com/) (créditos gratuitos al registrarte)<br>• TTS de [ElevenLabs](https://elevenlabs.io/) (créditos gratuitos al registrarte) |
| **Instalación** | • [Docker](https://www.docker.com/) / [Docker Compose](https://docs.docker.com/compose/)<br>• [Node.js (LTS) v18](https://nodejs.org/en) |
| **Requisitos mínimos del sistema** | • CPU ≥ 2 núcleos<br>• RAM ≥ 4 GB |

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<!-- > [!NOTE]
> **macOS: configuración de Docker en Apple Silicon**
>
> Desmarca "Use Rosetta for x86/amd64 emulation" en los ajustes de Docker. La compilación puede tardar más en ARM, pero el rendimiento será normal al desplegar en servidores x64. -->

#### Paso ⓶ - Compila los ejemplos dentro de una VM

##### 1. Clona el repositorio, entra en `ai_agents` y crea un `.env` a partir de `.env.example`

```bash
cd ai_agents
cp ./.env.example ./.env
```

##### 2. Configura el Agora App ID y App Certificate en `.env`

```bash
AGORA_APP_ID=
AGORA_APP_CERTIFICATE=

# Ejecutar el ejemplo predeterminado del asistente de voz
# Deepgram (requerido para STT)
DEEPGRAM_API_KEY=your_deepgram_api_key_here

# OpenAI (requerido para el modelo de lenguaje)
OPENAI_API_KEY=your_openai_api_key_here
OPENAI_MODEL=gpt-4o

# ElevenLabs (requerido para TTS)
ELEVENLABS_TTS_KEY=your_elevenlabs_api_key_here
```

##### 3. Inicia los contenedores de desarrollo del agente

```bash
docker compose up -d
```

##### 4. Entra en el contenedor

```bash
docker exec -it ten_agent_dev bash
```

##### 5. Compila el agente con el ejemplo predeterminado (≈5‑8 min)

En la carpeta `agents/examples` encontrarás más muestras.
Empieza con una de estas opciones:

```bash
# Usa el asistente de voz encadenado
cd agents/examples/voice-assistant

# O usa el asistente voz-a-voz en tiempo real
cd agents/examples/voice-assistant-realtime
```

##### 6. Inicia el servidor web

Ejecuta `task build` si cambiaste código local. Es obligatorio para lenguajes compilados (TypeScript, Go, etc.) y opcional para Python.

```bash
task install
task run
```

##### 7. Accede al agente

Cuando el ejemplo esté en marcha podrás usar estas interfaces:

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
- UI de ejemplos de agente: <http://localhost:3000>

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

#### Paso ⓷ - Personaliza tu ejemplo de agente

1. Abre [localhost:49483](http://localhost:49483).
2. Haz clic derecho en las extensiones STT, LLM y TTS.
3. Rellena sus propiedades con las API keys correspondientes.
4. Envía los cambios; el ejemplo actualizado aparecerá en [localhost:3000](http://localhost:3000).

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<br>

<a name="codespaces"></a>

### Codespaces

GitHub ofrece Codespaces gratuitos para cada repositorio. Puedes ejecutar los ejemplos de agente allí sin usar Docker, y normalmente inicia más rápido que un entorno local con contenedores.

[codespaces-shield]: <https://github.com/codespaces/badge.svg>
[![][codespaces-shield]](https://codespaces.new/ten-framework/ten-agent)

Consulta [esta guía](https://theten.ai/docs/ten_agent/setup_development_env/setting_up_development_inside_codespace) para obtener más detalles.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

<a name="agent-examples-self-hosting"></a>

## Auto-hospedaje de los ejemplos de agente

<a name="deploying-with-docker"></a>

### Implementar con Docker

Cuando personalices tu agente (ya sea con TMAN Designer o editando `property.json`), crea una imagen de Docker lista para producción y despliega tu servicio.

##### Publicar como imagen de Docker

**Nota**: Ejecuta estos comandos fuera de cualquier contenedor Docker.

###### Compilar la imagen

```bash
cd ai_agents
docker build -f agents/examples/<example-name>/Dockerfile -t example-app .
```

###### Ejecutar

```bash
docker run --rm -it --env-file .env -p 3000:3000 example-app
```

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<a name="deploying-with-other-cloud-services"></a>

### Implementar en otros servicios en la nube

Puedes dividir el despliegue en dos partes si deseas alojar TEN en proveedores como [Vercel](https://vercel.com) o [Netlify](https://www.netlify.com).

1. Ejecuta el backend de TEN en cualquier plataforma preparada para contenedores (una VM con Docker, Fly.io, Render, ECS, Cloud Run, etc.). Usa la imagen de ejemplo sin modificar y expón el puerto `8080`.
2. Despliega solo el frontend en Vercel o Netlify. Apunta la raíz del proyecto a `ai_agents/agents/examples/<example>/frontend`, ejecuta `pnpm install` (o `bun install`), luego `pnpm build` (o `bun run build`) y conserva el directorio de salida `.next` predeterminado.
3. Configura las variables de entorno en el panel de tu hosting. `AGENT_SERVER_URL` debe apuntar al backend y añade cualquier clave `NEXT_PUBLIC_*` necesaria (por ejemplo, credenciales de Agora visibles para el navegador).
4. Asegúrate de que tu backend acepte solicitudes desde el origen del frontend, ya sea mediante CORS abierto o usando el middleware proxy incluido.

Con esta arquitectura, el backend gestiona los procesos de larga duración y el frontend alojado solo reenvía el tráfico al backend.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

<a name="stay-tuned"></a>

## Mantente al día

Recibe notificaciones instantáneas sobre nuevas versiones y actualizaciones. Tu apoyo nos ayuda a seguir mejorando TEN.

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

| Proyecto | Vista previa |
| ------- | ------- |
| [**️TEN Framework**][ten-framework-link]<br>Marco de código abierto para agentes conversacionales.<br><br>![][ten-framework-shield] | ![][ten-framework-banner] |
| [**TEN VAD**][ten-vad-link]<br>Detector de actividad de voz (VAD) en streaming, ligero y de baja latencia.<br><br>![][ten-vad-shield] | ![][ten-vad-banner] |
| [**️TEN Turn Detection**][ten-turn-detection-link]<br>Permite diálogo full-duplex mediante detección de turnos.<br><br>![][ten-turn-detection-shield] | ![][ten-turn-detection-banner] |
| [**TEN Agent Examples**][ten-agent-link]<br>Casos de uso impulsados por TEN.<br><br> | ![][ten-agent-banner] |
| [**TEN Portal**][ten-portal-link]<br>Sitio oficial con documentación y blog.<br><br>![][ten-portal-shield] | ![][ten-portal-banner] |

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

<a name="questions"></a>

## Preguntas

TEN Framework también está disponible en plataformas de preguntas y respuestas impulsadas por IA. Ofrecen soporte multilingüe para todo, desde la configuración básica hasta la implementación avanzada.

| Servicio | Enlace |
| ------- | ---- |
| DeepWiki | [![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/TEN-framework/TEN-framework) |
| ReadmeX | [![ReadmeX](https://raw.githubusercontent.com/CodePhiliaX/resource-trusteeship/main/readmex.svg)](https://readmex.com/TEN-framework/ten-framework) |

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<a name="contributing"></a>

## Cómo contribuir

¡Toda forma de colaboración de código abierto es bienvenida! Ya sea que corrijas bugs, agregues funciones, mejores la documentación o compartas ideas, tus aportes ayudan a impulsar herramientas de IA personalizadas. Revisa los Issues y Projects de GitHub para encontrar oportunidades y mostrar tus habilidades. ¡Construyamos juntas y juntos algo increíble!

<br>

> [!TIP]
>
> **Agradecemos todo tipo de contribuciones** 🙏
>
> Acompáñanos a mejorar TEN. Cada PR, issue o guía suma. Comparte tus proyectos con TEN Agent en redes sociales para inspirar a la comunidad.
>
> Ponte en contacto con la persona mantenedora [@elliotchen200](https://x.com/elliotchen200) en 𝕏 o [@cyfyifanchen](https://github.com/cyfyifanchen) en GitHub para recibir novedades, debatir ideas y explorar colaboraciones.

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<a name="code-contributors"></a>

### Personas contribuidoras

[![TEN](https://contrib.rocks/image?repo=TEN-framework/ten-agent)](https://github.com/TEN-framework/ten-agent/graphs/contributors)

<a name="contribution-guidelines"></a>

### Guía de contribución

¡Contribuye cuando quieras! Lee primero la [guía de contribución](./code-of-conduct/contributing.md).

<br>

![divider](https://github.com/user-attachments/assets/aec54c94-ced9-4683-ae58-0a5a7ed803bd)

<a name="license"></a>

### Licencia

1. Todo TEN Framework, salvo los directorios listados más abajo, se publica bajo la licencia Apache 2.0 con restricciones adicionales. Consulta el archivo [LICENSE](./../LICENSE) en la raíz del proyecto.
2. Los componentes dentro de `packages` se liberan bajo Apache License 2.0. Cada paquete contiene su propio archivo `LICENSE` con los detalles.
3. Las dependencias de terceros que usa TEN Framework se describen en la carpeta [third_party](./../third_party/).

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
