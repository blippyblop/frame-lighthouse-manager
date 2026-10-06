# Lighthouse BLE Protocol

BLE GATT protocol for SteamVR Lighthouse v2 base stations and HTC Vive base
stations, as implemented by this app.

## Device discovery

| Device | BLE advertisement name | Backend |
|---|---|---|
| SteamVR Lighthouse (Index / Base Station 2.0) | `LHB-XXXXXXXXXX` | `LighthouseV2DeviceProvider` |
| HTC Vive Base Station | `HTC BS XXXX` | `ViveBaseStationDeviceProvider` |

The app scans over BLE GATT, connects to the device, and discovers the GATT
services below. Pairing/bonding is delegated to the OS (BlueZ); the app then
talks over the GATT characteristics.

## SteamVR Lighthouse v2 protocol

UUIDs (little-endian 16-bit style service/characteristic UUIDs with the
`1212-efde-1523-785feabcd124` base):

| Item | UUID |
|---|---|
| Control service (required) | `00001523-1212-efde-1523-785feabcd124` |
| Power characteristic | `00001525-1212-efde-1523-785feabcd124` |
| Channel characteristic | `00001524-1212-efde-1523-785feabcd124` |
| Identify characteristic | `00008421-1212-efde-1523-785feabcd124` |

### Power state

Writes to the **power characteristic** (`write without response`):

| Byte | Meaning |
|---|---|
| `0x01` | power **on** |
| `0x00` | **sleep** (off) |
| `0x02` | **standby** |

Reading the power characteristic returns the current state byte:

| Byte | State |
|---|---|
| `0x00` | sleep |
| `0x02` | standby |
| `0x0b` | on |
| `0x01`, `0x08`, `0x09` | booting |
| anything else | unknown |

The app **polls** the characteristic (not notifications), once per second
(`updateInterval`, min 1000 ms), while connected, guarded by a mutex so
writes and reads don't interleave.

### Identify

Writing any byte (the app writes `0x00`) to the **identify characteristic**
makes the lighthouse blink its front LED so the user can tell which physical
unit is which.

### Channel

Reading the **channel characteristic** as a little-endian `uint32` gives the
lighthouse's channel number (metadata).

### Metadata (standard GATT Device Information Service)

Read as strings (0x2A24–0x2A29):

| UUID | Field |
|---|---|
| `00002a24-0000-1000-8000-00805f9b34fb` | Model number |
| `00002a25-0000-1000-8000-00805f9b34fb` | Serial number |
| `00002a26-0000-1000-8000-00805f9b34fb` | Firmware revision |
| `00002a27-0000-1000-8000-00805f9b34fb` | Hardware revision |
| `00002a29-0000-1000-8000-00805f9b34fb` | Manufacturer name |

## Vive Base Station protocol

| Item | UUID |
|---|---|
| Power service | `0000cb00-0000-1000-8000-00805f9b34fb` |
| Power characteristic | `0000cb01-0000-1000-8000-00805f9b34fb` |

A single **20-byte command** is written (without response):

```
offset  size  field
0       1     0x12            (command header)
1       1     action: 0x00 = on, 0x02 = sleep
2       2     big-endian u16: 0x0000 for on, 0x0001 for sleep
4       4     little-endian u32: the device's pair ID
8..19   12    zero padding
```

The **pair ID** is an 8-hex-digit number printed on the base station. The app
uses the last 4 hex digits from the BLE name (`HTC BS XXXX`) as a hint and the
user types the first 4 digits from the label; the complete ID is stored and
reused. Without a stored ID the device cannot be controlled.

There is **no state readback** for Vive stations: the app shows "unknown" when
a pair ID is stored and "booting" when it is not. The supported actions are
only on/sleep (no standby, no identify).

## Power state state machine (UI behavior)

- `sleep` / `standby` → press power → `on`
- `on` → press power → `standby` (or `sleep` if "use standby" setting is off)
- `booting` → press power → "already booting!" snackbar with an "I'm sure"
  confirm → then switch to sleep/standby
- `unknown` → press power → dialog to pick the new state
- tap device row → metadata page; long-press power button → metadata page;
  long-press device row → selection mode (group/nickname management)

## Persistence

Stored via KConfig:

- `nicknames` (deviceId → nickname)
- `groups` (id → name) + `group_entries` (deviceId → groupId)
- `vive_base_station_ids` (deviceId → pair id) — Vive pair IDs
- `last_seen_devices` (deviceId) — remembered for the offline list
- app settings (use standby, scan duration, update interval)
