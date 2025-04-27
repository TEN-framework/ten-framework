#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import os
import shutil
import sys

# Ensures that the current version matches the last-produced version, which is
# stored in the version_file. If it does not, then the framework_root_dir is
# obliterated.
# Usage: python prepare_framework_version.py out/obj/version_file \
#                                            out/Framework.framework \
#                                            'A'


def PrepareFrameworkVersion(version_file, framework_root_dir, version):
    # Test what the current framework version is. Stop if it is up-to-date.
    try:
        with open(version_file, "r", encoding="utf-8") as f:
            current_version = f.read()
            if current_version == version:
                return
    except IOError:
        pass

    # The framework version has changed, so clobber the framework.
    if os.path.exists(framework_root_dir):
        shutil.rmtree(framework_root_dir)

    # Write out the new framework version file, making sure its containing
    # directory exists.
    dirname = os.path.dirname(version_file)
    if not os.path.isdir(dirname):
        os.makedirs(dirname)

    with open(version_file, "w+", encoding="utf-8") as f:
        f.write(version)


if __name__ == "__main__":
    PrepareFrameworkVersion(sys.argv[1], sys.argv[2], sys.argv[3])
    sys.exit(0)
