# Lighthouse Power Management

A KDE desktop app (Qt 6 + Kirigami 3) that turns SteamVR lighthouses (Valve
Index base stations) and HTC Vive base stations on, off and standby over
Bluetooth Low Energy, and organizes them into named groups and nicknames.

Lighthouses normally sleep while the SteamVR runtime is running; this app is
the companion for switching them back on without opening Steam.

- [Screens & features](docs/SCREENS.md)
- [BLE protocol](docs/PROTOCOL.md)

## Building (cross, x86_64 → aarch64)

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
