#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import ast
import os
import argparse


class GetPropertySingleVarVisitor(ast.NodeVisitor):
    def __init__(self, filename: str) -> None:
        self.filename: str = filename
        self.matches: list[dict[str, object]] = []

    def visit_Assign(self, node: ast.Assign) -> None:  # type: ignore[override]
        # Only handle single-variable assignment (not tuple unpacking)
        if len(node.targets) == 1 and isinstance(node.targets[0], ast.Name):
            value = node.value
            if isinstance(value, ast.Await):
                value = value.value
            if isinstance(value, ast.Call):
                func = value.func
                if (
                    isinstance(func, ast.Attribute)
                    and isinstance(func.value, ast.Name)
                    and func.value.id == "ten_env"
                    and func.attr.startswith("get_property_")
                ):
                    self.matches.append(
                        {
                            "filename": self.filename,
                            "lineno": node.lineno,
                            "var": node.targets[0].id,
                            "func": func.attr,
                            "args": [ast.unparse(arg) for arg in value.args],
                        }
                    )
        self.generic_visit(node)


def find_py_files(root_folder: str) -> list[str]:
    py_files: list[str] = []
    for root, _, files in os.walk(root_folder):
        for fname in files:
            if fname.endswith(".py"):
                py_files.append(os.path.join(root, fname))
    return py_files


def main() -> None:
    parser = argparse.ArgumentParser(
        description=(
            "Find single-variable ten_env.get_property_xxx assignments "
            "in Python code."
        )
    )
    _ = parser.add_argument("folder", help="Target folder to recursively scan")
    args = parser.parse_args()

    py_files: list[str] = find_py_files(args.folder)
    total_matches: list[dict[str, object]] = []

    for pyfile in py_files:
        try:
            with open(pyfile, "r", encoding="utf-8") as f:
                code: str = f.read()
            tree = ast.parse(code)
            visitor = GetPropertySingleVarVisitor(pyfile)
            visitor.visit(tree)
            total_matches.extend(visitor.matches)
        except Exception as e:
            print(f"Failed to parse {pyfile}: {e}")

    for m in total_matches:
        print(
            f"{m['filename']}:{m['lineno']}: {m['var']} = [await] "
            f"ten_env.{m['func']}({', '.join(m['args'])})"
        )

    if not total_matches:
        print("No matches found.")


if __name__ == "__main__":
    main()
