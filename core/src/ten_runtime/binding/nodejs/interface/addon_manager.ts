//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import * as fs from "fs";
import * as path from "path";
import { dirname } from "path";
import { fileURLToPath } from "url";

import type { Addon } from "./addon.js";
import ten_addon from "./ten_addon.js";

type Ctor<T> = {
  new (): T;
  prototype: T;
};

type addonRegisterHandler = (registerContext: unknown) => void;

export class AddonManager {
  private static _instance: AddonManager | undefined = undefined;
  private _registry: Map<string, addonRegisterHandler> = new Map();
  private _registeredAddons: Set<string> = new Set();

  // Make the constructor private to prevent direct instantiation.
  private constructor() {
    ten_addon.ten_nodejs_addon_manager_create(this);
  }

  private static findAppBaseDir(): string {
    let currentDir = dirname(fileURLToPath(import.meta.url));

    while (currentDir !== path.dirname(currentDir)) {
      const manifestPath = path.join(currentDir, "manifest.json");
      if (fs.existsSync(manifestPath)) {
        try {
          const manifestJson = JSON.parse(
            fs.readFileSync(manifestPath, "utf-8"),
          );
          if (manifestJson.type === "app") {
            return currentDir;
          }
          if (manifestJson.type === "extension") {
            // In Node.js, it's basically not easy to get the symbolic link path
            // of an imported module, and we always get the real path, so we
            // need special handling here. If the extension is in standalone
            // building mode, then the app base dir is the .ten/app directory.
            const standaloneBuildingAppDir = path.join(currentDir, ".ten/app");
            const standaloneBuildingAppManifestPath = path.join(
              standaloneBuildingAppDir,
              "manifest.json",
            );

            if (
              fs.existsSync(standaloneBuildingAppDir) &&
              fs.existsSync(standaloneBuildingAppManifestPath)
            ) {
              return standaloneBuildingAppDir;
            }
          }
          // eslint-disable-next-line @typescript-eslint/no-unused-vars
        } catch (error) {
          // Ignore
        }
      }

      currentDir = path.dirname(currentDir);
    }

    throw new Error("Cannot find app base dir");
  }

  static getInstance(): AddonManager {
    // Because this function is called in the JS main thread, so it's thread
    // safe.
    if (!AddonManager._instance) {
      AddonManager._instance = new AddonManager();
    }
    return AddonManager._instance;
  }

  setRegisterHandler(name: string, handler: addonRegisterHandler): void {
    this._registry.set(name, handler);
  }

  async loadAllAddons(): Promise<void> {
    const app_base_dir = AddonManager.findAppBaseDir();

    console.log(`app_base_dir: ${app_base_dir}`);

    const extension_folder = path.join(app_base_dir, "ten_packages/extension");
    if (!fs.existsSync(extension_folder)) {
      return;
    }

    const dirs = fs.opendirSync(extension_folder);
    const loadPromises = [];

    for (;;) {
      const entry = dirs.readSync();
      if (!entry) {
        break;
      }

      if (entry.name.startsWith(".")) {
        continue;
      }

      const packageJsonFile = `${extension_folder}/${entry.name}/package.json`;

      if (fs.existsSync(packageJsonFile)) {
        // Log the extension name.
        console.log(`_load_all_addons Loading extension ${entry.name}`);
        loadPromises.push(
          import(`${extension_folder}/${entry.name}/build/index.js`),
        );
      }
    }

    // Wait for all modules to be loaded.
    await Promise.all(loadPromises);
    console.log(`_load_all_addons loaded ${loadPromises.length} extensions`);
  }

  async loadSingleAddon(name: string): Promise<boolean> {
    const app_base_dir = AddonManager.findAppBaseDir();

    const extension_folder = path.join(
      app_base_dir,
      "ten_packages/extension",
      name,
    );
    if (!fs.existsSync(extension_folder)) {
      console.log(`Addon ${name} directory not found in ${extension_folder}`);
      return false;
    }

    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    const _dirs = fs.opendirSync(extension_folder);
    const packageJsonFile = `${extension_folder}/package.json`;
    if (!fs.existsSync(packageJsonFile)) {
      console.log(
        `Addon ${name} package.json not found in ${extension_folder}`,
      );
      return false;
    }

    try {
      await import(`${extension_folder}/build/index.js`);
      console.log(`Addon ${name} loaded`);
      return true;
    } catch (error) {
      console.error(`Failed to load addon ${name}: ${error}`);
      return false;
    }
  }

  deinit(): void {
    ten_addon.ten_nodejs_addon_manager_deinit(this);
  }

  // This function will be called from C code.
  private registerSingleAddon(name: string, registerContext: unknown): void {
    // Check if the addon is already registered.
    if (this._registeredAddons.has(name)) {
      console.log(
        `Addon ${name} has already been registered, skipping registration.`,
      );
      return;
    }

    const handler = this._registry.get(name);
    if (handler) {
      try {
        // Call the register handler.
        handler(registerContext);

        console.log(`Addon ${name} registered`);

        // Mark the addon as registered.
        this._registeredAddons.add(name);
      } catch (error) {
        console.error(`Failed to register addon ${name}: ${error}`);
      }
    } else {
      console.log(`Failed to find the register handler for addon ${name}`);
    }
  }
}

export function RegisterAddonAsExtension(
  name: string,
): <T extends Ctor<Addon>>(klass: T) => T {
  return <T extends Ctor<Addon>>(klass: T): T => {
    function registerHandler(registerContext: unknown) {
      const addon_instance = new klass();

      ten_addon.ten_nodejs_addon_manager_register_addon_as_extension(
        name,
        addon_instance,
        registerContext,
      );
    }

    const addonManager = AddonManager.getInstance();
    addonManager.setRegisterHandler(name, registerHandler);

    // Register the addon to the native addon manager.
    ten_addon.ten_nodejs_addon_manager_add_extension_addon(addonManager, name);

    console.log(`RegisterAddonAsExtension ${name} registered`);

    return klass;
  };
}
