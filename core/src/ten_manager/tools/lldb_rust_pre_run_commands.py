#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#

# These commands need to be loaded into LLDB before starting Rust debugging for
# LLDB to perform Rust debugging more effectively.

import pathlib
import subprocess
import lldb

rustc_sysroot = subprocess.getoutput("rustc --print sysroot")
rustlib_etc = pathlib.Path(rustc_sysroot) / "lib" / "rustlib" / "etc"

lldb.debugger.HandleCommand(
    f'command script import "{rustlib_etc / "lldb_lookup.py"}"'
)
lldb.debugger.HandleCommand(
    f'command source -s 0 "{rustlib_etc / "lldb_commands"}"'
)
