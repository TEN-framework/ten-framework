---
title: tman CLI Reference
_portal_target: api/tman.md
---

TEN Manager (`tman`) is the command-line tool for TEN Framework. It handles package installation, app scaffolding, extension publishing, and running app-defined tasks.

```bash
tman --version
# TEN Framework version: <version>
```

## Commands

### `tman install`

Install TEN packages declared in `manifest.json`.

```bash
# Install all packages listed in manifest.json (run from inside the app directory)
cd transcriber_demo
tman install
```

To install a specific package by type and name:

```bash
# Install an extension from the cloud store into the current app
tman install extension azure_asr_python

# Install an app template
tman install app transcriber_demo
```

To install in standalone mode (for extension development and testing):

```bash
# Inside an extension directory — installs a self-contained .ten/app tree
cd my_example_ext_python
tman install --standalone
```

---

### `tman create`

Scaffold a new extension project from an official template.

**Python (async):**

```bash
tman create extension my_asr_python \
  --template default_async_extension_python \
  --template-data class_name_prefix=MyASR
```

**Go:**

```bash
tman create extension my_vad_go \
  --template default_extension_go \
  --template-data class_name_prefix=MyVAD
```

**C++:**

```bash
tman create extension my_codec_cpp \
  --template default_extension_cpp
```

**Node.js:**

```bash
tman create extension my_ui_nodejs \
  --template default_extension_nodejs \
  --template-data class_name_prefix=MyUI
```

After creation the project directory contains `manifest.json`, `property.json`, source files, and a `tests/` skeleton ready for `tman run test`.

---

### `tman run`

Execute a task defined in the app's `manifest.json` `scripts` section.

Common built-in tasks:

| Task | Description |
|------|-------------|
| `tman run build` | Build the app (compiles C++/Go, bundles TypeScript) |
| `tman run install_deps` | Install Python pip and npm dependencies |
| `tman run start` | Start the TEN app |
| `tman run test` | Run the extension's test suite |

Examples:

```bash
# Inside transcriber_demo
tman run build
tman run install_deps
tman run start

# Inside an extension directory after tman install --standalone
tman run test
```

---

### `tman publish`

Publish an extension to the TEN cloud store (requires a registered account).

```bash
# Inside the extension directory
cd azure_asr_python
tman publish
```

The command reads `manifest.json` for the extension name, version, and package metadata. Make sure `manifest.json` is accurate and `package.include` lists all files to bundle before publishing.

---

### `tman designer`

Launch the TMAN Designer — a browser-based UI for exploring cloud-store extensions, downloading them, and visually wiring app graphs.

```bash
tman designer
# Opens http://localhost:49483 by default
```

---

## Configuration Files

Every TEN package has two JSON files that `tman` reads and writes.

### `manifest.json`

Declares the package identity, dependencies, and API contract.

```json
{
  "type": "extension",
  "name": "azure_asr_python",
  "version": "0.3.1",
  "dependencies": [
    { "type": "system", "name": "ten_runtime_python", "version": "0.11" },
    { "type": "system", "name": "ten_ai_base", "version": "0.7" }
  ],
  "package": {
    "include": [
      "manifest.json",
      "property.json",
      "**.py",
      "README.md",
      "requirements.txt"
    ]
  },
  "api": {
    "interface": [
      { "import_uri": "../../system/ten_ai_base/api/asr-interface.json" }
    ],
    "property": {
      "properties": {
        "params": {
          "type": "object",
          "properties": {
            "key":    { "type": "string" },
            "region": { "type": "string" }
          }
        }
      }
    }
  }
}
```

### `property.json`

Provides default runtime property values for the extension. These values are overridden by the graph's node configuration at runtime.

```json
{
  "params": {
    "key":      "${env:AZURE_STT_KEY|}",
    "region":   "${env:AZURE_STT_REGION|}",
    "language": "en-US",
    "sample_rate": 16000
  }
}
```

The `${env:VAR_NAME|}` syntax reads an environment variable at startup. The trailing `|` means the value is optional (empty string if the variable is not set); omitting `|` makes it required.

---

## Practical Workflows

### Adding an extension to an existing app

```bash
cd transcriber_demo

# 1. Install the extension from the cloud store
tman install extension deepgram_asr_python

# 2. Re-run install_deps to pick up any new Python packages
tman run install_deps

# 3. Wire the extension into your graph in property.json, then rebuild
tman run build
```

### Developing and testing an extension in isolation

```bash
# Create from template
tman create extension my_tts_python \
  --template default_async_extension_python \
  --template-data class_name_prefix=MyTTS

cd my_tts_python

# Install standalone runtime
tman install --standalone

# Implement the extension, then run its tests
tman run test
```

### Publishing a new extension version

```bash
cd my_tts_python

# Bump the version in manifest.json, then publish
tman publish
```
