# GUI libraries & BlueZ on the Frame — inventory

Verdict: **all requested stacks are present** — Qt 6, KDE Frameworks 6,
Kirigami (+addons), and BlueZ — no substitutes needed. Versions read from
`.so` filenames (no package-manager DB in this image — `/var/lib/pacman`
is empty; `/var` was scrubbed).

## 1. Qt 6.8.0 [E — `/usr/lib/libQt6Core.so.6.8.0`]

226 `libQt6*` libraries, including the full QML stack:
- `Qt6Core`, `Qt6Gui`, `Qt6Quick`, `Qt6QuickWidgets`, `Qt6QuickTest`
- **QuickControls2** + every style impl: Basic, Fusion, Material,
  Universal, Imagine, FluentWinUI3
- `Qt6QuickDialogs2`, `Qt6QuickEffects/Layouts/Shapes/Particles`
- `Qt6Core5Compat`, `Qt6Quick3DSpatialAudio`
- `.prl` files + static `.a` test libs present → a full Qt **development**
  install, not just runtime.

## 2. KDE Frameworks 6.14.0 [E — `/usr/lib/libKF6*.so.6.14.0`]

All four requested frameworks confirmed, plus many more:
| Requested | Library |
|---|---|
| kcoreaddons | `libKF6CoreAddons.so.6.14.0` (+ QML module) |
| kconfig | `libKF6ConfigCore/ConfigGui/ConfigQml.so.6.14.0` |
| ki18n | `libKF6I18n.so.6.14.0` (+ `I18nLocaleData`, `I18nQml`) |
| (extras) | Archive, Attica, Baloo(+Engine/Widgets), Bookmarks(+Widgets), BreezeIcons, CalendarEvents, Codecs, ColorScheme, Completion, Crash, DBusAddons, DNSSD, FileMetaData, GlobalAccel, GuiAddons, Holidays, IconThemes, IconWidgets, … |

## 3. Kirigami 6.14.1 + addons [E]

- `libKirigami.so.6.14.1` + split libs: `KirigamiPlatform`,
  `KirigamiPrimitives`, `KirigamiLayouts(+Private)`, `KirigamiDelegates`,
  `KirigamiDialogs`, `KirigamiPrivate`
- **Kirigami Addons**: `libKirigamiAddonsStatefulApp.so.1.7.0`
- QML modules present: `/usr/lib/qt6/qml/org/kde/kirigami/`
  (`AboutItem.qml`, `AboutPage.qml`, …) under the wider
  `qml/org/kde/` tree (activities, baloo, breeze, config, coreaddons,
  desktop, draganddrop, graphicaleffects, guiaddons, …)

## 4. BlueZ [E]

- Daemon: `/usr/lib/bluetooth/bluetoothd`
- Units: `bluetooth.service`, `bluetooth.target`, plus the Frame's own
  **`set-bluetooth-mac-address.service`** (EEPROM MAC → btmgmt, companion
  to the Wi-Fi MAC service)
- Tools: `bluetoothctl`, `btmgmt`, `btattach`; `libbluetooth.so.3.19.15`
- Kernel side: `CONFIG_BT=y` (BREDR, LE, L2CAP ECRED, HCI_UART, BT_QCA)

## 5. Where they're actually used on the device

- **Full Plasma desktop**: `plasmashell`, `kwin_wayland`, `kwin_x11`,
  `systemsettings` all ship — this is the stack behind
  `steamvr-nested-desktop.service` (the in-VR desktop) and the
  `valve.plasma.recoverymode` overlay path.
- **BlueZ's role vs the controllers**: important nuance from
  [controllers.md](controllers.md) — in production "Roy BLE" mode,
  **controller data bypasses BlueZ entirely** (proprietary spidev protocol
  via `driver_cv.so`). BlueZ serves everything else BT (MAC identity via
  btmgmt, future/other peripherals).
- The SteamVR **dashboard/settings UI is NOT Qt** — it's the CEF web app
  (see [streaming-dashboard-ui.md](streaming-dashboard-ui.md)); Qt/Kirigami
  power the OS-side desktop & system UIs instead.

## 6. Practical notes

- Link/develop against: `Qt6` (6.8.0), `KF6` (6.14.0), `Kirigami` (6.14.1),
  `bluetooth` (so.3 = BlueZ 5.7x-era). All aarch64.
- Kirigami Addons `StatefulApp` at 1.7.0 means modern Kirigami app
  templates work out of the box.
- If a doc/tool ever reports these "missing", it's because it looked in a
  scrubbed `/var/lib/pacman` — check `/usr/lib` directly.
