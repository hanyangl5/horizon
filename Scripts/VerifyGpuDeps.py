#!/usr/bin/env python3
import argparse
import json
import re
import urllib.request
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
CONFIG_PATH = SCRIPT_DIR / "GpuDeps.json"


def load_config():
    with CONFIG_PATH.open("r", encoding="utf-8") as f:
        return json.load(f)["dependencies"]


def read_json(url):
    request = urllib.request.Request(url, headers={"User-Agent": "horizon-gpu-deps"})
    with urllib.request.urlopen(request) as response:
        return json.load(response)


def is_preview(version):
    return bool(re.search(r"(preview|alpha|beta|rc)", version, re.IGNORECASE))


def latest_nuget(dep):
    package_id = dep["package"].lower()
    data = read_json(f"https://api.nuget.org/v3-flatcontainer/{package_id}/index.json")
    versions = data["versions"]
    if not dep.get("allow_preview", False):
        versions = [v for v in versions if not is_preview(v)]
    return versions[-1] if versions else None


def latest_github(dep):
    repo = dep["repo"]
    if dep.get("allow_preview", False):
        tags = read_json(f"https://api.github.com/repos/{repo}/tags?per_page=50")
        return tags[0]["name"] if tags else None
    try:
        release = read_json(f"https://api.github.com/repos/{repo}/releases/latest")
        return release["tag_name"]
    except Exception:
        repo_info = read_json(f"https://api.github.com/repos/{repo}")
        branch = read_json(f"https://api.github.com/repos/{repo}/branches/{repo_info['default_branch']}")
        return branch["commit"]["sha"]


def check_required(name, dep):
    dst = REPO_ROOT / dep["destination"]
    missing = []
    values = {
        "version": dep.get("version", ""),
        "ref": dep.get("ref", "")
    }
    for pattern in dep.get("required", []):
        rel = pattern.format(**values)
        if not (dst / rel).exists():
            missing.append(rel)
    return missing


def parse_args(deps):
    parser = argparse.ArgumentParser(description="Verify pinned GPU third-party dependencies.")
    parser.add_argument("--deps", nargs="+", choices=sorted(deps.keys()), default=sorted(deps.keys()))
    parser.add_argument("--check-remote", action="store_true")
    return parser.parse_args()


def main():
    deps = load_config()
    args = parse_args(deps)
    failed = False
    for name in args.deps:
        dep = deps[name]
        pinned = dep.get("version", dep.get("ref"))
        missing = check_required(name, dep)
        if missing:
            failed = True
            print(f"[missing] {name} {pinned}")
            for rel in missing:
                print(f"  {rel}")
        else:
            print(f"[ok] {name} {pinned}")

        if args.check_remote:
            try:
                latest = latest_nuget(dep) if dep["kind"] == "nuget" else latest_github(dep)
                if latest and latest != pinned:
                    print(f"  remote latest: {latest}")
            except Exception as exc:
                failed = True
                print(f"  remote check failed: {exc}")

    raise SystemExit(1 if failed else 0)


if __name__ == "__main__":
    main()
