---
name: use_tman
description: Comprehensive guide for using the TMAN CLI to manage the TEN framework, including creating, installing, packaging, and publishing projects.
---

# Use TMAN CLI

The `tman` (TEN Manager) CLI is the central tool for developing with the TEN framework. It handles project creation, dependency management, building, packaging, and interacting with the TEN registry.

## Global Options
These options apply to most commands:
*   `--verbose`: Enable verbose output for debugging.
*   `-y, --yes`: Automatically answer 'yes' to all prompts (useful for automation).
*   `-c, --config-file <FILE>`: Specify a custom configuration file.
*   `--user-token <TOKEN>`: Provide a user token for registry authentication.

## 1. Project Creation (`create`)
Create new packages, extensions, or applications based on templates.

**Syntax:**
```bash
tman create <PACKAGE_TYPE> <PACKAGE_NAME> --template <TEMPLATE> [OPTIONS]
```

**Arguments:**
*   `PACKAGE_TYPE`: One of `system`, `protocol`, `addon_loader`, `extension`, `app`.
*   `PACKAGE_NAME`: Name of the package (e.g., `my_extension` or `my_package@1.0.0`).
*   `--template <TEMPLATE>`: (Required) Template to use.
*   `--template-data <KEY=VALUE>`: (Optional) Override template placeholders. Use `class_name_prefix=MyPrefix` to customize the class name.
*   `--os <OS>`: Override OS (linux, mac, win).
*   `--arch <ARCH>`: Override CPU architecture (x86, x64, arm, arm64).

**Extension Creation Examples:**

You can create extensions in various languages. Use `--template-data class_name_prefix=...` to set the specific class name for your extension.

*   **C++ Extension:**
    ```bash
    tman create extension my_cpp_ext --template default_extension_cpp --template-data class_name_prefix=MyCpp
    ```

*   **Go Extension:**
    ```bash
    tman create extension my_go_ext --template default_extension_go --template-data class_name_prefix=MyGo
    ```

*   **Python Extension (Async):**
    ```bash
    tman create extension my_py_ext --template default_async_extension_python --template-data class_name_prefix=MyPy
    ```

*   **Node.js Extension:**
    ```bash
    tman create extension my_node_ext --template default_extension_nodejs --template-data class_name_prefix=MyNode
    ```

*   **ASR Extension (Python):**
    ```bash
    tman create extension my_asr_ext --template default_asr_python --template-data class_name_prefix=MyASR
    ```

*   **TTS Extension (Python):**
    ```bash
    tman create extension my_tts_ext --template default_tts_python --template-data class_name_prefix=MyTTS
    ```

*   **LLM Extension (Python):**
    ```bash
    tman create extension my_llm_ext --template default_llm_python --template-data class_name_prefix=MyLLM
    ```

**App Creation Examples:**

Similar to extensions, you can specify the class name prefix. **Note:** We recommend using the **Go** template (`default_app_go`) for applications.

*   **Go App (Recommended):**
    ```bash
    tman create app my_app --template default_app_go --template-data class_name_prefix=MyApp
    ```

*   **C++ App:**
    ```bash
    tman create app my_cpp_app --template default_app_cpp --template-data class_name_prefix=MyCppApp
    ```

*   **Python App:**
    ```bash
    tman create app my_py_app --template default_app_python --template-data class_name_prefix=MyPyApp
    ```

## 2. Dependency Management (`install` / `uninstall`)

### `install`
Installs dependencies defined in `manifest.json`.

**Syntax:**
```bash
tman install [PACKAGE_TYPE] [PACKAGE_NAME] [OPTIONS]
```

**Usage Modes:**
1.  **Install all dependencies (Project root):**
    ```bash
    # Run inside a project with manifest.json
    tman install
    ```
2.  **Install a specific package:**
    ```bash
    tman install extension some_extension@1.0.0
    ```
3.  **Install from local path:**
    ```bash
    tman install ./path/to/local/package
    ```
4.  **Standalone Mode (for Extensions):**
    Prepares a wrapper app to run an extension independently.
    ```bash
    # Inside extension directory
    tman install --standalone
    ```

**Options:**
*   `--standalone`: Install in standalone mode (only for extensions).
*   `--production`: Ignore `dev_dependencies`.
*   `--locked`: Install strictly from `manifest-lock.json` versions/hashes.
*   `--local-install-mode <MODE>`: `link` (symlink, default) or `copy`.
*   `-C, --cwd <DIR>`: Run in a different directory.

### `uninstall`
Removes a package.

**Syntax:**
```bash
tman uninstall <PACKAGE_TYPE> <PACKAGE_NAME>
```

## 3. Download Packages (`fetch`)

### `fetch`
Downloads a package from the registry.

**Syntax:**
```bash
tman fetch <PACKAGE_TYPE> <PACKAGE_NAME> [OPTIONS]
```
*   **Default:** Downloads and extracts to the current directory (or `--output-dir`).
*   `--no-extract`: Only download the `.tpkg` file without extracting.
*   `--output-dir <DIR>`: Target directory.

## 4. Execution & Scripts (`run`)
Run custom scripts defined in the `scripts` section of `manifest.json`.

**Syntax:**
```bash
tman run <SCRIPT_NAME> [-- <EXTRA_ARGS>...]
```

**Example:**
If `manifest.json` has `"scripts": { "start": "python main.py" }`:
```bash
tman run start
# With arguments
tman run start -- --port 8080
```

## 5. Development Tools

### `check`
Perform consistency validations.
*   `tman check env`: Check environment consistency.
*   `tman check interface-json`: Validate `interface.json`.
*   `tman check manifest-json`: Validate `manifest.json`.
*   `tman check property-json`: Validate `property.json`.

### `designer`
Launch the visual designer web interface.
*   `tman designer`: Starts the server (default port ~49483).
*   `--port <PORT>`: Specify port.
*   `--base-dir <DIR>`: Open a specific project directory.