#!/bin/bash
# Build lighthouse-pm natively for aarch64 on a GitHub-hosted ARM64 runner.
#
# There is no archlinux/arm64 container image and no cross-toolchain that
# matches the Frame, so instead we bootstrap a real Arch Linux aarch64 rootfs
# (the device's exact distro) by downloading the package closure, then build
# inside it with chroot. The runner is itself aarch64, so everything runs
# natively -- no QEMU, no emulation. The resulting binary links against the
# exact Qt6/KF6/Kirigami/BlueZ the Frame ships and runs there without
# bundling anything.
set -eu

ARCH="aarch64"
ROOT="/opt/arch-root"
SRC="$ROOT/src"
GHA="${GITHUB_WORKSPACE:-$(pwd)}"

# Packages that pull in the toolchain + the app's Qt6/KF6/Kirigami/BlueZ plus
# every transitive dependency.
export SEEDS="glibc linux-api-headers bash coreutils file gcc binutils make cmake ninja qt6-qtbase qt6-qtdeclarative kf6-core kf6-kconfig kf6-ki18n kirigami bluez"

echo "=== runner: $(uname -m) / $(arch) / $(nproc) cpus ==="
if [ "$(uname -m)" != "aarch64" ]; then
  echo "ERROR: this script must run on an aarch64 host (use runs-on: ubuntu-24.04-arm)"
  exit 1
fi

echo "==> [1/7] host tools"
export DEBIAN_FRONTEND=noninteractive
sudo apt-get update -qq
sudo apt-get install -y -qq python3 curl zstd >/dev/null

echo "==> [2/7] pick a working Arch mirror"
MIRRORS="
https://mirror.rackspace.com/archlinux
https://mirrors.edge.kernel.org/archlinux
https://us.mirror.nexgen.net/archlinux
https://mirror.f4st.host/archlinux
https://archmirror.dl.ksy.io
https://mirror.1and1.com/archlinux
https://de.mirror.neverssl.com/archlinux
https://mirror.pangea.fi/archlinux
https://mirror.ska01.nl/archlinux
https://mirror.leaseweb.com/archlinux
https://archlinux.org
"
MIRROR=""
for m in $MIRRORS; do
  if curl -fsIL --max-time 20 "$m/extra/os/$ARCH/db.tar" >/dev/null 2>&1; then
    MIRROR="$m"; echo "  using $m"; break
  fi
done
[ -n "$MIRROR" ] || { echo "ERROR: no reachable Arch mirror"; exit 1; }

DB="$(mktemp -d)"
echo "==> [3/7] fetch package databases"
for r in core extra community; do
  curl -fsSL --max-time 120 "$MIRROR/$r/os/$ARCH/db.tar" -o "$DB/$r.tar"
  mkdir -p "$DB/db/$r"
  tar -xf "$DB/$r.tar" -C "$DB/db/$r"
done

echo "==> [4/7] resolve package closure"
PKGS="$(python3 - "$DB/db" <<'PY'
import sys, os, re
dbdir = sys.argv[1]
seed = os.environ.get("SEEDS", "").split()
pkgs = {}
repo_of = {}
def depbase(dep):
    return re.split(r"[<>=~ ]", dep)[0]
def parse_db(path, repo):
    text = open(path, encoding="utf-8", errors="replace").read()
    for block in text.split("%"):
        d = {}
        for line in block.splitlines():
            m = re.match(r"^\s*(\w+)\s*=\s*(.*)$", line)
            if not m:
                continue
            k = m.group(1).upper()
            v = m.group(2).strip()
            if len(v) >= 2 and v[0] == '"' and v[-1] == '"':
                v = v[1:-1]
            d.setdefault(k, []).append(v)
        name = d.get("NAME", [None])[0]
        fn = d.get("FILENAME", [None])[0]
        if name and fn:
            pkgs[name] = {"filename": fn, "depends": d.get("DEPENDS", []), "provides": d.get("PROVIDES", [])}
            repo_of[name] = repo
for r in ("core", "extra", "community"):
    rdir = os.path.join(dbdir, r)
    if os.path.isdir(rdir):
        for f in os.listdir(rdir):
            parse_db(os.path.join(rdir, f), r)
provides = {}
for name, d in pkgs.items():
    for p in d["provides"]:
        provides.setdefault(depbase(p), name)
real_of = {}
resolved = set()
unresolved = []
stack = list(seed)
while stack:
    n = stack.pop()
    base = depbase(n)
    if base in resolved:
        continue
    real = pkgs.get(base) or provides.get(base)
    if real is None:
        if base not in unresolved:
            unresolved.append(base)
        continue
    real_of[base] = real
    resolved.add(base)
    for dep in pkgs[real]["depends"]:
        stack.append(dep)
if unresolved:
    sys.stderr.write("UNRESOLVED: " + ", ".join(sorted(unresolved)) + "\n")
    sys.exit(2)
for base in sorted(real_of):
    print(pkgs[real_of[base]]["filename"], repo_of[real_of[base]])
PY
)"
echo "$PKGS" | sed 's/^/  /'
echo "  ($(echo "$PKGS" | wc -l) packages)"

echo "==> [5/7] download + extract into $ROOT"
DL="$(mktemp -d)"
while read -r fn repo; do
  [ -n "$fn" ] || continue
  curl -fsSL --max-time 300 "$MIRROR/$repo/os/$ARCH/$fn" -o "$DL/$fn" || { echo "  download failed: $fn"; exit 1; }
done <<< "$PKGS"
sudo rm -rf "$ROOT"
sudo mkdir -p "$ROOT"
for fn in "$DL"/*.pkg.tar.zst; do
  [ -e "$fn" ] || continue
  zstd -dc "$fn" | sudo tar -xf - -C "$ROOT"
done
sudo mkdir -p "$ROOT/etc" "$ROOT/tmp" "$ROOT/usr/src"
sudo cp /etc/resolv.conf "$ROOT/etc/resolv.conf" 2>/dev/null || true
sudo cp /etc/hosts "$ROOT/etc/hosts" 2>/dev/null || true
printf 'root:x:0:0:root:/root:/bin/bash\nnobody:x:65534:65534:nobody:/home:/bin/false\n' | sudo tee "$ROOT/etc/passwd" >/dev/null
printf 'root:x:0:\nnogroup:x:65534:\n' | sudo tee "$ROOT/etc/group" >/dev/null
printf '/lib\n/usr/lib\n' | sudo tee "$ROOT/etc/ld.so.conf" >/dev/null
sudo chroot "$ROOT" ldconfig

echo "==> [6/7] build inside chroot"
sudo mkdir -p "$SRC"
sudo cp -r "$GHA/." "$SRC/"
sudo chroot "$ROOT" /bin/bash -c '
  set -eu
  cd /src
  echo "--- versions ---"
  pacman -Q qt6-qtbase qt6-qtdeclarative kf6-core kf6-kconfig kf6-ki18n kirigami bluez 2>/dev/null | sed "s/^/  /" || true
  echo "--- cmake configure ---"
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/usr
  echo "--- build ---"
  cmake --build build -j"$(nproc)"
  echo "--- NEEDED shared libraries ---"
  readelf -d build/lighthouse-pm | grep -E "NEEDED" || true
  echo "--- smoke test (offscreen, 10s) ---"
  rc=0
  timeout 10 env QT_QPA_PLATFORM=offscreen ./build/lighthouse-pm > /src/smoke.log 2>&1 || rc=$?
  case "$rc" in
    124|143) echo "OK: app ran until the timeout (event loop alive)" ;;
    *) echo "--- smoke log ---"; cat /src/smoke.log; echo "SMOKE FAIL (rc=$rc)"; exit 1 ;;
  esac
'

echo "==> [7/7] collect artifact"
mkdir -p "$GHA/dist"
sudo cp "$SRC/build/lighthouse-pm" "$GHA/dist/lighthouse-pm-aarch64"
sudo cp "$SRC/smoke.log" "$GHA/dist/smoke.log" 2>/dev/null || true
sudo chown -R "$(id -u):$(id -g)" "$GHA/dist" 2>/dev/null || true
file "$GHA/dist/lighthouse-pm-aarch64"
echo "=== DONE ==="
