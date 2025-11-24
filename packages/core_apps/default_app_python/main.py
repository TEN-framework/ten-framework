#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

# On Windows, we need to add DLL search directories before importing ten_runtime.
# This is required because Windows 8+ does not use PATH for DLL search.
# The DLL directory must be added in the same process that imports the module.
import os
import sys

if sys.platform == "win32" and hasattr(os, 'add_dll_directory'):
    # Get the app root directory (parent of the directory containing this script)
    script_dir = os.path.dirname(os.path.abspath(__file__))
    app_root = script_dir

    # Add ALL possible DLL locations (don't break - we need both!)
    # Priority order: out/win/x64 first, then app internal
    dll_search_dirs = [
        os.path.abspath(os.path.join(app_root, '../../../../../..')),  # out/win/x64
        os.path.join(app_root, 'ten_packages', 'system', 'ten_runtime', 'lib'),  # app internal
    ]

    for dll_dir in dll_search_dirs:
        if os.path.exists(dll_dir):
            try:
                os.add_dll_directory(dll_dir)
            except Exception:
                pass  # Ignore errors (e.g., duplicate directory)

from ten_runtime import App, TenEnv, LogLevel


class DefaultApp(App):

    def on_init(self, ten_env: TenEnv):
        ten_env.log(LogLevel.DEBUG, "on_init")
        ten_env.on_init_done()

    def on_deinit(self, ten_env: TenEnv) -> None:
        ten_env.log(LogLevel.DEBUG, "on_deinit")
        ten_env.on_deinit_done()


if __name__ == "__main__":

    app = DefaultApp()
    print("app created.")

    app.run(False)
    print("app run completed.")
