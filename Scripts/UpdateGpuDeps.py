#!/usr/bin/env python3
import argparse
import json
import shutil
import tempfile
import urllib.request
import zipfile
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
CONFIG_PATH = SCRIPT_DIR / "GpuDeps.json"
SCRIPT_MANAGED_KINDS = {"github_archive", "nuget"}


def load_config():
    with CONFIG_PATH.open("r", encoding="utf-8") as f:
        return json.load(f)["dependencies"]


def download(url, out_path):
    print(f"download {url}")
    with urllib.request.urlopen(url) as response, out_path.open("wb") as f:
        shutil.copyfileobj(response, f)


def replace_dir(src, dst, dry_run, preserve=None):
    preserve = preserve or []
    if dry_run:
        print(f"would replace {dst}")
        return
    preserved = []
    with tempfile.TemporaryDirectory(prefix="preserve_") as temp:
        temp_dir = Path(temp)
        for rel in preserve:
            path = dst / rel
            if path.exists():
                saved = temp_dir / rel
                saved.parent.mkdir(parents=True, exist_ok=True)
                if path.is_dir():
                    shutil.copytree(path, saved)
                else:
                    shutil.copy2(path, saved)
                preserved.append((rel, saved))

        if dst.exists():
            shutil.rmtree(dst)
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(src, dst)

        for rel, saved in preserved:
            target = dst / rel
            if target.exists():
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            if saved.is_dir():
                shutil.copytree(saved, target)
            else:
                shutil.copy2(saved, target)


def copy_tree_contents(src, dst, dry_run):
    if dry_run:
        print(f"would copy {src} -> {dst}")
        return
    if dst.exists():
        shutil.rmtree(dst)
    dst.mkdir(parents=True, exist_ok=True)
    for item in src.iterdir():
        target = dst / item.name
        if item.is_dir():
            shutil.copytree(item, target)
        else:
            shutil.copy2(item, target)


def prune_architectures(root, architectures, dry_run):
    if not architectures or not root.exists():
        return
    wanted = {arch.lower() for arch in architectures}
    for child in root.iterdir():
        if child.is_dir() and child.name.lower() not in wanted:
            if dry_run:
                print(f"would remove {child}")
            else:
                shutil.rmtree(child)


def copy_arch_tree(src, dst, architectures, dry_run):
    if dry_run:
        print(f"would copy {src} -> {dst} for {', '.join(architectures)}")
        return
    if dst.exists():
        shutil.rmtree(dst)
    dst.mkdir(parents=True, exist_ok=True)
    for arch in architectures:
        arch_src = src / arch
        if arch_src.exists():
            shutil.copytree(arch_src, dst / arch)


def update_agility(dep, pkg_dir, dry_run):
    version = dep["version"]
    dst_root = REPO_ROOT / dep["destination"]
    dst = dst_root / f"microsoft.direct3d.d3d12.{version}"
    src = pkg_dir / "build" / "native"
    if not src.exists():
        src = pkg_dir
    if dry_run:
        print(f"would remove older Agility SDK package directories in {dst_root}")
    else:
        dst_root.mkdir(parents=True, exist_ok=True)
        for child in dst_root.glob("microsoft.direct3d.d3d12.*"):
            if child.is_dir() and child.name != dst.name:
                shutil.rmtree(child)
    replace_dir(src, dst, dry_run)
    prune_architectures(dst / "bin", dep.get("architectures"), dry_run)


def update_directstorage(dep, pkg_dir, dry_run):
    dst_root = REPO_ROOT / dep["destination"]
    native = pkg_dir / "native"
    architectures = dep.get("architectures", ["x64"])
    for folder in ("include", "winmd"):
        src = native / folder
        if src.exists():
            copy_tree_contents(src, dst_root / folder, dry_run)
    for folder in ("bin", "lib"):
        src = native / folder
        if src.exists():
            copy_arch_tree(src, dst_root / folder, architectures, dry_run)
    for name in ("LICENSE.txt", "LICENSE-CODE.txt", "NOTICES.txt", "README.md", "distributable_files.txt"):
        src = pkg_dir / name
        if src.exists():
            if dry_run:
                print(f"would copy {src} -> {dst_root / name}")
            else:
                dst_root.mkdir(parents=True, exist_ok=True)
                shutil.copy2(src, dst_root / name)


def update_nuget(name, dep, dry_run):
    package = dep["package"]
    version = dep["version"]
    package_id = package.lower()
    url = f"https://api.nuget.org/v3-flatcontainer/{package_id}/{version}/{package_id}.{version}.nupkg"
    with tempfile.TemporaryDirectory(prefix=f"{name}_") as temp:
        temp_dir = Path(temp)
        archive = temp_dir / f"{package_id}.{version}.nupkg"
        download(url, archive)
        pkg_dir = temp_dir / "pkg"
        with zipfile.ZipFile(archive) as zf:
            zf.extractall(pkg_dir)
        if dep["layout"] == "agility":
            update_agility(dep, pkg_dir, dry_run)
        elif dep["layout"] == "directstorage":
            update_directstorage(dep, pkg_dir, dry_run)
        else:
            raise ValueError(f"unknown NuGet layout: {dep['layout']}")


def update_github_archive(name, dep, dry_run):
    repo = dep["repo"]
    ref = dep["ref"]
    url = f"https://github.com/{repo}/archive/{ref}.zip"
    with tempfile.TemporaryDirectory(prefix=f"{name}_") as temp:
        temp_dir = Path(temp)
        archive = temp_dir / f"{name}.zip"
        download(url, archive)
        extract_dir = temp_dir / "src"
        with zipfile.ZipFile(archive) as zf:
            zf.extractall(extract_dir)
        roots = [p for p in extract_dir.iterdir() if p.is_dir()]
        if len(roots) != 1:
            raise RuntimeError(f"expected one archive root in {archive}, got {len(roots)}")
        replace_dir(roots[0], REPO_ROOT / dep["destination"], dry_run, dep.get("preserve"))


def parse_args(deps):
    script_managed = {name: dep for name, dep in deps.items() if dep["kind"] in SCRIPT_MANAGED_KINDS}
    parser = argparse.ArgumentParser(description="Update pinned non-submodule GPU third-party dependencies.")
    parser.add_argument(
        "--deps",
        nargs="+",
        choices=sorted(script_managed.keys()),
        default=sorted(script_managed.keys()),
    )
    parser.add_argument("--dry-run", action="store_true")
    return parser.parse_args()


def main():
    deps = load_config()
    args = parse_args(deps)
    for name in args.deps:
        dep = deps[name]
        print(f"update {name} -> {dep.get('version', dep.get('ref'))}")
        if dep["kind"] == "nuget":
            update_nuget(name, dep, args.dry_run)
        elif dep["kind"] == "github_archive":
            update_github_archive(name, dep, args.dry_run)
        else:
            raise ValueError(f"unknown dependency kind: {dep['kind']}")


if __name__ == "__main__":
    main()
