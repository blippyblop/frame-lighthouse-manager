#!/bin/sh
# Stage the self-contained flatpak payload from the pristine firmware rootfs.
#
# Usage: sh stage.sh FW_ROOTFS SYSROOT BINARY OUTDIR
#   FW_ROOTFS  extracted firmware rootfs (pristine; provides all real files)
#   SYSROOT    build sysroot (provides the SONAME -> versioned-file map as symlinks)
#   BINARY     the built aarch64 lighthouse-pm binary
#   OUTDIR     output dir (gets: app/lighthouse-pm, app/lighthouse-pm.sh, app/rootfs/)
#
# The staged app/ is the flatpak's /app: firmware glibc/Qt/KF6/Kirigami under
# app/rootfs, patchelf'd binary, location-independent wrapper.
set -e
FW_ROOTFS=$1
SYSROOT=$2
BINARY=$3
OUT=$4
RFS=$OUT/app/rootfs

rm -rf "$OUT"
mkdir -p "$RFS/usr/lib" "$RFS/etc/fonts" "$RFS/usr/share/fonts" "$RFS/var/cache/fontconfig"

# 1) Shared libraries: copy the real (versioned) file from the firmware rootfs
#    and recreate the SONAME symlink the loader/dlopen expects.
while read -r name; do
  s="$SYSROOT/usr/lib/$name"
  if [ -L "$s" ]; then
    t=$(readlink "$s")
    if [ ! -f "$FW_ROOTFS/usr/lib/$t" ]; then echo "MISSING TARGET: $name -> $t" >&2; continue; fi
    cp -a "$FW_ROOTFS/usr/lib/$t" "$RFS/usr/lib/"
    ln -s "$t" "$RFS/usr/lib/$name"
  elif [ -f "$FW_ROOTFS/usr/lib/$name" ]; then
    cp -a "$FW_ROOTFS/usr/lib/$name" "$RFS/usr/lib/"
  elif [ -f "$s" ]; then
    cp -a "$s" "$RFS/usr/lib/"
  else
    echo "UNRESOLVED: $name" >&2
  fi
done <<'LIBS'
ld-linux-aarch64.so.1
libEGL.so.1
libGLX.so.0
libGLdispatch.so.0
libICE.so.6
libKF6ConfigCore.so.6
libKF6CoreAddons.so.6
libKF6I18n.so.6
libKirigami.so.6
libKirigamiDelegates.so.6
libKirigamiDialogs.so.6
libKirigamiLayouts.so.6
libKirigamiLayoutsPrivate.so.6
libKirigamiPlatform.so.6
libKirigamiPrimitives.so.6
libKirigamiPrivate.so.6
libOpenGL.so.0
libQt6Core.so.6
libQt6DBus.so.6
libQt6Gui.so.6
libQt6Network.so.6
libQt6OpenGL.so.6
libQt6Qml.so.6
libQt6QmlMeta.so.6
libQt6QmlModels.so.6
libQt6QmlWorkerScript.so.6
libQt6Quick.so.6
libQt6QuickControls2.so.6
libQt6QuickTemplates2.so.6
libQt6WaylandClient.so.6.8.0
libQt6WaylandEglClientHwIntegration.so.6.8.0
libQt6XcbQpa.so.6.8.0
libSM.so.6
libX11-xcb.so.1
libX11.so.6
libXau.so.6
libXdmcp.so.6
libb2.so.1
libblkid.so.1
libbrotlicommon.so.1
libbrotlidec.so.1
libbz2.so.1.0
libc.so.6
libcap.so.2
libcom_err.so.2
libcrypto.so.3
libcurl.so.4
libdbus-1.so.3
libdouble-conversion.so.3
libduktape.so.207
libexpat.so.1
libffi.so.8
libfontconfig.so.1
libfreetype.so.6
libgcc_s.so.1
libgio-2.0.so.0
libglib-2.0.so.0
libgmodule-2.0.so.0
libgobject-2.0.so.0
libgomp.so.1
libgraphite2.so.3
libgssapi_krb5.so.2
libharfbuzz.so.0
libicudata.so.74
libicui18n.so.74
libicuuc.so.74
libidn2.so.0
libk5crypto.so.3
libkeyutils.so.1
libkrb5.so.3
libkrb5support.so.0
libm.so.6
libmd4c.so.0
libmount.so.1
libnghttp2.so.14
libnghttp3.so.9
libpcre2-16.so.0
libpcre2-8.so.0
libpng16.so.16
libproxy.so.1
libpsl.so.5
libresolv.so.2
libssh2.so.1
libssl.so.3
libstdc++.so.6
libsystemd.so.0
libudev.so.1
libunistring.so.5
libwayland-client.so.0.26.0
libwayland-cursor.so.0.26.0
libwayland-egl.so.1.26.0
libwayland-server.so.0.26.0
libxcb-cursor.so.0
libxcb-icccm.so.4
libxcb-image.so.0
libxcb-keysyms.so.1
libxcb-randr.so.0
libxcb-render-util.so.0
libxcb-render.so.0
libxcb-shape.so.0
libxcb-shm.so.0
libxcb-sync.so.1
libxcb-xfixes.so.0
libxcb-xinput.so.0
libxcb-xkb.so.1
libxcb.so.1
libxkbcommon-x11.so.0
libxkbcommon.so.0
libz.so.1
libzstd.so.1
LIBS
chmod +x "$RFS"/usr/lib/ld-linux-aarch64.so.1

# 2) QML modules + plugins (whole trees)
mkdir -p "$RFS/usr/lib/qt6"
cp -a "$FW_ROOTFS/usr/lib/qt6/qml" "$RFS/usr/lib/qt6/qml"
cp -a "$FW_ROOTFS/usr/lib/qt6/plugins" "$RFS/usr/lib/qt6/plugins"
# platform-theme plugins ship a KIO/Widgets dep chain we do not bundle
rm -f "$RFS/usr/lib/qt6/plugins/platformthemes/KDEPlasmaPlatformTheme6.so" \
      "$RFS/usr/lib/qt6/plugins/platformthemes/libqgtk3.so" \
      "$RFS/usr/lib/qt6/plugins/platformthemes/libqxdgdesktopportal.so"

# 3) Fonts: basic UI families only (no CJK/bitmaps/rare scripts)
for d in TTF adobe-source-code-pro cantarell gnu-free; do
  cp -a "$FW_ROOTFS/usr/share/fonts/$d" "$RFS/usr/share/fonts/"
done
mkdir -p "$RFS/usr/share/fonts/noto"
cp -a "$FW_ROOTFS"/usr/share/fonts/noto/NotoSans-*.ttf "$RFS/usr/share/fonts/noto/"
cp -a "$FW_ROOTFS"/usr/share/fonts/noto/NotoSerif-*.ttf "$RFS/usr/share/fonts/noto/"
cp -a "$FW_ROOTFS/usr/share/fonts/noto/NotoColorEmoji.ttf" "$RFS/usr/share/fonts/noto/"

# 4) fontconfig: point at the bundled tree (no stale cache)
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

# 5) App binary + location-independent wrapper
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

# 6) Make the binary self-contained w.r.t. glibc at the flatpak /app location
patchelf --set-interpreter /app/rootfs/usr/lib/ld-linux-aarch64.so.1 \
         --set-rpath /app/rootfs/usr/lib "$OUT/app/lighthouse-pm"

# 7) Build the flatpak bundle (ostree repo + ref + bundle)
APPID=com.blippyblop.LighthousePM
BRANCH=stable
VER=1.0.0
A=$OUT/appdir
rm -rf "$A"
mkdir -p "$A/files"
cp -a "$OUT/app/." "$A/files/"
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
ostree --repo="$A/.ostree" commit -b "app/$APPID/aarch64/$BRANCH" -m "Lighthouse Power Manager $VER" -- "$A"
flatpak build-bundle --arch=aarch64 "$A/.ostree" "$OUT/$APPID-$VER-aarch64.flatpak" "$APPID" "$BRANCH"
echo "bundle: $OUT/$APPID-$VER-aarch64.flatpak"
