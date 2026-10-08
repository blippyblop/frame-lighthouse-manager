#!/bin/sh
# Stage the self-contained flatpak payload from the pristine firmware rootfs.
#
# Usage: sh stage.sh FW_ROOTFS SYSROOT BINARY OUTDIR
#   FW_ROOTFS  extracted firmware rootfs (pristine; provides all real files)
#   SYSROOT    build sysroot (provides the SONAME -> versioned-file map as
#              symlinks; may equal FW_ROOTFS)
#   BINARY     the built aarch64 lighthouse-pm binary
#   OUTDIR     output dir (gets: app/lighthouse-pm, app/lighthouse-pm.sh,
#              app/rootfs/, plus appdir/ + the .flatpak bundle)
#
# The staged app/ is the flatpak's /app: firmware glibc/Qt/KF6/Kirigami under
# app/rootfs, patchelf'd binary, location-independent wrapper.
#
# Library resolution is a *dependency closure*: after seeding a small base
# list of stable sonames, every NEEDED entry of the binary and of every
# staged ELF is resolved against the rootfs (exact file, symlink, or
# newest-versioned glob) and copied in recursively. Nothing is hardcoded to
# a library generation, so a moving firmware preview (icu 74 -> 77, wayland
# soname bumps, new Qt patch builds) cannot silently produce a broken bundle:
# any unresolved NEEDED is fatal.
set -e
FW_ROOTFS=$1
SYSROOT=$2
BINARY=$3
OUT=$4
RFS=$OUT/app/rootfs

[ -d "$FW_ROOTFS/usr/lib" ] || { echo "no rootfs at $FW_ROOTFS" >&2; exit 1; }
[ -f "$BINARY" ] || { echo "no binary at $BINARY" >&2; exit 1; }

READELF=${READELF:-$(command -v readelf 2>/dev/null || true)}
[ -n "$READELF" ] || READELF="$SYSROOT/usr/bin/readelf"
[ -x "$READELF" ] || { echo "no readelf available" >&2; exit 1; }

rm -rf "$OUT"
mkdir -p "$RFS/usr/lib" "$RFS/etc/fonts" "$RFS/usr/share/fonts" "$RFS/var/cache/fontconfig"

# ---- resolution helpers ----------------------------------------------------
# resolve_lib NAME -> prints the real (fully-versioned) file in the rootfs
resolve_lib() {
    f="$FW_ROOTFS/usr/lib/$1"
    if [ -e "$f" ]; then readlink -f "$f"; return 0; fi
    t=$(ls "$FW_ROOTFS/usr/lib/$1".* 2>/dev/null | sort -V | tail -1)
    if [ -n "$t" ] && [ -e "$t" ]; then readlink -f "$t"; return 0; fi
    f="$SYSROOT/usr/lib/$1"
    if [ -f "$f" ]; then readlink -f "$f"; return 0; fi
    return 1
}

# install_soname SONAME: copy the real file into RFS + create the soname link
install_soname() {
    [ -e "$RFS/usr/lib/$1" ] && return 0
    real=$(resolve_lib "$1") || return 1
    b=$(basename "$real")
    cp -a "$real" "$RFS/usr/lib/$b"
    # if the rootfs ships the file under its exact soname, the copy IS the
    # link target — creating libm.so.6 -> libm.so.6 would be a loop
    if [ "$b" != "$1" ]; then ln -sf "$b" "$RFS/usr/lib/$1"; fi
    return 0
}

# ---- 1) seed: base sonames that are always needed --------------------------
# (moving-generation libs — icu, wayland, Qt private platform libs — are
#  pulled in by the closure pass below, not listed here)
for name in \
  ld-linux-aarch64.so.1 \
  libEGL.so.1 libGLX.so.0 libGLdispatch.so.0 libOpenGL.so.0 \
  libICE.so.6 libSM.so.6 \
  libKF6ConfigCore.so.6 libKF6CoreAddons.so.6 libKF6I18n.so.6 \
  libKirigami.so.6 libKirigamiDelegates.so.6 libKirigamiDialogs.so.6 \
  libKirigamiLayouts.so.6 libKirigamiLayoutsPrivate.so.6 \
  libKirigamiPlatform.so.6 libKirigamiPrimitives.so.6 libKirigamiPrivate.so.6 \
  libQt6Core.so.6 libQt6DBus.so.6 libQt6Gui.so.6 libQt6Network.so.6 \
  libQt6OpenGL.so.6 libQt6Qml.so.6 libQt6QmlMeta.so.6 libQt6QmlModels.so.6 \
  libQt6QmlWorkerScript.so.6 libQt6Quick.so.6 libQt6QuickControls2.so.6 \
  libQt6QuickTemplates2.so.6 libQt6WaylandClient.so.6 libQt6XcbQpa.so.6 \
  libX11-xcb.so.1 libX11.so.6 libXau.so.6 libXdmcp.so.6 libxcb.so.1 \
  libxkbcommon.so.0 libxkbcommon-x11.so.0 \
  libb2.so.1 libblkid.so.1 libbrotlicommon.so.1 libbrotlidec.so.1 \
  libbz2.so.1 libc.so.6 libcap.so.2 libcom_err.so.2 libcrypto.so.3 \
  libcurl.so.4 libdbus-1.so.3 libdouble-conversion.so.3 libduktape.so.207 \
  libexpat.so.1 libffi.so.8 libfontconfig.so.1 libfreetype.so.6 \
  libgcc_s.so.1 libgio-2.0.so.0 libglib-2.0.so.0 libgmodule-2.0.so.0 \
  libgobject-2.0.so.0 libgomp.so.1 libgraphite2.so.3 libgssapi_krb5.so.2 \
  libharfbuzz.so.0 libidn2.so.0 libkeyutils.so.1 \
  libk5crypto.so.3 libkrb5.so.3 libkrb5support.so.0 libmd4c.so.0 \
  libm.so.6 libmount.so.1 libnghttp2.so.14 libnghttp3.so.9 libpcre2-16.so.0 \
  libpcre2-8.so.0 libpng16.so.16 libproxy.so.1 libpsl.so.5 libresolv.so.2 \
  libssh2.so.1 libssl.so.3 libstdc++.so.6 libsystemd.so.0 libudev.so.1 \
  libunistring.so.5 libwayland-client.so.0 libwayland-cursor.so.0 \
  libwayland-egl.so.1 libwayland-server.so.0 \
  libxcb-cursor.so.0 libxcb-icccm.so.4 libxcb-image.so.0 libxcb-keysyms.so.1 \
  libxcb-randr.so.0 libxcb-render-util.so.0 libxcb-render.so.0 \
  libxcb-shape.so.0 libxcb-shm.so.0 libxcb-sync.so.1 libxcb-xfixes.so.0 \
  libxcb-xinput.so.0 libxcb-xkb.so.1 libz.so.1 libzstd.so.1
do
  install_soname "$name" || true
done

# ---- 2) QML modules + plugins: whitelist ONLY what the app + Kirigami use --
mkdir -p "$RFS/usr/lib/qt6/qml/org/kde" "$RFS/usr/lib/qt6/plugins"
QML="$FW_ROOTFS/usr/lib/qt6/qml"
cp -a "$QML/QtQml" "$QML/QtCore" "$RFS/usr/lib/qt6/qml/"
mkdir -p "$RFS/usr/lib/qt6/qml/QtQuick"
for m in Controls Templates Layouts Window Effects Shapes tooling; do
  [ -d "$QML/QtQuick/$m" ] && cp -a "$QML/QtQuick/$m" "$RFS/usr/lib/qt6/qml/QtQuick/"
done
# Controls: keep Basic (required base/fallback) + Fusion (the style the app
# requests); drop Material/Universal/Imagine/macOS/iOS/Windows/NativeStyle
for s in Material Universal Imagine FluentWinUI3 macOS iOS Windows NativeStyle; do
  rm -rf "$RFS/usr/lib/qt6/qml/QtQuick/Controls/$s"
done
[ -d "$QML/org/kde/kirigami" ] && cp -a "$QML/org/kde/kirigami" "$RFS/usr/lib/qt6/qml/org/kde/"
# plugins: runtime platform + input + image/icon support only
for d in platforms xcbglintegrations wayland-graphics-integration-client \
         wayland-shell-integration platforminputcontexts iconengines \
         imageformats generic; do
  [ -d "$FW_ROOTFS/usr/lib/qt6/plugins/$d" ] && \
    cp -a "$FW_ROOTFS/usr/lib/qt6/plugins/$d" "$RFS/usr/lib/qt6/plugins/"
done

# ---- 3) Fonts: slim, freely-licensed (SIL OFL) UI set ----------------------
#    Cantarell (GNOME UI face) + the four core Noto Sans weights.
#    Best-effort: the Holo preview's font dirs shift between builds.
cp -a "$FW_ROOTFS/usr/share/fonts/cantarell" "$RFS/usr/share/fonts/" 2>/dev/null \
  || echo "font dir missing: cantarell" >&2
mkdir -p "$RFS/usr/share/fonts/noto"
for f in NotoSans-Regular.ttf NotoSans-Bold.ttf NotoSans-Italic.ttf NotoSans-BoldItalic.ttf; do
  cp -a "$FW_ROOTFS/usr/share/fonts/noto/$f" "$RFS/usr/share/fonts/noto/" 2>/dev/null \
    || echo "font missing: $f" >&2
done

# ---- 4) fontconfig: point at the bundled tree (no stale cache) -------------
cat > "$RFS/etc/fonts/fonts.conf" <<'EOF'
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">
<fontconfig>
	<description>Lighthouse PM bundled fonts</description>
	<dir>/app/rootfs/usr/share/fonts</dir>
	<cachedir>/app/rootfs/var/cache/fontconfig</cachedir>
	<config>
		<rescan><int>30</int></rescan>
	</config>
</fontconfig>
EOF

# ---- 5) App binary + location-independent wrapper --------------------------
mkdir -p "$OUT/app"
cp -a "$BINARY" "$OUT/app/lighthouse-pm"
cat > "$OUT/app/lighthouse-pm.sh" <<'EOF'
#!/bin/sh
D=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
R="$D/rootfs"
export LD_LIBRARY_PATH="$R/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_QPA_PLATFORM_PLUGIN_PATH="$R/usr/lib/qt6/plugins/platforms"
export QT_PLUGIN_PATH="$R/usr/lib/qt6/plugins"
export QML2_IMPORT_PATH="$R/usr/lib/qt6/qml"
export QML_PLUGIN_PATH="$R/usr/lib/qt6/qml"
export FONTCONFIG_FILE="$R/etc/fonts/fonts.conf"
export FONTCONFIG_PATH="$R/etc/fonts"
exec "$D/lighthouse-pm" "$@"
EOF
chmod +x "$OUT/app/lighthouse-pm" "$OUT/app/lighthouse-pm.sh"
# the extraction can lose the exec bit on the loader; the wrapper needs it
chmod +x "$RFS"/usr/lib/ld-linux-aarch64.so.1 2>/dev/null || true

# ---- 6) dependency closure + verification ----------------------------------
# Every NEEDED of the binary and of every staged ELF must resolve inside the
# staged tree. This is the gate that keeps the bundle launchable regardless
# of which firmware generation it was staged from.
MISSING=0
process_elf() {
  for n in $( "$READELF" -d "$1" 2>/dev/null | awk -F'[][]' '/NEEDED/{print $2}' ); do
    [ -e "$RFS/usr/lib/$n" ] && continue
    if real=$(resolve_lib "$n"); then
      b=$(basename "$real")
      cp -a "$real" "$RFS/usr/lib/$b"
      if [ "$b" != "$n" ]; then ln -sf "$b" "$RFS/usr/lib/$n"; fi
      process_elf "$RFS/usr/lib/$b"
    else
      echo "MISSING DEP: $n (needed by $(basename "$1"))" >&2
      MISSING=$((MISSING+1))
    fi
  done
}
process_elf "$OUT/app/lighthouse-pm"
for f in $(find "$RFS" -type f \( -name '*.so' -o -name '*.so.*' \)); do
  process_elf "$f"
done
[ "$MISSING" -eq 0 ] || { echo "FATAL: $MISSING unresolved NEEDED deps — refusing to stage a broken bundle" >&2; exit 1; }

# ---- 7) Make the binary self-contained w.r.t. glibc at the flatpak location
patchelf --set-interpreter /app/rootfs/usr/lib/ld-linux-aarch64.so.1 \
         --set-rpath /app/rootfs/usr/lib "$OUT/app/lighthouse-pm"

# ---- 8) Build the flatpak bundle (ostree repo + ref + bundle) --------------
APPID=com.blippyblop.LighthousePM
BRANCH=stable
VER=0.1.0
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ASSETS=$(CDPATH= cd -- "$HERE/../.." && pwd)/assets
A=$OUT/appdir
rm -rf "$A"
mkdir -p "$A/files/bin" "$A/files/share/applications" \
         "$A/files/share/metainfo" "$A/files/share/icons/hicolor/512x512/apps"
# binary + wrapper live in /app/bin so the metadata `command` resolves via PATH
# (a bare /app/lighthouse-pm.sh is not on the sandbox PATH and won't launch
# from the app grid).
cp -a "$OUT/app/lighthouse-pm"    "$A/files/bin/"
cp -a "$OUT/app/lighthouse-pm.sh" "$A/files/bin/"
cp -a "$OUT/app/rootfs"           "$A/files/rootfs"
# .desktop + metainfo + icon: required for the app grid / Discover presentation
install -m 644 "$HERE/com.blippyblop.LighthousePM.desktop" \
               "$A/files/share/applications/"
install -m 644 "$HERE/com.blippyblop.LighthousePM.metainfo.xml" \
               "$A/files/share/metainfo/"
install -m 644 "$ASSETS/icon.png" \
               "$A/files/share/icons/hicolor/512x512/apps/com.blippyblop.LighthousePM.png"
cat > "$A/metadata" <<EOF
[Application]
name=$APPID
arch=aarch64
branch=$BRANCH
version=$VER
runtime=org.freedesktop.Platform
runtime-version=24.08
sdk=org.freedesktop.Sdk
sdk-version=24.08
base=org.freedesktop.Platform
base-version=24.08
command=lighthouse-pm.sh

[Context]
shared=ipc
sockets=wayland,x11,fallback,session-bus
devices=dri
EOF
ostree init --repo="$A/.ostree"
# flatpak's installer (flatpak 1.16, flatpak-dir.c validate_commit_metadata /
# resolve_op_from_commit) requires the keyfile in the commit's *main* metadata
# as xa.metadata, byte-identical to the bundle header "metadata" (which
# build-bundle takes from the tree's metadata file). A bare `ostree commit`
# writes neither, and the bundle fails to install with "Commit metadata for
# ... not matching expected metadata".
ostree --repo="$A/.ostree" commit -b "app/$APPID/aarch64/$BRANCH" -m "Lighthouse Power Manager $VER" \
  --add-metadata-string=xa.metadata="$(cat "$A/metadata")" -- "$A"
flatpak build-bundle --arch=aarch64 "$A/.ostree" "$OUT/$APPID-$VER-aarch64.flatpak" "$APPID" "$BRANCH"
echo "bundle: $OUT/$APPID-$VER-aarch64.flatpak"
