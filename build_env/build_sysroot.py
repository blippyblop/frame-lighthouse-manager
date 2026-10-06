#!/usr/bin/env python3
"""Build an aarch64 Alpine sysroot by extracting .apk packages.

Downloads aarch64 packages from the Alpine v3.24 repositories (main + community),
resolves the dependency closure, and extracts everything into a sysroot
directory usable as --sysroot for a cross compiler.
"""
import os
import re
import sys
import tarfile
import urllib.request

MIRROR = "https://dl-cdn.alpinelinux.org/alpine/v3.24"
REPOS = ["main", "community"]
ARCH = "aarch64"
SYSROOT = "/opt/aarch64-sysroot"

SEEDS = [
    # toolchain
    "gcc", "g++", "binutils", "musl-dev", "libgcc-static",
    "libstdc++-dev", "musl", "libgcc", "libstdc++",
    # qt6
    "qt6-qtbase", "qt6-qtbase-dev", "qt6-qtbase-x11", "qt6-qtbase-wayland",
    "qt6-qtconnectivity", "qt6-qtconnectivity-dev",
    "qt6-qtsvg", "qt6-qtsvg-dev",
    "qt6-qtdeclarative", "qt6-qtdeclarative-dev",
    # KDE
    "kirigami", "kirigami-libs", "kirigami-dev",
    "kcoreaddons", "kcoreaddons-dev",
    "kconfig", "kconfig-dev",
    "ki18n", "ki18n-dev",
    # bluez
    "bluez", "bluez-dev", "bluez-libs",
    "dbus", "dbus-libs",
]

# build-only virtuals to skip
SKIP_PKGS = {"apk-tools", "m4", "autoconf", "automake", "linux-headers",
             "linux-headers-generic", "libtool", "pkgconfig"}


def fetch(url):
    print("fetch", url)
    with urllib.request.urlopen(url) as r:
        return r.read()


def parse_index(data):
    pkgs = {}
    for block in data.split(b"\n\n"):
        lines = block.decode("utf-8", "replace").splitlines()
        info = {}
        i = 0
        while i < len(lines):
            m = re.match(r"^(\w+):(.*)$", lines[i])
            if m and m.group(1) in ("P", "V"):
                info[m.group(1)] = m.group(2).strip()
                i += 1
                continue
            if m and m.group(1) in ("D", "p"):
                parts = [m.group(2)]
                i += 1
                while i < len(lines) and not re.match(r"^\w+:", lines[i]):
                    parts.append(lines[i])
                    i += 1
                info.setdefault(m.group(1), []).extend(" ".join(parts).split())
                continue
            i += 1
        if "P" in info:
            pkgs[info["P"]] = info
    return pkgs


def main():
    allpkgs = {}
    for repo in REPOS:
        blob = fetch(f"{MIRROR}/{repo}/{ARCH}/APKINDEX.tar.gz")
        with tarfile.open(fileobj=__import__("io").BytesIO(blob)) as t:
            index = t.extractfile("APKINDEX").read()
        for name, info in parse_index(index).items():
            info["repo"] = repo
            allpkgs[name] = info

    # map virtual dep names -> providing package (from p: PROVIDES lines)
    provides_map = {}
    for name, info in allpkgs.items():
        for p in info.get("p", []):
            pname = re.split(r"[><=!~]", p)[0]
            provides_map.setdefault(pname, name)

    VIRTUAL = {
        "c++-dev": ["libstdc++-dev"],
    }

    def resolve(dep):
        dep = dep.rstrip()
        name = re.split(r"[><=!~]", dep)[0]
        if name in SKIP_PKGS:
            return []
        if name in VIRTUAL:
            return VIRTUAL[name]
        if name in allpkgs:
            return [name]
        if name in provides_map:
            return [provides_map[name]]
        print(f"  ! unknown dep {dep}")
        return []

    wanted = set(SEEDS)
    for s in SEEDS:
        if s not in allpkgs:
            print(f"seed not found: {s}")
    queue = list(SEEDS)
    seen = set()
    while queue:
        p = queue.pop()
        if p in seen:
            continue
        seen.add(p)
        info = allpkgs.get(p)
        if not info:
            continue
        for d in info.get("D", []):
            if d.startswith("!"):
                continue
            for r in resolve(d):
                if r not in seen:
                    queue.append(r)

    print(f"\nTotal packages to install: {len(seen)}")
    os.makedirs(SYSROOT, exist_ok=True)
    for name in sorted(seen):
        info = allpkgs[name]
        ver = info.get("V", "")
        if not ver:
            print(f"skip virtual {name}")
            continue
        repo = info.get("repo", "main")
        apk = f"{MIRROR}/{repo}/{ARCH}/{name}-{ver}.apk"
        local = f"/tmp/apks/{name}.apk"
        os.makedirs("/tmp/apks", exist_ok=True)
        if not os.path.exists(local):
            data = fetch(apk)
            with open(local, "wb") as f:
                f.write(data)
        with tarfile.open(local) as t:
            for member in t.getmembers():
                if member.name.startswith("CONTROL"):
                    continue
                t.extract(member, SYSROOT, filter="fully_trusted")
        print(f"extracted {name}-{ver}")
    print(f"\nSysroot ready at {SYSROOT}")


if __name__ == "__main__":
    main()
