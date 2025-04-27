#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import json
import os
import sys
import importlib.util
from glob import glob
from typing import Callable, Dict, Type, Optional

from .addon import Addon
from libten_runtime_python import (
    _add_extension_addon_to_addon_manager,
    _register_addon_as_extension,
)


class _AddonManager:
    # Use the simple approach below, similar to a global array, to detect
    # whether a Python module provides the registration function required by the
    # TEN runtime. This avoids using `setattr` on the module, which may not be
    # supported in advanced environments like Cython. The global array method
    # is simple enough that it should work in all environments.
    _registry: Dict[str, Callable[[object], None]] = {}

    @classmethod
    def load_all_addons(cls):
        base_dir = cls._find_app_base_dir()

        # Read manifest.json under base_dir.
        manifest_path = os.path.join(base_dir, "manifest.json")
        if not os.path.isfile(manifest_path):
            raise FileNotFoundError("manifest.json not found in base_dir")

        with open(manifest_path, "r", encoding="utf-8") as f:
            manifest = json.load(f)

        # Note: The logic for loading extensions based on the `dependencies`
        # specified in the app's `manifest.json` is currently implemented
        # separately in both C and Python where addons need to be loaded. Since
        # the logic is fairly simple, a standalone implementation is directly
        # written at each required location. In the future, this could be
        # consolidated into a unified implementation in C, which could then be
        # reused across multiple languages. However, this would require handling
        # cross-language information exchange, which may not necessarily be
        # cost-effective.

        # Collect names of extensions from dependencies.
        extension_names = []
        dependencies = manifest.get("dependencies", [])
        for dep in dependencies:
            if dep.get("type") == "extension":
                extension_names.append(dep.get("name"))

        for module in glob(os.path.join(base_dir, "ten_packages/extension/*")):
            if os.path.isdir(module):
                module_name = os.path.basename(module)

                if module_name in extension_names:
                    cls._load_module(
                        module_full_name=(
                            f"ten_packages.extension.{module_name}"
                        ),
                        module_name=module_name,
                    )
                else:
                    print(f"Skipping module: {module_name}")

    @classmethod
    def _load_module(
        cls,
        module_full_name: str,
        module_name: str,
    ):
        try:
            spec = importlib.util.find_spec(module_full_name)
            if spec is None:
                raise ImportError(f"Cannot find module: {module_full_name}")

            _ = importlib.import_module(module_full_name)
            print(f"Imported module: {module_name}")

        except ImportError as e:
            print(f"Error importing module {module_name}: {e}")

    @classmethod
    def register_all_addons(cls, register_ctx: object):
        registry_keys = list(cls._registry.keys())

        for register_key in registry_keys:
            register_handler = cls._registry.get(register_key)
            if register_handler:
                try:
                    register_handler(register_ctx)

                    print(f"Successfully registered addon '{register_key}'")
                except Exception as e:
                    print(
                        (
                            "Error during registration of addon "
                            f"'{register_key}': {e}"
                        )
                    )

        cls._registry.clear()

    @classmethod
    def _register_addon(cls, addon_name: str, register_ctx: object):
        register_handler = cls._registry.get(addon_name, None)
        if register_handler:
            try:
                register_handler(register_ctx)
                print(f"Successfully registered addon '{addon_name}'")
            except Exception as e:
                print(f"Error during registration of addon '{addon_name}': {e}")
        else:
            print(f"No register handler found for addon '{addon_name}'")

    @staticmethod
    def _set_register_handler(
        addon_name: str,
        register_handler: Callable[[object], None],
    ) -> None:
        _AddonManager._registry[addon_name] = register_handler

    @staticmethod
    def _find_app_base_dir():
        current_dir = os.path.dirname(__file__)

        while current_dir != os.path.dirname(
            current_dir
        ):  # Stop at the root directory.
            manifest_path = os.path.join(current_dir, "manifest.json")
            if os.path.isfile(manifest_path):
                with open(
                    manifest_path, "r", encoding="utf-8"
                ) as manifest_file:
                    try:
                        manifest_data = json.load(manifest_file)
                        if manifest_data.get("type") == "app":
                            return current_dir
                    except json.JSONDecodeError:
                        pass
            current_dir = os.path.dirname(current_dir)

        raise FileNotFoundError(
            "App base directory with a valid manifest.json not found."
        )


def register_addon_as_extension(name: str, base_dir: Optional[str] = None):
    def decorator(cls: Type[Addon]) -> Type[Addon]:
        # Resolve base_dir.
        if base_dir is None:
            try:
                # Attempt to get the caller's file path using sys._getframe()
                caller_frame = sys._getframe(1)
                resolved_base_dir = os.path.dirname(
                    caller_frame.f_code.co_filename
                )
            except (AttributeError, ValueError):
                # Fallback in case sys._getframe() is not available or fails.
                # Example: in Cython or restricted environments.
                resolved_base_dir = None
        else:
            # If base_dir is provided, ensure it's the directory name
            resolved_base_dir = os.path.dirname(base_dir)

        # Define the register_handler that will be called by the Addon manager.
        def register_handler(register_ctx):
            # Instantiate the addon class.
            addon_instance = cls()

            try:
                _register_addon_as_extension(
                    name, resolved_base_dir, addon_instance, register_ctx
                )
            except Exception as e:
                print(f"Failed to register addon '{name}': {e}")

        # Define the registration function name based on the addon name.
        _AddonManager._set_register_handler(name, register_handler)

        # Add the addon to the native addon manager.
        _add_extension_addon_to_addon_manager(name)

        # Return the original class without modification.
        return cls

    return decorator
