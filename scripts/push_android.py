#!/usr/bin/env python3

# Push runtime resources for Android samples.
#
# Runtime lookup convention in engine (Android):
#   <externalFilesDir>/<external_shader_dir>/*.spv
#   <externalFilesDir>/<external_assets_dir>/**
#   <externalFilesDir>/config/config.toml
#
# This script keeps device paths aligned with ResolveRuntimeResourcePaths().

import argparse
import shutil
import subprocess
import sys
from pathlib import Path
import toml



def run(cmd):
    print("[cmd]", " ".join(cmd))
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.stdout:
        print(result.stdout.strip())
    if result.returncode != 0:
        if result.stderr:
            print(result.stderr.strip(), file=sys.stderr)
        raise RuntimeError(f"command failed ({result.returncode})")


def adb_cmd(serial):
    base = ["adb"]
    if serial:
        base += ["-s", serial]
    return base


def ensure_remote_dir(serial, remote_dir, created_dirs):
    if remote_dir in created_dirs:
        return
    run(adb_cmd(serial) + ["shell", "mkdir", "-p", remote_dir])
    created_dirs.add(remote_dir)


def clear_remote_dir(serial, remote_dir):
    run(adb_cmd(serial) + ["shell", "rm", "-rf", remote_dir])



def push_dir_contents(serial, local_dir: Path, remote_dir: str, created_dirs):
    if not local_dir.exists():
        print(f"[skip] local path does not exist: {local_dir}")
        return
    ensure_remote_dir(serial, remote_dir, created_dirs)

    files = [p for p in local_dir.rglob("*") if p.is_file()]
    if not files:
        print(f"[skip] no files under: {local_dir}")
        return

    for src in files:
        rel = src.relative_to(local_dir).as_posix()
        dst = f"{remote_dir}/{rel}"
        dst_parent = dst.rsplit("/", 1)[0]
        ensure_remote_dir(serial, dst_parent, created_dirs)
        run(adb_cmd(serial) + ["push", str(src), dst])


def load_android_path_config(config_file: Path):
    cfg = {
        "use_external_files_dir": True,
        "external_assets_dir": "assets",
        "external_shader_dir": "saved/shaders",
        "external_log_dir": "logs",
    }
    if not config_file.exists():
        return cfg

    try:
        data = toml.load(config_file)
    except Exception as exc:
        print(f"[warn] failed to parse config.toml: {exc}")
        return cfg

    android = data.get("android", {})
    cfg["use_external_files_dir"] = bool(android.get("use_external_files_dir", True))
    cfg["external_assets_dir"] = str(android.get("external_assets_dir", android.get("assets_dir", "assets")))
    cfg["external_shader_dir"] = str(android.get("external_shader_dir", android.get("shader_dir", "saved/shaders")))
    cfg["external_log_dir"] = str(android.get("external_log_dir", "logs"))
    return cfg


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Push sample SPIR-V shaders/assets/config to Android external files dir."
        )
    )
    parser.add_argument("sample", help="sample name, e.g. hellotriangle or deferred")
    parser.add_argument("--serial", help="adb device serial", default="")
    parser.add_argument(
        "--package",
        help="Android package name (default: com.horizon.<sample>)",
        default="",
    )
    parser.add_argument(
        "--device-root",
        help=(
            "remote root path; default: /sdcard/Android/data/<package>/files"
        ),
        default="",
    )
    parser.add_argument(
        "--no-assets",
        help="do not push shared assets/",
        action="store_true",
    )
    parser.add_argument(
        "--no-clean",
        help="do not clear remote target directories before pushing",
        action="store_true",
    )
    args = parser.parse_args()

    if shutil.which("adb") is None:
        print("adb not found in PATH", file=sys.stderr)
        return 1

    repo_root = Path(__file__).resolve().parents[1]
    sample = args.sample.strip()
    package = args.package.strip() or f"com.horizon.{sample}"
    device_root = args.device_root.strip() or f"/sdcard/Android/data/{package}/files"

    sample_spv_dir = repo_root / "samples" / sample / "saved" / "shaders"
    assets_dir = repo_root / "assets"
    config_file = repo_root / "framework" / "config" / "config.toml"
    android_cfg = load_android_path_config(config_file)

    remote_shader_dir = f"{device_root}/{android_cfg['external_shader_dir'].strip('/')}"
    remote_assets_dir = f"{device_root}/{android_cfg['external_assets_dir'].strip('/')}"
    remote_config_dir = f"{device_root}/config"
    remote_config_file = f"{remote_config_dir}/config.toml"

    print(f"[info] sample={sample}")
    print(f"[info] package={package}")
    print(f"[info] device_root={device_root}")
    print(f"[info] external_assets_dir={android_cfg['external_assets_dir']}")
    print(f"[info] external_shader_dir={android_cfg['external_shader_dir']}")

    try:
        created_dirs = set()
        run(adb_cmd(args.serial) + ["start-server"])
        if not args.no_clean:
            clear_remote_dir(args.serial, remote_shader_dir)
            if not args.no_assets:
                clear_remote_dir(args.serial, remote_assets_dir)
            clear_remote_dir(args.serial, remote_config_dir)
        ensure_remote_dir(args.serial, device_root, created_dirs)
        push_dir_contents(args.serial, sample_spv_dir, remote_shader_dir, created_dirs)
        if not args.no_assets:
            push_dir_contents(args.serial, assets_dir, remote_assets_dir, created_dirs)
        if config_file.exists():
            ensure_remote_dir(args.serial, remote_config_dir, created_dirs)
            run(adb_cmd(args.serial) + ["push", str(config_file), remote_config_file])
        run(adb_cmd(args.serial) + ["shell", "chmod", "-R", "0777", device_root])
    except RuntimeError as exc:
        print(f"[error] {exc}", file=sys.stderr)
        return 1

    print("[ok] push completed")
    print(f"[ok] shaders -> {remote_shader_dir}")
    if not args.no_assets:
        print(f"[ok] assets  -> {remote_assets_dir}")
    print(f"[ok] config  -> {remote_config_file}")
    return 0


if __name__ == "__main__":
    main()
