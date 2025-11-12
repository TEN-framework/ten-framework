#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import stat
import os
import shutil
import subprocess
import inspect
from . import log


def remove_readonly(func, path, excinfo):
    if not os.access(path, os.W_OK):
        os.chmod(path, stat.S_IWUSR)
        func(path)
    else:
        raise excinfo[1]


def remove_tree(path: str) -> None:
    if os.path.exists(path):
        shutil.rmtree(path, onerror=remove_readonly)


def copy_tree(src_path: str, dst_path: str, rm_dst=False) -> None:
    if not os.path.exists(src_path):
        raise FileNotFoundError(src_path + " not exist")

    if not os.path.isdir(src_path):
        raise NotADirectoryError(src_path + " is not a directory.")

    # Check if dst_path exists and is a directory.
    if not os.path.exists(dst_path):
        try:
            os.makedirs(dst_path)
        except Exception as exc:
            raise OSError(
                inspect.cleandoc(
                    f"""Failed to create destination directory:
                    {dst_path}
                    Exception: {exc}"""
                )
            ) from exc
    elif not os.path.isdir(dst_path):
        raise NotADirectoryError(
            f"Destination path '{dst_path}' exists and is not a directory."
        )

    try:
        if rm_dst:
            remove_tree(dst_path)
            # shutil.copytree requires the destination folder is empty,
            # otherwise, this function will throw exceptions. So use it
            # in this case.
            shutil.copytree(
                src_path,
                dst_path,
                symlinks=True,
                copy_function=shutil.copy,
            )
        # Loop all immediate items in src_path, and copy them one by
        # one.
        for name in os.listdir(src_path):
            src_item_path = os.path.join(src_path, name)
            dest_item_path = os.path.join(dst_path, name)
            if os.path.isdir(src_item_path):
                # Create the destination folder if not exist.
                if not os.path.exists(dest_item_path):
                    os.makedirs(dest_item_path, exist_ok=True)
                copy_tree(src_item_path, dest_item_path)
            else:
                copy_file(src_item_path, dest_item_path, rm_dst=rm_dst)
    except Exception as exc:
        log.error(
            inspect.cleandoc(
                f"""Failed to copy_tree:
                {src_path} =>
                {dst_path}
                Exception: {exc}
                rm_dst: {rm_dst}"""
            )
        )
        exit(1)

    try:
        # Update the file access and modify time to the current time.
        os.utime(dst_path, follow_symlinks=False)
    except Exception:
        try:
            # If follow_symlinks parameter is not supported, fall back to
            # default behavior
            os.utime(dst_path)
        except Exception as exc:
            log.error(
                inspect.cleandoc(
                    f"""Failed to update utime after copy_tree:
                {src_path} =>
                {dst_path}
                Exception: {exc}
                rm_dst: {rm_dst}"""
                )
            )
            exit(1)


def copy_file(src_file: str, dst_file: str, rm_dst=False) -> None:
    if not os.path.exists(src_file):
        raise FileNotFoundError(f"{src_file} not exist")

    if os.path.isdir(src_file):
        raise IsADirectoryError(f"{src_file} is a directory.")

    src_file = os.path.abspath(src_file)
    dst_file = os.path.abspath(dst_file)

    try:
        os.makedirs(os.path.dirname(dst_file), exist_ok=True)
    except Exception as exc:
        log.error(
            inspect.cleandoc(
                f"""Failed to create destination folder before copy_file:
                {src_file} =>
                {dst_file}
                Exception: {exc}
                rm_dst: {rm_dst}"""
            )
        )
        exit(1)

    try:
        if rm_dst and os.path.exists(dst_file):
            os.remove(dst_file)
    except Exception as exc:
        log.error(
            inspect.cleandoc(
                f"""Failed to delete dst_file before copy_file:
                {src_file} =>
                {dst_file}
                Exception: {exc}
                rm_dst: {rm_dst}"""
            )
        )
        exit(1)

    try:
        if os.path.islink(src_file):
            subprocess.call(["cp", "-a", src_file, dst_file])
        else:
            # Ensure the destination file is writable, so that the copying
            # operation would not be fail due to this.
            if os.path.exists(dst_file):
                st = os.stat(dst_file)
                os.chmod(dst_file, st.st_mode | stat.S_IWUSR)
            shutil.copy(src_file, dst_file)
    except Exception as exc:
        log.error(
            inspect.cleandoc(
                f"""Failed to copy_file:
                                   {src_file} =>
                                   {dst_file}
                                   Exception: {exc}
                                   rm_dst: {rm_dst}"""
            )
        )
        exit(1)

    try:
        # Update the file access and modify time to the current time.
        os.utime(dst_file, follow_symlinks=False)
    except Exception:
        try:
            # If follow_symlinks parameter is not supported, fall back to
            # default behavior
            os.utime(dst_file)
        except Exception as exc:
            log.error(
                inspect.cleandoc(
                    f"""Failed to copy_file:
                                       {src_file} =>
                                       {dst_file}
                                       Exception: {exc}
                                       rm_dst: {rm_dst}"""
                )
            )
            exit(1)


def copy(src: str, dst: str, rm_dst=False) -> None:
    if os.path.exists(src):
        # Perform copying.
        if os.path.isdir(src):
            copy_tree(src, dst, rm_dst)
        else:
            copy_file(src, dst, rm_dst)
    else:
        raise FileNotFoundError(f"{src} does not exist.")
