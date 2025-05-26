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

[Sitio Oficial](https://theten.ai)
•
[Documentación](https://theten.ai/docs/ten_agent/overview)
•
[Blog](https://theten.ai/blog)

<a href="https://trendshift.io/repositories/11978" target="_blank"><img src="https://trendshift.io/api/badge/repositories/11978" alt="TEN-framework%2Ften_framework | Trendshift" style="width: 250px; height: 55px;" width="250" height="55"/></a>

</div>

<br>

<details>
  <summary><kbd>Tabla de Contenidos</kbd></summary>

#### Tabla de Contenidos

- [👋 Bienvenido a TEN](#-bienvenido-a-ten)
- [🎨 TMAN Designer](#-tman-designer)
- [🤖 TEN Agent](#-ten-agent)
  - [1️⃣ Avatar en Tiempo Real](#1️⃣-avatar-en-tiempo-real)
  - [2️⃣ Voz en tiempo real con servidores MCP](#2️⃣-voz-en-tiempo-real-con-servidores-mcp)
  - [3️⃣ Comunicación en tiempo real con hardware](#3️⃣-comunicación-en-tiempo-real-con-hardware)
  - [4️⃣ Visión en tiempo real y detección de pantalla compartida](#4️⃣-visión-en-tiempo-real-y-detección-de-pantalla-compartida)
  - [5️⃣ TEN con otras plataformas LLM](#5️⃣-ten-con-otras-plataformas-llm)
  - [6️⃣ StoryTeller - Generación de imágenes TEN](#6️⃣-storyteller---generación-de-imágenes-ten)
- [🛝 Inicio Rápido con el Área de Pruebas de TEN Agent](#-inicio-rápido-con-el-área-de-pruebas-de-ten-agent)
  - [🅰️ Ejecutar Área de Pruebas en localhost](#🅰️-ejecutar-área-de-pruebas-en-localhost)
  - [🅱️ Ejecutar Área de Pruebas en Codespace(sin docker)](#🅱️-ejecutar-área-de-pruebas-en-codespacesin-docker)
- [🛳️ Auto-hospedaje de TEN Agent](#️-auto-hospedaje-de-ten-agent)
  - [🅰️ 🐳 Despliegue con Docker](#️--despliegue-con-docker)
  - [🅱️ Despliegue con otros servicios en la nube](#️-despliegue-con-otros-servicios-en-la-nube)
- [🏗️ Arquitectura de TEN Agent](#️-arquitectura-de-ten-agent)
- [🌏 Ecosistema TEN Framework](#-ecosistema-ten-framework)
- [❓ Hacer Preguntas](#-hacer-preguntas)
- [🥰 Contribuir](#-contribuir)
  - [Contribuidores de Código](#contribuidores-de-código)
  - [Guía de Contribución](#guía-de-contribución)
  - [Licencia](#licencia)

<br/>

</details>

## 👋 Bienvenido a TEN

TEN es una colección de proyectos de código abierto para construir agentes de voz conversacionales multimodales en tiempo real. Incluye [TEN Framework](https://github.com/ten-framework/ten-framework), [TEN Turn Detection](https://github.com/ten-framework/ten-turn-detection), TEN Agent, TMAN Designer y [TEN Portal](https://github.com/ten-framework/portal), todos completamente de código abierto. [TEN VAD](https://github.com/ten-framework/ten-vad) aún no es completamente de código abierto, pero está abierto para uso público.

<br>

| Canal de Comunidad | Propósito |
| ---------------- | ------- |
| [![Follow on X](https://img.shields.io/twitter/follow/TenFramework?logo=X&color=%20%23f5f5f5)](https://twitter.com/intent/follow?screen_name=TenFramework) | Sigue a TEN Framework en X para actualizaciones y anuncios |
| [![Follow on LinkedIn](https://custom-icon-badges.demolab.com/badge/LinkedIn-TEN_Framework-0A66C2?logo=linkedin-white&logoColor=fff)](https://www.linkedin.com/company/ten-framework) | Sigue a TEN Framework en LinkedIn para actualizaciones y anuncios |
| [![Discord TEN Community](https://dcbadge.vercel.app/api/server/VnPftUzAMJ?&style=flat&theme=light&color=lightgray)](https://discord.gg/VnPftUzAMJ) | Únete a nuestra comunidad Discord para conectar con desarrolladores |
| [![Hugging Face Space](https://img.shields.io/badge/Hugging%20Face-TEN%20Framework-yellow?style=flat&logo=huggingface)](https://huggingface.co/TEN-framework) | Únete a nuestra comunidad Hugging Face para explorar nuestros espacios y modelos |
| [![WeChat](https://img.shields.io/badge/TEN_Framework-WeChat_Group-%2307C160?logo=wechat&labelColor=darkgreen&color=gray)](https://github.com/TEN-framework/ten-agent/discussions/170) | Únete a nuestro grupo de WeChat para discusiones de la comunidad china |

<br>

> \[!IMPORTANT]
>
> **Dale una estrella a los repositorios TEN** ⭐️
>
> ¡Recibe notificaciones instantáneas de nuevos lanzamientos y actualizaciones. Tu apoyo nos ayuda a crecer y mejorar TEN!

<br>

![TEN star us gif](https://github.com/user-attachments/assets/eeebe996-8c14-4bf7-82ae-f1a1f7e30705)

<br>

<details>
  <summary><kbd>Historial de Estrellas</kbd></summary>
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

TMAN Designer es una opción de bajo código/sin código para crear agentes de voz con una interfaz de usuario de flujo de trabajo fácil de usar. Puede cargar aplicaciones y gráficos, e incluye un editor en línea, visor de registros y mucho más.

Consulta [este blog](https://theten.ai/blog/tman-designer-of-ten-framework) para más detalles.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

## ✨ Características

![TEN Agent with Trulience](https://github.com/user-attachments/assets/2f1dfd55-14a3-47ea-ae25-40ad40ceadea)

### 1️⃣ Avatar en Tiempo Real

Construye avatares de IA atractivos con TEN Agent utilizando la diversa colección de opciones de avatares gratuitos de [Trulience](https://trulience.com). Para ponerlo en marcha, solo necesitas 2 pasos:

1. Sigue el README para completar la configuración y ejecución del Área de Pruebas
2. Ingresa el ID del avatar y el [token](https://trulience.com/docs#/authentication/jwt-tokens/jwt-tokens?id=use-your-custom-userid) que obtienes de [Trulience](https://trulience.com)

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

![TEN Agent with MCP servers](https://github.com/user-attachments/assets/934ba928-a4a3-4662-8624-cebefc88ce05)

### 2️⃣ Voz en tiempo real con servidores MCP

TEN Agent ahora se integra perfectamente con servidores MCP, expandiendo sus capacidades de LLM. Para comenzar:

1. Abre el Selector de Módulos en el Área de Pruebas
2. Agrega la herramienta de servidor MCP para integración LLM
3. Pega una URL de tu servidor MCP en la extensión
4. Inicia una conversación en tiempo real con TEN Agent

Esta integración te permite aprovechar las diversas ofertas de servidores de MCP mientras mantienes las potentes habilidades conversacionales de TEN Agent.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

<https://github.com/user-attachments/assets/78647eef-2d66-44e6-99a8-1918a940fb9f>

### 3️⃣ Comunicación en tiempo real con hardware

TEN Agent ahora se ejecuta en la placa de desarrollo Espressif ESP32-S3 Korvo V3, una excelente manera de integrar la comunicación en tiempo real con LLM en hardware.

Consulta la [guía de integración](https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/esp32-client) para más detalles.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

![Real-time vision](https://github.com/user-attachments/assets/7be06e38-994e-4f82-8ec6-183d08fe90f1)

### 4️⃣ Visión en tiempo real y detección de pantalla compartida

Prueba la API Multimodal en Vivo de Google Gemini con capacidades de visión en tiempo real y detección de pantalla compartida, es una extensión lista para usar, junto con potentes herramientas como Verificación del Clima y Búsqueda Web integradas perfectamente en TEN Agent.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

![TEN with other LLM platforms](https://github.com/user-attachments/assets/a3766d50-6a25-4299-b28c-e15772e4201c)

### 5️⃣ TEN con otras plataformas LLM

[TEN Agent + Dify](https://doc.theten.ai/docs/ten_agent/playground/use-cases/voice-assistant/run_dify)

TEN ofrece un gran soporte para mejorar la experiencia interactiva en tiempo real en otras plataformas LLM también, consulta la documentación para más detalles.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

![Image](https://github.com/user-attachments/assets/ea1025d4-b22b-4416-ab35-36cd910bc28c)

### 6️⃣ StoryTeller - Generación de imágenes TEN

Experimenta la generación de imágenes en tiempo real con StoryTeller, es una extensión lista para usar, junto con potentes herramientas como Verificación del Clima y Búsqueda Web integradas perfectamente en TEN.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

## 🛝 Inicio Rápido con el Área de Pruebas de TEN Agent

#### 🅰️ Ejecutar Área de Pruebas en localhost

#### Paso ⓵ - Requisitos Previos

| Categoría | Requisitos |
| --- | --- |
| **Claves** | • Agora [ID de Aplicación](https://docs.agora.io/en/video-calling/get-started/manage-agora-account?platform=web#create-an-agora-project) y [Certificado de Aplicación](https://docs.agora.io/en/video-calling/get-started/manage-agora-account?platform=web#create-an-agora-project) (minutos gratuitos cada mes) <br>• [OpenAI](https://openai.com/index/openai-api/) clave API (cualquier LLM compatible con OpenAI)<br>• [Deepgram](https://deepgram.com/) ASR (créditos gratuitos disponibles al registrarse)<br>• [Elevenlabs](https://elevenlabs.io/) TTS (créditos gratuitos disponibles al registrarse) |
| **Instalación** | • [Docker](https://www.docker.com/) / [Docker Compose](https://docs.docker.com/compose/)<br>• [Node.js(LTS) v18](https://nodejs.org/en) |
| **Requisitos Mínimos del Sistema** | • CPU >= 2 Núcleos<br>• RAM >= 4 GB |

<br>

> \[!NOTA]
>
> **macOS: Configuración de Docker en Apple Silicon**
>
> Desmarca "Use Rosetta for x86/amd64 emulation" en la configuración de Docker, puede resultar en tiempos de compilación más lentos en ARM, pero el rendimiento será normal cuando se despliegue en servidores x64.

<br>

#### Paso ⓶ - Construir agente en VM

##### 1. Clona el repositorio, `cd` a `ai-agents` y crea el archivo `.env` desde `.env.example`

```bash
cd ai_agent
cp ./.env.example ./.env
```

##### 2. Configura el ID de Aplicación y Certificado de Agora en `.env`

```bash
AGORA_APP_ID=
AGORA_APP_CERTIFICATE=
```

##### 3. Inicia los contenedores de desarrollo del agente

```bash
docker compose up -d
```

##### 4. Entra al contenedor

```bash
docker exec -it ten_agent_dev bash
```

##### 5. Construye el agente con el `graph` predeterminado ( ~5min - ~8min)

revisa la carpeta `/examples` para más ejemplos

```bash
# usa el agente predeterminado
task use

# o usa el agente demo
task use AGENT=agents/examples/demo
```

##### 6. Inicia el servidor web

```bash
task run
```

<br>

#### Paso ⓷ - Personaliza tu agente con TMAN Designer

![Customize your agent with TMAN Designer](https://github.com/user-attachments/assets/33f8357b-6762-45eb-8231-c2d83bb77591)

 1. Abre [localhost:49483](http://localhost:49483).
 2. Carga el gráfico correspondiente desde el menú (por ejemplo, Asistente de Voz).
 3. Ingresa las claves API y establece las preferencias para cada extensión.
 4. Abre [localhost:3000](http://localhost:3000) para ver los cambios después de seleccionar Asistente de Voz.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

### 🅱️ Ejecutar Área de Pruebas en Codespace(sin docker)

GitHub ofrece Codespace gratuito para cada repositorio, puedes ejecutar el área de pruebas en Codespace sin usar Docker. Además, la velocidad de Codespace es mucho más rápida que localhost.

[codespaces-shield]: <https://github.com/codespaces/badge.svg>
[![][codespaces-shield]](https://codespaces.new/ten-framework/ten-agent)

Consulta [esta guía](https://theten.ai/docs/ten_agent/setup_development_env/setting_up_development_inside_codespace) para más detalles.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

## 🛳️ Auto-hospedaje de TEN Agent

#### 🅰️ 🐳 Despliegue con Docker

Una vez que hayas personalizado tu agente (ya sea usando el TMAN Manager, el Área de Pruebas, o editando `property.json` directamente), puedes desplegarlo creando una imagen Docker de lanzamiento para tu servicio.

Lee la [Guía de Despliegue](https://theten.ai/docs/ten_agent/deploy_ten_agent/deploy_agent_service) para información detallada sobre el despliegue.

<br>

#### 🅱️ Despliegue con otros servicios en la nube

*próximamente*

<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

## 🌏 Ecosistema TEN Framework

| Proyecto | Vista Previa |
| ------- | ------- |
| [**🏚️ TEN Framework**][ten-framework-link]<br>TEN es un framework de código abierto para IA conversacional multimodal en tiempo real.<br><br>![][ten-framework-shield] | ![][ten-framework-banner] |
| [**️🔂 TEN Turn Detection**][ten-turn-detection-link]<br>TEN es para comunicación de diálogo full-duplex.<br><br>![][ten-turn-detection-shield] | ![][ten-turn-detection-banner] |
| [**🔉 TEN VAD**][ten-vad-link]<br>TEN VAD es un detector de actividad de voz (VAD) de transmisión de baja latencia, ligero y de alto rendimiento.<br><br>![][ten-vad-shield] | ![][ten-vad-banner] |
| [**🎙️ TEN Agent**][ten-agent-link]<br>TEN Agent es una demostración de TEN Framework.<br><br> | ![][ten-agent-banner] |
| **🎨 TMAN Designer** <br>TMAN Designer es una opción de bajo código/sin código para crear un agente de voz con una interfaz de usuario de flujo de trabajo fácil de usar.<br><br> | ![][tman-designer-banner] |
| [**📒 TEN Portal**][ten-portal-link]<br>El sitio oficial de TEN framework, tiene documentación y blog.<br><br>![][ten-portal-shield] | ![][ten-portal-banner] |

<br>
<div align="right">

[![][back-to-top]](#readme-top)

</div>

<br>

## ❓ Hacer Preguntas

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/TEN-framework/TEN-framework)

La mayoría de las preguntas pueden ser respondidas usando DeepWiki, es rápido, intuitivo de usar y soporta múltiples idiomas.

<div align="right">

[![][back-to-top]](#readme-top)

</div>

## 🥰 Contribuir

¡Bienvenimos todas las formas de colaboración de código abierto! Ya sea que estés corrigiendo errores, agregando características, mejorando la documentación o compartiendo ideas - tus contribuciones ayudan a avanzar en las herramientas de IA personalizadas. ¡Revisa nuestros Issues y Proyectos de GitHub para encontrar formas de contribuir y mostrar tus habilidades. Juntos, podemos construir algo increíble!

<br>

> \[!TIP]
>
> **Bienvenimos todo tipo de contribuciones** 🙏
>
> ¡Únete a nosotros para hacer TEN mejor! Cada contribución marca la diferencia, desde código hasta documentación. ¡Comparte tus proyectos de TEN Agent en redes sociales para inspirar a otros!
>
> Conéctate con uno de los mantenedores de TEN [@elliotchen100](https://x.com/elliotchen100) en 𝕏 o [@cyfyifanchen](https://github.com/cyfyifanchen) en GitHub para actualizaciones del proyecto, discusiones y oportunidades de colaboración.

<br>

### Contribuidores de Código

[![TEN](https://contrib.rocks/image?repo=TEN-framework/ten-agent)](https://github.com/TEN-framework/ten-agent/graphs/contributors)

### Guía de Contribución

¡Las contribuciones son bienvenidas! Por favor, lee primero las [guías de contribución](./docs/code-of-conduct/contributing.md).

### Licencia

1. Todo el framework TEN (excepto las carpetas explícitamente listadas a continuación) se publica bajo la Licencia Apache, Versión 2.0, con restricciones adicionales. Para más detalles, consulta el archivo [LICENSE](./LICENSE) ubicado en el directorio raíz del framework TEN.

2. Los componentes dentro del directorio `packages` se publican bajo la Licencia Apache, Versión 2.0. Para más detalles, consulta el archivo `LICENSE` ubicado en el directorio raíz de cada paquete.

3. Las bibliotecas de terceros utilizadas por el framework TEN están listadas y descritas en detalle. Para más información, consulta la carpeta [third_party](./third_party/).

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
