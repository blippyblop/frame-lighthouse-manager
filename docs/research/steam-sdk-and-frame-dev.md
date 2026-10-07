# Steam SDK & Steam Deck/Frame development — research (Oct 2026)

Companion to [gui-and-bluez-libs.md](gui-and-bluez-libs.md). Sources: Valve
Steamworks docs, Valve/Collabora announcements, ecosystem reports. Raw Tavily
output in `.tavily/`.

## 1. Steam SDK (Steamworks)

- **Officially C++**: `libsteam_api.so` on SteamOS/Linux (GCC 4.6+ / Clang 3.0+);
  you must link *and ship* the lib. Docs: `partner.steamgames.com/doc/sdk/api`.
- **Community bindings** (SDK exposes a flat interface for this):
  | Lang | Binding |
  |---|---|
  | C# | **Facepunch.Steamworks** (de-facto standard for managed code) |
  | Rust | `steamworks` crate (docs.rs/steamworks), steamworks-rs |
  | D | DerelictSteamworks |
  | Java | steamworks4j |
  | JS | Greenworks |
  | Python | SteamworksPy |
- **Key fact for the Frame**: SDK **≥ 1.63 ships libraries that run on
  Android ARM64 and Linux ARM64** (per `doc/steamhardware/steamframe/engines/custom`)
  → Steamworks works natively on the Frame in both execution models.
- The SDK also bundles **OpenVR** (`libopenvr_api.so`) and **OpenXR** headers;
  Valve **recommends OpenXR** for the Frame (eye tracking via
  `XR_EXT_eye_gaze_interaction`).
- **Steam Input** (`ISteamInput`) is the current controller/gamepad API;
  `ISteamController` is deprecated.
- **Steam Web API** (HTTP, no local client) is the fallback when you can't
  link the SDK at all.
- On the Frame, Steam/Steamworks libs are also **mounted into the Lepton
  container**, so APK-side access to Steam works.

## 2. Steam Deck development

- SteamOS is Arch-based with a **read-only root**; OS updates ship as whole
  images → **install extra software via Flatpak** (Valve's own FAQ guidance;
  Discover Software Center = flatpak front end).
- **Steam Runtime (SLR)** containers are the standard app sandbox on Deck/Machine.
- **SteamOS Devkit Client** (install from Steam, "Software" category, appid
  943760; `devkit-gui.sh` on Linux): connect to device by IP, deploy + debug.
- GUI ecosystem on the Deck: system UI is **Plasma/KDE (Qt)**, third-party
  utilities are mostly **flatpak'd KDE/Qt apps**; Steam itself is custom C++
  UI; **Decky Loader** plugins are JS/React in a webview (Deck-only — not on
  the Frame).
- **SDL3 is Valve-maintained** (Joshua Ashton, Valve Linux graphics team, is
  SDL's lead) and is first-class on SteamOS — Wayland default, X11 fallback.

## 3. Steam Frame development

- Standalone aarch64 (Qualcomm) headset, SteamOS, inside-out tracking,
  integrated gamepad input; **full SteamVR + OpenXR compatibility**.
- **Holo Core**: official **Arch Linux AArch64 port** built by Collabora with
  Valve as the SteamOS 3 base for the Frame; experimental preview with
  binaries + sources published (Jul 2026). This is also the base for future
  x86 SteamOS work.
- **Execution models** (Valve docs + SteamOS Devkit Client):
  1. **Windows** via Steam Play/Proton
  2. **Native Linux ARM64** via **Steam Linux Runtime 3.0 (ARM64)**
  3. **Android APK** via **Lepton** — Android compatibility *container* on
     Linux (open-sourced Sep 2026; repo is on GitHub). Mounts host mesa/zink,
     Steam/Steamworks libs, and Vulkan layers incl. Valve's "foveated
     rendering injector" and "renderpass optimizer". Aimed at porting
     Quest/Android VR titles to the Frame.
- **Valve's recommendation for custom engines: target Android 10 or Linux
  ARM64** (`steamframe/engines/custom`).
- **Dev workflow**:
  - Enable Developer Mode on the headset (`steamframe/setup`).
  - Deploy APKs / Linux binaries / Windows binaries via **Devkit Client**
    (creates a Steam library entry → easy launch options, remote desktop).
  - **ADB**: launch **"Lepton Development"** from the Steam Library (starts a
    Lepton container with an Android home screen in a floating window), then
    `adb connect frame` (hostname is `frame`; lepton docs page =
    `steamframe/adb_lepton`).
  - Debugging: Remote Desktop to the Frame + **RenderDoc** on-device;
    Performance Assessment Overlay for VR titles.
- Device-side stack (from our own rootfs inventory): full Plasma desktop for
  the nested in-VR desktop (`steamvr-nested-desktop.service`), **Qt 6.8.0,
  KF 6.14.0, Kirigami 6.14.1, BlueZ** — all present with dev files.

## 4. GUI libraries — recommendations

Context: a system utility (Lighthouse base-station manager) on Deck/Frame,
gamepad-first, possibly with a PC-side companion.

| Rank | Stack | Why |
|---|---|---|
| 1 | **Qt 6 + Kirigami (C++/QML)** | **Preinstalled on both devices** (full dev install on Frame — see gui-and-bluez-libs.md) → zero dependency bundling. Native Plasma look, gamepad/Steam Input, i18n, stateful-app patterns (Kirigami Addons StatefulApp 1.7.0 ships on device). Distribute as **flatpak** (Deck convention) or deploy native aarch64 binaries via Devkit Client (Frame). This is literally what the OS is built with. |
| 2 | **Dear ImGui + SDL3** (C/C++) | SDL3 is Valve-maintained and first-class on SteamOS (Wayland default, XWayland for X11 — Frame ships kwin_x11). Tiny footprint, fastest iteration, the de-facto standard for tooling/debug UIs in games. You must bundle SDL3 + a GL/VK context, and it won't look like OS settings. |
| 3 | **egui + winit (Rust)** | Best if you're writing Rust (pairs with the `steamworks` Rust crate; shared code between a PC-side tool and the on-device binary). winit has X11/Wayland backends. Immediate-mode, small, but no native theming. |
| 4 | **Flutter desktop** (current src_git stack) | Works on Linux, but you must bundle GTK/Ozone runtime, and x86→aarch64 cross-compiling for the Frame is awkward. A far better fit is the **Android/Lepton route**, where Flutter/Android is a first-class target. |
| 5 | **Tauri / Electron** (web stack) | Electron is the most common choice among Steam utility apps (heavy: RAM + size; community reports it behaves better than Tauri on Deck — Tauri's webview deps are the weak point on SteamOS). Only pick if the UI is genuinely web-shaped. |
| 6 | **CEF / embedded web app** | What Valve does (Steam client, SteamVR dashboard are CEF). Right only if you already maintain a web UI. |
| — | **Decky plugin (JS/React)** | Deck-only, not available on Frame. |

**Bottom line**
- Native Linux aarch64 on Frame *and* Deck → **Qt6/Kirigami** (recommended:
  already on-device, flatpak-friendly, native feel) or **Dear ImGui+SDL3** if
  you prefer a tool-style UI and don't mind bundling ~2 libs.
- APK via Lepton → **Flutter/Android** (your current stack) or Kotlin/Compose.
- Need Steam integration (accounts, input, achievements) → Steamworks works
  natively on ARM64 (SDK ≥ 1.63) or via Facepunch (C#) / steamworks (Rust).

## Key sources

- Steam Frame overview: partner.steamgames.com/doc/steamhardware/steamframe
- Custom engines (SDK ARM64/Android, OpenXR, Devkit Client):
  .../steamframe/engines/custom
- ADB/Lepton: .../steamframe/adb_lepton · Debugging: .../steamframe/debugging
- Devkit Client: .../doc/steamhardware/loadgames (install page appid 943760)
- Steamworks API overview (languages): .../doc/sdk/api
- Holo Core: collabora.com "Building an Arch Linux aarch64 port for Holo Core"
  · Phoronix "Holo Core Experimental ARM64" (Jul 2026)
- Lepton open-source: 9to5google (Sep 2026) · mlq.ai "SteamOS stack now
  covers Windows, x86 and Android on Arm"
- Deck dev/flatpak guidance: GamingOnLinux guide · Steam Deck FAQ
  (help.steampowered.com) · mikeroyal/Steam-Deck-Guide Developers README