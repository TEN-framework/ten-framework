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

    def get_pip_version():
        """Get the major version number of the current pip installation."""
        import importlib.metadata

        try:
            version_str = importlib.metadata.version("pip")
            # Parse major version number
            major_version = int(version_str.split(".")[0])
            return major_version, version_str
        except Exception:
            return None, None

    # IMPORTANT: Check and fix pip version FIRST before installing pip-tools.
    # pip-tools has pip as a dependency, and installing pip-tools can upgrade
    # pip to 25.x, which is incompatible with all current pip-tools versions.
    #
    # Issue: pip 25.0+ removed the `InstallRequirement.use_pep517` attribute,
    # but pip-tools <= 7.5.1 still tries to access it, causing:
    # AttributeError: 'InstallRequirement' object has no attribute 'use_pep517'
    #
    # We need to ensure pip stays at 24.x until pip-tools releases a compatible
    # version.
    #
    # TODO: Once pip-tools releases a version that supports pip 25+ (likely
    # pip-tools >= 7.6.0 or 8.0.0), update this logic to:
    # 1. Remove the pip version constraint in installation commands
    # 2. Update min_pip_tools_version to the new compatible version
    # 3. Let pip remain at its latest version
    # Track: https://github.com/jazzband/pip-tools/issues (check for pip 25
    # support)

    pip_major_version, pip_full_version = get_pip_version()
    print(f"Detected pip version: {pip_full_version}")

    if pip_major_version and pip_major_version >= 25:
        print(
            f"Warning: pip {pip_full_version} has compatibility issues with pip-tools."
        )
        print("Downgrading pip to 24.3.1 for compatibility...")
        import subprocess

        args = [
            sys.executable,
            "-m",
            "pip",
            "install",
            "--force-reinstall",
            "pip==24.3.1",
        ]

        if index_url:
            args.extend(["-i", index_url])

        try:
            subprocess.check_call(args)
            print("pip downgraded successfully to 24.3.1")
            # Re-check pip version
            pip_major_version, pip_full_version = get_pip_version()
            print(f"Current pip version: {pip_full_version}")
        except subprocess.CalledProcessError as e:
            print(f"Failed to downgrade pip: {e}")
            print("Continuing anyway...")

    # Always require pip-tools >= 7.4.0 for maximum compatibility
    min_pip_tools_version = "7.4.0"

    if is_package_installed("pip-tools"):
        # Check if pip-tools version is new enough
        import importlib.metadata

        try:
            current_version = importlib.metadata.version("pip-tools")
            print(f"pip-tools {current_version} is already installed")

            # Always check if pip-tools version is compatible (>= 7.4.0)
            # Simple version comparison: compare major and minor version numbers
            current_parts = current_version.split(".")
            min_parts = min_pip_tools_version.split(".")

            needs_upgrade = False
            try:
                current_major = int(current_parts[0])
                current_minor = (
                    int(current_parts[1]) if len(current_parts) > 1 else 0
                )
                min_major = int(min_parts[0])
                min_minor = int(min_parts[1]) if len(min_parts) > 1 else 0

                if current_major < min_major or (
                    current_major == min_major and current_minor < min_minor
                ):
                    needs_upgrade = True
            except (ValueError, IndexError):
                # If version parsing fails, try to upgrade to be safe
                needs_upgrade = True

            if needs_upgrade:
                print(
                    f"pip-tools {current_version} is too old, "
                    f"upgrading to >={min_pip_tools_version}..."
                )
                import subprocess

                # Constrain pip to <25 to prevent automatic upgrade to
                # incompatible version
                args = [
                    sys.executable,
                    "-m",
                    "pip",
                    "install",
                    "--upgrade",
                    f"pip-tools>={min_pip_tools_version}",
                    "pip<25",
                ]

                if index_url:
                    args.extend(["-i", index_url])

                try:
                    subprocess.check_call(args)
                    print("pip-tools upgraded successfully")
                except subprocess.CalledProcessError as e:
                    print(f"Failed to upgrade pip-tools: {e}")
                    return False
        except Exception as e:
            print(f"Warning: Could not check pip-tools version: {e}")

        return True
    else:
        import subprocess

        print(
            f"pip-tools is not installed. Installing pip-tools>={min_pip_tools_version}..."
        )

        # Constrain pip to <25 to prevent automatic upgrade to incompatible
        # version
        args = [
            sys.executable,
            "-m",
            "pip",
            "install",
            f"pip-tools>={min_pip_tools_version}",
            "pip<25",
        ]

        if index_url:
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

        # Collect requirements.txt files from ten_packages/system directory.
        system_dir = os.path.join(self.root, "ten_packages", "system")
        if os.path.exists(system_dir):
            for root, _, files in os.walk(system_dir):
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

        # Use 'python -m piptools compile' instead of calling pip-compile
        # command directly.
        # This ensures we use the pip-tools from the current Python environment,
        # rather than the globally installed version.
        args = [
            sys.executable,
            "-m",
            "piptools",
            "compile",
            self.requirements_in_file,
            "--output-file",
            output,
        ]

        if self.index_url:
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
