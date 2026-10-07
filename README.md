# Lighthouse Power Management

A KDE desktop app (Qt 6 + Kirigami 3) that turns SteamVR lighthouses (Valve
Index base stations) and HTC Vive base stations on, off and standby over
Bluetooth Low Energy, and organizes them into named groups and nicknames.

Lighthouses normally sleep while the SteamVR runtime is running; this app is
the companion for switching them back on without opening Steam.

The BLE stack talks to BlueZ over D-Bus directly (org.bluez Device1/GATT,
via Qt6::DBus) — QtBluetooth is not shipped on the SteamOS Frame.

- [Screens & features](docs/SCREENS.md)
- [BLE protocol](docs/PROTOCOL.md)

## Getting it (Valve Frame, aarch64)

The release is a self-contained flatpak: it ships its own Qt 6.8 / KF6 6.14 /
Kirigami 6.14 and glibc, so it runs against the firmware's stack without
modifying the system.

```sh
flatpak install lighthouse-pm-1.0.0-aarch64.flatpak   # this repo; base runtime fetched from Flathub
flatpak run com.blippyblop.LighthousePM
```

Bluetooth: flatpak sandboxes cannot reach the system D-Bus, so live BLE is
unavailable inside the sandbox (the UI and all settings still work). For full
BLE, run the installed wrapper directly:

```sh
~/.local/share/flatpak/app/com.blippyblop.LighthousePM/active/1.0.0/files/lighthouse-pm.sh
```

## Building for the Frame (cross, x86_64 → aarch64, firmware sysroot)

```sh
sh build-frame.sh
```

Uses the Frame's own gcc 15.1.1/binutils/glibc/Qt (build_env/frame-sysroot,
qemu-emulated) so the binary matches the firmware ABI; ends with a qemu
smoke test. Build host needs a native cmake + ninja + gettext — this
workspace ships them under /workspace/tools/bin (wrappers for the
firmware toolchain are in there too).

## Building for generic aarch64 KDE (Alpine/musl)

The release pipeline builds for aarch64 Linux (Alpine/KDE, musl). Manual
cross-build from an x86_64 Alpine host:

```sh
# host packages
apk add gcc-aarch64-none-elf g++-aarch64-none-elf binutils-aarch64-none-elf \
        newlib-aarch64-none-elf cmake make ninja qemu-aarch64 python3 file

# aarch64 sysroot (Qt6/KF6/Kirigami/BlueZ) + qemu-wrapped Qt build tools
python3 build_env/build_sysroot.py
sh build_env/wrap_sysroot_qemu.sh

# build + smoke test
./build.sh
```

Requirements: a working BLE adapter (`bluetoothctl pair` is used for pairing)
and BlueZ >= 5.

## Runtime requirements (aarch64 target)

Qt 6 (>= 6.8), KF6 (kcoreaddons, kconfig, ki18n), Kirigami 3, BlueZ.
