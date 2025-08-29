#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import os
import sys
from typing import Callable

# Internal APIs from libten_runtime_python - these are private by design and
# only intended for use within ten-framework's Python binding layer.
from libten_runtime_python import (
    _ten_py_addon_manager_add_extension_addon,  # pyright: ignore[reportPrivateUsage] # noqa: E501
    _ten_py_addon_manager_register_addon_as_extension,  # pyright: ignore[reportPrivateUsage] # noqa: E501
)

from .addon import Addon


class _AddonManager:
    # Use the simple approach below, similar to a global array, to detect
    # whether a Python module provides the registration function required by the
    # TEN runtime. This avoids using `setattr` on the module, which may not be
    # supported in advanced environments like Cython. The global array method
    # is simple enough that it should work in all environments.
    _registry: dict[str, Callable[[object], None]] = {}
    _registered_addons: set[str] = set()

    @classmethod
    def register_all_addons(cls, register_ctx: object):
        registry_keys = list(cls._registry.keys())

        for register_key in registry_keys:
            register_handler = cls._registry.get(register_key)
            if register_handler:
                try:
                    # Check if the addon is already registered.
                    if register_key in cls._registered_addons:
                        print(
                            (
                                f"Addon '{register_key}' has already been "
                                "registered, skipping registration."
                            )
                        )
                        continue

                    # Call the register handler.
                    register_handler(register_ctx)

                    print(f"Successfully registered addon '{register_key}'")

                    # Mark the addon as registered.
                    cls._registered_addons.add(register_key)
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
                # Check if the addon is already registered.
                if addon_name in cls._registered_addons:
                    print(
                        (
                            f"Addon '{addon_name}' has already been "
                            "registered, skipping registration."
                        )
                    )
                    return

                # Call the register handler.
                register_handler(register_ctx)

                print(f"Successfully registered addon '{addon_name}'")

                # Mark the addon as registered.
                cls._registered_addons.add(addon_name)
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


def register_addon_as_extension(name: str, base_dir: str | None = None):
    def decorator(cls: type[Addon]) -> type[Addon]:
        # Resolve base_dir.
        if base_dir is None:
            try:
                # Attempt to get the caller's file path using sys._getframe()
                caller_frame = sys._getframe(  # pyright: ignore[reportPrivateUsage] # noqa: E501
                    1
                )
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
        def register_handler(register_ctx: object):
            # Instantiate the addon class.
            addon_instance = cls()

            try:
                _ten_py_addon_manager_register_addon_as_extension(
                    name, resolved_base_dir, addon_instance, register_ctx
                )
            except Exception as e:
                print(f"Failed to register addon '{name}': {e}")

        # Define the registration function name based on the addon name.
        _AddonManager._set_register_handler(  # pyright: ignore[reportPrivateUsage] # noqa: E501
            name, register_handler
        )

        # Add the addon to the native addon manager.
        _ten_py_addon_manager_add_extension_addon(name)

        # Return the original class without modification.
        return cls

    return decorator
