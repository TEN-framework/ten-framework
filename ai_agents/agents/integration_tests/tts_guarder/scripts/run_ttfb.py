#!/usr/bin/env python3
"""Run the seven first-batch TTFB configurations in an isolated prepared app."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from datetime import datetime, timezone

CASES = {
    "elevenlabs-tts": "elevenlabs_multiple_context_tts_python",
    "elevenlabs-ttd": "elevenlabs_multiple_context_tts_python",
    "bytedance_tts": "bytedance_tts",
    "bytedance_tts_duplex": "bytedance_tts_duplex",
    "minimax_tts_websocket": "minimax_tts_websocket",
    "minimax_tts_websocket_duplex": "minimax_tts_websocket_duplex",
    "soul_tts_python": "soul_tts_python",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--app-dir",
        type=Path,
        required=True,
        help="Prepared TEN app supplying ten_packages/system",
    )
    parser.add_argument(
        "--extension-dir",
        type=Path,
        required=True,
        help="Directory containing the six extension packages to test",
    )
    parser.add_argument(
        "--config-root",
        type=Path,
        required=True,
        help="Seven case folders, each containing property_basic_audio_setting1.json",
    )
    parser.add_argument("--python", default=sys.executable)
    parser.add_argument("--env-file", type=Path, action="append", default=[])
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--collect-only", action="store_true")
    parser.add_argument("--case", choices=list(CASES), help="Run only this configuration")
    args = parser.parse_args()
    cases = {args.case: CASES[args.case]} if args.case else CASES
    env = os.environ.copy()
    if args.env_file:
        from dotenv import dotenv_values

        for path in args.env_file:
            if not path.is_file():
                parser.error(f"Environment file missing: {path}")
            for key, value in dotenv_values(path).items():
                if value:
                    env[key] = value
    system = args.app_dir.resolve() / "ten_packages/system"
    extension_dir = args.extension_dir.resolve()
    config_root = args.config_root.resolve()
    for package in ["ten_runtime", "ten_runtime_python", "ten_ai_base"]:
        if not (system / package).is_dir():
            parser.error(f"Prepared package missing: {system / package}")
    manifests = {}
    for label, name in cases.items():
        path = config_root / label / "property_basic_audio_setting1.json"
        if not path.is_file():
            parser.error(f"Configuration missing: {path}")
        config = json.loads(path.read_text())
        if label.startswith("elevenlabs-"):
            model = config.get("params", {}).get("model_id", "")
            is_tts = model.startswith("eleven_") and "v2" in model
            if not model or is_tts != (label == "elevenlabs-tts"):
                parser.error(f"Wrong ElevenLabs model family for {label}")
        manifest_path = extension_dir / name / "manifest.json"
        if not manifest_path.is_file():
            parser.error(f"Extension missing: {manifest_path}")
        manifests[name] = json.loads(manifest_path.read_text())
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    print(f"TTFB verification | {datetime.now(timezone.utc).isoformat()}", flush=True)
    print(f"Sources: {extension_dir}\nConfigurations: {config_root}", flush=True)
    print(
        "Mode: "
        + (
            "COLLECTION ONLY (no provider requests)"
            if args.collect_only
            else "LIVE PROVIDERS"
        ),
        flush=True,
    )
    results = []
    guarder = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix="tts-ttfb-review-") as temporary:
        app = Path(temporary)
        shutil.copytree(
            guarder / "tests",
            app / "tests",
            ignore=shutil.ignore_patterns("__pycache__"),
        )
        shutil.copy2(guarder / "pytest.ini", app / "pytest.ini")
        (app / "ten_packages").mkdir()
        (app / "ten_packages/system").symlink_to(system, target_is_directory=True)
        extensions = app / "ten_packages/extension"
        extensions.mkdir()
        for name in manifests:
            (extensions / name).symlink_to(
                extension_dir / name, target_is_directory=True
            )
        (app / "manifest.json").write_text(
            json.dumps(
                {
                    "type": "app",
                    "name": "tts_ttfb_review",
                    "version": "0.1.0",
                    "dependencies": [
                        {
                            "type": "extension",
                            "name": name,
                            "version": manifest["version"],
                        }
                        for name, manifest in manifests.items()
                    ],
                }
            )
        )
        env["PYTHONPATH"] = (
            os.pathsep.join(
                map(
                    str,
                    [
                        app,
                        system / "ten_runtime_python/lib",
                        system / "ten_runtime_python/interface",
                        system / "ten_ai_base/interface",
                    ],
                )
            )
            + os.pathsep
            + env.get("PYTHONPATH", "")
        )
        for label, name in cases.items():
            print(f"\n=== {label} | test_sequential_requests_ttfb ===", flush=True)
            command = [
                "uv",
                "run",
                "--no-project",
                "--python",
                args.python,
                "python",
                "-m",
                "pytest",
                "tests/test_metrics.py::test_sequential_requests_ttfb",
                "--extension_name",
                name,
                "--config_dir",
                str(config_root / label),
                "-m",
                "api",
                "--strict-markers",
                "-v",
                "--tb=short",
                "--junitxml",
                str(output / f"{label}.xml"),
            ]
            if args.collect_only:
                command.append("--collect-only")
            with (output / f"{label}.log").open("w") as log:
                try:
                    completed = subprocess.run(
                        command,
                        cwd=app,
                        env=env,
                        stdout=log,
                        stderr=subprocess.STDOUT,
                        timeout=60,
                    )
                    code = completed.returncode
                except subprocess.TimeoutExpired:
                    code = 124
            status = (
                ("COLLECTED" if args.collect_only else "PASS") if code == 0 else "FAIL"
            )
            print(f"{status:9} {label} (exit={code})", flush=True)
            print(f"Log: {output / (label + '.log')}", flush=True)
            results.append(
                {"case": label, "extension": name, "status": status, "exit": code}
            )
            (output / "results.json").write_text(json.dumps(results, indent=2))
    summary = "\n".join(f"{r['status']:9} {r['case']}" for r in results)
    print(f"\n=== TTFB SUMMARY ===\n{summary}\nArtifacts: {output}", flush=True)
    (output / "summary.txt").write_text(summary + "\n")
    return int(any(r["exit"] != 0 for r in results))


if __name__ == "__main__":
    raise SystemExit(main())
