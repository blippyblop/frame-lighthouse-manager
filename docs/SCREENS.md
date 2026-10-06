# Screens & features

Turns SteamVR lighthouses (Valve Index base stations) and HTC Vive base
stations on/off/standby over Bluetooth Low Energy, and organizes them into
named groups and nicknames. Lighthouses normally sleep when the SteamVR
runtime is running; this app is the companion for switching them back on.

## Main page

- If the Bluetooth adapter is off: a "Bluetooth off" hint with instructions
  to turn it on.
- Otherwise:
  - **Scan/stop** toolbar action (search/stop icon) with a scan duration.
  - **Pair a new device** toolbar action → pairing via the OS (BlueZ).
  - "Scanning for lighthouses..." hint while scanning.
  - Device list, sorted by device id:
    - **Group sections** (named groups, sorted by name) with a group header
      row; tapping a header toggles group selection.
    - Remaining devices under "ungrouped".
    - **Device row**: nickname (or device name), power state text, the
      device id, and a power button whose color reflects the state
      (on/sleep/standby/booting/unknown).
      - Tap → **metadata page**.
      - Long-press → **selection mode**.
  - "Last seen" devices are kept after the scan ends and shown greyed out
    (offline).

## Selection mode (group/nickname management)

Long-pressing devices (or a group header) enters selection mode; a toolbar
appears with:

- **Nickname** → dialog to rename a device (blank removes it).
- **Add to group** / **Rename group** → dialog for the group name.
- **Delete group** → confirmation dialog; devices become ungrouped.

## Metadata page

Per device:

- Device type ("Lighthouse v2" / "Vive base station")
- Name, firmware version, and other metadata (channel, model, serial,
  hardware revision, manufacturer, Vive pair ID)
- **Extra actions**: Identify (blink LED), Standby, Sleep, On — buttons
  disabled when the device is offline.
- Vive base stations: set/clear the pair ID (from the label on the unit).

## Settings

- "Lighthouses with nicknames" list (remove nicknames)
- "Clear all last seen devices"
- "Clear all Vive base station ids"
- **Use STANDBY instead of SLEEP** (V2 lighthouses only)
- Scan duration, update interval
- Preferred theme (system/dark/light)

## Help

- Pairing (including Vive base station pair IDs)
- Nicknames and groups
- Troubleshooting

## Architecture

```
src/main.cpp            QGuiApplication + QQmlEngine (Kirigami is QML-side)
src/blemanager.*        scan, connect, pair via bluetoothctl
src/lighthousedevice.*  base class: connect/disconnect, service discovery
src/lighthousev2device.*  Lighthouse v2 (LHB- advertisements):
                    powerState (1 s polling), write power byte, identify()
src/vivebasestation.*   Vive base station (HTC BS advertisements):
                    pair id, on/sleep command
src/appmodel.*          QAbstractListModel feeding the device list (rows:
                    group headers + devices), selection state
src/settingsstore.*     KConfig persistence (nicknames, groups, last seen,
                    Vive pair ids, settings)
src/qml/                main.qml (device list + dialogs), MetadataPage,
                    PairPage, SettingsPage, HelpPage
```
