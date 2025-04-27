#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import argparse
import errno
import os
import shutil
import sys


def Main():
    parser = argparse.ArgumentParser(
        description="Create Mac Framework symlinks"
    )
    parser.add_argument("--framework", action="store", type=str, required=True)
    parser.add_argument("--version", action="store", type=str)
    parser.add_argument("--contents", action="store", type=str, nargs="+")
    parser.add_argument("--stamp", action="store", type=str, required=True)
    parser.add_argument("--headers", action="store", required=False)
    args = parser.parse_args()

    VERSIONS = "Versions"
    CURRENT = "Current"
    HEADERS = "Headers"
    MODULES = "Modules"

    # Ensure the Foo.framework/Versions/A/ directory exists and create the
    # Foo.framework/Versions/Current symlink to it.
    if args.version:
        target_dirs = [
            os.path.join(args.framework, VERSIONS, args.version),
            os.path.join(args.framework, VERSIONS, args.version, HEADERS),
            os.path.join(args.framework, VERSIONS, args.version, MODULES),
        ]
        for target_dir in target_dirs:
            try:
                if os.path.isfile(target_dir):
                    os.remove(target_dir)
                os.makedirs(target_dir)
            except OSError as e:
                if e.errno != errno.EEXIST:
                    raise e
        _Relink(
            os.path.join(args.version),
            os.path.join(args.framework, VERSIONS, CURRENT),
        )

    # Establish the top-level symlinks in the framework bundle. The dest of
    # the symlinks may not exist yet.
    if args.contents:
        for item in args.contents:
            _Relink(
                os.path.join(VERSIONS, CURRENT, item),
                os.path.join(args.framework, item),
            )

    # Write out a stamp file.
    if args.stamp:
        with open(args.stamp, "w", encoding="utf-8") as f:
            f.write(str(args))

    # the source must be absolute path
    if args.headers:
        raw_data = str(args.headers).split(",")
        for header in raw_data:
            shutil.copy(
                header, os.path.join(args.framework, VERSIONS, CURRENT, HEADERS)
            )
        _Relink(
            os.path.join(VERSIONS, CURRENT, HEADERS),
            os.path.join(args.framework, HEADERS),
        )
        _Relink(
            os.path.join(VERSIONS, CURRENT, MODULES),
            os.path.join(args.framework, MODULES),
        )
        # add to modulemap
        module_path = os.path.join(args.framework, MODULES)
        header_path = os.path.join(args.framework, HEADERS)
        binary = os.path.basename(args.framework).split(".")[0]
        module_template = "framework module %s {\n" % (binary)
        umbrella_header = "%s.h" % (binary)
        umbrella_header_path = os.path.join(header_path, umbrella_header)
        if os.path.exists(umbrella_header_path):
            module_template += '  umbrella header "%s"\n' % (umbrella_header)
        module_template += "  export *\n"
        module_template += "}\n"
        with open(
            os.path.join(module_path, "module.modulemap"), "w", encoding="utf-8"
        ) as module_file:
            module_file.write(module_template)
    return 0


def _Relink(dest, link):
    """Creates a symlink to |dest| named |link|. If |link| already exists,
    it is overwritten."""
    try:
        os.remove(link)
    except OSError as e:
        if e.errno != errno.ENOENT:
            shutil.rmtree(link)
    os.symlink(dest, link)


if __name__ == "__main__":
    sys.exit(Main())
