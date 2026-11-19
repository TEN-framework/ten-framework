#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
# Importing addon registers the extension. The TEN runtime is present in
# every real app/test-runner environment (the addon loader fails loudly if
# registration is missing there); bare unit-test environments without the
# runtime still need the package importable for the logic tests.
try:
    from . import addon
except ModuleNotFoundError:
    pass
