import argparse
import json
import os
import sys


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.root: str
        self.output: str
        self.index_url: str


def install_pip_tools_if_needed(index_url: str) -> bool:
    def is_package_installed(package_name):
        import importlib.metadata

        try:
            importlib.metadata.distribution(package_name)
            return True
        except importlib.metadata.PackageNotFoundError:
            return False

    if is_package_installed("pip-tools"):
        print("pip-tools is already installed")
        return True
    else:
        import subprocess

        print("pip-tools is not installed. Installing pip-tools...")

        args = [sys.executable, "-m", "pip", "install", "pip-tools"]

        if index_url is not None and index_url != "":
            args.extend(["-i", index_url])

        try:
            subprocess.check_call(args)
        except subprocess.CalledProcessError as e:
            print(f"Failed to install pip-tools: {e}")
            return False

        print("pip-tools is installed")
        return True


class DepsManager:

    def __collect_requirements_files(self) -> list[str]:
        # Read manifest.json under self.root.
        manifest_path = os.path.join(self.root, "manifest.json")
        if not os.path.isfile(manifest_path):
            raise FileNotFoundError("manifest.json not found in root directory")

        with open(manifest_path, "r", encoding="utf-8") as f:
            manifest = json.load(f)

        # Collect names of extensions from dependencies.
        extension_names = []
        dependencies = manifest.get("dependencies", [])
        for dep in dependencies:
            if dep.get("type") == "extension":
                extension_names.append(dep.get("name"))

        source_dir = os.path.join(self.root, "ten_packages", "extension")

        result = []

        for root, _, files in os.walk(source_dir):
            # Get the extension folder name relative to 'source_dir'.
            rel_path = os.path.relpath(root, source_dir)
            # Skip subdirectories (if any).
            if os.path.sep in rel_path:
                continue
            extension_name = rel_path

            if extension_name not in extension_names:
                continue

            if "requirements.txt" in files:
                source_file = os.path.relpath(
                    os.path.join(root, "requirements.txt"), self.root
                )

                result.append(source_file)

        # Include the requirements.txt file in the root directory.
        if os.path.exists(os.path.join(self.root, "requirements.txt")):
            result.append("requirements.txt")

        return result

    def __generate_requirements_in(self, requirements_files: list[str]):
        # If the file already exists, remove it.
        if os.path.exists(self.requirements_in_file):
            os.remove(self.requirements_in_file)

        # Create 'requirements.in' file under root dir.
        with open(self.requirements_in_file, "w", encoding="utf-8") as f:
            for file in requirements_files:
                f.write(f"-r {file}\n")

    def __delete_requirements_in(self):
        # If the file already exists, remove it.
        if os.path.exists(self.requirements_in_file):
            os.remove(self.requirements_in_file)

    def __init__(self, root: str, index_url: str):
        self.root = root
        self.index_url = index_url
        self.requirements_in_file = os.path.join(self.root, "requirements.in")
        self.has_deps = True

    def __enter__(self):
        # Collect 'requirements.txt' files in the ten_packages/extension/*
        # directory.
        dep_file_list = self.__collect_requirements_files()

        if len(dep_file_list) == 0:
            print("No requirements files found")
            self.has_deps = False
            return self

        # Generate 'requirements.in' file from the collected files into the root
        # dir.
        self.__generate_requirements_in(dep_file_list)

        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.__delete_requirements_in()

    def pip_compile(self, output: str):
        if self.has_deps is False:
            print("No requirements.in file generated, skipping pip-compile")
            return True

        import subprocess

        args = [
            "pip-compile",
            self.requirements_in_file,
            "--output-file",
            output,
        ]

        if self.index_url is not None and self.index_url != "":
            args.extend(["-i", self.index_url])

        try:
            subprocess.check_call(args)
        except subprocess.CalledProcessError as e:
            print(f"Failed to pip-compile: {e}")
            return False

        # Check if the output file exists.
        if not os.path.exists(output):
            print(f"Failed to generate {output}")
            return False

        return True


if __name__ == "__main__":
    file_dir = os.path.dirname(os.path.abspath(__file__))
    app_root_dir = os.path.abspath(
        os.path.join(file_dir, "..", "..", "..", "..")
    )

    parser = argparse.ArgumentParser(description="Resolve Python dependencies.")
    parser.add_argument(
        "-r", "--root", type=str, required=False, default=app_root_dir
    )
    parser.add_argument(
        "-o",
        "--output",
        type=str,
        required=False,
        default="merged_requirements.txt",
    )
    parser.add_argument(
        "-i",
        "--index-url",
        type=str,
        required=False,
        default="",
    )

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    rc = install_pip_tools_if_needed(args.index_url)
    if rc is False:
        exit(1)

    with DepsManager(args.root, args.index_url) as deps_manager:
        rc = deps_manager.pip_compile(args.output)
        if rc is False:
            exit(1)
